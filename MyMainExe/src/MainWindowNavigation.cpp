#include "MainWindow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include "device/DeviceSessionHost.h"

// 数据库配置只从发布目录 config/db_config.json 读取。
QString MainWindow::ResolveDatabaseConfigPath() const {
    // 只查发布目录 config/db_config.json。
    // 不回退到源码目录，避免客户机器误用开发机路径。
    const QString deployedPath = QCoreApplication::applicationDirPath() + "/config/db_config.json";
    if (QFileInfo::exists(deployedPath)) {
        return deployedPath;
    }
    return QString();
}

// 日志目录固定在 MeyerScan.exe 同级 logs 目录。
QString MainWindow::ResolveLogDir() const {
    // 日志目录固定为 MeyerScan.exe 同级 logs，便于打包、现场收集和权限配置。
    return QCoreApplication::applicationDirPath() + "/logs";
}

// 显示首页，并释放等待页/案例页等非活动页面。
void MainWindow::ShowHome() {
    if (EnsureHomePage()) {
        // 先把首页挂到内容区，再释放其它页面，避免删除当前正在显示的 QWidget。
        ReplaceContentWidget(m_homeWidget, "Home");
        ReleaseCasePage();
        ReleaseSettingsPage();
        ReleaseOrderWorkspacePage();
        ReleaseWaitPage();
    }
}

// 显示案例管理页，并释放等待页/首页等非活动页面。
void MainWindow::ShowCase() {
    if (EnsureCasePage()) {
        // CaseUI 是从首页入口进入的全屏页面，不作为首页的兄弟页面长期并列缓存。
        ReplaceContentWidget(m_caseWidget, "Case Management");
        ReleaseHomePage();
        ReleaseSettingsPage();
        ReleaseOrderWorkspacePage();
        ReleaseWaitPage();
    }
}

// 显示设置页，并记录关闭设置后应回到的来源页面。
void MainWindow::ShowSettings(int openSource) {
    // 记录来源不是为了页面跳转本身，而是让 SettingsUI 能判断校准入口是否允许显示。
    m_settingsOpenedFromActiveWorkspace =
        openSource == SettingsOpenSourceScanReconstruct ||
        (m_orderWorkspaceWidget && m_activeWidget == m_orderWorkspaceWidget);
    m_settingsOpenSource = openSource;
    if (EnsureSettingsPage()) {
        // 每次打开设置前都传来源上下文。
        // 这样同一个 SettingsUI 单例从首页、浏览、未来扫描重建重复打开时，都能刷新校准入口状态。
        m_settings->SetOpenContext(m_settingsOpenSource,
                                   IsCalibrationAllowedForSettingsSource(m_settingsOpenSource));
        ReplaceContentWidget(m_settingsWidget, "Settings");
        // 设置页是独立主页面，打开后释放来源页面 widget，避免不可见页面占资源。
        ReleaseHomePage();
        ReleaseCasePage();
        ReleaseOrderWorkspacePage();
        ReleaseWaitPage();
    }
}

// 显示建单/扫描工作台。
// 首页手工点击“创建”和第三方自动拉起最终都走这里，避免两套建单 UI 创建流程。
void MainWindow::ShowOrderWorkspace(const QString& orderContextJson) {
    if (m_orderWorkspaceWidget && m_currentWorkspaceMode != WorkspaceModeOrderCreate) {
        // 工作台模式决定顶部步骤按钮结构。
        // 已创建的练习壳子不能直接改成创建壳子，先切到等待页再释放旧工作台，避免旧 Scan/Process 按钮残留。
        ShowWaitPage(tr("Preparing order workspace"));
        ReleaseOrderWorkspacePage();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    if (EnsureOrderWorkspacePage(orderContextJson)) {
        ReplaceContentWidget(m_orderWorkspaceWidget, "Order Workspace");
        ReleaseHomePage();
        ReleaseCasePage();
        ReleaseSettingsPage();
        ReleaseWaitPage();
    }
}

// 显示练习工作台。
// 练习工作台复用 OrderScanWorkspaceShell，但模式只允许 Scan/Process 两步。
void MainWindow::ShowPracticeWorkspace() {
    if (m_orderWorkspaceWidget && m_currentWorkspaceMode != WorkspaceModePractice) {
        // 已创建的正式建单壳子包含 Order/Send 步骤，不能直接拿来显示练习。
        // 先用等待页占位，再释放旧工作台，保证客户不会看到按钮结构突然变形。
        ShowWaitPage(tr("Preparing practice"));
        ReleaseOrderWorkspacePage();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    if (EnsurePracticeWorkspacePage()) {
        ReplaceContentWidget(m_orderWorkspaceWidget, "Practice Workspace");
        ReleaseHomePage();
        ReleaseCasePage();
        ReleaseSettingsPage();
        ReleaseWaitPage();
    }
}

// 打开扫描重建前先释放案例管理页。
// 后续 ScanReconstructStudio 接入后，这里会启动/激活扫描进程并同步订单上下文。
void MainWindow::PrepareForScanReconstruct() {
    WriteUserAction("PrepareScanReconstruct", "Release Case page before opening ScanReconstructStudio");
    // 先显示等待页，让主内容区当前页不再是 CaseUI。
    // 这样 ReleaseCasePage 的保护条件允许释放案例页。
    ShowWaitPage(tr("Preparing scan reconstruct"));
    ReleaseCasePage();
    ReleaseOrderWorkspacePage();
    // deleteLater 需要事件循环处理 DeferredDelete。
    // singleShot(0) 把后续动作排到当前事件处理结束后执行。
    QTimer::singleShot(0, this, [this]() {
        // 立即处理已投递的 DeferredDelete，尽早释放案例页资源。
        // Qt 的 deleteLater 不会马上析构对象，而是投递 DeferredDelete 事件；
        // sendPostedEvents 可以在进入扫描前主动处理这类事件，减少不可见页面继续占资源的时间。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        WriteUserAction("PrepareScanReconstructDone", "Case page release event processed");
        WriteStatus(tr("Scan reconstruct module is not implemented yet"));
    });
}

// 创建首页页面。
// 配置中心给出产品默认显隐，权限模块再做授权过滤，两者都允许才显示。
bool MainWindow::EnsureHomePage() {
    if (m_homeWidget) {
        // 页面已经存在时直接复用，避免重复创建控件。
        // 复用只发生在当前页面未被释放的短周期内；离开页面后 ReleaseHomePage 会清空指针。
        return true;
    }
    // HomeUI 运行时从 DLL 工厂函数获取，MainExe 不再链接 HomeUI import lib。
    m_home = HomeUIModule();
    if (!m_home) {
        // DLL 加载失败、导出函数缺失或依赖 DLL 缺失都可能导致这里为空。
        WriteStatus(tr("HomeUI unavailable"));
        return false;
    }

    if (!m_homeInitialized) {
        // Init 只调用一次，CreateWidget 可以在页面释放后再次调用。
        // HomeUI 内部不拥有数据库和日志，只保存路径/接口引用。
        m_homeInitialized = m_home->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_homeInitialized) {
        // Init 失败后不能继续 CreateWidget，否则模块可能在未初始化路径上访问空资源。
        WriteStatus(tr("HomeUI initialize failed"));
        WriteUserAction("ModuleInitFailed", "HomeUI Init returned false");
        return false;
    }
    // 设置回调后，HomeUI 按钮点击才能回到 MainExe 分发。
    m_home->SetEntryCallback(&MainWindow::OnHomeEntryClicked, this);
    // 入口规则集中下发，避免多个按钮权限判断散落在页面创建逻辑里。
    ApplyHomeEntryRules();

    // 页面 QWidget 的父对象设为 MainWindow，随后挂入 MainExe 唯一内容区显示。
    m_homeWidget = m_home->CreateWidget(this);
    if (!m_homeWidget) {
        // CreateWidget 失败通常说明模块内部依赖不可用，先停留在当前页并写状态。
        WriteStatus(tr("Home widget create failed"));
        return false;
    }
    WriteUserAction("PageCreate", "Home page created");
    return true;
}

// 创建案例管理页面。
// 返回首页按钮同样由配置默认值和权限结果共同决定。
bool MainWindow::EnsureCasePage() {
    if (m_caseWidget) {
        // 页面已创建时直接复用，切换回来不会重新初始化整个模块。
        return true;
    }
    // CaseUI 运行时从 DLL 工厂函数获取，MainExe 不再链接 CaseUI import lib。
    m_case = CaseUIModule();
    if (!m_case) {
        // CaseUI 是独立 DLL，返回空说明模块或依赖没有正确复制到发布目录。
        WriteStatus(tr("CaseUI unavailable"));
        return false;
    }

    if (!m_caseInitialized) {
        // CaseUI 初始化只做轻量基础设施检查，业务数据后续走服务层。
        // 即使数据库暂不可用，CaseUI 也应能创建空列表页面。
        m_caseInitialized = m_case->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_caseInitialized) {
        WriteStatus(tr("CaseUI initialize failed"));
        WriteUserAction("ModuleInitFailed", "CaseUI Init returned false");
        return false;
    }
    // CaseUI 的所有按钮动作都回调给 MainExe 分发。
    m_case->SetActionCallback(&MainWindow::OnCaseAction, this);
    // 动作规则集中下发，避免权限读取/判断/设置分散在多个点击流程里。
    ApplyCaseActionRules();

    // CaseUI 首次绘制前注入患者/订单快照，避免页面先显示空列表再闪动刷新。
    const QByteArray caseContext = BuildRuntimeDataContextJson(
        QStringList() << "local.patients" << "local.orders").toUtf8();
    if (!m_case->SetDataContextJson(caseContext.constData())) {
        WriteStatus(tr("Case data context is invalid"));
        WriteUserAction("ContextRejected", "CaseUI rejected runtime data context");
        return false;
    }

    // 创建 QWidget 后由 ShowCase 挂到 MainExe 内容区显示。
    m_caseWidget = m_case->CreateWidget(this);
    if (!m_caseWidget) {
        // 页面创建失败时不切换内容区，避免把用户带到空白页。
        WriteStatus(tr("Case widget create failed"));
        return false;
    }
    WriteUserAction("PageCreate", "Case page created");
    return true;
}

// 创建设置页面。
// SettingsUI 内部负责设置分类和校准入口；MainExe 只负责挂载和返回来源。
bool MainWindow::EnsureSettingsPage() {
    if (m_settingsWidget) {
        // 设置页已经创建时直接复用，但 ShowSettings 仍会重新 SetOpenContext，
        // 因此来源变化和校准入口状态仍能刷新。
        return true;
    }
    m_settings = SettingsUIModule();
    if (!m_settings) {
        // 设置模块缺失时只提示状态，主程序仍可继续停留在来源页面。
        WriteStatus(tr("SettingsUI unavailable"));
        return false;
    }

    if (!m_settingsInitialized) {
        // SettingsUI 初始化只加载日志；数据由 MainExe 注入，校准模块仍然懒加载。
        m_settingsInitialized = m_settings->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_settingsInitialized) {
        WriteStatus(tr("SettingsUI initialize failed"));
        WriteUserAction("ModuleInitFailed", "SettingsUI Init returned false");
        return false;
    }
    m_settings->SetActionCallback(&MainWindow::OnSettingsAction, this);
    m_settings->SetCalibrationPreflightCallback(
        &MainWindow::OnCalibrationPreflight, this);
    // CreateWidget 前先传一次来源上下文，确保校准页首次创建时状态正确。
    m_settings->SetOpenContext(m_settingsOpenSource,
                               IsCalibrationAllowedForSettingsSource(m_settingsOpenSource));

    // 信息管理页只消费宿主快照，不知道 RuntimeDataCenter 或数据库配置。
    const QByteArray settingsContext = BuildRuntimeDataContextJson(
        QStringList() << "local.doctors" << "local.clinics" << "local.labs").toUtf8();
    if (!m_settings->SetDataContextJson(settingsContext.constData())) {
        WriteStatus(tr("Settings data context is invalid"));
        WriteUserAction("ContextRejected", "SettingsUI rejected runtime data context");
        return false;
    }

    m_settingsWidget = m_settings->CreateWidget(this);
    if (!m_settingsWidget) {
        // 设置页失败不能影响首页/浏览继续使用。
        WriteStatus(tr("Settings widget create failed"));
        return false;
    }
    WriteUserAction("PageCreate", "Settings page created");
    return true;
}

// 创建建单/扫描工作台页面。
// MainExe 只做容器编排：先创建 OrderCreateUI，再挂入 OrderScanWorkspaceShell 的建单步骤。
bool MainWindow::EnsureOrderWorkspacePage(const QString& orderContextJson) {
    if (m_orderWorkspaceWidget) {
        // 页面已经存在时，如果外部传入新上下文，直接刷新建单 UI。
        // 当前 ShowOrderWorkspace 每次离开都会释放页面，所以这里主要是防御重复调用。
        m_currentWorkspaceMode = WorkspaceModeOrderCreate;
        if (!orderContextJson.isEmpty() && m_orderCreate) {
            const QByteArray contextBytes = orderContextJson.toUtf8();
            if (!m_orderCreate->SetOrderContextJson(contextBytes.constData())) {
                WriteStatus(tr("Order context is invalid"));
                WriteUserAction("ContextRejected", "OrderCreateUI rejected refreshed order context");
                return false;
            }
            m_workspaceContextJson = orderContextJson;
        }
        if (m_orderWorkspace) {
            m_orderWorkspace->SetWorkspaceMode(WorkspaceModeOrderCreate);
            m_orderWorkspace->SetStep(WorkspaceStepOrderCreate);
        }
        return true;
    }

    m_currentWorkspaceMode = WorkspaceModeOrderCreate;
    m_workspaceContextJson = orderContextJson.isEmpty()
        ? BuildDefaultWorkspaceContextJson("order")
        : orderContextJson;

    m_orderWorkspace = OrderWorkspaceModule();
    if (!m_orderWorkspace) {
        WriteStatus(tr("Order workspace unavailable"));
        return false;
    }
    m_orderCreate = OrderCreateUIModule();
    if (!m_orderCreate) {
        WriteStatus(tr("OrderCreateUI unavailable"));
        return false;
    }

    if (!m_orderWorkspaceInitialized) {
        m_orderWorkspaceInitialized = m_orderWorkspace->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_orderWorkspaceInitialized) {
        WriteStatus(tr("Order workspace initialize failed"));
        WriteUserAction("ModuleInitFailed", "OrderScanWorkspaceShell Init returned false");
        return false;
    }
    if (!m_orderCreateInitialized) {
        m_orderCreateInitialized = m_orderCreate->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_orderCreateInitialized) {
        WriteStatus(tr("OrderCreateUI initialize failed"));
        WriteUserAction("ModuleInitFailed", "OrderCreateUI Init returned false");
        return false;
    }

    // CreateWidget 前先设置模式，避免壳子创建出错误的步骤按钮集合。
    m_orderWorkspace->SetWorkspaceMode(WorkspaceModeOrderCreate);
    m_orderWorkspace->SetShellActionCallback(&MainWindow::OnWorkspaceShellAction, this);

    // 建单页面动作仍回到 MainExe 分发，不能由 OrderCreateUI 直接切换 Shell 或主窗口。
    m_orderCreate->SetActionCallback(&MainWindow::OnOrderCreateAction, this);
    if (!m_workspaceContextJson.isEmpty()) {
        // 在 QWidget 创建前先传上下文，让 OrderCreateUI 能缓存并在 CreateWidget 后立即应用。
        const QByteArray contextBytes = m_workspaceContextJson.toUtf8();
        if (!m_orderCreate->SetOrderContextJson(contextBytes.constData())) {
            WriteStatus(tr("Order context is invalid"));
            WriteUserAction("ContextRejected", "OrderCreateUI rejected initial order context");
            return false;
        }
    }

    m_orderWorkspaceWidget = m_orderWorkspace->CreateWidget(this);
    if (!m_orderWorkspaceWidget) {
        WriteStatus(tr("Order workspace widget create failed"));
        return false;
    }
    // 根 widget 创建完成后再注册步骤变化回调。
    // 这样回调里懒加载 Scan/DataProcess 时能安全使用 m_orderWorkspaceWidget 作为父对象。
    m_orderWorkspace->SetStepChangedCallback(&MainWindow::OnWorkspaceStepChanged, this);

    m_orderCreateWidget = m_orderCreate->CreateWidget(m_orderWorkspaceWidget);
    if (!m_orderCreateWidget) {
        WriteStatus(tr("Order create widget create failed"));
        return false;
    }

    // Shell 只接收 QWidget 页面并管理步骤栈，不知道建单字段和第三方来源。
    m_orderWorkspace->AttachStepWidget(WorkspaceStepOrderCreate, m_orderCreateWidget);
    m_orderWorkspace->SetStep(WorkspaceStepOrderCreate);
    WriteUserAction("PageCreate", "Order workspace page created");
    return true;
}

// 创建练习工作台页面。
// 练习只需要扫描和数据处理，订单上下文使用默认 JSON，占位后续真实练习参数。
bool MainWindow::EnsurePracticeWorkspacePage() {
    if (m_orderWorkspaceWidget) {
        m_currentWorkspaceMode = WorkspaceModePractice;
        if (m_orderWorkspace) {
            m_orderWorkspace->SetWorkspaceMode(WorkspaceModePractice);
            m_orderWorkspace->SetStep(WorkspaceStepScan);
        }
        return true;
    }

    m_currentWorkspaceMode = WorkspaceModePractice;
    m_workspaceContextJson = BuildDefaultWorkspaceContextJson("practice");

    m_orderWorkspace = OrderWorkspaceModule();
    if (!m_orderWorkspace) {
        WriteStatus(tr("Order workspace unavailable"));
        return false;
    }
    if (!m_orderWorkspaceInitialized) {
        m_orderWorkspaceInitialized = m_orderWorkspace->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_orderWorkspaceInitialized) {
        WriteStatus(tr("Practice workspace initialize failed"));
        WriteUserAction("ModuleInitFailed", "OrderScanWorkspaceShell practice Init returned false");
        return false;
    }

    // 练习模式只显示 Scan/Process，因此必须在 CreateWidget 前设置。
    m_orderWorkspace->SetWorkspaceMode(WorkspaceModePractice);
    m_orderWorkspace->SetShellActionCallback(&MainWindow::OnWorkspaceShellAction, this);

    m_orderWorkspaceWidget = m_orderWorkspace->CreateWidget(this);
    if (!m_orderWorkspaceWidget) {
        WriteStatus(tr("Practice workspace widget create failed"));
        return false;
    }
    m_orderWorkspace->SetStepChangedCallback(&MainWindow::OnWorkspaceStepChanged, this);

    // 练习模式可使用生产默认身份，但仍必须通过连接、USB3、系列和型号检测。
    // 准入成功后先把 deviceIdentity 写入上下文，再创建扫描页面。
    if (!PrepareWorkspaceDeviceSession()) {
        return false;
    }
    if (!EnsureScanWorkflowPage()) {
        return false;
    }
    m_orderWorkspace->SetStep(WorkspaceStepScan);
    WriteUserAction("PageCreate", "Practice workspace page created");
    return true;
}

// 确保扫描步骤页面已创建并挂入工作台。
// ScanWorkflowUI 通过 QLibrary 动态加载，MainExe 只依赖接口头。
bool MainWindow::EnsureScanWorkflowPage() {
    if (m_scanWorkflowWidget) {
        return true;
    }
    if (!m_orderWorkspace || !m_orderWorkspaceWidget) {
        WriteStatus(tr("Order workspace unavailable"));
        return false;
    }

    m_scanWorkflow = ScanWorkflowModule();
    if (!m_scanWorkflow) {
        WriteStatus(tr("Scan workflow unavailable"));
        return false;
    }
    if (!m_scanWorkflowInitialized) {
        m_scanWorkflowInitialized = m_scanWorkflow->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_scanWorkflowInitialized) {
        WriteStatus(tr("Scan workflow initialize failed"));
        WriteUserAction("ModuleInitFailed", "ScanWorkflowUI Init returned false");
        return false;
    }

    m_scanWorkflow->SetActionCallback(&MainWindow::OnScanWorkflowAction, this);
    const QByteArray contextBytes = m_workspaceContextJson.toUtf8();
    if (!m_scanWorkflow->SetSessionContextJson(contextBytes.constData())) {
        WriteStatus(tr("Scan workflow context is invalid"));
        WriteUserAction("ContextRejected", "ScanWorkflowUI rejected workspace context");
        return false;
    }
    m_scanWorkflowWidget = m_scanWorkflow->CreateWidget(m_orderWorkspaceWidget);
    if (!m_scanWorkflowWidget) {
        WriteStatus(tr("Scan workflow widget create failed"));
        return false;
    }
    m_orderWorkspace->AttachStepWidget(WorkspaceStepScan, m_scanWorkflowWidget);
    WriteUserAction("PageCreate", "Scan workflow page created");
    return true;
}

// 确保数据处理步骤页面已创建并挂入工作台。
bool MainWindow::EnsureDataProcessPage() {
    if (m_dataProcessWidget) {
        return true;
    }
    if (!m_orderWorkspace || !m_orderWorkspaceWidget) {
        WriteStatus(tr("Order workspace unavailable"));
        return false;
    }

    m_dataProcess = DataProcessModule();
    if (!m_dataProcess) {
        WriteStatus(tr("Data process unavailable"));
        return false;
    }
    if (!m_dataProcessInitialized) {
        m_dataProcessInitialized = m_dataProcess->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_dataProcessInitialized) {
        WriteStatus(tr("Data process initialize failed"));
        WriteUserAction("ModuleInitFailed", "DataProcessUI Init returned false");
        return false;
    }

    m_dataProcess->SetActionCallback(&MainWindow::OnDataProcessAction, this);
    const QByteArray contextBytes = m_workspaceContextJson.toUtf8();
    if (!m_dataProcess->SetSessionContextJson(contextBytes.constData())) {
        WriteStatus(tr("Data process context is invalid"));
        WriteUserAction("ContextRejected", "DataProcessUI rejected workspace context");
        return false;
    }
    m_dataProcessWidget = m_dataProcess->CreateWidget(m_orderWorkspaceWidget);
    if (!m_dataProcessWidget) {
        WriteStatus(tr("Data process widget create failed"));
        return false;
    }
    m_orderWorkspace->AttachStepWidget(WorkspaceStepProcess, m_dataProcessWidget);
    WriteUserAction("PageCreate", "Data process page created");
    return true;
}

// 确保发送步骤页面已创建并挂入工作台。
bool MainWindow::EnsureSendPage() {
    if (m_sendWidget) {
        return true;
    }
    if (!m_orderWorkspace || !m_orderWorkspaceWidget) {
        WriteStatus(tr("Order workspace unavailable"));
        return false;
    }

    m_send = SendUIModule();
    if (!m_send) {
        WriteStatus(tr("SendUI unavailable"));
        return false;
    }
    if (!m_sendInitialized) {
        m_sendInitialized = m_send->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    if (!m_sendInitialized) {
        WriteStatus(tr("SendUI initialize failed"));
        WriteUserAction("ModuleInitFailed", "SendUI Init returned false");
        return false;
    }

    m_send->SetActionCallback(&MainWindow::OnSendAction, this);
    const QByteArray contextBytes = m_workspaceContextJson.toUtf8();
    if (!m_send->SetSessionContextJson(contextBytes.constData())) {
        WriteStatus(tr("Send context is invalid"));
        WriteUserAction("ContextRejected", "SendUI rejected workspace context");
        return false;
    }
    m_sendWidget = m_send->CreateWidget(m_orderWorkspaceWidget);
    if (!m_sendWidget) {
        WriteStatus(tr("Send widget create failed"));
        return false;
    }
    m_orderWorkspace->AttachStepWidget(WorkspaceStepSend, m_sendWidget);
    WriteUserAction("PageCreate", "Send page created");
    return true;
}

// 根据设置来源返回设置关闭后应该回到的页面名称。
// 当前用于日志/后续刷新预留；页面切换仍在 HandleSettingsAction 中按枚举分发。
QString MainWindow::SettingsReturnPageName(int openSource) const {
    switch (openSource) {
    case SettingsOpenSourceCase:
        return "Case";
    case SettingsOpenSourceScanReconstruct:
        return "ScanReconstruct";
    case SettingsOpenSourceHome:
    default:
        return "Home";
    }
}

// 扫描重建过程中不能打开校准。
// 校准可能占用设备、算法资源或改变扫描状态，所以只允许首页/案例管理等非扫描流程打开。
bool MainWindow::IsCalibrationAllowedForSettingsSource(int openSource) const {
    return openSource != SettingsOpenSourceScanReconstruct;
}

// 首页不是当前页时才允许释放，避免删除正在显示的 QWidget。
void MainWindow::ReleaseHomePage() {
    ReleasePageWidget(m_homeWidget, "Home", false);
}

// 案例页不是当前页时才允许释放，扫描前会先切到等待页再释放案例页。
void MainWindow::ReleaseCasePage() {
    ReleasePageWidget(m_caseWidget, "Case", false);
}

// 释放等待页。
// 即使等待页当前正在显示，也可以先从内容区移除再 deleteLater；
// 登录阶段随后会 hide 主窗口，登录成功后再创建/显示首页。
void MainWindow::ReleaseWaitPage() {
    ReleasePageWidget(m_waitWidget, "Wait", true);
}

// 设置页不是当前页时才允许释放。
void MainWindow::ReleaseSettingsPage() {
    if (m_settings && m_settingsWidget && m_activeWidget != m_settingsWidget) {
        // SettingsUI 内部缓存了当前页面中的少量控件弱引用。
        // 删除 QWidget 前先通知模块清空这些引用，避免下次打开设置时访问悬空指针。
        m_settings->DestroyWidget();
    }
    ReleasePageWidget(m_settingsWidget, "Settings", false);
}

// 释放建单/扫描工作台页面。
// 工作台根 widget 释放后，挂在其中的 OrderCreateUI widget 会随 Qt 父子树一起释放。
void MainWindow::ReleaseOrderWorkspacePage() {
    // 先释放扫描/处理页重资源，再释放工作台壳子。
    // 这样工作台根 widget 删除时，不会带着仍绑定 OpenGL/VTK 的子控件一起悬挂到事件队列。
    ReleaseScanWorkflowPage();
    ReleaseDataProcessPage();
    ReleaseSendPage();

    // 工作台是扫描设备会话的业务所有者。无论创建还是练习，离开工作台都要
    // 关闭唯一 DeviceCmd 会话，防止设置/校准误复用过期连接和身份快照。
    if (m_deviceSessionHost) {
        m_deviceSessionHost->CloseSession();
    }
    m_workspaceDeviceSessionReady = false;

    if (m_orderCreate && m_orderCreateWidget && m_activeWidget != m_orderWorkspaceWidget) {
        // OrderCreateUI 没有单独 DestroyWidget 接口，当前通过 Shutdown 清理弱引用。
        // 下一次打开时 EnsureOrderWorkspacePage 会重新 Init 并创建新 widget。
        m_orderCreate->Shutdown();
        m_orderCreateInitialized = false;
        m_orderCreateWidget = nullptr;
    }
    if (m_orderWorkspace && m_orderWorkspaceWidget && m_activeWidget != m_orderWorkspaceWidget) {
        // Shell 内部缓存步骤 widget 弱引用，释放根 widget 前先 Shutdown 清空缓存。
        m_orderWorkspace->Shutdown();
        m_orderWorkspaceInitialized = false;
    }
    ReleasePageWidget(m_orderWorkspaceWidget, "OrderWorkspace", false);
    if (!m_orderWorkspaceWidget) {
        // 根页面逻辑释放后，建单 widget 已由父子树接管删除，成员弱引用必须同步清空。
        m_orderCreateWidget = nullptr;
        m_scanWorkflowWidget = nullptr;
        m_dataProcessWidget = nullptr;
        m_sendWidget = nullptr;
        m_workspaceContextJson.clear();
        m_currentWorkspaceMode = WorkspaceModeOrderCreate;
    }
}

// 释放扫描页面。
// 如果工作台仍存在，先用占位页替换扫描步骤，避免 Shell 内部继续持有待删除 QWidget。
void MainWindow::ReleaseScanWorkflowPage() {
    if (!m_scanWorkflow || !m_scanWorkflowWidget) {
        return;
    }
    if (m_orderWorkspace && m_orderWorkspaceWidget) {
        auto* placeholder = new QLabel(tr("Scan placeholder"), m_orderWorkspaceWidget);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("WorkspaceScanReleasedPlaceholder");
        m_orderWorkspace->AttachStepWidget(WorkspaceStepScan, placeholder);
    }
    m_scanWorkflow->Shutdown();
    m_scanWorkflowInitialized = false;
    m_scanWorkflowWidget = nullptr;
}

// 释放数据处理页面。
// 处理页同样可能持有 QVTK/OpenGL 资源，不可只 hide。
void MainWindow::ReleaseDataProcessPage() {
    if (!m_dataProcess || !m_dataProcessWidget) {
        return;
    }
    if (m_orderWorkspace && m_orderWorkspaceWidget) {
        auto* placeholder = new QLabel(tr("Process placeholder"), m_orderWorkspaceWidget);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("WorkspaceProcessReleasedPlaceholder");
        m_orderWorkspace->AttachStepWidget(WorkspaceStepProcess, placeholder);
    }
    m_dataProcess->Shutdown();
    m_dataProcessInitialized = false;
    m_dataProcessWidget = nullptr;
}

// 释放发送页面。
// 发送页当前是轻量 UI，但仍要清理 Shell 内部弱引用，避免回到其它步骤后继续显示旧页面。
void MainWindow::ReleaseSendPage() {
    if (!m_send || !m_sendWidget) {
        return;
    }
    if (m_orderWorkspace && m_orderWorkspaceWidget) {
        auto* placeholder = new QLabel(tr("Send placeholder"), m_orderWorkspaceWidget);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("WorkspaceSendReleasedPlaceholder");
        m_orderWorkspace->AttachStepWidget(WorkspaceStepSend, placeholder);
    }
    m_send->Shutdown();
    m_sendInitialized = false;
    m_sendWidget = nullptr;
}

// 释放页面 widget 的统一函数。
// 所有页面释放都走这里，便于后续增加资源统计、动画结束后删除或泄漏检查。
void MainWindow::ReleasePageWidget(QWidget*& pageWidget, const QString& pageName, bool allowActive) {
    if (!pageWidget || !m_contentLayout) {
        // 页面不存在或容器不存在时直接返回，允许重复调用。
        return;
    }

    if (!allowActive && m_activeWidget == pageWidget) {
        // 默认不释放当前活动页，避免正在绘制或正在处理按钮点击的页面被立即销毁。
        return;
    }

    if (m_activeWidget == pageWidget) {
        // allowActive=true 的等待页释放会走到这里。
        // 先清 active 指针，避免后续 ReplaceContentWidget 以为旧页面仍在显示。
        m_activeWidget = nullptr;
    }

    // 从 layout 中移除后再 deleteLater，避免 layout 继续管理一个即将释放的 widget。
    m_contentLayout->removeWidget(pageWidget);
    WriteUserAction("PageRelease", QString("%1 page released").arg(pageName));
    // deleteLater 比 delete 更适合 Qt 槽函数/事件处理中释放控件：
    // 如果当前点击信号还在调用栈里，立即 delete 可能销毁 sender 或其父对象，导致崩溃。
    pageWidget->deleteLater();
    // 注意：deleteLater 后 C++ 对象尚未立即析构，但业务指针必须马上置空。
    // 这样后续 EnsureXXXPage 会重新创建页面，不会误用一个“等待删除”的 QWidget。
    // 这里把成员指针置空，表示“逻辑上已经释放”；真实析构由事件循环稍后完成。
    pageWidget = nullptr;
}

// 统一替换主窗口内容区。
// 这里不使用堆叠容器缓存兄弟页面，是为了让首页、浏览、扫描前等待页保持父子流程关系：
// 首页进入浏览、浏览返回首页，都是“当前页面替换”，离开的模块及时释放资源。
void MainWindow::ReplaceContentWidget(QWidget* widget, const QString& pageName) {
    if (!widget || !m_contentLayout) {
        // 防御空指针，避免异常模块返回空页面导致崩溃。
        return;
    }
    if (m_activeWidget == widget) {
        // 已经在目标页时只更新状态，不重复移除/加入 layout。
        WriteStatus(pageName);
        return;
    }

    // 切换期间关闭内容区更新，避免用户看到 layout 移除/加入过程中的中间状态。
    // setUpdatesEnabled(false) 只是暂停重绘，不会阻止 layout 数据结构更新。
    m_contentRoot->setUpdatesEnabled(false);

    QWidget* oldWidget = m_activeWidget;
    if (oldWidget) {
        // 旧页面先从 layout 中拿掉，但暂不删除。
        // 调用方随后会按资源规则释放对应成员指针，这样页面变量和真实对象保持一致。
        m_contentLayout->removeWidget(oldWidget);
        // hide 避免旧页面短暂浮现在内容区外；真正释放由 ShowHome/ShowCase 等函数决定。
        oldWidget->hide();
    }

    // 新页面必须占满内容区。Qt layout 会负责多分辨率和多语言下的实际排版。
    // addWidget(widget, 1) 中的 1 是 stretch，表示它吃掉垂直方向剩余空间。
    m_contentLayout->addWidget(widget, 1);
    widget->show();
    m_activeWidget = widget;

    m_contentRoot->setUpdatesEnabled(true);
    // update 触发一次重绘，把“移除旧页面 + 加入新页面”的结果合并显示，减少闪现。
    m_contentRoot->update();
    WriteUserAction("PageSwitch", QString("Switch to %1").arg(pageName));
    WriteStatus(pageName);
}
