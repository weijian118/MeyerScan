// =============================================================================
// 文件:    DatabaseImpl.cpp
// 模块:    MeyerScan_Database.dll
// 用途:
//   数据库基础设施模块的纯 C++ 实现。
//
// 实现要点:
//   1. 不使用 QtCore、QtSql、QJson、QString、QMutex 等 Qt 类型。
//   2. SQLite 通过运行时动态加载 sqlite3.dll 调用 C API。
//   3. Database 只执行基础 SQL 和结果行列转换，不理解患者/订单业务含义。
//   4. Qt 模块如需 QString/QJson 便利能力，应通过 MyDatabaseQtAdapter 转换。
// =============================================================================

#include "DatabaseImpl.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// SQLite 返回码。这里不包含 sqlite3.h，因此只定义当前实现需要的常量。
const int SQLITE_OK = 0;
const int SQLITE_ROW = 100;
const int SQLITE_DONE = 101;
const int SQLITE_INTEGER = 1;
const int SQLITE_FLOAT = 2;
const int SQLITE_NULL = 5;
const int SQLITE_OPEN_READWRITE = 0x00000002;
const int SQLITE_OPEN_CREATE = 0x00000004;
const int SQLITE_OPEN_FULLMUTEX = 0x00010000;

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与工程里的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_Database";

// 模块版本用于 GetModuleVersion() 和版本清单，需与 Version.rc 同步维护。
const char* Version = "MeyerScan_Database v1.3.0 (2026-07-02)";
}

// 把 std::string 复制到固定 char 数组中。
// Database.h 的配置结构体是 POD，不能把 std::string 跨 DLL 暴露给调用方。
void CopyText(char* target, size_t targetSize, const std::string& value) {
    // 固定数组为空时不能写入，直接返回。
    if (!target || targetSize == 0) {
        return;
    }

    // 先清零整块数组，确保旧内容不会残留到本次配置快照里。
    std::memset(target, 0, targetSize);

    // strncpy 最多写 targetSize - 1 个字符，最后一个字节留给 '\0'。
    std::strncpy(target, value.c_str(), targetSize - 1);
}

// 去掉字符串两端空白。
std::string Trim(const std::string& value) {
    // 找第一个非空白字符。
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    // 找最后一个非空白字符的后一位。
    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    // substr 使用 [begin, end) 区间，刚好得到去空白后的内容。
    return value.substr(begin, end - begin);
}

// 转成小写，主要用于 databaseType 比较。
std::string ToLower(std::string value) {
    // std::transform 会逐字符写回原字符串。
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace

ILogger* DatabaseImpl::s_logger = nullptr;
void* DatabaseImpl::s_loggerModule = nullptr;

// 返回 Database 单例。
extern "C" MEYERSCAN_DATABASE_API IDatabase* GetDatabase() {
    return &DatabaseImpl::Instance();
}

// 获取进程内唯一 DatabaseImpl 实例。
DatabaseImpl& DatabaseImpl::Instance() {
    // C++11 保证局部静态变量初始化过程线程安全。
    static DatabaseImpl instance;
    return instance;
}

// 构造函数初始化所有指针和 POD 成员。
DatabaseImpl::DatabaseImpl()
    : m_sqliteModule(nullptr),
      m_sqliteDb(nullptr),
      m_sqliteOpenV2(nullptr),
      m_sqliteClose(nullptr),
      m_sqliteExec(nullptr),
      m_sqlitePrepareV2(nullptr),
      m_sqliteStep(nullptr),
      m_sqliteFinalize(nullptr),
      m_sqliteErrmsg(nullptr),
      m_sqliteColumnCount(nullptr),
      m_sqliteColumnName(nullptr),
      m_sqliteColumnText(nullptr),
      m_sqliteColumnType(nullptr),
      m_sqliteChanges(nullptr),
      m_sqliteFree(nullptr),
      m_connected(false) {
    // POD 配置结构必须清零，避免调用方读到随机内存。
    std::memset(&m_config, 0, sizeof(m_config));

    // 备份时间为空字符串表示尚未完成备份。
    std::memset(m_lastBackupTime, 0, sizeof(m_lastBackupTime));

    // 最近错误为空表示当前没有可额外返回给调用方的底层错误。
    std::memset(m_lastErrorMessage, 0, sizeof(m_lastErrorMessage));
}

// 析构函数释放数据库连接和动态库句柄。
DatabaseImpl::~DatabaseImpl() {
    // Shutdown 内部会关闭 SQLite 连接，可重复调用。
    Shutdown();

    // SQLite DLL 是本模块 LoadLibrary 得到的，析构时释放。
    if (m_sqliteModule) {
        FreeLibrary(static_cast<HMODULE>(m_sqliteModule));
        m_sqliteModule = nullptr;
    }

    // Logger DLL 也是本模块动态加载得到的，释放模块句柄即可。
    if (s_loggerModule && s_loggerModule != reinterpret_cast<void*>(-1)) {
        FreeLibrary(static_cast<HMODULE>(s_loggerModule));
        s_loggerModule = nullptr;
        s_logger = nullptr;
    }
}

// 动态获取 Logger 单例。
ILogger* DatabaseImpl::GetLogger() {
    // 已经拿到 Logger 时直接返回，避免每条日志重复 LoadLibrary。
    if (s_logger) {
        return s_logger;
    }

    // -1 表示之前尝试加载失败，后续静默跳过日志即可。
    if (s_loggerModule == reinterpret_cast<void*>(-1)) {
        return nullptr;
    }

    // LoadLibraryA 会按 EXE 所在目录、系统目录、PATH 等规则查找 DLL。
    HMODULE module = LoadLibraryA("MeyerScan_Logger.dll");
    if (!module) {
        s_loggerModule = reinterpret_cast<void*>(-1);
        return nullptr;
    }

    // GetProcAddress 取 C ABI 导出的 GetLogger 函数。
    auto getLogger = reinterpret_cast<GetLoggerFunc>(GetProcAddress(module, "GetLogger"));
    if (!getLogger) {
        FreeLibrary(module);
        s_loggerModule = reinterpret_cast<void*>(-1);
        return nullptr;
    }

    // 缓存 Logger 指针和 DLL 句柄，后续日志直接使用。
    s_logger = getLogger();
    s_loggerModule = module;
    return s_logger;
}

// 初始化数据库模块，读取配置文件。
VoidResult DatabaseImpl::Init(const char* configPath) {
    // 公开方法统一加锁，避免 Init 和 Connect/Execute 并发修改状态。
    std::lock_guard<std::mutex> locker(m_mutex);

    // 配置路径必须由 MainExe/测试宿主按程序目录传入，不能依赖 currentPath。
    if (!configPath || configPath[0] == '\0') {
        LogError("Init", "Config path is empty");
        return VoidResult::Fail(ErrorCode::InvalidParameter, "Config path is empty");
    }

    // LoadConfig 只解析文本和路径，不建立数据库连接。
    if (!LoadConfig(configPath)) {
        return VoidResult::Fail(ErrorCode::DbQueryFailed, "Failed to load config file");
    }

    LogInfo("Init", "Database module initialized");
    return VoidResult::Ok();
}

// 读取 db_config.json 并填充 DbConfig。
bool DatabaseImpl::LoadConfig(const char* configPath) {
    // NormalizePath 把反斜杠统一成斜杠，后续 DirectoryOf/拼接更简单。
    const std::string normalizedPath = NormalizePath(configPath ? configPath : "");

    // ifstream 直接按字节读取 UTF-8 JSON；字段值也按 UTF-8 原样保存。
    std::ifstream file(normalizedPath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        LogError("LoadConfig", "Failed to open config file");
        return false;
    }

    // stringstream 可以一次性读入小型配置文件。配置文件不应该存放大数据。
    std::ostringstream stream;
    stream << file.rdbuf();
    const std::string json = stream.str();

    // 配置目录用于解析 sqlitePath/mysql.dataDir 这类相对路径。
    m_configDir = DirectoryOf(normalizedPath);

    // 先清空旧配置，避免新配置缺字段时保留旧值。
    std::memset(&m_config, 0, sizeof(m_config));
    m_config.version = 1;

    // databaseType 缺省使用 sqlite，符合当前重构默认离线本地数据库链路。
    const std::string databaseType = ToLower(ExtractJsonString(json, "databaseType", "sqlite"));
    m_config.dbType = databaseType == "mysql"
        ? static_cast<int32_t>(DatabaseType::MySQL)
        : static_cast<int32_t>(DatabaseType::SQLite);

    // MySQL 配置在 mysql 对象内。当前只解析保存，不建立 MySQL 原生连接。
    const std::string mysqlObject = ExtractObjectText(json, "mysql");
    CopyText(m_config.mysqlHost,
             sizeof(m_config.mysqlHost),
             ExtractJsonString(mysqlObject, "host", "127.0.0.1"));
    m_config.mysqlPort = ExtractJsonInt(mysqlObject, "port", 3308);
    CopyText(m_config.mysqlService,
             sizeof(m_config.mysqlService),
             ExtractJsonString(mysqlObject, "service", "MSCANDB"));
    CopyText(m_config.mysqlDatabase,
             sizeof(m_config.mysqlDatabase),
             ExtractJsonString(mysqlObject, "database", "mscan"));

    // dataDir 和 sqlitePath 支持相对配置文件目录，方便安装目录整体迁移。
    CopyText(m_config.mysqlDataDir,
             sizeof(m_config.mysqlDataDir),
             ResolvePathFromConfig(ExtractJsonString(mysqlObject, "dataDir", "../MySQL/data/mscan")));
    CopyText(m_config.sqlitePath,
             sizeof(m_config.sqlitePath),
             ResolvePathFromConfig(ExtractJsonString(json, "sqlitePath", "../Data/MeyerScanSQLite.db")));

    LogInfo("LoadConfig", "Config loaded");
    return true;
}

// 根据配置建立数据库连接。
VoidResult DatabaseImpl::Connect() {
    std::lock_guard<std::mutex> locker(m_mutex);

    // 已连接时直接成功，避免重复打开同一个文件。
    if (m_connected) {
        return VoidResult::Ok();
    }

    // 每次连接前清空上一轮底层错误，防止调用方读到陈旧原因。
    SetLastErrorMessage("");

    // 当前本轮完整落地 SQLite 原生驱动；MySQL 原生驱动保留接口和明确错误。
    const bool success = m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)
        ? ConnectMySQL()
        : ConnectSQLite();

    if (!success) {
        const char* message = m_lastErrorMessage[0] != '\0'
            ? m_lastErrorMessage
            : "Database connection failed";
        return VoidResult::Fail(ErrorCode::DbConnectionFailed, message);
    }

    LogInfo("Connect", "Database connected");
    return VoidResult::Ok();
}

// 连接 MySQL。
bool DatabaseImpl::ConnectMySQL() {
    // 当前环境没有 mysql.h/libmysql.lib，不能在本轮安全编译 MySQL C API。
    // 明确返回失败，而不是隐式回退 SQLite，避免调用方以为真的连上了 MySQL。
    SetLastErrorMessage("Native MySQL driver is not enabled; provide MySQL C API SDK first");
    LogError("ConnectMySQL", m_lastErrorMessage);
    return false;
}

// 加载 sqlite3.dll 并绑定所需 C API。
bool DatabaseImpl::LoadSqliteRuntime() {
    // 已加载时直接复用函数指针表。
    if (m_sqliteModule) {
        return true;
    }

    // 优先从 EXE 目录或 PATH 查找 sqlite3.dll。
    HMODULE module = LoadLibraryA("sqlite3.dll");
    if (!module) {
        SetLastWin32ErrorMessage("Failed to load sqlite3.dll", GetLastError());
        LogError("LoadSqliteRuntime", m_lastErrorMessage);
        return false;
    }

    // 这个宏只在当前函数内使用，用于减少重复 GetProcAddress 代码。
#define LOAD_SQLITE_PROC(member, name) \
    do { \
        member = reinterpret_cast<decltype(member)>(GetProcAddress(module, name)); \
        if (!member) { \
            FreeLibrary(module); \
            SetLastErrorMessage("sqlite3.dll is missing required function: " name); \
            LogError("LoadSqliteRuntime", m_lastErrorMessage); \
            return false; \
        } \
    } while (0)

    // 逐个获取 SQLite C API 函数地址。
    LOAD_SQLITE_PROC(m_sqliteOpenV2, "sqlite3_open_v2");
    LOAD_SQLITE_PROC(m_sqliteClose, "sqlite3_close");
    LOAD_SQLITE_PROC(m_sqliteExec, "sqlite3_exec");
    LOAD_SQLITE_PROC(m_sqlitePrepareV2, "sqlite3_prepare_v2");
    LOAD_SQLITE_PROC(m_sqliteStep, "sqlite3_step");
    LOAD_SQLITE_PROC(m_sqliteFinalize, "sqlite3_finalize");
    LOAD_SQLITE_PROC(m_sqliteErrmsg, "sqlite3_errmsg");
    LOAD_SQLITE_PROC(m_sqliteColumnCount, "sqlite3_column_count");
    LOAD_SQLITE_PROC(m_sqliteColumnName, "sqlite3_column_name");
    LOAD_SQLITE_PROC(m_sqliteColumnText, "sqlite3_column_text");
    LOAD_SQLITE_PROC(m_sqliteColumnType, "sqlite3_column_type");
    LOAD_SQLITE_PROC(m_sqliteChanges, "sqlite3_changes");
    LOAD_SQLITE_PROC(m_sqliteFree, "sqlite3_free");

#undef LOAD_SQLITE_PROC

    // 函数表全部有效后再保存模块句柄。
    m_sqliteModule = module;
    return true;
}

// 连接 SQLite 数据库。
bool DatabaseImpl::ConnectSQLite() {
    // SQLite 运行时 DLL 必须先加载成功。
    if (!LoadSqliteRuntime()) {
        return false;
    }

    // 首次运行时 Data 目录可能不存在，先创建父目录。
    if (!EnsureParentDirectoryExists(m_config.sqlitePath)) {
        SetLastErrorMessage("Failed to create SQLite parent directory");
        LogError("ConnectSQLite", m_lastErrorMessage);
        return false;
    }

    // sqlite3_open_v2 传入 UTF-8 路径；Windows 版 SQLite 支持 UTF-8 文件名。
    sqlite3* db = nullptr;
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = m_sqliteOpenV2(m_config.sqlitePath, &db, flags, nullptr);
    if (rc != SQLITE_OK) {
        // 打开失败时 db 可能非空，sqlite3_errmsg 仍可读取原因。
        const char* error = db && m_sqliteErrmsg ? m_sqliteErrmsg(db) : "sqlite3_open_v2 failed";
        SetLastErrorMessage(error);
        LogError("ConnectSQLite", m_lastErrorMessage);
        if (db && m_sqliteClose) {
            m_sqliteClose(db);
        }
        return false;
    }

    // 保存连接句柄后，后续所有 SQL 都通过同一个连接执行。
    m_sqliteDb = db;
    m_connected = true;
    LogInfo("ConnectSQLite", "SQLite connected");
    return true;
}

// 保存最近一次底层错误。
void DatabaseImpl::SetLastErrorMessage(const char* message) {
    std::memset(m_lastErrorMessage, 0, sizeof(m_lastErrorMessage));
    if (message && message[0] != '\0') {
        std::strncpy(m_lastErrorMessage, message, sizeof(m_lastErrorMessage) - 1);
    }
}

// 保存带 Windows GetLastError 码的错误，便于定位 DLL 缺失或位数不匹配。
void DatabaseImpl::SetLastWin32ErrorMessage(const char* prefix, unsigned long errorCode) {
    std::snprintf(m_lastErrorMessage,
                  sizeof(m_lastErrorMessage),
                  "%s, Win32Error=%lu",
                  prefix ? prefix : "Win32 error",
                  errorCode);
}

// 断开当前连接。
VoidResult DatabaseImpl::Disconnect() {
    std::lock_guard<std::mutex> locker(m_mutex);

    // 当前只有 SQLite 原生连接；MySQL 接入后在这里增加对应关闭逻辑。
    CloseSqlite();

    m_connected = false;
    LogInfo("Disconnect", "Database disconnected");
    return VoidResult::Ok();
}

// 关闭 SQLite 连接。
void DatabaseImpl::CloseSqlite() {
    // sqlite3_close 必须在所有 statement finalize 后调用。
    // 本实现每次查询都局部 prepare/finalize，因此这里可以直接关闭连接。
    if (m_sqliteDb && m_sqliteClose) {
        m_sqliteClose(m_sqliteDb);
        m_sqliteDb = nullptr;
    }
}

// 判断是否已连接。
bool DatabaseImpl::IsConnected() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_connected;
}

// 返回当前配置中的数据库类型。
DatabaseType DatabaseImpl::GetDatabaseType() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return static_cast<DatabaseType>(m_config.dbType);
}

// 备份数据库。
VoidResult DatabaseImpl::Backup(const char* backupPath) {
    std::lock_guard<std::mutex> locker(m_mutex);

    if (!backupPath || backupPath[0] == '\0') {
        LogError("Backup", "Backup path is empty");
        return VoidResult::Fail(ErrorCode::InvalidParameter, "Backup path is empty");
    }

    if (!m_connected) {
        LogError("Backup", "Database is not connected");
        return VoidResult::Fail(ErrorCode::NotInitialized, "Database is not connected");
    }

    const bool success = m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)
        ? BackupMySQL(backupPath)
        : BackupSQLite(backupPath);
    if (!success) {
        return VoidResult::Fail(ErrorCode::DbQueryFailed, "Database backup failed");
    }

    return VoidResult::Ok();
}

// 获取最近一次备份时间。
const char* DatabaseImpl::GetLastBackupTime() const {
    return m_lastBackupTime;
}

// 执行查询 SQL，但不返回结果集。
Result<DbResult> DatabaseImpl::ExecuteQuery(const char* sql) {
    std::lock_guard<std::mutex> locker(m_mutex);
    return ExecuteSqlLocked(sql, true);
}

// 执行更新/DDL SQL。
Result<DbResult> DatabaseImpl::ExecuteUpdate(const char* sql) {
    std::lock_guard<std::mutex> locker(m_mutex);
    return ExecuteSqlLocked(sql, false);
}

// 执行 SQL 并返回影响行数。
Result<DbResult> DatabaseImpl::ExecuteSqlLocked(const char* sql, bool /*expectRows*/) {
    DbResult result;
    result.version = 1;
    result.affectedRows = 0;
    std::memset(result.reserved, 0, sizeof(result.reserved));

    if (!sql || sql[0] == '\0') {
        LogError("ExecuteSql", "SQL is empty");
        return Result<DbResult>::Fail(ErrorCode::InvalidParameter, "SQL is empty");
    }

    if (!m_connected || !m_sqliteDb) {
        LogError("ExecuteSql", "Database is not connected");
        return Result<DbResult>::Fail(ErrorCode::NotInitialized, "Database is not connected");
    }

    // sqlite3_exec 可执行 SELECT/DDL/DML。这里不读取 SELECT 结果，只判断 SQL 是否成功。
    char* errorMessage = nullptr;
    const int rc = m_sqliteExec(m_sqliteDb, sql, nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        const char* message = errorMessage ? errorMessage : SqliteLastError();
        LogError("ExecuteSql", message);
        if (errorMessage && m_sqliteFree) {
            m_sqliteFree(errorMessage);
        }
        return Result<DbResult>::Fail(ErrorCode::DbQueryFailed, "SQL execution failed");
    }

    // sqlite3_changes 返回最近一次 INSERT/UPDATE/DELETE 影响行数。
    result.affectedRows = m_sqliteChanges ? m_sqliteChanges(m_sqliteDb) : 0;
    return Result<DbResult>::Ok(result);
}

// 批量执行 SQL 脚本。
int32_t DatabaseImpl::ExecuteScript(const char** sqlScripts, int32_t count) {
    if (!sqlScripts || count <= 0) {
        LogError("ExecuteScript", "Invalid script arguments");
        return 0;
    }

    int32_t successCount = 0;
    for (int32_t i = 0; i < count; ++i) {
        if (!sqlScripts[i]) {
            continue;
        }
        Result<DbResult> result = ExecuteUpdate(sqlScripts[i]);
        if (result.IsSuccess()) {
            ++successCount;
        }
    }

    char message[128] = {0};
    std::snprintf(message, sizeof(message), "Executed %d/%d SQL statements", successCount, count);
    LogInfo("ExecuteScript", message);
    return successCount;
}

// 查询 SQL 并返回通用表格 JSON。
Result<DbJsonResult> DatabaseImpl::ExecuteQueryJson(const char* sql,
                                                    char* jsonBuffer,
                                                    int32_t jsonBufferSize) {
    std::lock_guard<std::mutex> locker(m_mutex);
    return ExecuteQueryJsonLocked(sql, jsonBuffer, jsonBufferSize);
}

// 查询 SQL 并序列化为 JSON。调用方必须已持有互斥锁。
Result<DbJsonResult> DatabaseImpl::ExecuteQueryJsonLocked(const char* sql,
                                                          char* jsonBuffer,
                                                          int32_t jsonBufferSize) {
    DbJsonResult result;
    result.version = 1;
    result.rowCount = 0;
    result.bytesWritten = 0;
    std::memset(result.reserved, 0, sizeof(result.reserved));

    if (!sql || sql[0] == '\0') {
        LogError("ExecuteQueryJson", "SQL is empty");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter, "SQL is empty");
    }
    if (!jsonBuffer || jsonBufferSize <= 0) {
        LogError("ExecuteQueryJson", "JSON output buffer is invalid");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter, "JSON output buffer is invalid");
    }
    if (!m_connected || !m_sqliteDb) {
        LogError("ExecuteQueryJson", "Database is not connected");
        return Result<DbJsonResult>::Fail(ErrorCode::NotInitialized, "Database is not connected");
    }

    sqlite3_stmt* statement = nullptr;
    const int prepareRc = m_sqlitePrepareV2(m_sqliteDb, sql, -1, &statement, nullptr);
    if (prepareRc != SQLITE_OK || !statement) {
        LogError("ExecuteQueryJson", SqliteLastError());
        return Result<DbJsonResult>::Fail(ErrorCode::DbQueryFailed, "SQL query failed");
    }

    // 字段名先放到 columns，便于上层通用表格和调试工具展示。
    const int columnCount = m_sqliteColumnCount(statement);
    std::vector<std::string> columns;
    columns.reserve(static_cast<size_t>(columnCount));
    for (int i = 0; i < columnCount; ++i) {
        const char* name = m_sqliteColumnName(statement, i);
        columns.push_back(name ? name : "");
    }

    // 用 ostringstream 逐步拼 JSON，避免引入 Qt JSON 或第三方 JSON。
    std::ostringstream json;
    json << "{\"schemaVersion\":1,\"rowCount\":";

    // rowCount 需要遍历完才知道，这里先把 rows 单独拼出来。
    std::ostringstream rows;
    rows << "[";

    int rowCount = 0;
    bool firstRow = true;
    while (true) {
        const int stepRc = m_sqliteStep(statement);
        if (stepRc == SQLITE_ROW) {
            if (!firstRow) {
                rows << ",";
            }
            firstRow = false;
            rows << "{";
            for (int i = 0; i < columnCount; ++i) {
                if (i > 0) {
                    rows << ",";
                }
                const char* columnName = columns[static_cast<size_t>(i)].c_str();
                const int sqliteValueType = m_sqliteColumnType(statement, i);
                const unsigned char* valueBytes = m_sqliteColumnText(statement, i);
                const char* valueText = reinterpret_cast<const char*>(valueBytes);
                rows << "\"" << EscapeJson(columnName) << "\":";
                if (sqliteValueType == SQLITE_NULL || !valueText) {
                    rows << "null";
                } else if (sqliteValueType == SQLITE_INTEGER || sqliteValueType == SQLITE_FLOAT) {
                    rows << valueText;
                } else {
                    rows << "\"" << EscapeJson(valueText) << "\"";
                }
            }
            rows << "}";
            ++rowCount;
            continue;
        }

        if (stepRc == SQLITE_DONE) {
            break;
        }

        LogError("ExecuteQueryJson", SqliteLastError());
        m_sqliteFinalize(statement);
        return Result<DbJsonResult>::Fail(ErrorCode::DbQueryFailed, "SQL query failed");
    }

    rows << "]";
    m_sqliteFinalize(statement);

    json << rowCount << ",\"columns\":[";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        json << "\"" << EscapeJson(columns[i].c_str()) << "\"";
    }
    json << "],\"rows\":" << rows.str() << "}";

    const std::string jsonText = json.str();
    if (jsonText.size() + 1 > static_cast<size_t>(jsonBufferSize)) {
        LogError("ExecuteQueryJson", "JSON output buffer is too small");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter, "JSON output buffer is too small");
    }

    // 先清零再复制，保证调用方总能按 C 字符串读取。
    std::memset(jsonBuffer, 0, static_cast<size_t>(jsonBufferSize));
    std::memcpy(jsonBuffer, jsonText.c_str(), jsonText.size());

    result.rowCount = rowCount;
    result.bytesWritten = static_cast<int32_t>(jsonText.size());
    return Result<DbJsonResult>::Ok(result);
}

// 开始事务。
VoidResult DatabaseImpl::BeginTransaction() {
    Result<DbResult> result = ExecuteUpdate("BEGIN TRANSACTION");
    if (result.IsError()) {
        return VoidResult::Fail(ErrorCode::DbTransactionFailed, "Failed to begin transaction");
    }
    LogInfo("BeginTransaction", "Transaction started");
    return VoidResult::Ok();
}

// 提交事务。
VoidResult DatabaseImpl::Commit() {
    Result<DbResult> result = ExecuteUpdate("COMMIT");
    if (result.IsError()) {
        return VoidResult::Fail(ErrorCode::DbTransactionFailed, "Failed to commit transaction");
    }
    LogInfo("Commit", "Transaction committed");
    return VoidResult::Ok();
}

// 回滚事务。
VoidResult DatabaseImpl::Rollback() {
    Result<DbResult> result = ExecuteUpdate("ROLLBACK");
    if (result.IsError()) {
        return VoidResult::Fail(ErrorCode::DbTransactionFailed, "Failed to roll back transaction");
    }
    LogInfo("Rollback", "Transaction rolled back");
    return VoidResult::Ok();
}

// 返回配置快照。
const DbConfig& DatabaseImpl::GetConfig() const {
    return m_config;
}

// 切换数据库类型。
VoidResult DatabaseImpl::SetDatabaseType(DatabaseType dbType) {
    {
        std::lock_guard<std::mutex> locker(m_mutex);

        // 切换前关闭旧连接，避免同一实例继续使用旧驱动句柄。
        CloseSqlite();
        m_connected = false;
        m_config.dbType = static_cast<int32_t>(dbType);
    }

    // 释放锁后再连接，避免 Connect 内部重复加锁导致死锁。
    VoidResult connectResult = Connect();
    if (connectResult.IsSuccess()) {
        LogInfo("SetDatabaseType", "Database type switched");
    } else {
        LogError("SetDatabaseType", "Failed to switch database type");
    }
    return connectResult;
}

// 返回模块版本。
const char* DatabaseImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭模块。
VoidResult DatabaseImpl::Shutdown() {
    Disconnect();
    LogInfo("Shutdown", "Database module shut down");
    return VoidResult::Ok();
}

// 返回 SQLite 最近错误。
const char* DatabaseImpl::SqliteLastError() const {
    if (m_sqliteDb && m_sqliteErrmsg) {
        return m_sqliteErrmsg(m_sqliteDb);
    }
    return "SQLite error";
}

// 备份 SQLite 文件。
bool DatabaseImpl::BackupSQLite(const char* backupPath) {
    const std::string backupDir = NormalizePath(backupPath ? backupPath : "");
    if (!EnsureDirectoryExists(backupDir)) {
        LogError("BackupSQLite", "Failed to create backup directory");
        return false;
    }

    const std::string timestamp = CurrentTimestamp();
    const std::string targetFile = backupDir + "/" + timestamp + "-MeyerScanSQLite.db";

    // CopyFileA 第三个参数 FALSE 表示目标存在时覆盖，便于同秒重复测试。
    if (!CopyFileA(m_config.sqlitePath, targetFile.c_str(), FALSE)) {
        LogError("BackupSQLite", "Failed to copy SQLite database file");
        return false;
    }

    CopyText(m_lastBackupTime, sizeof(m_lastBackupTime), timestamp);
    LogInfo("BackupSQLite", "SQLite backup completed");
    return true;
}

// 备份 MySQL 数据目录。
bool DatabaseImpl::BackupMySQL(const char* backupPath) {
    // MySQL 原生连接尚未启用，本轮不再执行 robocopy 备份，避免误以为 MySQL 链路已完成。
    (void)backupPath;
    LogError("BackupMySQL", "Native MySQL backup is not enabled");
    return false;
}

// 标准化路径分隔符。
std::string DatabaseImpl::NormalizePath(const std::string& path) {
    std::string normalized = Trim(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

// 获取路径所在目录。
std::string DatabaseImpl::DirectoryOf(const std::string& path) {
    const std::string normalized = NormalizePath(path);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    return normalized.substr(0, slash);
}

// 判断是否为绝对路径。
bool DatabaseImpl::IsAbsolutePath(const std::string& path) {
    if (path.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(path[0])) &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
    if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
        return true;
    }
    return false;
}

// 按配置文件目录解析相对路径。
std::string DatabaseImpl::ResolvePathFromConfig(const std::string& configuredPath) const {
    const std::string normalized = NormalizePath(configuredPath);
    if (normalized.empty()) {
        return std::string();
    }
    if (IsAbsolutePath(normalized)) {
        return normalized;
    }
    if (m_configDir.empty() || m_configDir == ".") {
        return normalized;
    }
    return NormalizePath(m_configDir + "/" + normalized);
}

// 创建目录，支持多级目录。
bool DatabaseImpl::EnsureDirectoryExists(const std::string& dirPath) {
    const std::string normalized = NormalizePath(dirPath);
    if (normalized.empty()) {
        return false;
    }

    DWORD attrs = GetFileAttributesA(normalized.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    // 逐级创建目录，避免 CreateDirectoryA 只能创建最后一级的问题。
    std::string partial;
    for (size_t i = 0; i < normalized.size(); ++i) {
        const char ch = normalized[i];
        partial.push_back(ch);
        if (ch != '/' && i + 1 != normalized.size()) {
            continue;
        }

        // 跳过 "C:" 和空片段。
        if (partial.size() <= 2 || partial == "/" || partial == "//") {
            continue;
        }

        DWORD currentAttrs = GetFileAttributesA(partial.c_str());
        if (currentAttrs == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryA(partial.c_str(), nullptr);
        }
    }

    attrs = GetFileAttributesA(normalized.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// 创建文件父目录。
bool DatabaseImpl::EnsureParentDirectoryExists(const std::string& filePath) {
    return EnsureDirectoryExists(DirectoryOf(filePath));
}

// 从 JSON 文本读取对象片段。
std::string DatabaseImpl::ExtractObjectText(const std::string& json, const std::string& key) {
    const std::string pattern = "\"" + key + "\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return std::string();
    }

    const size_t colon = json.find(':', keyPos + pattern.size());
    if (colon == std::string::npos) {
        return std::string();
    }

    const size_t objectBegin = json.find('{', colon + 1);
    if (objectBegin == std::string::npos) {
        return std::string();
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = objectBegin; i < json.size(); ++i) {
        const char ch = json[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (ch == '\\' && inString) {
            escape = true;
            continue;
        }
        if (ch == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(objectBegin, i - objectBegin + 1);
            }
        }
    }
    return std::string();
}

// 从 JSON 文本读取字符串字段。
std::string DatabaseImpl::ExtractJsonString(const std::string& json,
                                            const std::string& key,
                                            const std::string& defaultValue) {
    const std::string pattern = "\"" + key + "\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return defaultValue;
    }

    const size_t colon = json.find(':', keyPos + pattern.size());
    if (colon == std::string::npos) {
        return defaultValue;
    }

    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) {
        return defaultValue;
    }

    std::string value;
    bool escape = false;
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (escape) {
            // 当前配置只需要常见转义；未知转义按原字符保留。
            if (ch == 'n') {
                value.push_back('\n');
            } else if (ch == 'r') {
                value.push_back('\r');
            } else if (ch == 't') {
                value.push_back('\t');
            } else {
                value.push_back(ch);
            }
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return defaultValue;
}

// 从 JSON 文本读取整数字段。
int DatabaseImpl::ExtractJsonInt(const std::string& json,
                                 const std::string& key,
                                 int defaultValue) {
    const std::string pattern = "\"" + key + "\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return defaultValue;
    }
    const size_t colon = json.find(':', keyPos + pattern.size());
    if (colon == std::string::npos) {
        return defaultValue;
    }

    size_t begin = colon + 1;
    while (begin < json.size() &&
           std::isspace(static_cast<unsigned char>(json[begin]))) {
        ++begin;
    }

    size_t end = begin;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) ||
            json[end] == '-' ||
            json[end] == '+')) {
        ++end;
    }

    if (end == begin) {
        return defaultValue;
    }

    return std::atoi(json.substr(begin, end - begin).c_str());
}

// JSON 字符串转义。
std::string DatabaseImpl::EscapeJson(const char* text) {
    if (!text) {
        return std::string();
    }

    std::string escaped;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        const unsigned char ch = *p;
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buffer[8] = {0};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                escaped += buffer;
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

// 判断文本是否可作为 JSON 数字直接输出。
bool DatabaseImpl::LooksLikeNumber(const char* text) {
    if (!text || !text[0]) {
        return false;
    }

    const char* p = text;
    if (*p == '-' || *p == '+') {
        ++p;
    }
    bool hasDigit = false;
    bool hasDot = false;
    while (*p) {
        if (std::isdigit(static_cast<unsigned char>(*p))) {
            hasDigit = true;
            ++p;
            continue;
        }
        if (*p == '.' && !hasDot) {
            hasDot = true;
            ++p;
            continue;
        }
        return false;
    }
    return hasDigit;
}

// 生成 yyyyMMddHHmmss 时间戳。
std::string DatabaseImpl::CurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_s(&localTime, &now);

    char buffer[32] = {0};
    std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &localTime);
    return buffer;
}

// 写 Error 日志。
void DatabaseImpl::LogError(const char* operation, const char* message) const {
    ILogger* logger = GetLogger();
    if (!logger) {
        return;
    }
    logger->Write(LogLevel::Error,
                  ModuleInfo::Name,
                  operation ? operation : "",
                  "",
                  "",
                  "",
                  message ? message : "");
}

// 写 Info 日志。
void DatabaseImpl::LogInfo(const char* operation, const char* message) const {
    ILogger* logger = GetLogger();
    if (!logger) {
        return;
    }
    logger->Write(LogLevel::Info,
                  ModuleInfo::Name,
                  operation ? operation : "",
                  "",
                  "",
                  "",
                  message ? message : "");
}
