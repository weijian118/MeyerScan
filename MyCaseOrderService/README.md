# MeyerScan CaseOrderService

`MyCaseOrderService` 输出 `MeyerScan_CaseOrderService.dll`，用于统一管理患者、订单、医生、技工所、诊所等与数据库强相关的领域数据。

2026-07-02 评审后，CaseOrderService 按非界面业务服务管理：后续新增能力优先评估非 Qt 实现。当前 Qt JSON 只作为内部字段映射实现，公共接口继续使用 UTF-8 JSON 和调用方缓冲区。

## 当前定位

- 患者和订单在口扫软件内强关联，当前不再拆成 `CaseService.dll` 和 `OrderService.dll` 两个模块。
- 本模块负责患者/订单组合数据、医生列表、技工所列表、诊所列表和后续类似数据库字典/主数据的服务边界。
- 对外先使用 UTF-8 JSON 字符串承载患者/订单组合数据，避免患者/订单字段频繁变化时破坏 DLL ABI。
- 当前已提供轻量 schema 占位：`ms_patient_order` 存患者/订单组合 JSON，`ms_reference_data` 存医生、诊所、技工所等分类主数据。
- 当前实现可保存/读取患者订单 JSON、列出参考数据，并预留 `QueryJson()` 统一查询入口；正式字段表、迁移脚本、DAO 和权限复核后续继续补充。
- 通过 `MeyerScan_DatabaseQtAdapter.dll` 访问 `MeyerScan_Database.dll v1.3.0+` 的 `ExecuteQueryJson()` 基础能力；本模块不直接包含 `Database.h`。
- 当前内部可继续使用 Qt JSON 维持已跑通的骨架链路，但 Qt/Database 类型转换集中在 `MyDatabaseQtAdapter`，Qt 不进入公共头文件和长期 ABI；后续可替换为非 Qt JSON/DAO 实现，调用方接口保持稳定。

## 边界

- 不做 UI 渲染。
- 不做扫描采集、扫描算法、设备通信。
- 不做扫描方案结构化保存，扫描方案仍归 `ScanSchemaService`。
- 不直接决定加载订单后进入建单、扫描、处理或发送，该规则归 `OrderWorkflowService`。
- 不把医生、诊所、技工所等主数据散落到 UI 内部；这类数据统一通过本模块读写。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseOrderService.sln /p:Configuration=Release /p:Platform=x64
```
