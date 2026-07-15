#include "ScanSchemaServiceImpl.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <cstring>

namespace {

namespace ModuleInfo {
const char* Name = "MeyerScan_ScanSchemaService";
const char* Version = "MeyerScan_ScanSchemaService v0.1.0 (2026-07-15)";
}

const char* kOcclusionNatural = "natural";
const char* kOcclusionMaxillaTemporary = "maxilla_temporary";
const char* kOcclusionMandibleTemporary = "mandible_temporary";
const char* kOcclusionFullTemporary = "full_temporary";
const char* kOcclusionRecord = "record";

// 安全复制 UTF-8 短消息，并始终写入字符串结束符。
void CopyText(char* target, int targetSize, const char* source) {
    if (!target || targetSize <= 0) {
        return;
    }
    const char* text = source ? source : "";
    const size_t copySize = qMin(static_cast<size_t>(targetSize - 1), std::strlen(text));
    std::memset(target, 0, static_cast<size_t>(targetSize));
    if (copySize > 0) {
        std::memcpy(target, text, copySize);
    }
}

} // namespace

// 返回扫描方案服务单例。
ScanSchemaServiceImpl& ScanSchemaServiceImpl::Instance() {
    static ScanSchemaServiceImpl instance;
    return instance;
}

// 初始化服务日志。
bool ScanSchemaServiceImpl::Init(const char* logDirUtf8) {
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty() && !m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
        // 日志不可用不应阻断纯规则服务，因此只清空借用指针。
        m_logger = nullptr;
    }
    m_initialized = true;
    WriteLog(LogLevel::Info, "Init", "Scan schema service initialized");
    return true;
}

// 把输入配置转换为标准扫描流程 JSON。
ScanSchemaServiceResult ScanSchemaServiceImpl::BuildScanProcessJson(const char* configJsonUtf8,
                                                                   char* outputBuffer,
                                                                   int outputBufferSize) {
    if (!m_initialized) {
        return MakeResult(5, "ScanSchemaService is not initialized");
    }
    if (!configJsonUtf8 || !configJsonUtf8[0]) {
        return MakeResult(2, "configJsonUtf8 is empty");
    }

    // Qt JSON 解析器先验证语法和根类型，非法输入不会覆盖调用方原有状态。
    QJsonParseError parseError;
    const QJsonDocument inputDocument = QJsonDocument::fromJson(QByteArray(configJsonUtf8), &parseError);
    if (parseError.error != QJsonParseError::NoError || !inputDocument.isObject()) {
        WriteLog(LogLevel::Warning, "BuildScanProcessJson", parseError.errorString());
        return MakeResult(2, "Scan schema config must be a JSON object");
    }

    // 兼容直接传 config 和传完整 scanProcess 两种调用方式，输出统一使用 config 节点。
    const QJsonObject inputRoot = inputDocument.object();
    const QJsonObject config = inputRoot.value("config").isObject()
        ? inputRoot.value("config").toObject()
        : inputRoot;

    QJsonObject scanProcess;
    scanProcess.insert("schemaVersion", 1);
    scanProcess.insert("source", "ScanSchemaService");
    scanProcess.insert("config", config);
    scanProcess.insert("steps", BuildSteps(config));

    const QByteArray output = QJsonDocument(scanProcess).toJson(QJsonDocument::Compact);
    const int requiredSize = output.size() + 1;
    if (!outputBuffer || outputBufferSize < requiredSize) {
        return MakeResult(2, "Output buffer is too small", 0, requiredSize);
    }

    // 先清零再复制，调用方可直接把缓冲区当作以 '\0' 结尾的 UTF-8 文本。
    std::memset(outputBuffer, 0, static_cast<size_t>(outputBufferSize));
    std::memcpy(outputBuffer, output.constData(), static_cast<size_t>(output.size()));
    WriteLog(LogLevel::Info,
             "BuildScanProcessJson",
             QString("Scan process generated, steps=%1").arg(scanProcess.value("steps").toArray().size()));
    return MakeResult(0, "OK", output.size(), requiredSize);
}

// 返回模块代码版本。
const char* ScanSchemaServiceImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 清理服务状态。
void ScanSchemaServiceImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Scan schema service shutdown");
    if (m_logger) {
        m_logger->Flush();
    }
    m_logger = nullptr;
    m_logDir.clear();
    m_initialized = false;
}

// 判断指定颌是否包含种植体。
bool ScanSchemaServiceImpl::HasImplantTooth(const QJsonObject& config, bool maxilla) const {
    const QJsonArray items = config.value("scanPlan").toObject().value("items").toArray();
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const int tooth = item.value("tooth").toInt();
        const bool inRequestedJaw = maxilla
            ? (tooth >= 11 && tooth <= 28)
            : (tooth >= 31 && tooth <= 48);
        if (inRequestedJaw && item.value("type").toString() == "implant") {
            return true;
        }
    }
    return false;
}

// 判断指定颌是否使用临时牙咬合流程。
bool ScanSchemaServiceImpl::IsJawTemporary(const QString& occlusionType, bool maxilla) const {
    if (occlusionType == kOcclusionFullTemporary) {
        return true;
    }
    return maxilla
        ? occlusionType == kOcclusionMaxillaTemporary
        : occlusionType == kOcclusionMandibleTemporary;
}

// 根据配置生成步骤数组。
QJsonArray ScanSchemaServiceImpl::BuildSteps(const QJsonObject& config) const {
    QJsonArray steps;
    const bool maxillaDiffRod = config.value("maxillaDiffRod").toBool(false);
    const bool mandibleDiffRod = config.value("mandibleDiffRod").toBool(false);
    const bool maxillaSegmented = config.value("maxillaSegmentedRod").toBool(false);
    const bool mandibleSegmented = config.value("mandibleSegmentedRod").toBool(false);
    const bool maxillaImplant = HasImplantTooth(config, true);
    const bool mandibleImplant = HasImplantTooth(config, false);
    const QString occlusionType = config.value("occlusionType").toString(kOcclusionNatural);
    const bool maxillaTemporary = IsJawTemporary(occlusionType, true);
    const bool mandibleTemporary = IsJawTemporary(occlusionType, false);

    if (maxillaDiffRod) {
        if (maxillaTemporary) {
            AppendStep(&steps, "maxilla", "maxilla_natural");
        }
        AppendStep(&steps, "maxilla", "maxilla_diff_rod_1");
        if (maxillaSegmented) {
            AppendStep(&steps, "maxilla", "maxilla_diff_rod_2");
        }
        AppendStep(&steps, "maxilla", "maxilla_cuff");
    } else {
        AppendStep(&steps, "maxilla", "maxilla_natural");
        if (maxillaImplant) {
            if (maxillaTemporary) {
                AppendStep(&steps, "maxilla", "maxilla_cuff");
            }
            AppendStep(&steps, "maxilla", "maxilla_scanbody_1");
            if (maxillaSegmented) {
                AppendStep(&steps, "maxilla", "maxilla_scanbody_2");
            }
        }
    }

    const bool needExchange = !maxillaDiffRod
        && !mandibleDiffRod
        && !maxillaSegmented
        && !mandibleSegmented
        && !maxillaTemporary
        && !mandibleTemporary;
    if (needExchange) {
        AppendStep(&steps, "exchange", "data_exchange", true);
    }

    if (mandibleDiffRod) {
        if (mandibleTemporary) {
            AppendStep(&steps, "mandible", "mandible_natural");
        }
        AppendStep(&steps, "mandible", "mandible_diff_rod_1");
        if (mandibleSegmented) {
            AppendStep(&steps, "mandible", "mandible_diff_rod_2");
        }
        AppendStep(&steps, "mandible", "mandible_cuff");
    } else {
        AppendStep(&steps, "mandible", "mandible_natural");
        if (mandibleImplant) {
            if (mandibleTemporary) {
                AppendStep(&steps, "mandible", "mandible_cuff");
            }
            AppendStep(&steps, "mandible", "mandible_scanbody_1");
            if (mandibleSegmented) {
                AppendStep(&steps, "mandible", "mandible_scanbody_2");
            }
        }
    }

    AppendStep(&steps,
               "occlusion",
               occlusionType == kOcclusionRecord ? "bite_record" : "natural_occlusion");
    return steps;
}

// 追加稳定步骤编码。
void ScanSchemaServiceImpl::AppendStep(QJsonArray* steps,
                                       const QString& part,
                                       const QString& code,
                                       bool exchange) const {
    if (!steps) {
        return;
    }
    QJsonObject item;
    item.insert("part", part);
    item.insert("code", code);
    item.insert("labelKey", code);
    item.insert("exchange", exchange);
    item.insert("enabled", true);
    steps->append(item);
}

// 构造公共结果。
ScanSchemaServiceResult ScanSchemaServiceImpl::MakeResult(int errorCode,
                                                          const char* message,
                                                          int bytesWritten,
                                                          int requiredSize) const {
    ScanSchemaServiceResult result = {};
    result.errorCode = errorCode;
    result.bytesWritten = bytesWritten;
    result.requiredSize = requiredSize;
    CopyText(result.message, static_cast<int>(sizeof(result.message)), message);
    return result;
}

// 写服务日志。
void ScanSchemaServiceImpl::WriteLog(LogLevel level,
                                     const char* operation,
                                     const QString& content) const {
    if (!m_logger) {
        return;
    }
    const QByteArray contentUtf8 = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", contentUtf8.constData());
}

// 导出服务单例。
extern "C" MEYERSCAN_SCANSCHEMASERVICE_API IScanSchemaService* GetScanSchemaService() {
    return &ScanSchemaServiceImpl::Instance();
}

// 导出接口版本。
extern "C" MEYERSCAN_SCANSCHEMASERVICE_API int GetMeyerModuleApiVersion() {
    return MEYER_SCAN_SCHEMA_SERVICE_API_VERSION;
}

// 导出代码版本。
extern "C" MEYERSCAN_SCANSCHEMASERVICE_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
