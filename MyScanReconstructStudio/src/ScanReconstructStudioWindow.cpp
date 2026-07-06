#include "ScanReconstructStudioWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// This value is written into the structured log module field.
const char* Name = "ScanReconstructStudio";

// Code version returned by GetMeyerModuleVersion(). Keep it in sync with Version.rc.
const char* Version = "ScanReconstructStudio v0.1.0 (2026-07-05)";
}

// Plain C function pointer types resolved from child DLLs.
typedef IScanWorkflowUI* (*GetScanWorkflowUIFunc)();
typedef IDataProcessUI* (*GetDataProcessUIFunc)();
}

// Constructs the shell window.
ScanReconstructStudioWindow::ScanReconstructStudioWindow(const QString& appDir,
                                                         const QString& logDir,
                                                         const QByteArray& contextJson,
                                                         QWidget* parent)
    : QMainWindow(parent),
      m_appDir(appDir),
      m_logDir(logDir),
      m_contextJson(contextJson),
      m_scanLibrary(QDir(appDir).filePath("MeyerScan_ScanWorkflowUI.dll")),
      m_processLibrary(QDir(appDir).filePath("MeyerScan_DataProcessUI.dll")) {
    // Visible text uses English source strings so qm files can translate later.
    setWindowTitle(tr("Scan Reconstruct Studio"));
    setMinimumSize(1280, 760);
}

// Releases active page resources before unloading child DLLs.
ScanReconstructStudioWindow::~ScanReconstructStudioWindow() {
    // Order matters: child widgets must release VTK resources before DLL unload.
    ReleaseCurrentStepResources();
    UnloadModules();
}

// Initializes logger, child modules, and default page.
bool ScanReconstructStudioWindow::Initialize() {
    // Logger is initialized first so later failures are recorded.
    m_logger = GetLogger();
    if (m_logger) {
        m_logger->Init(m_logDir.toUtf8().constData(), LogLevel::Info);
    }
    WriteLog(LogLevel::Info, "Initialize", "ScanReconstructStudio initializing");

    // Load both modules up front; page widgets are still created lazily on switch.
    if (!LoadScanModule()) {
        return false;
    }
    if (!LoadDataProcessModule()) {
        return false;
    }

    // Build the shell and enter scan mode.
    BuildShellUi();
    SwitchToStep(StepScan);
    WriteLog(LogLevel::Info, "Initialize", "ScanReconstructStudio initialized");
    return true;
}

// Runs a simple headless smoke path.
bool ScanReconstructStudioWindow::RunSmoke() {
    // Reuse the real initialize path so dynamic loading and widget creation are covered.
    if (!Initialize()) {
        return false;
    }

    // Switch forward and back to verify page release/recreate behavior.
    SwitchToStep(StepDataProcess);
    SwitchToStep(StepScan);
    WriteLog(LogLevel::Info, "RunSmoke", "ScanReconstructStudio smoke passed");
    return true;
}

// Creates the shell frame.
void ScanReconstructStudioWindow::BuildShellUi() {
    auto* central = new QWidget(this);
    central->setObjectName("ScanReconstructStudioRoot");
    central->setStyleSheet(
        "#ScanReconstructStudioRoot{background:#dfe4ea;}"
        "QLabel{color:#1f2b36;font-size:13px;}"
        "QPushButton{border:0;border-radius:4px;background:#f7f9fb;color:#273645;padding:8px 12px;min-height:28px;}"
        "QPushButton:hover{background:#eef3f6;}"
        "QPushButton[active=\"true\"]{background:#007d68;color:white;font-weight:600;}"
        "QFrame[bar=\"true\"]{background:#eef2f5;border-bottom:1px solid #c5d0d8;}"
    );

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Top bar is owned by the shell because both child pages share the same flow chrome.
    auto* topBar = new QFrame(central);
    topBar->setProperty("bar", true);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(18, 12, 18, 12);
    topLayout->setSpacing(20);
    topLayout->addWidget(CreateTopStepBar(topBar), 1);
    topLayout->addWidget(CreateWindowToolBar(topBar), 0);
    layout->addWidget(topBar, 0);

    // The stack holds only the active page; old pages are removed and deleted on switch.
    m_stack = new QStackedWidget(central);
    m_stack->setObjectName("ScanReconstructStudioStack");
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);
}

// Creates the top stage navigation row.
QWidget* ScanReconstructStudioWindow::CreateTopStepBar(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* backButton = new QPushButton(tr("Back"), widget);
    auto* scanButton = new QPushButton(tr("Scan"), widget);
    auto* processButton = new QPushButton(tr("Data Processing"), widget);
    m_stepLabel = new QLabel(tr("Scan"), widget);

    QObject::connect(scanButton, &QPushButton::clicked, [this]() {
        SwitchToStep(StepScan);
    });
    QObject::connect(processButton, &QPushButton::clicked, [this]() {
        SwitchToStep(StepDataProcess);
    });
    QObject::connect(backButton, &QPushButton::clicked, [this]() {
        close();
    });

    layout->addWidget(backButton, 0);
    layout->addStretch(1);
    layout->addWidget(scanButton, 0);
    layout->addWidget(processButton, 0);
    layout->addStretch(1);
    layout->addWidget(m_stepLabel, 0);
    return widget;
}

// Creates the top-right shell tool row.
QWidget* ScanReconstructStudioWindow::CreateWindowToolBar(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const QStringList tools = {
        tr("Upload"),
        tr("Crop"),
        tr("Settings"),
        tr("Folder"),
        tr("Home"),
    };

    for (const QString& text : tools) {
        auto* button = new QPushButton(text, widget);
        button->setMinimumWidth(68);
        QObject::connect(button, &QPushButton::clicked, [this, text]() {
            WriteLog(LogLevel::Info, "TopToolClicked", QString("Tool clicked: %1").arg(text));
        });
        layout->addWidget(button);
    }
    return widget;
}

// Switches stages and releases the previous stage resources.
void ScanReconstructStudioWindow::SwitchToStep(StudioStep step) {
    // If the requested page is already active, only refresh its label.
    if (m_currentStep == step && m_pages.contains(step)) {
        if (m_stepLabel) {
            m_stepLabel->setText(step == StepScan ? tr("Scan") : tr("Data Processing"));
        }
        return;
    }

    // Release the previous QVTK/OpenGL page before creating the next one.
    ReleaseCurrentStepResources();
    m_currentStep = step;

    QWidget* page = nullptr;
    if (step == StepScan) {
        page = CreateScanPage();
    } else {
        page = CreateDataProcessPage();
    }

    if (!page || !m_stack) {
        WriteLog(LogLevel::Error, "SwitchToStep", "Failed to create step page");
        return;
    }

    // The stack only contains the active heavy page after ReleaseCurrentStepResources().
    m_stack->addWidget(page);
    m_stack->setCurrentWidget(page);
    m_pages.insert(step, page);

    if (m_stepLabel) {
        m_stepLabel->setText(step == StepScan ? tr("Scan") : tr("Data Processing"));
    }

    WriteLog(LogLevel::Info, "SwitchToStep",
             QString("Current step: %1").arg(step == StepScan ? "scan" : "data-process"));
}

// Dynamically loads the scan UI DLL.
bool ScanReconstructStudioWindow::LoadScanModule() {
    if (!m_scanLibrary.load()) {
        WriteLog(LogLevel::Error, "LoadScanModule", m_scanLibrary.errorString());
        return false;
    }

    GetScanWorkflowUIFunc getter =
        reinterpret_cast<GetScanWorkflowUIFunc>(m_scanLibrary.resolve("GetScanWorkflowUI"));
    if (!getter) {
        WriteLog(LogLevel::Error, "LoadScanModule", "GetScanWorkflowUI not found");
        return false;
    }

    m_scanModule = getter();
    if (!m_scanModule) {
        WriteLog(LogLevel::Error, "LoadScanModule", "Scan module instance is null");
        return false;
    }

    m_scanModule->Init(m_appDir.toUtf8().constData(), m_logDir.toUtf8().constData());
    m_scanModule->SetSessionContextJson(m_contextJson.constData());
    m_scanModule->SetActionCallback(&ScanReconstructStudioWindow::OnChildAction, this);
    WriteLog(LogLevel::Info, "LoadScanModule", m_scanModule->GetModuleVersion());
    return true;
}

// Dynamically loads the data-processing UI DLL.
bool ScanReconstructStudioWindow::LoadDataProcessModule() {
    if (!m_processLibrary.load()) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", m_processLibrary.errorString());
        return false;
    }

    GetDataProcessUIFunc getter =
        reinterpret_cast<GetDataProcessUIFunc>(m_processLibrary.resolve("GetDataProcessUI"));
    if (!getter) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "GetDataProcessUI not found");
        return false;
    }

    m_processModule = getter();
    if (!m_processModule) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "Data process module instance is null");
        return false;
    }

    m_processModule->Init(m_appDir.toUtf8().constData(), m_logDir.toUtf8().constData());
    m_processModule->SetSessionContextJson(m_contextJson.constData());
    m_processModule->SetActionCallback(&ScanReconstructStudioWindow::OnChildAction, this);
    WriteLog(LogLevel::Info, "LoadDataProcessModule", m_processModule->GetModuleVersion());
    return true;
}

// Creates the scan page through the child DLL.
QWidget* ScanReconstructStudioWindow::CreateScanPage() {
    if (!m_scanModule) {
        return nullptr;
    }
    QWidget* page = m_scanModule->CreateWidget(m_stack);
    m_scanModule->Activate();
    return page;
}

// Creates the data-processing page through the child DLL.
QWidget* ScanReconstructStudioWindow::CreateDataProcessPage() {
    if (!m_processModule) {
        return nullptr;
    }
    QWidget* page = m_processModule->CreateWidget(m_stack);
    m_processModule->Activate();
    return page;
}

// Releases the active page and its heavy resources.
void ScanReconstructStudioWindow::ReleaseCurrentStepResources() {
    QWidget* page = m_pages.value(m_currentStep, nullptr);
    if (!page) {
        return;
    }

    // Tell the child module to release QVTK/OpenGL resources before widget deletion.
    if (m_currentStep == StepScan && m_scanModule) {
        m_scanModule->DeactivateAndRelease();
    }
    if (m_currentStep == StepDataProcess && m_processModule) {
        m_processModule->DeactivateAndRelease();
    }

    if (m_stack) {
        m_stack->removeWidget(page);
    }
    m_pages.remove(m_currentStep);
    page->deleteLater();

    // Force deferred deletes during smoke tests so the release path is observable.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

// Shuts down child modules and unloads DLL handles.
void ScanReconstructStudioWindow::UnloadModules() {
    if (m_scanModule) {
        m_scanModule->Shutdown();
        m_scanModule = nullptr;
    }
    if (m_processModule) {
        m_processModule->Shutdown();
        m_processModule = nullptr;
    }

    if (m_scanLibrary.isLoaded()) {
        m_scanLibrary.unload();
    }
    if (m_processLibrary.isLoaded()) {
        m_processLibrary.unload();
    }

    if (m_logger) {
        m_logger->Flush();
        m_logger = nullptr;
    }
}

// Static callback entry passed to child modules.
void ScanReconstructStudioWindow::OnChildAction(void* context, int actionId) {
    auto* self = static_cast<ScanReconstructStudioWindow*>(context);
    if (self) {
        self->HandleChildAction(actionId);
    }
}

// Handles child module actions.
void ScanReconstructStudioWindow::HandleChildAction(int actionId) {
    WriteLog(LogLevel::Info, "HandleChildAction",
             QString("Child action: %1, current step: %2").arg(actionId).arg(m_currentStep));

    // First version only wires the stage flow: scan complete -> processing.
    if (m_currentStep == StepScan &&
        (actionId == ScanWorkflowActionComplete || actionId == ScanWorkflowActionNext)) {
        SwitchToStep(StepDataProcess);
        return;
    }

    // Data processing can go back to scanning through the stable action id.
    if (m_currentStep == StepDataProcess && actionId == DataProcessActionPrevious) {
        SwitchToStep(StepScan);
        return;
    }
}

// Writes a structured log entry if logger is initialized.
void ScanReconstructStudioWindow::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }
    const QByteArray bytes = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation ? operation : "", "", "", "", bytes.constData());
}

// Unified version export used by runtime version-list collection.
extern "C" __declspec(dllexport) const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
