// =============================================================================
// 文件:    DatabaseQtAdapter.cpp
// 模块:    MeyerScan_DatabaseQtAdapter.dll
//
// 用途:
//   实现 Qt 类型与 MeyerScan_Database 纯 C++ 接口之间的转换。
//
// 模块边界:
//   - 上层 Qt UI/Service 模块可以传 QString、QStringList、QByteArray、QJsonDocument。
//   - 下层 MyDatabase 仍只接收 UTF-8 const char*、POD Result、调用方缓冲区。
//   - Adapter 不拥有 Database 和 Logger 生命周期，只缓存接口指针并写边界日志。
//
// 阅读重点:
//   - QByteArray 局部变量必须覆盖 constData() 被使用的整个调用周期。
//   - 查询 JSON 采用“调用方缓冲区 + 受控倍增”策略，避免跨 CRT 分配/释放内存。
//   - 失败日志记录 SQL 长度，不直接记录完整 SQL，避免日志泄露业务数据。
// =============================================================================

#include "DatabaseQtAdapter.h"

#include "Database.h"
#include "Logger.h"

#include <QJsonObject>
#include <QtGlobal>
#include <cstring>
#include <vector>

namespace {

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj/CMake 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_DatabaseQtAdapter";

// Adapter 自己的代码版本用于 MainExe 运行时版本清单。
// 这里必须与 src/Version.rc 的 FILEVERSION / ProductVersion 保持同步。
const char* Version = "MeyerScan_DatabaseQtAdapter v0.1.1 (2026-07-15)";
}

// 获取底层 Database 单例。
// 这个函数只存在于 cpp 内部，头文件不暴露 IDatabase，防止 UI/Service 绕过适配层。
IDatabase* RawDatabase(QString* errorMessage = nullptr) {
    // GetDatabase() 是 MyDatabase.dll 的 C ABI 工厂函数。
    // Adapter 只在 cpp 中调用它，避免其它 Qt 模块直接依赖 Database.h。
    IDatabase* database = GetDatabase();
    if (!database && errorMessage) {
        // 这里写入 Qt 字符串，调用方可以直接显示或写日志。
        *errorMessage = "Database instance unavailable";
    }
    return database;
}

// 把底层 Database 返回的消息转换为 QString。
// message 可能为空指针或空字符串，因此所有调用点统一走这里做兜底。
QString DatabaseResultMessage(const char* message, const QString& fallback) {
    if (message && message[0]) {
        return QString::fromUtf8(message);
    }
    return fallback;
}

// 按枚举切换底层数据库类型。
// DatabaseType 来自 Database.h，只放在 cpp 内部，避免把底层枚举扩散到 Qt 模块头文件。
bool SetRawDatabaseType(DatabaseType databaseType, QString* errorMessage) {
    // 先拿到底层数据库实例；失败时错误信息已经由 RawDatabase 填好。
    IDatabase* database = RawDatabase(errorMessage);
    if (!database) {
        return false;
    }

    // SetDatabaseType 是底层接口；当前实现会断开旧连接并按新类型尝试连接。
    VoidResult result = database->SetDatabaseType(databaseType);
    if (result.IsError()) {
        if (errorMessage) {
            // Database 返回的是 char 数组，Adapter 转成 QString 供 UI 层使用。
            *errorMessage = DatabaseResultMessage(result.message, "Database type switch failed");
        }
        return false;
    }

    return database->IsConnected();
}

} // namespace

// 返回适配器单例。
DatabaseQtAdapter& DatabaseQtAdapter::Instance() {
    // C++11 保证函数内 static 初始化是线程安全的。
    static DatabaseQtAdapter adapter;
    return adapter;
}

// C ABI 工厂函数。
extern "C" MEYERSCAN_DATABASEQTADAPTER_API DatabaseQtAdapter* GetDatabaseQtAdapter() {
    return &DatabaseQtAdapter::Instance();
}

// 统一版本导出函数。
// 版本清单读取 Adapter 自身代码版本时走这个轻量入口，
// 避免为了读取版本而创建/连接底层 Database。
extern "C" MEYERSCAN_DATABASEQTADAPTER_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}

// 返回数据库 Qt 适配器公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return 1;
}

// 初始化并连接底层数据库。
bool DatabaseQtAdapter::EnsureConnected(const QString& configPath, QString* errorMessage) {
    // 所有公开方法先通过 RawDatabase 获取底层对象，统一处理 DLL 未加载等异常。
    IDatabase* database = RawDatabase(errorMessage);
    if (!database) {
        WriteError("EnsureConnected", errorMessage ? *errorMessage : "Database instance unavailable");
        return false;
    }

    // 已连接时直接返回，避免重复 Init/Connect 扰动现有连接。
    if (database->IsConnected()) {
        // 已连接时不重复初始化，避免把已有 SQLite 连接重置掉。
        return true;
    }

    // QString 内部是 UTF-16，Database 公共接口要求 UTF-8 const char*。
    // QByteArray 必须保存为局部变量，保证 constData() 在 Init 调用期间有效。
    const QByteArray configBytes = configPath.toUtf8();
    VoidResult initResult = database->Init(configBytes.constData());
    if (initResult.IsError()) {
        // ResultMessage 负责空 message 兜底，避免日志里出现空错误。
        const QString message = ResultMessage(initResult.message, "Database init failed");
        if (errorMessage) {
            *errorMessage = message;
        }
        WriteError("EnsureConnected", message);
        return false;
    }

    // Connect 只返回结果结构，不抛异常；失败原因通过 message 取回。
    VoidResult connectResult = database->Connect();
    if (connectResult.IsError()) {
        const QString message = ResultMessage(connectResult.message, "Database connect failed");
        if (errorMessage) {
            *errorMessage = message;
        }
        WriteError("EnsureConnected", message);
        return false;
    }

    WriteInfo("EnsureConnected", "Database connected through Qt adapter");
    return database->IsConnected();
}

// 初始化、按指定类型切换并连接数据库。
bool DatabaseQtAdapter::EnsureConnected(const QString& configPath,
                                        const QString& databaseType,
                                        QString* errorMessage) {
    // 这个重载用于配置中心已经给出数据库类型时，显式覆盖 db_config.json 的类型。
    IDatabase* database = RawDatabase(errorMessage);
    if (!database) {
        WriteError("EnsureConnected", errorMessage ? *errorMessage : "Database instance unavailable");
        return false;
    }

    // 已连接时不再切换类型。进程运行期切换数据库属于高风险动作，后续应由 MainExe 统一编排。
    if (database->IsConnected()) {
        return true;
    }

    // 第一步读取 db_config.json。路径从调用方传入，不使用 currentPath。
    const QByteArray configBytes = configPath.toUtf8();
    VoidResult initResult = database->Init(configBytes.constData());
    if (initResult.IsError()) {
        const QString message = ResultMessage(initResult.message, "Database init failed");
        if (errorMessage) {
            *errorMessage = message;
        }
        WriteError("EnsureConnected", message);
        return false;
    }

    // 第二步用配置中心传入的 databaseType 覆盖 db_config.json 的类型。
    const QString normalizedType = databaseType.trimmed().toLower();
    if (!normalizedType.isEmpty()) {
        // 当前只有 mysql/sqlite 两类；未知值保守落到 SQLite，后续可改成严格报错策略。
        const DatabaseType targetType = normalizedType == "mysql"
            ? DatabaseType::MySQL
            : DatabaseType::SQLite;
        VoidResult typeResult = database->SetDatabaseType(targetType);
        if (typeResult.IsError()) {
            const QString message = ResultMessage(typeResult.message, "Database type switch failed");
            if (errorMessage) {
                *errorMessage = message;
            }
            WriteError("EnsureConnected", message);
            return false;
        }
    }

    // SetDatabaseType 当前会尝试连接；这里再调用 Connect 作为幂等确认。
    VoidResult connectResult = database->Connect();
    if (connectResult.IsError()) {
        const QString message = ResultMessage(connectResult.message, "Database connect failed");
        if (errorMessage) {
            *errorMessage = message;
        }
        WriteError("EnsureConnected", message);
        return false;
    }

    WriteInfo("EnsureConnected", normalizedType.isEmpty()
        ? "Database connected through Qt adapter"
        : QString("Database connected through Qt adapter, requested type=%1").arg(normalizedType));
    return database->IsConnected();
}

// 按字符串切换数据库类型。
bool DatabaseQtAdapter::SetDatabaseTypeName(const QString& databaseType, QString* errorMessage) {
    // 配置中心通常返回字符串，所以这里做大小写和空白归一化。
    const QString normalizedType = databaseType.trimmed().toLower();
    if (normalizedType == "sqlite") {
        // 字符串接口只存在于 Adapter；底层 Database 仍使用强类型枚举。
        const bool ok = SetRawDatabaseType(DatabaseType::SQLite, errorMessage);
        if (ok) {
            WriteInfo("SetDatabaseTypeName", "Database type switched to sqlite");
        } else {
            WriteError("SetDatabaseTypeName",
                       errorMessage ? *errorMessage : "Database type switch failed");
        }
        return ok;
    }
    if (normalizedType == "mysql") {
        // 这里允许切到 MySQL，但当前底层 MySQL 原生驱动尚未接入，底层会返回明确失败。
        const bool ok = SetRawDatabaseType(DatabaseType::MySQL, errorMessage);
        if (ok) {
            WriteInfo("SetDatabaseTypeName", "Database type switched to mysql");
        } else {
            WriteError("SetDatabaseTypeName",
                       errorMessage ? *errorMessage : "Database type switch failed");
        }
        return ok;
    }

    const QString message = QString("Unsupported database type: %1").arg(databaseType);
    if (errorMessage) {
        *errorMessage = message;
    }
    WriteWarning("SetDatabaseTypeName", message);
    return false;
}

// 判断底层数据库是否已连接。
bool DatabaseQtAdapter::IsConnected() const {
    // 查询状态只读底层连接标记，不修改任何模块状态。
    IDatabase* database = RawDatabase();
    return database && database->IsConnected();
}

// 断开底层数据库连接。
void DatabaseQtAdapter::Disconnect() {
    // Disconnect 是幂等操作；底层未连接时调用也应安全。
    IDatabase* database = RawDatabase();
    if (database) {
        database->Disconnect();
        WriteInfo("Disconnect", "Database disconnected through Qt adapter");
    }
}

// 关闭底层数据库模块。
void DatabaseQtAdapter::Shutdown() {
    // Shutdown 主要用于进程退出或测试结束，释放底层连接和配置状态。
    IDatabase* database = RawDatabase();
    if (database) {
        database->Shutdown();
        WriteInfo("Shutdown", "Database module shut down through Qt adapter");
    }
}

// 返回底层 Database 模块版本。
QString DatabaseQtAdapter::DatabaseModuleVersion() const {
    // 版本字符串由底层 Database 维护，Adapter 不额外复制版本号，避免两个来源不一致。
    IDatabase* database = RawDatabase();
    return database ? QString::fromUtf8(database->GetModuleVersion()) : QString();
}

// 返回 Adapter 自己的代码版本。
const char* DatabaseQtAdapter::GetModuleVersion() const {
    // 版本字符串集中在 ModuleInfo，避免日志模块名、版本清单和代码返回值各写一份。
    return ModuleInfo::Version;
}

// 执行无结果集 SQL。
bool DatabaseQtAdapter::ExecuteUpdate(const QString& sql, QString* errorMessage) {
    // 更新类 SQL 不返回行数据，只关心是否执行成功和影响行数。
    IDatabase* database = RawDatabase(errorMessage);
    if (!database) {
        WriteError("ExecuteUpdate", errorMessage ? *errorMessage : "Database instance unavailable");
        return false;
    }

    // SQL 文本统一转为 UTF-8，再交给纯 C++ Database。
    const QByteArray sqlBytes = sql.toUtf8();
    Result<DbResult> result = database->ExecuteUpdate(sqlBytes.constData());
    if (result.IsError()) {
        // 失败时不把完整 SQL 写入日志，只记录长度，避免日志泄露患者数据或 SQL 过长。
        const QString message = ResultMessage(result.message, "SQL update failed");
        if (errorMessage) {
            *errorMessage = message;
        }
        WriteError("ExecuteUpdate", QString("%1; sqlLength=%2").arg(message).arg(sqlBytes.size()));
        return false;
    }

    return true;
}

// 批量执行 QByteArray SQL 脚本。
int DatabaseQtAdapter::ExecuteScript(const QList<QByteArray>& scripts) {
    // 批量脚本通常用于建表或初始化数据，空脚本直接返回 0。
    IDatabase* database = RawDatabase();
    if (!database || scripts.isEmpty()) {
        WriteWarning("ExecuteScript", !database
            ? "Database instance unavailable"
            : "Script list is empty");
        return 0;
    }

    // Database::ExecuteScript 需要 const char* 数组。
    // QByteArray 列表在本函数内保持生命周期，指针数组只在 ExecuteScript 调用期间有效。
    std::vector<const char*> scriptPointers;
    scriptPointers.reserve(static_cast<size_t>(scripts.size()));
    for (const QByteArray& script : scripts) {
        // constData 指针指向 QByteArray 内部内存；scripts 参数在整个函数期间有效。
        scriptPointers.push_back(script.constData());
    }

    return database->ExecuteScript(scriptPointers.data(),
                                   static_cast<int32_t>(scriptPointers.size()));
}

// 批量执行 QString SQL 脚本。
int DatabaseQtAdapter::ExecuteScript(const QStringList& scripts) {
    // 先保存到 QList<QByteArray>，避免临时 QByteArray 析构后 constData 指针悬空。
    QList<QByteArray> utf8Scripts;
    utf8Scripts.reserve(scripts.size());
    for (const QString& script : scripts) {
        // 每条 QString 都转换成 UTF-8 QByteArray，保证后续 constData() 指针有效。
        utf8Scripts.append(script.toUtf8());
    }
    return ExecuteScript(utf8Scripts);
}

// 查询通用 JSON。
bool DatabaseQtAdapter::ExecuteQueryJson(const QString& sql,
                                         QByteArray* outputJson,
                                         QString* errorMessage,
                                         int initialBufferSize,
                                         int maxBufferSize) {
    // outputJson 由调用方传入，Adapter 只负责清空并填充，不拥有该对象生命周期。
    if (!outputJson) {
        if (errorMessage) {
            *errorMessage = "Output JSON buffer is invalid";
        }
        WriteError("ExecuteQueryJson", "Output JSON buffer is invalid");
        return false;
    }

    IDatabase* database = RawDatabase(errorMessage);
    if (!database) {
        WriteError("ExecuteQueryJson", errorMessage ? *errorMessage : "Database instance unavailable");
        return false;
    }

    const QByteArray sqlBytes = sql.toUtf8();
    // 初始缓冲区至少 1KB，避免调用方传 0 时进入无意义循环。
    int bufferSize = qMax(1024, initialBufferSize);
    // maxBufferSize 小于 initialBufferSize 时按 initialBufferSize 作为上限，保证至少尝试一次。
    const int bufferLimit = qMax(bufferSize, maxBufferSize);
    while (bufferSize <= bufferLimit) {
        // 调用方缓冲区模式可以避免 Database.dll 分配内存、Qt 模块释放内存造成跨 CRT 问题。
        QByteArray buffer(bufferSize, '\0');
        Result<DbJsonResult> result = database->ExecuteQueryJson(sqlBytes.constData(),
                                                                 buffer.data(),
                                                                 buffer.size());
        if (result.IsSuccess()) {
            // Database 写入的是以 '\0' 结尾的 UTF-8 JSON；append(const char*) 只取真实字符串长度。
            outputJson->clear();
            outputJson->append(buffer.constData());
            return true;
        }

        const QString message = ResultMessage(result.message, "SQL query failed");
        if (!message.contains("too small", Qt::CaseInsensitive)) {
            // 只有缓冲区太小才允许重试；SQL 语法错误、未连接等错误直接返回。
            if (errorMessage) {
                *errorMessage = message;
            }
            WriteError("ExecuteQueryJson",
                       QString("%1; sqlLength=%2").arg(message).arg(sqlBytes.size()));
            return false;
        }

        if (bufferSize == bufferLimit) {
            // 到达上限仍写不下时返回明确错误，避免无限扩容耗尽内存。
            const QString message = QString("JSON output is larger than %1 bytes").arg(bufferLimit);
            if (errorMessage) {
                *errorMessage = message;
            }
            WriteError("ExecuteQueryJson", message);
            return false;
        }

        // 受控倍增，避免一次查询直接申请过大内存。
        bufferSize = qMin(bufferSize * 2, bufferLimit);
    }

    if (errorMessage) {
        *errorMessage = "SQL query failed";
    }
    WriteError("ExecuteQueryJson", "SQL query failed");
    return false;
}

// 查询并解析为 QJsonDocument。
bool DatabaseQtAdapter::ExecuteQueryJsonDocument(const QString& sql,
                                                 QJsonDocument* document,
                                                 QString* errorMessage) {
    // document 由调用方提供，Adapter 只负责写入解析后的结果。
    if (!document) {
        if (errorMessage) {
            *errorMessage = "Output JSON document is invalid";
        }
        WriteError("ExecuteQueryJsonDocument", "Output JSON document is invalid");
        return false;
    }

    QByteArray json;
    // 先复用字节级查询接口，保证缓冲区扩容和错误处理只有一份实现。
    if (!ExecuteQueryJson(sql, &json, errorMessage)) {
        return false;
    }

    // QJsonDocument::fromJson 会校验 JSON 格式，parseError 用于给调用方可读错误。
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        WriteError("ExecuteQueryJsonDocument",
                   QString("JSON parse failed: %1").arg(parseError.errorString()));
        return false;
    }

    *document = parsed;
    return true;
}

// 从通用表格 JSON 字节中提取 rows。
QJsonArray DatabaseQtAdapter::RowsFromTableJson(const QByteArray& tableJson) {
    // 字节版先解析成文档，再复用文档版，避免两处 rows 提取逻辑不一致。
    return RowsFromTableJson(QJsonDocument::fromJson(tableJson));
}

// 从通用表格 JSON 文档中提取 rows。
QJsonArray DatabaseQtAdapter::RowsFromTableJson(const QJsonDocument& tableDocument) {
    // 通用表格 JSON 格式固定为 { columns:[], rows:[] }，UI 通常只关心 rows。
    return tableDocument.object().value("rows").toArray();
}

// 转义 SQL 文本中的单引号。
QString DatabaseQtAdapter::EscapeSqlText(const QString& text) {
    // SQL 字符串中单引号用两个单引号表示一个字面量单引号。
    QString escaped = text;
    escaped.replace("'", "''");
    return escaped;
}

// 转换 Database 结果消息。
QString DatabaseQtAdapter::ResultMessage(const char* message, const QString& fallback) {
    return DatabaseResultMessage(message, fallback);
}

// 获取并缓存 Logger 单例。
ILogger* DatabaseQtAdapter::Logger() const {
    if (!m_logger) {
        // GetLogger() 来自 Logger.dll；Adapter 只保存指针，不拥有 Logger 生命周期。
        // mutable m_logger 允许 const 日志函数中懒加载 Logger，不影响对外语义。
        m_logger = GetLogger();
    }
    return m_logger;
}

// 写 Info 级别边界日志。
void DatabaseQtAdapter::WriteInfo(const QString& operation, const QString& content) const {
    ILogger* logger = Logger();
    if (!logger) {
        return;
    }
    logger->Write(LogLevel::Info, ModuleInfo::Name, operation, "", "", "", content);
}

// 写 Warning 级别边界日志。
void DatabaseQtAdapter::WriteWarning(const QString& operation, const QString& content) const {
    ILogger* logger = Logger();
    if (!logger) {
        return;
    }
    logger->Write(LogLevel::Warning, ModuleInfo::Name, operation, "", "", "", content);
}

// 写 Error 级别边界日志。
void DatabaseQtAdapter::WriteError(const QString& operation, const QString& content) const {
    ILogger* logger = Logger();
    if (!logger) {
        return;
    }
    logger->Write(LogLevel::Error, ModuleInfo::Name, operation, "", "", "", content);
}
