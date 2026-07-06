#include "DataProcessUIImpl.h"

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
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSphereSource.h>
#include <vtkSmartPointer.h>

#include <opencv2/core.hpp>

VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);

namespace {
namespace ModuleInfo {
// This value is written into the structured log module field.
const char* Name = "MeyerScan_DataProcessUI";

// Code version returned by GetModuleVersion(). Keep it in sync with Version.rc.
const char* Version = "MeyerScan_DataProcessUI v0.1.0 (2026-07-05)";
}

const char* kPageBackground = "#dfe4ea";
const char* kPanelBackground = "#ffffff";
const char* kPrimaryColor = "#007d68";
const char* kMutedText = "#687785";
const char* kBorderColor = "#cbd5dd";
}

// Returns the data-processing UI singleton owned by this DLL.
DataProcessUIImpl& DataProcessUIImpl::Instance() {
    // A local static avoids cross-DLL ownership and is constructed once per process.
    static DataProcessUIImpl instance;
    return instance;
}

// Initializes data-processing paths and logger.
bool DataProcessUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // Copy incoming UTF-8 buffers so the caller can release temporary values safely.
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // Cache the logger pointer once; later writes reuse this member.
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    // This lightweight matrix proves that OpenCV 3.3 include/link paths work.
    cv::Mat buildProbe(1, 1, CV_8UC1);
    buildProbe.setTo(cv::Scalar(0));
    WriteLog(LogLevel::Info, "Init",
             QString("Data process UI initialized, OpenCV probe cols=%1").arg(buildProbe.cols));
    return true;
}

// Creates the root widget for the data-processing stage.
QWidget* DataProcessUIImpl::CreateWidget(QWidget* parent) {
    // Release any previous QVTKWidget before creating a new page.
    DeactivateAndRelease();

    // The shell/test host owns the returned QWidget through the Qt parent tree.
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanDataProcessUIRoot");
    root->setMinimumSize(1280, 720);
    root->setStyleSheet(QString(
        "#MeyerScanDataProcessUIRoot{background:%1;}"
        "QLabel{color:#1f2b36;font-size:13px;}"
        "QLabel[muted=\"true\"]{color:%2;}"
        "QPushButton{border:0;border-radius:4px;background:#f7f9fb;color:#273645;padding:8px 12px;min-height:28px;}"
        "QPushButton:hover{background:#eef3f6;}"
        "QPushButton[primary=\"true\"]{background:%3;color:#ffffff;font-weight:600;}"
        "QFrame[panel=\"true\"]{background:%4;border:1px solid %5;border-radius:6px;}"
        "QFrame[toolPanel=\"true\"]{background:%4;border-radius:6px;}"
    ).arg(kPageBackground, kMutedText, kPrimaryColor, kPanelBackground, kBorderColor));

    // Main layout mirrors the scan page so the two stages feel like one product.
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(CreateModelModeBar(root), 0, Qt::AlignHCenter);

    // The viewer expands while the right-side tools keep a stable width.
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);
    centerLayout->addStretch(0);
    centerLayout->addWidget(CreateViewerArea(root), 1);
    centerLayout->addWidget(CreateProcessingToolBar(root), 0, Qt::AlignVCenter);
    mainLayout->addLayout(centerLayout, 1);

    // Bottom bar exposes previous/next and status feedback.
    mainLayout->addWidget(CreateBottomStatusBar(root), 0, Qt::AlignHCenter);

    // Keep a non-owning pointer; Qt parent ownership destroys the actual widget.
    m_root = root;
    Activate();
    WriteLog(LogLevel::Info, "CreateWidget", "Data process widget created");
    return root;
}

// Stores lightweight session context as UTF-8 JSON.
bool DataProcessUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    // Keep JSON opaque here; real model loading can later use order/session IDs.
    m_contextJson = QByteArray(contextJsonUtf8 ? contextJsonUtf8 : "");
    WriteLog(LogLevel::Info, "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

// Registers the shell callback.
void DataProcessUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // Store a plain C callback to avoid exposing QObject or signal types across DLLs.
    m_actionCallback = callback;
    m_actionContext = context;
}

// Activates the data-processing stage.
void DataProcessUIImpl::Activate() {
    // Real implementation will restore active tool and selected model here.
    if (m_statusLabel) {
        m_statusLabel->setText(tr("Ready for processing"));
    }
    WriteLog(LogLevel::Info, "Activate", "Data process activated");
}

// Releases heavy VTK/OpenGL resources before leaving the data-processing stage.
void DataProcessUIImpl::DeactivateAndRelease() {
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

    WriteLog(LogLevel::Info, "DeactivateAndRelease", "Data process VTK resources released");
}

// Returns the module code version.
const char* DataProcessUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// Shuts down the module and clears cached state.
void DataProcessUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Data process UI shutdown");
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

// Creates the model selector bar.
QWidget* DataProcessUIImpl::CreateModelModeBar(QWidget* parent) {
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
            // The UI reports intent; model-state storage will live outside the button.
            EmitAction(DataProcessActionEdit, "ModelModeChanged");
        });
        layout->addWidget(button);
    }
    return frame;
}

// Creates the right-side processing toolbar.
QWidget* DataProcessUIImpl::CreateProcessingToolBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("toolPanel", true);
    frame->setFixedWidth(86);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Each entry keeps visible text, stable action id, and log operation together.
    struct ToolItem {
        QString text;
        int actionId;
        QString operation;
    };
    const ToolItem tools[] = {
        {tr("Screenshot"), DataProcessActionScreenshot, "Screenshot"},
        {tr("Edit"), DataProcessActionEdit, "Edit"},
        {tr("Margin"), DataProcessActionMargin, "Margin"},
        {tr("Undercut"), DataProcessActionUndercut, "Undercut"},
        {tr("Color"), DataProcessActionColor, "Color"},
        {tr("Measure"), DataProcessActionMeasure, "Measure"},
    };

    for (const ToolItem& item : tools) {
        auto* button = new QPushButton(item.text, frame);
        button->setMinimumHeight(54);
        QObject::connect(button, &QPushButton::clicked, [this, item]() {
            // The UI reports tool intent; heavy algorithms stay in later service DLLs.
            EmitAction(item.actionId, item.operation);
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return frame;
}

// Creates the embedded VTK viewer.
QWidget* DataProcessUIImpl::CreateViewerArea(QWidget* parent) {
    // QVTKWidget is the Qt 5.6 / VTK 8 bridge widget used by the legacy stack.
    m_vtkWidget = new QVTKWidget(parent);
    m_vtkWidget->setObjectName("DataProcessVTKWidget");
    m_vtkWidget->setMinimumSize(720, 460);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    BuildPlaceholderScene();
    return m_vtkWidget;
}

// Creates the bottom status/action bar.
QWidget* DataProcessUIImpl::CreateBottomStatusBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(12);

    auto* previousButton = new QPushButton(tr("Previous"), frame);
    auto* nextButton = new QPushButton(tr("Next"), frame);
    nextButton->setProperty("primary", true);
    m_statusLabel = new QLabel(tr("Ready"), frame);
    m_statusLabel->setProperty("muted", true);

    QObject::connect(previousButton, &QPushButton::clicked, [this]() {
        EmitAction(DataProcessActionPrevious, "Previous");
    });
    QObject::connect(nextButton, &QPushButton::clicked, [this]() {
        EmitAction(DataProcessActionNext, "Next");
    });

    layout->addWidget(previousButton);
    layout->addWidget(m_statusLabel, 1);
    layout->addWidget(nextButton);
    return frame;
}

// Builds a placeholder VTK scene.
void DataProcessUIImpl::BuildPlaceholderScene() {
    if (!m_vtkWidget) {
        return;
    }

    // The renderer owns the camera, background, and the actor list for this page.
    m_renderer = vtkRenderer::New();
    m_renderer->SetBackground(0.87, 0.89, 0.92);

    // Sphere geometry proves that the VTK render path is alive in the frame.
    vtkSmartPointer<vtkSphereSource> source = vtkSmartPointer<vtkSphereSource>::New();
    source->SetRadius(1.2);
    source->SetThetaResolution(96);
    source->SetPhiResolution(48);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(source->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.92, 0.48, 0.44);
    actor->GetProperty()->SetSpecular(0.25);

    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();

    // QVTKWidget exposes the vtkRenderWindow that actually owns OpenGL rendering.
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    m_vtkWidget->GetRenderWindow()->Render();
}

// Emits a user action to the shell and writes a log line.
void DataProcessUIImpl::EmitAction(int actionId, const QString& operation) {
    WriteLog(LogLevel::Info, operation.toUtf8().constData(),
             QString("Data process action: %1").arg(actionId));
    if (m_actionCallback) {
        // Only a stable integer crosses the DLL boundary.
        m_actionCallback(m_actionContext, actionId);
    }
}

// Writes a structured log entry if logger is initialized.
void DataProcessUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }
    const QByteArray bytes = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation ? operation : "", "", "", "", bytes.constData());
}

// C ABI factory used by the shell through QLibrary/GetProcAddress.
extern "C" MEYERSCAN_DATAPROCESSUI_API IDataProcessUI* GetDataProcessUI() {
    return &DataProcessUIImpl::Instance();
}

// Unified version export used by runtime version-list collection.
extern "C" MEYERSCAN_DATAPROCESSUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
