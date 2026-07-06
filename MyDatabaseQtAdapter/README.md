# MeyerScan DatabaseQtAdapter 模块

`MeyerScan_DatabaseQtAdapter.dll` 是 Qt 模块访问纯 C++ `MeyerScan_Database.dll` 的便利转换层。

## 模块边界

- 允许使用 `QString`、`QByteArray`、`QJsonDocument`、`QJsonArray` 等 QtCore 类型。
- 只负责 Qt 类型和 Database 的 UTF-8/POD 接口转换。
- 对外新增 `IDatabaseQtAdapter` 纯虚接口，便于 MainExe 通过 `QLibrary + GetDatabaseQtAdapter()` 动态加载 DLL 后按接口使用，降低 MainExe 对 import lib 的静态绑定。
- 不写业务规则，不决定患者、订单、诊所、医生等字段含义。
- 不被 `MeyerScan_Database.dll` 依赖，依赖方向只能是 `Qt Service/UI -> DatabaseQtAdapter -> Database`。
- 不依赖 QtSql，不使用 `QSqlDatabase`、`QSqlQuery`。
- 可以直接使用 `MeyerScan_Logger.dll` 记录适配层边界日志，但只记录生命周期、输入非法、连接失败、SQL 失败、JSON 解析失败、缓冲区超限等关键事件，不逐条记录正常查询成功，避免日志噪声。

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
- 运行目录必须同时存在 `MeyerScan_Database.dll`、`MeyerScan_Logger.dll` 和 x64 `sqlite3.dll`；VS2015 PostBuild 与 CMake 统一从 `ThirdParty/SQLite/win-x64/sqlite3.dll` 复制，不使用旧 SQLiteStudio 32 位 DLL。
- 日志入口在 Adapter 单例内缓存为一份 `ILogger*`，不要在每个执行函数内重复获取 Logger。
- 修改记录写入 `CHANGELOG.md`，中文记录。
- `GetModuleVersion()` 返回 Adapter 自身代码版本，必须与本模块 `src/Version.rc` 同步；`DatabaseModuleVersion()` 返回底层 `MeyerScan_Database.dll` 的版本，两者含义不同，不要混用。

## 测试入口
- VS2015：打开 `MeyerScan_DatabaseQtAdapter.sln`，构建并运行 `DatabaseQtAdapterTest.exe`。
- CMake/VSCode：默认开启 `DatabaseQtAdapterTest` 测试目标，可通过 `MEYER_BUILD_DATABASEQTADAPTERTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
- 2026-07-05 已验证 standalone `MeyerScan_DatabaseQtAdapter.sln` Release x64 可构建；如果 VS2015 再报项目 GUID 缺失，优先检查 `.sln` 中是否重新出现不存在工程依赖。
