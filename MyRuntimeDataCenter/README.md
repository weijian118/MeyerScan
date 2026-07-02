# MeyerScan RuntimeDataCenter

`MyRuntimeDataCenter` 输出 `MeyerScan_RuntimeDataCenter.dll`，用于缓存本地数据库常用信息和云端诊所信息。

2026-07-02 评审后，RuntimeDataCenter 按非界面读模型模块管理：Qt JSON/容器只能作为内部实现细节；公共 ABI 继续使用 UTF-8 JSON 和调用方缓冲区，后续可替换内部实现而不影响 UI/主程序。

## 当前定位

- 默认配合 SQLite 运行，也继续复用 `MyDatabase` 的 MySQL/SQLite 双数据库能力。
- 本模块是运行时读模型/缓存中心，适合放“多个模块都要读取、字段又经常扩展”的上下文数据。
- 从旧数据库表读取本地诊所、技工所、软件信息、医生、设置、账号、患者、订单、设备信息。
- 云端诊所信息暂不联网读取，由登录/云端同步模块拿到 JSON 后调用 `UpdateCloudClinicJson()` 注入。
- 对外返回 UTF-8 JSON 快照，不返回 Qt 对象、不返回固定 C++ 业务结构体。
- 字段经常增删时，优先改本模块的查询和映射，不让 UI、MainExe、建单模块频繁改 ABI。
- 调用方只能按 domain 读取快照，不能传 SQL、不能传表名，避免本模块变成通用数据库查询口。
- 本模块是读模型/缓存中心，不替代 `MeyerScan_CaseOrderService.dll`。保存、删除、编辑、状态变更和强业务查询仍归 CaseOrderService。
- 缺表、空库或云端数据尚未注入时，本模块会返回标准空 JSON 快照并写 Warning，框架期不阻断 MainExe 启动。
- 默认 SQLite 库首次运行可能是空库；本模块只兼容读取已存在的旧表或后续迁移后的表，不负责创建业务 schema 或迁移旧数据。
- 快照读取采用有限扩容策略，避免字段扩展后固定缓冲区过小；如果某个 domain 超过 32MB，说明该数据已不适合启动期全量缓存，应改为分页读取或 CaseOrderService 查询接口。

## Domain 清单

| domain | 来源 |
|--------|------|
| `local.clinics` | `clinic_tbl` |
| `local.labs` | `lab_tbl2`，失败时尝试 `lab_tbl` |
| `local.software` | `meyer_scan` |
| `local.doctors` | `dentist_tbl` |
| `local.settings` | `soft_init` |
| `local.users` | `user_tbl`，失败时尝试 `user_tbl2` |
| `local.orders` | `order_tbl2`，失败时尝试 `order_tbl` |
| `local.patients` | `patient_tbl2`，失败时尝试 `patient_tbl` |
| `local.devices` | `device_info_tbl2`，失败时尝试 `device_info_tbl` |
| `cloud.clinicProfile` | 外部模块注入的云端诊所 JSON |

`local.orders` 当前只缓存列表/上下文常用字段，避免把旧表里的 `*_TRAN` 大字段一次性读入内存。

## 与 CaseOrderService 的关系

| 场景 | 归属模块 |
|------|----------|
| 首页、案例管理、建单页需要读取当前诊所、医生、技工所、患者列表、订单列表、设备列表等上下文 | `RuntimeDataCenter` 返回 domain JSON 快照 |
| 新建患者/订单、编辑患者/订单、删除患者/订单、订单状态变化、医生/诊所/技工所主数据维护 | `CaseOrderService` |
| 读取一条订单后判断是否能继续扫描、进入处理或发送 | `OrderWorkflowService` 读取 `CaseOrderService`、`ScanSchemaService`、`Permission` 的结果后决策 |
| 直接执行 SQL、事务、备份、数据库类型切换 | `Database` |

## 边界

- 不做 UI 渲染。
- 不做权限判断。
- 不做订单流程判断。
- 不做云端请求。
- 不做患者/订单保存，正式 CRUD 仍归 `CaseOrderService`。
- 不允许调用方传 SQL 或表名；所有查询都在模块内部白名单维护。

## 数据形态

每个 domain 返回紧凑 JSON，基本结构如下：

```json
{
  "schemaVersion": 1,
  "domain": "local.patients",
  "source": "MeyerScan_RuntimeDataCenter",
  "loadedAtUtc": "2026-06-30T01:00:00Z",
  "databaseTable": "patient_tbl2",
  "loadStatus": "ok",
  "rowCount": 1,
  "columns": ["PATIENT_ID", "PATIENT_NAME"],
  "items": []
}
```

字段扩展规则：

- 新增字段优先自然出现在 `items` 对象中，调用方只读取自己需要的 key。
- 需要统一重命名或派生字段时，先在本模块内部做兼容映射，再更新 README。
- 不把这些高频变化字段做成跨 DLL 固定结构体，避免字段变化导致 UI、MainExe、测试宿主同时改 ABI。
- 若某个 domain 的结构变化会影响多个模块，必须同步更新本 README、全局架构文档和调用方 smoke 测试。
- 快照只适合“列表/上下文/字典/当前环境”这类轻量数据；扫描数据、大文本历史、大批量订单导出等必须走专用 Service、DataExport 或分页接口。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_RuntimeDataCenter.sln /p:Configuration=Release /p:Platform=x64
```

## 验证

```powershell
cd F:\MeyerScan\MyRuntimeDataCenter\bin\Release
.\RuntimeDataCenterTest.exe
```

## 2026-07-01 测试数据规则

- RuntimeDataCenter 正式模块只读取快照，不创建业务表、不执行旧 `mysql.sql`、不做 schema migration。
- `RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke` 可以准备最小 SQLite 演示库，用于验证数据库到 UI 的链路。
- 演示库必须覆盖 `Domain 清单` 中全部本地 domain 的最小表结构：诊所、技工所、软件信息、医生、设置、账号、患者、订单、设备。
- 新增本地 domain 时，必须同步修改 RuntimeDataCenter README、测试宿主演示数据和 MainExe smoke 验证，避免后续日志被预期缺表 Warning 淹没。
