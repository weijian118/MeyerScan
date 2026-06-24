#pragma once

#include "Calibration3DUI.h"

#include <QByteArray>
#include <QCoreApplication>

#include "Logger.h"

// Calibration3DUIImpl 是三维校准 UI 的当前骨架实现。
// 它负责创建占位界面、写结构化日志，并为后续接入算法/设备 DLL 预留生命周期。
class Calibration3DUIImpl : public ICalibration3DUI {
    Q_DECLARE_TR_FUNCTIONS(Calibration3DUI)

public:
    // 返回进程内单例，避免同一个 DLL 内存在多个校准状态对象。
    static Calibration3DUIImpl& Instance();

    // 初始化三维校准模块的路径和日志。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 创建三维校准 QWidget。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 返回模块版本。
    const char* GetModuleVersion() const override;

    // 清理模块缓存状态。
    void Shutdown() override;

private:
    // 构造/析构私有化，保证外部只能通过 Instance() 获取对象。
    Calibration3DUIImpl() = default;
    ~Calibration3DUIImpl() = default;

    // 禁止拷贝，避免 QWidget 指针和日志指针被复制后产生双重管理风险。
    Calibration3DUIImpl(const Calibration3DUIImpl&) = delete;
    Calibration3DUIImpl& operator=(const Calibration3DUIImpl&) = delete;

    // 写三维校准模块日志。
    // operation 使用稳定英文 key，content 当前为英文，便于后续日志检索。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // MeyerScan.exe 所在目录，后续用于加载图标、算法 DLL 或配置文件。
    QByteArray m_appDir;

    // 统一 logs 目录，所有模块必须由 MainExe 传入，不能使用 currentPath 推导。
    QByteArray m_logDir;

    // 缓存日志接口指针，避免每次写日志时重复 GetLogger()。
    ILogger* m_logger = nullptr;

    // 当前创建的根界面弱引用；真实销毁由 Qt 父子对象树或调用方负责。
    QWidget* m_root = nullptr;
};
