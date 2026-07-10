#include "ScanWorkflowUIImpl.h"

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
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

#include <opencv2/core.hpp>

#include <cmath>

VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);

namespace {
namespace ModuleInfo {
// This value is written into the structured log module field.
const char* Name = "MeyerScan_ScanWorkflowUI";

// Code version returned by GetModuleVersion(). Keep it in sync with Version.rc.
const char* Version = "MeyerScan_ScanWorkflowUI v0.2.2 (2026-07-10)";
}
}

// QVTKWidget 本身会处理大量鼠标事件；这里派生一个轻量子类，只接管滚轮缩放。
// 这样页面代码不用安装分散的 eventFilter，也不会影响后续 VTK 交互样式接入。
class ScanWorkflowViewerWidget : public QVTKWidget {
public:
    explicit ScanWorkflowViewerWidget(QWidget* parent = nullptr)
        : QVTKWidget(parent) {
        // 鼠标悬停时就接收滚轮事件，避免用户必须先点一下视图才能缩放。
        setFocusPolicy(Qt::WheelFocus);
    }

    // 将当前 renderer 传入视图控件；renderer 仍由 ScanWorkflowUIImpl 创建和释放。
    void SetRenderer(vtkRenderer* renderer) {
        m_renderer = renderer;
    }

protected:
    // 以鼠标所在位置为中心进行滚轮缩放。
    void wheelEvent(QWheelEvent* event) override {
        if (!event || !m_renderer || !m_renderer->GetActiveCamera() || !GetRenderWindow()) {
            QVTKWidget::wheelEvent(event);
            return;
        }

        // angleDelta().y() 是 Qt5 标准滚轮增量，正数表示向前滚动。
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->accept();
            return;
        }

        // 每一格滚轮按 1.12 缩放；快速滚动时按格数指数放大，交互更连续。
        const double wheelSteps = static_cast<double>(delta) / 120.0;
        const double zoomRatio = std::pow(1.12, wheelSteps);
        // 先计算目标缩放值，再夹紧到边界；真实相机只应用“当前到目标”的实际比例。
        const double targetZoom = qBound(0.35, m_zoomFactor * zoomRatio, 4.0);
        const double appliedRatio = targetZoom / m_zoomFactor;
        if (qFuzzyCompare(appliedRatio, 1.0)) {
            // 已经到边界时不继续调用 camera->Zoom，避免过界后再被拉回。
            event->accept();
            return;
        }

        vtkCamera* camera = m_renderer->GetActiveCamera();
        double beforeWorld[4] = {0.0, 0.0, 0.0, 1.0};
        double afterWorld[4] = {0.0, 0.0, 0.0, 1.0};

        // VTK 的 display 坐标原点在左下角，Qt 的 pos 原点在左上角，所以 y 需要翻转。
        const QPoint mousePos = event->pos();
        const int displayX = mousePos.x();
        const int displayY = height() - mousePos.y();
        ReadWorldPointAtDisplay(displayX, displayY, beforeWorld);

        // camera->Zoom 大于 1 表示放大，小于 1 表示缩小。
        camera->Zoom(appliedRatio);
        m_zoomFactor = targetZoom;

        ReadWorldPointAtDisplay(displayX, displayY, afterWorld);

        // 缩放后，让鼠标指向的世界点保持在原位置。
        // 做法是把相机 position/focalPoint 同步平移 before-after 的差值。
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
    // 把视图中的一个像素点投影到当前相机焦点深度对应的世界坐标。
    void ReadWorldPointAtDisplay(int displayX, int displayY, double worldPoint[4]) const {
        if (!m_renderer || !m_renderer->GetActiveCamera()) {
            return;
        }

        // 先读取相机焦点对应的 display z，确保鼠标点映射到模型所在深度附近。
        double focalPoint[4] = {0.0, 0.0, 0.0, 1.0};
        m_renderer->GetActiveCamera()->GetFocalPoint(focalPoint);
        m_renderer->SetWorldPoint(focalPoint);
        m_renderer->WorldToDisplay();
        double focalDisplay[3] = {0.0, 0.0, 0.0};
        m_renderer->GetDisplayPoint(focalDisplay);

        // 再把鼠标 display x/y 和焦点 display z 转回世界坐标。
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
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    MeyerQtModule::ApplyModuleQss(root, "MyScanWorkflowUI", "scan_workflow.qss", m_logger);

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
    RefreshScanProcessButtons();
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
    UpdateDisplayedScanData();
    WriteLog(LogLevel::Info, "Activate", "Scan workflow activated");
}

// Releases heavy VTK/OpenGL resources before leaving the scan stage.
void ScanWorkflowUIImpl::DeactivateAndRelease() {
    // QVTKWidget owns native OpenGL resources, so hiding it is not enough.
    if (m_vtkWidget) {
        // 先从 render window 移除 renderer，再清空 QVTKWidget 子类保存的 renderer 指针。
        // 如果只 Delete vtkRenderer，滚轮或析构路径仍可能访问已经释放的地址。
        vtkRenderWindow* renderWindow = m_vtkWidget->GetRenderWindow();
        if (m_renderer && renderWindow) {
            renderWindow->RemoveRenderer(m_renderer);
        }
        m_vtkWidget->SetRenderer(nullptr);
        // QVTKWidget 还会持有 vtkRenderWindow/Interactor；显式断开后再延迟删除，
        // 反复离开和重新进入 Scan 页面时不会复用上一次的 OpenGL 窗口状态。
        m_vtkWidget->SetRenderWindow(nullptr);
        // 延迟删除让当前按钮/页面切换调用栈先返回，避免信号处理中销毁原生窗口句柄。
        m_vtkWidget->setParent(nullptr);
        m_vtkWidget->deleteLater();
        m_vtkWidget = nullptr;
    }

    // vtkRenderer::New() uses reference counting and must be paired with Delete().
    if (m_renderer) {
        m_renderer->Delete();
        m_renderer = nullptr;
    }

    // 页面根控件随后由壳子释放；提前清空非 owning 指针，避免延迟删除后误访问旧页面。
    m_root = nullptr;
    m_statusLabel = nullptr;
    m_scanModeBar = nullptr;
    m_scanProcessButtons.clear();

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
    m_scanModeBar = nullptr;
    m_scanProcessButtons.clear();
    m_scanProcessSteps.clear();
    m_currentStepIndex = -1;
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
    m_scanModeBar = frame;
    frame->setProperty("panel", true);
    frame->setMinimumHeight(86);

    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(16);
    RefreshScanProcessButtons();
    return frame;
}

// Rebuilds the scan-process button bar from session context.
void ScanWorkflowUIImpl::RefreshScanProcessButtons() {
    if (!m_scanModeBar) {
        return;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(m_scanModeBar->layout());
    if (!layout) {
        return;
    }

    // 清空旧按钮，避免重新进入扫描页时继续显示上一单流程。
    // Clear old process buttons before rebuilding the bar.
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_scanProcessButtons.clear();

    // 重新解析完整步骤，而不是只解析 label。
    // code/part 后续用于真实模型加载，enabled/exchange 用于按钮状态和提示。
    m_scanProcessSteps = ResolveScanProcessSteps();
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_scanProcessSteps.size()) {
        m_currentStepIndex = 0;
    }

    for (int i = 0; i < m_scanProcessSteps.size(); ++i) {
        const ScanProcessStepInfo step = m_scanProcessSteps.at(i);
        auto* button = new QPushButton(step.label, m_scanModeBar);
        button->setObjectName(QString("ScanProcessStep_%1_Button").arg(step.code));
        button->setMinimumSize(104, 52);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        button->setCheckable(true);
        button->setEnabled(step.enabled);
        // 流程按钮是可点击的扫描部位入口，hover 时显示手型光标。
        button->setCursor(step.enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        button->setToolTip(tr("Show scan data for %1").arg(step.label));
        QObject::connect(button, &QPushButton::clicked, [this, i]() {
            // 点击按钮只切换当前扫描部位和占位数据；真实扫描流程状态后续仍由服务层保存。
            SelectScanProcessStep(i, "ButtonClicked");
        });
        layout->addWidget(button);
        m_scanProcessButtons.append(button);
    }
    RefreshScanProcessButtonStates();
    UpdateDisplayedScanData();
}

// Resolves scan-process steps from JSON context.
QVector<ScanWorkflowUIImpl::ScanProcessStepInfo> ScanWorkflowUIImpl::ResolveScanProcessSteps() const {
    QVector<ScanProcessStepInfo> steps;
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
                ScanProcessStepInfo step;
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
        ScanProcessStepInfo maxilla;
        maxilla.part = "maxilla";
        maxilla.code = "maxilla_natural";
        maxilla.label = tr("Natural maxilla");
        steps.append(maxilla);

        ScanProcessStepInfo exchange;
        exchange.part = "exchange";
        exchange.code = "data_exchange";
        exchange.label = tr("Exchange");
        exchange.exchange = true;
        steps.append(exchange);

        ScanProcessStepInfo mandible;
        mandible.part = "mandible";
        mandible.code = "mandible_natural";
        mandible.label = tr("Natural mandible");
        steps.append(mandible);

        ScanProcessStepInfo occlusion;
        occlusion.part = "occlusion";
        occlusion.code = "natural_occlusion";
        occlusion.label = tr("Natural occlusion");
        steps.append(occlusion);
    }
    return steps;
}

// Switches the active scan-process step.
void ScanWorkflowUIImpl::SelectScanProcessStep(int index, const QString& reason) {
    if (index < 0 || index >= m_scanProcessSteps.size()) {
        WriteLog(LogLevel::Warning, "SelectScanProcessStep", QString("Invalid scan step index: %1").arg(index));
        return;
    }
    if (!m_scanProcessSteps.at(index).enabled) {
        WriteLog(LogLevel::Info, "SelectScanProcessStep", QString("Disabled scan step ignored: %1").arg(index));
        return;
    }

    m_currentStepIndex = index;
    RefreshScanProcessButtonStates();
    UpdateDisplayedScanData();
    EmitAction(ScanWorkflowActionJawModeChanged,
               QString("ScanStepChanged:%1:%2").arg(reason, m_scanProcessSteps.at(index).code));
}

// Refreshes selected state and style for scan-process buttons.
void ScanWorkflowUIImpl::RefreshScanProcessButtonStates() {
    for (int i = 0; i < m_scanProcessButtons.size(); ++i) {
        QPushButton* button = m_scanProcessButtons.at(i);
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

// Updates the visible placeholder data for the current scan step.
void ScanWorkflowUIImpl::UpdateDisplayedScanData() {
    if (m_scanProcessSteps.isEmpty()) {
        m_scanProcessSteps = ResolveScanProcessSteps();
    }
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_scanProcessSteps.size()) {
        m_currentStepIndex = 0;
    }

    const ScanProcessStepInfo step = m_scanProcessSteps.value(m_currentStepIndex);
    if (m_statusLabel) {
        // 左下角提示框属于 Scan 页面，文案与 Process 页面保持独立。
        m_statusLabel->setText(tr("Scanning: %1").arg(step.label));
        m_statusLabel->setToolTip(tr("Current scan step code: %1").arg(step.code));
    }
    RebuildStepPlaceholderScene();
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
    frame->setMinimumSize(190, 140);
    frame->setMaximumWidth(230);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Hint"), frame);
    title->setObjectName("ScanWorkflowPanelTitle");
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
    m_vtkWidget = new ScanWorkflowViewerWidget(parent);
    m_vtkWidget->setObjectName("ScanWorkflowVTKWidget");
    m_vtkWidget->setMinimumSize(520, 340);
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
    m_vtkWidget->SetRenderer(m_renderer);

    // QVTKWidget exposes the vtkRenderWindow that actually owns OpenGL rendering.
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    RebuildStepPlaceholderScene();
}

// Rebuilds lightweight placeholder geometry for the active scan step.
void ScanWorkflowUIImpl::RebuildStepPlaceholderScene() {
    if (!m_vtkWidget || !m_renderer) {
        return;
    }

    // RemoveAllViewProps 清掉旧的 actor，但不销毁 renderer/camera。
    // 真实模型接入后，这里会替换成按 step.code 加载对应扫描数据。
    m_renderer->RemoveAllViewProps();

    const ScanProcessStepInfo step = m_scanProcessSteps.value(m_currentStepIndex);
    const QString part = step.part.toLower();
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    if (part == "mandible") {
        // 下颌占位使用圆柱，便于人工肉眼区分当前按钮切换是否生效。
        vtkSmartPointer<vtkCylinderSource> source = vtkSmartPointer<vtkCylinderSource>::New();
        source->SetRadius(0.9);
        source->SetHeight(1.8);
        source->SetResolution(64);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.22, 0.42, 0.78);
    } else if (part == "exchange") {
        // 数据交换步骤通常不显示真实口腔模型，先用立方体表达“流程节点”。
        vtkSmartPointer<vtkCubeSource> source = vtkSmartPointer<vtkCubeSource>::New();
        source->SetXLength(1.6);
        source->SetYLength(1.0);
        source->SetZLength(1.0);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(0.0, 0.49, 0.41);
    } else {
        // 上颌/咬合/未知步骤使用锥体，后续真实数据加载后会被替换。
        vtkSmartPointer<vtkConeSource> source = vtkSmartPointer<vtkConeSource>::New();
        source->SetHeight(part == "occlusion" ? 1.4 : 2.2);
        source->SetRadius(part == "occlusion" ? 1.2 : 0.8);
        source->SetResolution(64);
        mapper->SetInputConnection(source->GetOutputPort());
        actor->GetProperty()->SetColor(part == "occlusion" ? 0.86 : 0.95,
                                       part == "occlusion" ? 0.64 : 0.47,
                                       part == "occlusion" ? 0.24 : 0.42);
    }

    actor->GetProperty()->SetSpecular(0.18);
    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();
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
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// C ABI factory used by the shell through QLibrary/GetProcAddress.
extern "C" MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI* GetScanWorkflowUI() {
    return &ScanWorkflowUIImpl::Instance();
}

// Unified version export used by runtime version-list collection.
extern "C" MEYERSCAN_SCANWORKFLOWUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
