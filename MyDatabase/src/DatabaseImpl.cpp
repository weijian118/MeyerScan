// =============================================================================
// 文件:    DatabaseImpl.cpp
// 模块:    MeyerScan_Database.dll
// 版本号:  v1.1.0
//
// 用途说明:
//   数据库模块的具体实现代码。实现了 IDatabase 接口定义的所有方法，
//   包括配置加载、数据库连接、SQL 执行、事务管理、备份功能等。
//
// 实现细节:
//   1. 使用 QJsonDocument 解析 JSON 配置文件
//   2. 使用 QSqlDatabase 管理 MySQL/SQLite 连接
//   3. 使用 QMutex 实现线程安全
//   4. 使用 robocopy 复制 MySQL 数据目录进行备份（仅 Windows 平台）
//   5. 使用 QFile::copy 复制 SQLite 数据库文件
//   6. 使用 LoadLibrary/GetProcAddress 动态加载 Logger.dll
//
// 返回值规范:
//   所有公共方法遵循架构规范，返回 Result<T> 或 VoidResult。
//   调用方必须检查 IsSuccess() 确认操作结果。
//
// Logger 集成:
//   由于 Logger.dll（/MT 静态 CRT）与 Database.dll（/MD 动态 CRT）
//   使用不同的 CRT 链接方式，不能直接在编译时链接 Logger.lib。
//   因此采用 LoadLibrary 运行时动态加载的方式获取 Logger 实例。
//   如果 Logger.dll 不存在或加载失败，日志操作静默跳过。
//
// @todo 密码安全:
//   MySQL 账号密码当前硬编码在源码中（admin/123456）。
//   后续需要改为通过 ConfigCenter + Crypto 加密存储的方式，
//   届时删除此处的硬编码凭据。
//
// @todo 备份路径:
//   MySQL 数据目录当前从 JSON 配置读取，支持相对配置文件目录解析；
//   后续需要迁入 ConfigCenter。
//
// 依赖项:
//   - Qt5Core: 核心功能（文件、JSON、进程等）
//   - Qt5Sql: 数据库访问
//   - Windows API: LoadLibrary / GetProcAddress（动态加载 Logger）
//   - Windows API: robocopy 命令（MySQL 备份）
// =============================================================================

#include "DatabaseImpl.h"
#include <QByteArray>          // Qt 字节数组
#include <QFile>              // Qt 文件操作
#include <QJsonArray>         // Qt JSON 数组
#include <QJsonDocument>      // Qt JSON 文档解析
#include <QJsonObject>        // Qt JSON 对象
#include <QJsonValue>         // Qt JSON 值
#include <QDir>               // Qt 目录操作
#include <QFileInfo>          // Qt 文件信息和路径判断
#include <QProcess>           // Qt 进程执行（用于 robocopy）
#include <QSqlRecord>         // Qt SQL 记录元数据
#include <QVariant>           // Qt 通用值类型
#include <QUuid>              // Qt UUID 生成（用于唯一连接名）
#include <QDebug>             // Qt 调试输出（保留，仅在极端情况下使用）

#include <Windows.h>          // LoadLibrary / GetProcAddress
#include <cstring>            // std::memset / std::strncpy

namespace {
// 将 QString 按 UTF-8 复制到固定长度 char 缓冲区。
// 数据库公共结构使用 POD 字符数组，不能把 QString 或 std::string 直接跨 DLL 返回。
void CopyUtf8(char* target, size_t targetSize, const QString& value) {
    if (!target || targetSize == 0) {
        // 调用方没有提供有效缓冲区，不能写入任何内容。
        return;
    }
    // 先清零固定数组，避免上一次写入的长字符串尾部残留。
    std::memset(target, 0, targetSize);
    const QByteArray bytes = value.toUtf8();
    // 固定数组最后一个字节留给 '\0'，方便 C ABI 调用方按字符串读取。
    std::strncpy(target, bytes.constData(), targetSize - 1);
}
}

// =============================================================================
// 硬编码的 MySQL 连接凭据
// =============================================================================
// 说明:
//   MySQL 数据库的用户名和密码当前硬编码在 DLL 内部。
//   这是临时方案，后续会通过 ConfigCenter 读取加密存储的凭据。
//
// @todo 迁移计划:
//   1. ConfigCenter 添加数据库凭据配置项（加密存储）
//   2. Crypto 提供解密能力
//   3. Database::Init() 从 ConfigCenter 获取解密后的凭据
//   4. 删除此处的静态常量
// =============================================================================
static const char* MYSQL_USER = "admin";
static const char* MYSQL_PASSWORD = "123456";

// =============================================================================
// 模块信息常量（用于日志输出和版本查询）
// =============================================================================
// 说明:
//   因为 Database 不链接 Logger.lib，不能使用 MEYER_LOG_* 宏
//   （宏内部调用了 GetLogger() 的 dllimport 声明），所以直接使用
//   ModuleInfo::Name 作为日志模块名。Name 必须与 vcxproj 中的
//   MEYER_MODULE_NAME 保持一致；Version 必须与 Version.rc 同步。
// =============================================================================
namespace ModuleInfo {
const char* Name = "MeyerScan_Database";
const char* Version = "MeyerScan_Database v1.2.0 (2026-06-23)";
}

// =============================================================================
// 静态成员初始化
// =============================================================================
ILogger* DatabaseImpl::s_logger = nullptr;
void*    DatabaseImpl::s_loggerModule = nullptr;

// =============================================================================
// DLL 导出函数：GetDatabase
// =============================================================================
// 说明:
//   工厂函数，返回数据库模块的单例实例。
//   这是用户访问数据库功能的唯一入口。
// =============================================================================
extern "C" MEYERSCAN_DATABASE_API IDatabase* GetDatabase() {
    return &DatabaseImpl::Instance();
}

// =============================================================================
// GetLogger - 动态加载 Logger 实例
// =============================================================================
// 说明:
//   通过 LoadLibrary 运行时加载 MeyerScan_Logger.dll，
//   然后通过 GetProcAddress 获取 GetLogger 函数指针。
//   获取到的 ILogger* 实例被缓存，后续调用直接返回。
//
// 线程安全:
//   LoadLibrary 是线程安全的 Windows API。
//   首次调用后，后续读取 s_logger 是原子读取操作。
//
// 返回值:
//   - 成功加载 Logger.dll 时，返回 ILogger* 指针
//   - Logger.dll 不存在、加载失败或导出函数不可用时，返回 nullptr
// =============================================================================
ILogger* DatabaseImpl::GetLogger() {
    // 已缓存，直接返回
    if (s_logger) {
        return s_logger;
    }

    // 已尝试加载但失败，不再重试
    if (s_loggerModule == reinterpret_cast<void*>(-1)) {
        return nullptr;
    }

    // 动态加载 Logger.dll
    HMODULE module = LoadLibraryA("MeyerScan_Logger.dll");
    if (!module) {
        // 记录加载失败，后续不再重试
        s_loggerModule = reinterpret_cast<void*>(-1);
        return nullptr;
    }

    // 获取 GetLogger 函数地址
    auto getLogger = reinterpret_cast<GetLoggerFunc>(
        GetProcAddress(module, "GetLogger"));
    if (!getLogger) {
        FreeLibrary(module);
        s_loggerModule = reinterpret_cast<void*>(-1);
        return nullptr;
    }

    // 获取 Logger 实例并缓存
    s_logger = getLogger();
    s_loggerModule = module;

    // Logger 应由主程序在启动阶段最先初始化。
    // Database 只获取已初始化的实例，不使用空路径主动 Init，避免覆盖日志级别或触发初始化失败。

    return s_logger;
}

// =============================================================================
// 单例获取方法
// =============================================================================
// 说明:
//   返回 DatabaseImpl 的唯一实例。
//   使用 Meyer's 单例模式（C++11 线程安全的静态局部变量）。
// =============================================================================
DatabaseImpl& DatabaseImpl::Instance() {
    static DatabaseImpl s_instance;
    return s_instance;
}

// =============================================================================
// 构造函数
// =============================================================================
// 说明:
//   初始化所有成员变量为默认状态。
// =============================================================================
DatabaseImpl::DatabaseImpl()
    : m_connected(false) {
    // m_config 是跨 DLL 返回的 POD 结构，必须清零，避免保留随机栈/堆数据。
    memset(&m_config, 0, sizeof(m_config));
    // m_lastBackupTime 也是固定 char 数组，清零后默认就是空字符串。
    memset(m_lastBackupTime, 0, sizeof(m_lastBackupTime));
}

// =============================================================================
// 析构函数
// =============================================================================
// 说明:
//   对象销毁时调用 Shutdown() 释放资源。
// =============================================================================
DatabaseImpl::~DatabaseImpl() {
    Shutdown();
    // 释放 Logger 模块句柄（如果有）
    if (s_loggerModule && s_loggerModule != reinterpret_cast<void*>(-1)) {
        FreeLibrary(static_cast<HMODULE>(s_loggerModule));
        s_loggerModule = nullptr;
        s_logger = nullptr;
    }
}

// =============================================================================
// Init - 初始化数据库模块
// =============================================================================
// 参数:
//   configPath - 配置文件路径（UTF-8 编码）
//
// 返回值:
//   VoidResult::Ok()                - 初始化成功
//   VoidResult::Fail(InvalidParameter) - configPath 为空
//   VoidResult::Fail(DbQueryFailed)    - 配置文件不存在或格式错误
// =============================================================================
VoidResult DatabaseImpl::Init(const char* configPath) {
    QMutexLocker locker(&m_mutex);

    // 参数校验
    if (!configPath || configPath[0] == '\0') {
        LogError("Init", "Config path is empty");
        return VoidResult::Fail(ErrorCode::InvalidParameter,
                                "Config path is empty");
    }

    // 调用内部方法加载配置
    if (!LoadConfig(configPath)) {
        return VoidResult::Fail(ErrorCode::DbQueryFailed,
                                "Failed to load config file");
    }

    LogInfo("Init", "Database module initialized");
    return VoidResult::Ok();
}

// =============================================================================
// LoadConfig - 加载配置文件
// =============================================================================
// 参数:
//   configPath - 配置文件路径
//
// 返回值:
//   true  - 加载成功
//   false - 加载失败
//
// 功能说明:
//   读取 JSON 配置文件，解析数据库连接参数。
//
// 路径规则:
//   configPath 必须由主程序按 applicationDirPath()/ConfigCenter 传入。
//   JSON 内的路径建议使用相对路径；Database 会以 db_config.json 所在目录为基准解析。
//   禁止在 Database 内部回退到开发机绝对路径或当前工作目录。
//
// @todo 过渡方案
//   当前 Database 自己读取 JSON 配置文件。
//   等 ConfigCenter 就绪后，此方法将被替换为从 ConfigCenter 获取配置。
// =============================================================================
bool DatabaseImpl::LoadConfig(const char* configPath) {
    // 打开配置文件
    const QString configFilePath = QDir::fromNativeSeparators(QString::fromUtf8(configPath));
    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LogError("LoadConfig", "Failed to open config file");
        return false;
    }

    // 保存配置文件所在目录。后续 JSON 内相对路径都以这个目录为基准解析，
    // 而不是以进程 currentPath 为基准，避免第三方启动时路径错乱。
    m_configDir = QFileInfo(configFilePath).absoluteDir().absolutePath();

    // 读取文件内容
    QByteArray data = file.readAll();
    file.close();

    // 解析 JSON 文档
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        LogError("LoadConfig", "Failed to parse JSON config");
        return false;
    }

    // 获取根对象
    QJsonObject root = doc.object();

    // 解析数据库类型
    // JSON 缺省使用 mysql，保持与当前已安装软件默认数据库一致。
    QString dbTypeValue = root["databaseType"].toString("mysql");
    m_config.dbType = (dbTypeValue.toLower() == "sqlite") ?
        static_cast<int32_t>(DatabaseType::SQLite) :
        static_cast<int32_t>(DatabaseType::MySQL);
    m_config.version = 1;

    // 解析 MySQL 配置
    QJsonObject mysql = root["mysql"].toObject();

    // 下面所有 CopyUtf8 都是把 QString 安全复制进 DbConfig 的固定 char 数组。
    // 这样调用方可以跨 DLL 读取配置快照，不涉及 QString 内存所有权。
    CopyUtf8(m_config.mysqlHost,
             sizeof(m_config.mysqlHost),
             mysql["host"].toString("127.0.0.1"));

    m_config.mysqlPort = mysql["port"].toInt(3308);

    CopyUtf8(m_config.mysqlService,
             sizeof(m_config.mysqlService),
             mysql["service"].toString("MSCANDB"));

    CopyUtf8(m_config.mysqlDatabase,
             sizeof(m_config.mysqlDatabase),
             mysql["database"].toString("mscan"));

    // dataDir 支持相对路径。ResolvePathFromConfig 会相对 db_config.json 所在目录解析。
    const QString mysqlDataDir = ResolvePathFromConfig(mysql["dataDir"].toString("../MySQL/data/mscan"));
    CopyUtf8(m_config.mysqlDataDir, sizeof(m_config.mysqlDataDir), mysqlDataDir);

    // 解析 SQLite 配置
    // sqlitePath 同样允许相对路径，便于整个安装目录迁移。
    const QString sqlitePath = ResolvePathFromConfig(root["sqlitePath"].toString("data/MeyerScanSQLite.db"));
    CopyUtf8(m_config.sqlitePath, sizeof(m_config.sqlitePath), sqlitePath);

    LogInfo("LoadConfig", "Config loaded");
    return true;
}

// =============================================================================
// ResolvePathFromConfig - 解析配置路径
// =============================================================================
QString DatabaseImpl::ResolvePathFromConfig(const QString& configuredPath) const {
    // 统一把 Windows 反斜杠转成 Qt 风格斜杠，后续拼接/比较更稳定。
    QString normalized = QDir::fromNativeSeparators(configuredPath.trimmed());
    if (normalized.isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(normalized);
    if (fileInfo.isAbsolute()) {
        // 已经是绝对路径时只做 cleanPath，不再套配置目录。
        return QDir::cleanPath(normalized);
    }

    // 相对路径以配置文件目录为基准，而不是以进程工作目录为基准。
    QDir baseDir(m_configDir);
    return QDir::cleanPath(baseDir.filePath(normalized));
}

// =============================================================================
// Connect - 建立数据库连接
// =============================================================================
// 返回值:
//   VoidResult::Ok()                    - 连接成功（或已连接）
//   VoidResult::Fail(DbConnectionFailed) - 连接失败
// =============================================================================
VoidResult DatabaseImpl::Connect() {
    QMutexLocker locker(&m_mutex);

    // 已连接状态，直接返回成功
    if (m_connected) {
        return VoidResult::Ok();
    }

    // 根据数据库类型调用相应连接方法
    bool success = false;
    if (m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)) {
        success = ConnectMySQL();
    } else {
        success = ConnectSQLite();
    }

    if (!success) {
        return VoidResult::Fail(ErrorCode::DbConnectionFailed,
                                "Database connection failed");
    }

    LogInfo("Connect", "Database connected");
    return VoidResult::Ok();
}

// =============================================================================
// ConnectMySQL - 连接 MySQL 数据库
// =============================================================================
// 说明:
//   连接参数:
//   - 主机: m_config.mysqlHost
//   - 端口: m_config.mysqlPort
//   - 数据库: m_config.mysqlDatabase
//   - 用户名: admin（硬编码）
//   - 密码: 123456（硬编码）
//
// @todo 凭据来源
//   当前用户名密码硬编码，后续改为从 ConfigCenter 获取加密存储的凭据。
// =============================================================================
bool DatabaseImpl::ConnectMySQL() {
    // 生成唯一连接名，避免与现有 Qt 连接冲突
    QString connectionName = QUuid::createUuid().toString();

    // 创建 QMYSQL 数据库连接
    // Qt 按 connectionName 管理连接池，后续 removeDatabase 必须使用同一个名字。
    m_db = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    m_connectionName = connectionName;

    // 设置连接参数
    m_db.setHostName(QString::fromUtf8(m_config.mysqlHost));
    m_db.setPort(m_config.mysqlPort);
    m_db.setDatabaseName(QString::fromUtf8(m_config.mysqlDatabase));
    m_db.setUserName(QString::fromUtf8(MYSQL_USER));
    m_db.setPassword(QString::fromUtf8(MYSQL_PASSWORD));

    // 尝试打开连接
    if (!m_db.open()) {
        LogError("ConnectMySQL",
                 m_db.lastError().text().toUtf8().constData());
        // 打开失败时必须先把 m_db 置空，再 removeDatabase。
        // Qt 要求没有 QSqlDatabase 对象引用该连接时才能安全移除。
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    // 更新连接状态
    m_connected = true;
    LogInfo("ConnectMySQL", "MySQL connected");
    return true;
}

// =============================================================================
// ConnectSQLite - 连接 SQLite 数据库
// =============================================================================
// 说明:
//   创建 QSQLITE 数据库连接，指向指定的数据库文件。
//   如果文件不存在，SQLite 会自动创建。
// =============================================================================
bool DatabaseImpl::ConnectSQLite() {
    // 生成唯一连接名
    QString connectionName = QUuid::createUuid().toString();

    // 创建 QSQLITE 数据库连接
    // SQLite 驱动同样使用独立 connectionName，避免和其他模块的默认连接冲突。
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_connectionName = connectionName;

    // 设置数据库文件路径
    // SQLite 文件路径来自配置文件，允许相对安装目录迁移。
    m_db.setDatabaseName(QString::fromUtf8(m_config.sqlitePath));

    // 尝试打开连接
    if (!m_db.open()) {
        LogError("ConnectSQLite",
                 m_db.lastError().text().toUtf8().constData());
        // 失败清理顺序同 MySQL：先释放 QSqlDatabase 引用，再移除连接名。
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    // 更新连接状态
    m_connected = true;
    LogInfo("ConnectSQLite", "SQLite connected");
    return true;
}

// =============================================================================
// Disconnect - 断开数据库连接
// =============================================================================
// 返回值: 始终返回 VoidResult::Ok()
// =============================================================================
VoidResult DatabaseImpl::Disconnect() {
    QMutexLocker locker(&m_mutex);

    // 检查连接状态
    if (m_db.isValid() && m_db.isOpen()) {
        // close 只关闭当前连接，真正从 Qt 连接池移除还要 removeDatabase。
        m_db.close();
    }
    // 必须先让 m_db 不再引用 connectionName，否则 Qt 会警告连接仍在使用。
    m_db = QSqlDatabase();

    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
    }

    if (m_connected) {
        LogInfo("Disconnect", "Database disconnected");
    }
    m_connected = false;

    return VoidResult::Ok();
}

// =============================================================================
// IsConnected - 检查连接状态
// =============================================================================
bool DatabaseImpl::IsConnected() const {
    // m_connected 是模块自己的状态位，m_db.isOpen() 是 Qt 连接实际状态。
    // 两者都满足时才认为数据库可用。
    return m_connected && m_db.isOpen();
}

// =============================================================================
// GetDatabaseType - 获取数据库类型
// =============================================================================
DatabaseType DatabaseImpl::GetDatabaseType() const {
    // m_config 中保存 int32_t 是为了 POD 结构跨 DLL 稳定；
    // 对外接口再转换回 DatabaseType 枚举。
    return static_cast<DatabaseType>(m_config.dbType);
}

// =============================================================================
// Backup - 备份数据库
// =============================================================================
// @todo
//   MySQL 备份路径当前来自 Database JSON 配置，后续需要改为从 ConfigCenter 读取。
// =============================================================================
VoidResult DatabaseImpl::Backup(const char* backupPath) {
    QMutexLocker locker(&m_mutex);

    // 参数校验
    if (!backupPath || backupPath[0] == '\0') {
        LogError("Backup", "Backup path is empty");
        return VoidResult::Fail(ErrorCode::InvalidParameter, "Backup path is empty");
    }

    // 检查连接状态
    if (!m_connected) {
        LogError("Backup", "Database is not connected");
        return VoidResult::Fail(ErrorCode::NotInitialized, "Database is not connected");
    }

    // 根据数据库类型执行备份
    bool success = false;
    if (m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)) {
        success = BackupMySQL(backupPath);
    } else {
        success = BackupSQLite(backupPath);
    }

    if (!success) {
        return VoidResult::Fail(ErrorCode::UnknownError, "Backup failed");
    }

    LogInfo("Backup", "Database backup completed");
    return VoidResult::Ok();
}

// =============================================================================
// BackupMySQL - 备份 MySQL 数据库
// =============================================================================
// 说明:
//   使用 Windows robocopy 命令复制 MySQL 数据目录。
//   备份目录名格式: yyyyMMddHHmmss-mscan
//
// @todo
//   MySQL 数据目录后续改为从 ConfigCenter 读取。
//
// robocopy 参数:
//   /E    - 复制子目录（包括空目录）
//   /R:0  - 失败不重试
//   /W:0  - 等待时间为 0
// =============================================================================
bool DatabaseImpl::BackupMySQL(const char* backupPath) {
    // 生成时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");

    QString sourceDir = QString::fromUtf8(m_config.mysqlDataDir);
    if (sourceDir.isEmpty()) {
        // MySQL 备份依赖 dataDir，如果配置为空，robocopy 没有可靠源目录。
        LogError("BackupMySQL", "MySQL data directory is empty");
        return false;
    }

    // 构建目标目录路径
    QString targetDir = QString("%1/%2-mscan").arg(backupPath).arg(timestamp);

    // 创建目标目录
    QDir dir;
    if (!dir.mkpath(targetDir)) {
        LogError("BackupMySQL", "Failed to create backup directory");
        return false;
    }

    // 执行 robocopy 命令
    // robocopy 返回值: 0-7 成功，8+ 错误
    QStringList args;
    args << sourceDir << targetDir << "/E" << "/R:0" << "/W:0";

    int ret = QProcess::execute("robocopy", args);
    if (ret > 7) {
        // robocopy 0~7 都可能表示成功或带警告成功，8 以上才是失败。
        LogError("BackupMySQL", "robocopy failed");
        return false;
    }

    // 记录备份时间
    // m_lastBackupTime 是固定数组，保留最后一次成功备份时间供 UI 显示。
    strncpy(m_lastBackupTime, timestamp.toUtf8().constData(),
            sizeof(m_lastBackupTime) - 1);
    LogInfo("BackupMySQL", "MySQL backup completed");
    return true;
}

// =============================================================================
// BackupSQLite - 备份 SQLite 数据库
// =============================================================================
// 说明:
//   使用 QFile::copy 复制 SQLite 数据库文件。
//   备份文件名格式: yyyyMMddHHmmss-MeyerScanSQLite.db
// =============================================================================
bool DatabaseImpl::BackupSQLite(const char* backupPath) {
    // 生成时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");

    // 源文件路径
    // SQLite 是单文件数据库，备份逻辑比 MySQL 简单，直接复制数据库文件。
    QString sourceFile = QString::fromUtf8(m_config.sqlitePath);

    // 目标文件路径
    QString targetFile = QString("%1/%2-MeyerScanSQLite.db")
        .arg(backupPath).arg(timestamp);

    // 复制文件
    if (!QFile::copy(sourceFile, targetFile)) {
        // QFile::copy 不覆盖已存在文件；由于文件名含秒级时间戳，正常不会冲突。
        LogError("BackupSQLite", "Failed to copy SQLite database file");
        return false;
    }

    // 记录备份时间
    strncpy(m_lastBackupTime, timestamp.toUtf8().constData(),
            sizeof(m_lastBackupTime) - 1);
    LogInfo("BackupSQLite", "SQLite backup completed");
    return true;
}

// =============================================================================
// GetLastBackupTime - 获取上次备份时间
// =============================================================================
const char* DatabaseImpl::GetLastBackupTime() const {
    return m_lastBackupTime;
}

// =============================================================================
// ExecuteQuery - 执行查询语句
// =============================================================================
// 返回值:
//   Result::Ok(DbResult)          - 查询执行成功
//   Result::Fail(NotInitialized)  - 数据库未连接
//   Result::Fail(DbQueryFailed)   - SQL 执行失败
// =============================================================================
Result<DbResult> DatabaseImpl::ExecuteQuery(const char* sql) {
    QMutexLocker locker(&m_mutex);

    // 初始化结果
    DbResult result;
    result.version = 1;
    result.affectedRows = 0;
    memset(result.reserved, 0, sizeof(result.reserved));

    // 参数校验
    if (!sql || sql[0] == '\0') {
        LogError("ExecuteQuery", "SQL is empty");
        return Result<DbResult>::Fail(ErrorCode::InvalidParameter,
                                      "SQL is empty");
    }

    // 检查连接状态
    if (!IsConnected()) {
        LogError("ExecuteQuery", "Database is not connected");
        return Result<DbResult>::Fail(ErrorCode::NotInitialized,
                                      "Database is not connected");
    }

    // 执行查询
    // ExecuteQuery 只返回影响行数，不返回结果集。
    // 需要结果集时使用 ExecuteQueryJson，避免调用方直接依赖 QSqlQuery。
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromUtf8(sql))) {
        LogError("ExecuteQuery", query.lastError().text().toUtf8().constData());
        return Result<DbResult>::Fail(
            ErrorCode::DbQueryFailed,
            "SQL query failed");
    }

    // 记录影响行数
    result.affectedRows = query.numRowsAffected();
    return Result<DbResult>::Ok(result);
}

// =============================================================================
// ExecuteQueryJson - 执行查询并返回 JSON 结果
// =============================================================================
// 说明:
//   只做通用 SQL 结果集到 JSON 的转换，不理解业务字段语义。
// =============================================================================
Result<DbJsonResult> DatabaseImpl::ExecuteQueryJson(const char* sql,
                                                    char* jsonBuffer,
                                                    int32_t jsonBufferSize) {
    QMutexLocker locker(&m_mutex);

    // DbJsonResult 是 POD 返回结构，先填默认值，后续只写 rowCount/bytesWritten。
    DbJsonResult result;
    result.version = 1;
    result.rowCount = 0;
    result.bytesWritten = 0;
    memset(result.reserved, 0, sizeof(result.reserved));

    if (!sql || sql[0] == '\0') {
        // SQL 为空时直接返回参数错误，不交给 Qt 驱动产生模糊错误。
        LogError("ExecuteQueryJson", "SQL is empty");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter,
                                          "SQL is empty");
    }
    if (!jsonBuffer || jsonBufferSize <= 0) {
        // JSON 由调用方提供缓冲区接收，缓冲区无效时不能执行查询。
        LogError("ExecuteQueryJson", "JSON output buffer is invalid");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter,
                                          "JSON output buffer is invalid");
    }
    if (!IsConnected()) {
        // 所有查询都要求已连接，避免 QSqlQuery 在无效连接上产生不可预测行为。
        LogError("ExecuteQueryJson", "Database is not connected");
        return Result<DbJsonResult>::Fail(ErrorCode::NotInitialized,
                                          "Database is not connected");
    }

    // Database 模块只负责通用 SQL 执行，不判断业务字段含义。
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromUtf8(sql))) {
        LogError("ExecuteQueryJson", query.lastError().text().toUtf8().constData());
        return Result<DbJsonResult>::Fail(ErrorCode::DbQueryFailed,
                                          "SQL query failed");
    }

    // 先读取字段名，输出到 columns，方便调用方调试或做通用表格展示。
    const QSqlRecord record = query.record();
    QJsonArray columns;
    for (int i = 0; i < record.count(); ++i) {
        columns.append(record.fieldName(i));
    }

    // 将每一行转换成 JSON 对象。
    // QVariant -> QJsonValue 会处理常见基础类型，复杂类型后续再按需要扩展。
    QJsonArray rows;
    while (query.next()) {
        QJsonObject row;
        for (int i = 0; i < record.count(); ++i) {
            const QString fieldName = record.fieldName(i);
            const QVariant value = query.value(i);
            row.insert(fieldName, QJsonValue::fromVariant(value));
        }
        rows.append(row);
    }

    // 统一输出格式，服务层可以稳定解析 rows，不暴露 Qt 类型。
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("rowCount", rows.size());
    root.insert("columns", columns);
    root.insert("rows", rows);

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (bytes.size() + 1 > jsonBufferSize) {
        // 缓冲区不足时不写半截 JSON，直接返回失败，防止调用方解析截断内容。
        LogError("ExecuteQueryJson", "JSON output buffer is too small");
        return Result<DbJsonResult>::Fail(ErrorCode::InvalidParameter,
                                          "JSON output buffer is too small");
    }

    // 清零后再复制，保证 jsonBuffer 总是以 '\0' 结尾。
    memset(jsonBuffer, 0, static_cast<size_t>(jsonBufferSize));
    memcpy(jsonBuffer, bytes.constData(), static_cast<size_t>(bytes.size()));

    result.rowCount = rows.size();
    result.bytesWritten = bytes.size();
    return Result<DbJsonResult>::Ok(result);
}

// =============================================================================
// ExecuteUpdate - 执行更新语句
// =============================================================================
// 返回值:
//   Result::Ok(DbResult)          - 更新执行成功
//   Result::Fail(NotInitialized)  - 数据库未连接
//   Result::Fail(DbUpdateFailed)  - SQL 执行失败
// =============================================================================
Result<DbResult> DatabaseImpl::ExecuteUpdate(const char* sql) {
    QMutexLocker locker(&m_mutex);

    // 初始化结果
    DbResult result;
    result.version = 1;
    result.affectedRows = 0;
    memset(result.reserved, 0, sizeof(result.reserved));

    // 参数校验
    if (!sql || sql[0] == '\0') {
        LogError("ExecuteUpdate", "SQL is empty");
        return Result<DbResult>::Fail(ErrorCode::InvalidParameter,
                                      "SQL is empty");
    }

    // 检查连接状态
    if (!IsConnected()) {
        LogError("ExecuteUpdate", "Database is not connected");
        return Result<DbResult>::Fail(ErrorCode::NotInitialized,
                                      "Database is not connected");
    }

    // 执行更新
    // INSERT/UPDATE/DELETE/DDL 都走这里，返回 numRowsAffected 供调用方参考。
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromUtf8(sql))) {
        LogError("ExecuteUpdate", query.lastError().text().toUtf8().constData());
        return Result<DbResult>::Fail(
            ErrorCode::DbUpdateFailed,
            "SQL update failed");
    }

    // 记录影响行数
    result.affectedRows = query.numRowsAffected();
    return Result<DbResult>::Ok(result);
}

// =============================================================================
// ExecuteScript - 批量执行 SQL 脚本
// =============================================================================
// 返回值: 成功执行的 SQL 语句数量
//
// 说明:
//   返回值是成功数量而非错误码，因此不使用 Result 包装。
//   某条失败不会中断后续语句的执行。
// =============================================================================
int32_t DatabaseImpl::ExecuteScript(const char** sqlScripts, int32_t count) {
    // 参数校验
    if (!sqlScripts || count <= 0) {
        LogError("ExecuteScript", "Invalid script arguments");
        return 0;
    }

    int32_t successCount = 0;

    // 依次执行每条 SQL
    for (int32_t i = 0; i < count; ++i) {
        if (!sqlScripts[i]) {
            // 空脚本跳过，不计入成功数量。
            continue;
        }
        Result<DbResult> result = ExecuteUpdate(sqlScripts[i]);
        if (result.IsSuccess()) {
            ++successCount;
        }
    }

    LogInfo("ExecuteScript", QString("Executed %1/%2 SQL statements")
            .arg(successCount).arg(count).toUtf8().constData());
    return successCount;
}

// =============================================================================
// BeginTransaction - 开始事务
// =============================================================================
VoidResult DatabaseImpl::BeginTransaction() {
    QMutexLocker locker(&m_mutex);

    // 事务直接委托 Qt 驱动。
    // 如果数据库未连接或驱动不支持事务，Qt 会返回 false 并给出 lastError。
    bool success = m_db.transaction();
    if (!success) {
        LogError("BeginTransaction",
                 m_db.lastError().text().toUtf8().constData());
        return VoidResult::Fail(ErrorCode::DbTransactionFailed,
                                "Failed to begin transaction");
    }

    LogInfo("BeginTransaction", "Transaction started");
    return VoidResult::Ok();
}

// =============================================================================
// Commit - 提交事务
// =============================================================================
VoidResult DatabaseImpl::Commit() {
    // 所有事务操作都用同一个互斥锁保护，避免多个线程交叉提交/回滚同一连接。
    QMutexLocker locker(&m_mutex);

    // 提交失败时调用方应根据返回值决定是否告警或重试。
    bool success = m_db.commit();
    if (!success) {
        // lastError 属于当前 QSqlDatabase/QSqlDriver 状态，必须在失败现场立即读取。
        LogError("Commit",
                 m_db.lastError().text().toUtf8().constData());
        return VoidResult::Fail(ErrorCode::DbTransactionFailed,
                                "Failed to commit transaction");
    }

    // 成功提交后记录 Info，便于和 BeginTransaction/Rollback 成对追踪。
    LogInfo("Commit", "Transaction committed");
    return VoidResult::Ok();
}

// =============================================================================
// Rollback - 回滚事务
// =============================================================================
VoidResult DatabaseImpl::Rollback() {
    // 和 Commit 使用同一把锁，保证事务状态不会被其它线程同时修改。
    QMutexLocker locker(&m_mutex);

    // 回滚用于上层服务发生错误时撤销已执行 SQL。
    bool success = m_db.rollback();
    if (!success) {
        // 回滚失败通常表示连接异常或驱动状态异常，需要记录底层错误文本。
        LogError("Rollback",
                 m_db.lastError().text().toUtf8().constData());
        return VoidResult::Fail(ErrorCode::DbTransactionFailed,
                                "Failed to roll back transaction");
    }

    // 回滚成功也写 Info，方便确认异常路径是否完成清理。
    LogInfo("Rollback", "Transaction rolled back");
    return VoidResult::Ok();
}

// =============================================================================
// GetConfig - 获取当前配置
// =============================================================================
const DbConfig& DatabaseImpl::GetConfig() const {
    // 返回内部配置快照引用，调用方只读，不得保存后跨 Shutdown 使用。
    return m_config;
}

// =============================================================================
// SetDatabaseType - 切换数据库类型
// =============================================================================
VoidResult DatabaseImpl::SetDatabaseType(DatabaseType dbType) {
    {
        QMutexLocker locker(&m_mutex);

        // 切换数据库类型前必须关闭现有连接，否则 Qt 连接仍指向旧驱动。
        if (m_connected && m_db.isOpen()) {
            m_db.close();
            m_db = QSqlDatabase();
            if (!m_connectionName.isEmpty()) {
                // removeDatabase 必须在 m_db 置空后调用，避免 Qt 连接仍被引用。
                QSqlDatabase::removeDatabase(m_connectionName);
                m_connectionName.clear();
            }
            m_connected = false;
        }

        // 只切换类型，不重新加载配置；host/path 等参数仍来自 Init 读取的配置文件。
        m_config.dbType = static_cast<int32_t>(dbType);
    }

    // 释放锁后再 Connect，避免 Connect 内部再次加锁造成死锁。
    VoidResult result = Connect();

    if (result.IsSuccess()) {
        LogInfo("SetDatabaseType", "Database type switched");
    } else {
        LogError("SetDatabaseType", "Failed to switch database type");
    }

    return result;
}

// =============================================================================
// GetModuleVersion - 获取模块版本号
// =============================================================================
// 说明:
//   返回字符串格式版本号，方便调试器查看。
//   后续如需要程序化比较版本号，可增加 GetModuleInfo() 接口。
// =============================================================================
const char* DatabaseImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// =============================================================================
// Shutdown - 关闭数据库模块
// =============================================================================
VoidResult DatabaseImpl::Shutdown() {
    // Shutdown 可重复调用；Disconnect 内部会检查当前连接状态。
    Disconnect();
    LogInfo("Shutdown", "Database module shut down");
    return VoidResult::Ok();
}

// =============================================================================
// LogError - 输出错误日志
// =============================================================================
// 说明:
//   通过动态加载的 Logger 实例输出 Error 级别日志。
//   如果 Logger.dll 不可用，日志静默跳过。
// =============================================================================
void DatabaseImpl::LogError(const char* operation, const char* message) {
    // GetLogger 内部会缓存动态加载结果。
    // 这里虽然调用函数名是 GetLogger，但不是每条日志都重新 LoadLibrary/GetProcAddress。
    ILogger* logger = GetLogger();
    if (logger) {
        // Database 不直接链接 Logger.lib，所以只能通过动态获取到的 ILogger 写日志。
        logger->Write(LogLevel::Error,
                      ModuleInfo::Name,
                      // operation/message 都做空指针保护，避免错误路径再次崩溃。
                      operation ? operation : "",
                      "",   // deviceId（当前无设备关联）
                      "",   // caseId（当前无病例关联）
                      "",   // operator（当前无操作人关联）
                      message ? message : "");
    }
}

// =============================================================================
// LogInfo - 输出信息日志
// =============================================================================
// 说明:
//   通过动态加载的 Logger 实例输出 Info 级别日志。
//   如果 Logger.dll 不可用，日志静默跳过。
// =============================================================================
void DatabaseImpl::LogInfo(const char* operation, const char* message) {
    // Info 日志同样走动态 Logger 缓存；Logger 缺失时静默跳过。
    ILogger* logger = GetLogger();
    if (logger) {
        // Info 日志用于记录连接、断开、备份等正常流程节点。
        logger->Write(LogLevel::Info,
                      ModuleInfo::Name,
                      // 空 operation 统一写为空字符串，避免 Logger 收到 nullptr。
                      operation ? operation : "",
                      "",   // deviceId
                      "",   // caseId
                      "",   // operator
                      message ? message : "");
    }
}
