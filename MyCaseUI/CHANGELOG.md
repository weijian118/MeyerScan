# MeyerScan CaseUI 变更记录

## 2026-07-10

- 版本升级为 `v0.2.1`，同步更新代码版本、CMake 和 `Version.rc`。
- 浏览页顶部区域增加品牌、设置、返回首页、最小化和关闭入口；新增 `CaseActionMinimize` / `CaseActionClose`，所有窗口动作只上报 MainExe。
- 顶部控件和页面内容继续由 QSS/Qt Layout 管理，不使用 Qt 原生标题栏，不让 CaseUI 持有顶层窗口所有权。
- CaseUITest 增加顶部关闭动作 ID 回调验证，保持测试宿主与正式跨模块合同一致。

## 2026-07-06

- 修正 `CaseUI.h` 和 `CaseUIImpl.cpp` 中容易误导后续维护的旧说明：正式 CaseUI 只初始化日志、共享 UI 和 RuntimeDataCenter，只读取运行时快照展示患者/订单列表，不负责数据库健康检查、建表、迁移或业务写入。
- 复核当前链路仍保持 `CaseUI -> RuntimeDataCenter -> DatabaseQtAdapter -> Database`，CaseUI 不直接包含 Database 接口，也不直接拼业务 SQL。
- 已在根聚合 CMake `Release` 构建中验证本模块和测试宿主可以随全工程编译通过；CMake 使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 与 VS2015 x64 生成器。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `CaseUITest.exe` 测试宿主文件级阅读说明，说明测试数据库配置、患者/订单/参考数据造数、RuntimeDataCenter 快照链路、`--smoke` 表格行数验证和 CaseUI 只负责显示不负责建表/造数的边界。
- 本轮仅补充注释，不改变 CaseUI 列表读取、动作上报或页面切换逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；`CaseUITest.exe --smoke` 返回 0；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-03

- 修复 `CaseUITest.exe --smoke` 在根聚合输出目录 `F:\MeyerScan\bin\Release` 运行时的路径推导问题：测试宿主现在从 EXE 所在目录向上查找 `MeyerScan_AllModules.sln` 作为仓库根，不再假设 EXE 一定在 `MyCaseUI\bin\Release`。
- 测试宿主改为生成 `config/CaseUITest/db_config.json` 和独立 SQLite 测试库，不再复用公共 `config/db_config.json` 指向的数据库，避免不同测试旧表结构互相污染。
- 测试日志目录按运行形态区分：根聚合输出写入仓库 `logs`，单模块输出写入 `MyCaseUI\logs`，便于批量测试和单模块调试分别排查。
- 重新验证根输出目录 `CaseUITest.exe --smoke`，数据库演示数据、RuntimeDataCenter 快照和患者/订单表格链路返回 0。

## 2026-07-02

- 2026-07-03 复查补充：单模块 `MeyerScan_CaseUI.sln` 已重新构建，输出目录同步 RuntimeDataCenter、DatabaseQtAdapter、Database 和 x64 `sqlite3.dll`；`CaseUITest.exe --smoke` 返回 0。
- 更新模块 `CMakeLists.txt`，改为复用根目录公共 CMake 规则，并补齐 Logger、UIComponents、RuntimeDataCenter 等当前依赖；正式 CaseUI DLL 不再依赖 Database，测试宿主造数经 DatabaseQtAdapter 完成。
- 按评审结论同步 UI/业务分离规则：CaseUI 继续作为 Qt 界面模块，只展示列表、记录操作和上报动作；业务保存、删除、加载订单规则不得回到 UI 内。
- 正式 CaseUI 不再直连 Database；测试宿主需要造最小演示数据时，也统一通过 `MyDatabaseQtAdapter` 访问纯 C++ Database。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试宿主和自研产物。
- 继续按“实现技巧型注释”要求补强 `CaseUITest.exe`：说明患者/订单/参考数据最小旧表分别服务哪个 RuntimeDataCenter domain，测试宿主为什么可以造数据而正式 CaseUI 不能造数据，患者/订单演示字段如何支撑两个 Tab 的表格显示，以及 `--smoke` 如何通过表格行数验证真实链路。
- 本轮只补充测试宿主注释和文档记录，不改变 CaseUI 列表读取、动作上报或页面切换逻辑。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `CaseUITest.exe` 测试宿主：补充屏幕居中、多显示器坐标、模块根目录推导、SQLite 演示数据准备、测试造数据与正式 UI 边界、`findChildren<QTableWidget*>` 冒烟检查和事件循环等待说明。
- 前一轮已补强 `CaseUIImpl.cpp` 中 RuntimeDataCenter 动态加载、跨 DLL 缓冲区、JSON items 解析、`QTableWidget` 所有权、字段兼容读取和 UI 不拼业务 SQL 的说明。
- 根据文档规则与代码复核结果，CaseUI 不再主动调用 `RuntimeDataCenter.ReloadAll()`；MainExe 启动期负责全域刷新，CaseUI 只初始化 RuntimeDataCenter，并在读取患者/订单 domain 时按需懒加载。
- 复核需求与代码后调整 CaseUI 初始化顺序：正式 DLL 只初始化 RuntimeDataCenter 并读取快照；独立测试宿主先经 DatabaseQtAdapter 完成测试库准备，再加载 RuntimeDataCenter，避免产生无意义的数据库未就绪日志。
- 修正患者/订单表格附近的旧注释：当前列表展示读取 RuntimeDataCenter 只读快照；复杂搜索、分页、编辑、删除、状态变化和打开订单仍归 CaseOrderService / OrderWorkflowService。
- `CaseUITest.exe --smoke` 的 SQLite 演示库补齐 `meyer_scan`、`soft_init`、`user_tbl`、`user_tbl2`、`device_info_tbl2` 等轻量表。
- 演示库现在覆盖 RuntimeDataCenter 当前声明的全部本地 domain，避免 MainExe 集成测试日志被预期缺表 Warning 干扰。
- 正式 `MeyerScan_CaseUI.dll` 仍只负责 UI 展示和动作上报，不负责建表、迁移或业务写入；正式 schema 初始化仍归后续 migration / CaseOrderService。
- 验证：`MeyerScan_AllModules.sln` Release x64 构建通过；`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、MainExe 单模块和根聚合目录 `MeyerScan.exe --smoke-main` 均返回 0。

## 2026-06-30

- 动态加载 `MeyerScan_RuntimeDataCenter.dll`，从 `local.patients` 和 `local.orders` 读取运行时 JSON 快照填充患者/订单表格。
- CaseUI 仍只负责 UI 展示、客户操作日志和动作 ID 上报；RuntimeDataCenter 只提供只读快照，新增/编辑/删除和状态变化仍归 CaseOrderService/Workflow。
- Release PostBuild 增加 `MeyerScan_RuntimeDataCenter.dll` 复制，保证单模块测试宿主和 MainExe 运行目录都能加载快照模块。
- RuntimeDataCenter 读取缓冲改为 512KB 起步、倍增到 32MB 的有限重试，防止字段扩展后固定缓冲区不足导致患者/订单列表误显示为空。
- `CaseUITest.exe --smoke` 增加 SQLite 演示数据准备：空库时创建最小旧表并写入患者、订单、诊所、技工所、医生各一条数据。
- `CaseUITest.exe --smoke` 从只验证窗口创建升级为检查患者表和订单表均有数据行。
- 验证：`MeyerScan_CaseUI.sln` Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。

## 2026-06-26

- 顶部“返回首页”和“设置”按钮接入 `MeyerScan_UIComponents.dll` 的 Secondary 标准样式。
- 患者/订单工具栏按钮接入 UIComponents 标准按钮样式：普通操作使用 Secondary，主要打开操作使用 Primary，删除类操作使用 Danger。
- 新增运行时动态加载 UIComponents 的缓存逻辑；共享 UI 模块不可用时保留本地 `QPushButton` 降级样式，保证案例管理页面仍可启动。
- 明确 CaseUI 只负责页面结构、客户操作日志和动作 ID 上报；按钮视觉归 UIComponents，权限显隐/启用态仍由 MainExe / Workflow / Service 复核。
- VS2015 工程增加 `..\MyUIComponents\include`，Release PostBuild 复制 `MeyerScan_UIComponents.dll`，保证测试宿主和主程序运行目录可加载共享样式模块。
- 复查版本信息：`ModuleInfo::Version` 和 `Version.rc` 升级为 v0.2.0，避免版本清单继续显示旧框架版本。
- 验证：`MeyerScan_CaseUI.sln` Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。

## 2026-06-25

- 新增 `SetActionEnabled()` 接口，接收 MainExe 下发的动作启用态；当前先落地“返回首页”按钮。
- “返回首页”按钮创建时同时应用 `visible` 和 `enabled`，后续其它动作可按 actionId 扩展同一状态模型。
- Qt 模块日志调用改为使用 `Logger.h` 的 `QString` 便捷接口，跨 DLL ABI 仍保持 UTF-8 `const char*`。
- 日志 `module` 字段统一使用 `MeyerScan_CaseUI`，与 VS2015 工程 `MEYER_MODULE_NAME` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名、Debug 标志、`FILEOS` 和 `FILETYPE` 与全项目版本资源规范保持一致。
- 确认 `MEYER_MODULE_NAME="MeyerScan_CaseUI"` 已存在，日志宏可输出稳定案例管理模块名。
- 为 `CaseUITest` 测试宿主补充独立 `MEYER_MODULE_NAME`，避免测试日志混入正式模块名。

## 2026-06-24

- 对齐新版 Logger 规则：CaseUI 当前没有真实登录人/操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段，避免输出无意义 `[Op:System]`。
- 根据“初学者可读”要求补强函数体内部注释：日志/数据库借用、返回首页按钮显隐、Tab 切换、患者/订单工具栏、搜索触发、空表格占位和测试宿主路径推导均增加关键说明。
- 补充 `ICaseUI` 公共接口和 `CaseUITest` 测试宿主的中文注释，明确动作 ID 分组、页面返回、路径推导和冒烟测试流程。
- 补充 CaseUI 头文件和实现文件的函数级中文注释，明确 CaseUI 只做列表/按钮/页签框架和动作 ID 上报。
- 补充关键边界注释：正式列表、搜索、CRUD、打开订单必须走 CaseOrderService、DataExport 和 OrderWorkflowService，不能在 UI 内直接拼业务 SQL。
- 补充 Logger/Database 生命周期注释：CaseUI 只缓存并借用接口，不关闭进程级 Logger / Database。

## 2026-06-22

- 2026-06-23 补充：新增 `SetActionVisible()`，用于接收 MainExe/Permission 下发的浏览模块操作显隐规则；当前可控制“返回首页”按钮显示状态。
- 新增 `SetActionCallback()` 客户操作事件接口，浏览模块只上报操作 ID，由 MainExe 负责跨页面切换。
- 在浏览模块顶部新增“返回首页”按钮，点击后上报 `CaseActionBackHome`。
- 患者页和订单页的工具按钮、搜索回车、页签切换均写入 `UserAction` 日志。
- MainExe 已接入返回首页操作，可从 CaseUI 丝滑切回 HomeUI。
- 2026-06-23 再次补充：测试宿主不再硬编码 `F:/MeyerScan/...`，改为根据 exe 所在目录推导模块日志目录和 MyDatabase 配置路径。
- 2026-06-23 历史口径：CaseUI 当时曾借用进程级 Logger/Database 做框架期健康检查；2026-07-03 起正式 DLL 不接入 Database，测试宿主造数统一通过 DatabaseQtAdapter。
- 2026-06-23 再次补充：按钮源文案改为稳定英文 `Back Home`，中文显示后续由模块 qm 提供，避免源码文案混用语言。
- 2026-06-23 复查优化：初始化数据库前优先检查进程级 Database 是否已连接；MainExe 已完成数据库健康检查时，CaseUI 只借用现有连接，不重复 Init/Connect。
- 2026-06-23 复查补充：明确 CaseUI 必须支持被 MainExe 释放并重建；后续进入扫描重建前由 MainExe 销毁 CaseUI widget，避免案例管理界面长期占用资源。
- 2026-06-23 复查补充：界面可见文字从 `QApplication::translate()` 统一改为 `tr("English source text")`；源码不写中文 UI 文案，中文显示后续由模块 `.qm` 提供。
- 2026-06-23 复查验证：`MeyerScan_CaseUI.sln` Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。
- 重新验证：VS2015 Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。
- 新增模块级变更记录文件。
- 历史记录：当时 Database 调用只用于框架 smoke 健康检查；当前正式 CaseUI 已移除 Database 直连。
- CaseUI 正式业务行为应调用 CaseOrderService、DataExport 和 OrderWorkflowService，不直接执行业务 SQL。
- 修正 Logger 生命周期边界：使用 `QLibrary::PreventUnloadHint` 避免退出阶段 DLL 卸载顺序问题。
- 当前 CaseUI 不做 Database 健康检查；正式业务后续迁入 Service/Workflow。
- 调整 `CaseUITest.exe` 生命周期：退出前先关闭并删除顶层 widget，再执行模块 `Shutdown()`。

## 2026-06-18

- 在测试宿主中于 `QApplication` 创建前启用 High DPI 相关属性。
- `CaseUITest.exe` 增加按当前屏幕可用区域居中和限制初始尺寸的逻辑。
- 界面可见文本完成国际化预埋，为后续独立 `.qm` 翻译文件预留基础。
- README 补充 DPI 和多语言翻译说明。

## 2026-06-17

- 创建 Qt Widgets DLL 框架和 `CaseUITest.exe`。
- 新增患者和订单两个页签框架。
- 历史记录：初始框架曾接入 Logger 和 Database 用于启动链路验证；当前正式 DLL 接入 Logger、UIComponents 和 RuntimeDataCenter，不直连 Database。
- 在 PostBuild 中加入 Qt 5.6.3 运行库复制规则。
