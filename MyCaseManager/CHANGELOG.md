# MeyerScan CaseManager Legacy Reference 变更记录

## 2026-06-26

- 新增 README 和 CHANGELOG，明确 `MyCaseManager` 是旧版数据库/旧 schema 参考目录，不是当前重构架构下的活跃案例管理模块。
- 记录边界：当前案例管理 UI 归 `MyCaseUI`，患者/订单/医生/诊所/技工所等数据库领域数据归 `MyCaseOrderService`。
- 明确 `mysql.sql`、SQLite 样例库和旧 MySQL 目录只能作为迁移参考，新代码不得直接依赖这些历史文件。
