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
// 结构化日志的 module 字段使用稳定英文名，不随界面语言变化。
const char* Name = "MeyerScan_ScanWorkflowUI";

// GetModuleVersion 返回此代码版本，必须与 CMakeLists.txt 和 Version.rc 同步。
const char* Version = "MeyerScan_ScanWorkflowUI v0.2.3 (2026-07-12)";
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

// 返回 DLL 持有的扫描 UI 单例。
ScanWorkflowUIImpl& ScanWorkflowUIImpl::Instance() {
    // C++11 保证函数内静态对象只构造一次，且对象不会跨 DLL 交给调用方 delete。
    static ScanWorkflowUIImpl instance;
    return instance;
}

// 初始化扫描阶段路径和 Logger。
bool ScanWorkflowUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 立即复制 UTF-8 参数，调用方传入的 QByteArray 临时缓冲区在函数返回后可以安全释放。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 应用目录为空时回退到 Qt 记录的 EXE 目录，禁止使用第三方启动器可改变的 currentPath。
    if (m_appDir.isEmpty()) {
        m_appDir = QCoreApplication::applicationDirPath().toUtf8();
    }

    // Logger 指针只获取一次并缓存；后续日志不重复调用 GetLogger。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        // Logger::Init 是幂等接口，目录必须由 MainExe/扫描壳统一传入。
        if (!m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
            // 日志不可用时扫描框架仍可运行，但不能继续调用半初始化接口。
            m_logger = nullptr;
        }
    }

    // 当前初版尚未接入图像算法；轻量 cv::Size 只验证 OpenCV 3.3 头文件和链接配置可用。
    // 真正算法不得继续堆入本 UI 文件，后续应进入独立算法/扫描业务 DLL。
    cv::Size buildProbe(1920, 1080);
    WriteLog(LogLevel::Info, "Init",
             QString("Scan workflow UI initialized, OpenCV probe %1x%2")
                 .arg(buildProbe.width)
                 .arg(buildProbe.height));
    return true;
}

// 创建扫描阶段根页面。
QWidget* ScanWorkflowUIImpl::CreateWidget(QWidget* parent) {
    // 单例一次只允许持有一套 QVTK 资源；创建新页面前先释放上一套显示资源。
    DeactivateAndRelease();

    // 返回对象由 shell/test host 的 Qt 父子树持有，本模块只缓存非 owning 指针。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanScanWorkflowUIRoot");
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    MeyerQtModule::ApplyModuleQss(root, "MyScanWorkflowUI", "scan_workflow.qss", m_logger);

    // 页面使用纵向 Layout 组合流程栏、中央 VTK 区和扫描控制栏，不按分辨率写绝对坐标。
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 顶部流程栏由 session JSON 驱动，Scan 页面不自行生成建单规则。
    mainLayout->addWidget(CreateScanModeBar(root), 0, Qt::AlignHCenter);

    // 中央区由 Scan 专属提示、可伸缩 VTK 视图和右侧工具组成。
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);
    centerLayout->addWidget(CreateHintPanel(root), 0, Qt::AlignBottom);
    centerLayout->addWidget(CreateViewerArea(root), 1);
    centerLayout->addWidget(CreateRightToolBar(root), 0, Qt::AlignVCenter);
    mainLayout->addLayout(centerLayout, 1);

    // 底部按钮只上报扫描阶段动作，不在 UI 线程直接控制设备或算法。
    mainLayout->addWidget(CreateBottomControlBar(root), 0, Qt::AlignHCenter);

    // 只保存弱引用。Activate 必须由宿主在页面真正成为当前步骤后调用，避免重复激活日志。
    m_root = root;
    WriteLog(LogLevel::Info, "CreateWidget", "Scan workflow widget created");
    return root;
}

// 校验并保存轻量会话 JSON。
bool ScanWorkflowUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    // 先解析局部候选值。非法输入不能覆盖上一份有效流程，否则 Scan/Process 两页会显示不同状态。
    const QByteArray candidateJson(contextJsonUtf8 ? contextJsonUtf8 : "");
    if (!candidateJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(candidateJson, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            WriteLog(LogLevel::Warning,
                     "SetSessionContextJson",
                     QString("Invalid session context: %1").arg(parseError.errorString()));
            return false;
        }
    }

    // 空字符串表示清空上下文并回退默认练习流程；未知 JSON 字段由 ResolveScanProcessSteps 忽略。
    m_contextJson = candidateJson;
    RefreshScanProcessButtons();
    WriteLog(LogLevel::Info, "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

// 保存宿主动作回调。
void ScanWorkflowUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // 纯 C 回调避免跨 DLL 暴露 QObject、Qt signal 或 std::function 的编译器 ABI。
    m_actionCallback = callback;
    m_actionContext = context;
}

// 激活扫描阶段。
void ScanWorkflowUIImpl::Activate() {
    // 当前只刷新显示；真实设备会话恢复应由后续扫描业务服务执行，不能写进 UI。
    UpdateDisplayedScanData();
    WriteLog(LogLevel::Info, "Activate", "Scan workflow activated");
}

// 离开 Scan 页面前释放 VTK/OpenGL 重资源。
void ScanWorkflowUIImpl::DeactivateAndRelease() {
    // QVTKWidget 持有原生 OpenGL 上下文，只 hide 不会归还显存。
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

    // vtkRenderer::New 使用 VTK 引用计数，必须与 Delete 成对，不能使用 C++ delete。
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

// 返回模块代码版本。
const char* ScanWorkflowUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭模块并清空缓存状态。
void ScanWorkflowUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Scan workflow shutdown");
    DeactivateAndRelease();
    if (m_logger) {
        // Flush 当前是兼容空操作，保留调用用于表达退出前完成日志的生命周期意图。
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

// 创建扫描流程按钮宿主栏。
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

// 根据会话上下文重建扫描流程按钮。
void ScanWorkflowUIImpl::RefreshScanProcessButtons() {
    if (!m_scanModeBar) {
        return;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(m_scanModeBar->layout());
    if (!layout) {
        return;
    }

    // 清空旧按钮，避免重新进入扫描页时继续显示上一单流程。
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
    if (m_currentStepIndex < 0
        || m_currentStepIndex >= m_scanProcessSteps.size()
        || !m_scanProcessSteps.at(m_currentStepIndex).enabled) {
        // 当前索引失效或被禁用时选择第一个可用步骤；全禁用则保持 -1，不伪装成可进入状态。
        m_currentStepIndex = -1;
        for (int i = 0; i < m_scanProcessSteps.size(); ++i) {
            if (m_scanProcessSteps.at(i).enabled) {
                m_currentStepIndex = i;
                break;
            }
        }
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

// 从 JSON 上下文解析扫描流程。
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
        // 没有有效步骤时使用练习模式默认流程，不把规则复制到 Process 之外的其它调用方。
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

// 切换当前扫描流程步骤。
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

// 刷新流程按钮选中状态和 QSS。
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

// 更新当前扫描部位的提示和占位数据。
void ScanWorkflowUIImpl::UpdateDisplayedScanData() {
    if (m_scanProcessSteps.isEmpty()) {
        m_scanProcessSteps = ResolveScanProcessSteps();
    }
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_scanProcessSteps.size()) {
        // 全部步骤被禁用时不越权选择索引 0；清空 actor 并给出明确状态。
        if (m_statusLabel) {
            m_statusLabel->setText(tr("No enabled scan step"));
            m_statusLabel->setToolTip(QString());
        }
        if (m_renderer && m_vtkWidget && m_vtkWidget->GetRenderWindow()) {
            m_renderer->RemoveAllViewProps();
            m_vtkWidget->GetRenderWindow()->Render();
        }
        return;
    }

    const ScanProcessStepInfo step = m_scanProcessSteps.value(m_currentStepIndex);
    if (m_statusLabel) {
        // 左下角提示框属于 Scan 页面，文案与 Process 页面保持独立。
        m_statusLabel->setText(tr("Scanning: %1").arg(step.label));
        m_statusLabel->setToolTip(tr("Current scan step code: %1").arg(step.code));
    }
    RebuildStepPlaceholderScene();
}

// 创建右侧扫描工具栏。
QWidget* ScanWorkflowUIImpl::CreateRightToolBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("toolPanel", true);
    frame->setFixedWidth(86);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 显示文字可翻译，日志/回调操作名必须使用稳定英文 code。
    struct ToolEntry {
        QString text;
        const char* code;
    };
    const QList<ToolEntry> tools = {
        {tr("True Color"), "true_color"},
        {tr("Model"), "model"},
        {tr("Color"), "color"},
        {tr("Edit"), "edit"},
    };

    for (const ToolEntry& tool : tools) {
        auto* button = new QPushButton(tool.text, frame);
        button->setObjectName(QString("ScanTool_%1_Button").arg(QString::fromLatin1(tool.code)));
        button->setMinimumHeight(54);
        button->setCursor(Qt::PointingHandCursor);
        QObject::connect(button, &QPushButton::clicked, [this, tool]() {
            // 工具点击只改变扫描页意图，算法执行继续位于 UI 边界之外。
            EmitAction(ScanWorkflowActionToolChanged,
                       QString("ToolChanged:%1").arg(QString::fromLatin1(tool.code)));
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return frame;
}

// 创建扫描页底部控制栏。
QWidget* ScanWorkflowUIImpl::CreateBottomControlBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);

    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(18);

    auto* playButton = new QPushButton(tr("Start / Pause"), frame);
    playButton->setObjectName("ScanStartPauseButton");
    auto* completeButton = new QPushButton(tr("Complete"), frame);
    completeButton->setObjectName("ScanCompleteButton");
    auto* deleteButton = new QPushButton(tr("Delete"), frame);
    deleteButton->setObjectName("ScanDeleteButton");
    playButton->setCursor(Qt::PointingHandCursor);
    completeButton->setCursor(Qt::PointingHandCursor);
    deleteButton->setCursor(Qt::PointingHandCursor);
    completeButton->setProperty("primary", true);

    // 三个按钮都只把用户意图转换成稳定 actionId；宿主/流程服务决定是否真正开始、完成或删除扫描。
    QObject::connect(playButton, &QPushButton::clicked, [this]() {
        // clicked 信号在 UI 线程同步进入 lambda，再通过 EmitAction 调用宿主登记的 C 回调。
        EmitAction(ScanWorkflowActionStartPause, "StartPause");
    });
    QObject::connect(completeButton, &QPushButton::clicked, [this]() {
        // UI 不在回调中保存订单状态，防止页面层和后续 WorkflowService 形成两份状态源。
        EmitAction(ScanWorkflowActionComplete, "Complete");
    });
    QObject::connect(deleteButton, &QPushButton::clicked, [this]() {
        // 删除同样只上报请求，确认策略和数据删除由业务层实现。
        EmitAction(ScanWorkflowActionDelete, "Delete");
    });

    layout->addWidget(playButton);
    layout->addWidget(completeButton);
    layout->addWidget(deleteButton);
    return frame;
}

// 创建 Scan 专属左下提示区。
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

// 创建嵌入式 VTK 视图。
QWidget* ScanWorkflowUIImpl::CreateViewerArea(QWidget* parent) {
    // QVTKWidget 是当前 Qt 5.6 与 VTK 8 的桥接控件，父对象负责 QWidget 生命周期。
    m_vtkWidget = new ScanWorkflowViewerWidget(parent);
    m_vtkWidget->setObjectName("ScanWorkflowVTKWidget");
    m_vtkWidget->setMinimumSize(520, 340);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    BuildPlaceholderScene();
    return m_vtkWidget;
}

// 创建初始 VTK 占位场景。
void ScanWorkflowUIImpl::BuildPlaceholderScene() {
    if (!m_vtkWidget) {
        return;
    }

    // renderer 管理本页 camera、背景和 actor 列表；初始引用由 New 返回。
    m_renderer = vtkRenderer::New();
    m_renderer->SetBackground(0.87, 0.89, 0.92);
    m_vtkWidget->SetRenderer(m_renderer);

    // QVTKWidget 暴露的 vtkRenderWindow 真正持有 OpenGL 渲染上下文。
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    RebuildStepPlaceholderScene();
}

// 根据当前扫描步骤重建轻量占位几何。
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

// 写客户操作日志并向宿主上报动作。
void ScanWorkflowUIImpl::EmitAction(int actionId, const QString& operation) {
    // 命名 QByteArray 让 UTF-8 字节在整个 WriteLog 调用期间保持有效，
    // 也明确说明 Logger 的公共边界接收 char*，并不接收 QString 对象。
    const QByteArray operationBytes = operation.toUtf8();
    WriteLog(LogLevel::Info, operationBytes.constData(),
             QString("Scan workflow action: %1").arg(actionId));
    if (m_actionCallback) {
        // DLL 边界只穿过稳定整数和宿主原样传入的 context 指针。
        m_actionCallback(m_actionContext, actionId);
    }
}

// Logger 可用时写结构化日志。
void ScanWorkflowUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 壳子通过 QLibrary/GetProcAddress 动态解析的 C ABI 工厂。
extern "C" MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI* GetScanWorkflowUI() {
    return &ScanWorkflowUIImpl::Instance();
}

// 运行时 versionList 使用的统一代码版本导出。
extern "C" MEYERSCAN_SCANWORKFLOWUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
        // 只读取约定的 scanProcess.steps，患者/订单其它字段在本 UI 中保持不透明。
