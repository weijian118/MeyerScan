# MeyerScan

MeyerScan 是美亚光电口扫软件重构仓库。当前仓库按模块一级目录维护，每个模块独立保存 VS2015 工程、源码、README 和中文 CHANGELOG。

## 当前模块

| 目录 | 产物 | 状态 | 边界 |
|------|------|------|------|
| `MyLogger` | `MeyerScan_Logger.dll` | v1.1.0 | 结构化日志、按天和大小轮转、逐条同步写入；不承载业务逻辑 |
| `MyDatabase` | `MeyerScan_Database.dll` | v1.3.0 | 纯 C++ 数据库基础设施；SQLite 通过 sqlite3.dll C API 动态加载，只做连接、SQL、事务、备份 |
| `MyDatabaseQtAdapter` | `MeyerScan_DatabaseQtAdapter.dll` | v0.1.0 | Qt 模块访问纯 C++ Database 的类型转换层；只做 QString/QJson/缓冲区适配 |
| `MyRuntimeDataCenter` | `MeyerScan_RuntimeDataCenter.dll` | v0.1.0 | 本地/云端常用信息运行时 JSON 快照；只读缓存，不做 CRUD |
| `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | v0.1.0 | 运行配置读取和默认配置生成；不做权限判断 |
| `MyPermission` | `MeyerScan_Permission.dll` | v0.1.0 | 权限 visible/enabled 骨架；不做 UI |
| `MyUIComponents` | `MeyerScan_UIComponents.dll` | v0.2.0 | 通用控件和样式；当前已统一标准按钮角色和内容布局 |
| `MyHomeUI` | `MeyerScan_HomeUI.dll` | v0.2.0 | 首页入口 UI；只上报入口 ID |
| `MyCaseUI` | `MeyerScan_CaseUI.dll` | v0.2.0 | 案例管理 UI 框架；只上报动作 ID |
| `MySettingsUI` | `MeyerScan_SettingsUI.dll` | v0.2.0 | 用户设置 UI；校准入口嵌入，设置持久化后续走 ConfigCenter |
| `MyMainExe` | `MeyerScan.exe` | v0.1.0 | 主入口、单实例、基础设施初始化、页面容器和模块编排 |

另有 `MyCaseOrderService`、`MyOrderScanWorkspaceShell`、`MyCalibration3DUI`、`MyCalibrationColorUI`、`MyVersionManager` 等骨架模块，具体状态见各模块 README/CHANGELOG 和 `D:\wj\重构文档`。

`MyCaseManager` 是旧版数据库/旧 schema 参考目录，不是当前活跃案例管理模块。当前案例管理 UI 归 `MyCaseUI`，患者/订单和医生、诊所、技工所等数据库领域数据归 `MyCaseOrderService`；本地常用信息和云端诊所信息的只读快照归 `MyRuntimeDataCenter`。

## 关键规则

- 源码注释、模块 CHANGELOG 和 GitHub 提交信息使用中文。
- UI 可见文案统一写 `tr("English source text")`，源码不写中文 UI source text。
- 运行路径以 `QCoreApplication::applicationDirPath()` 或调用方传入的应用目录为基准，禁止用 `QDir::currentPath()` 推导资源路径。
- UI 模块只做展示、输入收集、操作日志和 ID 上报；业务规则进入 Service/Workflow。
- 当前默认数据库链路为 SQLite，Database 本体不依赖 Qt；Qt 模块通过 `MyDatabaseQtAdapter` 访问 Database。MySQL 原生驱动待 MySQL C API SDK 接入后恢复，UI 不直接拼业务 SQL。
- SQLite 运行时固定使用 x64 `ThirdParty/SQLite/win-x64/sqlite3.dll`，各模块 VS2015 PostBuild 和 CMake 公共规则从该目录复制到输出目录；不要再从旧 SQLiteStudio 目录复制 32 位 DLL。第三方 DLL 本体不提交仓库，只提交放置说明。
- 本地诊所、技工所、医生、患者、订单、设备等高频变化字段优先通过 `MyRuntimeDataCenter` 的 domain JSON 快照读取，保存/删除/状态变化仍走 `MyCaseOrderService`。
- `MyRuntimeDataCenter` 是有上限的运行时只读快照，不是无限大数据通道；字段扩展可自然进入 JSON，记录量过大时必须改分页/查询服务，不能继续把大表全量塞进 UI。
- 通用控件和通用样式进入 `MyUIComponents`，单模块专用控件留在自身模块。
- 每个 EXE/DLL 保持 `Version.rc`、`ModuleInfo::Version`、`GetModuleVersion()` 一致。
- 每个模块和测试宿主都必须同时保留 VS2015 工程和 `CMakeLists.txt`。VS2015 继续用于现有调试习惯，VSCode 优先通过 CMake Tools / VS2015 生成器打开根目录或单模块目录。
- 当前活跃自研 DLL 模块必须有独立 `*Test.exe` 或等价 smoke 入口；根 `MeyerScan_AllModules.sln` 必须纳入这些测试项目。主程序 `MeyerScan.exe` 本身通过 `--smoke-main` 覆盖主链路，不额外创建 `MainExeTest.exe`。
- 单模块 `.sln` 和根 `MeyerScan_AllModules.sln` 都必须能生成可运行输出；涉及 Database/DatabaseQtAdapter 的模块需要同时验证自身 `bin/Release` 和根 `bin/Release`，避免只在聚合目录复制了依赖。
- 非界面模块默认优先评估纯 C++ 实现，能不用 Qt 就不用 Qt；已有 Qt 非界面模块先保持公共 ABI 不暴露 Qt 类型，把 Qt 限制在 `.cpp` 私有实现内，后续再逐步替换。
- GitHub 提交之外，还必须用 `tools/BackupToLocalRepository.ps1` 同步到本地仓库 `F:\MeyerScan-Reposit`。本地仓库按所有模块整体备份，中文提交日志要写清变更内容和验证结果。
- 本地仓库只保存自研源码、测试项目、VS2015 工程、CMake、配置说明、文档快照和自研 DLL/EXE/LIB。Qt 插件、VC/UCRT、OpenSSL、AWS、MySQL/SQLiteStudio/SQL 驱动、日志、数据库现场文件、IDE 临时文件必须过滤。
- 后续扫描重建模块中，EXE 只负责 UI/交互/流程编排；编辑、处理、数据 IO、预处理等业务/算法能力优先拆成 DLL 或独立库，避免界面代码和数据处理代码混在一起。

## 构建

每个模块目录都保留 VS2015 `.sln/.vcxproj`。也可以用根目录聚合方案一次打开当前已拆分模块：

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' F:\MeyerScan\MeyerScan_AllModules.sln /p:Configuration=Release /p:Platform=x64 /m
```

常用 smoke：

```powershell
F:\MeyerScan\bin\Release\LoggerTest.exe
F:\MeyerScan\bin\Release\DatabaseTest.exe
F:\MeyerScan\bin\Release\ConfigCenterTest.exe
F:\MeyerScan\bin\Release\PermissionTest.exe
F:\MeyerScan\bin\Release\VersionManagerTest.exe
F:\MeyerScan\bin\Release\DatabaseQtAdapterTest.exe
F:\MeyerScan\bin\Release\CaseOrderServiceTest.exe
F:\MeyerScan\bin\Release\RuntimeDataCenterTest.exe
F:\MeyerScan\bin\Release\UIComponentsTest.exe
F:\MeyerScan\MyHomeUI\bin\Release\HomeUITest.exe --smoke
F:\MeyerScan\MyCaseUI\bin\Release\CaseUITest.exe --smoke
F:\MeyerScan\MySettingsUI\bin\Release\SettingsUITest.exe --smoke
F:\MeyerScan\bin\Release\Calibration3DUITest.exe
F:\MeyerScan\bin\Release\CalibrationColorUITest.exe
F:\MeyerScan\bin\Release\OrderScanWorkspaceShellTest.exe
F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main
```

登录测试宿主 `MyMainExe\MyLogin\MeyerLoginTest.exe` 用于验证外部既有登录 DLL，会打开登录界面并依赖外部 SDK/安装目录文件，按人工集成测试独立维护。

VSCode/CMake 入口：

```powershell
cd F:\MeyerScan
cmake -G "Visual Studio 14 2015 Win64" -S . -B build_vs2015
cmake --build build_vs2015 --config Release
```

本地仓库备份：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File F:\MeyerScan\tools\BackupToLocalRepository.ps1 -RefactorDocsRoot "D:\wj\重构文档" -CommitMessage "本地完整备份：说明本次变更、影响模块和验证结果"
```

脚本会额外把 `D:\wj\重构文档` 下的 Markdown 文件同步到 `_RefactorDocs`，只保存 `.md`，不保存 token 文本、临时提取文件、图片或其他非 Markdown 文件。
