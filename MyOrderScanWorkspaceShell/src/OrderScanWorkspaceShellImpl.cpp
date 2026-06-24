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
    static OrderScanWorkspaceShellImpl instance;
    return instance;
}

// 初始化壳子模块。
// 壳子只准备路径和日志，不主动创建建单/扫描页面，页面由调用方按需挂入。
bool OrderScanWorkspaceShellImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 缓存路径字节，避免调用方临时 QByteArray 销毁后本模块仍持有悬空指针。
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
    root->setObjectName("MeyerScanOrderScanWorkspaceShellRoot");
    root->setMinimumSize(980, 620);

    // 使用纵向布局：顶部当前步骤、步骤条、中间页面栈。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    // 当前步骤标题单独显示，便于后续加状态、进度、返回等工具按钮。
    m_stepLabel = new QLabel(root);
    QFont font = m_stepLabel->font();
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
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(34);
        label->setStyleSheet("QLabel{border:1px solid #cfd8dc;border-radius:4px;padding:6px;color:#23313f;background:#f7f9fb;}");
        stepBar->addWidget(label);
    }
    layout->addLayout(stepBar);

    // QStackedWidget 负责页面切换，避免多个顶层窗口 close/show 造成闪现。
    m_stack = new QStackedWidget(root);
    for (int step : steps) {
        QWidget* widget = m_stepWidgets.value(step, nullptr);
        if (!widget) {
            // 某一步尚未接入真实模块时使用占位页，保证壳子本身可以独立运行和测试。
            widget = CreatePlaceholder(step, root);
            m_stepWidgets.insert(step, widget);
        }
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
        const int index = m_stack->indexOf(oldWidget);
        if (index >= 0) {
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
    // 工作台壳当前没有真实操作员上下文，传空字符串让 Logger 省略 Op 字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// 导出工作区壳子模块实例。
extern "C" MEYERSCAN_ORDERSCANWORKSPACESHELL_API IOrderScanWorkspaceShell* GetOrderScanWorkspaceShell() {
    return &OrderScanWorkspaceShellImpl::Instance();
}
