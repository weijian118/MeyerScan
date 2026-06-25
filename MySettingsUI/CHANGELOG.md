# MeyerScan SettingsUI 变更记录

## 2026-06-26

- 复查设置模块占位路径：General 页面订单存储路径和打包路径不再显示 `D:/SOFTSCANDATA` / `D:/SCANDATA`，骨架期改为基于 `QStandardPaths::DocumentsLocation` 生成用户目录下的占位路径。
- 继续保留 TODO：正式阶段设置项读写统一走 ConfigCenter，不由 SettingsUI 直接持久化。

## 2026-06-25 v0.2.0

- 升级 Information 页面：使用 QTabWidget 实现医生/诊所/技工所三标签切换，每个标签页包含搜索栏、QTableWidget 数据表格和编辑/删除按钮。
- 升级 Cloud 页面：新增云端账号登录表单（用户名/密码）和服务器配置卡片（服务器地址、自动上传开关）。
- 升级 Scan 页面：新增扫描行为设置卡片，包含扫描提示图开关、可续扫时间选择、录屏开关、默认订单类型、完成后跳转和体感控制选项。
- 升级 Data Processing 页面：新增数据处理配置卡片，包含处理配置选择、上下颌补洞范围和扫描杆补洞范围。
- 更新 ModuleInfo::Version 至 v0.2.0，Version.rc 同步更新。

## 2026-06-25 v0.1.1

- 记录 General 页面硬编码 D:/ 路径问题并增加 TODO；2026-06-26 已进一步改为基于 `QStandardPaths::DocumentsLocation` 生成占位路径（[#1][#4]）。
- 修复 SettingsUITest.vcxproj 中 MEYER_MODULE_NAME 不含 MeyerScan_ 前缀（[#5]）。
- 更新 ModuleInfo::Version 至 v0.1.1，Version.rc 同步更新。
- 校准模块 LoadCalibrationModules() 补充超时/降级/loading 状态 TODO 注释（[#6]）。
- 补充 README.md 设置数据模型和持久化策略说明（[#3]）。
- 架构文档模块清单新增 MySettingsUI 行（[#2]）。

## 2026-06-25

- 新增设置模块骨架，输出 `MeyerScan_SettingsUI.dll`。
- 按现有设置截图搭建左侧分类导航、右侧内容区、底部 Confirm/Apply/Restore/Cancel 操作区。
- Calibration 分类中接入 3D Calibration 和 Color Calibration 入口，当前通过动态加载 `MeyerScan_Calibration3DUI.dll` / `MeyerScan_CalibrationColorUI.dll` 嵌入校准页面。
- 新增 `SettingsActionId`，用于 MainExe 记录关闭、确认、应用、恢复和校准入口操作。
- 新增 `ModuleInfo::Name` / `ModuleInfo::Version`，日志模块名和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 源码可见文字统一使用 `tr("English source text")`，源码注释使用中文。
