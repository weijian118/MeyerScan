#pragma once

#include "SettingsUI.h"
#include "Logger.h"
#include "RuntimeDataCenter.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLibrary>
#include <QString>
#include <QStringList>

class QLabel;
class QPushButton;
class QStackedWidget;
class QWidget;

class ICalibration3DUI;
class ICalibrationColorUI;

using GetLoggerFunc = ILogger* (*)();
using GetCalibration3DUIFunc = ICalibration3DUI* (*)();
using GetCalibrationColorUIFunc = ICalibrationColorUI* (*)();
using GetRuntimeDataCenterFunc = IRuntimeDataCenter* (*)();

// SettingsUIImpl 是设置模块当前骨架实现。
// 设计目标是先跑通“多入口打开设置 -> 设置内切换分类 -> 打开校准子页 -> 返回/关闭”的流程。
class SettingsUIImpl : public ISettingsUI {
    Q_DECLARE_TR_FUNCTIONS(SettingsUI)

public:
    // 返回进程内单例，避免多个设置页面重复初始化校准模块和日志入口。
    static SettingsUIImpl& Instance();

    // 初始化设置模块路径、日志和可选校准模块。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 保存 MainExe 传入的设置动作回调。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;

    // 保存本次打开来源和校准入口可用性。
    void SetOpenContext(int openSource, bool allowCalibration) override;

    // 创建设置主页面。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 页面被 MainExe 销毁前清理内部缓存指针。
    void DestroyWidget() override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 清理设置模块缓存引用。
    void Shutdown() override;

private:
    SettingsUIImpl() = default;
    ~SettingsUIImpl() = default;
    SettingsUIImpl(const SettingsUIImpl&) = delete;
    SettingsUIImpl& operator=(const SettingsUIImpl&) = delete;

    // 动态加载 Logger 并缓存 ILogger 指针。
    void LoadLogger();

    // 动态加载三维校准和颜色校准模块。
    void LoadCalibrationModules();

    // 动态加载运行时数据中心，用于读取医生、诊所、技工所等只读快照。
    void LoadRuntimeDataCenter();

    // 写结构化日志；日志不可用时静默返回。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

    // 上报设置动作给 MainExe。
    void NotifyAction(int actionId, const QString& content);

    // 创建左侧导航按钮。
    QPushButton* CreateNavButton(QWidget* parent, const QString& text, int pageIndex);

    // 创建底部按钮。
    QPushButton* CreateFooterButton(QWidget* parent, const QString& text, int actionId);

    // 创建各设置分类页面。
    QWidget* CreateGeneralPage(QWidget* parent);
    QWidget* CreateInfoPage(QWidget* parent);
    QWidget* CreateCalibrationPage(QWidget* parent);
    QWidget* CreateCloudPage(QWidget* parent);
    QWidget* CreateScanPage(QWidget* parent);
    QWidget* CreateDataPage(QWidget* parent);
    QWidget* CreateAboutPage(QWidget* parent);

    // 创建信息管理页的单个标签页。
    QWidget* CreateInfoTabPage(QWidget* parent,
                               const QStringList& headers,
                               const QList<QStringList>& rows);

    // 从 RuntimeDataCenter 读取某个 domain 的 items 数组。
    QJsonArray LoadRuntimeItems(const char* domain);

    // 把医生快照转换成表格行。
    QList<QStringList> BuildDoctorRows(const QJsonArray& items) const;

    // 把诊所快照转换成表格行。
    QList<QStringList> BuildClinicRows(const QJsonArray& items) const;

    // 把技工所快照转换成表格行。
    QList<QStringList> BuildLabRows(const QJsonArray& items) const;

    // 从 JSON 对象读取第一个非空字段。
    QString FirstText(const QJsonObject& object, const QStringList& keys) const;

    // 创建校准卡片。
    QWidget* CreateCalibrationCard(QWidget* parent,
                                   const QString& title,
                                   const QString& description,
                                   int actionId);

    // 根据打开来源刷新校准入口；扫描重建来源打开设置时不允许进入校准流程。
    void ApplyCalibrationAvailability();

    // 把打开来源枚举转为日志可读文本。
    QString OpenSourceName() const;

    // 打开设置内部页面或嵌入的校准页面。
    void SwitchToPage(int pageIndex, const QString& pageName);
    void ShowEmbeddedCalibration(int actionId);
    void RestoreSettingsOverview();

private:
    // 应用安装目录和日志目录，均由 MainExe 显式传入。
    QString m_appDir;
    QString m_logDir;

    // Logger DLL 句柄和缓存后的日志接口。
    QLibrary m_loggerLibrary;
    ILogger* m_logger = nullptr;

    // RuntimeDataCenter DLL 句柄和缓存后的快照接口。
    QLibrary m_runtimeDataCenterLibrary;
    IRuntimeDataCenter* m_runtimeDataCenter = nullptr;

    // 校准模块句柄和接口指针。设置模块只嵌入 QWidget，不处理算法细节。
    QLibrary m_calibration3DLibrary;
    QLibrary m_calibrationColorLibrary;
    ICalibration3DUI* m_calibration3D = nullptr;
    ICalibrationColorUI* m_calibrationColor = nullptr;

    // 当前设置页面内的堆叠容器。它只用于设置模块内部分类切换，不代表主页面并列常驻。
    QStackedWidget* m_pages = nullptr;
    QLabel* m_titleLabel = nullptr;
    QWidget* m_calibrationNavButton = nullptr;
    QWidget* m_calibrationPage = nullptr;

    // 最近一次打开来源。设置关闭后由 MainExe 按来源刷新对应页面。
    int m_openSource = SettingsOpenSourceHome;

    // 校准入口是否允许显示/进入。扫描重建模块打开设置时必须为 false。
    bool m_allowCalibration = true;

    // MainExe 注册的动作回调。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionCallbackContext = nullptr;
};

