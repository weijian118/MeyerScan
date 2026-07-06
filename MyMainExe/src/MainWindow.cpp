#include "MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QRegExp>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <windows.h>
#include <cstring>
#include <vector>

#include "DatabaseQtAdapter.h"
#include "Logger.h"

namespace {
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_MainExe";

// 模块版本用于运行时版本清单；版本号必须与 Version.rc 中的文件版本保持一致。
const char* Version = "MeyerScan_MainExe v0.1.0 (2026-06-25)";
}

struct VersionModuleEntry {
    const char* file;
    const char* versionFunction;
};

const VersionModuleEntry kDefaultVersionModules[] = {
    {"MeyerScan.exe", ""},
    {"MeyerLoginWidget.dll", ""},
    {"MeyerScan_Logger.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Database.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DatabaseQtAdapter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ConfigCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Permission.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_UIComponents.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_HomeUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_SettingsUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseOrderService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_RuntimeDataCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderCreateUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderScanWorkspaceShell.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ExternalLaunchAdapter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Calibration3DUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CalibrationColorUI.dll", "GetMeyerModuleVersion"},
    {"ScanReconstructStudio.exe", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanWorkflowUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DataProcessUI.dll", "GetMeyerModuleVersion"},
};
}

// 构造函数只做主窗口外壳和信号连接，不创建重业务页面。
// 页面使用懒加载，避免登录前就占用首页/案例管理资源。
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    // 主窗口标题也使用 tr，虽然产品名通常不翻译，
    // 但保持统一规则可以避免后续可见文本遗漏。
    setWindowTitle(tr("MeyerScan"));
    // 这里设置的是默认窗口尺寸，不是固定尺寸。
    // 具体分辨率适配交给 Qt Layout 和 UIComponents 的统一策略处理。
    resize(1180, 760);

    // 日志必须尽早初始化，尽量覆盖从构造函数开始的启动问题。
    // 后续所有客户操作日志都复用 m_logger 这个缓存指针。
    InitLoggerEarly();

    // MainExe 只提供一个全屏内容区。
    // 首页、浏览、等待页不会作为并列兄弟页面常驻；
    // 每次只把当前需要显示的模块页面挂到这个 layout 中，离开后及时释放旧页面资源。
    m_contentRoot = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentRoot);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    setCentralWidget(m_contentRoot);

    // 状态栏只做开发期和现场排查提示，不承载核心业务状态。
    m_status = new QLabel(this);
    statusBar()->addPermanentWidget(m_status, 1);

    // 登录 DLL 通过 Qt 信号返回 LoginReturnParameters。
    // 这里用 lambda 转到成员函数，避免暴露 MainWindow 内部给登录模块。
    connect(&m_loginWidget,
            &CBLMeyerLoginWidget::loginStatusReturn,
            this,
            [this](const LoginReturnParameters& result) { OnLoginStatusReturn(result); });
}

// 析构函数负责模块级 Shutdown。
// 顺序上先释放 UI/服务模块，最后关闭数据库和日志，避免后续模块写日志时 Logger 已失效。
MainWindow::~MainWindow() {
    // UI 模块先 Shutdown，让它们有机会把最后一批操作日志 Flush 出去。
    if (m_home) {
        m_home->Shutdown();
    }
    if (m_case) {
        m_case->Shutdown();
    }
    if (m_settings) {
        m_settings->Shutdown();
    }
    if (m_orderCreate) {
        // 建单 UI 只持有临时表单状态，先于工作台壳释放，避免壳子还引用已 Shutdown 的页面。
        m_orderCreate->Shutdown();
    }
    if (m_orderWorkspace) {
        // 工作台壳只做容器和导航，关闭时不应反向释放全局基础设施。
        m_orderWorkspace->Shutdown();
    }
    if (m_externalLaunchAdapter) {
        // 第三方适配器不保存业务数据，关闭时只清路径和日志引用。
        m_externalLaunchAdapter->Shutdown();
    }
    if (m_runtimeDataCenter) {
        // 运行时数据中心借用 Database/Logger，必须在 Database/Logger 关闭前释放缓存引用。
        m_runtimeDataCenter->Shutdown();
    }
    if (m_permission) {
        m_permission->Shutdown();
    }
    if (m_config) {
        m_config->Shutdown();
    }
    if (m_uiComponents) {
        m_uiComponents->Shutdown();
    }

    // Database 通过 QtAdapter 收尾，MainExe 不直接包含 Database.h。
    IDatabaseQtAdapter* databaseAdapter = DatabaseAdapterModule();
    if (databaseAdapter) {
        databaseAdapter->Disconnect();
        databaseAdapter->Shutdown();
    }

    // Logger 最后关闭。这样析构阶段其它模块如果写日志，仍然有日志对象可用。
    if (m_logger) {
        m_logger->Write(LogLevel::Info, ModuleInfo::Name, "Shutdown", "", "", "", "MainExe shutdown");
        m_logger->Flush();
        m_logger->Shutdown();
        m_logger = nullptr;
    }
}

// 正常启动流程：
// 1. 显示等待页，给用户明确反馈；
// 2. 初始化日志/配置/权限/数据库/版本清单；
// 3. 释放等待页并隐藏主窗口；
// 4. 打开既有登录模块。
void MainWindow::StartLogin() {
    // 等待页先出现，给数据库检查、配置加载、版本清单生成留一个可见反馈。
    ShowWaitPage(tr("Preparing login"));
    InitInfrastructure();
    // 登录窗口由登录 DLL 自己作为独立窗口显示。
    // 进入登录前释放并隐藏 MainExe 等待页，避免用户看到等待页残留。
    HideWaitPage();
    WriteStatus(tr("Login module starting"));
    // BuildLoginParameters 每次实时构造路径，确保安装目录变化时不使用旧路径。
    m_loginWidget.initLoginWidgetAndShow(BuildLoginParameters());
}

// 冒烟测试不弹登录窗口，只验证基础设施和页面切换。
void MainWindow::StartWithoutLoginForSmoke() {
    // 冒烟测试需要覆盖正常启动的大部分基础设施，但不能弹出真实登录窗口。
    ShowWaitPage(tr("Preparing modules"));
    InitInfrastructure();
    // 手动标记登录完成，使单实例激活逻辑和页面切换逻辑走主界面分支。
    m_loginCompleted = true;
    InitPages();
    ShowHome();
    show();
    // 用定时器模拟用户切页面，验证页面创建、切换和释放不会阻塞事件循环。
    // 先覆盖首页打开设置，再覆盖浏览页和扫描前资源释放，防止入口回调断链后人工才发现。
    QTimer::singleShot(400, this, [this]() { ShowSettings(SettingsOpenSourceHome); });
    QTimer::singleShot(800, this, [this]() { ShowHome(); });
    QTimer::singleShot(1200, this, [this]() { ShowOrderWorkspace(); });
    QTimer::singleShot(1600, this, [this]() { ShowHome(); });
    QTimer::singleShot(2000, this, [this]() { ShowCase(); });
    // 最后模拟打开扫描重建前的资源释放要求。
    QTimer::singleShot(2400, this, [this]() { PrepareForScanReconstruct(); });
}

// 第三方拉起入口。
// 该流程内部仍初始化 MainExe 基础设施，并后台准备首页“创建”入口；
// 客户视觉上不显示首页，只看到等待页之后直接进入建单/扫描工作台。
void MainWindow::StartExternalOrder(const QString& inputJsonPath, const QString& thirdPartyType) {
    ShowWaitPage(tr("Preparing external order"));
    InitInfrastructure();

    QString normalizedContextJson;
    if (!NormalizeExternalOrderContext(inputJsonPath, thirdPartyType, &normalizedContextJson)) {
        // 归一化失败时保留主窗口等待/错误状态，不跳到首页，避免第三方流程误进入手工操作。
        HideWaitPage();
        WriteStatus(tr("External order failed"));
        show();
        return;
    }

    // 第三方拉起是自动建单入口，当前框架阶段跳过登录，用于打通外部启动链路。
    // 后续若需要登录态/授权校验，可在这里加入云端账号或离线许可判断，但仍不显示首页。
    m_loginCompleted = true;
    InitPages();

    if (!PrepareHomeCreateEntryForExternalOrder()) {
        // 外部拉起本质仍是自动点击首页“创建”入口。
        // 如果创建入口被权限禁用，不能绕过首页入口规则直接进入建单。
        HideWaitPage();
        WriteStatus(tr("Create entry is disabled"));
        show();
        return;
    }

    // 记录自动创建入口，便于日志中把外部拉起和人工点击创建区分开。
    WriteUserAction("HomeEntryAutoCreate",
                    QString("External order enters create workspace, thirdPartyType=%1").arg(thirdPartyType));
    ShowOrderWorkspace(normalizedContextJson);
    show();
}

// 第三方启动时不显示首页，但仍后台准备 HomeUI 的创建入口规则。
// 这样外部启动和用户在首页点击“Create”共用 order.create 的权限/配置语义。
bool MainWindow::PrepareHomeCreateEntryForExternalOrder() {
    // EnsureHomePage 会初始化 HomeUI、设置入口回调并调用 ApplyHomeEntryRules。
    // 它创建出来的首页 widget 不会挂到内容区显示，随后立即释放，因此客户看不到首页闪现。
    if (!EnsureHomePage()) {
        WriteUserAction("ExternalOrderFailed", "Home create entry preparation failed");
        return false;
    }

    const char* featureId = HomeEntryFeatureId(HomeEntryCreate);
    if (featureId && !IsFeatureVisible(featureId, nullptr, true)) {
        // visible=false 表示当前产品包/权限结果不开放创建入口。
        // 外部第三方自动拉起也必须尊重这个结果，否则会形成“首页隐藏但外部可进”的漏洞。
        WriteUserAction("PermissionDenied", QString("External order create hidden: %1").arg(featureId));
        ReleaseHomePage();
        return false;
    }
    if (featureId && !IsFeatureEnabled(featureId, true)) {
        // 外部启动也不能越过 enabled=false。
        // enabled=false 表示功能当前不可执行，即使入口可见也只能展示禁用态。
        WriteUserAction("PermissionDenied", QString("External order create disabled: %1").arg(featureId));
        ReleaseHomePage();
        return false;
    }

    // 释放未显示的首页，避免后台创建的入口页占用资源。
    ReleaseHomePage();
    return true;
}

// 登录参数必须基于应用目录构造。
// 第三方软件拉起 MeyerScan.exe 时，进程当前工作目录可能不是安装目录。
UserLoginParameters MainWindow::BuildLoginParameters() const {
    // applicationDirPath 指向 MeyerScan.exe 所在目录。
    // 即使第三方软件从别的工作目录启动本程序，这个值也稳定可靠。
    const QString appDir = QCoreApplication::applicationDirPath();

    UserLoginParameters params;
    // 登录模块沿用旧接口，要求传入基于 1920x1080 的比例。
    // 当前框架期先给 1.0，后续统一分辨率策略稳定后再集中接入。
    params.nfaktoW = 1.0;
    params.nfaktoH = 1.0;
    // 离线 license 文件按安装目录同级查找，不依赖 currentPath。
    params.dataPath = QDir(appDir).filePath("Resources/license.lic");
    // 语言索引暂沿用登录模块既有定义；MainExe 自身可通过 qm 独立翻译。
    params.languageType = G_LANG_SIMPLIFIED_CHINESE;
    // AppPath 告诉登录模块安装目录，登录模块内部加载 qm/依赖 DLL 时会使用。
    params.AppPath = appDir;
    // 当前先使用口扫云登录地址作为登录/注册占位，后续接配置中心。
    params.loginUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.registerUrl = QString::fromUtf8(kDefaultLoginUrl);
    // version 是旧登录接口字段，当前给固定值用于打通流程。
    params.version = 100;
    return params;
}

// 登录模块返回后，只有成功状态才进入主页面。
// 失败状态保留登录模块自己的处理，MainExe 只写日志和状态。
void MainWindow::OnLoginStatusReturn(const LoginReturnParameters& result) {
    // qDebug 只辅助本地调试；正式可追踪记录仍写入 Logger。
    qDebug() << "Login status:" << result.currentStatus;

    if (m_logger) {
        // 登录状态码是跨模块排查的关键节点，成功/失败都要写日志。
        const QByteArray statusText = QString("Login returned status %1").arg(result.currentStatus).toUtf8();
        m_logger->Write(IsLoginAcceptedStatus(result.currentStatus) ? LogLevel::Info : LogLevel::Warning,
                        ModuleInfo::Name,
                        "LoginStatus",
                        "",
                        "",
                        "",
                        statusText.constData());
    }

    if (!IsLoginAcceptedStatus(result.currentStatus)) {
        // 登录失败时不强行关闭登录窗口，也不进入首页。
        // 具体错误提示仍由登录模块自己的界面处理。
        WriteStatus(tr("Login returned status %1").arg(result.currentStatus));
        return;
    }

    // 登录成功后才允许单实例激活已打开窗口，并开始加载主页面。
    m_loginCompleted = true;
    // InitInfrastructure 允许重复调用；如果登录前已初始化，这里会直接返回。
    InitInfrastructure();
    InitPages();
    ShowHome();
    show();
}

// 既有登录 DLL 当前把 LOGIN_SUCCESS 和 WRITECLOUDMSG_SUCCESS 视为可进入主界面。
bool MainWindow::IsLoginAcceptedStatus(int status) const {
    // 登录模块历史上有多个“可以进入软件”的成功码。
    // 这里集中判断，避免散落在各个流程里写魔法数字。
    return status == LOGIN_SUCCESS || status == WRITECLOUDMSG_SUCCESS;
}

// 初始化所有轻量基础设施。
// 注意：Logger 在构造期已尽早初始化，这里只补齐后续模块。
void MainWindow::InitInfrastructure() {
    if (m_infrastructureInitialized) {
        // 基础设施只初始化一次，避免重复连接数据库或重复生成启动资源。
        return;
    }

    // 所有路径都从 EXE 所在目录推导，不读取 QDir::currentPath()。
    const QString logDir = ResolveLogDir();
    const QString databaseConfigPath = ResolveDatabaseConfigPath();
    // 日志目录是固定流程，启动时必须确保存在。
    QDir().mkpath(logDir);

    // QByteArray 成员保存 UTF-8 字节，传给 C ABI/DLL 接口时生命周期足够长。
    m_appDirUtf8 = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath()).toUtf8();
    m_logDirUtf8 = QDir::fromNativeSeparators(logDir).toUtf8();
    m_databaseConfigPathUtf8 = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    // 构造函数已调用过，这里再调用一次是为了防御未来入口绕过构造期流程。
    InitLoggerEarly();

    // ConfigCenter 管产品默认配置，例如某些入口默认是否显示、数据库类型默认值等。
    m_config = ConfigCenterModule();
    if (m_config) {
        m_config->Init(m_appDirUtf8.constData());
    }

    // Permission 在配置默认值之上做授权过滤，例如客户版本功能阉割。
    m_permission = PermissionModule();
    if (m_permission) {
        m_permission->Init(m_appDirUtf8.constData());
    }

    // UIComponents 负责等待页、通用控件和后续统一缩放/样式能力。
    m_uiComponents = UIComponentsModule();
    if (m_uiComponents) {
        m_uiComponents->Init(m_appDirUtf8.constData());
    }

    // Database 仍由 MainExe 统一初始化和连接，但 Qt 侧只通过 DatabaseQtAdapter 访问。
    IDatabaseQtAdapter* databaseAdapter = DatabaseAdapterModule();
    QString databaseError;
    if (databaseAdapter && !m_databaseConfigPathUtf8.isEmpty()) {
        char databaseType[32] = {0};
        // database.type 由配置中心控制，当前支持 mysql/sqlite 两种。
        if (!m_config || !m_config->GetString("database.type", "sqlite", databaseType, sizeof(databaseType))) {
            std::strncpy(databaseType, "sqlite", sizeof(databaseType) - 1);
        }
        // EnsureConnected 内部负责 QString 路径到 UTF-8 的转换、底层 Init 和类型切换。
        m_databaseReady = databaseAdapter->EnsureConnected(QString::fromUtf8(m_databaseConfigPathUtf8),
                                                           QString::fromUtf8(databaseType),
                                                           &databaseError);
    }

    if (m_logger) {
        // 数据库检查是当前启动准备阶段最重要的结果，必须写入日志。
        const QByteArray databaseErrorBytes = databaseError.toUtf8();
        const char* databaseMessage = m_databaseConfigPathUtf8.isEmpty()
            ? "Database config not found"
            : (m_databaseReady ? "Database connected" : databaseErrorBytes.constData());
        m_logger->Write(m_databaseReady ? LogLevel::Info : LogLevel::Warning,
                        ModuleInfo::Name,
                        "DatabaseCheck",
                        "",
                        "",
                        "",
                        databaseMessage);
    }

    // RuntimeDataCenter 在数据库连接后初始化。
    // 它负责把常用本地表和云端诊所信息缓存成 JSON 快照，UI/建单模块后续只读取稳定 domain。
    m_runtimeDataCenter = RuntimeDataCenterModule();
    if (m_runtimeDataCenter) {
        const bool runtimeDataInitOk = m_runtimeDataCenter->Init(m_databaseConfigPathUtf8.constData(),
                                                                 m_logDirUtf8.constData());
        RuntimeDataCenterResult reloadResult;
        if (runtimeDataInitOk) {
            // 当前 SQLite 可能是空库，ReloadAll 失败不阻断启动，只写 Warning。
            // 等 migration/初始化模块落地后，再把缺表视作更明确的安装或迁移问题。
            reloadResult = m_runtimeDataCenter->ReloadAll();
        } else {
            std::memset(&reloadResult, 0, sizeof(reloadResult));
            reloadResult.errorCode = 5;
            std::strncpy(reloadResult.message,
                         "RuntimeDataCenter init failed",
                         sizeof(reloadResult.message) - 1);
        }
        if (m_logger) {
            m_logger->Write(reloadResult.IsSuccess() ? LogLevel::Info : LogLevel::Warning,
                            ModuleInfo::Name,
                            "RuntimeDataCenter",
                            "",
                            "",
                            "",
                            reloadResult.message);
        }
    }

    // 每次启动生成一份版本清单，便于现场复现时知道 EXE/DLL 的具体版本组合。
    WriteVersionList();

    m_infrastructureInitialized = true;
    WriteStatus(m_databaseReady ? tr("Infrastructure ready") : tr("Infrastructure ready, database unavailable"));
}

// 当前页面都采用懒加载，这个函数保留为后续统一模块加载入口。
void MainWindow::InitPages() {
    // 页面目前懒加载，这里只是统一入口。
    // 后续新增建单/扫描壳/校准等模块时，可以在这里预热轻量接口。
    InitInfrastructure();

    WriteStatus(tr("Modules loaded"));
}

// 显示等待页。等待页是固定启动流程，不再由 runtime_config.json 控制。
void MainWindow::ShowWaitPage(const QString& message) {
    if (!m_contentLayout) {
        // 构造函数尚未完成或窗口异常时，不能创建页面。
        return;
    }
    if (!m_waitWidget) {
        // 优先使用共享 UI 组件模块创建等待页，保证后续所有等待界面风格一致。
        m_uiComponents = UIComponentsModule();
        if (m_uiComponents) {
            const QByteArray appDirBytes = QCoreApplication::applicationDirPath().toUtf8();
            m_uiComponents->Init(appDirBytes.constData());
            const QByteArray messageBytes = message.toUtf8();
            const QByteArray titleBytes = tr("MeyerScan").toUtf8();
            // CreateWaitWidget 返回 QWidget*，父对象设为 MainWindow，最终由 ReleaseWaitPage 释放。
            m_waitWidget = m_uiComponents->CreateWaitWidget(titleBytes.constData(), messageBytes.constData(), this);
        } else {
            // UIComponents 不可用时使用 QLabel 降级，保证启动流程仍有可见反馈。
            m_waitWidget = new QLabel(message, this);
        }
    }
    ReplaceContentWidget(m_waitWidget, "Wait");
    show();
    // 启动准备通常在同一事件循环周期内执行。
    // 主动 processEvents 可以让等待页先绘制出来，避免用户看到空白窗口。
    QApplication::processEvents();
}

// 隐藏等待页。
// 登录窗口由既有 DLL 自己显示，MainExe 此时必须隐藏并释放等待页，
// 否则用户会看到等待页残留在登录窗口后面。
void MainWindow::HideWaitPage() {
    // 先移除等待页，再隐藏主窗口。
    // 登录 DLL 自己显示登录界面，MainExe 主窗口不应停留在后台显示等待页。
    ReleaseWaitPage();
    hide();
    WriteStatus(tr("Ready"));
}

// C ABI 回调转发：HomeUI 不直接依赖 MainWindow 类型。
void MainWindow::OnHomeEntryClicked(void* context, int entryId) {
    // context 是 SetEntryCallback 时传入的 MainWindow*。
    // static_cast 前先判空，避免异常回调导致崩溃。
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleHomeEntryClicked(entryId);
    }
}

// C ABI 回调转发：CaseUI 不直接依赖 MainWindow 类型。
void MainWindow::OnCaseAction(void* context, int actionId) {
    // CaseUI 只知道 C ABI 回调，不直接包含 MainWindow 头文件。
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleCaseAction(actionId);
    }
}

// C ABI 回调转发：SettingsUI 不直接依赖 MainWindow 类型。
void MainWindow::OnSettingsAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleSettingsAction(actionId);
    }
}

// C ABI 回调转发：OrderCreateUI 不直接依赖 MainWindow 类型。
void MainWindow::OnOrderCreateAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleOrderCreateAction(actionId);
    }
}

// 首页入口统一从这里分发，避免 HomeUI 自己切换其他模块页面。
void MainWindow::HandleHomeEntryClicked(int entryId) {
    // 每一次客户点击都先写日志，再进入具体页面流程。
    WriteUserAction("HomeEntryClicked", QString("Home entry clicked: %1").arg(entryId));

    const char* featureId = HomeEntryFeatureId(entryId);
    if (featureId && !IsFeatureEnabled(featureId, true)) {
        // enabled=false 时即使 UI 误触发回调，MainExe 也不继续执行动作。
        WriteUserAction("PermissionDenied", QString("Home entry disabled: %1").arg(featureId));
        WriteStatus(tr("Feature is disabled"));
        return;
    }

    switch (entryId) {
    case HomeEntryBrowse:
        // 浏览入口进入 CaseUI，ShowCase 内部会懒加载并释放首页资源。
        ShowCase();
        break;
    case HomeEntrySettings:
        // 首页设置入口进入 SettingsUI，设置关闭后返回首页。
        ShowSettings(SettingsOpenSourceHome);
        break;
    case HomeEntryCreate:
        // 创建入口进入统一建单/扫描工作台，工作台第一步挂载 OrderCreateUI。
        ShowOrderWorkspace();
        break;
    case HomeEntryPractice:
    default:
        WriteStatus(tr("Home entry %1 is not implemented yet").arg(entryId));
        break;
    }
}

// 案例管理动作统一从这里分发，避免 CaseUI 自己切换其他模块页面。
void MainWindow::HandleCaseAction(int actionId) {
    // CaseUI 的按钮、Tab 切换、打开订单等都走这里统一分发。
    WriteUserAction("CaseAction", QString("Case action clicked: %1").arg(actionId));

    const char* featureId = CaseActionFeatureId(actionId);
    if (featureId && !IsFeatureEnabled(featureId, true)) {
        // CaseUI 的按钮禁用只是第一层体验，MainExe 仍要做执行前复核。
        WriteUserAction("PermissionDenied", QString("Case action disabled: %1").arg(featureId));
        WriteStatus(tr("Feature is disabled"));
        return;
    }

    switch (actionId) {
    case CaseActionBackHome:
        // 返回首页时释放案例页，避免浏览模块长期占用资源。
        ShowHome();
        break;
    case CaseActionOpenOrder:
        // 打开订单是进入扫描重建前的关键入口，必须先释放案例页。
        PrepareForScanReconstruct();
        break;
    case CaseActionOpenSettings:
        // 浏览页打开设置，关闭设置后回到浏览页。
        ShowSettings(SettingsOpenSourceCase);
        break;
    case CaseActionSwitchTab:
        break;
    default:
        WriteStatus(tr("Case action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 设置模块动作统一从这里分发，避免 SettingsUI 自己切换 MainExe 页面。
void MainWindow::HandleSettingsAction(int actionId) {
    WriteUserAction("SettingsAction", QString("Settings action clicked: %1").arg(actionId));

    switch (actionId) {
    case SettingsActionConfirm:
    case SettingsActionClose:
        // 当前骨架期 Confirm/Close 都只关闭设置并返回来源页面。
        if (m_settingsOpenSource == SettingsOpenSourceCase) {
            ShowCase();
        } else {
            ShowHome();
        }
        break;
    case SettingsActionApply:
        // Apply 先只记录日志，后续接 ConfigCenter/SettingsService 保存配置。
        WriteStatus(tr("Settings applied"));
        break;
    case SettingsActionRestore:
        // Restore 先只记录日志，后续接 ConfigCenter 默认值恢复。
        WriteStatus(tr("Settings restored"));
        break;
    case SettingsActionOpen3DCalibration:
    case SettingsActionOpenColorCalibration:
        // 校准页面已经由 SettingsUI 内部嵌入，这里只记录跨模块动作。
        WriteStatus(tr("Calibration page opened"));
        break;
    default:
        WriteStatus(tr("Settings action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 建单页面动作统一由 MainExe 分发。
// OrderCreateUI 只负责表单和动作 ID，不保存数据库、不启动扫描进程。
void MainWindow::HandleOrderCreateAction(int actionId) {
    WriteUserAction("OrderCreateAction", QString("Order create action clicked: %1").arg(actionId));

    switch (actionId) {
    case OrderCreateActionCancel:
    case OrderCreateActionPrevious:
        // 当前初版把取消/上一步都回到首页。
        // 后续如果从浏览页或第三方入口进入，可在上下文中记录 returnPage 再做精确返回。
        ShowHome();
        break;
    case OrderCreateActionConfirm:
        // 保存订单应由 CaseOrderService / ScanSchemaService 完成，本轮只记录流程已到达。
        WriteStatus(tr("Order create confirmed"));
        break;
    case OrderCreateActionNext:
        // 下一步正式进入扫描前必须经过 OrderWorkflowService 决策。
        // 当前骨架只切换工作台步骤，证明 Shell 可以承载后续扫描页。
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepScan);
        }
        WriteStatus(tr("Scan step is not implemented yet"));
        break;
    case OrderCreateActionClearAllTeeth:
        WriteStatus(tr("Tooth selection cleared"));
        break;
    case OrderCreateActionToothSelectionChanged:
        WriteStatus(tr("Tooth selection changed"));
        break;
    default:
        WriteStatus(tr("Order create action %1 is not implemented yet").arg(actionId));
        break;
    }
}

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
    if (EnsureOrderWorkspacePage(orderContextJson)) {
        ReplaceContentWidget(m_orderWorkspaceWidget, "Order Workspace");
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
        m_homeInitialized = m_home->Init(m_databaseConfigPathUtf8.constData(), m_logDirUtf8.constData());
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
        m_caseInitialized = m_case->Init(m_databaseConfigPathUtf8.constData(), m_logDirUtf8.constData());
    }
    // CaseUI 的所有按钮动作都回调给 MainExe 分发。
    m_case->SetActionCallback(&MainWindow::OnCaseAction, this);
    // 动作规则集中下发，避免权限读取/判断/设置分散在多个点击流程里。
    ApplyCaseActionRules();

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
        // SettingsUI 初始化会加载 Logger 和 RuntimeDataCenter，但校准模块仍然懒加载。
        m_settingsInitialized = m_settings->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }
    m_settings->SetActionCallback(&MainWindow::OnSettingsAction, this);
    // CreateWidget 前先传一次来源上下文，确保校准页首次创建时状态正确。
    m_settings->SetOpenContext(m_settingsOpenSource,
                               IsCalibrationAllowedForSettingsSource(m_settingsOpenSource));

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
        if (!orderContextJson.isEmpty() && m_orderCreate) {
            m_orderCreate->SetOrderContextJson(orderContextJson.toUtf8().constData());
        }
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepOrderCreate);
        }
        return true;
    }

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
    if (!m_orderCreateInitialized) {
        m_orderCreateInitialized = m_orderCreate->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData());
    }

    // 建单页面动作仍回到 MainExe 分发，不能由 OrderCreateUI 直接切换 Shell 或主窗口。
    m_orderCreate->SetActionCallback(&MainWindow::OnOrderCreateAction, this);
    if (!orderContextJson.isEmpty()) {
        // 在 QWidget 创建前先传上下文，让 OrderCreateUI 能缓存并在 CreateWidget 后立即应用。
        m_orderCreate->SetOrderContextJson(orderContextJson.toUtf8().constData());
    }

    m_orderWorkspaceWidget = m_orderWorkspace->CreateWidget(this);
    if (!m_orderWorkspaceWidget) {
        WriteStatus(tr("Order workspace widget create failed"));
        return false;
    }

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
    }
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

// 调用第三方拉起适配器，把外部 JSON 文件归一化为 OrderCreateUI 标准上下文。
bool MainWindow::NormalizeExternalOrderContext(const QString& inputJsonPath,
                                               const QString& thirdPartyType,
                                               QString* outputContextJson) {
    if (outputContextJson) {
        outputContextJson->clear();
    }
    if (inputJsonPath.trimmed().isEmpty()) {
        WriteUserAction("ExternalOrderFailed", "External order json path is empty");
        return false;
    }

    m_externalLaunchAdapter = ExternalLaunchAdapterModule();
    if (!m_externalLaunchAdapter) {
        WriteUserAction("ExternalOrderFailed", "ExternalLaunchAdapter unavailable");
        WriteStatus(tr("External launch adapter unavailable"));
        return false;
    }

    if (!m_externalLaunchAdapterInitialized) {
        m_externalLaunchAdapterInitialized = m_externalLaunchAdapter->Init(m_appDirUtf8.constData(),
                                                                           m_logDirUtf8.constData());
    }

    // 先给 64KB 缓冲区。患者/订单/扫描方案字段正常远小于这个尺寸；
    // 若第三方字段扩展导致不够，适配器会通过 requiredBufferSize 告诉调用方重新分配。
    std::vector<char> outputBuffer(64 * 1024, '\0');
    ExternalLaunchResult result = {};
    const QByteArray inputPathBytes = QDir::fromNativeSeparators(inputJsonPath).toUtf8();
    const QByteArray thirdPartyTypeBytes = thirdPartyType.toUtf8();
    bool ok = m_externalLaunchAdapter->NormalizeOrderFile(inputPathBytes.constData(),
                                                          thirdPartyTypeBytes.constData(),
                                                          outputBuffer.data(),
                                                          static_cast<int>(outputBuffer.size()),
                                                          &result);
    if (!ok && result.errorCode == 5 && result.requiredBufferSize > static_cast<int>(outputBuffer.size())) {
        // 缓冲区不足是可恢复错误，按适配器提示大小重试一次。
        outputBuffer.assign(static_cast<size_t>(result.requiredBufferSize), '\0');
        ok = m_externalLaunchAdapter->NormalizeOrderFile(inputPathBytes.constData(),
                                                         thirdPartyTypeBytes.constData(),
                                                         outputBuffer.data(),
                                                         static_cast<int>(outputBuffer.size()),
                                                         &result);
    }

    if (!ok) {
        const QString message = QString::fromUtf8(result.message);
        WriteUserAction("ExternalOrderFailed", message.isEmpty() ? "Normalize external order failed" : message);
        WriteStatus(tr("External order failed"));
        return false;
    }

    if (outputContextJson) {
        *outputContextJson = QString::fromUtf8(outputBuffer.data());
    }
    WriteUserAction("ExternalOrderNormalized",
                    QString("thirdPartyType=%1, sourceSystem=%2")
                        .arg(QString::fromUtf8(result.thirdPartyType))
                        .arg(QString::fromUtf8(result.sourceSystem)));
    return true;
}

// 下发首页入口规则。
// ConfigCenter 表达产品默认策略，Permission 表达授权结果；MainExe 集中合并，HomeUI 只接收最终 UI 状态。
void MainWindow::ApplyHomeEntryRules() {
    if (!m_home) {
        // HomeUI 尚未加载时没有规则可下发。
        return;
    }

    // 设置入口：配置默认可见 && 权限可见；enabled 只由权限决定。
    m_home->SetEntryVisible(HomeEntrySettings,
                            IsFeatureVisible("home.settings", "feature.home.settingsVisible", true));
    // visible 控制是否显示，enabled 控制显示后能否点击；两者含义不同，不能互相替代。
    m_home->SetEntryEnabled(HomeEntrySettings, IsFeatureEnabled("home.settings", true));

    // 浏览入口当前没有 runtime_config 默认项，先由 Permission 控制 visible/enabled。
    m_home->SetEntryVisible(HomeEntryBrowse, IsFeatureVisible("case.browse", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryBrowse, IsFeatureEnabled("case.browse", true));

    // 建单和练习入口先接入 Permission，后续对应模块落地后即可继续复用。
    m_home->SetEntryVisible(HomeEntryCreate, IsFeatureVisible("order.create", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryCreate, IsFeatureEnabled("order.create", true));
    m_home->SetEntryVisible(HomeEntryPractice, IsFeatureVisible("scan.practice", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryPractice, IsFeatureEnabled("scan.practice", true));
}

// 下发案例管理动作规则。
// 不同动作后续可能有不同判断时机；当前先把页面创建前即可确定的按钮状态集中到这里。
void MainWindow::ApplyCaseActionRules() {
    if (!m_case) {
        // CaseUI 尚未加载时没有按钮状态可设置。
        return;
    }

    // 返回首页按钮：配置默认可见 && 权限可见；enabled 由权限控制禁用态。
    m_case->SetActionVisible(CaseActionBackHome,
                             IsFeatureVisible("case.backHome", "feature.case.backHomeVisible", true));
    // enabled=false 时按钮仍可见但不可点，适合让用户知道有此功能但当前无权限/不可用。
    m_case->SetActionEnabled(CaseActionBackHome, IsFeatureEnabled("case.backHome", true));
    // 浏览页设置入口复用 home.settings 权限，避免同一设置模块出现两套授权规则。
    m_case->SetActionVisible(CaseActionOpenSettings,
                             IsFeatureVisible("home.settings", "feature.home.settingsVisible", true));
    m_case->SetActionEnabled(CaseActionOpenSettings, IsFeatureEnabled("home.settings", true));
}

// 统一合并配置显隐和权限显隐。
// configKey 允许为空，表示该功能暂时没有产品默认项，只读取 Permission。
bool MainWindow::IsFeatureVisible(const char* featureId, const char* configKey, bool defaultVisible) const {
    // ConfigCenter 控制产品/客户默认策略，例如某客户版本默认隐藏某入口。
    const bool visibleByConfig = (m_config && configKey && configKey[0])
        ? m_config->GetBool(configKey, defaultVisible)
        : defaultVisible;
    // Permission 控制授权结果，例如设备/账号/版本不满足时隐藏入口。
    const bool visibleByPermission = m_permission
        ? m_permission->IsFeatureVisible(featureId, defaultVisible)
        : defaultVisible;
    // 两者取 AND：产品策略不开放或授权不允许，最终都不可见。
    // 这里把合并逻辑放在 MainExe，是为了让 UI 模块只接收最终状态，不关心配置/权限来源。
    return visibleByConfig && visibleByPermission;
}

// 统一读取 Permission enabled。
// enabled=false 必须真实生效：UI 设置禁用态，后续动作执行入口还要继续复核。
bool MainWindow::IsFeatureEnabled(const char* featureId, bool defaultEnabled) const {
    // enabled 只表达“可执行/可点击”，不会改变可见性。
    // 动作执行前也会再次调用它复核，避免 UI 状态异常时越权执行。
    return m_permission ? m_permission->IsFeatureEnabled(featureId, defaultEnabled) : defaultEnabled;
}

// 将首页入口 ID 映射为权限 featureId。
// 映射集中在 MainExe，后续新增入口时只需要在这里和 ApplyHomeEntryRules 中维护。
const char* MainWindow::HomeEntryFeatureId(int entryId) const {
    switch (entryId) {
    case HomeEntryCreate:   return "order.create";
    case HomeEntryBrowse:   return "case.browse";
    case HomeEntryPractice: return "scan.practice";
    case HomeEntrySettings: return "home.settings";
    default:                return nullptr;
    }
}

// 将案例管理动作 ID 映射为权限 featureId。
// 当前只对已纳入权限文件的动作做复核，未映射动作默认继续走开发期流程。
const char* MainWindow::CaseActionFeatureId(int actionId) const {
    switch (actionId) {
    case CaseActionBackHome: return "case.backHome";
    case CaseActionOpenSettings: return "home.settings";
    default:                 return nullptr;
    }
}

// 生成版本清单 JSON。
// 清单写入 logs/versionList，内容只包含 manifest 中列出的拆分模块 EXE/DLL。
void MainWindow::WriteVersionList() {
    // appDirPath 是发布目录。版本清单不再无差别扫描同级 EXE/DLL，
    // 因为发布目录中会有大量 Qt、OpenSSL、AWS、VC 运行库等第三方依赖。
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QString logDirPath = ResolveLogDir();
    const QString manifestPath = QDir(appDirPath).filePath("config/version_modules.json");
    // manifest 不存在时写默认清单；存在时尊重文件内容，便于后续新增模块无需改 MainExe 代码。
    EnsureDefaultVersionManifest(manifestPath);

    QDir versionDir(logDirPath + "/versionList");
    if (!versionDir.exists()) {
        // versionList 目录不存在时自动创建，失败会在后面写文件阶段体现。
        QDir().mkpath(versionDir.absolutePath());
    }

    QJsonArray modules;
    const QList<QPair<QString, QString>> moduleEntries = LoadVersionManifest(manifestPath);
    for (const QPair<QString, QString>& moduleEntry : moduleEntries) {
        const QString moduleFile = moduleEntry.first;
        const QString versionFunctionName = moduleEntry.second;
        // manifest 中只写文件名或相对路径，最终都以 appDir 为根解析。
        QFileInfo fileInfo(QDir(appDirPath).filePath(moduleFile));
        QJsonObject module;
        // name 便于人工快速查看，path 便于工具定位实际文件。
        module.insert("name", moduleFile);
        module.insert("versionFunction", versionFunctionName);
        module.insert("path", QDir::fromNativeSeparators(fileInfo.absoluteFilePath()));
        module.insert("exists", fileInfo.exists());
        if (fileInfo.exists()) {
            // 某些 DLL 没有 Windows 版本资源，ReadFileVersion 会返回空字符串。
            const QString fileVersion = ReadFileVersion(fileInfo.absoluteFilePath());
            QString codeVersionError;
            const QString codeVersion = moduleFile.compare("MeyerScan.exe", Qt::CaseInsensitive) == 0
                ? QString::fromLatin1(ModuleInfo::Version)
                : ReadCodeVersion(fileInfo.absoluteFilePath(), versionFunctionName, &codeVersionError);
            module.insert("fileVersion", fileVersion);
            module.insert("codeVersion", codeVersion);
            if (!versionFunctionName.isEmpty() || moduleFile.compare("MeyerScan.exe", Qt::CaseInsensitive) == 0) {
                // versionMatch 只对有代码版本来源的模块有意义。
                // 对没有统一版本函数的外部模块不写 true/false，避免误导维护者。
                module.insert("versionMatch", AreVersionsConsistent(fileVersion, codeVersion));
            }
            if (!codeVersionError.isEmpty()) {
                module.insert("codeVersionError", codeVersionError);
            }
            // JSON 没有 64 位整数的稳定跨解析器表示，这里转 double 足够记录文件大小。
            module.insert("size", static_cast<double>(fileInfo.size()));
            module.insert("lastModified", fileInfo.lastModified().toString(Qt::ISODate));
        } else {
            // 缺失项仍写入清单，方便安装包阶段发现模块漏复制。
            module.insert("fileVersion", QString());
            module.insert("codeVersion", QString());
            module.insert("versionMatch", false);
            module.insert("size", 0);
            module.insert("lastModified", QString());
        }
        modules.append(module);
    }

    QJsonObject root;
    root.insert("generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("appDir", QDir::fromNativeSeparators(appDirPath));
    root.insert("generator", QString::fromLatin1(ModuleInfo::Version));
    root.insert("schemaVersion", 2);
    root.insert("manifest", QDir::fromNativeSeparators(manifestPath));
    root.insert("modules", modules);

    // 文件名带毫秒时间戳，每次启动保留一份现场版本快照。
    // smoke 或第三方拉起测试可能在同一秒内连续启动两个 MeyerScan.exe；
    // 如果只精确到秒，后一次启动会覆盖前一次版本清单，现场追溯时会少一份启动快照。
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString versionListPath = versionDir.filePath(QString("versionList_%1.json").arg(stamp));
    QFile file(versionListPath);
    const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (opened) {
        // 使用 Indented 方便人工直接打开阅读。
        // versionList 是排查文件，不走 Compact；可读性优先于体积。
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    if (m_logger) {
        // 版本清单写入成功时记录路径，失败时记录 Warning，方便定位权限问题。
        const QByteArray pathBytes = QDir::fromNativeSeparators(versionListPath).toUtf8();
        m_logger->Write(opened ? LogLevel::Info : LogLevel::Warning,
                        ModuleInfo::Name,
                        "VersionList",
                        "",
                        "",
                        "",
                        opened ? pathBytes.constData() : "Failed to write version list");
    }
}

// 读取版本清单 manifest。
// JSON 不允许写注释，字段解释写在 config/version_modules.md 中；这里仅解析机器可读字段。
QList<QPair<QString, QString>> MainWindow::LoadVersionManifest(const QString& manifestPath) const {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // 读不到 manifest 时返回空列表；启动不中断，后续日志/版本清单会暴露缺失。
        return QList<QPair<QString, QString>>();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        // JSON 格式错误时不猜测内容，避免把第三方 DLL 全扫进去。
        return QList<QPair<QString, QString>>();
    }

    QList<QPair<QString, QString>> modules;
    const QJsonArray items = document.object().value("modules").toArray();
    for (const QJsonValue& item : items) {
        if (item.isString()) {
            // 兼容旧格式：modules 数组中直接写文件名字符串。
            // 旧格式没有 versionFunction，因此只能记录文件版本，不能读取代码版本。
            const QString name = item.toString().trimmed();
            if (!name.isEmpty()) {
                // manifest 中按顺序追加，版本清单输出顺序也保持一致，方便人工比对。
                modules.append(qMakePair(name, QString()));
            }
        } else if (item.isObject()) {
            // 当前主格式：{ "file": "...", "versionFunction": "GetMeyerModuleVersion" }。
            // 旧字段 factory 记录的是业务工厂函数，例如 GetHomeUI / GetLogger。
            // 业务工厂返回的是接口对象指针，不能按 const char* 版本函数调用。
            // 因此旧配置只作为“该自研模块有代码版本来源”的提示，实际仍尝试读取统一版本函数。
            const QJsonObject object = item.toObject();
            const QString name = object.value("file").toString().trimmed();
            QString versionFunction = object.value("versionFunction").toString().trimmed();
            if (versionFunction.isEmpty() && !object.value("factory").toString().trimmed().isEmpty()) {
                versionFunction = "GetMeyerModuleVersion";
            }
            if (!name.isEmpty()) {
                // note 等说明字段只给人看，不进入版本扫描路径。
                modules.append(qMakePair(name, versionFunction));
            }
        }
    }
    return modules;
}

// 首次运行没有 manifest 时写入默认拆分模块清单。
// 后续新增模块时维护 config/version_modules.json，而不是修改扫描代码。
void MainWindow::EnsureDefaultVersionManifest(const QString& manifestPath) const {
    if (QFileInfo::exists(manifestPath)) {
        // 文件已存在时不覆盖，避免用户或打包脚本维护的清单被默认值重写。
        return;
    }

    QDir dir(QFileInfo(manifestPath).absolutePath());
    if (!dir.exists()) {
        // mkpath 可递归创建 config 目录；已存在时也安全。
        QDir().mkpath(dir.absolutePath());
    }

    QJsonArray modules;
    for (const VersionModuleEntry& moduleEntry : kDefaultVersionModules) {
        // 默认清单只包含拆分出来的自研模块，不包含 Qt、VC runtime、OpenSSL 等第三方库。
        QJsonObject module;
        module.insert("file", QString::fromLatin1(moduleEntry.file));
        module.insert("versionFunction", QString::fromLatin1(moduleEntry.versionFunction));
        modules.append(module);
    }

    QJsonObject root;
    root.insert("description", "MeyerScan split modules recorded in startup versionList");
    root.insert("schemaVersion", 2);
    root.insert("modules", modules);

    QFile file(manifestPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

// 从 Windows 文件版本资源读取 FileVersion。
// 有些第三方 DLL 没有版本资源，这种情况返回空字符串，不中断启动。
QString MainWindow::ReadFileVersion(const QString& filePath) const {
    DWORD handle = 0;
    // Windows 版本资源 API 使用宽字符路径，先转成本机分隔符和 std::wstring。
    const std::wstring nativePath = QDir::toNativeSeparators(filePath).toStdWString();
    // 第一次调用只问资源块大小；handle 在现代 Windows 中通常未使用，但 API 需要传地址。
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &handle);
    if (size == 0) {
        // 没有版本资源或读取失败都返回空，不能影响主程序启动。
        return QString();
    }

    // 版本资源大小由 Windows API 返回，按该大小申请缓冲区。
    QByteArray data(static_cast<int>(size), 0);
    // GetFileVersionInfoW 把整个版本资源块写入 data，后续 VerQueryValueW 在这块内存里取结构。
    if (!GetFileVersionInfoW(nativePath.c_str(), handle, size, data.data())) {
        return QString();
    }

    // "\\" 表示读取 VS_FIXEDFILEINFO 根结构。
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    // VerQueryValueW 返回的 info 指针指向 data 内部，不能在 data 析构后继续保存。
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || !info) {
        return QString();
    }

    // Windows 把版本号拆在两个 DWORD 中，高低字分别组成 a.b.c.d。
    // HIWORD/LOWORD 是 Windows 提供的宏，用来从 32 位整数中拆出高 16 位和低 16 位。
    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
}

// 从模块 DLL 读取代码版本。
// versionFunctionName 为空时表示该文件没有统一版本函数，只记录文件版本即可。
QString MainWindow::ReadCodeVersion(const QString& filePath,
                                    const QString& versionFunctionName,
                                    QString* errorMessage) const {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (versionFunctionName.trimmed().isEmpty()) {
        return QString();
    }

    QLibrary library(filePath);
    // PreventUnloadHint 让版本读取阶段加载的 DLL 保持驻留。
    // 后续如果 MainExe 再通过成员 QLibrary 加载同一 DLL，可复用系统已加载模块。
    library.setLoadHints(QLibrary::PreventUnloadHint);
    if (!library.load()) {
        if (errorMessage) {
            *errorMessage = library.errorString();
        }
        return QString();
    }

    const QByteArray functionBytes = versionFunctionName.toLatin1();
    QFunctionPointer pointer = library.resolve(functionBytes.constData());
    if (!pointer) {
        if (errorMessage) {
            *errorMessage = QString("Version function not found: %1").arg(versionFunctionName);
        }
        return QString();
    }

    // 统一版本函数签名固定为 const char* (*)()。
    // 它只返回模块静态版本字符串，不创建业务接口对象，避免版本扫描和业务初始化耦合。
    typedef const char* (*VersionFunction)();
    VersionFunction readVersion = reinterpret_cast<VersionFunction>(pointer);
    const char* version = readVersion ? readVersion() : nullptr;
    return version ? QString::fromUtf8(version) : QString();
}

// 把不同来源的版本字符串归一化为可比较的数字版本。
QString MainWindow::NormalizeVersionText(const QString& versionText) const {
    // 先把 rc 文件里偶尔出现的逗号版本 "1, 3, 0, 0" 归一成点号。
    QString normalized = versionText;
    normalized.replace(",", ".");

    // 从代码版本字符串中抓取第一个数字版本，例如:
    // "MeyerScan_UIComponents v0.4.0 (2026-07-05)" -> "0.4.0"。
    QRegExp expression("(\\d+(\\.\\d+)+)");
    if (expression.indexIn(normalized) >= 0) {
        return expression.cap(1);
    }
    return normalized.trimmed();
}

// 比较 Windows 文件版本和模块代码版本是否一致。
bool MainWindow::AreVersionsConsistent(const QString& fileVersion, const QString& codeVersion) const {
    const QString file = NormalizeVersionText(fileVersion);
    const QString code = NormalizeVersionText(codeVersion);
    if (file.isEmpty() || code.isEmpty()) {
        return false;
    }
    if (file == code) {
        return true;
    }

    // rc 文件常用四段版本 0.4.0.0，代码版本常用三段语义版本 0.4.0。
    // 只允许文件版本比代码版本多一个末尾 .0，避免 0.4.1 和 0.4.0 被误判一致。
    return file == code + ".0";
}

// 统一加载 MeyerScan.exe 同目录下的自研 DLL，并解析指定 C ABI 工厂函数。
QFunctionPointer MainWindow::ResolveFactory(QLibrary& library,
                                            const char* dllName,
                                            const char* factoryName,
                                            QString* errorMessage) const {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!dllName || !dllName[0] || !factoryName || !factoryName[0]) {
        if (errorMessage) {
            *errorMessage = "DLL name or factory name is empty";
        }
        return nullptr;
    }

    // QLibrary 默认按进程当前目录和 PATH 查找；这里显式给出 EXE 同目录绝对路径，
    // 避免第三方软件从其它 current directory 拉起 MeyerScan.exe 时加载错 DLL。
    if (library.fileName().isEmpty()) {
        library.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(dllName)));
        library.setLoadHints(QLibrary::PreventUnloadHint);
    }

    if (!library.isLoaded() && !library.load()) {
        if (errorMessage) {
            *errorMessage = library.errorString();
        }
        return nullptr;
    }

    QFunctionPointer pointer = library.resolve(factoryName);
    if (!pointer && errorMessage) {
        *errorMessage = QString("Factory not found: %1 in %2").arg(factoryName).arg(dllName);
    }
    return pointer;
}

// 动态加载 Logger.dll 并返回 ILogger 单例。
ILogger* MainWindow::LoggerModule() {
    if (m_logger) {
        return m_logger;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_loggerLibrary, "MeyerScan_Logger.dll", "GetLogger", &error);
    if (!pointer) {
        WriteStatus(tr("Logger unavailable"));
        return nullptr;
    }
    typedef ILogger* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 DatabaseQtAdapter.dll。
IDatabaseQtAdapter* MainWindow::DatabaseAdapterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_databaseAdapterLibrary,
                                              "MeyerScan_DatabaseQtAdapter.dll",
                                              "GetDatabaseQtAdapter",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Database adapter unavailable"));
        return nullptr;
    }
    typedef DatabaseQtAdapter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 ConfigCenter.dll。
IConfigCenter* MainWindow::ConfigCenterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_configLibrary,
                                              "MeyerScan_ConfigCenter.dll",
                                              "GetConfigCenter",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Config center unavailable"));
        return nullptr;
    }
    typedef IConfigCenter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 Permission.dll。
IPermission* MainWindow::PermissionModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_permissionLibrary,
                                              "MeyerScan_Permission.dll",
                                              "GetPermission",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Permission unavailable"));
        return nullptr;
    }
    typedef IPermission* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 UIComponents.dll。
IUIComponents* MainWindow::UIComponentsModule() {
    if (m_uiComponents) {
        return m_uiComponents;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_uiComponentsLibrary,
                                              "MeyerScan_UIComponents.dll",
                                              "GetUIComponents",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("UI components unavailable"));
        return nullptr;
    }
    typedef IUIComponents* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 RuntimeDataCenter.dll。
IRuntimeDataCenter* MainWindow::RuntimeDataCenterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_runtimeDataCenterLibrary,
                                              "MeyerScan_RuntimeDataCenter.dll",
                                              "GetRuntimeDataCenter",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Runtime data center unavailable"));
        return nullptr;
    }
    typedef IRuntimeDataCenter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 HomeUI.dll。
IHomeUI* MainWindow::HomeUIModule() {
    if (m_home) {
        return m_home;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_homeLibrary, "MeyerScan_HomeUI.dll", "GetHomeUI", &error);
    if (!pointer) {
        WriteStatus(tr("HomeUI unavailable"));
        return nullptr;
    }
    typedef IHomeUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 CaseUI.dll。
ICaseUI* MainWindow::CaseUIModule() {
    if (m_case) {
        return m_case;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_caseLibrary, "MeyerScan_CaseUI.dll", "GetCaseUI", &error);
    if (!pointer) {
        WriteStatus(tr("CaseUI unavailable"));
        return nullptr;
    }
    typedef ICaseUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 SettingsUI.dll。
ISettingsUI* MainWindow::SettingsUIModule() {
    if (m_settings) {
        return m_settings;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_settingsLibrary,
                                              "MeyerScan_SettingsUI.dll",
                                              "GetSettingsUI",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("SettingsUI unavailable"));
        return nullptr;
    }
    typedef ISettingsUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 OrderScanWorkspaceShell.dll。
IOrderScanWorkspaceShell* MainWindow::OrderWorkspaceModule() {
    if (m_orderWorkspace) {
        return m_orderWorkspace;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_orderWorkspaceLibrary,
                                              "MeyerScan_OrderScanWorkspaceShell.dll",
                                              "GetOrderScanWorkspaceShell",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Order workspace unavailable"));
        return nullptr;
    }
    typedef IOrderScanWorkspaceShell* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 OrderCreateUI.dll。
IOrderCreateUI* MainWindow::OrderCreateUIModule() {
    if (m_orderCreate) {
        return m_orderCreate;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_orderCreateLibrary,
                                              "MeyerScan_OrderCreateUI.dll",
                                              "GetOrderCreateUI",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("OrderCreateUI unavailable"));
        return nullptr;
    }
    typedef IOrderCreateUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 ExternalLaunchAdapter.dll。
IExternalLaunchAdapter* MainWindow::ExternalLaunchAdapterModule() {
    if (m_externalLaunchAdapter) {
        return m_externalLaunchAdapter;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_externalLaunchAdapterLibrary,
                                              "MeyerScan_ExternalLaunchAdapter.dll",
                                              "GetExternalLaunchAdapter",
                                              &error);
    if (!pointer) {
        WriteStatus(tr("External launch adapter unavailable"));
        return nullptr;
    }
    typedef IExternalLaunchAdapter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 写客户操作日志。日志对象使用构造期缓存的 m_logger，避免每次重新 GetLogger()。
void MainWindow::WriteUserAction(const QString& operation, const QString& content) {
    if (!m_logger) {
        // 日志未初始化时不阻断 UI 操作。
        return;
    }
    // operation/content 先转 UTF-8，保证跨 DLL char* 接口读到稳定字节。
    // QByteArray 局部变量必须活到 m_logger->Write 调用结束，所以不能把 toUtf8().constData() 拆到临时表达式里长期保存。
    const QByteArray operationBytes = operation.toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    m_logger->Write(LogLevel::Info,
                    ModuleInfo::Name,
                    operationBytes.constData(),
                    "",
                    "",
                    "",
                    contentBytes.constData());
}

// 更新主窗口状态栏。登录窗口显示期间主窗口隐藏，但状态仍保留给后续排查。
void MainWindow::WriteStatus(const QString& text) {
    if (m_status) {
        // 状态栏更新只影响本地 UI，不写日志，避免高频状态变化刷屏。
        m_status->setText(text);
    }
}

// 早期日志初始化。
// 此函数允许重复调用：第一次创建 logs 目录并 Init，后续调用只复用已缓存指针。
void MainWindow::InitLoggerEarly() {
    if (m_loggerInitialized) {
        // 已初始化时直接返回，保证多入口重复调用安全。
        return;
    }

    // 日志目录固定在 EXE 同级 logs，并在最早阶段创建。
    const QString logDir = ResolveLogDir();
    QDir().mkpath(logDir);
    m_logDirUtf8 = QDir::fromNativeSeparators(logDir).toUtf8();
    // Logger 也走运行时动态加载，保证 MainExe 启动期不再依赖 Logger.lib。
    m_logger = LoggerModule();
    if (m_logger && m_logger->Init(m_logDirUtf8.constData(), LogLevel::Info)) {
        m_loggerInitialized = true;
        m_logger->Write(LogLevel::Info, ModuleInfo::Name, "Startup", "", "", "", "Logger initialized early");
    }
}
