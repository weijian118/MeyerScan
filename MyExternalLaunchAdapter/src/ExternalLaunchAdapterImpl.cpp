#include "ExternalLaunchAdapterImpl.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>
#include <cstring>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与工程中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_ExternalLaunchAdapter";
// 模块版本用于版本清单和现场排查，修改 Version.rc 时要同步修改这里。
const char* Version = "MeyerScan_ExternalLaunchAdapter v0.1.0 (2026-07-04)";
}
}

ExternalLaunchAdapterImpl& ExternalLaunchAdapterImpl::Instance() {
    // 函数内 static 在首次调用时构造，后续所有调用返回同一个对象。
    // 这里不保存订单内容，只缓存路径和 Logger 指针，所以单例不会造成订单数据串扰。
    static ExternalLaunchAdapterImpl instance;
    return instance;
}

bool ExternalLaunchAdapterImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 调用方传入的 const char* 可能来自临时 QByteArray::constData()。
    // 这里必须复制到成员 QByteArray，不能保存原始指针。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // Logger 是进程级单例，本模块缓存一次指针，后续写日志不重复 GetLogger。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        // Logger::Init 可重复调用；日志目录由 MainExe 统一传入，避免使用 currentPath。
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }
    WriteLog(LogLevel::Info, "Init", "External launch adapter initialized");
    return true;
}

bool ExternalLaunchAdapterImpl::NormalizeOrderFile(const char* inputJsonPathUtf8,
                                                   const char* thirdPartyTypeUtf8,
                                                   char* outputJsonUtf8,
                                                   int outputSize,
                                                   ExternalLaunchResult* result) {
    // 先把 result 置为成功默认值，后续失败分支再覆盖错误码和信息。
    SetResult(result, 0, "OK");
    if (!inputJsonPathUtf8 || !inputJsonPathUtf8[0]) {
        // 输入文件路径为空时不能继续猜测，直接返回可诊断错误。
        SetResult(result, 1, "Input json path is empty");
        WriteLog(LogLevel::Warning, "NormalizeOrderFile", "Input json path is empty");
        return false;
    }
    if (!outputJsonUtf8 || outputSize <= 0) {
        // 输出 JSON 使用调用方缓冲区，空指针或尺寸无效都会导致越界风险。
        SetResult(result, 2, "Output buffer is invalid");
        WriteLog(LogLevel::Warning, "NormalizeOrderFile", "Output buffer is invalid");
        return false;
    }

    // 第三方路径按 UTF-8 传入，Qt 文件 API 使用 QString 表达路径。
    const QString inputPath = QString::fromUtf8(inputJsonPathUtf8);
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        SetResult(result, 3, QString("Failed to open input json: %1").arg(inputPath));
        WriteLog(LogLevel::Warning, "NormalizeOrderFile", QString("Open failed: %1").arg(inputPath));
        return false;
    }

    QJsonParseError parseError;
    // readAll 一次性读取小型建单 JSON。
    // 第三方建单信息应是轻量文本，不允许把大块扫描数据放进这个 JSON。
    const QByteArray inputBytes = file.readAll();
    // QJsonDocument::fromJson 会解析 UTF-8 JSON，并把错误位置/原因写入 parseError。
    const QJsonDocument inputDocument = QJsonDocument::fromJson(inputBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !inputDocument.isObject()) {
        SetResult(result, 4, QString("Invalid input json: %1").arg(parseError.errorString()));
        WriteLog(LogLevel::Warning, "NormalizeOrderFile", QString("Parse failed: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject inputRoot = inputDocument.object();
    const QJsonObject sourceObject = inputRoot.value("source").toObject();
    // 命令行传入的 thirdPartyType 优先级最高，便于同一种 JSON 文件按不同第三方规则测试。
    QString thirdPartyType = QString::fromUtf8(thirdPartyTypeUtf8 ? thirdPartyTypeUtf8 : "").trimmed();
    if (thirdPartyType.isEmpty()) {
        // 命令行未传第三方类型时，再从 JSON 的 source.thirdPartyType 读取。
        thirdPartyType = ReadString(sourceObject, "thirdPartyType", "generic").trimmed();
    }
    if (thirdPartyType.isEmpty()) {
        // 最后兜底 generic，保证标准上下文始终有第三方分类字段。
        thirdPartyType = "generic";
    }

    // BuildNormalizedContext 只做字段映射，不写文件、不显示 UI、不保存数据库。
    QJsonObject normalizedRoot = BuildNormalizedContext(inputRoot, thirdPartyType, result);
    QJsonObject normalizedSource = normalizedRoot.value("source").toObject();
    // inputJsonPath 写入 source，便于日志或后续排查知道原始第三方文件来自哪里。
    normalizedSource.insert("inputJsonPath", QFileInfo(inputPath).absoluteFilePath());
    normalizedRoot.insert("source", normalizedSource);

    // Compact JSON 适合跨模块传递和日志摘要，避免无意义空白增大调用方缓冲区。
    const QByteArray outputBytes = QJsonDocument(normalizedRoot).toJson(QJsonDocument::Compact);
    const int requiredSize = outputBytes.size() + 1;
    if (requiredSize > outputSize) {
        // 缓冲区不足时不写截断 JSON，而是告诉调用方需要多大缓冲区。
        // MainExe 会根据 requiredBufferSize 扩容后重试一次。
        SetResult(result,
                  5,
                  QString("Output buffer too small, required %1 bytes").arg(requiredSize),
                  thirdPartyType,
                  normalizedSource.value("thirdPartyName").toString(),
                  normalizedSource.value("sourceSystem").toString(),
                  requiredSize);
        WriteLog(LogLevel::Warning,
                 "NormalizeOrderFile",
                 QString("Buffer too small, required=%1").arg(requiredSize));
        return false;
    }

    // ABI 使用调用方拥有的 C 字符缓冲区，所以需要手动复制字节并补 '\0'。
    std::memcpy(outputJsonUtf8, outputBytes.constData(), outputBytes.size());
    outputJsonUtf8[outputBytes.size()] = '\0';
    SetResult(result,
              0,
              "Order context normalized",
              thirdPartyType,
              normalizedSource.value("thirdPartyName").toString(),
              normalizedSource.value("sourceSystem").toString(),
              requiredSize);
    WriteLog(LogLevel::Info,
             "NormalizeOrderFile",
             QString("Normalized external order, thirdPartyType=%1").arg(thirdPartyType));
    return true;
}

// 返回模块版本字符串。
// 版本字符串来自 ModuleInfo，要求与 Version.rc 文件版本同步维护。
const char* ExternalLaunchAdapterImpl::GetModuleVersion() const {
    // 返回字符串字面量，生命周期覆盖整个进程。
    return ModuleInfo::Version;
}

void ExternalLaunchAdapterImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "External launch adapter shutdown");
    if (m_logger) {
        // 只刷新日志，不关闭 Logger；Logger 生命周期由 MainExe 统一管理。
        m_logger->Flush();
    }
    // 清空缓存指针和路径，下一次 Init 可重新设置运行目录。
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

QJsonObject ExternalLaunchAdapterImpl::BuildNormalizedContext(const QJsonObject& inputRoot,
                                                              const QString& thirdPartyType,
                                                              ExternalLaunchResult* result) const {
    // 标准结构优先读取 source/patient/order/scanPlan。
    // 同时兼容少量扁平字段，便于 cmd 模拟或早期第三方快速接入测试。
    const QJsonObject inputSource = inputRoot.value("source").toObject();
    const QJsonObject inputPatient = inputRoot.value("patient").toObject();
    const QJsonObject inputOrder = inputRoot.value("order").toObject();
    const QJsonObject inputScanPlan = inputRoot.value("scanPlan").toObject();

    QJsonObject source;
    // launchType 标记这次不是人工首页填写，而是第三方拉起。
    source.insert("launchType", "external");
    // thirdPartyType 是后续多第三方差异映射的关键分流字段，不能为空。
    source.insert("thirdPartyType", thirdPartyType);
    source.insert("thirdPartyName", ReadString(inputSource, "thirdPartyName", thirdPartyType));
    source.insert("sourceSystem", ReadString(inputSource, "sourceSystem", "cmd-simulator"));
    source.insert("sourceVersion", ReadString(inputSource, "sourceVersion", "0.1"));

    QJsonObject patient;
    // 患者字段允许从 patient 对象读取；早期模拟 JSON 也可以直接写在根对象。
    patient.insert("patientId", ReadString(inputPatient, "patientId", ReadString(inputRoot, "patientId")));
    patient.insert("name", ReadString(inputPatient, "name", ReadString(inputRoot, "patientName", "External Patient")));
    const QString ageText = ReadString(inputPatient, "age", ReadString(inputRoot, "age"));
    if (!ageText.isEmpty()) {
        // 年龄在标准上下文中写成 number，OrderCreateUI 再按文本显示。
        patient.insert("age", ageText.toInt());
    }
    patient.insert("birthDate", ReadString(inputPatient, "birthDate", ReadString(inputRoot, "birthDate", "2000/01/01")));
    patient.insert("gender", ReadString(inputPatient, "gender", ReadString(inputRoot, "gender", "unknown")));
    patient.insert("contact", ReadString(inputPatient, "contact", ReadString(inputRoot, "contact")));
    patient.insert("note", ReadString(inputPatient, "note", ReadString(inputRoot, "patientNote")));

    QJsonObject order;
    // 订单字段同样支持 order 对象优先、根对象兜底，方便外部系统逐步迁移到标准结构。
    order.insert("orderId", ReadString(inputOrder, "orderId", ReadString(inputRoot, "orderId")));
    order.insert("doctor", ReadString(inputOrder, "doctor", ReadString(inputRoot, "doctor", "Doctor")));
    order.insert("lab", ReadString(inputOrder, "lab", ReadString(inputRoot, "lab", "Default Lab")));
    order.insert("deliveryDate", ReadString(inputOrder, "deliveryDate", QDate::currentDate().addDays(3).toString("yyyy/MM/dd")));
    order.insert("caseType", ReadString(inputOrder, "caseType", "restoration"));
    order.insert("note", ReadString(inputOrder, "note", ReadString(inputRoot, "orderNote")));

    QJsonObject scanPlan;
    QJsonArray items = inputScanPlan.value("items").toArray();
    if (items.isEmpty()) {
        // 兼容简单测试结构：{ "teeth": [15, 16] }。
        // 这种结构只用于模拟第三方输入，正式第三方建议传 scanPlan.items。
        const QJsonArray teeth = inputRoot.value("teeth").toArray();
        for (const QJsonValue& toothValue : teeth) {
            // 每个牙位生成一个最小扫描方案项。
            QJsonObject item;
            item.insert("tooth", toothValue.toInt());
            item.insert("type", "crown");
            item.insert("material", "--");
            item.insert("shade", "--");
            items.append(item);
        }
        if (items.isEmpty()) {
            // 兜底生成一个牙位，保证 UI smoke 能看到扫描方案区域被填充。
            QJsonObject item;
            item.insert("tooth", 15);
            item.insert("type", "crown");
            item.insert("material", "--");
            item.insert("shade", "A2");
            items.append(item);
        }
    }
    scanPlan.insert("items", items);

    QJsonObject root;
    // schemaVersion 允许后续标准上下文升级时做兼容判断。
    root.insert("schemaVersion", 1);
    root.insert("source", source);
    root.insert("patient", patient);
    root.insert("order", order);
    root.insert("scanPlan", scanPlan);

    SetResult(result,
              0,
              "Order context normalized",
              thirdPartyType,
              source.value("thirdPartyName").toString(),
              source.value("sourceSystem").toString());
    return root;
}

QString ExternalLaunchAdapterImpl::ReadString(const QJsonObject& object,
                                              const QString& key,
                                              const QString& defaultValue) const {
    // Qt JSON 值是弱类型容器，读取前需要判断真实类型。
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        // 字符串字段直接返回。
        return value.toString();
    }
    if (value.isDouble()) {
        // 第三方可能把编号、年龄、牙位等数字写成 number，这里统一转成十进制文本。
        return QString::number(static_cast<qint64>(value.toDouble()));
    }
    if (value.isBool()) {
        // bool 字段转成稳定英文文本，避免跨模块传递本地化字符串。
        return value.toBool() ? "true" : "false";
    }
    // 字段缺失或类型无法识别时返回调用方给定默认值。
    return defaultValue;
}

void ExternalLaunchAdapterImpl::SetResult(ExternalLaunchResult* result,
                                          int errorCode,
                                          const QString& message,
                                          const QString& thirdPartyType,
                                          const QString& thirdPartyName,
                                          const QString& sourceSystem,
                                          int requiredBufferSize) const {
    if (!result) {
        // result 允许为空；这种情况下调用方只关心 bool 返回值。
        return;
    }

    // 先清零整个 POD 结构，保证短字符串之后不会残留上一次内容。
    std::memset(result, 0, sizeof(ExternalLaunchResult));
    result->errorCode = errorCode;
    result->requiredBufferSize = requiredBufferSize;
    CopyText(result->message, sizeof(result->message), message);
    CopyText(result->thirdPartyType, sizeof(result->thirdPartyType), thirdPartyType);
    CopyText(result->thirdPartyName, sizeof(result->thirdPartyName), thirdPartyName);
    CopyText(result->sourceSystem, sizeof(result->sourceSystem), sourceSystem);
}

// 将 QString 复制到固定大小的 C 字符串缓冲区。
// 这个函数专门服务跨 DLL POD 结果结构，保证字符串始终以 '\0' 结尾。
void ExternalLaunchAdapterImpl::CopyText(char* target, int targetSize, const QString& text) const {
    if (!target || targetSize <= 0) {
        // 空目标或非法长度不能写入。
        return;
    }
    // 公共 ABI 使用 UTF-8 char 数组，QString 需要先转换成 QByteArray。
    const QByteArray bytes = text.toUtf8();
    // 预留 1 字节给 '\0'，避免 C 字符串无终止符。
    const int copySize = std::min(targetSize - 1, bytes.size());
    if (copySize > 0) {
        std::memcpy(target, bytes.constData(), copySize);
    }
    // 即使源字符串为空，也要写入终止符。
    target[copySize] = '\0';
}

void ExternalLaunchAdapterImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        // Logger 未初始化时不影响适配器主流程，直接静默返回。
        return;
    }
    // Logger 公共接口是 const char*，QString 日志内容需要转 UTF-8。
    const QByteArray bytes = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

extern "C" MEYERSCAN_EXTERNALLAUNCHADAPTER_API IExternalLaunchAdapter* GetExternalLaunchAdapter() {
    // 导出 C ABI 工厂函数，避免调用方依赖 C++ 名字修饰。
    return &ExternalLaunchAdapterImpl::Instance();
}

// 统一版本导出函数。
// 版本清单通过固定符号名读取代码版本，不需要解析第三方 JSON 文件。
extern "C" MEYERSCAN_EXTERNALLAUNCHADAPTER_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
