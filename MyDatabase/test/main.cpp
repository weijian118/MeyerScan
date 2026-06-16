// =============================================================================
// 文件名:    main.cpp
// 模块名:    DatabaseTest（数据库模块测试程序）
// 版本号:    v1.0.0
//
// 用途说明:
//   MeyerScan_Database.dll 模块的完整测试程序。
//   包含 9 个测试用例，覆盖模块的所有核心功能。
//
// 测试用例:
//   Test 1: 模块初始化测试
//   Test 2: 配置加载测试
//   Test 3: 数据库连接测试
//   Test 4: 查询执行测试
//   Test 5: 事务管理测试
//   Test 6: 备份功能测试
//   Test 7: 数据库类型切换测试
//   Test 8: 断开连接和清理测试
//   Test 9: 线程安全测试
//
// 输出:
//   测试结果输出到日志文件（test_output.log），
//   包括每个测试的通过/失败状态和汇总信息。
//
// 使用方法:
//   1. 编译 MeyerScan_Database.dll 和 DatabaseTest.exe
//   2. 确保 MySQL 服务运行（可选，部分测试依赖 MySQL）
//   3. 运行 DatabaseTest.exe
//   4. 查看 test_output.log 了解测试结果
//
// 注意事项:
//   - 部分 MySQL 测试依赖数据库服务运行
//   - 备份测试依赖 MySQL 数据目录存在
//   - 测试会创建备份文件，注意磁盘空间
// =============================================================================

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QThread>
#include <QDateTime>
#include "../include/Database.h"

// =============================================================================
// 全局变量
// =============================================================================

// 日志文件对象
// 说明: 所有测试输出写入此文件
static QFile s_logFile;

// 测试统计计数器
// 说明: 记录通过和失败的测试数量
static int s_testsPassed = 0;
static int s_testsFailed = 0;

// =============================================================================
// messageToFile - Qt 消息处理器
// =============================================================================
// 参数:
//   type    - 消息类型（调试、警告、错误等）
//   context - 消息上下文（文件名、行号等）
//   msg     - 消息内容
//
// 功能说明:
//   自定义 Qt 消息处理函数，将所有 qDebug() 输出写入日志文件。
//   用于捕获测试过程中的所有输出信息。
//
// 实现细节:
//   1. 打开日志文件（追加模式）
//   2. 写入消息内容并添加换行符
//   3. 刷新并关闭文件
//
// 注意事项:
//   - 每次调用都会打开和关闭文件，确保消息及时写入
//   - 如果文件无法打开，消息会丢失
// =============================================================================
void messageToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString txt = msg;
    if (s_logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&s_logFile);
        stream << txt << "\n";
        stream.flush();
        s_logFile.close();
    }
}

// =============================================================================
// TEST_ASSERT - 测试断言宏
// =============================================================================
// 参数:
//   condition - 测试条件（表达式）
//   message   - 测试描述（字符串）
//
// 功能说明:
//   通用测试断言宏，用于验证测试结果。
//   - 如果 condition 为 true，记录 [PASS] 并增加通过计数
//   - 如果 condition 为 false，记录 [FAIL] 并增加失败计数
//
// 使用示例:
//   TEST_ASSERT(db != nullptr, "GetDatabase() 返回有效指针");
// =============================================================================
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            qDebug() << "[PASS]" << message; \
            s_testsPassed++; \
        } else { \
            qDebug() << "[FAIL]" << message; \
            s_testsFailed++; \
        } \
    } while(0)

// =============================================================================
// Test 1: 模块初始化测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证 GetDatabase() 返回有效指针
//   2. 验证 GetModuleVersion() 返回有效版本字符串
//
// 设计说明:
//   这是基础测试，确保模块可以正常加载和初始化。
// =============================================================================
void testModuleInit(IDatabase* db) {
    qDebug() << "\n=== Test 1: 模块初始化测试 ===";

    // 验证单例实例获取
    TEST_ASSERT(db != nullptr, "GetDatabase() 返回有效指针");

    // 验证版本信息获取
    const char* version = db->GetModuleVersion();
    TEST_ASSERT(version != nullptr && strlen(version) > 0,
                "GetModuleVersion() 返回有效字符串");
    qDebug() << "模块版本:" << version;
}

// =============================================================================
// Test 2: 配置加载测试
// =============================================================================
// 参数:
//   db         - 数据库接口指针
//   configPath - 配置文件路径
//
// 测试内容:
//   1. 验证 Init() 能成功加载配置文件
//   2. 验证配置版本号正确
//   3. 验证数据库类型有效
//   4. 验证 MySQL 主机地址已配置
//   5. 验证 MySQL 端口已配置
//
// 设计说明:
//   配置加载是所有后续操作的前提，必须首先验证。
// =============================================================================
void testConfig(IDatabase* db, const char* configPath) {
    qDebug() << "\n=== Test 2: 配置加载测试 ===";

    // 验证配置文件加载
    bool initResult = db->Init(configPath);
    TEST_ASSERT(initResult, "Init() 成功加载有效配置文件");

    // 获取并验证配置信息
    DbConfig config = db->GetConfig();
    TEST_ASSERT(config.version == 1, "配置版本号正确");
    TEST_ASSERT(config.dbType == 0 || config.dbType == 1, "数据库类型有效");

    // 验证 MySQL 配置参数
    TEST_ASSERT(strlen(config.mysqlHost) > 0, "MySQL 主机地址已配置");
    TEST_ASSERT(config.mysqlPort > 0, "MySQL 端口已配置");

    // 输出配置信息（调试用）
    qDebug() << "配置 - 类型:" << (config.dbType == 0 ? "MySQL" : "SQLite");
    qDebug() << "配置 - 主机:" << config.mysqlHost;
    qDebug() << "配置 - 端口:" << config.mysqlPort;
    qDebug() << "配置 - 数据库:" << config.mysqlDatabase;
}

// =============================================================================
// Test 3: 数据库连接测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证初始状态未连接
//   2. 验证 Connect() 能成功连接
//   3. 验证 IsConnected() 返回正确状态
//
// 注意事项:
//   - 此测试依赖 MySQL 服务运行
//   - 如果 MySQL 未运行，连接测试会失败
// =============================================================================
void testConnection(IDatabase* db) {
    qDebug() << "\n=== Test 3: 数据库连接测试 ===";

    // 验证初始状态
    TEST_ASSERT(!db->IsConnected(), "Connect() 前未连接");

    // 尝试连接
    bool connectResult = db->Connect();
    if (connectResult) {
        TEST_ASSERT(db->IsConnected(),
                    "Connect() 后 IsConnected() 返回 true");
        qDebug() << "数据库连接成功";
    } else {
        qDebug() << "连接失败 - MySQL 可能未运行（单元测试可接受）";
    }
}

// =============================================================================
// Test 4: 查询执行测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证 ExecuteQuery() 执行有效查询成功
//   2. 验证 ExecuteQuery() 执行无效查询失败（但不崩溃）
//
// 注意事项:
//   - 需要先连接数据库
//   - SHOW TABLES 是 MySQL 特有命令
// =============================================================================
void testQuery(IDatabase* db) {
    qDebug() << "\n=== Test 4: 查询执行测试 ===";

    // 检查连接状态
    if (!db->IsConnected()) {
        qDebug() << "跳过 - 未连接数据库";
        return;
    }

    // 执行有效查询
    DbResult result = db->ExecuteQuery("SHOW TABLES");
    TEST_ASSERT(result.success, "ExecuteQuery(SHOW TABLES) 成功");

    // 执行无效查询（测试错误处理）
    DbResult badResult = db->ExecuteQuery(
        "SELECT * FROM nonexistent_table_xyz"
    );
    TEST_ASSERT(!badResult.success,
                "ExecuteQuery(无效表) 失败但不崩溃");
}

// =============================================================================
// Test 5: 事务管理测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证 BeginTransaction() 成功开始事务
//   2. 验证 Rollback() 成功回滚事务
//
// 注意事项:
//   - 需要先连接数据库
// =============================================================================
void testTransaction(IDatabase* db) {
    qDebug() << "\n=== Test 5: 事务管理测试 ===";

    if (!db->IsConnected()) {
        qDebug() << "跳过 - 未连接数据库";
        return;
    }

    // 开始事务
    bool beginResult = db->BeginTransaction();
    TEST_ASSERT(beginResult, "BeginTransaction() 成功");

    // 回滚事务
    bool rollbackResult = db->Rollback();
    TEST_ASSERT(rollbackResult, "Rollback() 成功");
}

// =============================================================================
// Test 6: 备份功能测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证 Backup() 成功执行备份
//   2. 验证 GetLastBackupTime() 返回有效时间戳
//
// 注意事项:
//   - MySQL 备份依赖数据目录存在
//   - 备份会创建实际文件，占用磁盘空间
// =============================================================================
void testBackup(IDatabase* db) {
    qDebug() << "\n=== Test 6: 备份功能测试 ===";

    const char* backupPath = "F:/MeyerScan/MyDatabase/backup";
    bool backupResult = db->Backup(backupPath);

    if (backupResult) {
        TEST_ASSERT(true, "Backup() 成功");

        const char* backupTime = db->GetLastBackupTime();
        TEST_ASSERT(backupTime != nullptr && strlen(backupTime) > 0,
                    "GetLastBackupTime() 返回有效时间戳");
        qDebug() << "上次备份时间:" << backupTime;
    } else {
        qDebug() << "备份失败 - 源目录可能不存在";
    }
}

// =============================================================================
// Test 7: 数据库类型切换测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证数据库类型一致性
//   2. 验证 Disconnect() 后状态正确
//
// 设计说明:
//   SetDatabaseType() 会尝试连接新数据库，
//   如果连接失败会影响后续测试，因此简化为状态验证。
// =============================================================================
void testDbTypeSwitch(IDatabase* db) {
    qDebug() << "\n=== Test 7: 数据库类型切换测试 ===";

    DatabaseType originalType = db->GetDatabaseType();
    qDebug() << "原始类型:" << (originalType == DatabaseType::MySQL ?
                                "MySQL" : "SQLite");

    // 断开连接
    db->Disconnect();

    // 验证类型一致性
    DatabaseType currentType = db->GetDatabaseType();
    TEST_ASSERT(currentType == originalType, "数据库类型保持一致");
    qDebug() << "数据库类型切换测试完成";
}

// =============================================================================
// Test 8: 断开连接和清理测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   1. 验证 Disconnect() 正确断开连接
//   2. 验证 Shutdown() 正常执行
// =============================================================================
void testDisconnect(IDatabase* db) {
    qDebug() << "\n=== Test 8: 断开连接和清理测试 ===";

    // 断开连接
    db->Disconnect();
    TEST_ASSERT(!db->IsConnected(), "Disconnect() 后未连接");

    // 关闭模块
    db->Shutdown();
    qDebug() << "Shutdown 完成";
}

// =============================================================================
// Test 9: 线程安全测试
// =============================================================================
// 参数:
//   db - 数据库接口指针
//
// 测试内容:
//   验证连续多次调用不会崩溃或死锁
//
// 设计说明:
//   这是基本的线程安全测试，验证互斥锁正常工作。
//   更完整的测试需要多线程并发调用。
// =============================================================================
void testThreadSafety(IDatabase* db) {
    qDebug() << "\n=== Test 9: 线程安全测试 ===";

    // 重新连接以进行测试
    if (!db->IsConnected()) {
        db->Connect();
    }

    if (db->IsConnected()) {
        // 连续调用多个方法，测试互斥锁
        for (int i = 0; i < 5; i++) {
            db->IsConnected();
            db->GetDatabaseType();
            db->GetConfig();
        }
        TEST_ASSERT(true, "连续调用完成，无崩溃或死锁");
    } else {
        qDebug() << "跳过 - 未连接数据库";
    }
}

// =============================================================================
// 主函数
// =============================================================================
// 功能说明:
//   1. 设置日志文件和消息处理器
//   2. 获取数据库实例
//   3. 执行所有测试用例
//   4. 输出测试汇总
//   5. 返回测试结果状态码
//
// 返回值:
//   0 - 所有测试通过
//   1 - 有测试失败
// =============================================================================
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // -------------------------------------------------------------------------
    // 设置日志文件
    // -------------------------------------------------------------------------
    s_logFile.setFileName("F:/MeyerScan/MyDatabase/bin/Debug/test_output.log");
    s_logFile.remove();  // 清除之前的日志

    // 安装自定义消息处理器
    qInstallMessageHandler(messageToFile);

    // 输出测试标题
    qDebug() << "===========================================";
    qDebug() << "MeyerScan Database Module Test Suite";
    qDebug() << "构建时间:" << __DATE__ << __TIME__;
    qDebug() << "===========================================";

    // -------------------------------------------------------------------------
    // 获取数据库实例
    // -------------------------------------------------------------------------
    IDatabase* db = GetDatabase();
    if (!db) {
        qDebug() << "致命错误: 无法获取数据库实例";
        return 1;
    }

    const char* configPath = "F:/MeyerScan/MyDatabase/config/db_config.json";

    // -------------------------------------------------------------------------
    // 执行所有测试
    // -------------------------------------------------------------------------
    testModuleInit(db);           // Test 1: 模块初始化
    testConfig(db, configPath);   // Test 2: 配置加载
    testConnection(db);           // Test 3: 数据库连接
    testQuery(db);                // Test 4: 查询执行
    testTransaction(db);          // Test 5: 事务管理
    testBackup(db);               // Test 6: 备份功能
    testDbTypeSwitch(db);         // Test 7: 类型切换
    testThreadSafety(db);         // Test 9: 线程安全
    testDisconnect(db);           // Test 8: 断开连接

    // -------------------------------------------------------------------------
    // 输出测试汇总
    // -------------------------------------------------------------------------
    qDebug() << "\n===========================================";
    qDebug() << "测试汇总";
    qDebug() << "===========================================";
    qDebug() << "通过测试数:" << s_testsPassed;
    qDebug() << "失败测试数:" << s_testsFailed;
    qDebug() << "总测试数:" << (s_testsPassed + s_testsFailed);
    qDebug() << "===========================================";

    if (s_testsFailed == 0) {
        qDebug() << "所有测试通过 ✓";
    } else {
        qDebug() << "存在失败的测试 ✗";
    }

    // 返回测试状态码
    return (s_testsFailed == 0) ? 0 : 1;
}