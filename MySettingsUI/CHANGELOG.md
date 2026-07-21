# MeyerScan SettingsUI 变更记录

## 2026-07-21 - 0.7.0

- SettingsUI API 升级为 7、校准上下文 schema 升级为 5，新增主控板/投图板版本快照并逐字段转发给 CalibrationColorUI。
- 颜色校准入口设备信息弹窗显示主控板版本；`mOS MyScan` 额外显示投图板版本，其他系列不显示不存在的板卡。
- Ready 上下文门禁新增版本结构、主控板有效状态及投图板 `Valid/NotRequired` 检查，避免旧宿主或空版本继续进入校准。
- 新增版本读取失败提示，测试宿主补充版本快照；代码/CMake/Windows 文件版本升级为 0.7.0。

## 2026-07-20 - 0.6.0

- SettingsUI API 升级为 6、校准设备上下文 schema 升级为 4，并完整转发 DeviceCmd 检测记录。
- 设备上下文新增 D9/C7/CE 步骤状态、生产模式、兼容标志、真实 reported 值、最终 effective 值及各字段来源；SettingsUI 不解析原始回包。
- 颜色校准入口把原“仅显示设备编号”改为一次性设备信息提示，统一展示有效编号、具体产品、型号代码、生产模式和兼容来源，避免连续多个模态弹窗。
- CE 旧固件、回包异常、未初始化、校验失败和型号值非法使用英文 `tr()` 源文本说明兼容原因；D9 普通回包异常、编号非法和型号代码非法增加独立错误提示。
- 注入 CalibrationColorUI 时逐字段复制检测 POD，不使用 `reinterpret_cast` 假设两个 DLL 的结构布局相同。
- 代码版本、CMake 版本和 Windows 文件版本统一升级为 0.6.0。

## 2026-07-20 - 0.5.0

- SettingsUI API 升级为 5、颜色校准上下文 schema 升级为 3；只读转发 DeviceCmd 已识别的产品系列、具体产品、协议 Profile、状态和证据。
- 新增产品身份冲突提示，设备编号前缀与型号代码不匹配时显示错误并阻止颜色校准。
- 界面术语由历史“机器码”统一为“设备编号”；DeviceCmd API 中 MachineCode 名称仅为兼容保留。
- 注入 CalibrationColorUI 的 POD 增加产品名称和识别状态，SettingsUI 仍不解析设备回包、不维护产品映射表。
- CMake、代码版本和 Windows 文件版本统一升级为 0.5.0，相关 CTest 通过。

## 2026-07-20 - 0.4.0

- 颜色校准预检顺序扩展为工作台门禁、连接、USB3、`0xD4/0xD9` 机器码、`0xCD/0xCE` 机型；新增机器码读取失败状态 9。
- 机器码读取成功后先用公共信息弹窗显示可复制的 13 位机器码，再继续处理机型结果和颜色校准入口。
- 设备上下文在原保留空间中增加 `modelCodeUtf8[32]`，结构总大小不变；SettingsUI API 升级为 4，颜色校准上下文 schema 升级为 2。
- 动态加载 UIComponents 单按钮弹窗 C ABI；缺失或 ABI 不匹配时降级为 `QMessageBox`，不阻断设置流程。
- `SettingsUITest` 覆盖机器码读取失败提示、公共弹窗结构、机器码内容和关闭机器码提示后进入颜色校准遮罩。

## 2026-07-17 - 0.3.0 设备预检链路

- 公共接口 ABI 升级为 3，增加 `SetCalibrationPreflightCallback` 和固定 POD `SettingsCalibrationDeviceContext`。
- 颜色校准按钮改为先同步检查工作台占用、设备连接、USB2/USB3、设备信息和型号，只有 `Ready` 才加载并创建颜色校准弹窗。
- 分别提示退出扫描模块、设备未连接、重新插入 USB3 和型号读取失败；提示文本均使用 `tr("English source text")`。
- 颜色校准关闭后上报 `SettingsActionColorCalibrationClosed`，MainExe 据此关闭唯一设备会话。
- `SettingsUITest --test-preflight-status` 覆盖工作台、未连接、USB2、型号未知四个失败提示，并验证失败时不创建遮罩。

## 2026-07-17 - 颜色校准蒙层

- 修复 Windows 透明顶层窗口未绘制蒙层的问题：增加独立 `SettingsCalibrationDimmer` 子控件，颜色校准打开后设置背景会正确变暗。
- 配合颜色校准面板拖动能力，取消遮罩对颜色校准根控件的 Layout 托管，改由 SettingsUI 在打开时计算初始居中位置。
- 颜色校准拖动只改变面板位置，遮罩仍覆盖原设置窗口范围，关闭按钮和 Exit 行为保持不变。

## 2026-07-16

- 版本升级为 `v0.2.4`，同步代码版本、CMake 项目版本和 Windows DLL 文件版本，公共接口 ABI 保持版本 2。
- 颜色校准从动态 `QStackedWidget` 页面改为覆盖设置宿主的半透明模态遮罩弹窗，校准面板居中显示；三维校准暂时保留原嵌入流程。
- 为颜色/三维校准入口增加稳定 objectName 和主按钮语义属性，不依赖翻译文字执行自动化定位。
- CMake/VS2015 构建补齐 Calibration3DUI、CalibrationColorUI 和 UIComponents 运行时准备，避免 `SettingsUITest` 读取旧校准 DLL。
- `SettingsUITest` 新增 `--capture-color-calibration <png>`，通过真实颜色校准入口验证 DLL 懒加载、遮罩弹窗、动作回调和离屏视觉合成。

## 2026-07-15

- 版本升级为 `v0.2.3`，公共接口版本升级为 2；新增 `SetDataContextJson()`，医生、诊所、技工所只从宿主快照读取。
- 删除 RuntimeDataCenter/DatabaseQtAdapter 自举、数据库配置解析和 SQLite 运行依赖；测试宿主改用纯 JSON fixture。
- Logger 与两个校准 DLL 使用应用目录绝对路径，并在调用虚接口前校验 ABI 版本。

## 2026-07-13

- 版本升级为 `v0.2.2`，同步 CMake、代码版本和 Windows `Version.rc`。
- 清理“SettingsUI 直接读取 ConfigCenter”的旧 TODO，明确正式配置由 MainExe/设置服务读取后通过版本化上下文注入；云端地址改为空白可翻译提示，不再内置可能被误保存的占位 URL。
- 信息管理表格注释改为“少量只读快照”，保持 `SettingsUI -> RuntimeDataCenter` 只读边界；`SettingsUITest --smoke` 已登记到根 CTest 清单。
- VS2015/CMake Release 构建、隔离运行目录中的 24 项 CTest 和 MainExe 登录前 smoke 通过；运行时版本清单中文件/代码版本均为 0.2.2。

## 2026-07-12

- 版本升级为 `v0.2.1`；三维校准和颜色校准 DLL 懒加载后必须检查 `Init()`，失败时调用子模块 `Shutdown()`、清空接口并写 Warning，设置其它页面继续可用。
- 保持设置页只读 RuntimeDataCenter、校准能力按来源禁用和子 DLL 可选降级边界，不把校准算法、设备资源或配置持久化实现放进 SettingsUI。
- VS2015/CMake Release、`SettingsUITest.exe --smoke` 和 MainExe 集成 smoke 通过。

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
