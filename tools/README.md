# MeyerScan 自动化脚本维护规范

本目录脚本必须同时兼容当前开发机的 Windows PowerShell 5.1、VS2015 构建事件和命令行手工执行。脚本失败时先检查本规范中的工具链差异，不要直接修改业务代码规避。

## 1. 编码与语法

1. `.ps1` 保存为**带 BOM 的 UTF-8**。Windows PowerShell 5.1 会把无 BOM UTF-8 按系统 ANSI 代码页读取，中文注释的字节可能破坏后续代码行解析。
2. 不使用 PowerShell 7 专属语法：三元表达式 `? :`、空合并 `??`、`ForEach-Object -Parallel`、`$IsWindows` 等都不能进入正式脚本。
3. 不把 `$Args` 用作自定义参数名。`$Args` 是 PowerShell 自动变量，曾导致 robocopy 参数数组未按预期传递。
4. 读取文本时显式指定编码，或使用带明确 `Encoding` 的 .NET API；不要依赖 `Get-Content` 在 PS5/PS7 中不同的默认编码。
   查看 UTF-8 Markdown 时固定使用 `Get-Content -Encoding UTF8`；终端乱码不代表文件已经损坏，禁止据此直接批量转码。
5. VS2015 `rc.exe` 是例外：`.rc` 文件若含中文和 UTF-8 BOM 可能误解析 `RCDATA/VS_VERSION_INFO`。资源脚本保持纯 ASCII；C++、头文件和 `.ps1` 继续使用 UTF-8 BOM。
6. 命令行正则同时含有单双引号、反斜杠和 Unicode 范围时，不要继续嵌套进一条 PowerShell 字符串。优先拆成多个 `rg` 命令，或把复杂模式放进脚本变量/独立文件，避免外层引号提前闭合。
7. C/C++ 的 `//` 注释必须独占物理行，后续代码从下一行开始；注释末尾严禁反斜杠 `\`，因为预处理续行发生在注释删除之前，会把下一行代码一并吞掉。
8. 禁止用 `/* ... */` 临时包住待禁用代码。需要短期关闭代码时使用带原因说明的 `#if 0`，避免块注释边界在补丁换行后错位。
9. 修改中文注释后必须运行 `tools\CheckSourceCommentSafety.ps1`；脚本硬性检查中文源码 BOM、`.rc` 纯 ASCII 和注释末尾续行，并提示疑似“注释与代码粘在同一物理行”。检查通过不能替代 VS2015/CMake 实际编译。

## 2. 执行与退出码

1. 机器可能禁止直接执行 `.ps1`。构建事件使用一次性 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ...`，不要永久修改系统执行策略。
2. 调用原生 EXE 后检查 `$LASTEXITCODE`，不能只看 `$?`。`$?` 只说明 PowerShell 是否成功启动了命令，不等于工具业务退出码。
3. robocopy 的 `0..7` 都是成功/有差异状态，只有大于 7 才是失败；不能按普通“非 0 即失败”处理。
4. CMake、MSBuild、rcc、git 等长命令一次只验证一个主要动作，失败后保留完整原始输出，避免多条命令串联后丢失真正失败点。
5. 子 PowerShell 或原生命令的中文控制台输出可能在捕获终端中显示乱码；以文件内容、Git 提交信息和退出码复核，不根据显示乱码判断文件已损坏。
6. `rg` 在“没有匹配项”时返回退出码 1，这通常表示规则检查通过，不是工具故障。批量自动化必须区分“无匹配=预期”与退出码 2 的真实错误，不能让一个预期无匹配中断其它独立检查。
7. PowerShell 直接调用 Windows GUI 子系统 EXE 时，`$LASTEXITCODE` 可能为空或沿用旧值。GUI smoke/截图测试统一使用 `Start-Process -Wait -PassThru` 并读取进程对象的 `ExitCode`；后台启动时同时使用 `-WindowStyle Hidden`，避免测试窗口干扰桌面。
8. VS2015 根方案与 CMake 方案虽然构建目录不同，但当前模块产物都会写入各模块 `bin\Release`；禁止并行构建同一模块，否则会争用 `.exp/.lib/.dll` 并触发 LNK1104。两套构建必须串行执行。
9. 发生构建文件锁后，先确认没有仍在运行的构建任务，再结束 MSBuild 节点并用 `/nodeReuse:false` 重跑；不能通过删除正在使用的输出文件绕过锁。

## 3. 路径与参数

1. 文件系统操作优先使用 `-LiteralPath`，避免 `[]`、`*` 等字符被当成通配符。
2. 所有仓库路径从脚本参数、`$PSScriptRoot` 或已校验根目录推导；不依赖调用者当前目录。
3. 调用 EXE 使用调用运算符 `&` 和独立参数，不使用 `Invoke-Expression` 拼接命令字符串。
4. 中文、空格和括号路径必须作为单独参数传递；不要先拼成一整条带引号字符串再二次解释。
5. JSON/XML 使用 `ConvertFrom-Json`、`ConvertTo-Json`、`XmlWriter` 等结构化 API；JSON 文件内部不写注释。
6. 大仓库文件检索优先使用 `rg --files -g '<pattern>'`。不要依赖 `Get-ChildItem -LiteralPath ... -Include ...` 对整个仓库过滤，它可能仍遍历并输出第三方源码树。
7. 递归检索必须排除构建目录和第三方目录，并限制输出字段，避免终端卡顿及关键结果被截断。

## 4. robocopy 与删除安全

1. `robocopy /MIR` 配合 `/XD`、`/XF` 只阻止排除内容继续复制，**不会删除目标仓库中已经存在的历史排除文件**。镜像后必须单独执行受保护的排除内容清理。
2. 递归删除前必须把源/目标解析为绝对路径，并验证待删路径严格位于预期根目录内；禁止对盘符根目录、源目录或动态未校验路径执行递归删除。
3. 排除目录按路径深度从深到浅删除，避免先删父目录后继续访问已不存在的子目录。
4. robocopy 参数使用字符串数组传递，`/XD`、`/XF` 和各自值保持正确顺序；不要用会和自动变量冲突的参数名。
5. 备份脚本修改后至少执行两次：第一次验证实际变更，第二次必须命中 0 个排除项且没有新 Git 提交，证明幂等。

## 5. 当前脚本

- `BackupToLocalRepository.ps1`：整体同步到 `F:\MeyerScan-Reposit`，负责第三方/现场文件过滤、历史排除内容清理和中文 Git 提交。
- `CheckSourceCommentSafety.ps1`：检查自研源码的 UTF-8 BOM、`.rc` ASCII 约束、`//` 续行和疑似注释粘连；提交前和批量补注释后必须执行。
- `MyUIResources/tools/GenerateResourceManifest.ps1`：扫描各模块 `Resources`，用 `XmlWriter` 生成确定性 qrc 清单；只允许 PNG/QSS/SVG/ICO/JPG/BMP/GIF/QM 等 UI 资源类型。

每次修改脚本后必须完成：PowerShell AST 解析、Windows PowerShell 5.1 实际执行、关键输出存在性检查、重复执行检查和 `git diff --check`。
