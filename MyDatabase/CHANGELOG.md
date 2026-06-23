# MeyerScan Database 变更记录

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
