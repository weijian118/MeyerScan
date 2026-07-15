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

// =============================================================================
// 文件说明:
//   CaseOrderService 的最小回归测试宿主。
//
// 测试链路:
//   1. 创建 SQLite 测试配置。
//   2. 初始化 CaseOrderService，并由服务层确保内部表结构。
//   3. 保存/读取患者订单组合 JSON。
//   4. 通过参考数据和 QueryJson 验证服务层扩展入口。
//
// 阅读重点:
//   - 患者/订单字段经常扩展，因此服务层优先使用 JSON 承载变化字段。
//   - DLL 接口用调用方提供的 char buffer 返回 JSON，避免 QString/std::string 跨 DLL 释放问题。
//   - 业务模块应优先通过服务层访问患者/订单，不绕过服务层直接拼 SQL。
// =============================================================================

namespace {

// 写入服务层测试用数据库配置。
// CaseOrderService 依赖 DatabaseQtAdapter/MyDatabase，因此测试前需要准备一份 SQLite 配置。
QString WriteTestConfig(const QString& appDir) {
    // QDir 以 exe 所在目录作为根目录，确保 VS 调试和命令行运行结果一致。
    QDir dir(appDir);
    // 所有测试产物都放在 exe 输出目录，保证 VS 调试、命令行运行和第三方拉起时路径一致。
    dir.mkpath("config");
    dir.mkpath("Data");

    // db_config.json 是 MyDatabase 的统一配置入口。
    const QString configPath = dir.filePath("config/db_config.json");
    QFile file(configPath);
    // 每次测试重写配置，避免之前手工切库影响本次回归。
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        // 用 UTF-8 写 JSON，保持配置文件编码稳定。
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

// 测试断言函数。
// 失败时 main 返回不同错误码，便于快速定位是初始化、建表、保存还是查询失败。
bool Check(bool condition, const char* message) {
    // 成功检查写 stdout。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败检查写 stderr。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// CaseOrderService 测试入口：准备 SQLite 配置并验证患者订单服务的保存、查询和参考数据链路。
int main(int argc, char* argv[]) {
    // 服务层测试只使用 QtCore/Qt 数据类型，不创建界面控件。
    QCoreApplication app(argc, argv);

    // 所有路径从 exe 所在目录开始推导，避免 currentPath 不稳定。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志目录按正式结构放在 exe 同级 logs 下。
    const QString logDir = QDir(appDir).filePath("logs");
    // 提前创建日志目录，避免 Logger 初始化失败。
    QDir().mkpath(logDir);

    // 服务层会通过 Logger 记录关键步骤；测试也先初始化 Logger，方便排查失败。
    ILogger* logger = GetLogger();
    if (logger) {
        // Qt 测试代码使用 Logger 的 QString 重载，转换细节由公共头文件统一维护。
        logger->Init(logDir, LogLevel::Info);
    }

    // 写入 SQLite 配置后再初始化 CaseOrderService。
    const QString configPath = WriteTestConfig(appDir);
    // 服务接口保持稳定 C ABI；两个命名缓冲区让路径指针生命周期覆盖 Init 调用。
    const QByteArray configPathUtf8 = configPath.toUtf8();
    const QByteArray logDirUtf8 = logDir.toUtf8();
    // 获取服务层 DLL 接口，验证导出函数可用。
    ICaseOrderService* service = GetCaseOrderService();
    if (!Check(service != nullptr, "CaseOrderService 工厂函数返回有效实例")) {
        return 1;
    }

    // Init 内部会初始化 DatabaseQtAdapter 和 MyDatabase。
    if (!Check(service->Init(configPathUtf8.constData(), logDirUtf8.constData()),
               "CaseOrderService 初始化成功")) {
        return 2;
    }
    // EnsureSchema 创建患者/订单 JSON 表和参考数据表，是服务层可用的前置条件。
    if (!Check(service->EnsureSchema().IsSuccess(), "CaseOrderService 能创建/检查服务表")) {
        return 3;
    }

    // 患者/订单组合使用 JSON 保存，验证字段可扩展时 ABI 不需要改函数签名。
    // 字段未来增删时只改 JSON 内容和服务层解析，不必修改 DLL 函数签名。
    const char* patientOrderJson =
        "{"
        "\"schemaVersion\":1,"
        "\"status\":\"created\","
        "\"patient\":{\"patientId\":\"P001\",\"name\":\"Patient Demo\"},"
        "\"order\":{\"orderId\":\"COS-001\",\"caseId\":\"C001\"},"
        "\"extensions\":{}"
        "}";
    // SavePatientOrderJson 从标准嵌套 order/patient 对象提取索引字段，并把完整 JSON 存入数据库。
    if (!Check(service->SavePatientOrderJson(patientOrderJson).IsSuccess(),
               "SavePatientOrderJson 能保存患者/订单组合 JSON")) {
        return 4;
    }

    // 使用固定大 buffer 接收查询结果，避免 std::string/QString 跨 DLL 边界。
    char buffer[16 * 1024] = {0};
    // 按订单号读取刚刚保存的 JSON，验证写入和查询链路闭环。
    if (!Check(service->GetPatientOrderJson("COS-001", buffer, sizeof(buffer)).IsSuccess(),
               "GetPatientOrderJson 能按订单号读取 JSON")) {
        return 5;
    }
    // 用 strstr 做轻量内容检查，不引入额外 JSON 解析依赖。
    if (!Check(std::strstr(buffer, "Patient Demo") != nullptr,
               "读取结果包含患者名称")) {
        return 6;
    }

    // 参考数据写入通过 Adapter 完成，这是测试准备动作；正式 UI 仍通过服务层查询。
    // 这里直接拿 Adapter 是为了造数；业务模块不应绕过服务层读写患者/订单数据。
    DatabaseQtAdapter* adapter = GetDatabaseQtAdapter();
    if (!Check(adapter != nullptr, "测试准备阶段可获取 DatabaseQtAdapter")) {
        return 7;
    }
    // errorMessage 用来接收 SQL 执行失败时的具体错误文本。
    QString errorMessage;
    // REPLACE INTO 让重复运行测试时同一个 id 被覆盖，不会因为主键冲突失败。
    if (!Check(adapter->ExecuteUpdate(
            "REPLACE INTO ms_reference_data "
            "(id, category, display_name, payload_json, enabled, sort_index, updated_at) "
            "VALUES ('DOC-001', 'doctor', 'Dr Demo', '{}', 1, 1, '2026-07-03')",
            &errorMessage),
        "测试准备阶段能写入医生参考数据")) {
        // 输出前保存 UTF-8 字节，避免示例代码依赖临时 constData 指针。
        const QByteArray errorUtf8 = errorMessage.toUtf8();
        std::fprintf(stderr, "写入参考数据失败: %s\n", errorUtf8.constData());
        return 8;
    }

    // 清空 buffer，确保下面验证的内容来自本次查询而非上一次残留。
    std::memset(buffer, 0, sizeof(buffer));
    // category=doctor 代表医生列表，设置模块会通过该入口读取参考数据。
    if (!Check(service->ListReferenceDataJson("doctor", buffer, sizeof(buffer)).IsSuccess(),
               "ListReferenceDataJson 能读取医生参考数据")) {
        return 9;
    }
    // 验证医生名称真实出现在 JSON 数组中。
    if (!Check(std::strstr(buffer, "Dr Demo") != nullptr,
               "医生参考数据包含测试医生")) {
        return 10;
    }

    // 再次清空 buffer，验证统一 QueryJson 入口。
    std::memset(buffer, 0, sizeof(buffer));
    // QueryJson 使用稳定 queryName + 参数 JSON，便于未来扩展更多查询而不增加新导出函数。
    if (!Check(service->QueryJson("patientOrder.byOrderId", "{\"orderId\":\"COS-001\"}", buffer, sizeof(buffer)).IsSuccess(),
               "QueryJson 稳定查询入口能读取患者/订单")) {
        return 11;
    }
    // 验证统一查询入口返回了目标订单。
    if (!Check(std::strstr(buffer, "COS-001") != nullptr,
               "QueryJson 读取结果包含订单号")) {
        return 12;
    }

    // 查询案例管理使用的轻量订单读模型，验证完整 payload 已经转换成稳定摘要字段。
    std::memset(buffer, 0, sizeof(buffer));
    if (!Check(service->QueryJson("patientOrder.listOrders", "{}", buffer, sizeof(buffer)).IsSuccess(),
               "QueryJson 能返回轻量订单列表")) {
        return 13;
    }
    if (!Check(std::strstr(buffer, "Patient Demo") != nullptr &&
               std::strstr(buffer, "COS-001") != nullptr,
               "订单列表包含患者名称和订单号")) {
        return 14;
    }

    // 患者读模型由患者/订单组合表按 patientId 归并，并统计该患者的订单数量。
    std::memset(buffer, 0, sizeof(buffer));
    if (!Check(service->QueryJson("patientOrder.listPatients", "{}", buffer, sizeof(buffer)).IsSuccess(),
               "QueryJson 能返回归并后的患者列表")) {
        return 15;
    }
    if (!Check(std::strstr(buffer, "P001") != nullptr &&
               std::strstr(buffer, "\"orderCount\":1") != nullptr,
               "患者列表包含患者号和订单计数")) {
        return 16;
    }

    // 先关闭服务层，再关闭 Adapter/Logger，避免服务层析构阶段仍写日志或访问数据库。
    service->Shutdown();
    adapter->Shutdown();
    if (logger) {
        logger->Shutdown();
    }

    std::printf("CaseOrderServiceTest passed.\n");
    return 0;
}
