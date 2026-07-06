# MeyerScan

MeyerScan 是美亚光电口扫软件重构仓库。当前仓库按模块一级目录维护，每个模块需要保留源码、VS2015 工程、CMakeLists、README 和中文 CHANGELOG。

## 当前模块

| 目录 | 产物 | 状态 | 边界 |
|------|------|------|------|
| `MyLogger` | `MeyerScan_Logger.dll` | 已落地 | 结构化日志、按天/大小轮转、逐条同步写入；不承载业务逻辑 |
| `MyDatabase` | `MeyerScan_Database.dll` | 已落地 | 纯 C++ 数据库基础设施；当前默认 SQLite；不理解患者/订单语义 |
| `MyDatabaseQtAdapter` | `MeyerScan_DatabaseQtAdapter.dll` | 已落地 | Qt 模块访问纯 C++ Database 的类型转换层 |
| `MyRuntimeDataCenter` | `MeyerScan_RuntimeDataCenter.dll` | 已落地 | 本地/云端常用信息运行时 JSON 快照；只读缓存，不做 CRUD |
| `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | 已落地 | 运行配置读取和默认配置生成；不做权限判断 |
| `MyPermission` | `MeyerScan_Permission.dll` | 已落地 | 权限 visible/enabled 骨架；不做 UI |
| `MyUIComponents` | `MeyerScan_UIComponents.dll` | 已落地 | 通用控件和样式；不承载业务行为 |
| `MyHomeUI` | `MeyerScan_HomeUI.dll` | 已落地 | 首页入口 UI，只上报入口 ID |
| `MyCaseUI` | `MeyerScan_CaseUI.dll` | 已落地 | 案例管理 UI，只展示/上报动作，业务保存走服务 |
| `MySettingsUI` | `MeyerScan_SettingsUI.dll` | 已落地 | 用户设置 UI，校准入口嵌入，持久化后续走配置/服务 |
| `MyOrderCreateUI` | `MeyerScan_OrderCreateUI.dll` | 已落地 | 建单 UI，展示基本信息、扫描方案和牙位选择，只上报动作 ID |
| `MyOrderScanWorkspaceShell` | `MeyerScan_OrderScanWorkspaceShell.dll` | 已落地 | 建单、扫描、处理、发送统一工作台壳 |
| `MyExternalLaunchAdapter` | `MeyerScan_ExternalLaunchAdapter.dll` | 已落地 | 第三方拉起建单 JSON 映射，保留 `thirdPartyType` |
| `MyScanWorkflowUI` | `MeyerScan_ScanWorkflowUI.dll` | 初版落地 | `ScanReconstructStudio.exe` 内部“扫描”阶段 UI，负责 QVTK 显示和扫描操作入口 |
| `MyDataProcessUI` | `MeyerScan_DataProcessUI.dll` | 初版落地 | `ScanReconstructStudio.exe` 内部“数据处理”阶段 UI，负责 QVTK 显示和处理工具入口 |
| `MyScanReconstructStudio` | `ScanReconstructStudio.exe` | 初版落地 | 扫描重建独立进程壳，动态加载扫描 UI 和数据处理 UI，切换阶段时释放重资源 |
| `MyMainExe` | `MeyerScan.exe` | 已落地 | 主入口、单实例、登录、页面容器、模块编排和版本清单 |

另有 `MyCaseOrderService`、`MyCalibration3DUI`、`MyCalibrationColorUI`、`MyVersionManager` 等骨架模块，具体状态见各模块 README/CHANGELOG 和 `D:\wj\重构文档`。

## 关键规则

- 模块 README / CHANGELOG / 提交日志使用中文；UI 可见文本源码统一写 `tr("English source text")`。
- 运行路径以 `QCoreApplication::applicationDirPath()` 或调用方传入的应用目录为准，禁止用 `QDir::currentPath()` 推导资源路径。
- UI 模块只做展示、输入收集、操作日志和动作 ID 上报；业务规则进入 Service/Workflow。
- Database 本体不依赖 Qt；Qt 模块通过 `MyDatabaseQtAdapter` 访问 Database。
- 扫描重建保持独立进程。`ScanReconstructStudio.exe` 只做壳子、UI/交互/流程编排；扫描采集、设备命令、数据 IO、预处理、编辑、颈缘、测量、倒凹、咬合、底座等能力后续继续拆 DLL 或独立库。
- 重资源页面离开时必须释放或暂停资源。扫描 UI、数据处理 UI 当前通过 `DeactivateAndRelease()` 释放 `QVTKWidget`、VTK renderer、OpenGL/显存相关资源。
- 每个 EXE/DLL 保持 `Version.rc`、模块代码版本和 `GetMeyerModuleVersion()` 口径一致。
- GitHub 之外还需要用 `tools/BackupToLocalRepository.ps1` 同步到本地仓库 `F:\MeyerScan-Reposit`。

## 构建

VS2015 单模块构建：

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\amd64\MSBuild.exe' F:\MeyerScan\MyScanReconstructStudio\MeyerScan_ScanReconstructStudio.sln /p:Configuration=Release /p:Platform=x64 /m
```

根聚合方案：

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\amd64\MSBuild.exe' F:\MeyerScan\MeyerScan_AllModules.sln /p:Configuration=Release /p:Platform=x64 /m
```

VSCode/CMake 入口：

```powershell
cd F:\MeyerScan
& 'F:\Tools\CMakePython\cmake\data\bin\cmake.exe' -G "Visual Studio 14 2015 Win64" -S . -B build\cmake-vs2015-x64
& 'F:\Tools\CMakePython\cmake\data\bin\cmake.exe' --build build\cmake-vs2015-x64 --config Release
```

当前电脑已安装 CMake 3.31.6，安装位置为 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe`；该路径已写入用户 PATH，新开的 VSCode / PowerShell 可直接使用 `cmake`。已用 VS2015 x64 生成器完成根聚合工程 `Release` 配置和构建验证。

运行时版本清单由 `MyMainExe/config/version_modules.json` 驱动，只记录自研拆分模块 EXE/DLL。扫描 UI 需要的 VTK/OpenCV/Qt 运行库会复制到运行目录，但不进入 `logs/versionList`；正式安装包依赖清单后续由打包模块单独维护。

扫描相关第三方依赖路径优先使用环境变量：

- `QT_ROOT`
- `VTK_ROOT`
- `VTK_HEADERS_ROOT`
- `OPENCV_ROOT`

未设置环境变量时，VS/CMake 会尝试仓库 `ThirdParty` 目录，最后回退到当前开发机参考路径。

## 常用烟测

```powershell
F:\MeyerScan\MyScanWorkflowUI\bin\Release\ScanWorkflowUITest.exe
F:\MeyerScan\MyDataProcessUI\bin\Release\DataProcessUITest.exe
F:\MeyerScan\MyScanReconstructStudio\bin\Release\ScanReconstructStudio.exe --smoke
F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main
```

需要人工查看界面时：

```powershell
F:\MeyerScan\MyScanWorkflowUI\bin\Release\ScanWorkflowUITest.exe --show
F:\MeyerScan\MyDataProcessUI\bin\Release\DataProcessUITest.exe --show
F:\MeyerScan\MyScanReconstructStudio\bin\Release\ScanReconstructStudio.exe
```

## 本地仓库备份

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File F:\MeyerScan\tools\BackupToLocalRepository.ps1 -RefactorDocsRoot "D:\wj\重构文档" -CommitMessage "本地完整备份：说明本次变更、影响模块和验证结果"
```
