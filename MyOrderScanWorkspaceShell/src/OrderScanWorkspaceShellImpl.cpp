#include "OrderScanWorkspaceShellImpl.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_OrderScanWorkspaceShell";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_OrderScanWorkspaceShell v0.1.0 (2026-06-23)";
}
}

// 返回工作区壳子单例。
OrderScanWorkspaceShellImpl& OrderScanWorkspaceShellImpl::Instance() {
    // 壳子模块保存当前步骤和已挂载页面，使用单例让 MainExe 多次获取时仍是同一份状态。
    static OrderScanWorkspaceShellImpl instance;
    return instance;
}

// 初始化壳子模块。
// 壳子只准备路径和日志，不主动创建建单/扫描页面，页面由调用方按需挂入。
bool OrderScanWorkspaceShellImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 缓存路径字节，避免调用方临时 QByteArray 销毁后本模块仍持有悬空指针。
    // QByteArray 会复制 const char* 内容，后续日志初始化可安全使用 constData()。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 缓存日志接口，后续所有用户操作和步骤切换都复用这一个指针。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }
    WriteLog(LogLevel::Info, "Init", "Workspace shell initialized");
    return true;
}

// 创建建单/扫描工作区根界面。
// 当前实现包含顶部步骤条和中间 QStackedWidget，真实业务页面通过 AttachStepWidget 接入。
QWidget* OrderScanWorkspaceShellImpl::CreateWidget(QWidget* parent) {
    // 工作区根界面由 MainExe 或上层工作区容器持有。
    auto* root = new QWidget(parent);
    // objectName 用于样式表、自动化测试和调试时识别控件树。
    root->setObjectName("MeyerScanOrderScanWorkspaceShellRoot");
    // 最小尺寸保护占位页面和步骤条不会被压到不可用。
    root->setMinimumSize(980, 620);

    // 使用纵向布局：顶部当前步骤、步骤条、中间页面栈。
    auto* layout = new QVBoxLayout(root);
    // 统一内容边距，避免每个子页面自己贴边处理。
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    // 当前步骤标题单独显示，便于后续加状态、进度、返回等工具按钮。
    m_stepLabel = new QLabel(root);
    QFont font = m_stepLabel->font();
    // 复制当前字体后只改大小/粗细，保留系统字体族和语言渲染能力。
    font.setPointSize(14);
    font.setBold(true);
    m_stepLabel->setFont(font);
    layout->addWidget(m_stepLabel);

    auto* stepBar = new QHBoxLayout();
    const int steps[] = {
        WorkspaceStepOrderCreate,
        WorkspaceStepScan,
        WorkspaceStepProcess,
        WorkspaceStepSend,
    };

    // 顶部步骤条当前只展示文字。后续可把 QLabel 替换成可点击步骤控件。
    for (int step : steps) {
        auto* label = new QLabel(StepTitle(step), root);
        // 居中展示步骤名，后续替换成可点击步骤控件时也能沿用这一排布局。
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(34);
        label->setStyleSheet("QLabel{border:1px solid #cfd8dc;border-radius:4px;padding:6px;color:#23313f;background:#f7f9fb;}");
        // QHBoxLayout 会平均分配可用空间，让四个步骤视觉上均匀排列。
        stepBar->addWidget(label);
    }
    layout->addLayout(stepBar);

    // QStackedWidget 负责页面切换，避免多个顶层窗口 close/show 造成闪现。
    m_stack = new QStackedWidget(root);
    for (int step : steps) {
        // value(step, nullptr) 让未登记页面时返回空指针，而不是插入默认项。
        QWidget* widget = m_stepWidgets.value(step, nullptr);
        if (!widget) {
            // 某一步尚未接入真实模块时使用占位页，保证壳子本身可以独立运行和测试。
            widget = CreatePlaceholder(step, root);
            // 将占位页记录到映射表，后续 SetStep 可以直接根据 step 找页面。
            m_stepWidgets.insert(step, widget);
        }
        // QStackedWidget 只显示当前页，其它页保持隐藏，从而避免顶层窗口切换闪烁。
        m_stack->addWidget(widget);
    }
    layout->addWidget(m_stack, 1);

    // 保存根界面弱引用，Shutdown 只清状态，不主动删除 root。
    m_root = root;

    // 创建后立即同步当前步骤，让标题和 stack 当前页一致。
    SetStep(m_currentStep);
    WriteLog(LogLevel::Info, "CreateWidget", "Workspace shell widget created");
    return root;
}

// 切换当前工作步骤。
// 如果收到非法步骤，只写 Warning 日志并保持当前页面不变。
void OrderScanWorkspaceShellImpl::SetStep(int step) {
    if (step < WorkspaceStepOrderCreate || step > WorkspaceStepSend) {
        // 非法步骤不抛异常，避免外部误传 step 时把主界面打崩。
        WriteLog(LogLevel::Warning, "SetStep", QString("Invalid step: %1").arg(step));
        return;
    }

    // 先更新状态，再切换界面。即使 m_stack 尚未创建，后续 CreateWidget 也会按 m_currentStep 展示。
    m_currentStep = step;
    if (m_stack) {
        QWidget* widget = m_stepWidgets.value(step, nullptr);
        if (widget) {
            // setCurrentWidget 要求 widget 已经在 stack 内；Attach/CreateWidget 会保证这一点。
            m_stack->setCurrentWidget(widget);
        }
    }
    RefreshStepLabel();
    WriteLog(LogLevel::Info, "SetStep", QString("Current step: %1").arg(step));
}

// 挂入指定步骤的真实 QWidget。
// 用于把建单模块、扫描重建页面、处理页面或发送页面接到统一壳子里。
void OrderScanWorkspaceShellImpl::AttachStepWidget(int step, QWidget* widget) {
    if (step < WorkspaceStepOrderCreate || step > WorkspaceStepSend || !widget) {
        // step 非法或 widget 为空都说明调用方接入有问题，记录 Warning 后保持旧页面。
        WriteLog(LogLevel::Warning, "AttachStepWidget", "Invalid step widget");
        return;
    }

    // 如果该步骤已经有页面，先从 stack 移除旧页面，再登记新页面。
    QWidget* oldWidget = m_stepWidgets.value(step, nullptr);
    if (m_stack && oldWidget) {
        // indexOf 返回 -1 表示旧页面没有加入当前 stack，可能是 CreateWidget 前登记的页面。
        const int index = m_stack->indexOf(oldWidget);
        if (index >= 0) {
            // removeWidget 只从 stack 管理列表移除，不会 delete 页面。
            m_stack->removeWidget(oldWidget);
        }
        // 旧页面可能属于业务模块，使用 deleteLater 避免在当前事件处理中立即析构导致 Qt 信号链不稳定。
        oldWidget->deleteLater();
    }

    // 更新映射表。即使 m_stack 还没创建，CreateWidget 时也能直接使用真实页面。
    m_stepWidgets.insert(step, widget);
    if (m_stack) {
        // 新页面加入 stack 后，如果它正是当前步骤，需要立即切过去。
        m_stack->addWidget(widget);
        if (m_currentStep == step) {
            m_stack->setCurrentWidget(widget);
        }
    }
    WriteLog(LogLevel::Info, "AttachStepWidget", QString("Step widget attached: %1").arg(step));
}

// 返回模块版本字符串。
const char* OrderScanWorkspaceShellImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭壳子模块。
// 不主动 delete 根界面，由 MainExe/父 QWidget 管理真实销毁。
void OrderScanWorkspaceShellImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Workspace shell shutdown");
    if (m_logger) {
        // 退出前刷新日志，确保最后的步骤切换或页面挂载记录落盘。
        m_logger->Flush();
    }

    // 清空所有 QWidget 弱引用。真实对象由 Qt 父子树或外部容器销毁。
    // 这里不遍历 delete m_stepWidgets，因为其中可能包含外部模块创建并由父对象持有的页面。
    m_root = nullptr;
    m_stepLabel = nullptr;
    m_stack = nullptr;
    m_stepWidgets.clear();
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
    m_currentStep = WorkspaceStepOrderCreate;
}

// 根据步骤 ID 返回可翻译标题。
// 源码文案保持英文并包在 tr() 中，后续由 qm 文件提供中文/其他语言翻译。
QString OrderScanWorkspaceShellImpl::StepTitle(int step) const {
    // switch 只做 ID 到显示标题的映射，不做流程判断。
    switch (step) {
    case WorkspaceStepOrderCreate:
        // 建单步骤标题。真实建单页面由外部模块挂载，不在这里创建业务表单。
        return tr("Order");
    case WorkspaceStepScan:
        // 扫描步骤标题。后续接入 ScanReconstructStudio 或扫描页面占位时复用该 ID。
        return tr("Scan");
    case WorkspaceStepProcess:
        // 后处理步骤标题。这里仅显示步骤，不承载算法处理逻辑。
        return tr("Process");
    case WorkspaceStepSend:
        // 发送步骤标题。后续订单上传/导出流程由 Workflow/Service 决定。
        return tr("Send");
    default:
        // 未知步骤保底显示 Unknown，避免非法 ID 导致空标题或崩溃。
        return tr("Unknown");
    }
}

// 创建步骤占位页面。
// 占位页让壳子模块在真实建单/扫描页面尚未接入前也能独立运行和联调。
QWidget* OrderScanWorkspaceShellImpl::CreatePlaceholder(int step, QWidget* parent) const {
    // QLabel 作为占位页足够轻，便于工作区壳子在真实模块接入前独立 smoke。
    // tr("%1 placeholder").arg(...) 先翻译模板，再填入步骤标题，便于多语言语序调整。
    auto* widget = new QLabel(tr("%1 placeholder").arg(StepTitle(step)), parent);
    widget->setObjectName(QString("WorkspaceStep%1Placeholder").arg(step));
    widget->setAlignment(Qt::AlignCenter);
    widget->setStyleSheet("QLabel{color:#607080;background:#ffffff;border:1px dashed #b0bec5;}");
    return widget;
}

// 刷新当前步骤标题。
void OrderScanWorkspaceShellImpl::RefreshStepLabel() {
    if (m_stepLabel) {
        // 标题根据当前步骤动态拼接。源文案仍保持英文，翻译由 qm 处理。
        // m_stepLabel 可能在 CreateWidget 前为空，所以调用方可先 SetStep 再创建界面。
        m_stepLabel->setText(tr("Current Step: %1").arg(StepTitle(m_currentStep)));
    }
}

// 写结构化日志。
// 日志不可用时直接跳过，壳子界面不能因为日志 DLL 问题影响主流程。
void OrderScanWorkspaceShellImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }

    // Logger ABI 使用 UTF-8 C 字符串，Qt 字符串在调用前转换。
    const QByteArray bytes = content.toUtf8();
    // bytes 的生命周期覆盖整个 Write 调用，constData() 指针不会悬空。
    // 工作台壳当前没有真实操作员上下文，传空字符串让 Logger 省略 Op 字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// 导出工作区壳子模块实例。
extern "C" MEYERSCAN_ORDERSCANWORKSPACESHELL_API IOrderScanWorkspaceShell* GetOrderScanWorkspaceShell() {
    // 保持 C ABI 导出名稳定，方便 MainExe 或测试宿主动态加载该 DLL。
    return &OrderScanWorkspaceShellImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取工作台壳代码版本时，不创建工作台页面或步骤占位控件。
extern "C" MEYERSCAN_ORDERSCANWORKSPACESHELL_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
