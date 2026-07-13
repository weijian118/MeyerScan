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
// 结构化日志的 module 字段使用稳定英文名，不随界面语言变化。
const char* Name = "MeyerScan_DataProcessUI";

// GetModuleVersion 返回此代码版本，必须与 CMakeLists.txt 和 Version.rc 同步。
const char* Version = "MeyerScan_DataProcessUI v0.2.3 (2026-07-12)";
}
}

// 数据处理页的 VTK 视图控件。
// 它只接管滚轮缩放，把缩放范围和鼠标中心缩放逻辑集中在一个地方。
class DataProcessViewerWidget : public QVTKWidget {
public:
    explicit DataProcessViewerWidget(QWidget* parent = nullptr)
        : QVTKWidget(parent) {
        // WheelFocus 让鼠标悬停时直接接收滚轮，不要求用户先点击视图取得焦点。
        setFocusPolicy(Qt::WheelFocus);
    }

    // renderer 由 DataProcessUIImpl 拥有；本控件只借用它做事件响应。
    void SetRenderer(vtkRenderer* renderer) {
        m_renderer = renderer;
    }

protected:
    // 在限定范围内以鼠标位置为中心缩放当前 VTK 场景。
    void wheelEvent(QWheelEvent* event) override {
        // renderer/camera/renderWindow 任一不可用时交还基类，避免空指针崩溃。
        if (!event || !m_renderer || !m_renderer->GetActiveCamera() || !GetRenderWindow()) {
            QVTKWidget::wheelEvent(event);
            return;
        }

        // Qt5 的 angleDelta().y() 以 120 表示常见鼠标滚轮一格。
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            event->accept();
            return;
        }

        // 指数缩放让多格滚动连续；先夹紧目标值，再只应用实际比例，避免越界后视觉回弹。
        const double wheelSteps = static_cast<double>(delta) / 120.0;
        const double zoomRatio = std::pow(1.12, wheelSteps);
        const double targetZoom = qBound(0.35, m_zoomFactor * zoomRatio, 4.0);
        const double appliedRatio = targetZoom / m_zoomFactor;
        if (qFuzzyCompare(appliedRatio, 1.0)) {
            // 已到 0.35/4.0 边界时不再修改 camera。
            event->accept();
            return;
        }

        vtkCamera* camera = m_renderer->GetActiveCamera();
        double beforeWorld[4] = {0.0, 0.0, 0.0, 1.0};
        double afterWorld[4] = {0.0, 0.0, 0.0, 1.0};
        // VTK display 坐标原点位于左下，Qt 鼠标坐标原点位于左上，因此 y 轴需要翻转。
        const QPoint mousePos = event->pos();
        const int displayX = mousePos.x();
        const int displayY = height() - mousePos.y();
        // 先记录缩放前鼠标对应世界点，再缩放相机并读取缩放后同一像素的世界点。
        ReadWorldPointAtDisplay(displayX, displayY, beforeWorld);

        camera->Zoom(appliedRatio);
        m_zoomFactor = targetZoom;
        ReadWorldPointAtDisplay(displayX, displayY, afterWorld);

        // 同步平移相机位置和焦点，使鼠标指向的世界点在屏幕上保持不动。
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
    // 把 display 像素投影到当前相机焦点深度所在的世界坐标。
    void ReadWorldPointAtDisplay(int displayX, int displayY, double worldPoint[4]) const {
        if (!m_renderer || !m_renderer->GetActiveCamera()) {
            return;
        }

        // 先把相机焦点投影到 display，取得模型附近的 z 深度。
        double focalPoint[4] = {0.0, 0.0, 0.0, 1.0};
        m_renderer->GetActiveCamera()->GetFocalPoint(focalPoint);
        m_renderer->SetWorldPoint(focalPoint);
        m_renderer->WorldToDisplay();
        double focalDisplay[3] = {0.0, 0.0, 0.0};
        m_renderer->GetDisplayPoint(focalDisplay);

        // 再组合鼠标 x/y 与焦点 z 反投影；齐次坐标 w 非零时归一化到三维坐标。
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

// 返回 DLL 持有的数据处理 UI 单例。
DataProcessUIImpl& DataProcessUIImpl::Instance() {
    // C++11 保证函数内静态对象只构造一次，调用方不能跨 DLL delete。
    static DataProcessUIImpl instance;
    return instance;
}

// 初始化数据处理阶段路径和 Logger。
bool DataProcessUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 立即复制 UTF-8 参数，调用结束后调用方临时 QByteArray 可以安全释放。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 应用目录为空时回退到 Qt 保存的 EXE 目录，不依赖 currentPath。
    if (m_appDir.isEmpty()) {
        m_appDir = QCoreApplication::applicationDirPath().toUtf8();
    }

    // Logger 指针只获取一次，后续日志持续复用 m_logger。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        if (!m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
            // 日志失败不阻断处理页骨架，但后续不再使用半初始化 Logger。
            m_logger = nullptr;
        }
    }

    // 当前占位版没有处理算法；1x1 cv::Mat 只验证 OpenCV 3.3 头文件和链接配置。
    // 后续真实算法必须进入独立处理 DLL，不能继续堆入 UI 文件。
    cv::Mat buildProbe(1, 1, CV_8UC1);
    buildProbe.setTo(cv::Scalar(0));
    WriteLog(LogLevel::Info, "Init",
             QString("Data process UI initialized, OpenCV probe cols=%1").arg(buildProbe.cols));
    return true;
}

// 创建数据处理阶段根页面。
QWidget* DataProcessUIImpl::CreateWidget(QWidget* parent) {
    // 单例只允许持有一套 QVTK/OpenGL 资源，创建前释放上一套。
    DeactivateAndRelease();

    // 返回 QWidget 由 shell/test host 的 Qt 父子树持有，本模块只保存弱引用。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanDataProcessUIRoot");
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    MeyerQtModule::ApplyModuleQss(root, "MyDataProcessUI", "data_process.qss", m_logger);

    // 页面骨架与 Scan 保持一致，使用 Layout 自适应，不按分辨率写绝对坐标。
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(CreateModelModeBar(root), 0, Qt::AlignHCenter);

    // VTK 视图占据主要伸缩空间，右侧工具保持稳定宽度。
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);
    // Process 也保留左下角提示框，但内容与 Scan 页独立，后续可按处理规则单独替换文案。
    centerLayout->addWidget(CreateHintPanel(root), 0, Qt::AlignBottom);
    centerLayout->addWidget(CreateViewerArea(root), 1);
    centerLayout->addWidget(CreateProcessingToolBar(root), 0, Qt::AlignVCenter);
    mainLayout->addLayout(centerLayout, 1);

    // Process 页不放扫描 Start/Pause，底部只保留前后步骤和状态。
    mainLayout->addWidget(CreateBottomStatusBar(root), 0, Qt::AlignHCenter);

    // Activate 由宿主在页面成为当前步骤后调用，避免重复激活。
    m_root = root;
    WriteLog(LogLevel::Info, "CreateWidget", "Data process widget created");
    return root;
}

// 校验并保存轻量会话 JSON。
bool DataProcessUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    // 先解析局部候选值，失败时保留上一份有效流程，避免 Process 与 Scan 状态分裂。
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

    // 空字符串表示清空上下文并回退默认练习流程；其它业务字段在本 UI 中保持不透明。
    m_contextJson = candidateJson;
    RefreshScanProcessButtons();
    WriteLog(LogLevel::Info, "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

// 保存宿主动作回调。
void DataProcessUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // 纯 C 回调避免 QObject、Qt signal 或 std::function ABI 穿过 DLL 边界。
    m_actionCallback = callback;
    m_actionContext = context;
}

// 激活数据处理阶段。
void DataProcessUIImpl::Activate() {
    // 当前只刷新显示；真实处理状态和模型加载由后续业务服务负责。
    UpdateDisplayedProcessData();
    WriteLog(LogLevel::Info, "Activate", "Data process activated");
}

// 离开 Process 页面前释放 VTK/OpenGL 重资源。
void DataProcessUIImpl::DeactivateAndRelease() {
    // QVTKWidget 持有原生 OpenGL 上下文，只 hide 不会释放显存。
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

    // vtkRenderer::New 使用 VTK 引用计数，必须通过 Delete 释放，不能使用 C++ delete。
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

// 返回模块代码版本。
const char* DataProcessUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭模块并清空缓存状态。
void DataProcessUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Data process UI shutdown");
    DeactivateAndRelease();
    if (m_logger) {
        // Flush 当前为兼容空操作，保留调用用于表达退出前完成日志的意图。
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

// 创建处理流程按钮宿主栏。
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

// 根据会话上下文重建处理流程按钮。
void DataProcessUIImpl::RefreshScanProcessButtons() {
    if (!m_modelModeBar) {
        return;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(m_modelModeBar->layout());
    if (!layout) {
        return;
    }

    // 清空旧按钮，避免处理页复用时残留上一单流程。
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_processButtons.clear();

    // Process 只消费已有 scanProcess.steps，避免复制建单规则或推导逻辑。
    m_processSteps = ResolveScanProcessSteps();
    if (m_currentStepIndex < 0
        || m_currentStepIndex >= m_processSteps.size()
        || !m_processSteps.at(m_currentStepIndex).enabled) {
        // 当前索引失效或被禁用时选择第一个可用步骤；全禁用时保持 -1。
        m_currentStepIndex = -1;
        for (int i = 0; i < m_processSteps.size(); ++i) {
            if (m_processSteps.at(i).enabled) {
                m_currentStepIndex = i;
                break;
            }
        }
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

// 从与 Scan 页面相同的 JSON 上下文解析处理流程。
QVector<DataProcessUIImpl::ProcessStepInfo> DataProcessUIImpl::ResolveScanProcessSteps() const {
    QVector<ProcessStepInfo> steps;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_contextJson, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        // 本页只读取 scanProcess.steps，不解释患者、订单和建单开关。
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
        // 没有有效步骤时使用练习模式默认四步，保持 Scan/Process 初始体验一致。
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

// 切换当前数据处理步骤。
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

// 刷新处理流程按钮选中状态和 QSS。
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

// 更新当前处理部位的提示和占位数据。
void DataProcessUIImpl::UpdateDisplayedProcessData() {
    if (m_processSteps.isEmpty()) {
        m_processSteps = ResolveScanProcessSteps();
    }
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_processSteps.size()) {
        // 全部步骤被禁用时不越权选择索引 0，并清空旧 actor。
        if (m_statusLabel) {
            m_statusLabel->setText(tr("No enabled process step"));
            m_statusLabel->setToolTip(QString());
        }
        if (m_hintLabel) {
            m_hintLabel->setText(tr("No process step is currently available."));
        }
        if (m_renderer && m_vtkWidget && m_vtkWidget->GetRenderWindow()) {
            m_renderer->RemoveAllViewProps();
            m_vtkWidget->GetRenderWindow()->Render();
        }
        return;
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

// 创建右侧处理工具栏。
QWidget* DataProcessUIImpl::CreateProcessingToolBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("toolPanel", true);
    frame->setFixedWidth(86);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 单项同时保存可翻译文字、稳定 actionId 和稳定英文日志 operation。
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
        button->setObjectName(QString("ProcessTool_%1_Button").arg(item.operation));
        button->setMinimumHeight(54);
        button->setCursor(Qt::PointingHandCursor);
        QObject::connect(button, &QPushButton::clicked, [this, item]() {
            // UI 只上报工具意图，截图/编辑/分析算法继续位于独立业务 DLL。
            EmitAction(item.actionId, item.operation);
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return frame;
}

// 创建嵌入式 VTK 视图。
QWidget* DataProcessUIImpl::CreateViewerArea(QWidget* parent) {
    // QVTKWidget 是 Qt 5.6 与 VTK 8 的桥接控件，QWidget 生命周期由父对象管理。
    m_vtkWidget = new DataProcessViewerWidget(parent);
    m_vtkWidget->setObjectName("DataProcessVTKWidget");
    m_vtkWidget->setMinimumSize(540, 340);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    BuildPlaceholderScene();
    return m_vtkWidget;
}

// 创建 Process 专属左下提示区。
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

// 创建底部状态和流程导航栏。
QWidget* DataProcessUIImpl::CreateBottomStatusBar(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("panel", true);
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(12);

    auto* previousButton = new QPushButton(tr("Previous"), frame);
    previousButton->setObjectName("ProcessPreviousButton");
    auto* nextButton = new QPushButton(tr("Next"), frame);
    nextButton->setObjectName("ProcessNextButton");
    previousButton->setCursor(Qt::PointingHandCursor);
    nextButton->setCursor(Qt::PointingHandCursor);
    nextButton->setProperty("primary", true);
    m_statusLabel = new QLabel(tr("Ready"), frame);
    m_statusLabel->setProperty("muted", true);

    // 底部导航只上报跨步骤意图，不直接持有或调用 WorkspaceShell，避免内容页反向依赖壳子。
    QObject::connect(previousButton, &QPushButton::clicked, [this]() {
        EmitAction(DataProcessActionPrevious, "Previous");
    });
    QObject::connect(nextButton, &QPushButton::clicked, [this]() {
        // 宿主收到 Next 后决定进入发送页还是拒绝切换，处理 UI 不自行推进工作台状态。
        EmitAction(DataProcessActionNext, "Next");
    });

    layout->addWidget(previousButton);
    layout->addWidget(m_statusLabel, 1);
    layout->addWidget(nextButton);
    return frame;
}

// 创建初始 VTK 占位场景。
void DataProcessUIImpl::BuildPlaceholderScene() {
    if (!m_vtkWidget) {
        return;
    }

    // renderer 管理本页 camera、背景和 actor 列表，初始引用由 New 返回。
    m_renderer = vtkRenderer::New();
    m_renderer->SetBackground(0.87, 0.89, 0.92);
    m_vtkWidget->SetRenderer(m_renderer);

    // QVTKWidget 暴露的 vtkRenderWindow 真正持有 OpenGL 渲染上下文。
    m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);
    RebuildStepPlaceholderScene();
}

// 根据当前处理步骤重建轻量占位几何。
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

// 写客户操作日志并向宿主上报动作。
void DataProcessUIImpl::EmitAction(int actionId, const QString& operation) {
    // 显式保存 UTF-8 缓冲区，保证 constData 在 WriteLog 调用期间有效。
    const QByteArray operationBytes = operation.toUtf8();
    WriteLog(LogLevel::Info, operationBytes.constData(),
             QString("Data process action: %1").arg(actionId));
    if (m_actionCallback) {
        // DLL 边界只穿过稳定整数和宿主原样传入的 context 指针。
        m_actionCallback(m_actionContext, actionId);
    }
}

// Logger 可用时写结构化日志。
void DataProcessUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 壳子通过 QLibrary/GetProcAddress 动态解析的 C ABI 工厂。
extern "C" MEYERSCAN_DATAPROCESSUI_API IDataProcessUI* GetDataProcessUI() {
    return &DataProcessUIImpl::Instance();
}

// 运行时 versionList 使用的统一代码版本导出。
extern "C" MEYERSCAN_DATAPROCESSUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
