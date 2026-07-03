# MeyerScan DatabaseQtAdapter 变更记录

## 2026-07-03

- 按日志模块设计初衷，`MyDatabaseQtAdapter` 正式接入 `MeyerScan_Logger.dll`；模块内部缓存一份 `ILogger*`，生命周期内持续复用。
- VS2015 工程和 CMake 同步链接 `MeyerScan_Logger.lib`，输出目录复制 `MeyerScan_Logger.dll`；根聚合 `MeyerScan_AllModules.sln` 补充 Adapter 对 Logger 的构建依赖，避免并行构建顺序问题。
- 日志粒度收口为适配层边界日志：连接/切库/断开/关闭写少量生命周期日志；输入无效、连接失败、SQL 执行失败、JSON 解析失败、查询缓冲区超限写 Warning/Error；正常高频查询成功不逐条写，避免日志噪声。

## 2026-07-02

- 2026-07-03 复查补充：VS2015 PostBuild 与 CMake 均统一从 `..\ThirdParty\SQLite\win-x64\sqlite3.dll` 复制 x64 SQLite 运行时；单模块 `bin\Release` 已验证不再残留旧 32 位 `sqlite3.dll`。
- 新增 `MyDatabaseQtAdapter` 模块，输出 `MeyerScan_DatabaseQtAdapter.dll`。
- 明确依赖方向：`Qt Service/UI -> DatabaseQtAdapter -> MeyerScan_Database`，Database 不依赖 QtAdapter。
- 提供 `QString`、`QByteArray`、`QJsonDocument` 与 Database UTF-8/POD 接口之间的转换能力。
- 不引入 QtSql，不使用 `QSqlDatabase`、`QSqlQuery`。
- 作为 `RuntimeDataCenter`、`CaseOrderService` 访问纯 C++ Database 的 Qt 便利层。
- VS2015 PostBuild 的 `sqlite3.dll` 复制来源使用仓库内相对路径，避免依赖个人开发机目录。

## 2026-07-03
- 新增 `DatabaseQtAdapterTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
