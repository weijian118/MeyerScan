# MeyerScan DatabaseQtAdapter 变更记录

## 2026-07-02

- 2026-07-03 复查补充：VS2015 PostBuild 与 CMake 均统一从 `..\ThirdParty\SQLite\win-x64\sqlite3.dll` 复制 x64 SQLite 运行时；单模块 `bin\Release` 已验证不再残留旧 32 位 `sqlite3.dll`。
- 新增 `MyDatabaseQtAdapter` 模块，输出 `MeyerScan_DatabaseQtAdapter.dll`。
- 明确依赖方向：`Qt Service/UI -> DatabaseQtAdapter -> MeyerScan_Database`，Database 不依赖 QtAdapter。
- 提供 `QString`、`QByteArray`、`QJsonDocument` 与 Database UTF-8/POD 接口之间的转换能力。
- 不引入 QtSql，不使用 `QSqlDatabase`、`QSqlQuery`。
- 作为 `RuntimeDataCenter`、`CaseOrderService` 访问纯 C++ Database 的 Qt 便利层。
- VS2015 PostBuild 的 `sqlite3.dll` 复制来源使用仓库内相对路径，避免依赖个人开发机目录。
