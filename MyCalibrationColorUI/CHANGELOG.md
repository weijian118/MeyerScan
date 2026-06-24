# MeyerScan CalibrationColorUI 变更记录

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；颜色校准日志 `[Mod:]` 字段和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_CalibrationColorUI"`，保证后续日志宏输出正确颜色校准模块名。

## 2026-06-24

- 对齐新版 Logger 规则：当前颜色校准页面没有真实操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段。
- 根据“初学者可读”要求补强函数体内部注释：路径缓存、Qt 父子关系、布局创建、tr 英文 source text、占位区、Start/Cancel 占位动作、Flush 和 Logger UTF-8 ABI 均增加关键说明。
- 补充 `ICalibrationColorUI` 公共接口、实现类和关键界面创建流程的中文注释，明确颜色校准 UI、算法接入和设备接入都收敛在本模块内部。
- 新增 `MyCalibrationColorUI` 模块骨架，输出 `MeyerScan_CalibrationColorUI.dll`。
- 新增 `ICalibrationColorUI` 接口，当前提供初始化、创建 Qt Widgets 页面、版本查询和释放入口。
- 明确颜色校准 UI、流程编排和颜色校正参数生成入口归本模块，后续算法 DLL、DeviceCmd、DeviceTransport 在本模块内部接入。
- 明确本模块可以使用 Qt 默认能力，边界重点是职责隔离和跨进程对象所有权。
- 验证 Release x64 构建通过，0 warning / 0 error。
