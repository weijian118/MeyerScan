# MeyerScan

MeyerScan 是美亚光电口扫软件重构仓库。当前仓库按模块一级目录维护，每个模块独立保存 VS2015 工程、源码、README 和中文 CHANGELOG。

## 当前模块

| 目录 | 产物 | 状态 | 边界 |
|------|------|------|------|
| `MyLogger` | `MeyerScan_Logger.dll` | v1.1.0 | 结构化日志、按天和大小轮转、逐条同步写入；不承载业务逻辑 |
| `MyDatabase` | `MeyerScan_Database.dll` | v1.2.0 | Qt SQL 数据库基础设施；只做连接、SQL、事务、备份 |
| `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | v0.1.0 | 运行配置读取和默认配置生成；不做权限判断 |
| `MyPermission` | `MeyerScan_Permission.dll` | v0.1.0 | 权限 visible/enabled 骨架；不做 UI |
| `MyUIComponents` | `MeyerScan_UIComponents.dll` | v0.2.0 | 通用控件和样式；当前已统一标准按钮角色和内容布局 |
| `MyHomeUI` | `MeyerScan_HomeUI.dll` | v0.2.0 | 首页入口 UI；只上报入口 ID |
| `MyCaseUI` | `MeyerScan_CaseUI.dll` | v0.2.0 | 案例管理 UI 框架；只上报动作 ID |
| `MySettingsUI` | `MeyerScan_SettingsUI.dll` | v0.2.0 | 用户设置 UI；校准入口嵌入，设置持久化后续走 ConfigCenter |
| `MyMainExe` | `MeyerScan.exe` | v0.1.0 | 主入口、单实例、基础设施初始化、页面容器和模块编排 |

另有 `MyCaseOrderService`、`MyOrderScanWorkspaceShell`、`MyCalibration3DUI`、`MyCalibrationColorUI`、`MyVersionManager` 等骨架模块，具体状态见各模块 README/CHANGELOG 和 `D:\wj\重构文档`。

`MyCaseManager` 是旧版数据库/旧 schema 参考目录，不是当前活跃案例管理模块。当前案例管理 UI 归 `MyCaseUI`，患者/订单和医生、诊所、技工所等数据库领域数据归 `MyCaseOrderService`。

## 关键规则

- 源码注释、模块 CHANGELOG 和 GitHub 提交信息使用中文。
- UI 可见文案统一写 `tr("English source text")`，源码不写中文 UI source text。
- 运行路径以 `QCoreApplication::applicationDirPath()` 或调用方传入的应用目录为基准，禁止用 `QDir::currentPath()` 推导资源路径。
- UI 模块只做展示、输入收集、操作日志和 ID 上报；业务规则进入 Service/Workflow。
- 通用控件和通用样式进入 `MyUIComponents`，单模块专用控件留在自身模块。
- 每个 EXE/DLL 保持 `Version.rc`、`ModuleInfo::Version`、`GetModuleVersion()` 一致。

## 构建

每个模块目录都保留 VS2015 `.sln/.vcxproj`。也可以用根目录聚合方案一次打开当前已拆分模块：

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' F:\MeyerScan\MeyerScan_AllModules.sln /p:Configuration=Release /p:Platform=x64 /m
```

常用 smoke：

```powershell
F:\MeyerScan\MyHomeUI\bin\Release\HomeUITest.exe --smoke
F:\MeyerScan\MyCaseUI\bin\Release\CaseUITest.exe --smoke
F:\MeyerScan\MySettingsUI\bin\Release\SettingsUITest.exe --smoke
F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main
```
