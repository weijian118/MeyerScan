#pragma once

#include "CalibrationColorUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QLibrary>

#include "Logger.h"

class IUIComponents;
class QPushButton;

// CalibrationColorUIImpl 是颜色校准 UI 的骨架实现。
// 当前先提供可嵌入界面和日志链路，后续再替换为真实颜色采集、计算和结果确认流程。
class CalibrationColorUIImpl : public ICalibrationColorUI {
    Q_DECLARE_TR_FUNCTIONS(CalibrationColorUI)

public:
    // 返回进程内单例。
    static CalibrationColorUIImpl& Instance();

    // 初始化路径和日志。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 创建颜色校准界面。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 返回模块版本。
    const char* GetModuleVersion() const override;

    // 关闭模块并清理内部缓存。
    void Shutdown() override;

private:
    // 构造/析构私有化，确保实例生命周期由 Instance() 统一管理。
    CalibrationColorUIImpl() = default;
    ~CalibrationColorUIImpl() = default;

    // 禁止拷贝，避免 QWidget 和 ILogger 指针被复制后生命周期不清。
    CalibrationColorUIImpl(const CalibrationColorUIImpl&) = delete;
    CalibrationColorUIImpl& operator=(const CalibrationColorUIImpl&) = delete;

    // 写颜色校准模块日志。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

    // 动态加载共享 UI 组件；加载失败时颜色校准页面使用本地 Qt 控件降级。
    void LoadUIComponents();

    // 创建浏览页 Search 同视觉角色的主按钮，并统一补充颜色校准按钮属性。
    QPushButton* CreatePrimaryButton(const QString& text, QWidget* parent) const;

    // 关闭当前颜色校准宿主窗口；兼容独立测试窗口和设置模块遮罩弹窗。
    void CloseHostWindow(QWidget* root, const char* operation);

private:
    // MeyerScan.exe 所在目录。
    QByteArray m_appDir;

    // 统一日志目录。
    QByteArray m_logDir;

    // 缓存日志接口，避免重复获取单例。
    ILogger* m_logger = nullptr;

    // UIComponents 使用 QLibrary 动态加载，颜色校准 DLL 不形成静态导入依赖。
    QLibrary m_uiComponentsLibrary;

    // 借用的共享 UI 组件接口，由 UIComponents DLL 内部管理生命周期。
    IUIComponents* m_uiComponents = nullptr;

    // 当前根界面的弱引用，真实释放由调用方/Qt 父子关系负责。
    QWidget* m_root = nullptr;
};
