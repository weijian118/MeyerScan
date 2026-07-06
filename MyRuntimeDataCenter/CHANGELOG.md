# MeyerScan RuntimeDataCenter 变更记录

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `RuntimeDataCenterImpl.cpp` 中文注释，说明 domain 快照缓存、候选旧表兼容、云端 envelope、调用方 buffer 返回、有限扩容查询、锁粒度和 Logger 降级策略。
- 本轮仅补充注释，不改变 RuntimeDataCenter domain 映射、缓存结构或数据库读取逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 2026-07-03 复查补充：单模块 `MeyerScan_RuntimeDataCenter.sln` 已重新构建，输出目录同步 `MeyerScan_DatabaseQtAdapter.dll`、`MeyerScan_Database.dll` 和 x64 `sqlite3.dll`；`RuntimeDataCenterTest.exe` 通过 Adapter 连接 SQLite 并返回 0。
- 新增模块 `CMakeLists.txt`，同时声明 `RuntimeDataCenterTest.exe`，支持 VSCode/CMake Tools 与 VS2015 生成器构建。
- 按评审结论修正非界面模块 Qt 边界：RuntimeDataCenter 当前 Qt JSON/容器只作为 `.cpp` 内部快照实现，公共 ABI 继续使用 UTF-8 JSON 和调用方缓冲区，后续可替换内部实现。
- 数据库访问改为通过 `MyDatabaseQtAdapter` 进入纯 C++ `MyDatabase`，不再直接包含 `Database.h`；调用方向固定为 `RuntimeDataCenter -> DatabaseQtAdapter -> Database`。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试项目和自研产物。
- 继续按“实现技巧型注释”要求补强 `RuntimeDataCenterImpl.cpp`：补充 `ReloadAll()` 按 domain 独立失败计数、云端 domain `notLoaded` 空快照、旧表候选表选择、domain JSON envelope、调用方缓冲区、有限扩容重试、旧表白名单、订单摘要字段裁剪和快照一次性替换的实现说明。
- 继续补强 `RuntimeDataCenterTest.exe`：说明临时 SQLite 配置为什么使用相对路径、每张最小旧表对应哪个 domain、`REPLACE` / `DELETE + INSERT` 的重复运行策略、调用方缓冲区为什么能规避跨 DLL 字符串释放问题，以及云端 JSON 注入链路的验证目的。
- 本轮仅补充注释和文档记录，不改变 RuntimeDataCenter 快照结构或接口。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `RuntimeDataCenterTest.exe` 测试宿主：补充临时 SQLite 配置写入、旧表最小 schema、`ExecuteUpdate` 测试准备、调用方缓冲区、云端 JSON 注入、`strstr` 内容校验和关闭顺序说明。
- 前一轮已补强 `RuntimeDataCenterImpl.cpp` 中跨 DLL 缓冲区、JSON 快照包装、有限扩容、旧表白名单、空快照统一结构和 Database/Logger 单例借用说明。
- `RuntimeDataCenterTest.exe` 的设备演示表改为优先表 `device_info_tbl2`，与 RuntimeDataCenter 的 `local.devices` 读取顺序保持一致。
- CaseUI / SettingsUI 的 smoke 演示库补齐全部本地 domain 所需的最小轻量表，避免集成测试出现预期缺表 Warning。
- 验证：`RuntimeDataCenterTest.exe` 返回 0；MainExe 单模块目录和根聚合目录的 `MeyerScan.exe --smoke-main` 均返回 0，日志显示 `All runtime domains reloaded`。

## 2026-06-30

- 新增 `MyRuntimeDataCenter` 模块，输出 `MeyerScan_RuntimeDataCenter.dll`。
- 新增 `IRuntimeDataCenter` 接口，提供 `Init()`、`ReloadAll()`、`ReloadDomain()`、`GetDomainJson()`、`UpdateCloudClinicJson()`、`Shutdown()`。
- 本地数据库信息按 domain 缓存为 JSON 快照：诊所、技工所、软件信息、医生、设置、账号、订单、患者、设备。
- 云端诊所信息先通过 JSON 注入内存，后续再由登录/网络模块刷新。
- 订单 domain 只读取列表/上下文常用字段，避免旧表大字段长期占用内存。
- 新增 VS2015 DLL 工程、测试宿主和中文 README。
- 明确模块边界：RuntimeDataCenter 只做只读快照和上下文缓存，不替代 CaseOrderService，不允许调用方传 SQL 或表名。
- `GetDomainJson()` 对未注入的云端 domain 也返回标准空 JSON 快照，调用方可按统一结构读取。
- 测试宿主覆盖 SQLite 默认链路、本地旧表快照读取、云端诊所 JSON 注入和快照读取。
- 查询旧表时改为 1MB 起步、倍增到 32MB 的有限缓冲区重试，避免患者/订单/诊所等字段扩展后因为固定缓冲区过小导致快照读取失败。
- 补充 SQLite 空库和旧表迁移边界：RuntimeDataCenter 只读取快照，不创建业务 schema，不执行旧 `mysql.sql` 迁移。
