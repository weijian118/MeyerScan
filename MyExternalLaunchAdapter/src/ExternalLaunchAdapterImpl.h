#pragma once

#include "ExternalLaunchAdapter.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include "Logger.h"

// ExternalLaunchAdapterImpl 是第三方拉起 JSON 归一化的具体实现。
// 它内部使用 Qt Core 的 JSON 能力解析文件，但公共 ABI 不暴露 Qt 类型。
// 输出结构固定为 source / patient / order / scanPlan，供 OrderCreateUI 直接填充界面。
class ExternalLaunchAdapterImpl : public IExternalLaunchAdapter {
public:
    // 返回进程内单例。
    // 适配器不保存订单业务状态，所以单例只缓存日志目录、应用目录和 Logger 指针。
    static ExternalLaunchAdapterImpl& Instance();

    // 复制应用目录和日志目录，并初始化日志指针。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 把第三方 JSON 文件转换为标准建单上下文 JSON。
    bool NormalizeOrderFile(const char* inputJsonPathUtf8,
                            const char* thirdPartyTypeUtf8,
                            char* outputJsonUtf8,
                            int outputSize,
                            ExternalLaunchResult* result) override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 清理模块缓存状态。
    void Shutdown() override;

private:
    // 构造函数私有化，保证外部只能通过 Instance 使用同一份轻量状态。
    ExternalLaunchAdapterImpl() = default;
    ~ExternalLaunchAdapterImpl() = default;

    ExternalLaunchAdapterImpl(const ExternalLaunchAdapterImpl&) = delete;
    ExternalLaunchAdapterImpl& operator=(const ExternalLaunchAdapterImpl&) = delete;

    // 组装标准建单上下文。
    // inputRoot 允许是完整 source/patient/order/scanPlan 结构，也允许是简单扁平测试结构。
    QJsonObject BuildNormalizedContext(const QJsonObject& inputRoot,
                                       const QString& thirdPartyType,
                                       ExternalLaunchResult* result) const;

    // 从 JSON 对象中读取字符串。
    // 为兼容第三方字段，数字和 bool 也会转换成字符串返回。
    QString ReadString(const QJsonObject& object,
                       const QString& key,
                       const QString& defaultValue = QString()) const;

    // 写入调用方结果结构。
    // 所有字符串都会转换为 UTF-8 并截断到固定 char 数组内。
    void SetResult(ExternalLaunchResult* result,
                   int errorCode,
                   const QString& message,
                   const QString& thirdPartyType = QString(),
                   const QString& thirdPartyName = QString(),
                   const QString& sourceSystem = QString(),
                   int requiredBufferSize = 0) const;

    // 把 QString 安全复制到固定大小 char 缓冲区。
    void CopyText(char* target, int targetSize, const QString& text) const;

    // 写模块日志，日志内容从 QString 转 UTF-8 后进入 Logger 公共接口。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // appDir/logDir 使用 QByteArray 保存 UTF-8 副本，避免保存调用方临时 const char*。
    QByteArray m_appDir;
    QByteArray m_logDir;
    // Logger 是进程级单例，本模块只缓存裸指针，不负责释放。
    ILogger* m_logger = nullptr;
};
