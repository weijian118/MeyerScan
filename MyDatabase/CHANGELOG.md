# MeyerScan Database 变更记录

## 2026-07-23

- 自检修正 `Database.h` 和 `DatabaseTest` 文件头中的过期版本说明，对齐当前 `v1.3.0`；不改变数据库 ABI 或运行行为。

## 2026-07-06

- 修正 `Database.h` 中容易造成边界回退的旧说明：Database 只作为纯 C++ 数据库基础设施，供 DatabaseQtAdapter、领域服务、迁移工具、统计/导出等数据访问边界使用；Permission/UI 等上层模块不得绕过 Adapter 或 Service 直接执行业务 SQL。
- 复核当前实现仍不包含 QtCore、QtSql、QString、QSqlDatabase、QSqlQuery 或 QMutex，SQLite 继续通过原生 `sqlite3.dll` C API 动态加载。
- 已在根聚合 CMake `Release` 构建中验证 Database 与 DatabaseTest 可以随全工程编译通过；CMake 使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 与 VS2015 x64 生成器。

## 2026-07-10

- 修复 `.vscode/tasks.json` 中 Windows 反斜杠未做 JSON 转义的问题，统一改为正斜杠路径，保证 VSCode 可以解析并执行 MSBuild/测试任务。
- 修正 `Database.h` 中把 ErrorCode/Result 迁入 Core.lib 视为必做项的旧 TODO；当前结果类型属于 Database 公共 ABI，只有多个基础模块形成稳定一致合同后才评估抽取，本轮不修改接口布局和运行逻辑。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `DatabaseImpl.cpp` 中文注释和阅读重点，重点说明纯 C++ ABI、POD/UTF-8 边界、SQLite 动态加载、跨 DLL 字符串生命周期、调用方缓冲区、路径解析、手写轻量 JSON 解析和 SQLite 查询序列化技巧。
- 本轮仅补充注释，不改变数据库连接、事务、备份、SQLite 查询或配置解析行为。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-03

- 复测发现旧 `sqlite3.dll` 来源为 32 位 SQLiteStudio 运行库，x64 `DatabaseTest.exe` 会加载失败；已统一 VS2015 PostBuild 和 CMake 复制来源为 `..\ThirdParty\SQLite\win-x64\sqlite3.dll`。
- 新增 `ThirdParty/SQLite/win-x64/README.md`，说明 x64 SQLite 运行时的放置要求；第三方 DLL 本体不提交 GitHub/本地仓库。
- `DatabaseTest.exe` 固定使用 SQLite 测试配置，SQLite 连接失败直接记为失败并输出底层错误，不再用“MySQL 可能未运行”跳过。
- `DatabaseImpl` 增加最近底层错误缓存，`LoadLibraryA("sqlite3.dll")` 失败时返回带 Win32 错误码的说明，便于定位 DLL 缺失或位数不匹配。
- 验证：`MeyerScan_Database.sln` Release x64 通过，`DatabaseTest.exe` 24 passed / 0 failed；关键输出目录中的 `sqlite3.dll` 均确认为 x64。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，同时声明 `DatabaseTest.exe`，支持 VSCode/CMake Tools 与 VS2015 生成器构建。
- 按评审结论推进非界面模块去 Qt：Database 已改为纯 C++ 实现，不再链接 QtCore/QtSql；SQLite 通过运行时动态加载 `sqlite3.dll` 调用 C API，MySQL 原生链路待 MySQL C API SDK 接入。
- VS2015 和 CMake 中 Database/DatabaseTest 改为静态 CRT（`/MT`），减少基础设施运行依赖；跨模块接口仍保持 POD、UTF-8 和调用方缓冲区，避免跨 DLL 释放内存。
- Qt 模块访问 Database 的 `QString`、`QByteArray`、`QJsonDocument` 转换能力移入 `MyDatabaseQtAdapter`，调用方向统一为 `Qt UI/Service -> DatabaseQtAdapter -> Database`。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试项目和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求继续补强 `DatabaseTest.exe`：补充 VS2015 不使用 `std::filesystem` 的路径处理、`GetModuleFileNameA`、`CreateDirectoryA`、测试配置写入、测试日志兜底和重复调用验证说明。
- 前一轮已补强 `DatabaseImpl.cpp` 中锁保护、UTF-8 路径、JSON 默认值、POD ABI、SQL UTF-8 和 UI 不应直接传业务 SQL 等说明；2026-07-03 起锁实现改为 `std::mutex`。
- 本轮只补充注释和文档记录，不改变数据库连接、事务、备份或 SQLite 默认链路。

## 2026-06-30

- 默认数据库配置切换为 SQLite，`config/db_config.json` 中 `databaseType` 改为 `sqlite`。
- SQLite 连接前自动创建数据库文件父目录，避免首次安装或首次运行时 `Data` 目录不存在导致连接失败。
- `DatabaseTest.exe` 默认生成 SQLite 测试配置，并按当前数据库类型选择列出表 SQL，避免日常验证依赖本机 MySQL 服务状态。
- 历史 QtSql 版本曾统一 Qt DLL、SQL 驱动复制来源；2026-07-03 起 DatabaseTest 不再依赖 `QCoreApplication` 或 Qt SQL 驱动，只需复制 x64 `sqlite3.dll`。

## 2026-06-25

- 修正 `DatabaseTest` PostBuild 中 `libmysql.dll` 的复制来源，改为从既有安装目录 `C:\Program Files (x86)\MeyerScan\libmysql.dll` 复制，避免构建时继续访问不存在的 `F:\MeyerScan\MySQL\lib`。
- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；Database 日志模块名和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 工程一致性方向，为 `DatabaseTest` Debug/Release 工程补充 `MEYER_MODULE_NAME="DatabaseTest"`，保证测试宿主日志可追溯。
- 复核 `Version.rc` 已符合统一公司名、产品名、命名常量和 `FileDescription` 规范。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：配置路径解析、数据库连接生命周期、查询 JSON 转换、备份、事务、数据库类型切换、日志动态加载和 DatabaseTest 路径/备份逻辑均增加关键说明。
- 补充 `Result<T>`、`VoidResult`、错误码辅助函数和 `DatabaseTest` 路径辅助函数的中文注释，确保测试项目和公共结果结构同样满足函数级注释要求。
- 复查自研源码注释覆盖，确认 Database 公共接口、实现函数和测试宿主均有函数级中文说明；第三方 MySQL 头文件不纳入自研注释修改范围。

## 2026-06-23

- 版本提升到 v1.2.0。
- 新增 `DbJsonResult` 和 `IDatabase::ExecuteQueryJson()`，用于把通用 SELECT 查询结果转成调用方缓冲区中的 UTF-8 JSON。
- 新增测试覆盖 `ExecuteQueryJson()` 的最小常量查询结果。
- 明确该接口只做行列结果转换，不理解患者、订单、医生、诊所、技工所等业务语义；UI 仍不得绕过 CaseOrderService 等领域服务直接拼业务 SQL。
- 新增虚函数按 ABI 规则追加在接口末尾，避免插入已有虚函数中间。
- 当时明确 Database 使用 Qt SQL 是过渡设计选择；2026-07-03 起当前实现已改为纯 C++，历史记录仅用于追溯。

## 2026-06-22

- 新增模块级变更记录文件。
- 重新确认 Database 边界：只负责数据库连接、事务、SQL 执行、备份、MySQL/SQLite 适配等基础设施能力。
- 业务 schema、migration 选择、病例/订单语义、权限判断和 UI 展示必须留在本模块之外。
- 现有 MySQL 账号密码硬编码和备份路径硬编码仍作为 TODO 保留，等待 ConfigCenter 接入后统一处理。
- 历史 QtSql 版本在 `Disconnect()` 和数据库类型切换时显式从 Qt 连接池移除 SQL 连接；当前纯 C++ 版本关闭 SQLite 原生连接句柄。
- `DatabaseTest` 不再硬编码 `F:/MeyerScan/...` 作为日志、备份目标和配置路径，改为根据测试程序所在目录推导模块根目录。
- `DbConfig` 增加 `mysqlDataDir` 字段，MySQL 备份源目录改为从 `config/db_config.json` 的 `mysql.dataDir` 读取，后续再迁入 ConfigCenter/Crypto。
- `db_config.json` 改为使用相对路径，Database 以配置文件所在目录为基准解析 `mysql.dataDir` 和 `sqlitePath`，禁止回退到开发机绝对路径或当前工作目录。

## 2026-06-18

- 确认旧 `MyCaseManager/mysql.sql` 不由 Database 自动加载。
- 将 `mysql.sql` 记录为旧 schema 参考文件。
- 保留 `ExecuteScript()` 作为通用 SQL 执行能力；schema 版本管理和 migration 选择由 ConfigCenter 或业务 Service 模块负责。

## 2026-06-17

- 接口统一调整为 `Result<T>` / `VoidResult` 风格。
- 通过运行时动态加载方式接入 Logger。
- 修复 `SetDatabaseType()` 递归锁导致的死锁风险。
- 修复 SQL 错误消息生命周期导致的悬空指针风险。
- 验证 Release x64 构建和 `DatabaseTest`。
