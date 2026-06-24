# MeyerScan CaseOrderService

`MyCaseOrderService` 输出 `MeyerScan_CaseOrderService.dll`，用于统一管理患者、订单、医生、技工所、诊所等与数据库强相关的领域数据。

## 当前定位

- 患者和订单在口扫软件内强关联，当前不再拆成 `CaseService.dll` 和 `OrderService.dll` 两个模块。
- 本模块负责患者/订单组合数据、医生列表、技工所列表、诊所列表和后续类似数据库字典/主数据的服务边界。
- 对外先使用 UTF-8 JSON 字符串承载患者/订单组合数据，避免患者/订单字段频繁变化时破坏 DLL ABI。
- 当前已提供轻量 schema 占位：`ms_patient_order` 存患者/订单组合 JSON，`ms_reference_data` 存医生、诊所、技工所等分类主数据。
- 当前实现可保存/读取患者订单 JSON、列出参考数据，并预留 `QueryJson()` 统一查询入口；正式字段表、迁移脚本、DAO 和权限复核后续继续补充。
- 依赖 `MeyerScan_Database.dll v1.2.0+` 的 `ExecuteQueryJson()` 基础能力。
- 本模块允许并优先使用 Qt 默认能力，包括 `QString`、`QJsonDocument/QJsonObject`、`QVariantMap`、`QDateTime` 和 Qt SQL 相关类型；不为“架构干净”刻意规避 Qt。需要收敛为 `const char*` / 调用方缓冲区的只是对外稳定 ABI 和跨进程/第三方边界。

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
