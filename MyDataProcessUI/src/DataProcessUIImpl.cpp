#include "DataProcessUIImpl.h"

#include "MeyerQtModuleUtils.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayoutItem>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariant>
#include <QWheelEvent>
#include <QtGlobal>

#include <QVTKWidget.h>
#include <vtkActor.h>
#include <vtkAutoInit.h>
#include <vtkCamera.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSphereSource.h>
#include <vtkSmartPointer.h>

#include <opencv2/core.hpp>

#include <cmath>

VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);

namespace {
namespace ModuleInfo {
// This value is written into the structured log module field.
const char* Name = "MeyerScan_DataProcessUI";

// Code version returned by GetModuleVersion(). Keep it in sync with Version.rc.
const char* Version = "MeyerScan_DataProcessUI v0.2.2 (2026-07-10)";
}
}

// 数据处理页的 VTK 视图控件。
// 它只接管滚轮缩放，把缩放范围和鼠标中心缩放逻辑集中在一个地方。
class DataProcessViewerWidget : public QVTKWidget {
public:
    explicit DataProcessViewerWidget(QWidget* parent = nullptr)
        : QVTKWidget(parent) {
        setFocusPolicy(Qt::WheelFocus);
    }

    // renderer 由 DataProcessUIImpl 拥有；本控件只借用它做事件响应。
    void SetRenderer(vtkRenderer* renderer) {
        m_renderer = renderer;
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!event || !m_renderer || !m_renderer->GetActiveCamera() || !GetRenderWindow()) {
            QVTKWidget::wheelEvent(event);
            return;
        }

        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->accept();
            return;
        }

        const double wheelSteps = static_cast<double>(delta) / 120.0;
        const double zoomRatio = std::pow(1.12, wheelSteps);
        const double targetZoom = qBound(0.35, m_zoomFactor * zoomRatio, 4.0);
        const double appliedRatio = targetZoom / m_zoomFactor;
        if (qFuzzyCompare(appliedRatio, 1.0)) {
            event->accept();
            return;
        }

        vtkCamera* camera = m_renderer->GetActiveCamera();
        double beforeWorld[4] = {0.0, 0.0, 0.0, 1.0};
        double afterWorld[4] = {0.0, 0.0, 0.0, 1.0};
        const QPoint mousePos = event->pos();
        const int displayX = mousePos.x();
        const int displayY = height() - mousePos.y();
        ReadWorldPointAtDisplay(displayX, displayY, beforeWorld);

        camera->Zoom(appliedRatio);
        m_zoomFactor = targetZoom;
        ReadWorldPointAtDisplay(displayX, displayY, afterWorld);

        const double dx = beforeWorld[0] - afterWorld[0];
        const double dy = beforeWorld[1] - afterWorld[1];
        const double dz = beforeWorld[2] - afterWorld[2];
        double position[3] = {0.0, 0.0, 0.0};
        double focalPoint[3] = {0.0, 0.0, 0.0};
        camera->GetPosition(position);
        camera->GetFocalPoint(focalPoint);
        camera->SetPosition(position[0] + dx, position[1] + dy, position[2] + dz);
        camera->SetFocalPoint(focalPoint[0] + dx, focalPoint[1] + dy, focalPoint[2] + dz);

        m_renderer->ResetCameraClippingRange();
        GetRenderWindow()->Render();
        event->accept();
    }

private:
    void ReadWorldPointAtDisplay(int displayX, int displayY, double worldPoint[4]) const {
        if (!m_renderer || !m_renderer->GetActiveCamera()) {
            return;
        }

        double focalPoint[4] = {0.0, 0.0, 0.0, 1.0};
        m_renderer->GetActiveCamera()->GetFocalPoint(focalPoint);
        m_renderer->SetWorldPoint(focalPoint);
        m_renderer->WorldToDisplay();
        double focalDisplay[3] = {0.0, 0.0, 0.0};
        m_renderer->GetDisplayPoint(focalDisplay);

        m_renderer->SetDisplayPoint(displayX, displayY, focalDisplay[2]);
        m_renderer->DisplayToWorld();
        m_renderer->GetWorldPoint(worldPoint);
        if (!qFuzzyIsNull(worldPoint[3])) {
            worldPoint[0] /= worldPoint[3];
            worldPoint[1] /= worldPoint[3];
            worldPoint[2] /= worldPoint[3];
            worldPoint[3] = 1.0;
        }
    }

private:
    vtkRenderer* m_renderer = nullptr;
    double m_zoomFactor = 1.0;
};

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
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    MeyerQtModule::ApplyModuleQss(root, "MyDataProcessUI", "data_process.qss", m_logger);

    // Main layout mirrors the scan page so the two stages feel like one product.
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(CreateModelModeBar(root), 0, Qt::AlignHCenter);

    // The viewer expands while the right-side tools keep a stable width.
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);
    // Process 也保留左下角提示框，但内容与 Scan 页独立，后续可按处理规则单独替换文案。
    centerLayout->addWidget(CreateHintPanel(root), 0, Qt::AlignBottom);
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
    RefreshScanProcessButtons();
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
    UpdateDisplayedProcessData();
    WriteLog(LogLevel::Info, "Activate", "Data process activated");
}

// Releases heavy VTK/OpenGL resources before leaving the data-processing stage.
void DataProcessUIImpl::DeactivateAndRelease() {
    // QVTKWidget owns native OpenGL resources, so hiding it is not enough.
    if (m_vtkWidget) {
        // 释放顺序与 Scan 页面保持一致：先解除 renderer，再断开 render window/interactor。
        // 这能避免退出 Process 后重新进入 Scan 时，QVTKWidget 析构访问已删除的 VTK 对象。
        vtkRenderWindow* renderWindow = m_vtkWidget->GetRenderWindow();
        if (m_renderer && renderWindow) {
            renderWindow->RemoveRenderer(m_renderer);
        }
        m_vtkWidget->SetRenderer(nullptr);
        m_vtkWidget->SetRenderWindow(nullptr);
        // 延迟删除原生 OpenGL 控件，让当前页面切换调用栈先完整返回。
        m_vtkWidget->setParent(nullptr);
        m_vtkWidget->deleteLater();
        m_vtkWidget = nullptr;
    }

    // vtkRenderer::New() uses reference counting and must be paired with Delete().
    if (m_renderer) {
        m_renderer->Delete();
        m_renderer = nullptr;
    }

    // 清除旧页面中的非 owning 指针；下次 CreateWidget 会重新绑定新控件。
    m_root = nullptr;
    m_statusLabel = nullptr;
    m_hintLabel = nullptr;
    m_modelModeBar = nullptr;
    m_processButtons.clear();

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
    m_hintLabel = nullptr;
    m_modelModeBar = nullptr;
    m_processButtons.clear();
    m_processSteps.clear();
    m_currentStepIndex = -1;
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
    m_modelModeBar = frame;
    frame->setProperty("panel", true);
    frame->setMinimumHeight(86);

    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(16);
    RefreshScanProcessButtons();
    return frame;
}

// Rebuilds the model/process button bar from session context.
void DataProcessUIImpl::RefreshScanProcessButtons() {
    if (!m_modelModeBar) {
        return;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(m_modelModeBar->layout());
    if (!layout) {
        return;
    }

    // 清空旧按钮，避免处理页复用时残留上一单流程。
    // Clear old process buttons before rebuilding the bar.
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_processButtons.clear();

    // Process 只消费已有 scanProcess.steps，避免复制建单规则或推导逻辑。
    m_processSteps = ResolveScanProcessSteps();
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_processSteps.size()) {
        m_currentStepIndex = 0;
    }

    for (int i = 0; i < m_processSteps.size(); ++i) {
        const ProcessStepInfo step = m_processSteps.at(i);
        auto* button = new QPushButton(step.label, m_modelModeBar);
        button->setObjectName(QString("ProcessStep_%1_Button").arg(step.code));
        button->setMinimumSize(104, 52);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        button->setCheckable(true);
        button->setEnabled(step.enabled);
        button->setCursor(step.enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        button->setToolTip(tr("Show process data for %1").arg(step.label));
        QObject::connect(button, &QPushButton::clicked, [this, i]() {
            // 点击顶部步骤按钮只切换当前显示模型；真实编辑/分析状态后续由处理服务维护。
            SelectProcessStep(i, "ButtonClicked");
        });
        layout->addWidget(button);
        m_processButtons.append(button);
    }
    RefreshScanProcessButtonStates();
    UpdateDisplayedProcessData();
}

// Resolves scan-process steps from JSON context.
QVector<DataProcessUIImpl::ProcessStepInfo> DataProcessUIImpl::ResolveScanProcessSteps() const {
    QVector<ProcessStepInfo> steps;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_contextJson, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonObject root = document.object();
        const QJsonObject scanProcess = root.value("scanProcess").toObject();
        const QJsonArray jsonSteps = scanProcess.value("steps").toArray();
        for (const QJsonValue& value : jsonSteps) {
            const QJsonObject item = value.toObject();
            const QString label = item.value("label").toString().trimmed();
            const QString code = item.value("code").toString().trimmed();
            if (!label.isEmpty()) {
                ProcessStepInfo step;
                step.part = item.value("part").toString().trimmed();
                step.code = code.isEmpty() ? QString("step_%1").arg(steps.size() + 1) : code;
                step.label = label;
                step.exchange = item.value("exchange").toBool(false);
                step.enabled = item.value("enabled").toBool(true);
                steps.append(step);
            }
        }
    }

    if (steps.isEmpty()) {
        ProcessStepInfo maxilla;
        maxilla.part = "maxilla";
        maxilla.code = "maxilla_natural";
        maxilla.label = tr("Natural maxilla");
        steps.append(maxilla);

        ProcessStepInfo exchange;
        exchange.part = "exchange";
        exchange.code = "data_exchange";
        exchange.label = tr("Exchange");
        exchange.exchange = true;
        steps.append(exchange);

        ProcessStepInfo mandible;
        mandible.part = "mandible";
        mandible.code = "mandible_natural";
        mandible.label = tr("Natural mandible");
        steps.append(mandible);

        ProcessStepInfo occlusion;
        occlusion.part = "occlusion";
        occlusion.code = "natural_occlusion";
        occlusion.label = tr("Natural occlusion");
        steps.append(occlusion);
    }
    return steps;
}

// Switches the active process step.
void DataProcessUIImpl::SelectProcessStep(int index, const QString& reason) {
    if (index < 0 || index >= m_processSteps.size()) {
        WriteLog(LogLevel::Warning, "SelectProcessStep", QString("Invalid process step index: %1").arg(index));
        return;
    }
    if (!m_processSteps.at(index).enabled) {
        WriteLog(LogLevel::Info, "SelectProcessStep", QString("Disabled process step ignored: %1").arg(index));
        return;
    }

    m_currentStepIndex = index;
    RefreshScanProcessButtonStates();
    UpdateDisplayedProcessData();
    EmitAction(DataProcessActionEdit,
               QString("ProcessStepChanged:%1:%2").arg(reason, m_processSteps.at(index).code));
}

// Refreshes selected state and style for process buttons.
void DataProcessUIImpl::RefreshScanProcessButtonStates() {
    for (int i = 0; i < m_processButtons.size(); ++i) {
        QPushButton* button = m_processButtons.at(i);
        if (!button) {
            continue;
        }

        const bool checked = i == m_currentStepIndex;
        button->setChecked(checked);
        button->setProperty("primary", checked);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

// Updates placeholder model and status text for the current process step.
void DataProcessUIImpl::UpdateDisplayedProcessData() {
    if (m_processSteps.isEmpty()) {
        m_processSteps = ResolveScanProcessSteps();
    }
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_processSteps.size()) {
        m_currentStepIndex = 0;
    }

    const ProcessStepInfo step = m_processSteps.value(m_currentStepIndex);
    if (m_statusLabel) {
        m_statusLabel->setText(tr("Processing: %1").arg(step.label));
        m_statusLabel->setToolTip(tr("Current process step code: %1").arg(step.code));
    }
    if (m_hintLabel) {
        // Process 左下角提示框与 Scan 不共用内容；具体产品文案等待后续补充。
        m_hintLabel->setText(tr("Select a model step before editing or analysis."));
    }
    RebuildStepPlaceholderScene();
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
    m_vtkWidget = new DataProcessViewerWidget(parent);
    m_vtkWidget->setObjectName("DataProcessVTKWidget");
    m_vtkWidget->setMinimumSize(540, 340);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    BuildPlaceholderScene();
    return m_vtkWidget;
}

// Creates the lower-left process hint panel.
QWidget* DataProcessUIImpl::CreateHintPanel(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);
    frame->setMinimumSize(190, 140);
    frame->setMaximumWidth(230);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Process Hint"), frame);
    title->setObjectName("DataProcessPanelTitle");
    layout->addWidget(title);

    m_hintLabel = new QLabel(tr("Select a model step before editing or analysis."), frame);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setProperty("muted", true);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);
    return frame;
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
    m_vtkWidget->SetRenderer(m_renderer);

    // QVTKWidget exposes the vtkRenderWindow that actually owns OpenGL rendering.
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    RebuildStepPlaceholderScene();
}

// Rebuilds lightweight placeholder geometry for the active process step.
void DataProcessUIImpl::RebuildStepPlaceholderScene() {
    if (!m_vtkWidget || !m_renderer) {
        return;
    }

    // RemoveAllViewProps 只清除旧 actor，不删除 renderer/camera。
    // 后续真实模型接入时，这里会改为按 step.code 切换对应处理数据。
    m_renderer->RemoveAllViewProps();

    const ProcessStepInfo step = m_processSteps.value(m_currentStepIndex);
    const QString part = step.part.toLower();
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    if (part == "mandible") {
        vtkSmartPointer<vtkCylinderSource> source = vtkSmartPointer<vtkCylinderSource>::New();
        source->SetRadius(0.95);
        source->SetHeight(1.4);
        source->SetResolution(72);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.18, 0.48, 0.76);
    } else if (part == "exchange") {
        vtkSmartPointer<vtkCubeSource> source = vtkSmartPointer<vtkCubeSource>::New();
        source->SetXLength(1.2);
        source->SetYLength(1.2);
        source->SetZLength(1.2);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.0, 0.49, 0.41);
    } else if (part == "occlusion") {
        vtkSmartPointer<vtkConeSource> source = vtkSmartPointer<vtkConeSource>::New();
        source->SetHeight(1.6);
        source->SetRadius(1.1);
        source->SetResolution(72);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.86, 0.64, 0.24);
    } else {
        vtkSmartPointer<vtkSphereSource> source = vtkSmartPointer<vtkSphereSource>::New();
        source->SetRadius(1.2);
        source->SetThetaResolution(96);
        source->SetPhiResolution(48);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.92, 0.48, 0.44);
    }

    actor->GetProperty()->SetSpecular(0.25);
    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();
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
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// C ABI factory used by the shell through QLibrary/GetProcAddress.
extern "C" MEYERSCAN_DATAPROCESSUI_API IDataProcessUI* GetDataProcessUI() {
    return &DataProcessUIImpl::Instance();
}

// Unified version export used by runtime version-list collection.
extern "C" MEYERSCAN_DATAPROCESSUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
