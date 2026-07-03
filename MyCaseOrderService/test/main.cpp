#include "CaseOrderService.h"
#include "DatabaseQtAdapter.h"
#include "Logger.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include <cstring>

namespace {

QString WriteTestConfig(const QString& appDir) {
    QDir dir(appDir);
    // 所有测试产物都放在 exe 输出目录，保证 VS 调试、命令行运行和第三方拉起时路径一致。
    dir.mkpath("config");
    dir.mkpath("Data");

    const QString configPath = dir.filePath("config/db_config.json");
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        // 固定 SQLite，避免本地没有 MySQL 服务时影响服务层基本链路验证。
        stream << "{\n"
               << "  \"databaseType\": \"sqlite\",\n"
               << "  \"mysql\": {\n"
               << "    \"host\": \"127.0.0.1\",\n"
               << "    \"port\": 3308,\n"
               << "    \"service\": \"MSCANDB\",\n"
               << "    \"database\": \"mscan\",\n"
               << "    \"dataDir\": \"../MySQL/data/mscan\"\n"
               << "  },\n"
               << "  \"sqlitePath\": \"../Data/CaseOrderServiceTest.db\"\n"
               << "}\n";
    }
    return configPath;
}

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
    QCoreApplication app(argc, argv);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    ILogger* logger = GetLogger();
    if (logger) {
        logger->Init(logDir.toUtf8().constData(), LogLevel::Info);
    }

    const QString configPath = WriteTestConfig(appDir);
    ICaseOrderService* service = GetCaseOrderService();
    if (!Check(service != nullptr, "CaseOrderService 工厂函数返回有效实例")) {
        return 1;
    }

    if (!Check(service->Init(configPath.toUtf8().constData(), logDir.toUtf8().constData()),
               "CaseOrderService 初始化成功")) {
        return 2;
    }
    if (!Check(service->EnsureSchema().IsSuccess(), "CaseOrderService 能创建/检查服务表")) {
        return 3;
    }

    // 患者/订单组合使用 JSON 保存，验证字段可扩展时 ABI 不需要改函数签名。
    const char* patientOrderJson =
        "{"
        "\"orderId\":\"COS-001\","
        "\"patientId\":\"P001\","
        "\"caseId\":\"C001\","
        "\"status\":\"created\","
        "\"patientName\":\"Patient Demo\""
        "}";
    if (!Check(service->SavePatientOrderJson(patientOrderJson).IsSuccess(),
               "SavePatientOrderJson 能保存患者/订单组合 JSON")) {
        return 4;
    }

    char buffer[16 * 1024] = {0};
    if (!Check(service->GetPatientOrderJson("COS-001", buffer, sizeof(buffer)).IsSuccess(),
               "GetPatientOrderJson 能按订单号读取 JSON")) {
        return 5;
    }
    if (!Check(std::strstr(buffer, "Patient Demo") != nullptr,
               "读取结果包含患者名称")) {
        return 6;
    }

    // 参考数据写入通过 Adapter 完成，这是测试准备动作；正式 UI 仍通过服务层查询。
    DatabaseQtAdapter* adapter = GetDatabaseQtAdapter();
    if (!Check(adapter != nullptr, "测试准备阶段可获取 DatabaseQtAdapter")) {
        return 7;
    }
    QString errorMessage;
    if (!Check(adapter->ExecuteUpdate(
            "REPLACE INTO ms_reference_data "
            "(id, category, display_name, payload_json, enabled, sort_index, updated_at) "
            "VALUES ('DOC-001', 'doctor', 'Dr Demo', '{}', 1, 1, '2026-07-03')",
            &errorMessage),
        "测试准备阶段能写入医生参考数据")) {
        std::fprintf(stderr, "写入参考数据失败: %s\n", errorMessage.toUtf8().constData());
        return 8;
    }

    std::memset(buffer, 0, sizeof(buffer));
    if (!Check(service->ListReferenceDataJson("doctor", buffer, sizeof(buffer)).IsSuccess(),
               "ListReferenceDataJson 能读取医生参考数据")) {
        return 9;
    }
    if (!Check(std::strstr(buffer, "Dr Demo") != nullptr,
               "医生参考数据包含测试医生")) {
        return 10;
    }

    std::memset(buffer, 0, sizeof(buffer));
    if (!Check(service->QueryJson("patientOrder.byOrderId", "{\"orderId\":\"COS-001\"}", buffer, sizeof(buffer)).IsSuccess(),
               "QueryJson 稳定查询入口能读取患者/订单")) {
        return 11;
    }
    if (!Check(std::strstr(buffer, "COS-001") != nullptr,
               "QueryJson 读取结果包含订单号")) {
        return 12;
    }

    service->Shutdown();
    adapter->Shutdown();
    if (logger) {
        logger->Shutdown();
    }

    std::printf("CaseOrderServiceTest passed.\n");
    return 0;
}
