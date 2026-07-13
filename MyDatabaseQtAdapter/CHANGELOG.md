# MeyerScan DatabaseQtAdapter 变更记录

## 2026-07-13

- 测试宿主改用 Logger QString 重载，并在打印数据库错误前保存命名 UTF-8 缓冲区；Adapter/Database 接口和数据库行为不变。
- `DatabaseQtAdapterTest` 已登记到根 CTest 清单，用于统一验证 Qt 类型转换和 SQLite 链路。

## 2026-07-10

- `Version.rc` 补齐 `LegalCopyright` 文件详细信息字段；Qt/C++ 类型转换、缓冲区扩容和 Database 单向依赖边界保持不变。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。
- 新增 `IDatabaseQtAdapter` 纯虚接口，`DatabaseQtAdapter` 实现该接口；MainExe 可通过 `QLibrary` 动态加载 `MeyerScan_DatabaseQtAdapter.dll` 后按接口使用，不再需要链接 `MeyerScan_DatabaseQtAdapter.lib`。
- 新增 `GetModuleVersion()`，返回 Adapter 自身代码版本，供 MainExe 运行时版本清单记录 `codeVersion`；原有 `DatabaseModuleVersion()` 继续返回底层 `MeyerScan_Database.dll` 版本。
- 继续兼容既有 `GetDatabaseQtAdapter()` 返回具体类指针的调用方式，避免影响当前测试宿主和服务模块源码。
- 清理 standalone `MeyerScan_DatabaseQtAdapter.sln` 中测试项目指向不存在工程的 stale GUID 依赖，避免 VS2015 打开/构建该解决方案时报错或卡住。
- 验证：`MeyerScan_DatabaseQtAdapter.sln` Release x64 构建通过；根/单模块 MainExe 版本清单均可读取 Adapter 自身 `codeVersion`，并与 `Version.rc` 的 `fileVersion=0.1.0.0` 匹配。

## 2026-07-04

- 补充 `DatabaseQtAdapter.cpp` 和 `DatabaseQtAdapterTest.exe` 中文注释，说明 Qt 类型到纯 C++ Database 的边界转换、`QByteArray::constData()` 生命周期、查询 JSON 有限扩容、错误日志不打印完整 SQL 和测试数据库隔离策略。
- 本轮仅补充注释，不改变 DatabaseQtAdapter 连接、查询、脚本执行或日志调用逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；`DatabaseQtAdapterTest.exe` 返回 0；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

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
