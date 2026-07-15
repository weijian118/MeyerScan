#pragma once

#include "ScanSchemaService.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "Logger.h"

// ScanSchemaServiceImpl 保存扫描步骤生成规则，不创建 QWidget，也不访问数据库。
class ScanSchemaServiceImpl : public IScanSchemaService {
public:
    // 返回进程内单例，保证所有调用方使用同一套规则和日志入口。
    static ScanSchemaServiceImpl& Instance();

    // 缓存日志目录和 Logger 借用指针。
    bool Init(const char* logDirUtf8) override;

    // 解析配置、生成步骤并复制到调用方缓冲区。
    ScanSchemaServiceResult BuildScanProcessJson(const char* configJsonUtf8,
                                                 char* outputBuffer,
                                                 int outputBufferSize) override;

    // 返回集中维护的模块版本。
    const char* GetModuleVersion() const override;

    // 清理借用引用，不关闭进程级 Logger。
    void Shutdown() override;

private:
    ScanSchemaServiceImpl() = default;
    ~ScanSchemaServiceImpl() = default;
    ScanSchemaServiceImpl(const ScanSchemaServiceImpl&) = delete;
    ScanSchemaServiceImpl& operator=(const ScanSchemaServiceImpl&) = delete;

    // 从 scanPlan.items 判断指定颌是否包含种植体牙位。
    bool HasImplantTooth(const QJsonObject& config, bool maxilla) const;

    // 根据咬合类型判断指定颌是否需要临时牙流程。
    bool IsJawTemporary(const QString& occlusionType, bool maxilla) const;

    // 根据标准配置生成有序步骤数组。
    QJsonArray BuildSteps(const QJsonObject& config) const;

    // 追加一个只包含稳定编码的步骤，不把翻译后的显示文本写进数据合同。
    void AppendStep(QJsonArray* steps,
                    const QString& part,
                    const QString& code,
                    bool exchange = false) const;

    // 构造统一结果并复制短消息。
    ScanSchemaServiceResult MakeResult(int errorCode,
                                       const char* message,
                                       int bytesWritten = 0,
                                       int requiredSize = 0) const;

    // 写结构化日志；Logger 缺失时静默降级。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QByteArray m_logDir;            // 调用方传入的日志目录 UTF-8 副本。
    ILogger* m_logger = nullptr;    // 借用的进程级 Logger 单例。
    bool m_initialized = false;     // 标记 Init 是否完成。
};
