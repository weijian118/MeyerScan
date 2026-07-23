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
#include <QMessageBox>
#include <QRegExp>
#include <QSet>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <windows.h>
#include <cstring>
#include <vector>

#include "DatabaseQtAdapter.h"
#include "Logger.h"
#include "device/DeviceSessionHost.h"

namespace {
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";
// RuntimeDataCenter 使用调用方缓冲区返回 JSON；宿主从 512KB 开始有限扩容到 32MB。
const int kInitialRuntimeDomainBufferSize = 512 * 1024;
const int kMaxRuntimeDomainBufferSize = 32 * 1024 * 1024;

// 把 ConfigCenter 中稳定的语言代码映射到既有登录 SDK 的整数索引。
// 语言只在进程启动时读取；运行中修改配置不会重装 QTranslator，也不会刷新页面。
int LoginLanguageTypeFromCode(const QString& languageCode) {
    const QString normalized = languageCode.trimmed().toLower().replace('_', '-');
    if (normalized == "en" || normalized.startsWith("en-")) return G_LANG_ENGLISH;
    if (normalized == "es" || normalized.startsWith("es-")) return G_LANG_SPANISH;
    if (normalized == "fr" || normalized.startsWith("fr-")) return G_LANG_FRENCH;
    if (normalized == "pt" || normalized.startsWith("pt-")) return G_LANG_POTUGESE;
    if (normalized == "fa" || normalized.startsWith("fa-")) return G_LANG_PERSIAN;
    if (normalized == "ru" || normalized.startsWith("ru-")) return G_LANG_RUSSIAN;
    if (normalized == "zh-tw" || normalized == "zh-hk" ||
        normalized == "zh-hant") {
        return G_LANG_TRADITIONAL_CHINESE;
    }
    // 缺失、非法和其它暂未支持代码统一使用简体中文，保证旧配置行为稳定。
    return G_LANG_SIMPLIFIED_CHINESE;
}

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_MainExe";

// 模块版本用于运行时版本清单；版本号必须与 Version.rc 中的文件版本保持一致。
const char* Version = "MeyerScan_MainExe v0.8.0 (2026-07-23)";
}

// 从患者/订单读模型行中取稳定 ID。
// 新服务使用 camelCase，旧库快照保留大写列名；这里统一兼容两种命名，合并逻辑无需知道来源。
QString ReadModelStableId(const QString& domain, const QJsonObject& item) {
    const QStringList keys = domain == "local.orders"
        ? (QStringList() << "orderId" << "ORDER_ID")
        : (QStringList() << "patientId" << "PATIENT_ID");
    for (int keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
        // value.toVariant().toString() 同时兼容字符串和旧库可能返回的数字 ID。
        const QString value = item.value(keys.at(keyIndex)).toVariant().toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

// 合并服务读模型和旧库快照。
// 服务数据排在前面并按稳定 ID 去重；未迁移的旧库记录继续追加，保证迁移期间两类数据都可见。
QJsonArray MergeReadModelItems(const QString& domain,
                               const QJsonArray& serviceItems,
                               const QJsonArray& legacyItems) {
    QJsonArray mergedItems;
    QSet<QString> seenIds;

    // 服务查询已经按更新时间倒序，先加入可让刚保存的订单显示在案例页前部。
    for (int itemIndex = 0; itemIndex < serviceItems.size(); ++itemIndex) {
        const QJsonObject item = serviceItems.at(itemIndex).toObject();
        const QString stableId = ReadModelStableId(domain, item);
        if (!stableId.isEmpty() && seenIds.contains(stableId)) {
            // 同一服务结果中出现重复稳定 ID 时保留第一条，也就是最新的一条。
            continue;
        }
        mergedItems.append(item);
        if (!stableId.isEmpty()) {
            seenIds.insert(stableId);
        }
    }

    for (int itemIndex = 0; itemIndex < legacyItems.size(); ++itemIndex) {
        const QJsonObject item = legacyItems.at(itemIndex).toObject();
        const QString stableId = ReadModelStableId(domain, item);
        if (!stableId.isEmpty() && seenIds.contains(stableId)) {
            // 同一个 ID 已经有服务层新格式记录时，不再展示旧表重复记录。
            continue;
        }
        mergedItems.append(item);
        if (!stableId.isEmpty()) {
            seenIds.insert(stableId);
        }
    }
    return mergedItems;
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
    {"MeyerScan_DeviceTransport.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DeviceCmd.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ConfigCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Permission.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_UIComponents.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_UIResources.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_HomeUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_SettingsUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseOrderService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanSchemaService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_RuntimeDataCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderCreateUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderScanWorkspaceShell.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ExternalLaunchAdapter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Calibration3DUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CalibrationColorUI.dll", "GetMeyerModuleVersion"},
    {"ScanReconstructStudio.exe", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanReconstructStudio.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanWorkflowUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DataProcessUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_SendUI.dll", "GetMeyerModuleVersion"},
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

    // MainExe 只负责操作系统窗口能力，不绘制一条所有页面共用的可见标题栏。
    // 首页、浏览、创建/练习的顶部区域包含不同业务导航，应由各自 UI/壳模块绘制；
    // 这里仅关闭 Qt 原生边框和系统标题栏，避免出现两套窗口控制按钮。
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    // 日志必须尽早初始化，尽量覆盖从构造函数开始的启动问题。
    // 后续所有客户操作日志都复用 m_logger 这个缓存指针。
    InitLoggerEarly();

    // 设备会话宿主属于 MainExe 生命周期，但首次颜色校准前才真正加载 DeviceCmd。
    // 这保证登录和普通首页浏览不会提前枚举 USB。
    m_deviceSessionHost = new DeviceSessionHost();
    m_deviceSessionHost->Initialize(QCoreApplication::applicationDirPath(), m_logger);

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
    if (m_scanWorkflow) {
        // 扫描页持有 QVTK/OpenGL 资源，退出时必须主动释放。
        m_scanWorkflow->Shutdown();
    }
    if (m_dataProcess) {
        // 数据处理页也持有 QVTK/OpenGL 资源，退出时必须主动释放。
        m_dataProcess->Shutdown();
    }
    if (m_send) {
        // 发送页只持有轻量 UI 状态，但仍按模块生命周期统一 Shutdown。
        m_send->Shutdown();
    }
    if (m_orderWorkspace) {
        // 工作台壳只做容器和导航，关闭时不应反向释放全局基础设施。
        m_orderWorkspace->Shutdown();
    }
    if (m_externalLaunchAdapter) {
        // 第三方适配器不保存业务数据，关闭时只清路径和日志引用。
        m_externalLaunchAdapter->Shutdown();
    }
    if (m_caseOrderService) {
        // 患者订单服务借用 Database/Logger，必须在这两个进程级模块关闭前释放引用。
        m_caseOrderService->Shutdown();
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

    // 设备会话必须在 Logger 关闭前释放，关闭结果和异常仍能进入统一日志。
    if (m_deviceSessionHost) {
        m_deviceSessionHost->Shutdown();
        delete m_deviceSessionHost;
        m_deviceSessionHost = nullptr;
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
    // smoke-main 只验证模块装载和页面导航，不应依赖测试电脑是否插入真实设备。
    // 该标志没有配置文件入口，也不会在正式启动、第三方启动或登录流程中启用。
    m_skipWorkspaceDevicePreflightForSmoke = true;
    // 冒烟测试需要覆盖正常启动的大部分基础设施，但不能弹出真实登录窗口。
    ShowWaitPage(tr("Preparing modules"));
    InitInfrastructure();
    // 手动标记登录完成，使单实例激活逻辑和页面切换逻辑走主界面分支。
    m_loginCompleted = true;
    InitPages();
    ShowHome();
    ShowMainWindow();
    // 用定时器模拟用户切页面，验证页面创建、切换和释放不会阻塞事件循环。
    // 先覆盖首页打开设置，再覆盖浏览页和扫描前资源释放，防止入口回调断链后人工才发现。
    QTimer::singleShot(400, this, [this]() { ShowSettings(SettingsOpenSourceHome); });
    QTimer::singleShot(800, this, [this]() { ShowHome(); });
    QTimer::singleShot(1200, this, [this]() { ShowOrderWorkspace(); });
    QTimer::singleShot(1600, this, [this]() {
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepScan);
        }
    });
    QTimer::singleShot(2000, this, [this]() {
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepProcess);
        }
    });
    QTimer::singleShot(2400, this, [this]() {
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepSend);
        }
    });
    QTimer::singleShot(2800, this, [this]() { ShowPracticeWorkspace(); });
    QTimer::singleShot(3200, this, [this]() {
        if (m_orderWorkspace) {
            m_orderWorkspace->SetStep(WorkspaceStepProcess);
        }
    });
    QTimer::singleShot(3600, this, [this]() { ShowHome(); });
    QTimer::singleShot(4000, this, [this]() { ShowCase(); });
    // 最后模拟打开扫描重建前的资源释放要求。
    QTimer::singleShot(4400, this, [this]() { PrepareForScanReconstruct(); });
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
        ShowMainWindow();
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
        ShowMainWindow();
        return;
    }

    // 记录自动创建入口，便于日志中把外部拉起和人工点击创建区分开。
    WriteUserAction("HomeEntryAutoCreate",
                    QString("External order enters create workspace, thirdPartyType=%1").arg(thirdPartyType));
    ShowOrderWorkspace(normalizedContextJson);
    ShowMainWindow();
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
    // 语言配置只在启动阶段读取。用户修改 application.language 后必须重启，
    // MainExe 与登录 SDK 才会在下一次进程启动时加载一致的 qm。
    char languageCode[32] = {};
    if (!m_config ||
        !m_config->GetString("application.language",
                             "zh-CN",
                             languageCode,
                             sizeof(languageCode))) {
        std::strncpy(languageCode, "zh-CN", sizeof(languageCode) - 1U);
    }
    params.languageType = LoginLanguageTypeFromCode(
        QString::fromUtf8(languageCode));
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
    ShowMainWindow();
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
        // ConfigCenter 失败时仍允许使用代码默认值启动，但必须留下可检索日志。
        const bool configInitOk = m_config->Init(m_appDirUtf8.constData());
        if (!configInitOk) {
            WriteUserAction("ModuleInitFailed", "ConfigCenter Init returned false; use fallback defaults");
            // 清空接口可确保后续分支使用显式代码默认值，不调用半初始化模块。
            m_config = nullptr;
        }
    }

    // Permission 在配置默认值之上做授权过滤，例如客户版本功能阉割。
    m_permission = PermissionModule();
    if (m_permission) {
        // Permission 失败由各 Apply*Rules 的保守默认策略接管，不能静默忽略。
        const bool permissionInitOk = m_permission->Init(m_appDirUtf8.constData());
        if (!permissionInitOk) {
            WriteUserAction("ModuleInitFailed", "Permission Init returned false; use conservative rules");
            // 权限模块不可用时由 Apply*Rules 的保守默认值接管。
            m_permission = nullptr;
        }
    }

    // UIComponents 负责等待页、通用控件和后续统一缩放/样式能力。
    m_uiComponents = UIComponentsModule();
    if (m_uiComponents) {
        // UIComponents 是可降级依赖，失败时业务页面继续使用 Qt 原生控件和模块 QSS。
        const bool uiComponentsInitOk = m_uiComponents->Init(m_appDirUtf8.constData());
        if (!uiComponentsInitOk) {
            WriteUserAction("ModuleInitFailed", "UIComponents Init returned false; use local controls");
            // 页面模块会使用自身 Qt 控件和 QSS 降级，不调用半初始化共享工厂。
            m_uiComponents = nullptr;
        }
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

    // 患者/订单组合服务是正式写入口。MainExe 只负责初始化和编排，不在这里拼业务 SQL。
    m_caseOrderService = CaseOrderServiceModule();
    if (m_caseOrderService && m_databaseReady) {
        m_caseOrderServiceInitialized = m_caseOrderService->Init(m_databaseConfigPathUtf8.constData(),
                                                                 m_logDirUtf8.constData());
        if (m_caseOrderServiceInitialized) {
            // 当前骨架先检查服务所需表；后续表结构演进应迁移为独立版本化 migration。
            const CaseOrderServiceResult schemaResult = m_caseOrderService->EnsureSchema();
            m_caseOrderServiceInitialized = schemaResult.IsSuccess();
            WriteUserAction(m_caseOrderServiceInitialized ? "CaseOrderSchemaReady" : "CaseOrderSchemaFailed",
                            QString::fromUtf8(schemaResult.message));
        } else {
            WriteUserAction("ModuleInitFailed", "CaseOrderService Init returned false");
        }
    } else if (!m_databaseReady) {
        WriteUserAction("CaseOrderServiceSkipped", "Database is unavailable");
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
            if (m_uiComponents->Init(appDirBytes.constData())) {
                const QByteArray messageBytes = message.toUtf8();
                const QByteArray titleBytes = tr("MeyerScan").toUtf8();
                // CreateWaitWidget 返回 QWidget*，父对象设为 MainWindow，最终由 ReleaseWaitPage 释放。
                m_waitWidget = m_uiComponents->CreateWaitWidget(titleBytes.constData(), messageBytes.constData(), this);
            } else {
                // 共享组件初始化失败时不调用其控件工厂，等待页直接走 QLabel 降级。
                WriteUserAction("ModuleInitFailed", "UIComponents Init failed while creating wait page");
                m_uiComponents = nullptr;
            }
        }
        if (!m_waitWidget) {
            // UIComponents 不可用时使用 QLabel 降级，保证启动流程仍有可见反馈。
            m_waitWidget = new QLabel(message, this);
        }
    }

    ReplaceContentWidget(m_waitWidget, "Wait");
    ShowMainWindow();
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

// C ABI 同步回调转发：SettingsUI 在真正创建校准弹窗前等待该函数返回。
int MainWindow::OnCalibrationPreflight(
    void* context,
    int actionId,
    SettingsCalibrationDeviceContext* deviceContext) {
    auto* window = static_cast<MainWindow*>(context);
    if (!window) {
        return SettingsCalibrationPreflightInternalError;
    }
    return window->HandleCalibrationPreflight(actionId, deviceContext);
}

// C ABI 回调转发：OrderCreateUI 不直接依赖 MainWindow 类型。
void MainWindow::OnOrderCreateAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleOrderCreateAction(actionId);
    }
}

// C ABI 回调转发：工作台壳不直接依赖 MainWindow 类型。
void MainWindow::OnWorkspaceShellAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleWorkspaceShellAction(actionId);
    }
}

// C ABI 回调转发：壳子步骤变化统一交给 MainExe 做懒加载和资源释放。
void MainWindow::OnWorkspaceStepChanged(void* context, int step) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleWorkspaceStepChanged(step);
    }
}

// C ABI 回调转发：ScanWorkflowUI 不直接依赖 MainWindow 类型。
void MainWindow::OnScanWorkflowAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleScanWorkflowAction(actionId);
    }
}

// C ABI 回调转发：DataProcessUI 不直接依赖 MainWindow 类型。
void MainWindow::OnDataProcessAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleDataProcessAction(actionId);
    }
}

// C ABI 回调转发：SendUI 不直接依赖 MainWindow 类型。
void MainWindow::OnSendAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleSendAction(actionId);
    }
}
