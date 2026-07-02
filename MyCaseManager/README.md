# MeyerScan CaseManager Legacy Reference

`MyCaseManager` 是旧版案例/数据库资料参考目录，不是当前重构架构下的活跃 UI 或服务模块。

## 当前定位

- 保留旧版 `mysql.sql`、SQLite 数据库样例、MySQL 运行目录和历史表结构，用于理解旧软件患者、订单、医生、诊所、技工所等数据组织方式。
- `mysql.sql` 没有被新架构自动执行或直接加载；当前只作为旧表名、旧字段和迁移映射参考。
- 当前新架构的案例管理界面是 `MyCaseUI`，产物为 `MeyerScan_CaseUI.dll`。
- 当前新架构的患者/订单/参考数据服务是 `MyCaseOrderService`，产物为 `MeyerScan_CaseOrderService.dll`。
- 当前新架构的本地/云端常用信息只读快照是 `MyRuntimeDataCenter`，产物为 `MeyerScan_RuntimeDataCenter.dll`，它会参考旧表结构读取 domain JSON。
- `MyDatabase` 只提供数据库连接、SQL 执行、事务和通用查询能力，不理解患者/订单业务语义。

## 使用规则

- 新代码不要直接依赖本目录的旧数据库文件、旧 MySQL 目录或 `mysql.sql`。
- 旧 schema 只能作为字段迁移、数据映射和兼容性分析参考。
- RuntimeDataCenter 内部可以参考旧表名做只读快照白名单，但调用方不得传旧表名或 SQL。
- 正式患者/订单、医生、诊所、技工所等数据读写必须走 `CaseOrderService`，UI 不得绕过服务层直接拼业务 SQL。
- 若后续需要迁移旧数据，应新建独立 migration/adapter 代码，不在本目录中继续堆业务实现。

## 维护注意

- 本目录可能包含旧版数据库程序、测试数据和第三方 MySQL 文件，不能按当前自研模块的源码注释和构建规则要求逐文件改造。
- 如果某个旧表或旧字段被确认仍需沿用，应在 `MyCaseOrderService` 或后续 migration 文档中记录映射关系，而不是只在本目录里隐式保留。
