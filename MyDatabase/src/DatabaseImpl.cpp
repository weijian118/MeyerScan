// =============================================================================
// 文件名:    DatabaseImpl.cpp
// 模块名:    MeyerScan_Database.dll
// 版本号:    v1.0.0
//
// 用途说明:
//   数据库模块的具体实现代码。实现了 IDatabase 接口定义的所有方法，
//   包括配置加载、数据库连接、SQL执行、事务管理、备份功能等。
//
// 实现细节:
//   1. 使用 QJsonDocument 解析 JSON 配置文件
//   2. 使用 QSqlDatabase 管理 MySQL/SQLite 连接
//   3. 使用 QMutex 实现线程安全
//   4. 使用 robocopy 复制 MySQL 数据目录进行备份
//   5. 使用 QFile::copy 复制 SQLite 数据库文件
//
// 依赖项:
//   - Qt5Core: 核心功能（文件、JSON、进程等）
//   - Qt5Sql: 数据库访问
//   - Windows API: robocopy 命令
//
// 注意事项:
//   - MySQL 账号密码硬编码在源码中
//   - 备份使用 Windows robocopy 命令，仅支持 Windows 平台
// =============================================================================

#include "DatabaseImpl.h"
#include <QFile>              // Qt 文件操作
#include <QJsonDocument>      // Qt JSON 文档解析
#include <QJsonObject>        // Qt JSON 对象
#include <QDir>               // Qt 目录操作
#include <QProcess>           // Qt 进程执行（用于 robocopy）
#include <QUuid>              // Qt UUID 生成（用于唯一连接名）
#include <QDebug>             // Qt 调试输出

// =============================================================================
// 硬编码的 MySQL 连接凭据
// =============================================================================
// 说明:
//   MySQL 数据库的用户名和密码硬编码在 DLL 内部。
//   这是设计决策，目的是：
//   1. 简化配置，用户不需要管理凭据
//   2. 统一身份认证，所有模块使用同一账号
//   3. 安全考虑，凭据不暴露在配置文件中
//
// 注意:
//   - 生产环境应考虑使用更安全的方式（如加密存储）
//   - 当前用户名: admin，密码: 123456
// =============================================================================
static const char* MYSQL_USER = "admin";
static const char* MYSQL_PASSWORD = "123456";

// =============================================================================
// DLL 导出函数：GetDatabase
// =============================================================================
// 说明:
//   工厂函数，返回数据库模块的单例实例。
//   这是用户访问数据库功能的唯一入口。
//
// 实现:
//   调用 DatabaseImpl::Instance() 获取单例引用，
//   然后转换为 IDatabase 接口指针返回。
//
// 线程安全:
//   C++11 保证局部静态变量的线程安全初始化，
//   Instance() 内部的静态实例只会被创建一次。
// =============================================================================
extern "C" MEYERSCAN_DATABASE_API IDatabase* GetDatabase() {
    return &DatabaseImpl::Instance();
}

// =============================================================================
// 单例获取方法
// =============================================================================
// 说明:
//   返回 DatabaseImpl 的唯一实例。
//   使用 Meyer's 单例模式（局部静态变量）。
//
// 实现:
//   static DatabaseImpl s_instance;
//   - 第一次调用时创建实例
//   - 后续调用返回同一实例
//   - 程序结束时自动析构
//
// 线程安全:
//   C++11 标准保证局部静态变量的线程安全初始化。
//   如果多个线程同时首次调用，编译器会保证只有一个线程创建实例。
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
//   私有构造函数，防止外部直接创建实例。
//
// 初始化内容:
//   - m_connected = false: 初始未连接状态
//   - m_config: 清零所有字段
//   - m_lastBackupTime: 清零时间戳
// =============================================================================
DatabaseImpl::DatabaseImpl()
    : m_connected(false) {
    // 将配置结构体清零
    // 说明: memset 是安全的，DbConfig 是 POD 类型
    memset(&m_config, 0, sizeof(m_config));
    // 清零备份时间戳
    memset(m_lastBackupTime, 0, sizeof(m_lastBackupTime));
}

// =============================================================================
// 析构函数
// =============================================================================
// 说明:
//   对象销毁时调用 Shutdown() 释放资源。
//   私有析构函数，由单例模式管理生命周期。
// =============================================================================
DatabaseImpl::~DatabaseImpl() {
    Shutdown();
}

// =============================================================================
// Init - 初始化数据库模块
// =============================================================================
// 参数:
//   configPath - 配置文件路径（UTF-8 编码）
//
// 返回值:
//   true - 初始化成功
//   false - 初始化失败
//
// 功能说明:
//   1. 加锁保护（QMutexLocker）
//   2. 调用 LoadConfig 解析配置文件
//   3. 返回加载结果
//
// 线程安全:
//   使用 QMutexLocker 自动管理锁，异常安全。
//   即使 LoadConfig 抛出异常，锁也会自动释放。
// =============================================================================
bool DatabaseImpl::Init(const char* configPath) {
    // 使用 QMutexLocker 自动加锁，函数返回时自动解锁
    // 说明: RAII 模式，确保异常情况下也能正确释放锁
    QMutexLocker locker(&m_mutex);

    // 调用内部方法加载配置
    // 注意: LoadConfig 不加锁，由调用者负责加锁
    if (!LoadConfig(configPath)) {
        return false;
    }

    return true;
}

// =============================================================================
// LoadConfig - 加载配置文件
// =============================================================================
// 参数:
//   configPath - 配置文件路径
//
// 返回值:
//   true - 加载成功
//   false - 加载失败
//
// 功能说明:
//   读取 JSON 配置文件，解析以下字段：
//   - databaseType: 数据库类型（"mysql" 或 "sqlite"）
//   - mysql.host: MySQL 服务器地址
//   - mysql.port: MySQL 服务器端口
//   - mysql.service: MySQL 服务名称
//   - mysql.database: 数据库名称
//   - sqlitePath: SQLite 数据库文件路径
//
// 错误处理:
//   - 文件无法打开: 记录错误，返回 false
//   - JSON 解析失败: 记录错误，返回 false
//   - 字段缺失: 使用默认值
//
// 注意事项:
//   - 此方法不加锁，由调用者（Init）负责
//   - 配置值使用 UTF-8 编码存储
// =============================================================================
bool DatabaseImpl::LoadConfig(const char* configPath) {
    // 打开配置文件
    QFile file(QString::fromUtf8(configPath));
    if (!file.open(QIODevice::ReadOnly)) {
        LogError("LoadConfig", "无法打开配置文件");
        return false;
    }

    // 读取文件内容
    QByteArray data = file.readAll();
    file.close();

    // 解析 JSON 文档
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        LogError("LoadConfig", "JSON 解析错误");
        return false;
    }

    // 获取根对象
    QJsonObject root = doc.object();

    // -------------------------------------------------------------------------
    // 解析数据库类型
    // -------------------------------------------------------------------------
    // 字段: databaseType
    // 类型: 字符串（"mysql" 或 "sqlite"）
    // 默认: mysql
    // -------------------------------------------------------------------------
    QString dbTypeValue = root["databaseType"].toString("mysql");
    m_config.dbType = (dbTypeValue.toLower() == "sqlite") ?
        static_cast<int32_t>(DatabaseType::SQLite) :
        static_cast<int32_t>(DatabaseType::MySQL);
    m_config.version = 1;  // 配置版本号

    // -------------------------------------------------------------------------
    // 解析 MySQL 配置
    // -------------------------------------------------------------------------
    // 字段: mysql（对象）
    // 子字段: host, port, service, database
    // 默认值: 127.0.0.1:3308, MSCANDB, mscan
    // -------------------------------------------------------------------------
    QJsonObject mysql = root["mysql"].toObject();

    // MySQL 服务器地址
    strncpy(m_config.mysqlHost,
            mysql["host"].toString("127.0.0.1").toUtf8().constData(),
            127);

    // MySQL 服务器端口
    m_config.mysqlPort = mysql["port"].toInt(3308);

    // MySQL 服务名称（标识用，不影响连接）
    strncpy(m_config.mysqlService,
            mysql["service"].toString("MSCANDB").toUtf8().constData(),
            63);

    // MySQL 数据库名称
    strncpy(m_config.mysqlDatabase,
            mysql["database"].toString("mscan").toUtf8().constData(),
            63);

    // -------------------------------------------------------------------------
    // 解析 SQLite 配置
    // -------------------------------------------------------------------------
    // 字段: sqlitePath
    // 类型: 字符串（完整文件路径）
    // 说明: 如果数据库文件不存在，连接时会自动创建
    // -------------------------------------------------------------------------
    strncpy(m_config.sqlitePath,
            root["sqlitePath"].toString("").toUtf8().constData(),
            255);

    return true;
}

// =============================================================================
// Connect - 建立数据库连接
// =============================================================================
// 返回值:
//   true - 连接成功（或已连接）
//   false - 连接失败
//
// 功能说明:
//   根据配置的数据库类型，调用相应的连接方法。
//   已连接状态下直接返回 true，不会重复连接。
//
// 线程安全:
//   使用 QMutexLocker 加锁保护。
// =============================================================================
bool DatabaseImpl::Connect() {
    QMutexLocker locker(&m_mutex);

    // 已连接状态，直接返回成功
    if (m_connected) {
        return true;
    }

    // 根据数据库类型调用相应的连接方法
    if (m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)) {
        return ConnectMySQL();
    } else {
        return ConnectSQLite();
    }
}

// =============================================================================
// ConnectMySQL - 连接 MySQL 数据库
// =============================================================================
// 返回值:
//   true - 连接成功
//   false - 连接失败
//
// 功能说明:
//   1. 生成唯一连接名（使用 QUuid）
//   2. 创建 QMYSQL 数据库连接
//   3. 设置连接参数（主机、端口、数据库、用户名、密码）
//   4. 打开连接
//   5. 更新连接状态
//
// 连接参数:
//   - 主机: m_config.mysqlHost
//   - 端口: m_config.mysqlPort
//   - 数据库: m_config.mysqlDatabase
//   - 用户名: MYSQL_USER (admin)
//   - 密码: MYSQL_PASSWORD (123456)
//
// 错误处理:
//   连接失败时，使用 LogError 记录错误信息。
//   错误信息来自 QSqlDatabase::lastError()。
//
// 注意事项:
//   - 使用唯一连接名，避免与现有连接冲突
//   - MySQL 服务必须运行并监听配置的端口
//   - 用户名密码必须正确
// =============================================================================
bool DatabaseImpl::ConnectMySQL() {
    // 生成唯一连接名
    QString connectionName = QUuid::createUuid().toString();

    // 创建 QMYSQL 数据库连接
    m_db = QSqlDatabase::addDatabase("QMYSQL", connectionName);

    // 设置连接参数
    m_db.setHostName(QString::fromUtf8(m_config.mysqlHost));
    m_db.setPort(m_config.mysqlPort);
    m_db.setDatabaseName(QString::fromUtf8(m_config.mysqlDatabase));
    m_db.setUserName(QString::fromUtf8(MYSQL_USER));
    m_db.setPassword(QString::fromUtf8(MYSQL_PASSWORD));

    // 尝试打开连接
    if (!m_db.open()) {
        LogError("ConnectMySQL", m_db.lastError().text().toUtf8().constData());
        return false;
    }

    // 更新连接状态
    m_connected = true;
    LogError("ConnectMySQL", "MySQL 连接成功");
    return true;
}

// =============================================================================
// ConnectSQLite - 连接 SQLite 数据库
// =============================================================================
// 返回值:
//   true - 连接成功
//   false - 连接失败
//
// 功能说明:
//   创建 QSQLITE 数据库连接，指向指定的数据库文件。
//   如果文件不存在，SQLite 会自动创建。
//
// 连接参数:
//   - 数据库文件: m_config.sqlitePath
//
// 注意事项:
//   - SQLite 是文件型数据库，无需服务器
//   - 确保目标目录存在且有写权限
//   - 支持跨平台使用
// =============================================================================
bool DatabaseImpl::ConnectSQLite() {
    // 生成唯一连接名
    QString connectionName = QUuid::createUuid().toString();

    // 创建 QSQLITE 数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);

    // 设置数据库文件路径
    m_db.setDatabaseName(QString::fromUtf8(m_config.sqlitePath));

    // 尝试打开连接
    if (!m_db.open()) {
        LogError("ConnectSQLite", m_db.lastError().text().toUtf8().constData());
        return false;
    }

    // 更新连接状态
    m_connected = true;
    LogError("ConnectSQLite", "SQLite 连接成功");
    return true;
}

// =============================================================================
// Disconnect - 断开数据库连接
// =============================================================================
// 功能说明:
//   关闭当前数据库连接，释放相关资源。
//   设置连接状态为 false。
//
// 注意事项:
//   - 未连接状态下调用此方法安全（无操作）
//   - 如有未提交事务，会自动回滚
//   - 断开后可以重新连接
// =============================================================================
void DatabaseImpl::Disconnect() {
    QMutexLocker locker(&m_mutex);

    // 检查连接状态
    if (m_connected && m_db.isOpen()) {
        // 关闭数据库连接
        m_db.close();
        // 重置数据库对象
        m_db = QSqlDatabase();
        // 更新连接状态
        m_connected = false;
    }
}

// =============================================================================
// IsConnected - 检查连接状态
// =============================================================================
// 返回值:
//   true - 已连接且数据库打开
//   false - 未连接或数据库已关闭
// =============================================================================
bool DatabaseImpl::IsConnected() const {
    return m_connected && m_db.isOpen();
}

// =============================================================================
// GetDatabaseType - 获取数据库类型
// =============================================================================
// 返回值: DatabaseType 枚举值
// =============================================================================
DatabaseType DatabaseImpl::GetDatabaseType() const {
    return static_cast<DatabaseType>(m_config.dbType);
}

// =============================================================================
// Backup - 备份数据库
// =============================================================================
// 参数:
//   backupPath - 备份目标目录
//
// 返回值:
//   true - 备份成功
//   false - 备份失败
//
// 功能说明:
//   根据数据库类型调用相应的备份方法。
//   MySQL 使用 robocopy 复制数据目录。
//   SQLite 使用 QFile::copy 复制数据库文件。
// =============================================================================
bool DatabaseImpl::Backup(const char* backupPath) {
    QMutexLocker locker(&m_mutex);

    if (m_config.dbType == static_cast<int32_t>(DatabaseType::MySQL)) {
        return BackupMySQL(backupPath);
    } else {
        return BackupSQLite(backupPath);
    }
}

// =============================================================================
// BackupMySQL - 备份 MySQL 数据库
// =============================================================================
// 参数:
//   backupPath - 备份目标目录
//
// 返回值:
//   true - 备份成功
//   false - 备份失败
//
// 功能说明:
//   使用 Windows robocopy 命令复制 MySQL 数据目录。
//   备份目录名格式: yyyyMMddHHmmss-数据库名
//
// 实现步骤:
//   1. 生成时间戳
//   2. 构建源目录和目标目录路径
//   3. 创建目标目录（如果不存在）
//   4. 执行 robocopy 命令
//   5. 更新备份时间戳
//
// robocopy 参数:
//   /E    - 复制子目录（包括空目录）
//   /R:0  - 失败不重试
//   /W:0  - 等待时间为 0
//
// 注意事项:
//   - 仅支持 Windows 平台（使用 robocopy）
//   - 备份的是数据文件，不是 SQL 导出
//   - 大型数据库备份可能耗时较长
// =============================================================================
bool DatabaseImpl::BackupMySQL(const char* backupPath) {
    // 生成时间戳（格式：yyyyMMddHHmmss）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");

    // 构建源目录路径（MySQL 数据目录）
    // 说明: 这是硬编码的路径，生产环境应从配置读取
    QString sourceDir = QString("F:/MeyerScan/MyCaseManager/MySQL/data/mscan");

    // 构建目标目录路径
    QString targetDir = QString("%1/%2-mscan").arg(backupPath).arg(timestamp);

    // 创建目标目录
    QDir dir;
    if (!dir.mkpath(targetDir)) {
        LogError("BackupMySQL", "无法创建备份目录");
        return false;
    }

    // -------------------------------------------------------------------------
    // 执行 robocopy 命令
    // -------------------------------------------------------------------------
    // 说明:
    //   robocopy 是 Windows 内置的可靠文件复制工具。
    //   返回值说明:
    //     0-7: 成功（有文件被复制）
    //     8+:  错误
    // -------------------------------------------------------------------------
    QStringList args;
    args << sourceDir << targetDir << "/E" << "/R:0" << "/W:0";

    int ret = QProcess::execute("robocopy", args);
    if (ret > 7) {
        LogError("BackupMySQL", "robocopy 执行失败");
        return false;
    }

    // 记录备份时间
    strncpy(m_lastBackupTime, timestamp.toUtf8().constData(), 31);
    LogError("BackupMySQL", "MySQL 备份完成");
    return true;
}

// =============================================================================
// BackupSQLite - 备份 SQLite 数据库
// =============================================================================
// 参数:
//   backupPath - 备份目标目录
//
// 返回值:
//   true - 备份成功
//   false - 备份失败
//
// 功能说明:
//   使用 QFile::copy 复制 SQLite 数据库文件。
//   备份文件名格式: yyyyMMddHHmmss-MeyerScanSQLite.db
//
// 注意事项:
//   - 建议在数据库关闭状态下备份
//   - 如果数据库正在写入，复制可能失败或数据不一致
// =============================================================================
bool DatabaseImpl::BackupSQLite(const char* backupPath) {
    // 生成时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");

    // 源文件路径
    QString sourceFile = QString::fromUtf8(m_config.sqlitePath);

    // 目标文件路径
    QString targetFile = QString("%1/%2-MeyerScanSQLite.db")
        .arg(backupPath).arg(timestamp);

    // 复制文件
    if (!QFile::copy(sourceFile, targetFile)) {
        LogError("BackupSQLite", "文件复制失败");
        return false;
    }

    // 记录备份时间
    strncpy(m_lastBackupTime, timestamp.toUtf8().constData(), 31);
    LogError("BackupSQLite", "SQLite 备份完成");
    return true;
}

// =============================================================================
// GetLastBackupTime - 获取上次备份时间
// =============================================================================
// 返回值:
//   上次备份时间戳字符串（格式：yyyyMMddHHmmss）
//   从未备份则返回空字符串
// =============================================================================
const char* DatabaseImpl::GetLastBackupTime() const {
    return m_lastBackupTime;
}

// =============================================================================
// ExecuteQuery - 执行查询语句
// =============================================================================
// 参数:
//   sql - SQL 查询语句
//
// 返回值:
//   DbResult 结构体，包含执行结果和状态
//
// 功能说明:
//   执行 SELECT 类型的 SQL 查询。
//   当前版本仅返回执行状态，不返回结果集数据。
//
// 注意事项:
//   - 必须先连接数据库
//   - SQL 语句必须有效
//   - 如需获取查询结果数据，需要扩展 DbResult 结构体
// =============================================================================
DbResult DatabaseImpl::ExecuteQuery(const char* sql) {
    QMutexLocker locker(&m_mutex);

    // 初始化结果结构体
    DbResult result;
    result.version = 1;
    result.success = 0;
    result.affectedRows = 0;
    memset(result.errorMessage, 0, sizeof(result.errorMessage));

    // 检查连接状态
    if (!IsConnected()) {
        strncpy(result.errorMessage, "数据库未连接", 255);
        LogError("ExecuteQuery", "数据库未连接");
        return result;
    }

    // 执行查询
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromUtf8(sql))) {
        strncpy(result.errorMessage,
                query.lastError().text().toUtf8().constData(),
                255);
        LogError("ExecuteQuery", result.errorMessage);
        return result;
    }

    // 标记成功
    result.success = 1;
    return result;
}

// =============================================================================
// ExecuteUpdate - 执行更新语句
// =============================================================================
// 参数:
//   sql - SQL 更新语句（INSERT/UPDATE/DELETE）
//
// 返回值:
//   DbResult 结构体，包含执行结果、影响行数和状态
//
// 功能说明:
//   执行数据修改语句，返回受影响的行数。
// =============================================================================
DbResult DatabaseImpl::ExecuteUpdate(const char* sql) {
    QMutexLocker locker(&m_mutex);

    // 初始化结果结构体
    DbResult result;
    result.version = 1;
    result.success = 0;
    result.affectedRows = 0;
    memset(result.errorMessage, 0, sizeof(result.errorMessage));

    // 检查连接状态
    if (!IsConnected()) {
        strncpy(result.errorMessage, "数据库未连接", 255);
        LogError("ExecuteUpdate", "数据库未连接");
        return result;
    }

    // 执行更新
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromUtf8(sql))) {
        strncpy(result.errorMessage,
                query.lastError().text().toUtf8().constData(),
                255);
        LogError("ExecuteUpdate", result.errorMessage);
        return result;
    }

    // 标记成功，记录影响行数
    result.success = 1;
    result.affectedRows = query.numRowsAffected();
    return result;
}

// =============================================================================
// ExecuteScript - 批量执行 SQL 脚本
// =============================================================================
// 参数:
//   sqlScripts - SQL 语句数组
//   count      - SQL 语句数量
//
// 返回值:
//   成功执行的 SQL 语句数量
//
// 功能说明:
//   依次执行多条 SQL 语句，记录成功数量。
//   某条失败不会中断后续语句的执行。
//
// 注意事项:
//   - 不保证原子性，部分成功的情况需要手动处理
//   - 如需原子性，请使用事务
// =============================================================================
int32_t DatabaseImpl::ExecuteScript(const char** sqlScripts, int32_t count) {
    int32_t successCount = 0;

    // 依次执行每条 SQL
    for (int32_t i = 0; i < count; ++i) {
        DbResult result = ExecuteUpdate(sqlScripts[i]);
        if (result.success) {
            ++successCount;
        }
    }

    return successCount;
}

// =============================================================================
// BeginTransaction - 开始事务
// =============================================================================
// 返回值:
//   true - 事务开始成功
//   false - 事务开始失败
// =============================================================================
bool DatabaseImpl::BeginTransaction() {
    QMutexLocker locker(&m_mutex);
    return m_db.transaction();
}

// =============================================================================
// Commit - 提交事务
// =============================================================================
// 返回值:
//   true - 提交成功
//   false - 提交失败
// =============================================================================
bool DatabaseImpl::Commit() {
    QMutexLocker locker(&m_mutex);
    return m_db.commit();
}

// =============================================================================
// Rollback - 回滚事务
// =============================================================================
// 返回值:
//   true - 回滚成功
//   false - 回滚失败
// =============================================================================
bool DatabaseImpl::Rollback() {
    QMutexLocker locker(&m_mutex);
    return m_db.rollback();
}

// =============================================================================
// GetConfig - 获取当前配置
// =============================================================================
// 返回值:
//   DbConfig 结构体的副本
// =============================================================================
DbConfig DatabaseImpl::GetConfig() const {
    return m_config;
}

// =============================================================================
// SetDatabaseType - 切换数据库类型
// =============================================================================
// 参数:
//   dbType - 目标数据库类型
//
// 返回值:
//   true - 切换成功
//   false - 切换失败
//
// 功能说明:
//   1. 断开当前连接
//   2. 更新数据库类型配置
//   3. 尝试连接新类型的数据库
//
// 注意事项:
//   - 切换会断开当前连接
//   - 确保配置文件中包含目标类型的配置
// =============================================================================
bool DatabaseImpl::SetDatabaseType(DatabaseType dbType) {
    QMutexLocker locker(&m_mutex);

    // 断开当前连接
    Disconnect();

    // 更新数据库类型
    m_config.dbType = static_cast<int32_t>(dbType);

    // 释放锁后重新连接
    locker.unlock();
    return Connect();
}

// =============================================================================
// GetModuleVersion - 获取模块版本号
// =============================================================================
// 返回值:
//   版本字符串指针
// =============================================================================
const char* DatabaseImpl::GetModuleVersion() const {
    return "MeyerScan_Database v1.0.0";
}

// =============================================================================
// Shutdown - 关闭数据库模块
// =============================================================================
// 功能说明:
//   关闭数据库模块，释放所有资源。
//   主要操作是断开数据库连接。
// =============================================================================
void DatabaseImpl::Shutdown() {
    Disconnect();
}

// =============================================================================
// LogError - 输出错误日志
// =============================================================================
// 参数:
//   operation - 操作名称
//   message   - 日志消息
//
// 功能说明:
//   统一的日志输出函数，格式化为 "[Database] 操作名: 消息内容"。
//   使用 qDebug() 输出到调试控制台。
//
// 设计说明:
//   当前使用 qDebug() 输出，后续可以替换为 MeyerScan_Logger 的日志接口，
//   只需修改此函数的实现即可，不影响其他代码。
// =============================================================================
void DatabaseImpl::LogError(const char* operation, const char* message) {
    QString logMsg = QString("[Database] %1: %2").arg(operation).arg(message);
    qDebug(logMsg.toUtf8().constData());
}
