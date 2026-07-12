# PowerShell 开发与自动化脚本规范

> 适用范围：`F:\MeyerScan` 下构建、资源生成、本地备份、测试和发布辅助脚本。当前基线是 Windows PowerShell 5.1、VS2015 和 CMake，不能只按 PowerShell 7 行为编写。

## 1. 编码和语法

1. `.ps1` 保存为带 BOM 的 UTF-8。Windows PowerShell 5.1 会把无 BOM UTF-8 按系统 ANSI 代码页读取，中文注释可能破坏后续代码解析。
2. 不使用 `? :`、`??`、`ForEach-Object -Parallel`、`$IsWindows` 等 PowerShell 7 专属语法。
3. 不把 `$Args` 用作参数或普通变量名；它是 PowerShell 自动变量。
4. 读取文本时显式传 `-Encoding`，或使用带明确编码的 .NET API。PS5 与 PS7 的默认编码不同。
5. VS2015 `rc.exe` 是例外：资源 `.rc` 使用纯 ASCII，避免中文和 UTF-8 BOM 影响 `RCDATA` / `VS_VERSION_INFO` 解析。
6. 在 Windows PowerShell 5.1 中查看 UTF-8 Markdown 时使用 `Get-Content -Encoding UTF8`；终端出现乱码时先按指定编码重读，不能直接判断源文件已经损坏并批量转码。
7. C/C++ 的 `//` 注释必须独占物理行，代码从下一行开始；注释末尾禁止反斜杠 `\`，否则预处理续行可能让下一行代码也进入注释。
8. 临时禁用代码使用带原因说明的 `#if 0`，禁止用块注释包裹代码，避免补丁换行或嵌套注释破坏边界。
9. 批量补充中文注释后运行 `F:\MeyerScan\tools\CheckSourceCommentSafety.ps1`，再分别做 VS2015/CMake 编译；静态检查不能代替编译验证。

## 2. 引号、正则和参数

1. 同时包含单双引号、反斜杠、Unicode 范围的复杂正则不要嵌入多层 PowerShell 命令字符串。优先拆成多个 `rg` 命令，或把模式放入变量/独立脚本。
2. 文件操作优先使用 `-LiteralPath`，避免 `[]`、`*`、`?` 被当成通配符。
3. 调用 EXE 使用 `&` 和独立参数数组，不使用 `Invoke-Expression` 二次解释拼接字符串。
4. 中文、空格、括号路径作为单独参数传递；路径从脚本参数、`$PSScriptRoot` 或已校验根目录推导，不依赖 current directory。
5. JSON/XML 使用 `ConvertFrom-Json`、`ConvertTo-Json`、`XmlWriter` 等结构化 API，不用字符串拼接生成。
6. 大仓库文件检索优先使用 `rg --files -g '<pattern>'`。`Get-ChildItem -LiteralPath ... -Include ...` 的过滤行为容易与预期不一致，可能遍历并输出整个第三方目录树；必须使用 PowerShell 时，应在已限定目录后用 `Where-Object` 明确过滤。
7. 递归检索必须排除构建输出和第三方源码目录，并限制输出字段；避免把 MySQL、VTK 等大目录的全部文件名送入终端，造成交互卡顿和关键结果被截断。

## 3. 退出码和错误处理

1. 调用 CMake、MSBuild、rcc、git、robocopy、rg 等原生工具后检查 `$LASTEXITCODE`；`$?` 只说明 PowerShell 是否成功调用命令。
2. robocopy 的退出码 `0..7` 都表示成功或存在已处理差异，只有大于 7 才是失败。
3. `rg` 无匹配返回 1，规则扫描时通常代表“未发现违规”；退出码 2 才表示参数、路径或 IO 错误。
4. 长构建一次只执行一个主要动作，失败时保留完整输出。不要把多个独立验证强行串联到一个失败即终止的表达式中。
5. 控制台中文乱码不等于文件损坏，以文件编码、Git diff、退出码和实际运行结果复核。
6. Windows GUI 子系统 EXE 不能依赖直接调用后的 `$LASTEXITCODE`；它可能为空或沿用旧值。UI smoke、截图和主程序自动测试使用 `Start-Process -Wait -PassThru`，从返回的进程对象读取 `ExitCode`，后台执行同时设置 `-WindowStyle Hidden`。
7. VS2015 根方案和 CMake 构建不得并行写同一模块输出目录。当前两套工程最终都写入模块 `bin\Release`，并发会争用 `.exp/.lib/.dll` 并产生 LNK1104；必须串行验证。
8. 构建锁冲突后先确认没有活跃构建任务，再结束残留 MSBuild 节点并使用 `/nodeReuse:false` 重跑。禁止删除仍被链接器使用的产物来掩盖命令并发错误。

## 4. 删除、镜像和幂等

1. 递归删除前把目标解析为绝对路径，并验证它严格位于预期工作区/备份根目录内；禁止对盘符根目录或未校验动态路径递归删除。
2. `robocopy /MIR` 的 `/XD`、`/XF` 只阻止继续复制，不会删除目标仓库历史遗留排除项；镜像后需要受路径边界保护的主动清理。
3. 排除目录按深度从深到浅删除；参数使用字符串数组，保持 `/XD`、`/XF` 和值的顺序。
4. 资源生成、备份和清理脚本必须可重复执行。首次执行完成变更，第二次应无额外修改或提交。

## 5. 当前脚本验收

每次修改 `.ps1` 后至少完成：

1. 使用 PowerShell AST 解析，错误数必须为 0。
2. 在 Windows PowerShell 5.1 中实际执行一次。
3. 检查关键输出文件存在且内容可读。
4. 再执行一次验证幂等。
5. 运行 `git diff --check`。

当前正式脚本：

- `F:\MeyerScan\tools\BackupToLocalRepository.ps1`：把全部自研源码、测试、工程文件和必要产物整体备份到 `F:\MeyerScan-Reposit`。
- `F:\MeyerScan\tools\CheckSourceCommentSafety.ps1`：检查中文源码 BOM、`.rc` ASCII、`//` 注释续行和疑似注释/代码粘连。
- `F:\MeyerScan\MyUIResources\tools\GenerateResourceManifest.ps1`：扫描各模块 UI 资源并用 `XmlWriter` 生成 qrc 清单。

仓库内精简执行说明见 `F:\MeyerScan\tools\README.md`。本文件是方案级长期规范，两者修改时必须同步。

---

> **文档版本**：v1.2（2026-07-12，补充注释物理换行、执行策略、GUI 退出码、构建并发和源码安全检查规则）
