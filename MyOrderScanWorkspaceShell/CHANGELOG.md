# MeyerScan OrderScanWorkspaceShell 变更记录

## 2026-07-23 - 工作台设备上下文方案同步（未修改代码）

- 明确工作台壳只转发设备上下文和采集会话标识，不创建设备会话、不发送设备命令。
- Order/Scan/Process/Send 切换时保留机型系列、Profile、设备编号状态、设备编号/型号（有则记录）和 reported/effective 来源。
- 详细方案同步到 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。

## 2026-07-15

- 版本升级为 `v0.1.4`；新增公共接口 ABI 版本导出，MainExe 在挂载工作台前执行版本门禁。

## 2026-07-12

- 版本升级为 `v0.1.3`；Logger 初始化失败时清空日志接口，壳子仍可执行步骤导航和页面挂载，不再连续使用半初始化 Logger。
- 补充日志生命周期和测试宿主实现注释；模块继续只拥有唯一步骤导航、页面容器和动作回调，不创建 Scan/Process/Send 业务页面。
- `OrderScanWorkspaceShellTest.exe` 覆盖创建/练习模式、真实按钮切换、返回动作和重复 Init/Shutdown，全部通过。

## 2026-07-10

- 测试宿主可见占位文本补充 `tr("English source text")`，测试项目同样遵守多语言源码规则。
- 版本升级为 `v0.1.2`，同步更新代码版本、CMake 和 `Version.rc`。
- 顶部区域整合品牌、返回入口、唯一的 Order/Scan/Process/Send 步骤导航、最小化和关闭；创建/练习内容页不得再绘制第二套步骤条。
- 新增 `WorkspaceShellActionBack`，返回、最小化和关闭均只上报稳定动作 ID，由 MainExe 执行顶层窗口或页面操作。
- 删除重复的 `Current Step` 文本，步骤按钮的选中态直接表达当前步骤。
- 单模块解决方案补齐 Logger 工程依赖，测试宿主增加返回动作回调验证。

## 2026-07-07

- 版本升级为 `v0.1.1`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 复核工作台模式、步骤按钮、右上角 `Minimize` / `Close` 回调和步骤变化回调：创建模式显示 Order / Scan / Process / Send，练习模式只显示 Scan / Process。
- 明确本模块只负责容器、步骤导航和页面挂载；Scan/Process 页面创建、激活和释放仍由 MainExe 或后续 `ScanReconstructStudio.exe` 生命周期编排。
- 文档补充 SendUI 挂载边界：壳子只提供 Send 步骤容器和步骤变化回调，`MeyerScan_SendUI.dll` 由 MainExe 懒加载并注入上下文，真实发送业务不进入壳子。

## 2026-07-06

- 新增工作台模式：`WorkspaceModeOrderCreate` 显示 Order / Scan / Process / Send，`WorkspaceModePractice` 只显示 Scan / Process，用于首页“Practice”入口。
- 新增右上角壳按钮回调：创建和练习工作台只显示 `Minimize` / `Close`，壳子通过 `WorkspaceShellActionMinimize` / `WorkspaceShellActionClose` 上报 MainExe。
- 新增步骤变化回调：MainExe 可在切到 Scan/Process 时懒加载真实页面，并在离开时释放 QVTK/VTK 等重资源。
- `OrderScanWorkspaceShellTest.exe` 增加练习模式验证，覆盖不显示 Order/Send、默认选中 Scan 和 Scan/Process 步骤按钮存在。
- 修复顶部 Order / Scan / Process / Send 只能展示不能点击的问题：步骤条从 `QLabel` 改为 `QPushButton`，点击按钮后由壳子内部调用 `SetStep()` 切换 `QStackedWidget` 页面。
- 新增步骤按钮选中态同步，当前步骤按钮保持高亮；测试宿主增加按钮点击验证，覆盖 Scan 和 Process 页面切换。
- README 补充步骤条必须使用可点击按钮、点击后由壳子内部切换页面的约束。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- MainExe 已接入本工作台壳：首页点击“Create”和第三方自动拉起建单都会进入 `OrderScanWorkspaceShell`，并把 `OrderCreateUI` 挂载到 `WorkspaceStepOrderCreate`。
- README 补充本模块当前与 MainExe、OrderCreateUI、ExternalLaunchAdapter 的集成关系，强调工作台壳不解析第三方字段、不保存建单数据。
- 补充 `OrderScanWorkspaceShellTest.exe` 测试宿主中文注释，说明 QApplication 初始化、工作区壳根控件创建、步骤页面挂载、非法 step 防崩溃验证、`--show` 人工查看模式和资源释放流程。
- 本轮仅补充注释，不改变工作区壳子模块页面挂载和步骤切换逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；该日 CMake 未执行，2026-07-06 已使用 CMake 3.31.6 和 VS2015 x64 生成器完成根聚合 Release 补验证。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步工作台边界：本模块是 Qt 工作台壳，只做建单、扫描、处理、发送的统一容器和导航，不承载数据编辑/处理业务；后续扫描重建的数据处理能力应拆 DLL 或独立库。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `OrderScanWorkspaceShellImpl.cpp`：补充 `QStackedWidget` 页面切换、步骤页面挂载、旧页面 `deleteLater()`、Qt 父子对象、弱引用清空和日志 UTF-8 ABI 的机制说明。
- 本轮只补充注释和文档记录，不改变工作台壳步骤切换或页面挂载逻辑。

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；工作台壳日志 `[Mod:]` 字段和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_OrderScanWorkspaceShell"`，保证后续日志宏输出正确建单扫描工作台壳模块名。

## 2026-06-24

- 对齐新版 Logger 规则：工作台壳当前没有真实操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段。
- 根据“初学者可读”要求补强函数体内部注释：路径缓存、Qt 父子关系、步骤页挂载、旧页释放、Shutdown 弱引用清空、步骤标题映射和日志写入均增加关键说明。
- 补充 `IOrderScanWorkspaceShell` 公共接口、实现类、步骤切换和页面挂载流程的中文注释，明确壳子只管理步骤容器，不实现建单、扫描、处理或发送业务。
- 替换步骤页面时释放旧页面，避免后续挂载真实建单/扫描页面时残留占位资源。
- 重新验证 Release x64 构建通过，0 warning / 0 error。

## 2026-06-23

- 新增 `MyOrderScanWorkspaceShell` 模块骨架，输出 `MeyerScan_OrderScanWorkspaceShell.dll`。
- 新增 `IOrderScanWorkspaceShell` 接口，提供初始化、创建工作台 widget、设置步骤、挂载步骤 widget 和关闭接口。
- 当前提供 Order / Scan / Process / Send 四步占位容器，后续接入 OrderCreateUI 和 ScanReconstructStudio 窗口/进程容器。
- 补充模块规则：本模块是 Qt Widgets UI 容器，可以使用 Qt 控件、布局、信号槽和 Qt 容器组织界面；业务保存、扫描采集和数据处理不进入本模块，跨进程同步时使用 IPC/POD/UTF-8 JSON。

## 2026-07-03
- 新增 `OrderScanWorkspaceShellTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
