#pragma once

#include "DataProcessUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>

#include "Logger.h"

class QLabel;
class QPushButton;
class DataProcessViewerWidget;
class vtkRenderer;

// Implementation of the data-processing stage UI.
class DataProcessUIImpl : public IDataProcessUI {
    Q_DECLARE_TR_FUNCTIONS(DataProcessUI)

public:
    static DataProcessUIImpl& Instance();

    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    bool SetSessionContextJson(const char* contextJsonUtf8) override;
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    void Activate() override;
    void DeactivateAndRelease() override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    DataProcessUIImpl() = default;
    ~DataProcessUIImpl() = default;
    DataProcessUIImpl(const DataProcessUIImpl&) = delete;
    DataProcessUIImpl& operator=(const DataProcessUIImpl&) = delete;

    struct ProcessStepInfo {
        QString part;
        QString code;
        QString label;
        bool exchange = false;
        bool enabled = true;
    };

    QWidget* CreateModelModeBar(QWidget* parent);
    void RefreshScanProcessButtons();
    QVector<ProcessStepInfo> ResolveScanProcessSteps() const;
    void SelectProcessStep(int index, const QString& reason);
    void RefreshScanProcessButtonStates();
    void UpdateDisplayedProcessData();
    QWidget* CreateProcessingToolBar(QWidget* parent);
    QWidget* CreateViewerArea(QWidget* parent);
    QWidget* CreateBottomStatusBar(QWidget* parent);
    QWidget* CreateHintPanel(QWidget* parent);
    void BuildPlaceholderScene();
    void RebuildStepPlaceholderScene();
    void EmitAction(int actionId, const QString& operation);
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QByteArray m_appDir;
    QByteArray m_logDir;
    ILogger* m_logger = nullptr;
    QWidget* m_root = nullptr;
    DataProcessViewerWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QWidget* m_modelModeBar = nullptr;
    QVector<QPushButton*> m_processButtons;
    QVector<ProcessStepInfo> m_processSteps;
    int m_currentStepIndex = -1;
    QByteArray m_contextJson;
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};
