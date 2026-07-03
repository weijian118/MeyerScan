#include "DatabaseQtAdapter.h"
#include "Logger.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cstdio>

namespace {

// 写出本测试专用的 SQLite 配置。
// 配置文件放在 DatabaseQtAdapterTest.exe 同级目录下，避免污染源码目录和正式发布配置。
QString WriteTestConfig(const QString& appDir) {
    QDir dir(appDir);
    // config 模拟正式发布目录结构，Data 存放测试数据库文件。
    dir.mkpath("config");
    dir.mkpath("Data");

    const QString configPath = dir.filePath("config/db_config.json");
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&file);
        // JSON 文件统一使用 UTF-8，后续如果路径包含中文也不会被本地编码破坏。
        stream.setCodec("UTF-8");
        // sqlitePath 故意使用相对路径，用来验证底层 Database 按配置文件目录解析路径。
        stream << "{\n"
               << "  \"databaseType\": \"sqlite\",\n"
               << "  \"mysql\": {\n"
               << "    \"host\": \"127.0.0.1\",\n"
               << "    \"port\": 3308,\n"
               << "    \"service\": \"MSCANDB\",\n"
               << "    \"database\": \"mscan\",\n"
               << "    \"dataDir\": \"../MySQL/data/mscan\"\n"
               << "  },\n"
               << "  \"sqlitePath\": \"../Data/DatabaseQtAdapterTest.db\"\n"
               << "}\n";
    }
    return configPath;
}

// 统一测试断言输出。
// 返回 bool 是为了 main 可以在任意一步失败时立即退出，方便定位第一个错误。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

int main(int argc, char* argv[]) {
    // 本测试只需要 Qt Core：路径、JSON、文件和字节转换都来自 Qt Core。
    QCoreApplication app(argc, argv);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    // Logger 是 DatabaseQtAdapter 的可选依赖，但测试中主动初始化，确保适配层日志链路可用。
    ILogger* logger = GetLogger();
    if (logger) {
        logger->Init(logDir.toUtf8().constData(), LogLevel::Info);
    }

    DatabaseQtAdapter* adapter = GetDatabaseQtAdapter();
    if (!Check(adapter != nullptr, "DatabaseQtAdapter 工厂函数返回有效实例")) {
        return 1;
    }

    const QString configPath = WriteTestConfig(appDir);
    QString errorMessage;
    if (!Check(adapter->EnsureConnected(configPath, "sqlite", &errorMessage), "Adapter 能通过 SQLite 配置连接数据库")) {
        std::fprintf(stderr, "数据库连接错误: %s\n", errorMessage.toUtf8().constData());
        return 2;
    }
    if (!Check(adapter->IsConnected(), "Adapter 连接后 IsConnected 为 true")) {
        return 3;
    }

    // 建表和写入通过 ExecuteUpdate 完成，验证 Qt QString -> Database UTF-8 SQL 的转换链路。
    if (!Check(adapter->ExecuteUpdate("CREATE TABLE IF NOT EXISTS adapter_smoke (id INTEGER PRIMARY KEY, name TEXT)", &errorMessage),
               "ExecuteUpdate 能创建测试表")) {
        return 4;
    }
    if (!Check(adapter->ExecuteUpdate("REPLACE INTO adapter_smoke (id, name) VALUES (1, 'Adapter Demo')", &errorMessage),
               "ExecuteUpdate 能写入测试数据")) {
        return 5;
    }

    // 查询结果以 Database 通用表格 JSON 返回，Adapter 负责把 C 缓冲区转换成 QByteArray。
    QByteArray tableJson;
    if (!Check(adapter->ExecuteQueryJson("SELECT id, name FROM adapter_smoke WHERE id=1", &tableJson, &errorMessage),
               "ExecuteQueryJson 能读取测试数据")) {
        return 6;
    }
    const QJsonArray rows = DatabaseQtAdapter::RowsFromTableJson(tableJson);
    if (!Check(rows.size() == 1, "RowsFromTableJson 能提取一行数据")) {
        return 7;
    }
    if (!Check(rows.first().toObject().value("name").toString() == "Adapter Demo",
               "查询结果包含写入的名称")) {
        return 8;
    }

    QJsonDocument document;
    if (!Check(adapter->ExecuteQueryJsonDocument("SELECT 2 AS id, 'JsonDocument Demo' AS name", &document, &errorMessage),
               "ExecuteQueryJsonDocument 能返回 QJsonDocument")) {
        return 9;
    }
    if (!Check(DatabaseQtAdapter::RowsFromTableJson(document).size() == 1,
               "RowsFromTableJson(QJsonDocument) 能提取数据")) {
        return 10;
    }

    // 单引号转义是骨架期手写 SQL 的最低安全线，正式 DAO 后续再迁移到参数绑定。
    if (!Check(DatabaseQtAdapter::EscapeSqlText("O'Brien") == "O''Brien",
               "EscapeSqlText 能正确转义单引号")) {
        return 11;
    }

    adapter->Disconnect();
    if (!Check(!adapter->IsConnected(), "Disconnect 后连接状态为 false")) {
        return 12;
    }
    adapter->Shutdown();
    if (logger) {
        logger->Shutdown();
    }

    std::printf("DatabaseQtAdapterTest passed.\n");
    return 0;
}
