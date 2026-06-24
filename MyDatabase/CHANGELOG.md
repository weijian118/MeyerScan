# MeyerScan Database 变更记录

## 2026-06-25

- 修正 `DatabaseTest` PostBuild 中 `libmysql.dll` 的复制来源，改为从既有安装目录 `C:\Program Files (x86)\MeyerScan\libmysql.dll` 复制，避免构建时继续访问不存在的 `F:\MeyerScan\MySQL\lib`。
- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；Database 日志模块名和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 工程一致性方向，为 `DatabaseTest` Debug/Release 工程补充 `MEYER_MODULE_NAME="DatabaseTest"`，保证测试宿主日志可追溯。
- 复核 `Version.rc` 已符合统一公司名、产品名、命名常量和 `FileDescription` 规范，无需额外修改数据库模块版本资源。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：配置路径解析、Qt SQL 连接生命周期、查询 JSON 转换、备份、事务、数据库类型切换、日志动态加载和 DatabaseTest 路径/备份逻辑均增加关键说明。
- 补充 `Result<T>`、`VoidResult`、错误码辅助函数和 `DatabaseTest` 路径辅助函数的中文注释，确保测试项目和公共结果结构同样满足函数级注释要求。
- 复查自研源码注释覆盖，确认 Database 公共接口、实现函数和测试宿主均有函数级中文说明；第三方 MySQL 头文件不纳入自研注释修改范围。

## 2026-06-23

- 版本提升到 v1.2.0。
- 新增 `DbJsonResult` 和 `IDatabase::ExecuteQueryJson()`，用于把通用 SELECT 查询结果转成调用方缓冲区中的 UTF-8 JSON。
- 新增测试覆盖 `ExecuteQueryJson()` 的最小常量查询结果。
- 明确该接口只做行列结果转换，不理解患者、订单、医生、诊所、技工所等业务语义；UI 仍不得绕过 CaseOrderService 等领域服务直接拼业务 SQL。
- 新增虚函数按 ABI 规则追加在接口末尾，避免插入已有虚函数中间。
- 明确 Database 使用 Qt SQL 是当前设计选择，职责边界要求是不承载业务语义，不是去 Qt 化。
- 2026-06-24 复查验证：Release x64 构建通过，`DatabaseTest.exe` 23 passed / 0 failed。

## 2026-06-22

- 新增模块级变更记录文件。
- 重新确认 Database 边界：只负责数据库连接、事务、SQL 执行、备份、MySQL/SQLite 适配等基础设施能力。
- 业务 schema、migration 选择、病例/订单语义、权限判断和 UI 展示必须留在本模块之外。
- 现有 MySQL 账号密码硬编码和备份路径硬编码仍作为 TODO 保留，等待 ConfigCenter 接入后统一处理，避免现在做半套配置方案。
- `Disconnect()` 和数据库类型切换时显式从 Qt 连接池移除 SQL 连接，避免 UI smoke 宿主退出时出现 Qt SQL 插件或生命周期顺序问题。
- 2026-06-23 补充：DatabaseTest 不再硬编码 `F:/MeyerScan/...` 作为日志、备份目标和配置路径，改为根据测试程序所在目录推导模块根目录。
- 2026-06-23 补充：`DbConfig` 增加 `mysqlDataDir` 字段，MySQL 备份源目录改为从 `config/db_config.json` 的 `mysql.dataDir` 读取；后续再迁入 ConfigCenter/Crypto。
- 2026-06-23 复查优化：`db_config.json` 改为使用相对路径；Database 以配置文件所在目录为基准解析 `mysql.dataDir` 和 `sqlitePath`，禁止回退到开发机绝对路径或当前工作目录。
- 2026-06-23 复查优化：DatabaseTest 补齐 Qt 运行库、SQL 驱动和 `libmysql.dll` 的 PostBuild 复制规则；测试宿主在 `bin/Release/config` 生成独立测试配置，避免混用正式发布配置；重新验证 21 项通过、0 失败。

## 2026-06-18

- 确认旧 `MyCaseManager/mysql.sql` 不由 Database 自动加载。
- 将 `mysql.sql` 记录为旧 schema 参考文件。
- 保留 `ExecuteScript()` 作为通用 SQL 执行能力；schema 版本管理和 migration 选择归 ConfigCenter 或业务 Service 模块负责。

## 2026-06-17

- 接口统一调整为 `Result<T>` / `VoidResult` 风格。
- 通过运行时动态加载方式接入 Logger。
- 修复 `SetDatabaseType()` 递归锁导致的死锁风险。
- 修复 SQL 错误消息生命周期导致的悬空指针风险。
- 验证 Release x64 构建和 `DatabaseTest`。
