# version_modules.json 字段说明

`version_modules.json` 用来声明 MeyerScan 启动时需要写入 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json` 的拆分模块文件。

## 设计规则

- 只记录 MeyerScan 拆分出来的自研 `exe` / `dll`，不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库。
- 新增模块后优先维护本文件的 `modules` 数组，不修改 MainExe 的目录扫描逻辑。
- 新增模块加入清单后，必须同步修改 MainExe PostBuild 或后续安装包脚本，把该 DLL/EXE 复制到运行目录。
- 文件名按 `MeyerScan.exe` 所在目录解析，不允许写开发机绝对路径。
- 缺失文件也会写入版本清单，并标记 `exists=false`，便于发现安装包漏复制。
- 文件名时间戳包含毫秒，避免 smoke、第三方拉起或重复启动在同一秒内生成的版本快照互相覆盖。

## 输入字段

- `description`：给维护人员看的简短说明，程序不依赖该字段。
- `schemaVersion`：清单结构版本。当前为 `2`。
- `modules`：模块数组。推荐使用对象格式。
- `modules[].file`：需要记录版本的 EXE/DLL 文件名或相对路径。
- `modules[].versionFunction`：可选的统一 C ABI 版本函数名；自研 DLL 固定使用 `GetMeyerModuleVersion`。为空表示只记录文件版本。
- `modules[].factory`：旧字段，只为兼容历史清单读取；新增模块不要再使用该字段。

## 输出字段

MainExe 启动后读取本清单，输出以下关键字段：

- `fileVersion`：Windows 文件版本资源，来自 EXE/DLL 内编译进去的 `Version.rc`。
- `codeVersion`：代码运行时版本，来自模块统一导出函数 `GetMeyerModuleVersion()`；`MeyerScan.exe` 使用自身 `ModuleInfo::Version`。
- `versionMatch`：文件版本和代码版本是否一致。允许 `0.4.0.0` 与 `0.4.0` 视为一致。
- `codeVersionError`：动态加载 DLL、解析统一版本函数或读取代码版本失败时的错误信息。
- `exists`、`size`、`lastModified`：文件是否存在、文件大小和最后修改时间。

## 版本维护要求

- 每个自研 EXE/DLL 都必须把 `src/Version.rc` 编进 VS2015 工程和 CMake 目标。
- Windows 文件“详细信息”页显示的版本来自 `Version.rc`，不是来自 `GetModuleVersion()`。
- `Version.rc` 中的 `FILEVERSION`、`PRODUCTVERSION`、字符串 `FileVersion`、`ProductVersion` 必须同步维护。
- 模块代码中的 `ModuleInfo::Version` 或等价常量必须与 `Version.rc` 同步维护。
- 自研 DLL 必须导出统一 C ABI 函数 `GetMeyerModuleVersion()`，返回与模块 `GetModuleVersion()` 相同的 `ModuleInfo::Version` 字符串。这样版本清单读取代码版本时不需要创建业务接口对象，也不需要 MainExe 包含各业务模块头文件。
- 模块原有业务工厂函数（例如 `GetHomeUI`、`GetDatabaseQtAdapter`）继续用于功能动态加载，不再推荐用于版本清单读取。

## 动态加载关系

MainExe 对 HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter、ConfigCenter、Permission、UIComponents、RuntimeDataCenter、DatabaseQtAdapter、Logger 等自研模块采用运行时动态加载 DLL 的方式。扫描相关的 `ScanReconstructStudio.exe`、`MeyerScan_ScanWorkflowUI.dll`、`MeyerScan_DataProcessUI.dll` 已纳入本清单，启动时同样记录文件版本与代码版本。工程仍包含接口头文件，但不链接这些模块的 import lib。

Qt、Windows `Version.lib`、既有外部登录模块 `MeyerLoginWidget.lib` 仍按当前阶段静态/导入库方式链接。后续若登录模块也整理出稳定适配层，再单独讨论是否改为动态加载。
