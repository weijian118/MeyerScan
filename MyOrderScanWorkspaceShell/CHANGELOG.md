# MeyerScan OrderScanWorkspaceShell 变更记录

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
- 补充模块规则：本模块是 Qt Widgets UI 容器，可以优先使用 Qt 控件、布局、信号槽和 Qt 容器；跨进程同步时再使用 IPC/POD/UTF-8 JSON。
