#include "ScanWorkflowUIImpl.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>

#include <QVTKWidget.h>
#include <vtkActor.h>
#include <vtkAutoInit.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>

#include <opencv2/core.hpp>

VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);

namespace {
namespace ModuleInfo {
// This value is written into the structured log module field.
const char* Name = "MeyerScan_ScanWorkflowUI";

// Code version returned by GetModuleVersion(). Keep it in sync with Version.rc.
const char* Version = "MeyerScan_ScanWorkflowUI v0.1.0 (2026-07-05)";
}

const char* kPageBackground = "#dfe4ea";
const char* kPanelBackground = "#ffffff";
const char* kPrimaryColor = "#007d68";
const char* kMutedText = "#687785";
const char* kBorderColor = "#cbd5dd";
}

// Returns the scan UI singleton owned by this DLL.
ScanWorkflowUIImpl& ScanWorkflowUIImpl::Instance() {
    // A local static avoids cross-DLL ownership and is constructed once per process.
    static ScanWorkflowUIImpl instance;
    return instance;
}

// Initializes scan-stage paths and logger.
bool ScanWorkflowUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // Copy incoming UTF-8 buffers so the caller can release temporary values safely.
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // Cache the logger pointer once; later writes reuse this member.
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        // Logger writes into the log directory passed by the shell.
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    // This lightweight object proves that OpenCV 3.3 include/link paths work.
    cv::Size buildProbe(1920, 1080);
    WriteLog(LogLevel::Info, "Init",
             QString("Scan workflow UI initialized, OpenCV probe %1x%2")
                 .arg(buildProbe.width)
                 .arg(buildProbe.height));
    return true;
}

// Creates the root widget for the scan stage.
QWidget* ScanWorkflowUIImpl::CreateWidget(QWidget* parent) {
    // Release any previous QVTKWidget before creating a new page.
    DeactivateAndRelease();

    // The shell/test host owns the returned QWidget through the Qt parent tree.
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanScanWorkflowUIRoot");
    root->setMinimumSize(1280, 720);
    root->setStyleSheet(QString(
        "#MeyerScanScanWorkflowUIRoot{background:%1;}"
        "QLabel{color:#1f2b36;font-size:13px;}"
        "QLabel[muted=\"true\"]{color:%2;}"
        "QPushButton{border:0;border-radius:4px;background:#f7f9fb;color:#273645;padding:8px 12px;min-height:28px;}"
        "QPushButton:hover{background:#eef3f6;}"
        "QPushButton[primary=\"true\"]{background:%3;color:#ffffff;font-weight:600;}"
        "QPushButton[primary=\"true\"]:hover{background:#009176;}"
        "QFrame[panel=\"true\"]{background:%4;border:1px solid %5;border-radius:6px;}"
        "QFrame[toolPanel=\"true\"]{background:%4;border-radius:6px;}"
    ).arg(kPageBackground, kMutedText, kPrimaryColor, kPanelBackground, kBorderColor));

    // Main layout: mode bar, central VTK area, and bottom controls.
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // The mode bar matches the scan-object selector in the reference product.
    mainLayout->addWidget(CreateScanModeBar(root), 0, Qt::AlignHCenter);

    // Central area: hint panel, expandable VTK viewer, and right-side tools.
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);
    centerLayout->addWidget(CreateHintPanel(root), 0, Qt::AlignBottom);
    centerLayout->addWidget(CreateViewerArea(root), 1);
    centerLayout->addWidget(CreateRightToolBar(root), 0, Qt::AlignVCenter);
    mainLayout->addLayout(centerLayout, 1);

    // Bottom controls expose stage-level actions only.
    mainLayout->addWidget(CreateBottomControlBar(root), 0, Qt::AlignHCenter);

    // Keep a non-owning pointer; Qt parent ownership destroys the actual widget.
    m_root = root;
    Activate();
    WriteLog(LogLevel::Info, "CreateWidget", "Scan workflow widget created");
    return root;
}

// Stores lightweight session context as UTF-8 JSON.
bool ScanWorkflowUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    // Keep JSON opaque here; the real scanner can later resolve order/session IDs.
    m_contextJson = QByteArray(contextJsonUtf8 ? contextJsonUtf8 : "");
    WriteLog(LogLevel::Info, "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

// Registers the shell callback.
void ScanWorkflowUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // Store a plain C callback to avoid exposing QObject or signal types across DLLs.
    m_actionCallback = callback;
    m_actionContext = context;
}

// Activates the scan stage.
void ScanWorkflowUIImpl::Activate() {
    // Real implementation will restore device/session state here.
    if (m_statusLabel) {
        m_statusLabel->setText(tr("Ready for scan"));
    }
    WriteLog(LogLevel::Info, "Activate", "Scan workflow activated");
}

// Releases heavy VTK/OpenGL resources before leaving the scan stage.
void ScanWorkflowUIImpl::DeactivateAndRelease() {
    // QVTKWidget owns native OpenGL resources, so hiding it is not enough.
    if (m_vtkWidget) {
        // Detach the renderer first to avoid stale render-window references.
        if (m_renderer && m_vtkWidget->GetRenderWindow()) {
            m_vtkWidget->GetRenderWindow()->RemoveRenderer(m_renderer);
        }
        // deleteLater is safe when called during Qt event processing.
        m_vtkWidget->setParent(nullptr);
        m_vtkWidget->deleteLater();
        m_vtkWidget = nullptr;
    }

    // vtkRenderer::New() uses reference counting and must be paired with Delete().
    if (m_renderer) {
        m_renderer->Delete();
        m_renderer = nullptr;
    }

    WriteLog(LogLevel::Info, "DeactivateAndRelease", "Scan VTK resources released");
}

// Returns the module code version.
const char* ScanWorkflowUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// Shuts down the module and clears cached state.
void ScanWorkflowUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Scan workflow shutdown");
    DeactivateAndRelease();
    if (m_logger) {
        // Flush is currently a compatibility no-op, but keeping it documents intent.
        m_logger->Flush();
    }
    m_root = nullptr;
    m_statusLabel = nullptr;
    m_contextJson.clear();
    m_actionCallback = nullptr;
    m_actionContext = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// Creates the scan-object selector bar.
QWidget* ScanWorkflowUIImpl::CreateScanModeBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);
    frame->setMinimumHeight(86);

    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(16);

    const QStringList labels = {
        tr("True Color"),
        tr("Upper Jaw"),
        tr("Occlusion"),
        tr("Lower Jaw"),
        tr("Implant"),
        tr("Denture"),
    };

    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels.at(i), frame);
        button->setMinimumSize(92, 52);
        button->setProperty("primary", i == 0);
        QObject::connect(button, &QPushButton::clicked, [this]() {
            // The UI reports intent; scan-plan state will live behind the service layer.
            EmitAction(ScanWorkflowActionJawModeChanged, "JawModeChanged");
        });
        layout->addWidget(button);
    }
    return frame;
}

// Creates the right-side scan toolbar.
QWidget* ScanWorkflowUIImpl::CreateRightToolBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("toolPanel", true);
    frame->setFixedWidth(86);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    const QStringList tools = {
        tr("True Color"),
        tr("Model"),
        tr("Color"),
        tr("Edit"),
    };

    for (const QString& tool : tools) {
        auto* button = new QPushButton(tool, frame);
        button->setMinimumHeight(54);
        QObject::connect(button, &QPushButton::clicked, [this, tool]() {
            // Tool switching changes scan-page state only; algorithms remain outside UI.
            EmitAction(ScanWorkflowActionToolChanged, QString("ToolChanged:%1").arg(tool));
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return frame;
}

// Creates the bottom scan control bar.
QWidget* ScanWorkflowUIImpl::CreateBottomControlBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);

    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(18);

    auto* playButton = new QPushButton(tr("Start / Pause"), frame);
    auto* completeButton = new QPushButton(tr("Complete"), frame);
    auto* deleteButton = new QPushButton(tr("Delete"), frame);
    completeButton->setProperty("primary", true);

    QObject::connect(playButton, &QPushButton::clicked, [this]() {
        EmitAction(ScanWorkflowActionStartPause, "StartPause");
    });
    QObject::connect(completeButton, &QPushButton::clicked, [this]() {
        EmitAction(ScanWorkflowActionComplete, "Complete");
    });
    QObject::connect(deleteButton, &QPushButton::clicked, [this]() {
        EmitAction(ScanWorkflowActionDelete, "Delete");
    });

    layout->addWidget(playButton);
    layout->addWidget(completeButton);
    layout->addWidget(deleteButton);
    return frame;
}

// Creates the lower-left hint/status panel.
QWidget* ScanWorkflowUIImpl::CreateHintPanel(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);
    frame->setFixedSize(230, 170);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Hint"), frame);
    title->setStyleSheet("font-weight:600;");
    layout->addWidget(title);

    auto* text = new QLabel(tr("Keep scanner moving steadily and fill missing areas before completion."), frame);
    text->setWordWrap(true);
    text->setProperty("muted", true);
    layout->addWidget(text);

    m_statusLabel = new QLabel(tr("Ready"), frame);
    m_statusLabel->setProperty("muted", true);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);
    return frame;
}

// Creates the embedded VTK viewer.
QWidget* ScanWorkflowUIImpl::CreateViewerArea(QWidget* parent) {
    // QVTKWidget is the Qt 5.6 / VTK 8 bridge widget used by the legacy stack.
    m_vtkWidget = new QVTKWidget(parent);
    m_vtkWidget->setObjectName("ScanWorkflowVTKWidget");
    m_vtkWidget->setMinimumSize(640, 420);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    BuildPlaceholderScene();
    return m_vtkWidget;
}

// Builds a placeholder VTK scene.
void ScanWorkflowUIImpl::BuildPlaceholderScene() {
    if (!m_vtkWidget) {
        return;
    }

    // The renderer owns the camera, background, and the actor list for this page.
    m_renderer = vtkRenderer::New();
    m_renderer->SetBackground(0.87, 0.89, 0.92);

    // Cone and cube only prove that the VTK render path is alive in the frame.
    vtkSmartPointer<vtkConeSource> cone = vtkSmartPointer<vtkConeSource>::New();
    cone->SetHeight(2.2);
    cone->SetRadius(0.8);
    cone->SetResolution(48);

    vtkSmartPointer<vtkPolyDataMapper> coneMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    coneMapper->SetInputConnection(cone->GetOutputPort());

    vtkSmartPointer<vtkActor> coneActor = vtkSmartPointer<vtkActor>::New();
    coneActor->SetMapper(coneMapper);
    coneActor->GetProperty()->SetColor(0.95, 0.47, 0.42);
    coneActor->SetPosition(-0.8, 0.0, 0.0);

    vtkSmartPointer<vtkCubeSource> cube = vtkSmartPointer<vtkCubeSource>::New();
    cube->SetXLength(0.8);
    cube->SetYLength(0.8);
    cube->SetZLength(0.8);

    vtkSmartPointer<vtkPolyDataMapper> cubeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    cubeMapper->SetInputConnection(cube->GetOutputPort());

    vtkSmartPointer<vtkActor> cubeActor = vtkSmartPointer<vtkActor>::New();
    cubeActor->SetMapper(cubeMapper);
    cubeActor->GetProperty()->SetColor(0.0, 0.49, 0.41);
    cubeActor->SetPosition(1.0, 0.0, 0.0);

    m_renderer->AddActor(coneActor);
    m_renderer->AddActor(cubeActor);
    m_renderer->ResetCamera();

    // QVTKWidget exposes the vtkRenderWindow that actually owns OpenGL rendering.
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    m_vtkWidget->GetRenderWindow()->Render();
}

// Emits a user action to the shell and writes a log line.
void ScanWorkflowUIImpl::EmitAction(int actionId, const QString& operation) {
    WriteLog(LogLevel::Info, operation.toUtf8().constData(),
             QString("Scan workflow action: %1").arg(actionId));
    if (m_actionCallback) {
        // Only a stable integer crosses the DLL boundary.
        m_actionCallback(m_actionContext, actionId);
    }
}

// Writes a structured log entry if logger is initialized.
void ScanWorkflowUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }
    const QByteArray bytes = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation ? operation : "", "", "", "", bytes.constData());
}

// C ABI factory used by the shell through QLibrary/GetProcAddress.
extern "C" MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI* GetScanWorkflowUI() {
    return &ScanWorkflowUIImpl::Instance();
}

// Unified version export used by runtime version-list collection.
extern "C" MEYERSCAN_SCANWORKFLOWUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
