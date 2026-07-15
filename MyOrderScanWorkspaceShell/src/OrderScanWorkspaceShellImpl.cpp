#include "OrderScanWorkspaceShellImpl.h"

#include "MeyerQtModuleUtils.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStackedWidget>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_OrderScanWorkspaceShell";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_OrderScanWorkspaceShell v0.1.4 (2026-07-15)";
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
        if (!m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
            // 壳子仍可执行页面导航，但后续不能继续使用半初始化 Logger。
            m_logger = nullptr;
        }
    }
    WriteLog(LogLevel::Info, "Init", "Workspace shell initialized");
    return true;
}

// 创建建单/扫描工作区根界面。
// 当前实现包含顶部步骤条和中间 QStackedWidget，真实业务页面通过 AttachStepWidget 接入。
QWidget* OrderScanWorkspaceShellImpl::CreateWidget(QWidget* parent) {
    // 每次创建新的根界面前先清空按钮弱引用。
    // m_stepWidgets 不能在这里清空，因为调用方可能先 AttachStepWidget 再 CreateWidget。
    m_stepButtons.clear();

    // 工作区根界面由 MainExe 或上层工作区容器持有。
    auto* root = new QWidget(parent);
    // objectName 用于样式表、自动化测试和调试时识别控件树。
    root->setObjectName("MeyerScanOrderScanWorkspaceShellRoot");
    // 最小尺寸保护占位页面和步骤条不会被压到不可用。
    root->setMinimumSize(980, 620);
    MeyerQtModule::ApplyModuleQss(root, "MyOrderScanWorkspaceShell", "workspace_shell.qss", m_logger);

    // 使用纵向布局：顶部业务导航区 + 中间页面栈。
    // 步骤导航只在壳子中保留一份，OrderCreateUI 不再重复绘制第二套步骤条。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部栏左侧显示当前步骤，右侧只放最小化和关闭，符合创建/练习工作区的窗口控制要求。
    auto* headerFrame = new QFrame(root);
    headerFrame->setObjectName("WorkspaceTopBar");
    auto* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(20, 8, 18, 8);
    headerLayout->setSpacing(10);

    // 品牌与返回按钮属于创建/练习工作台顶部业务区。
    auto* brandLabel = new QLabel(headerFrame);
    brandLabel->setObjectName("WorkspaceBrandLabel");
    const QPixmap brandPixmap(MeyerQtModule::ModuleResourceFile(
        "MyOrderScanWorkspaceShell", "icon/workspace", "logo.png"));
    if (!brandPixmap.isNull()) {
        brandLabel->setPixmap(brandPixmap.scaledToHeight(42, Qt::SmoothTransformation));
    } else {
        brandLabel->setText(tr("MEYER"));
    }
    headerLayout->addWidget(brandLabel, 0, Qt::AlignVCenter);

    auto* backButton = new QToolButton(headerFrame);
    backButton->setObjectName("WorkspaceBackButton");
    backButton->setProperty("workspaceShellAction", WorkspaceShellActionBack);
    backButton->setToolTip(tr("Back"));
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QIcon backIcon;
    backIcon.addFile(MeyerQtModule::ModuleResourceFile(
                         "MyOrderScanWorkspaceShell", "icon/workspace", "back_b.png"),
                     QSize(25, 25), QIcon::Normal, QIcon::Off);
    backIcon.addFile(MeyerQtModule::ModuleResourceFile(
                         "MyOrderScanWorkspaceShell", "icon/workspace", "back_h.png"),
                     QSize(25, 25), QIcon::Active, QIcon::Off);
    backButton->setIcon(backIcon);
    backButton->setIconSize(QSize(25, 25));
    backButton->setFixedSize(42, 38);
    QObject::connect(backButton, &QToolButton::clicked, [this]() {
        EmitShellAction(WorkspaceShellActionBack);
    });
    headerLayout->addWidget(backButton, 0, Qt::AlignVCenter);

    // 创建/练习工作区右上角只保留最小化和关闭两个自绘按钮。
    // 按钮图片来自模块 Resources，窗口本身仍由 MainExe 全屏无边框承载。
    auto createWindowButton = [headerFrame](const QString& objectName,
                                            const QString& tooltip,
                                            const QString& normalIcon,
                                            const QString& hoverIcon) {
        auto* button = new QToolButton(headerFrame);
        button->setObjectName(objectName);
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setMinimumSize(40, 34);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        // QIcon 可以同时登记普通和悬停态图标。
        // QSS 只负责按钮背景/边框，图标资源路径仍通过公共资源函数从 EXE 目录解析。
        QIcon icon;
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyOrderScanWorkspaceShell", "icon/workspace", normalIcon),
                     QSize(22, 22),
                     QIcon::Normal,
                     QIcon::Off);
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyOrderScanWorkspaceShell", "icon/workspace", hoverIcon),
                     QSize(22, 22),
                     QIcon::Active,
                     QIcon::Off);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        return button;
    };

    const QList<int> steps = VisibleSteps();
    headerLayout->addStretch(1);

    // 步骤导航位于工作台唯一顶部区。OrderCreateUI、ScanWorkflowUI 等子页只绘制各自内容，
    // 不再各自复制 Order/Scan/Process/Send 导航，避免状态不一致和重复占用高度。
    for (int step : steps) {
        auto* button = new QPushButton(StepTitle(step), headerFrame);
        button->setProperty("workspaceStep", true);
        // setCheckable 让当前步骤能保持选中状态，避免用户不知道当前处于哪一步。
        button->setCheckable(true);
        button->setMinimumSize(112, 44);
        button->setCursor(Qt::PointingHandCursor);

        // 不同步骤只选择对应业务图标；按钮状态和切换逻辑仍由同一个 step ID 驱动。
        QString iconBaseName;
        switch (step) {
        case WorkspaceStepOrderCreate: iconBaseName = "order"; break;
        case WorkspaceStepScan: iconBaseName = "scan"; break;
        case WorkspaceStepProcess: iconBaseName = "process"; break;
        case WorkspaceStepSend: iconBaseName = "send"; break;
        default: break;
        }
        if (!iconBaseName.isEmpty()) {
            QIcon stepIcon;
            stepIcon.addFile(MeyerQtModule::ModuleResourceFile(
                                 "MyOrderScanWorkspaceShell",
                                 "icon/workspace",
                                 QString("%1_b.png").arg(iconBaseName)),
                             QSize(24, 24), QIcon::Normal, QIcon::Off);
            stepIcon.addFile(MeyerQtModule::ModuleResourceFile(
                                 "MyOrderScanWorkspaceShell",
                                 "icon/workspace",
                                 QString("%1_h.png").arg(iconBaseName)),
                             QSize(24, 24), QIcon::Normal, QIcon::On);
            button->setIcon(stepIcon);
            button->setIconSize(QSize(24, 24));
        }
        // objectName 便于测试宿主和后续自动化脚本定位具体步骤按钮。
        button->setObjectName(QString("WorkspaceStep%1Button").arg(step));
        // lambda 捕获 step 的值；点击按钮后统一进入 HandleStepButtonClicked，避免多个槽函数重复。
        QObject::connect(button, &QPushButton::clicked, [this, step]() {
            HandleStepButtonClicked(step);
        });
        m_stepButtons.insert(step, button);
        headerLayout->addWidget(button, 0, Qt::AlignVCenter);
    }
    headerLayout->addStretch(1);

    auto* minimizeButton = createWindowButton("WorkspaceMinimizeButton",
                                              tr("Minimize"),
                                              "min_b.png",
                                              "min_h.png");
    minimizeButton->setProperty("workspaceShellAction", WorkspaceShellActionMinimize);
    QObject::connect(minimizeButton, &QToolButton::clicked, [this]() {
        EmitShellAction(WorkspaceShellActionMinimize);
    });
    headerLayout->addWidget(minimizeButton, 0, Qt::AlignVCenter);

    auto* closeButton = createWindowButton("WorkspaceCloseButton",
                                           tr("Close"),
                                           "close_b.png",
                                           "close_h.png");
    closeButton->setProperty("workspaceShellAction", WorkspaceShellActionClose);
    QObject::connect(closeButton, &QToolButton::clicked, [this]() {
        EmitShellAction(WorkspaceShellActionClose);
    });
    headerLayout->addWidget(closeButton, 0, Qt::AlignVCenter);

    layout->addWidget(headerFrame, 0);

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
    if (!IsStepAllowed(m_currentStep)) {
        // 切换到练习模式后，旧的当前步骤可能还是 Order，必须收敛到该模式的默认步骤。
        m_currentStep = DefaultStepForMode();
    }
    SetStep(m_currentStep);
    WriteLog(LogLevel::Info, "CreateWidget", "Workspace shell widget created");
    return root;
}

// 切换当前工作步骤。
// 如果收到非法步骤，只写 Warning 日志并保持当前页面不变。
void OrderScanWorkspaceShellImpl::SetStep(int step) {
    if (step < WorkspaceStepOrderCreate || step > WorkspaceStepSend || !IsStepAllowed(step)) {
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
    RefreshStepButtons();
    WriteLog(LogLevel::Info, "SetStep", QString("Current step: %1").arg(step));

    if (m_stepChangedCallback) {
        // 步骤变化只上报稳定 int，MainExe 决定是否懒加载真实页面或释放重资源。
        m_stepChangedCallback(m_stepChangedContext, step);
    }
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
    m_stack = nullptr;
    m_stepWidgets.clear();
    m_stepButtons.clear();
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
    m_workspaceMode = WorkspaceModeOrderCreate;
    m_currentStep = WorkspaceStepOrderCreate;
    m_shellActionCallback = nullptr;
    m_shellActionContext = nullptr;
    m_stepChangedCallback = nullptr;
    m_stepChangedContext = nullptr;
}

// 设置工作台模式。
// 练习模式只保留 Scan/Process，不展示 Order/Send，避免用户以为练习会创建正式订单。
void OrderScanWorkspaceShellImpl::SetWorkspaceMode(int mode) {
    if (mode != WorkspaceModeOrderCreate && mode != WorkspaceModePractice) {
        WriteLog(LogLevel::Warning, "SetWorkspaceMode", QString("Invalid mode: %1").arg(mode));
        return;
    }

    m_workspaceMode = mode;
    if (!IsStepAllowed(m_currentStep)) {
        // 模式切换后，当前步骤若不在新模式内，立即换到新模式默认页。
        m_currentStep = DefaultStepForMode();
    }
    WriteLog(LogLevel::Info, "SetWorkspaceMode", QString("Workspace mode: %1").arg(mode));

    if (m_stack) {
        // 当前初版要求 MainExe 在 CreateWidget 前设置模式。
        // 若后续运行中切模式，调用方应释放并重建根 widget，以免旧步骤按钮仍残留。
        SetStep(m_currentStep);
    }
}

// 保存右上角壳按钮回调。
void OrderScanWorkspaceShellImpl::SetShellActionCallback(void (*callback)(void* context, int actionId),
                                                         void* context) {
    m_shellActionCallback = callback;
    m_shellActionContext = context;
    WriteLog(LogLevel::Info, "SetShellActionCallback", "Shell action callback updated");
}

// 保存步骤变化回调。
void OrderScanWorkspaceShellImpl::SetStepChangedCallback(void (*callback)(void* context, int step),
                                                         void* context) {
    m_stepChangedCallback = callback;
    m_stepChangedContext = context;
    WriteLog(LogLevel::Info, "SetStepChangedCallback", "Step changed callback updated");
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

// 返回当前模式需要显示的步骤列表。
QList<int> OrderScanWorkspaceShellImpl::VisibleSteps() const {
    if (m_workspaceMode == WorkspaceModePractice) {
        // 练习模式用默认订单信息，只练扫描和处理两个环节。
        return QList<int>() << WorkspaceStepScan << WorkspaceStepProcess;
    }

    // 正式创建模式覆盖建单、扫描、处理和发送完整流程。
    return QList<int>()
        << WorkspaceStepOrderCreate
        << WorkspaceStepScan
        << WorkspaceStepProcess
        << WorkspaceStepSend;
}

// 判断 step 是否属于当前模式。
bool OrderScanWorkspaceShellImpl::IsStepAllowed(int step) const {
    const QList<int> steps = VisibleSteps();
    return steps.contains(step);
}

// 返回当前模式默认步骤。
int OrderScanWorkspaceShellImpl::DefaultStepForMode() const {
    return m_workspaceMode == WorkspaceModePractice
        ? WorkspaceStepScan
        : WorkspaceStepOrderCreate;
}

// 创建步骤占位页面。
// 占位页让壳子模块在真实建单/扫描页面尚未接入前也能独立运行和联调。
QWidget* OrderScanWorkspaceShellImpl::CreatePlaceholder(int step, QWidget* parent) const {
    // QLabel 作为占位页足够轻，便于工作区壳子在真实模块接入前独立 smoke。
    // tr("%1 placeholder").arg(...) 先翻译模板，再填入步骤标题，便于多语言语序调整。
    auto* widget = new QLabel(tr("%1 placeholder").arg(StepTitle(step)), parent);
    widget->setObjectName(QString("WorkspaceStep%1Placeholder").arg(step));
    widget->setProperty("workspacePlaceholder", true);
    widget->setAlignment(Qt::AlignCenter);
    return widget;
}

// 刷新顶部步骤按钮状态。
// QPushButton::setChecked 只影响显示状态，不会触发 clicked 信号，因此不会造成递归切换。
void OrderScanWorkspaceShellImpl::RefreshStepButtons() {
    for (auto it = m_stepButtons.begin(); it != m_stepButtons.end(); ++it) {
        // it.key() 是步骤 ID，it.value() 是对应按钮弱引用。
        QPushButton* button = it.value();
        if (!button) {
            continue;
        }

        // 当前步骤按钮显示为选中，其它按钮取消选中。
        button->setChecked(it.key() == m_currentStep);

        // checked 属性变化后刷新 qss，保证步骤高亮立即生效。
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

// 顶部步骤按钮点击处理。
// 当前壳子只负责页面切换；具体是否允许进入某步，后续由 Workflow/Permission 给出禁用策略。
void OrderScanWorkspaceShellImpl::HandleStepButtonClicked(int step) {
    WriteLog(LogLevel::Info, "StepButtonClicked", QString("Step button clicked: %1").arg(step));
    SetStep(step);
}

// 上报右上角壳按钮动作。
// 壳子不直接操作主窗口，避免 DLL 内部依赖 QMainWindow 或 MainExe 类型。
void OrderScanWorkspaceShellImpl::EmitShellAction(int actionId) {
    WriteLog(LogLevel::Info, "ShellAction", QString("Shell action clicked: %1").arg(actionId));
    if (m_shellActionCallback) {
        m_shellActionCallback(m_shellActionContext, actionId);
    }
}

// 写结构化日志。
// 日志不可用时直接跳过，壳子界面不能因为日志 DLL 问题影响主流程。
void OrderScanWorkspaceShellImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // 公共 Qt 日志工具会自动使用 MEYER_MODULE_NAME 填充模块名。
    // 工作台壳只传操作名和内容，设备/订单/操作员字段暂由后续上下文扩展。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
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

// 返回建单扫描工作台壳公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return 1;
}
