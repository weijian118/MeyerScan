#pragma once

#include "ScanWorkflowUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include "Logger.h"

class QLabel;
class QVTKWidget;
class vtkRenderer;

// Implementation of the scan-stage UI.
// It creates a placeholder VTK viewer and scan controls; real device/algorithm
// integration will be added behind this boundary later.
class ScanWorkflowUIImpl : public IScanWorkflowUI {
    Q_DECLARE_TR_FUNCTIONS(ScanWorkflowUI)

public:
    // Returns the process-local singleton.
    static ScanWorkflowUIImpl& Instance();

    // Initializes module paths and the cached logger pointer.
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // Creates the root QWidget for the scan stage.
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // Stores lightweight session context.
    bool SetSessionContextJson(const char* contextJsonUtf8) override;

    // Registers a callback for user actions.
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;

    // Activates the page.
    void Activate() override;

    // Releases VTK viewer resources before page switch.
    void DeactivateAndRelease() override;

    // Returns module version.
    const char* GetModuleVersion() const override;

    // Shuts down the module and clears cached state.
    void Shutdown() override;

private:
    // Private constructor/destructor enforce singleton usage.
    ScanWorkflowUIImpl() = default;
    ~ScanWorkflowUIImpl() = default;

    // QWidget and VTK pointers must never be copied.
    ScanWorkflowUIImpl(const ScanWorkflowUIImpl&) = delete;
    ScanWorkflowUIImpl& operator=(const ScanWorkflowUIImpl&) = delete;

    // Creates the scan object selector row.
    QWidget* CreateScanModeBar(QWidget* parent);

    // Creates the right-side scan tool bar.
    QWidget* CreateRightToolBar(QWidget* parent);

    // Creates bottom scan controls.
    QWidget* CreateBottomControlBar(QWidget* parent);

    // Creates the hint/status panel.
    QWidget* CreateHintPanel(QWidget* parent);

    // Creates the QVTK viewer widget.
    QWidget* CreateViewerArea(QWidget* parent);

    // Builds a small placeholder VTK scene.
    void BuildPlaceholderScene();

    // Emits an action callback and writes a log entry.
    void EmitAction(int actionId, const QString& operation);

    // Writes a structured log line if logger is available.
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QByteArray m_appDir;
    QByteArray m_logDir;
    ILogger* m_logger = nullptr;
    QWidget* m_root = nullptr;
    QVTKWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    QLabel* m_statusLabel = nullptr;
    QByteArray m_contextJson;
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};

