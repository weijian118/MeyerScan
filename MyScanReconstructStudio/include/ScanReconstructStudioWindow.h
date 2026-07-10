#pragma once

#include <QByteArray>
#include <QHash>
#include <QLibrary>
#include <QMainWindow>
#include <QString>

#include "DataProcessUI.h"
#include "Logger.h"
#include "ScanWorkflowUI.h"

class QLabel;
class QStackedWidget;
class QPushButton;

// ScanReconstructStudio 的壳窗口。
// 壳窗口只负责阶段导航、动态加载 DLL、页面切换和重资源释放；
// 扫描、数据处理的业务细节继续留在各自 DLL 内。
class ScanReconstructStudioWindow : public QMainWindow {
public:
    // 接收程序目录、日志目录和可选会话 JSON。
    explicit ScanReconstructStudioWindow(const QString& appDir,
                                         const QString& logDir,
                                         const QByteArray& contextJson,
                                         QWidget* parent = nullptr);

    // 析构前释放活动页面资源。
    ~ScanReconstructStudioWindow() override;

    // 加载子模块，创建壳 UI，并进入扫描阶段。
    bool Initialize();

    // 自动化烟测入口：加载两个页面并切换一次。
    bool RunSmoke();

private:
    // 壳窗口承载的工作阶段。
    enum StudioStep {
        StepScan = 1,
        StepDataProcess = 2,
    };

    // 创建壳窗口框架。
    void BuildShellUi();

    // 创建顶部阶段导航行。
    QWidget* CreateTopStepBar(QWidget* parent);

    // 创建右上角工具按钮行。
    QWidget* CreateWindowToolBar(QWidget* parent);

    // 切换到目标阶段，并释放上一个阶段的资源。
    void SwitchToStep(StudioStep step);

    // 动态加载 MeyerScan_ScanWorkflowUI.dll。
    bool LoadScanModule();

    // 动态加载 MeyerScan_DataProcessUI.dll。
    bool LoadDataProcessModule();

    // 通过子 DLL 接口创建扫描页面。
    QWidget* CreateScanPage();

    // 通过子 DLL 接口创建数据处理页面。
    QWidget* CreateDataProcessPage();

    // 释放当前页面和它持有的 VTK/OpenGL 资源。
    void ReleaseCurrentStepResources();

    // 关闭子模块并清理 DLL 句柄。
    void UnloadModules();

    // 子模块使用的静态回调入口。
    static void OnChildAction(void* context, int actionId);

    // 实例级子模块动作处理。
    void HandleChildAction(int actionId);

    // 写结构化日志。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QString m_appDir;
    QString m_logDir;
    QByteArray m_contextJson;

    ILogger* m_logger = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_stepLabel = nullptr;
    StudioStep m_currentStep = StepScan;

    QLibrary m_scanLibrary;
    QLibrary m_processLibrary;
    IScanWorkflowUI* m_scanModule = nullptr;
    IDataProcessUI* m_processModule = nullptr;

    QHash<int, QWidget*> m_pages;
};
