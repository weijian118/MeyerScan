# MeyerScan CaseOrderService 变更记录

## 2026-07-06

- 修复根聚合 CMake 构建中 `CaseOrderServiceTest` 依赖不完整的问题：测试目标补齐 `MyDatabaseQtAdapter` 依赖，保证测试宿主在 VSCode/CMake 和 VS2015 聚合构建中都能找到数据库适配层。
- 复核当前服务链路仍保持 `CaseOrderService -> DatabaseQtAdapter -> Database`，公共接口继续使用 UTF-8 JSON 和调用方缓冲区，不把 Qt 类型暴露到跨 DLL 边界。
- 已在根聚合 CMake `Release` 构建中验证本模块和测试宿主可以随全工程编译通过；CMake 使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 与 VS2015 x64 生成器。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `CaseOrderServiceTest.exe` 测试宿主中文注释和文件级阅读说明，说明 SQLite 测试配置、服务层建表、患者/订单 JSON 保存读取、参考数据造数、`QueryJson` 扩展入口和跨 DLL buffer 返回策略。
- 本轮仅补充注释，不改变 CaseOrderService 业务逻辑和数据库表结构。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；`CaseOrderServiceTest.exe` 返回 0；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-03

- 复核所有模块落实“Database 去 Qt + DatabaseQtAdapter 中介链路”时，清理 `CaseOrderServiceImpl.cpp` 中残留的 `QSqlQuery` 参数绑定说明，改为描述后续应在 Database/DAO 层提供参数绑定能力。
- 该调整只修正文档化注释，不改变当前 `CaseOrderService -> DatabaseQtAdapter -> Database` 的运行逻辑；目的是避免后续维护者误以为服务层可以重新引入 QtSql。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论修正非界面模块 Qt 边界：当前 Qt JSON 只作为 `.cpp` 内部字段映射实现，公共头文件继续使用 UTF-8 JSON 和调用方缓冲区，后续可替换内部实现而不影响 UI/主程序。
- 数据库访问改为通过 `MyDatabaseQtAdapter` 进入纯 C++ `MyDatabase`，不再直接包含 `Database.h` 或链接 `MeyerScan_Database.lib`；调用方向固定为 `CaseOrderService -> DatabaseQtAdapter -> Database`。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求继续补强 `CaseOrderServiceImpl.cpp`：补充 C ABI 固定结构体返回、`QByteArray::constData()` 生命周期、JSON compact 存储、调用方缓冲区、静态数组脚本计数、SQL 白名单和骨架期 SQL 转义限制说明。
- 本轮只补充注释和文档记录，不改变患者/订单 JSON 保存、参考数据查询或 schema 占位逻辑。

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
- 历史口径曾允许 CaseOrderService 优先使用 Qt Core/JSON/SQL；2026-07-02 评审后修正为非界面模块优先评估非 Qt 实现，当前 Qt JSON 仅作为内部实现细节，公共 ABI 不暴露 Qt 类型。

## 2026-07-03
- 新增 `CaseOrderServiceTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
