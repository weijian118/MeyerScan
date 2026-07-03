# MeyerScan DatabaseQtAdapter 模块

`MeyerScan_DatabaseQtAdapter.dll` 是 Qt 模块访问纯 C++ `MeyerScan_Database.dll` 的便利转换层。

## 模块边界

- 允许使用 `QString`、`QByteArray`、`QJsonDocument`、`QJsonArray` 等 QtCore 类型。
- 只负责 Qt 类型和 Database 的 UTF-8/POD 接口转换。
- 不写业务规则，不决定患者、订单、诊所、医生等字段含义。
- 不被 `MeyerScan_Database.dll` 依赖，依赖方向只能是 `Qt Service/UI -> DatabaseQtAdapter -> Database`。
- 不依赖 QtSql，不使用 `QSqlDatabase`、`QSqlQuery`。

## 当前接口

- `EnsureConnected()`：把 `QString` 配置路径转成 UTF-8，初始化并连接 Database。
- `ExecuteUpdate()`：把 `QString SQL` 转成 UTF-8 后调用 Database。
- `ExecuteScript()`：批量转换并执行 SQL。
- `ExecuteQueryJson()`：自动扩容调用方缓冲区，返回 Database 通用表格 JSON。
- `ExecuteQueryJsonDocument()`：查询后解析成 `QJsonDocument`。
- `RowsFromTableJson()`：从 `{columns, rows}` 通用表格 JSON 中取 `rows`。
- `EscapeSqlText()`：骨架期单引号转义工具，正式 DAO 后续应使用参数绑定能力替代。

## 维护要求

- 本模块是适配层，不是业务服务层。
- 新增 Qt 便利方法前，先确认它是否只是类型转换；如果包含业务字段判断，应放到 `CaseOrderService`、`RuntimeDataCenter` 或对应领域服务。
- 运行目录必须同时存在 `MeyerScan_Database.dll` 和 x64 `sqlite3.dll`；VS2015 PostBuild 与 CMake 统一从 `ThirdParty/SQLite/win-x64/sqlite3.dll` 复制，不使用旧 SQLiteStudio 32 位 DLL。
- 修改记录写入 `CHANGELOG.md`，中文记录。
