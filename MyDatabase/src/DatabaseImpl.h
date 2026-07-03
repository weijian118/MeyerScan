// =============================================================================
// 文件:    DatabaseImpl.h
// 模块:    MeyerScan_Database.dll
// 用途:
//   Database 公共接口的纯 C++ 实现类声明。
//
// 设计说明:
//   1. 本实现不包含 Qt 头文件，也不链接 QtCore/QtSql。
//   2. SQLite 通过 LoadLibrary/GetProcAddress 动态加载 sqlite3.dll。
//   3. MySQL 的公共配置和枚举继续保留；原生 MySQL C API 待 SDK 进入工程后接入。
//   4. 对外 ABI 仍保持 Database.h 中的 POD / UTF-8 / 调用方缓冲区形式。
// =============================================================================

#pragma once

#include "Database.h"

#include <mutex>
#include <string>

// =============================================================================
// Logger 相关前向声明和函数指针
// =============================================================================
// 说明:
//   Database 不直接链接 Logger.lib，而是运行时动态加载 MeyerScan_Logger.dll。
//   这样可以减少基础设施模块之间的编译期耦合。
// =============================================================================
enum class LogLevel : int {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4
};

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual bool Init(const char* logDir, LogLevel level) = 0;
    virtual void Write(LogLevel level,
                       const char* module,
                       const char* operation,
                       const char* deviceId,
                       const char* caseId,
                       const char* operator_,
                       const char* content) = 0;
    virtual void SetLogLevel(LogLevel level) = 0;
    virtual LogLevel GetLogLevel() const = 0;
    virtual void Flush() = 0;
    virtual void Shutdown() = 0;
    virtual const char* GetModuleVersion() const = 0;
};

using GetLoggerFunc = ILogger* (*)();

// =============================================================================
// DatabaseImpl 类
// =============================================================================
// 说明:
//   进程内单例。所有公开方法内部加锁，避免多个模块同时访问同一个连接句柄。
// =============================================================================
class DatabaseImpl : public IDatabase {
public:
    // 获取进程内唯一实例。
    static DatabaseImpl& Instance();

    // 加载数据库配置文件。
    VoidResult Init(const char* configPath) override;

    // 建立/断开连接。
    VoidResult Connect() override;
    VoidResult Disconnect() override;
    bool IsConnected() const override;
    DatabaseType GetDatabaseType() const override;

    // 备份当前数据库。
    VoidResult Backup(const char* backupPath) override;
    const char* GetLastBackupTime() const override;

    // 执行 SQL。
    Result<DbResult> ExecuteQuery(const char* sql) override;
    Result<DbResult> ExecuteUpdate(const char* sql) override;
    int32_t ExecuteScript(const char** sqlScripts, int32_t count) override;
    Result<DbJsonResult> ExecuteQueryJson(const char* sql,
                                          char* jsonBuffer,
                                          int32_t jsonBufferSize) override;

    // 事务控制。
    VoidResult BeginTransaction() override;
    VoidResult Commit() override;
    VoidResult Rollback() override;

    // 配置和版本信息。
    const DbConfig& GetConfig() const override;
    VoidResult SetDatabaseType(DatabaseType dbType) override;
    const char* GetModuleVersion() const override;

    // 释放连接和运行时资源。
    VoidResult Shutdown() override;

private:
    // 私有构造/析构，外部只能通过 Instance/GetDatabase 使用。
    DatabaseImpl();
    ~DatabaseImpl();

    // 单例对象禁止复制，避免两个对象持有同一个数据库句柄。
    DatabaseImpl(const DatabaseImpl&) = delete;
    DatabaseImpl& operator=(const DatabaseImpl&) = delete;

    // 动态加载 Logger 单例。
    static ILogger* GetLogger();

    // 配置读取和路径处理。
    bool LoadConfig(const char* configPath);
    std::string ResolvePathFromConfig(const std::string& configuredPath) const;
    static std::string NormalizePath(const std::string& path);
    static std::string DirectoryOf(const std::string& path);
    static bool IsAbsolutePath(const std::string& path);
    static bool EnsureDirectoryExists(const std::string& dirPath);
    static bool EnsureParentDirectoryExists(const std::string& filePath);

    // 极简 JSON 读取工具，只读取 db_config.json 中当前需要的字段。
    static std::string ExtractJsonString(const std::string& json,
                                         const std::string& key,
                                         const std::string& defaultValue);
    static int ExtractJsonInt(const std::string& json,
                              const std::string& key,
                              int defaultValue);
    static std::string ExtractObjectText(const std::string& json,
                                         const std::string& key);

    // SQLite 动态加载和连接。
    bool LoadSqliteRuntime();
    bool ConnectSQLite();
    bool ConnectMySQL();
    void CloseSqlite();
    const char* SqliteLastError() const;
    void SetLastErrorMessage(const char* message);
    void SetLastWin32ErrorMessage(const char* prefix, unsigned long errorCode);

    // SQL 执行内部方法。调用方必须已经持有 m_mutex。
    Result<DbResult> ExecuteSqlLocked(const char* sql, bool expectRows);
    Result<DbJsonResult> ExecuteQueryJsonLocked(const char* sql,
                                                char* jsonBuffer,
                                                int32_t jsonBufferSize);

    // SQLite 结果 JSON 序列化。
    static std::string EscapeJson(const char* text);
    static bool LooksLikeNumber(const char* text);

    // 备份实现。
    bool BackupSQLite(const char* backupPath);
    bool BackupMySQL(const char* backupPath);
    static std::string CurrentTimestamp();

    // 日志辅助。
    void LogError(const char* operation, const char* message) const;
    void LogInfo(const char* operation, const char* message) const;

private:
    // SQLite C API 前向声明和函数指针。
    struct sqlite3;
    struct sqlite3_stmt;
    using sqlite3_open_v2_fn = int (__cdecl *)(const char*, sqlite3**, int, const char*);
    using sqlite3_close_fn = int (__cdecl *)(sqlite3*);
    using sqlite3_exec_fn = int (__cdecl *)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
    using sqlite3_prepare_v2_fn = int (__cdecl *)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    using sqlite3_step_fn = int (__cdecl *)(sqlite3_stmt*);
    using sqlite3_finalize_fn = int (__cdecl *)(sqlite3_stmt*);
    using sqlite3_errmsg_fn = const char* (__cdecl *)(sqlite3*);
    using sqlite3_column_count_fn = int (__cdecl *)(sqlite3_stmt*);
    using sqlite3_column_name_fn = const char* (__cdecl *)(sqlite3_stmt*, int);
    using sqlite3_column_text_fn = const unsigned char* (__cdecl *)(sqlite3_stmt*, int);
    using sqlite3_column_type_fn = int (__cdecl *)(sqlite3_stmt*, int);
    using sqlite3_changes_fn = int (__cdecl *)(sqlite3*);
    using sqlite3_free_fn = void (__cdecl *)(void*);

    // Logger 动态加载状态。
    static ILogger* s_logger;
    static void* s_loggerModule;

    // 数据库配置快照。
    DbConfig m_config;

    // SQLite 运行时 DLL 句柄和数据库连接句柄。
    void* m_sqliteModule;
    sqlite3* m_sqliteDb;

    // SQLite 函数指针表。
    sqlite3_open_v2_fn m_sqliteOpenV2;
    sqlite3_close_fn m_sqliteClose;
    sqlite3_exec_fn m_sqliteExec;
    sqlite3_prepare_v2_fn m_sqlitePrepareV2;
    sqlite3_step_fn m_sqliteStep;
    sqlite3_finalize_fn m_sqliteFinalize;
    sqlite3_errmsg_fn m_sqliteErrmsg;
    sqlite3_column_count_fn m_sqliteColumnCount;
    sqlite3_column_name_fn m_sqliteColumnName;
    sqlite3_column_text_fn m_sqliteColumnText;
    sqlite3_column_type_fn m_sqliteColumnType;
    sqlite3_changes_fn m_sqliteChanges;
    sqlite3_free_fn m_sqliteFree;

    // 配置文件目录，用于解析配置中的相对路径。
    std::string m_configDir;

    // 连接状态。
    bool m_connected;

    // 保护配置、连接句柄和事务状态。
    mutable std::mutex m_mutex;

    // 最近一次备份时间。
    char m_lastBackupTime[32];

    // 最近一次底层错误描述。
    // 对外 VoidResult::message 只能返回 const char*，因此用固定数组保存，
    // 避免把 std::string 的跨 DLL 生命周期暴露给调用方。
    char m_lastErrorMessage[512];
};

extern "C" MEYERSCAN_DATABASE_API IDatabase* GetDatabase();
