#pragma once

#include "DataProcessUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include "Logger.h"

class QLabel;
class QVTKWidget;
class vtkRenderer;

// Implementation of the data-processing stage UI.
// It owns the placeholder VTK viewer and tool buttons. Real editing/analysis
// services will be attached behind this boundary later.
class DataProcessUIImpl : public IDataProcessUI {
    Q_DECLARE_TR_FUNCTIONS(DataProcessUI)

public:
    // Returns the process-local singleton.
    static DataProcessUIImpl& Instance();

    // Initializes module paths and the cached logger pointer.
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // Creates the root QWidget for the data-processing stage.
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
    DataProcessUIImpl() = default;
    ~DataProcessUIImpl() = default;

    // QWidget and VTK pointers must never be copied.
    DataProcessUIImpl(const DataProcessUIImpl&) = delete;
    DataProcessUIImpl& operator=(const DataProcessUIImpl&) = delete;

    // Creates the model selector row.
    QWidget* CreateModelModeBar(QWidget* parent);

    // Creates the right-side processing tool bar.
    QWidget* CreateProcessingToolBar(QWidget* parent);

    // Creates the QVTK viewer widget.
    QWidget* CreateViewerArea(QWidget* parent);

    // Creates the bottom status/action bar.
    QWidget* CreateBottomStatusBar(QWidget* parent);

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
