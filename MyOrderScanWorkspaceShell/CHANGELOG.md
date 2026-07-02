# MeyerScan OrderScanWorkspaceShell 变更记录

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
