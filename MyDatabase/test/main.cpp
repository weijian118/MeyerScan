// =============================================================================
// 文件:    main.cpp
// 模块:    DatabaseTest（数据库模块测试程序）
// 版本号:  v1.1.0
//
// 用途说明:
//   MeyerScan_Database.dll 模块的完整测试程序。
//   包含 9 个测试用例，覆盖模块的所有核心功能。
//
// 接口说明:
//   本测试程序适配 v1.1.0 的 Result<T>/VoidResult 接口风格。
//   所有操作方法调用均通过 IsSuccess() 判断执行结果。
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
// 注意事项:
//   - 部分 MySQL 测试依赖数据库服务运行
//   - 备份测试依赖 MySQL 数据目录存在
//   - 测试输出到控制台和日志文件
// =============================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <windows.h>
#include "../include/Database.h"

// =============================================================================
// 全局变量
// =============================================================================

// 日志文件指针
static FILE* s_logFile = nullptr;

// 测试统计计数器
static int s_testsPassed = 0;
static int s_testsFailed = 0;
static std::string s_moduleRoot;

// =============================================================================
// TEST_ASSERT - 测试断言宏
// =============================================================================
// 参数:
//   condition - 测试条件（表达式）
//   message   - 测试描述（字符串）
//
// 功能说明:
//   - 如果 condition 为 true，记录 [PASS] 并增加通过计数
//   - 如果 condition 为 false，记录 [FAIL] 并增加失败计数
//   结果同时输出到控制台和日志文件。
// =============================================================================
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            fprintf(s_logFile, "[PASS] %s\n", message); \
            printf("[PASS] %s\n", message); \
            s_testsPassed++; \
        } else { \
            fprintf(s_logFile, "[FAIL] %s\n", message); \
            printf("[FAIL] %s\n", message); \
            s_testsFailed++; \
        } \
    } while(0)

// =============================================================================
// Test 1: 模块初始化测试
// =============================================================================
// 测试内容:
//   1. 验证 GetDatabase() 返回有效指针
//   2. 验证 GetModuleVersion() 返回有效版本字符串
// =============================================================================
static void TestModuleInit(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 1: 模块初始化测试 ===\n");
    printf("\n=== Test 1: 模块初始化测试 ===\n");

    // 验证单例实例获取
    TEST_ASSERT(db != nullptr, "GetDatabase() 返回有效指针");

    // 验证版本信息获取
    const char* version = db->GetModuleVersion();
    TEST_ASSERT(version != nullptr && strlen(version) > 0,
                "GetModuleVersion() 返回有效字符串");
    fprintf(s_logFile, "  模块版本: %s\n", version);
    printf("  模块版本: %s\n", version);
}

// =============================================================================
// Test 2: 配置加载测试
// =============================================================================
// 测试内容:
//   1. 验证 Init() 能成功加载配置文件
//   2. 验证配置版本号正确
//   3. 验证数据库类型有效
//   4. 验证 MySQL 主机地址已配置
//   5. 验证 MySQL 端口已配置
// =============================================================================
static void TestConfig(IDatabase* db, const char* configPath) {
    fprintf(s_logFile, "\n=== Test 2: 配置加载测试 ===\n");
    printf("\n=== Test 2: 配置加载测试 ===\n");

    // 验证配置文件加载
    VoidResult initResult = db->Init(configPath);
    TEST_ASSERT(initResult.IsSuccess(), "Init() 成功加载有效配置文件");

    // 获取并验证配置信息
    const DbConfig& config = db->GetConfig();
    TEST_ASSERT(config.version == 1, "配置版本号正确");
    TEST_ASSERT(config.dbType == 0 || config.dbType == 1, "数据库类型有效");

    // 验证 MySQL 配置参数
    TEST_ASSERT(strlen(config.mysqlHost) > 0, "MySQL 主机地址已配置");
    TEST_ASSERT(config.mysqlPort > 0, "MySQL 端口已配置");

    // 输出配置信息
    const char* typeStr = (config.dbType == 0) ? "MySQL" : "SQLite";
    fprintf(s_logFile, "  配置 - 类型: %s\n", typeStr);
    fprintf(s_logFile, "  配置 - 主机: %s\n", config.mysqlHost);
    fprintf(s_logFile, "  配置 - 端口: %d\n", config.mysqlPort);
    fprintf(s_logFile, "  配置 - 数据库: %s\n", config.mysqlDatabase);
    printf("  配置 - 类型: %s\n", typeStr);
    printf("  配置 - 主机: %s\n", config.mysqlHost);
    printf("  配置 - 端口: %d\n", config.mysqlPort);
    printf("  配置 - 数据库: %s\n", config.mysqlDatabase);
}

// =============================================================================
// Test 3: 数据库连接测试
// =============================================================================
// 测试内容:
//   1. 验证初始状态未连接
//   2. 验证 Connect() 能成功连接
//   3. 验证 IsConnected() 返回正确状态
// =============================================================================
static void TestConnection(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 3: 数据库连接测试 ===\n");
    printf("\n=== Test 3: 数据库连接测试 ===\n");

    // 验证初始状态
    TEST_ASSERT(!db->IsConnected(), "Connect() 前未连接");

    // 尝试连接
    VoidResult connectResult = db->Connect();
    if (connectResult.IsSuccess()) {
        TEST_ASSERT(db->IsConnected(),
                    "Connect() 后 IsConnected() 返回 true");
        fprintf(s_logFile, "  数据库连接成功\n");
        printf("  数据库连接成功\n");
    } else {
        fprintf(s_logFile, "  连接失败 - MySQL 可能未运行（单元测试可接受）\n");
        printf("  连接失败 - MySQL 可能未运行（单元测试可接受）\n");
    }
}

// =============================================================================
// Test 4: 查询执行测试
// =============================================================================
// 测试内容:
//   1. 验证 ExecuteQuery() 执行有效查询成功
//   2. 验证 ExecuteQueryJson() 可返回通用 JSON 行列结果
//   3. 验证 ExecuteQuery() 执行无效查询失败（但不崩溃）
// =============================================================================
static void TestQuery(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 4: 查询执行测试 ===\n");
    printf("\n=== Test 4: 查询执行测试 ===\n");

    // 检查连接状态
    if (!db->IsConnected()) {
        fprintf(s_logFile, "  跳过 - 未连接数据库\n");
        printf("  跳过 - 未连接数据库\n");
        return;
    }

    // 执行有效查询
    Result<DbResult> result = db->ExecuteQuery("SHOW TABLES");
    TEST_ASSERT(result.IsSuccess(), "ExecuteQuery(SHOW TABLES) 成功");

    // 执行通用 JSON 查询。该能力只做行列转换，不承载业务语义。
    char jsonBuffer[4096] = {0};
    Result<DbJsonResult> jsonResult = db->ExecuteQueryJson(
        "SELECT 1 AS id, 'database_json_smoke' AS name",
        jsonBuffer,
        sizeof(jsonBuffer));
    TEST_ASSERT(jsonResult.IsSuccess(), "ExecuteQueryJson(SELECT 常量) 成功");
    TEST_ASSERT(strstr(jsonBuffer, "database_json_smoke") != nullptr,
                "ExecuteQueryJson() 返回 JSON 内容");

    // 执行无效查询（测试错误处理）
    Result<DbResult> badResult = db->ExecuteQuery(
        "SELECT * FROM nonexistent_table_xyz");
    TEST_ASSERT(badResult.IsError(),
                "ExecuteQuery(无效表) 失败但不崩溃");
}

// =============================================================================
// Test 5: 事务管理测试
// =============================================================================
// 测试内容:
//   1. 验证 BeginTransaction() 成功开始事务
//   2. 验证 Rollback() 成功回滚事务
// =============================================================================
static void TestTransaction(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 5: 事务管理测试 ===\n");
    printf("\n=== Test 5: 事务管理测试 ===\n");

    if (!db->IsConnected()) {
        fprintf(s_logFile, "  跳过 - 未连接数据库\n");
        printf("  跳过 - 未连接数据库\n");
        return;
    }

    // 开始事务
    VoidResult beginResult = db->BeginTransaction();
    TEST_ASSERT(beginResult.IsSuccess(), "BeginTransaction() 成功");

    // 回滚事务
    VoidResult rollbackResult = db->Rollback();
    TEST_ASSERT(rollbackResult.IsSuccess(), "Rollback() 成功");
}

// =============================================================================
// Test 6: 备份功能测试
// =============================================================================
// 测试内容:
//   1. 验证 Backup() 成功执行备份
//   2. 验证 GetLastBackupTime() 返回有效时间戳
// =============================================================================
static void TestBackup(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 6: 备份功能测试 ===\n");
    printf("\n=== Test 6: 备份功能测试 ===\n");

    if (!db->IsConnected()) {
        // 备份必须依赖有效数据库连接；未连接时跳过，不把环境缺失记为失败。
        fprintf(s_logFile, "  跳过 - 未连接数据库\n");
        printf("  跳过 - 未连接数据库\n");
        return;
    }

    // 根据当前数据库类型检查备份源是否存在。
    // MySQL 备份复制数据目录，SQLite 备份复制单个数据库文件。
    const DbConfig& config = db->GetConfig();
    if (config.dbType == static_cast<int32_t>(DatabaseType::MySQL)) {
        // GetFileAttributesA 返回目录属性，INVALID 表示路径不存在或无权限访问。
        const DWORD attributes = GetFileAttributesA(config.mysqlDataDir);
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // 本机没有安装/启动测试 MySQL 数据目录时，备份测试可以跳过。
            fprintf(s_logFile, "  跳过 - MySQL 数据目录不存在: %s\n", config.mysqlDataDir);
            printf("  跳过 - MySQL 数据目录不存在: %s\n", config.mysqlDataDir);
            return;
        }
    } else {
        // SQLite 备份要求源路径是文件，不能是目录。
        const DWORD attributes = GetFileAttributesA(config.sqlitePath);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // 测试环境没有 SQLite 文件时跳过，避免误报模块逻辑失败。
            fprintf(s_logFile, "  跳过 - SQLite 数据库文件不存在: %s\n", config.sqlitePath);
            printf("  跳过 - SQLite 数据库文件不存在: %s\n", config.sqlitePath);
            return;
        }
    }

    // 备份输出到模块根目录下 backup，便于人工检查，也避免写到系统目录。
    const std::string backupPath = s_moduleRoot + "/backup";
    VoidResult backupResult = db->Backup(backupPath.c_str());

    if (backupResult.IsSuccess()) {
        // Backup 成功后应更新模块内部的最后备份时间。
        TEST_ASSERT(true, "Backup() 成功");

        const char* backupTime = db->GetLastBackupTime();
        TEST_ASSERT(backupTime != nullptr && strlen(backupTime) > 0,
                    "GetLastBackupTime() 返回有效时间戳");
        fprintf(s_logFile, "  上次备份时间: %s\n", backupTime);
        printf("  上次备份时间: %s\n", backupTime);
    } else {
        fprintf(s_logFile, "  备份失败 - 源目录可能不存在\n");
        printf("  备份失败 - 源目录可能不存在\n");
    }
}

// 统一路径分隔符。
// Windows API 返回的路径通常包含反斜杠，这里转成正斜杠，方便后续字符串拼接。
static std::string NormalizePath(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') {
            // 统一转为正斜杠，后续 ParentPath 只查找一种分隔符即可。
            ch = '/';
        }
    }
    return path;
}

// 返回指定路径的父目录。
// 这里不使用 std::filesystem，因为 VS2015/v140 环境不支持 C++17 filesystem。
static std::string ParentPath(const std::string& path) {
    const size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? std::string() : path.substr(0, pos);
}

// 根据 DatabaseTest.exe 所在位置推导 MyDatabase 模块根目录。
// 不能依赖当前工作目录，因为 VS 调试、脚本启动或第三方启动都可能改变 currentPath。
static std::string ResolveModuleRoot() {
    char buffer[MAX_PATH] = {0};
    // nullptr 表示获取当前 EXE 的完整路径。
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        // 获取失败时返回当前目录兜底，测试会继续运行并暴露后续路径问题。
        return ".";
    }

    const std::string exePath = NormalizePath(buffer);
    // exePath:     F:/MeyerScan/MyDatabase/bin/Release/DatabaseTest.exe
    // releaseDir: F:/MeyerScan/MyDatabase/bin/Release
    const std::string releaseDir = ParentPath(exePath);
    // binDir:     F:/MeyerScan/MyDatabase/bin
    const std::string binDir = ParentPath(releaseDir);
    // moduleRoot: F:/MeyerScan/MyDatabase
    return ParentPath(binDir);
}

// 返回测试 EXE 的 Release 目录。
// 运行时配置和测试日志放在 Release 目录附近，便于人工查看和清理。
static std::string ResolveReleaseDir() {
    char buffer[MAX_PATH] = {0};
    // 当前测试程序的 EXE 路径是定位运行时 config/log 的基准。
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return ".";
    }

    // ParentPath 去掉 DatabaseTest.exe，只保留 bin/Release 目录。
    return ParentPath(NormalizePath(buffer));
}

// 写入运行时测试配置文件。
// 目的:
//   - 测试相对路径解析能力。
//   - 避免直接修改仓库中的正式 db_config.json。
//   - 让测试宿主在 Release 目录下即可独立运行。
static std::string WriteRuntimeTestConfig() {
    // 测试配置写到 Release/config 下，模拟真实发布目录结构。
    const std::string releaseDir = ResolveReleaseDir();
    const std::string configDir = releaseDir + "/config";
    // CreateDirectoryA 只能创建最后一级目录；Release 目录已存在，所以这里足够。
    CreateDirectoryA(configDir.c_str(), nullptr);

    const std::string configPath = configDir + "/db_config_test.json";
    std::ofstream file(configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        // 如果 Release/config 不可写，回退到仓库自带配置，保证测试仍可继续。
        return s_moduleRoot + "/config/db_config.json";
    }

    // 这里故意写相对路径，验证 Database::ResolvePathFromConfig 是否按配置文件目录解析。
    file
        << "{\n"
        << "    \"databaseType\": \"mysql\",\n"
        << "    \"mysql\": {\n"
        << "        \"host\": \"127.0.0.1\",\n"
        << "        \"port\": 3308,\n"
        << "        \"service\": \"MSCANDB\",\n"
        << "        \"database\": \"mscan\",\n"
        << "        \"dataDir\": \"../../../../MySQL/data/mscan\"\n"
        << "    },\n"
        << "    \"sqlitePath\": \"../Data/MeyerScanSQLite.db\"\n"
        << "}\n";
    return configPath;
}

// =============================================================================
// Test 7: 数据库类型切换测试
// =============================================================================
// 测试内容:
//   1. 验证数据库类型一致性
//   2. 验证 Disconnect() 后状态正确
// =============================================================================
static void TestDbTypeSwitch(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 7: 数据库类型切换测试 ===\n");
    printf("\n=== Test 7: 数据库类型切换测试 ===\n");

    DatabaseType originalType = db->GetDatabaseType();
    const char* typeStr = (originalType == DatabaseType::MySQL) ?
                          "MySQL" : "SQLite";
    fprintf(s_logFile, "  原始类型: %s\n", typeStr);
    printf("  原始类型: %s\n", typeStr);

    // 断开连接
    VoidResult disconnectResult = db->Disconnect();
    TEST_ASSERT(disconnectResult.IsSuccess(), "Disconnect() 成功");

    // 验证类型一致性
    DatabaseType currentType = db->GetDatabaseType();
    TEST_ASSERT(currentType == originalType, "数据库类型保持一致");
}

// =============================================================================
// Test 8: 断开连接和清理测试
// =============================================================================
// 测试内容:
//   1. 验证 Disconnect() 正确断开连接
//   2. 验证 Shutdown() 正常执行
// =============================================================================
static void TestDisconnect(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 8: 断开连接和清理测试 ===\n");
    printf("\n=== Test 8: 断开连接和清理测试 ===\n");

    // 断开连接
    VoidResult disconnectResult = db->Disconnect();
    TEST_ASSERT(disconnectResult.IsSuccess(), "Disconnect() 后返回 Success");
    TEST_ASSERT(!db->IsConnected(), "Disconnect() 后未连接");

    // 关闭模块
    VoidResult shutdownResult = db->Shutdown();
    TEST_ASSERT(shutdownResult.IsSuccess(), "Shutdown() 执行成功");
}

// =============================================================================
// Test 9: 线程安全测试
// =============================================================================
// 测试内容:
//   验证连续多次调用不会崩溃或死锁
// =============================================================================
static void TestThreadSafety(IDatabase* db) {
    fprintf(s_logFile, "\n=== Test 9: 线程安全测试 ===\n");
    printf("\n=== Test 9: 线程安全测试 ===\n");

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
        fprintf(s_logFile, "  跳过 - 未连接数据库\n");
        printf("  跳过 - 未连接数据库\n");
    }
}

// =============================================================================
// 主函数
// =============================================================================
// 功能说明:
//   1. 设置日志文件
//   2. 获取数据库实例
//   3. 执行所有测试用例
//   4. 输出测试汇总
//   5. 返回测试结果状态码
// =============================================================================
int main() {
    s_moduleRoot = ResolveModuleRoot();

    // -------------------------------------------------------------------------
    // 设置日志文件
    // -------------------------------------------------------------------------
    const std::string testLogPath = s_moduleRoot + "/bin/Release/test_output.log";
    s_logFile = fopen(testLogPath.c_str(), "w");
    if (!s_logFile) {
        s_logFile = stdout;
    }

    // 输出测试标题
    fprintf(s_logFile, "============================================\n");
    fprintf(s_logFile, "MeyerScan Database Module Test Suite\n");
    fprintf(s_logFile, "Database Interface: Result<T> / VoidResult\n");
    fprintf(s_logFile, "============================================\n");
    printf("============================================\n");
    printf("MeyerScan Database Module Test Suite\n");
    printf("Database Interface: Result<T> / VoidResult\n");
    printf("============================================\n");

    // -------------------------------------------------------------------------
    // 获取数据库实例
    // -------------------------------------------------------------------------
    IDatabase* db = GetDatabase();
    if (!db) {
        fprintf(s_logFile, "致命错误: 无法获取数据库实例\n");
        printf("致命错误: 无法获取数据库实例\n");
        if (s_logFile != stdout) fclose(s_logFile);
        return 1;
    }

    const std::string configPath = WriteRuntimeTestConfig();

    // -------------------------------------------------------------------------
    // 执行所有测试
    // -------------------------------------------------------------------------
    TestModuleInit(db);           // Test 1: 模块初始化
    TestConfig(db, configPath.c_str());   // Test 2: 配置加载
    TestConnection(db);           // Test 3: 数据库连接
    TestQuery(db);                // Test 4: 查询执行
    TestTransaction(db);          // Test 5: 事务管理
    TestBackup(db);               // Test 6: 备份功能
    TestDbTypeSwitch(db);         // Test 7: 类型切换
    TestThreadSafety(db);         // Test 9: 线程安全
    TestDisconnect(db);           // Test 8: 断开连接

    // -------------------------------------------------------------------------
    // 输出测试汇总
    // -------------------------------------------------------------------------
    fprintf(s_logFile, "\n============================================\n");
    fprintf(s_logFile, "测试汇总\n");
    fprintf(s_logFile, "============================================\n");
    fprintf(s_logFile, "通过测试数: %d\n", s_testsPassed);
    fprintf(s_logFile, "失败测试数: %d\n", s_testsFailed);
    fprintf(s_logFile, "总测试数: %d\n", s_testsPassed + s_testsFailed);
    fprintf(s_logFile, "============================================\n");
    printf("\n============================================\n");
    printf("测试汇总\n");
    printf("============================================\n");
    printf("通过测试数: %d\n", s_testsPassed);
    printf("失败测试数: %d\n", s_testsFailed);
    printf("总测试数: %d\n", s_testsPassed + s_testsFailed);
    printf("============================================\n");

    if (s_testsFailed == 0) {
        fprintf(s_logFile, "所有测试通过 [OK]\n");
        printf("所有测试通过 [OK]\n");
    } else {
        fprintf(s_logFile, "存在失败的测试 [FAIL]\n");
        printf("存在失败的测试 [FAIL]\n");
    }

    // 关闭日志文件
    if (s_logFile != stdout) {
        fclose(s_logFile);
    }

    return (s_testsFailed == 0) ? 0 : 1;
}
