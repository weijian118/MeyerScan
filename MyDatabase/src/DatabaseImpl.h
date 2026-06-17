// =============================================================================
// 文件:    DatabaseImpl.h
// 模块:    MeyerScan_Database.dll
// 版本号:  v1.1.0
//
// 用途说明:
//   数据库接口的具体实现类头文件。继承 IDatabase 接口，
//   提供完整的数据库操作功能实现，包括连接管理、SQL 执行、
//   事务控制、备份功能等。
//
// 设计原则:
//   1. 单例模式：整个进程只存在一个 DatabaseImpl 实例
//   2. 线程安全：使用 QMutex 保护所有公共方法
//   3. RAII 资源管理：利用 C++ 构造/析构函数管理资源生命周期
//   4. Logger 动态加载：通过 LoadLibrary/GetProcAddress 在运行时
//      获取 Logger 实例，无需编译时链接 Logger.lib，避免 CRT 冲突
//
// Logger 集成说明:
//   MeyerScan_Logger.dll 使用 /MT（静态 CRT）编译，
//   MeyerScan_Database.dll 使用 /MD（动态 CRT）编译。
//   两者 CRT 不同，不能直接链接 Logger.lib。
//   因此采用运行时动态加载方式：
//   1. 在首次需要写日志时调用 LoadLibrary("MeyerScan_Logger.dll")
//   2. 通过 GetProcAddress 获取 GetLogger() 函数指针
//   3. 缓存 ILogger* 实例，后续直接使用
//   4. 如果 Logger.dll 不存在或加载失败，日志操作静默跳过
//
// 内部结构:
//   - m_config: 存储数据库配置信息
//   - m_db: Qt SQL 数据库连接对象
//   - m_mutex: 线程互斥锁，保护所有操作
//   - m_connected: 连接状态标志
//   - m_lastBackupTime: 上次备份时间戳
//
// 注意事项:
//   - 此类为内部实现类，用户代码不应直接使用
//   - 所有公共接口方法都加锁，确保线程安全
//   - 内部方法不加锁，由调用者负责加锁
// =============================================================================

#pragma once

#include "Database.h"           // IDatabase 接口和结构体定义
#include <QSqlDatabase>         // Qt SQL 数据库类
#include <QSqlQuery>            // Qt SQL 查询类
#include <QSqlError>            // Qt SQL 错误类
#include <QMutex>               // Qt 互斥锁类
#include <QDateTime>            // Qt 日期时间类
#include <QString>              // Qt 字符串类
#include <memory>               // C++ 智能指针

// =============================================================================
// Logger 相关前向声明和类型定义
// =============================================================================
// 说明:
//   不使用 Logger.h 中的 extern "C" ILogger* GetLogger() 声明
//   （该声明使用 __declspec(dllimport) 需要链接 Logger.lib），
//   而是通过 LoadLibrary/GetProcAddress 在运行时动态加载。
//   因此这里手动声明 ILogger 的完整接口，以及定义函数指针类型。
// =============================================================================

// LogLevel 枚举定义（与 Logger.h 保持一致）
// 注意: Logger.h 中 LogLevel 定义在 enum class 中，
// 此处重复定义以确保 DatabaseImpl 中可用。
// @todo 待 Core.lib 建好后，改为 #include "Core.h"
enum class LogLevel : int {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4
};

// ILogger 接口前向声明（完整接口在 Logger.h 中定义）
// 这里只声明我们需要用到的 Write 方法。
// 实际调用时通过函数指针，不需要完整的类定义。
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

// GetLogger 函数指针类型
using GetLoggerFunc = ILogger* (*)();

// =============================================================================
// DatabaseImpl 类声明
// =============================================================================
// 说明:
//   数据库接口的具体实现类。
//   采用单例模式，通过 Instance() 方法获取唯一实例。
//   所有方法都是线程安全的，内部使用 QMutex 进行同步。
// =============================================================================
class DatabaseImpl : public IDatabase {
public:
    // =========================================================================
    // 获取单例实例
    // =========================================================================
    // 说明:
    //   获取 DatabaseImpl 的唯一实例。使用局部静态变量实现线程安全的单例。
    //   C++11 标准保证局部静态变量初始化的线程安全性。
    //
    // 返回值:
    //   DatabaseImpl 引用，指向唯一实例
    // =========================================================================
    static DatabaseImpl& Instance();

    // =========================================================================
    // IDatabase 接口实现
    // =========================================================================
    // 说明:
    //   以下方法继承自 IDatabase 接口，每个方法都是线程安全的，
    //   内部使用 QMutexLocker 自动加锁和解锁。
    // =========================================================================

    // 初始化：加载配置文件
    VoidResult Init(const char* configPath) override;

    // 连接管理：建立数据库连接
    VoidResult Connect() override;
    VoidResult Disconnect() override;
    bool IsConnected() const override;
    DatabaseType GetDatabaseType() const override;

    // 数据库备份
    VoidResult Backup(const char* backupPath) override;
    const char* GetLastBackupTime() const override;

    // SQL 执行
    Result<DbResult> ExecuteQuery(const char* sql) override;
    Result<DbResult> ExecuteUpdate(const char* sql) override;
    int32_t ExecuteScript(const char** sqlScripts, int32_t count) override;

    // 事务管理
    VoidResult BeginTransaction() override;
    VoidResult Commit() override;
    VoidResult Rollback() override;

    // 配置管理
    const DbConfig& GetConfig() const override;
    VoidResult SetDatabaseType(DatabaseType dbType) override;

    // 版本信息
    const char* GetModuleVersion() const override;

    // 关闭
    VoidResult Shutdown() override;

private:
    // =========================================================================
    // 私有构造函数
    // =========================================================================
    // 说明:
    //   私有构造函数，防止外部直接创建实例。
    //   初始化成员变量为默认值。
    // =========================================================================
    DatabaseImpl();

    // =========================================================================
    // 私有析构函数
    // =========================================================================
    // 说明:
    //   私有析构函数，确保单例对象的生命周期由类自身管理。
    //   析构时调用 Shutdown() 确保资源释放。
    // =========================================================================
    ~DatabaseImpl();

    // =========================================================================
    // 禁止拷贝和赋值
    // =========================================================================
    DatabaseImpl(const DatabaseImpl&) = delete;
    DatabaseImpl& operator=(const DatabaseImpl&) = delete;

    // =========================================================================
    // Logger 动态加载
    // =========================================================================
    // 说明:
    //   运行时动态加载 Logger.dll 并缓存 ILogger* 指针。
    //   通过 LoadLibrary/GetProcAddress 方式加载，避免编译时
    //   链接 Logger.lib 带来的 CRT 冲突问题。
    //
    // 线程安全:
    //   首次调用时执行 LoadLibrary（仅一次），后续直接返回缓存指针。
    //   不需要额外加锁，因为 LoadLibrary 是线程安全的。
    // =========================================================================

    // -------------------------------------------------------------------------
    // GetLogger - 获取 Logger 实例
    // -------------------------------------------------------------------------
    // 返回值:
    //   ILogger* 指针（如果 Logger.dll 加载成功）
    //   nullptr  （如果 Logger.dll 不存在或加载失败）
    //
    // 功能说明:
    //   1. 检查 s_logger 是否已缓存，有则直接返回
    //   2. 调用 LoadLibrary 加载 MeyerScan_Logger.dll
    //   3. 通过 GetProcAddress 获取 GetLogger 函数指针
    //   4. 调用 GetLogger() 获取 ILogger 实例并缓存
    //   5. 后续调用直接返回缓存的指针
    //
    // 注意事项:
    //   - 此方法可被多次调用，实际加载只执行一次
    //   - 返回 nullptr 时调用方应静默跳过日志写入
    // -------------------------------------------------------------------------
    static ILogger* GetLogger();

    // 缓存 Logger 实例指针和模块句柄
    static ILogger* s_logger;
    static void*    s_loggerModule;

    // =========================================================================
    // 内部方法（线程安全由调用者保证）
    // =========================================================================

    // -------------------------------------------------------------------------
    // LoadConfig - 加载配置文件
    // -------------------------------------------------------------------------
    bool LoadConfig(const char* configPath);

    // -------------------------------------------------------------------------
    // ConnectMySQL - 连接 MySQL 数据库
    // -------------------------------------------------------------------------
    bool ConnectMySQL();

    // -------------------------------------------------------------------------
    // ConnectSQLite - 连接 SQLite 数据库
    // -------------------------------------------------------------------------
    bool ConnectSQLite();

    // -------------------------------------------------------------------------
    // BackupMySQL - 备份 MySQL 数据库
    // -------------------------------------------------------------------------
    bool BackupMySQL(const char* backupPath);

    // -------------------------------------------------------------------------
    // BackupSQLite - 备份 SQLite 数据库
    // -------------------------------------------------------------------------
    bool BackupSQLite(const char* backupPath);

    // =========================================================================
    // 日志辅助方法
    // =========================================================================
    // 说明:
    //   统一通过 GetLogger() 动态加载的 Logger 实例输出日志。
    //   如果 Logger.dll 不可用，日志操作静默跳过，不影响功能。
    // =========================================================================

    // -------------------------------------------------------------------------
    // LogError - 输出错误日志
    // -------------------------------------------------------------------------
    // 功能说明:
    //   输出 Error 级别日志，包含模块名 "MeyerScan_Database"、
    //   操作名和内容。
    // -------------------------------------------------------------------------
    void LogError(const char* operation, const char* message);

    // -------------------------------------------------------------------------
    // LogInfo - 输出信息日志
    // -------------------------------------------------------------------------
    // 功能说明:
    //   输出 Info 级别日志。
    // -------------------------------------------------------------------------
    void LogInfo(const char* operation, const char* message);

    // =========================================================================
    // 成员变量
    // =========================================================================

    // 数据库配置信息
    DbConfig m_config;

    // Qt SQL 数据库对象
    QSqlDatabase m_db;

    // 线程互斥锁
    mutable QMutex m_mutex;

    // 连接状态标志
    bool m_connected;

    // 上次备份时间
    char m_lastBackupTime[32];
};

// =============================================================================
// DLL 导出函数声明
// =============================================================================
extern "C" MEYERSCAN_DATABASE_API IDatabase* GetDatabase();
