#include "ScanSchemaService.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

// 输出断言结果并返回布尔值，使失败位置可以直接从控制台识别。
bool Check(bool condition, const QString& message) {
    QTextStream stream(condition ? stdout : stderr);
    stream << (condition ? "[PASS] " : "[FAIL] ") << message << "\n";
    stream.flush();
    return condition;
}

// 调用服务并把返回缓冲区解析为 JSON 对象。
QJsonObject BuildProcess(IScanSchemaService* service,
                         const QJsonObject& config,
                         bool* ok) {
    QByteArray output(64 * 1024, '\0');
    const QByteArray input = QJsonDocument(config).toJson(QJsonDocument::Compact);
    const ScanSchemaServiceResult result = service->BuildScanProcessJson(
        input.constData(), output.data(), output.size());
    *ok = result.IsSuccess();
    return *ok ? QJsonDocument::fromJson(output.constData()).object() : QJsonObject();
}

} // namespace

// 验证默认、种植和异性扫描杆三类规则。
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    IScanSchemaService* service = GetScanSchemaService();
    if (!Check(service != nullptr, "获取 ScanSchemaService 单例")) {
        return 1;
    }
    if (!Check(service->Init(""), "初始化 ScanSchemaService")) {
        return 2;
    }

    QJsonObject defaultConfig;
    defaultConfig.insert("occlusionType", "natural");
    defaultConfig.insert("scanPlan", QJsonObject{{"items", QJsonArray()}});
    bool ok = false;
    const QJsonObject defaultProcess = BuildProcess(service, defaultConfig, &ok);
    if (!Check(ok, "生成默认扫描流程") ||
        !Check(defaultProcess.value("steps").toArray().size() == 4,
               "默认流程包含上颌、交换、下颌、咬合四步")) {
        return 3;
    }

    QJsonArray implantItems;
    implantItems.append(QJsonObject{{"tooth", 16}, {"type", "implant"}});
    QJsonObject implantConfig;
    implantConfig.insert("occlusionType", "natural");
    implantConfig.insert("maxillaSegmentedRod", true);
    implantConfig.insert("scanPlan", QJsonObject{{"items", implantItems}});
    const QJsonObject implantProcess = BuildProcess(service, implantConfig, &ok);
    const QByteArray implantJson = QJsonDocument(implantProcess).toJson(QJsonDocument::Compact);
    if (!Check(ok, "生成种植扫描流程") ||
        !Check(implantJson.contains("maxilla_scanbody_1") && implantJson.contains("maxilla_scanbody_2"),
               "上颌种植且分段时生成两段扫描杆步骤")) {
        return 4;
    }

    QJsonObject specialConfig;
    specialConfig.insert("maxillaDiffRod", true);
    specialConfig.insert("maxillaSegmentedRod", true);
    specialConfig.insert("occlusionType", "maxilla_temporary");
    specialConfig.insert("scanPlan", QJsonObject{{"items", QJsonArray()}});
    const QJsonObject specialProcess = BuildProcess(service, specialConfig, &ok);
    const QByteArray specialJson = QJsonDocument(specialProcess).toJson(QJsonDocument::Compact);
    if (!Check(ok, "生成异性扫描杆流程") ||
        !Check(specialJson.contains("maxilla_diff_rod_1") && specialJson.contains("maxilla_cuff"),
               "异性扫描杆流程包含扫描杆和愈合帽步骤") ||
        !Check(!specialJson.contains("\"label\":"), "流程合同不携带翻译后的 label 文本")) {
        return 5;
    }

    char tinyBuffer[4] = {};
    const QByteArray input = QJsonDocument(defaultConfig).toJson(QJsonDocument::Compact);
    const ScanSchemaServiceResult tinyResult = service->BuildScanProcessJson(
        input.constData(), tinyBuffer, static_cast<int>(sizeof(tinyBuffer)));
    if (!Check(tinyResult.IsError() && tinyResult.requiredSize > static_cast<int>(sizeof(tinyBuffer)),
               "缓冲区不足时返回 requiredSize")) {
        return 6;
    }

    service->Shutdown();
    return 0;
}
