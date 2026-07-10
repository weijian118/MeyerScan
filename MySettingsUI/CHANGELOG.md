# MeyerScan SettingsUI 变更记录

## 2026-07-10

- 设置页样式迁入 `Resources/qss/settings.qss`，源码通过公共资源/QSS/日志辅助函数加载；CMake/VS2015 工程补齐模块资源复制。
- 保持设置业务入口和校准懒加载边界不变；`Version.rc` 补齐版权字段，VS2015/CMake Release 构建通过。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `SettingsUIImpl.cpp` 和 `SettingsUITest.exe` 中文注释及文件级阅读说明，说明设置 UI 模块边界、RuntimeDataCenter 只读快照、校准 DLL 懒加载、扫描重建来源禁止校准、测试库 PID 隔离和 Information 页冒烟验证。
- 本轮仅补充注释，不改变 SettingsUI 设置页结构、校准入口、数据读取或动作回调逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；`SettingsUITest.exe --smoke` 返回 0；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-03

- 修复 `SettingsUITest.exe --smoke` 在根聚合输出目录 `F:\MeyerScan\bin\Release` 运行时的路径推导问题：测试宿主现在从 EXE 所在目录向上查找 `MeyerScan_AllModules.sln` 作为仓库根，不再假设 EXE 一定在 `MySettingsUI\bin\Release`。
- 测试宿主改为生成 `config/SettingsUITest/db_config.json` 和独立 SQLite 测试库，不再复用公共 `config/db_config.json` 指向的数据库，避免不同测试旧表结构互相污染。
- 测试日志目录按运行形态区分：根聚合输出写入仓库 `logs`，单模块输出写入 `MySettingsUI\logs`，便于批量测试和单模块调试分别排查。
- 重新验证根输出目录 `SettingsUITest.exe --smoke`，数据库演示数据、RuntimeDataCenter 快照和 Information 页医生/诊所/技工所表格链路返回 0。

## 2026-07-02

- 2026-07-03 复查补充：单模块 `MeyerScan_SettingsUI.sln` 已重新构建，输出目录同步 RuntimeDataCenter、DatabaseQtAdapter、Database 和 x64 `sqlite3.dll`；`SettingsUITest.exe --smoke` 返回 0。
- 新增模块 `CMakeLists.txt`，同时声明 `SettingsUITest.exe`，支持 VSCode/CMake Tools 与 VS2015 生成器构建。
- 按评审结论同步 UI/业务分离规则：SettingsUI 继续作为 Qt 设置界面模块，设置保存、权限判断、业务数据维护和校准算法/设备重资源不写入设置 UI 主流程。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试项目和自研产物。
- 继续按“实现技巧型注释”要求补强 `SettingsUIImpl.cpp`：补充 QSS/objectName 精确选择器、Qt Layout 伸缩和边距、设置模块内部 `QStackedWidget` 分类页、RuntimeDataCenter 动态加载、校准 DLL 懒加载、校准页动态 wrapper、`deleteLater()`、`QTableWidgetItem` 所有权和多语言/多分辨率布局说明。
- 继续补强 `SettingsUITest.exe` 测试宿主：说明测试宿主为什么可以准备 SQLite 最小旧表、正式 SettingsUI 为什么不能建表/插表、各旧表和演示数据分别服务哪个 RuntimeDataCenter domain、以及 `findChildren<QTableWidget*>` 如何验证 Information 页链路。
- 本轮仅补充注释和文档记录，不改变 SettingsUI 运行逻辑。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `SettingsUITest.exe` 测试宿主：补充模块根目录/仓库根目录推导、发布配置优先、SQLite 演示数据准备、测试造数据与正式 UI 边界、`findChildren<QTableWidget*>` 冒烟检查和事件循环延迟检查说明。
- 前一轮已补强 `SettingsUIImpl.cpp` 中 RuntimeDataCenter 动态加载、校准 DLL 懒加载、设置来源上下文、`QStackedWidget` 内部页切换、表格所有权和 JSON 字段兼容说明。
- 根据文档规则与代码复核结果，SettingsUI 不再主动调用 `RuntimeDataCenter.ReloadAll()`；MainExe 启动期负责全域刷新，SettingsUI 只初始化 RuntimeDataCenter，并在 Information 页读取医生/诊所/技工所 domain 时按需懒加载。
- `SettingsUITest.exe --smoke` 的 SQLite 演示库补齐软件信息、设置、账号、设备信息最小表。
- 演示库覆盖 RuntimeDataCenter 当前全部本地 domain；MainExe 打开设置页、返回首页、进入案例页时，日志应显示 `All runtime domains reloaded`。
- 正式 `MeyerScan_SettingsUI.dll` 仍只读取 RuntimeDataCenter 快照，不负责 schema 初始化、旧库迁移或业务数据写入。
- 验证：`MeyerScan_SettingsUI.sln`、`MeyerScan_AllModules.sln` Release x64 构建通过；`SettingsUITest.exe --smoke`、`MeyerScan.exe --smoke-main` 均返回 0。

## 2026-06-30

- Information 页面接入 `MeyerScan_RuntimeDataCenter.dll`，医生读取 `local.doctors`，诊所读取 `local.clinics`，技工所读取 `local.labs`，不再使用硬编码占位数据。
- `MeyerScan_SettingsUI.vcxproj` 补齐 RuntimeDataCenter、DatabaseQtAdapter、Database、sqlite3.dll 和 `db_config.json` 发布目录复制，保证独立测试宿主可完整运行；SettingsUI 正式代码只读 RuntimeDataCenter 快照，不直接访问 Database。
- `SettingsUITest.exe --smoke` 增加 SQLite 演示数据准备：空库时创建最小旧表并写入医生、诊所、技工所、患者、订单各一条数据。
- `SettingsUITest.exe --smoke` 从只验证窗口创建升级为检查 Information 页面三张表均有数据行。
- 修复 RuntimeDataCenter JSON 快照解析问题：调用方按 C 字符串真实内容解析，避免预分配缓冲区尾部空字节导致 `QJsonDocument` 报 invalid JSON。
- 验证：`MeyerScan_SettingsUI.sln` Release x64 构建通过，`SettingsUITest.exe --smoke` 返回 0。

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
