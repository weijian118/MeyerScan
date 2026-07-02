# MeyerScan CaseManager Legacy Reference 变更记录

## 2026-07-02

- 新增参考目录 `CMakeLists.txt`，不生成 DLL/EXE，只用于说明 `MyCaseManager` 是旧 schema / `mysql.sql` 参考目录。
- 按评审结论同步本地备份规则：旧 schema 参考资料随全部模块一起进入 `F:\MeyerScan-Reposit`，但旧 MySQL/SQLite 运行数据和第三方文件不作为当前工程产物备份。

## 2026-07-01

- 全模块实现技巧型注释复查时再次确认：`MyCaseManager` 是旧版数据库/旧 schema 参考目录，不作为当前活跃重构模块逐文件补注释；新代码应在 `MyCaseUI`、`MyCaseOrderService`、`MyRuntimeDataCenter` 等当前模块中维护中文注释和实现技巧说明。
- 旧目录内的第三方 MySQL 文件、历史样例库和旧脚本不纳入自研源码注释改造范围，避免破坏后续迁移对照。

## 2026-06-26

- 新增 README 和 CHANGELOG，明确 `MyCaseManager` 是旧版数据库/旧 schema 参考目录，不是当前重构架构下的活跃案例管理模块。
- 记录边界：当前案例管理 UI 归 `MyCaseUI`，患者/订单/医生/诊所/技工所等数据库领域数据归 `MyCaseOrderService`。
- 明确 `mysql.sql`、SQLite 样例库和旧 MySQL 目录只能作为迁移参考，新代码不得直接依赖这些历史文件。

## 2026-06-30

- 补充 RuntimeDataCenter 口径：它可以在内部参考旧表名读取只读 domain JSON 快照，但调用方不得传旧表名或 SQL。
- 再次明确 `mysql.sql` 未被新架构自动执行，只作为旧 schema 和字段迁移映射参考。
