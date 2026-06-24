# MeyerScan CaseOrderService 变更记录

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；服务层日志 `[Mod:]` 字段和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_CaseOrderService"`，保证后续日志宏输出正确服务模块名。
- `CaseOrderServiceResult` 增加 `IsSuccess()` / `IsError()` 辅助方法，调用方后续可少写 `errorCode == 0`，也便于 Core.lib 的 `Result/VoidResult` 落地后集中迁移。

## 2026-06-24

- 对齐新版 Logger 规则：服务层没有真实操作员上下文时，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段，避免输出无意义 `[Op:System]`。
- 根据“初学者可读”要求补强函数体内部注释：固定缓冲区复制、初始化、schema 创建、JSON 保存/读取、参考数据白名单、QueryJson 分发、SQL 转义和 CopyToBuffer 均增加关键说明。
- 补充 `ICaseOrderService` 公共接口中文注释，说明患者/订单组合 JSON、参考数据、稳定查询入口和调用方缓冲区返回约束。
- 补充 CaseOrderService 头文件和实现文件的函数级中文注释，明确本模块负责患者/订单组合数据和参考数据服务，不负责 UI、扫描方案、扫描采集、算法或流程决策。
- 补充稳定查询边界说明：UI/外部适配器只能通过 `QueryJson()` 等服务接口传查询名和 JSON 参数，不允许直接拼 SQL 访问 Database。
- 补充当前 SQL 转义只是骨架期保护，正式 DAO 层后续应改为参数绑定和版本化 migration。

## 2026-06-23

- 新增 `MyCaseOrderService` 模块骨架，输出 `MeyerScan_CaseOrderService.dll`。
- 统一患者、订单、医生、技工所、诊所等数据库域数据服务边界，替代原规划中的 `CaseService.dll` + `OrderService.dll` 双模块。
- 新增 `ICaseOrderService` 接口，当前提供初始化、schema 检查占位、患者/订单 JSON 保存入口、患者/订单 JSON 查询入口、字典/主数据列表查询入口。
- 版本提升到 v0.2.0。
- `EnsureSchema()` 新增轻量表结构占位：`ms_patient_order` 保存患者/订单组合 JSON，`ms_reference_data` 保存医生、诊所、技工所等参考数据。
- `SavePatientOrderJson()` 当前按 `orderId` 保存患者/订单 JSON；`GetPatientOrderJson()` 可按订单 ID 读取组合 JSON。
- `ListReferenceDataJson()` 统一读取医生、诊所、技工所等分类主数据，避免 UI 直接拼业务 SQL。
- 新增 `QueryJson()` 统一查询入口，当前支持 `patientOrder.byOrderId` 和 `referenceData.list` 两个查询名。
- 当前仍属于服务边界与轻量 schema 骨架；正式字段表、迁移脚本、DAO、权限复核和完整 CRUD 后续实现。
- 补充模块规则：CaseOrderService 可以优先使用 Qt Core/JSON/SQL 能力；边界控制职责和所有权，不刻意去 Qt 化。
