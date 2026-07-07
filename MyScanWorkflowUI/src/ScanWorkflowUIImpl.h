#pragma once

#include "ScanWorkflowUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>

#include "Logger.h"

class QLabel;
class QPushButton;
class ScanWorkflowViewerWidget;
class vtkRenderer;

// Implementation of the scan-stage UI.
class ScanWorkflowUIImpl : public IScanWorkflowUI {
    Q_DECLARE_TR_FUNCTIONS(ScanWorkflowUI)

public:
    static ScanWorkflowUIImpl& Instance();

    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    bool SetSessionContextJson(const char* contextJsonUtf8) override;
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    void Activate() override;
    void DeactivateAndRelease() override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    ScanWorkflowUIImpl() = default;
    ~ScanWorkflowUIImpl() = default;
    ScanWorkflowUIImpl(const ScanWorkflowUIImpl&) = delete;
    ScanWorkflowUIImpl& operator=(const ScanWorkflowUIImpl&) = delete;

    struct ScanProcessStepInfo {
        QString part;
        QString code;
        QString label;
        bool exchange = false;
        bool enabled = true;
    };

    QWidget* CreateScanModeBar(QWidget* parent);
    void RefreshScanProcessButtons();
    QVector<ScanProcessStepInfo> ResolveScanProcessSteps() const;
    void SelectScanProcessStep(int index, const QString& reason);
    void RefreshScanProcessButtonStates();
    void UpdateDisplayedScanData();
    QWidget* CreateRightToolBar(QWidget* parent);
    QWidget* CreateBottomControlBar(QWidget* parent);
    QWidget* CreateHintPanel(QWidget* parent);
    QWidget* CreateViewerArea(QWidget* parent);
    void BuildPlaceholderScene();
    void RebuildStepPlaceholderScene();
    void EmitAction(int actionId, const QString& operation);
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QByteArray m_appDir;
    QByteArray m_logDir;
    ILogger* m_logger = nullptr;
    QWidget* m_root = nullptr;
    ScanWorkflowViewerWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_scanModeBar = nullptr;
    QVector<QPushButton*> m_scanProcessButtons;
    QVector<ScanProcessStepInfo> m_scanProcessSteps;
    int m_currentStepIndex = -1;
    QByteArray m_contextJson;
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};

