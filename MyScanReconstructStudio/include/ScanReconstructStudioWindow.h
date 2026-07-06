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

// Shell window for ScanReconstructStudio.exe.
// The shell owns stage navigation, dynamic DLL loading, page switching, and
// heavy-resource release. Scan and data-processing business details stay in
// their own DLLs.
class ScanReconstructStudioWindow : public QMainWindow {
public:
    // Receives the executable directory, log directory, and optional session JSON.
    explicit ScanReconstructStudioWindow(const QString& appDir,
                                         const QString& logDir,
                                         const QByteArray& contextJson,
                                         QWidget* parent = nullptr);

    // Releases active page resources before unloading child DLLs.
    ~ScanReconstructStudioWindow() override;

    // Loads child modules, builds the shell UI, and enters the scan stage.
    bool Initialize();

    // Used by automated smoke checks: load both pages and switch once.
    bool RunSmoke();

private:
    // Work stages hosted by ScanReconstructStudio.exe.
    enum StudioStep {
        StepScan = 1,
        StepDataProcess = 2,
    };

    // Creates the shell frame.
    void BuildShellUi();

    // Creates the top stage navigation row.
    QWidget* CreateTopStepBar(QWidget* parent);

    // Creates the top-right shell tool row.
    QWidget* CreateWindowToolBar(QWidget* parent);

    // Switches to the requested stage and releases the previous stage resources.
    void SwitchToStep(StudioStep step);

    // Dynamically loads MeyerScan_ScanWorkflowUI.dll.
    bool LoadScanModule();

    // Dynamically loads MeyerScan_DataProcessUI.dll.
    bool LoadDataProcessModule();

    // Creates the scan page through the child DLL interface.
    QWidget* CreateScanPage();

    // Creates the data-processing page through the child DLL interface.
    QWidget* CreateDataProcessPage();

    // Releases the current page and its VTK/OpenGL resources.
    void ReleaseCurrentStepResources();

    // Shuts down child modules and unloads DLL handles.
    void UnloadModules();

    // Static callback entry used by child modules.
    static void OnChildAction(void* context, int actionId);

    // Instance handler for child actions.
    void HandleChildAction(int actionId);

    // Writes a structured log line if logger is available.
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
