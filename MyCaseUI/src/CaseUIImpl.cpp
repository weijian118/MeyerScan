#include "CaseUIImpl.h"
#include "MeyerQtModuleUtils.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_CaseUI";

// 模块版本用于 GetModuleVersion() 和版本清单，必须与 Version.rc 保持一致。
const char* Version = "MeyerScan_CaseUI v0.3.3 (2026-07-15)";
}
}

// 返回案例管理模块单例。
// 当前 DLL 暴露一个 ICaseUI 实例，避免多个页面实例重复初始化基础设施。
CaseUIImpl& CaseUIImpl::Instance() {
    static CaseUIImpl instance;
    return instance;
}

// 初始化案例管理 UI。
// 这里只保存安装目录并加载日志、共享 UI；患者/订单数据必须由宿主通过 SetDataContextJson 注入。
bool CaseUIImpl::Init(const char* appDirUtf8, const char* logDir) {
    // 安装目录由 MainExe 基于 applicationDirPath() 显式传入。
    // 保存该目录后，模块加载 DLL 和资源时不再受第三方启动工作目录影响。
    m_appDir = QDir::fromNativeSeparators(QString::fromUtf8(appDirUtf8 ? appDirUtf8 : ""));
    if (m_appDir.isEmpty()) {
        m_lastStatus = "Application directory is empty";
        return false;
    }

    // 案例管理 UI 只初始化界面基础设施，不连接数据库、不初始化进程级读模型。
    LoadLogger(logDir);
    LoadUIComponents();
    m_lastStatus = "Waiting for host data context";
    // Init 只说明界面基础设施准备完成；数据是否可用由后续注入结果单独记录。
    WriteLog(LogLevel::Info, "Init", m_lastStatus);
    return true;
}

// 保存 MainExe 传入的动作回调。
// CaseUI 不直接切换到首页或扫描进程，只上报动作 ID。
void CaseUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // 使用 C ABI 回调是为了让 MainExe 和 CaseUI 之间保持稳定 DLL 边界。
    // context 通常是 MainWindow*，CaseUI 不解释它，只在点击动作时原样传回。
    m_actionCallback = callback;
    m_actionCallbackContext = context;
}

// 设置动作入口显隐。
// 当前落地“返回首页”和“设置”，后续其它动作也应按 actionId 在这里扩展。
void CaseUIImpl::SetActionVisible(int actionId, bool visible) {
    // 目前先落地返回首页按钮，后续导入/导出/删除等也可以按 actionId 扩展到这里。
    // CaseUI 只保存显隐状态，具体权限判断在 MainExe/Permission 完成。
    if (actionId == CaseActionBackHome) {
        m_backHomeVisible = visible;
    } else if (actionId == CaseActionOpenSettings) {
        m_settingsVisible = visible;
    }
}

// 设置动作入口启用态。
// 当前落地“返回首页”和“设置”，后续其它动作也应按 actionId 在这里扩展。
void CaseUIImpl::SetActionEnabled(int actionId, bool enabled) {
    // enabled 与 visible 分离：visible=false 是隐藏入口，enabled=false 是显示但禁止点击。
    // 这样便于后续在按钮上加禁用原因提示。
    if (actionId == CaseActionBackHome) {
        m_backHomeEnabled = enabled;
    } else if (actionId == CaseActionOpenSettings) {
        m_settingsEnabled = enabled;
    }
}

// 加载日志模块并缓存 ILogger 指针。
// 日志不可用时只更新状态，不阻止案例管理页面创建。
void CaseUIImpl::LoadLogger(const char* logDir) {
    // 每个模块只缓存一份 ILogger 指针，避免每次写日志都重新查找 DLL 导出函数。
    if (m_logger) {
        return;
    }
    // 日志目录必须由 MainExe 传入，CaseUI 不使用 currentPath 猜测运行目录。
    if (!logDir || !logDir[0]) {
        m_lastStatus = "Logger log directory not configured; continuing without log output";
        return;
    }

    // PreventUnloadHint 用来降低进程退出阶段 DLL 卸载顺序风险。
    // 本模块 Shutdown 时只清空指针，不主动 unload Logger DLL。
    m_loggerLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    // 使用安装目录下的绝对路径，避免 PATH 中同名 DLL 或 currentPath 变化造成误加载。
    m_loggerLibrary.setFileName(QDir(m_appDir).filePath("MeyerScan_Logger.dll"));
    if (!m_loggerLibrary.load()) {
        m_lastStatus = "Logger unavailable; continuing without log output";
        return;
    }

    // Logger 返回 C++ 虚接口，获取单例前必须校验 ABI 版本。
    auto loggerApiVersion = reinterpret_cast<int (*)()>(
        m_loggerLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!loggerApiVersion || loggerApiVersion() != 1) {
        m_lastStatus = "Logger API version mismatch; continuing without log output";
        return;
    }

    auto getLogger = reinterpret_cast<GetLoggerFunc>(m_loggerLibrary.resolve("GetLogger"));
    if (!getLogger) {
        m_lastStatus = "GetLogger export not found";
        return;
    }

    // GetLogger 返回日志模块内部单例，CaseUI 只是借用，不拥有其生命周期。
    m_logger = getLogger();
    if (!m_logger->Init(logDir, LogLevel::Info)) {
        // 日志不可用时允许页面继续创建，避免辅助能力阻断核心 UI。
        m_logger = nullptr;
        m_lastStatus = "Logger init failed; continuing without log output";
        return;
    }
}

// 加载共享 UI 组件模块。
// CaseUI 只使用它统一按钮样式；动作 ID、日志和业务流程仍由 CaseUI/MainExe 管理。
void CaseUIImpl::LoadUIComponents() {
    if (m_uiComponents) {
        return;
    }

    // 共享 UI 是可降级能力；使用绝对路径加载，缺失时仍可使用本地 Qt 控件。
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName(QDir(m_appDir).filePath("MeyerScan_UIComponents.dll"));
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local styles");
        return;
    }

    auto uiApiVersion = reinterpret_cast<int (*)()>(
        m_uiComponentsLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!uiApiVersion || uiApiVersion() != 1) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents API version mismatch");
        return;
    }

    // resolve 返回裸函数地址；reinterpret_cast 把它转换成头文件约定的工厂函数指针。
    // 工厂函数使用 extern "C"，避免 C++ 名字改编导致不同编译器/工程配置下找不到符号。
    auto getUIComponents = reinterpret_cast<GetUIComponentsFunc>(m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!getUIComponents) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "GetUIComponents export not found");
        return;
    }

    // getUIComponents 返回 UIComponents 内部单例，CaseUI 只是借用，不负责 delete。
    m_uiComponents = getUIComponents();
    if (m_uiComponents) {
        // QByteArray 必须保存到局部变量，不能直接写 applicationDirPath().toUtf8().constData() 后跨语句使用。
        // 这里 Init 在当前语句内同步完成，所以局部 QByteArray 生命周期足够。
        const QByteArray appDirBytes = m_appDir.toUtf8();
        if (!m_uiComponents->Init(appDirBytes.constData())) {
            // 初始化失败后清空接口，后续控件工厂会自动走 CaseUI 本地降级实现。
            WriteLog(LogLevel::Warning,
                     "LoadUIComponents",
                     "UIComponents Init returned false; fallback to local styles");
            m_uiComponents = nullptr;
        }
    }
}

// 创建案例管理主页面。
// 页面只提供框架和动作入口；列表数据、搜索结果和打开订单规则后续由服务层提供。
QWidget* CaseUIImpl::CreateWidget(QWidget* parent) {
    // root 作为整页根控件，所有子控件都挂在它下面。
    // MainExe 删除 root 时，Qt 会自动删除内部 Layout、按钮、表格等对象。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanCaseUIRoot");
    root->setMinimumSize(960, 600);
    MeyerQtModule::ApplyModuleQss(root, "MyCaseUI", "case.qss", m_logger);

    // 浏览页参考旧软件的全屏顶栏结构：品牌在左，订单/患者入口在中间，窗口工具在右。
    // 业务动作仍通过 NotifyAction 上报 MainExe，不在 UI 内直接切换主页面。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // QTabWidget 只管理两个内容页，不显示 Qt 默认页签。
    // 页面级切换入口放在顶部语义区域，避免出现上下两套导航。
    auto* tabs = new QTabWidget(root);
    tabs->setObjectName("CaseContentTabs");
    tabs->addTab(CreatePatientTab(tabs), tr("Patients"));
    tabs->addTab(CreateOrderTab(tabs), tr("Orders"));
    tabs->tabBar()->hide();
    tabs->setCurrentIndex(1);

    auto* topBar = new QFrame(root);
    topBar->setObjectName("CaseTopBar");
    auto* headerLayout = new QHBoxLayout(topBar);
    headerLayout->setContentsMargins(22, 12, 20, 10);
    headerLayout->setSpacing(14);

    auto* brand = new QLabel(topBar);
    brand->setObjectName("CaseBrandLabel");
    brand->setMinimumWidth(180);
    const QPixmap brandPixmap(MeyerQtModule::ModuleResourceFile("MyCaseUI", "icon/browse/top", "logo.png"));
    if (!brandPixmap.isNull()) {
        // 图片按高度缩放并保持宽高比，避免不同分辨率下固定坐标拉伸品牌图形。
        brand->setPixmap(brandPixmap.scaledToHeight(44, Qt::SmoothTransformation));
    } else {
        brand->setText(tr("MEYER"));
    }
    headerLayout->addWidget(brand, 0);

    headerLayout->addStretch(1);

    // 订单/患者按钮使用同一个 QButtonGroup 保证单选。
    // checked 状态只表示当前内容页，不参与权限判断或数据库查询。
    auto* modeGroup = new QButtonGroup(topBar);
    modeGroup->setExclusive(true);
    auto createModeButton = [topBar](const QString& text,
                                     const QString& normalIcon,
                                     const QString& selectedIcon) {
        auto* button = new QToolButton(topBar);
        button->setObjectName("CaseModeButton");
        button->setText(text);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setMinimumSize(132, 52);

        QIcon icon;
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyCaseUI", "icon/browse/top", normalIcon),
                     QSize(28, 28), QIcon::Normal, QIcon::Off);
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyCaseUI", "icon/browse/top", selectedIcon),
                     QSize(28, 28), QIcon::Normal, QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(28, 28));
        return button;
    };

    auto* ordersModeButton = createModeButton(
        tr("Orders"), "browseOrder_b.png", "browseOrder_h.png");
    auto* patientsModeButton = createModeButton(
        tr("Patients"), "browsePatient_b.png", "browsePatient_h.png");
    modeGroup->addButton(ordersModeButton, 1);
    modeGroup->addButton(patientsModeButton, 0);
    ordersModeButton->setChecked(true);
    headerLayout->addWidget(ordersModeButton, 0, Qt::AlignVCenter);
    headerLayout->addWidget(patientsModeButton, 0, Qt::AlignVCenter);
    headerLayout->addStretch(1);

    QObject::connect(ordersModeButton, &QToolButton::clicked, [tabs]() {
        tabs->setCurrentIndex(1);
    });
    QObject::connect(patientsModeButton, &QToolButton::clicked, [tabs]() {
        tabs->setCurrentIndex(0);
    });

    // 顶部工具按钮统一登记普通/悬停图标，并只通过动作 ID 上报 MainExe。
    auto createTopButton = [topBar](const QString& tooltip,
                                    const QString& normalIcon,
                                    const QString& hoverIcon) {
        auto* button = new QToolButton(topBar);
        button->setObjectName("CaseTopToolButton");
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(38, 36);

        QIcon icon;
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyCaseUI", "icon/browse/top", normalIcon),
                     QSize(23, 23),
                     QIcon::Normal,
                     QIcon::Off);
        icon.addFile(MeyerQtModule::ModuleResourceFile("MyCaseUI", "icon/browse/top", hoverIcon),
                     QSize(23, 23),
                     QIcon::Active,
                     QIcon::Off);
        button->setIcon(icon);
        button->setIconSize(QSize(23, 23));
        return button;
    };

    auto* cloudButton = createTopButton(tr("Cloud"), "cloud_b.png", "cloud_h.png");
    cloudButton->setProperty("caseActionId", CaseActionCloud);
    QObject::connect(cloudButton, &QToolButton::clicked, [this]() {
        // 浏览模块只上报云端入口，真实同步/上传能力后续由 NetworkHelper 和流程服务实现。
        NotifyAction(CaseActionCloud, "Cloud");
    });
    headerLayout->addWidget(cloudButton, 0);

    auto* screenshotButton = createTopButton(tr("Screenshot"), "cut_b.png", "cut_h.png");
    screenshotButton->setProperty("caseActionId", CaseActionScreenshot);
    QObject::connect(screenshotButton, &QToolButton::clicked, [this]() {
        // 截图路径、命名和权限由宿主统一决定，CaseUI 不直接写文件。
        NotifyAction(CaseActionScreenshot, "Screenshot");
    });
    headerLayout->addWidget(screenshotButton, 0);

    auto* settingsButton = createTopButton(tr("Settings"), "set_b.png", "set_h.png");
    settingsButton->setProperty("caseActionId", CaseActionOpenSettings);
    settingsButton->setVisible(IsActionVisible(CaseActionOpenSettings));
    settingsButton->setEnabled(IsActionEnabled(CaseActionOpenSettings));
    QObject::connect(settingsButton, &QToolButton::clicked, [this]() {
        NotifyAction(CaseActionOpenSettings, "OpenSettings");
    });
    headerLayout->addWidget(settingsButton, 0);

    auto* backButton = createTopButton(tr("Back Home"), "home_b.png", "home_h.png");
    backButton->setProperty("caseActionId", CaseActionBackHome);
    // 返回按钮的显隐来自 MainExe 综合配置中心和权限模块后的结果。
    backButton->setVisible(IsActionVisible(CaseActionBackHome));
    // enabled=false 时保留按钮位置但禁止点击，避免未授权动作进入流程。
    backButton->setEnabled(IsActionEnabled(CaseActionBackHome));
    QObject::connect(backButton, &QToolButton::clicked, [this]() {
        // clicked 信号没有业务参数，所以用 lambda 把固定 actionId 包进去。
        // 这种写法比给每个按钮写单独槽函数更短，但仍然保持动作 ID 集中管理。
        // 按钮本身不直接访问 MainWindow，只上报动作 ID，保持模块间低耦合。
        NotifyAction(CaseActionBackHome, "BackHome");
    });
    headerLayout->addWidget(backButton);

    auto* minimizeButton = createTopButton(tr("Minimize"), "min_b.png", "min_h.png");
    minimizeButton->setProperty("caseActionId", CaseActionMinimize);
    QObject::connect(minimizeButton, &QToolButton::clicked, [this]() {
        NotifyAction(CaseActionMinimize, "Minimize");
    });
    headerLayout->addWidget(minimizeButton);

    auto* closeButton = createTopButton(tr("Close"), "close_b.png", "close_h.png");
    closeButton->setProperty("caseActionId", CaseActionClose);
    QObject::connect(closeButton, &QToolButton::clicked, [this]() {
        // 浏览页的 Close 语义是退出浏览并返回首页，不直接结束整个进程。
        NotifyAction(CaseActionClose, "Close");
    });
    headerLayout->addWidget(closeButton);
    layout->addWidget(topBar, 0);

    auto* body = new QWidget(root);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(20, 20, 20, 24);
    bodyLayout->setSpacing(12);

    // 顶部按钮和内容索引保持同步；未来代码主动切页时也不会出现按钮高亮错误。
    QObject::connect(tabs, &QTabWidget::currentChanged,
                     [this, tabs, ordersModeButton, patientsModeButton](int index) {
        ordersModeButton->setChecked(index == 1);
        patientsModeButton->setChecked(index == 0);
        // 捕获 tabs 是为了读取当前 tab 文案；tabs 是 root 子对象，连接生命周期内有效。
        // Tab 切换也按客户操作写日志，后续问题排查可以还原用户路径。
        NotifyAction(CaseActionSwitchTab, QString("SwitchTab:%1").arg(tabs->tabText(index)));
    });
    bodyLayout->addWidget(tabs, 1);

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), body);
    status->setObjectName(m_dataContextReady ? "CaseStatusLabelReady" : "CaseStatusLabelWarning");
    // 当前状态标签用于开发期确认只读快照链路是否可用。
    // 正式界面可由 UIComponents 提供统一状态提示样式。
    // 正式页面不显示开发状态文字；测试仍可通过 objectName 从对象树读取。
    status->setVisible(false);
    bodyLayout->addWidget(status);
    layout->addWidget(body, 1);

    WriteLog(LogLevel::Info, "CreateWidget", "Case widget created");
    return root;
}

// 创建患者管理 Tab。
// 当前按钮只写日志并上报动作，正式导入/导出/删除必须通过服务层完成。
QWidget* CaseUIImpl::CreatePatientTab(QWidget* parent) {
    // page 挂在 QTabWidget 下，Tab 销毁时会自动释放内部控件。
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout();
    auto* search = new QLineEdit(page);
    // 搜索框只负责收集用户输入，真正查询应由 CaseOrderService 提供稳定接口。
    search->setPlaceholderText(tr("Search patient name, phone or case id"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        tr("Import"),
        tr("Export"),
        tr("Delete"),
        tr("New Patient")
    };
    const int actionIds[] = {
        // actionIds 与 buttons 下标必须保持一致。
        // 如果新增按钮，要同时新增动作 ID，避免按钮显示和业务动作错位。
        CaseActionImportPatient,
        CaseActionExportPatient,
        CaseActionDeletePatient,
        CaseActionNewPatient,
    };
    for (int i = 0; i < buttons.size(); ++i) {
        // 循环中先复制 actionId/actionName 到局部常量，后面 lambda 按值捕获这些常量。
        // 如果直接捕获 i，用户点击时 i 已经等于 buttons.size()，会导致动作错位。
        const int actionId = actionIds[i];
        const QString actionName = buttons[i];
        const int role = actionId == CaseActionDeletePatient ? MeyerButtonRoleDanger : MeyerButtonRoleSecondary;
        const QByteArray buttonTextBytes = actionName.toUtf8();
        QPushButton* button = m_uiComponents
            ? m_uiComponents->CreateButton(role,
                                           MeyerButtonContentTextOnly,
                                           buttonTextBytes.constData(),
                                           "",
                                           page)
            : new QPushButton(actionName, page);
        // actionId/actionName 按值捕获，避免循环变量 i 在点击时已经变化。
        QObject::connect(button, &QPushButton::clicked, [this, actionId, actionName]() {
            // 导入/导出/删除等动作只上报，不在 UI 层直接操作数据库或文件。
            NotifyAction(actionId, actionName);
        });
        toolbar->addWidget(button);
    }
    QObject::connect(search, &QLineEdit::returnPressed, [this]() {
        // 这里只记录用户发起搜索。后续接入服务层时，再把搜索文本通过参数传出。
        NotifyAction(CaseActionSearchPatient, "SearchPatient");
    });
    layout->addLayout(toolbar);

    // 表格读取 MainExe 注入的只读快照。
    // 复杂搜索、分页、编辑和删除仍必须通过 CaseOrderService，避免 UI 直接拼 SQL 或写数据库。
    auto* table = new QTableWidget(0, 6, page);
    // QTableWidget 是 item-based 控件，适合骨架期快速展示少量行。
    // 后续大数据分页时应改成 model/view，避免一次创建大量 QTableWidgetItem。
    table->setHorizontalHeaderLabels({
        tr("Patient"),
        tr("Gender"),
        tr("Age"),
        tr("Case ID"),
        tr("Orders"),
        tr("Updated")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // LoadRuntimeItems 返回 JSON 数组，FillPatientTable 负责把弱类型字段映射成可见列。
    FillPatientTable(table, LoadContextItems("local.patients"));
    layout->addWidget(table, 1);
    return page;
}

// 创建订单管理 Tab。
// “Open” 动作会上报给 MainExe，后续必须走 OrderWorkflowService 再进入扫描重建。
QWidget* CaseUIImpl::CreateOrderTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    page->setObjectName("CaseOrdersPage");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // 筛选工具条只收集查询条件并上报动作，当前不在 UI 内拼 SQL。
    // 后续接 CaseOrderService 查询接口时，可以把这些控件值组装成稳定 Query DTO/JSON。
    auto* filterBar = new QFrame(page);
    filterBar->setObjectName("CaseFilterBar");
    auto* toolbar = new QHBoxLayout(filterBar);
    toolbar->setContentsMargins(12, 10, 12, 10);
    toolbar->setSpacing(10);

    auto* search = new QLineEdit(filterBar);
    search->setObjectName("CaseOrderSearchEdit");
    search->setPlaceholderText(tr("Patient name or order ID"));
    search->setMinimumWidth(220);
    toolbar->addWidget(search, 2);

    auto* typeFilter = new QComboBox(filterBar);
    typeFilter->setObjectName("CaseOrderTypeFilter");
    typeFilter->addItems(QStringList() << tr("All Types") << tr("Restoration") << tr("Orthodontics"));
    typeFilter->setMinimumWidth(130);
    toolbar->addWidget(typeFilter, 1);

    auto* timeFilter = new QComboBox(filterBar);
    timeFilter->setObjectName("CaseOrderTimeFilter");
    timeFilter->addItems(QStringList() << tr("All Time") << tr("Today") << tr("Last 7 Days") << tr("Last 30 Days"));
    timeFilter->setMinimumWidth(130);
    toolbar->addWidget(timeFilter, 1);

    auto* startDate = new QDateEdit(QDate::currentDate().addYears(-1), filterBar);
    auto* endDate = new QDateEdit(QDate::currentDate(), filterBar);
    startDate->setCalendarPopup(true);
    endDate->setCalendarPopup(true);
    startDate->setDisplayFormat("yyyy/MM/dd");
    endDate->setDisplayFormat("yyyy/MM/dd");
    startDate->setObjectName("CaseOrderStartDate");
    endDate->setObjectName("CaseOrderEndDate");
    toolbar->addWidget(startDate, 1);
    toolbar->addWidget(new QLabel(tr("to"), filterBar), 0);
    toolbar->addWidget(endDate, 1);

    auto* searchButton = new QPushButton(tr("Search"), filterBar);
    searchButton->setObjectName("CaseSearchButton");
    searchButton->setProperty("role", "primary");
    auto* resetButton = new QPushButton(tr("Reset"), filterBar);
    resetButton->setObjectName("CaseResetButton");
    toolbar->addWidget(searchButton);
    toolbar->addWidget(resetButton);
    toolbar->addStretch(1);

    auto* newPatientButton = new QPushButton(tr("New Patient"), filterBar);
    newPatientButton->setObjectName("CaseNewPatientButton");
    newPatientButton->setProperty("role", "primary");
    toolbar->addWidget(newPatientButton);

    // 视图按钮先提供明确状态和日志入口；当前卡片流是正式默认视图。
    auto* compactViewButton = new QToolButton(filterBar);
    auto* cardViewButton = new QToolButton(filterBar);
    compactViewButton->setObjectName("CaseViewModeButton");
    cardViewButton->setObjectName("CaseViewModeButton");
    compactViewButton->setCheckable(true);
    cardViewButton->setCheckable(true);
    cardViewButton->setChecked(true);
    compactViewButton->setToolTip(tr("Compact View"));
    cardViewButton->setToolTip(tr("Card View"));
    compactViewButton->setCursor(Qt::PointingHandCursor);
    cardViewButton->setCursor(Qt::PointingHandCursor);
    compactViewButton->setIcon(QIcon(MeyerQtModule::ModuleResourceFile(
        "MyCaseUI", "icon/browse/order", "layoutManage_one.png")));
    cardViewButton->setIcon(QIcon(MeyerQtModule::ModuleResourceFile(
        "MyCaseUI", "icon/browse/order", "layoutManage_two.png")));
    auto* viewModeGroup = new QButtonGroup(filterBar);
    viewModeGroup->setExclusive(true);
    viewModeGroup->addButton(compactViewButton);
    viewModeGroup->addButton(cardViewButton);
    toolbar->addWidget(compactViewButton);
    toolbar->addWidget(cardViewButton);
    layout->addWidget(filterBar, 0);

    QObject::connect(searchButton, &QPushButton::clicked, [this]() {
        NotifyAction(CaseActionSearchOrder, "SearchOrder");
    });
    QObject::connect(search, &QLineEdit::returnPressed, searchButton, &QPushButton::click);
    QObject::connect(resetButton, &QPushButton::clicked,
                     [this, search, typeFilter, timeFilter, startDate, endDate]() {
        search->clear();
        typeFilter->setCurrentIndex(0);
        timeFilter->setCurrentIndex(0);
        startDate->setDate(QDate::currentDate().addYears(-1));
        endDate->setDate(QDate::currentDate());
        NotifyAction(CaseActionSearchOrder, "ResetOrderFilters");
    });
    QObject::connect(newPatientButton, &QPushButton::clicked, [this]() {
        NotifyAction(CaseActionNewPatient, "NewPatient");
    });

    // QListWidget 的 IconMode 会根据可用宽度自动换行。
    // 1920 宽度约显示四列，较窄屏幕自动降为三列/两列，不写分辨率 if/else。
    auto* orderList = new QListWidget(page);
    orderList->setObjectName("CaseOrderCardList");
    orderList->setViewMode(QListView::IconMode);
    orderList->setFlow(QListView::LeftToRight);
    orderList->setWrapping(true);
    orderList->setResizeMode(QListView::Adjust);
    orderList->setMovement(QListView::Static);
    orderList->setSelectionMode(QAbstractItemView::SingleSelection);
    orderList->setSpacing(10);
    orderList->setGridSize(QSize(420, 306));
    orderList->setUniformItemSizes(true);
    orderList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QJsonArray items = LoadContextItems("local.orders");
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject itemObject = items.at(row).toObject();
        const QString orderId = FirstText(itemObject, QStringList() << "ORDER_ID" << "orderId");
        const QString patient = FirstText(itemObject, QStringList() << "PATIENT_NAME" << "patientName");
        const QString type = FirstText(itemObject, QStringList() << "ORDER_TYPE" << "orderType");
        const QString doctor = FirstText(itemObject, QStringList() << "PHYSICIAN_NAME" << "doctorName" << "DENTIST_ID");
        const QString status = FirstText(itemObject, QStringList() << "ORDER_STATE" << "ORDER_ISCOMPETE" << "status");
        const QString created = FirstText(itemObject, QStringList() << "ORDER_DATE" << "APPOINT_DATE" << "createdAt");

        auto* listItem = new QListWidgetItem(orderList);
        listItem->setData(Qt::UserRole, orderId);
        listItem->setSizeHint(QSize(404, 292));

        auto* card = new QFrame(orderList);
        card->setObjectName("CaseOrderCard");
        card->setCursor(Qt::PointingHandCursor);
        card->setToolTip(tr("Double click to open order"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(8);

        // 旧库 ORDER_STATE 常用 0/1/2 数字；在 UI 边界转换成可读状态，不能把裸数字显示给客户。
        const QString displayStatus = (status.isEmpty() || status == "0")
            ? tr("Not Scanned")
            : (status == "1" ? tr("Sent") : (status == "2" ? tr("Uploaded") : status));
        auto* statusLabel = new QLabel(displayStatus, card);
        statusLabel->setObjectName("CaseOrderStatusLabel");
        const QString normalizedStatus = displayStatus.toLower();
        statusLabel->setProperty(
            "statusKind",
            normalizedStatus.contains("upload") ? "uploaded" :
            ((normalizedStatus.contains("send") || normalizedStatus.contains("sent"))
                ? "sent" : "pending"));
        statusLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        cardLayout->addWidget(statusLabel, 0, Qt::AlignLeft);

        auto* divider = new QFrame(card);
        divider->setObjectName("CaseOrderCardDivider");
        divider->setFrameShape(QFrame::NoFrame);
        divider->setFixedHeight(1);
        cardLayout->addWidget(divider);

        auto* preview = new QLabel(card);
        preview->setObjectName("CaseOrderPreview");
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumHeight(150);
        const QPixmap previewPixmap(MeyerQtModule::ModuleResourceFile(
            "MyCaseUI", "icon/browse/order", "teeth.png"));
        if (!previewPixmap.isNull()) {
            preview->setPixmap(previewPixmap.scaled(
                QSize(250, 150), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            preview->setText(tr("No Preview"));
        }
        cardLayout->addWidget(preview, 1);

        auto* metadata = new QGridLayout();
        metadata->setContentsMargins(0, 0, 0, 0);
        metadata->setHorizontalSpacing(10);
        metadata->setVerticalSpacing(4);
        auto* typeLabel = new QLabel(type.isEmpty() ? tr("Restoration") : type, card);
        auto* dateLabel = new QLabel(created.isEmpty() ? tr("No Date") : created, card);
        auto* doctorLabel = new QLabel(doctor.isEmpty() ? tr("No Doctor") : doctor, card);
        auto* patientLabel = new QLabel(patient.isEmpty() ? orderId : patient, card);
        typeLabel->setObjectName("CaseOrderMetaMuted");
        dateLabel->setObjectName("CaseOrderMetaMuted");
        doctorLabel->setObjectName("CaseOrderMetaMuted");
        patientLabel->setObjectName("CaseOrderPatientName");
        patientLabel->setToolTip(patientLabel->text());
        metadata->addWidget(typeLabel, 0, 0);
        metadata->addWidget(doctorLabel, 0, 1);
        metadata->addWidget(dateLabel, 1, 0);
        metadata->addWidget(patientLabel, 1, 1);
        metadata->setColumnStretch(0, 1);
        metadata->setColumnStretch(1, 1);
        cardLayout->addLayout(metadata);

        orderList->setItemWidget(listItem, card);
    }

    if (items.isEmpty()) {
        // 空状态仍放在列表区域内，不弹窗打断用户；数据库刷新后重建页面即可恢复卡片。
        auto* emptyItem = new QListWidgetItem(orderList);
        emptyItem->setSizeHint(QSize(404, 200));
        auto* emptyLabel = new QLabel(tr("No orders"), orderList);
        emptyLabel->setObjectName("CaseOrdersEmptyState");
        emptyLabel->setAlignment(Qt::AlignCenter);
        orderList->setItemWidget(emptyItem, emptyLabel);
    }

    QObject::connect(orderList, &QListWidget::itemDoubleClicked,
                     [this](QListWidgetItem* item) {
        const QString orderId = item ? item->data(Qt::UserRole).toString() : QString();
        NotifyAction(CaseActionOpenOrder,
                     orderId.isEmpty() ? "OpenOrder" : QString("OpenOrder:%1").arg(orderId));
    });
    layout->addWidget(orderList, 1);
    return page;
}

// 返回模块版本字符串。
// 修改 Version.rc 时必须同步更新这里，保证版本清单一致。
const char* CaseUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 释放案例 UI 模块状态。
// 不关闭全局 Logger；MainExe 会统一管理基础设施生命周期。
void CaseUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CaseUI shutdown");
    if (m_logger) {
        // 只刷盘，不关闭 Logger；Logger 生命周期由 MainExe 统一控制。
        m_logger->Flush();
    }
    // 这些指针都不是 CaseUI 创建的对象，Shutdown 只清引用，避免误删进程级单例。
    m_dataContextReady = false;
    m_dataContext = QJsonObject();
    m_appDir.clear();
    m_logger = nullptr;
    m_uiComponents = nullptr;
    // QLibrary uses PreventUnloadHint to avoid process-exit unload-order issues.
}

// 保存宿主注入的版本化只读数据上下文。
// 先完整解析和校验临时对象，全部通过后再替换成员，避免无效 JSON 破坏上一份可用快照。
bool CaseUIImpl::SetDataContextJson(const char* contextJsonUtf8) {
    if (!contextJsonUtf8 || !contextJsonUtf8[0]) {
        WriteLog(LogLevel::Warning, "SetDataContextJson", "Data context is empty");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(contextJsonUtf8), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteLog(LogLevel::Warning,
                 "SetDataContextJson",
                 QString("Invalid data context JSON: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject candidate = document.object();
    if (!candidate.value("domains").isObject()) {
        WriteLog(LogLevel::Warning, "SetDataContextJson", "Data context domains must be an object");
        return false;
    }

    m_dataContext = candidate;
    m_dataContextReady = true;
    m_lastStatus = "Host data context ready";
    WriteLog(LogLevel::Info, "SetDataContextJson", "Host data context accepted");
    return true;
}

// 写结构化日志。
// UI 内部使用 QString 组织内容，写入 Logger 前统一转 UTF-8。
void CaseUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    // 公共 Qt 日志工具会自动使用 MEYER_MODULE_NAME 填充模块名。
    // CaseUI 只传操作名和内容，保持 UI 模块内部写日志足够轻。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 统一处理客户操作。
// 先写日志，再上报给 MainExe，保证即使后续流程失败也能看到用户点击。
void CaseUIImpl::NotifyAction(int actionId, const QString& actionName) {
    WriteLog(LogLevel::Info, "UserAction", QString("%1 (%2)").arg(actionName).arg(actionId));
    if (m_actionCallback) {
        // CaseUI 不直接依赖 MainExe 类，所有页面跳转和业务流程由 MainExe 分发。
        m_actionCallback(m_actionCallbackContext, actionId);
    }
}

// 从宿主注入的数据上下文读取指定 domain 的 items 数组。
QJsonArray CaseUIImpl::LoadContextItems(const char* domain) const {
    // UI 只认稳定 domain，不接收 SQL、表名或数据库配置。
    if (!m_dataContextReady || !domain || !domain[0]) {
        return QJsonArray();
    }
    const QJsonObject domains = m_dataContext.value("domains").toObject();
    return domains.value(QString::fromUtf8(domain)).toObject().value("items").toArray();
}

// 用患者快照填充患者表。
void CaseUIImpl::FillPatientTable(QTableWidget* table, const QJsonArray& items) {
    if (!table) {
        return;
    }

    table->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        // 每个 item 是旧库一行的 JSON 对象。字段名保留旧库大小写，减少迁移期映射损耗。
        const QJsonObject item = items.at(row).toObject();
        const QString patientName = FirstText(item, QStringList() << "PATIENT_NAME" << "patientName");
        const QString gender = FirstText(item, QStringList() << "PATIENT_GENDER" << "PATIENT_SEX" << "gender");
        const QString age = FirstText(item, QStringList() << "PATIENT_AGE" << "age");
        const QString patientId = FirstText(item, QStringList() << "PATIENT_ID" << "patientId");
        const QString orderCount = FirstText(item, QStringList() << "PATIENT_ORDERCOUNTS" << "orderCount");
        const QString updated = FirstText(item, QStringList() << "PATIENT_UPDATETIME" << "PATIENT_CREATETIME" << "register_date");

        // QTableWidget::setItem 会接管 new 出来的 QTableWidgetItem，后续表格销毁时自动释放。
        table->setItem(row, 0, new QTableWidgetItem(patientName));
        table->setItem(row, 1, new QTableWidgetItem(gender));
        table->setItem(row, 2, new QTableWidgetItem(age));
        table->setItem(row, 3, new QTableWidgetItem(patientId));
        table->setItem(row, 4, new QTableWidgetItem(orderCount));
        table->setItem(row, 5, new QTableWidgetItem(updated));
    }
}

// 用订单快照填充订单表。
void CaseUIImpl::FillOrderTable(QTableWidget* table, const QJsonArray& items) {
    if (!table) {
        return;
    }

    table->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        // 订单行同样允许新旧字段并存，FirstText 会按候选 key 顺序取第一个可显示值。
        const QJsonObject item = items.at(row).toObject();
        const QString orderId = FirstText(item, QStringList() << "ORDER_ID" << "orderId");
        const QString patient = FirstText(item, QStringList() << "PATIENT_NAME" << "patientName");
        const QString type = FirstText(item, QStringList() << "ORDER_TYPE" << "orderType");
        const QString doctor = FirstText(item, QStringList() << "PHYSICIAN_NAME" << "doctorName" << "DENTIST_ID");
        const QString status = FirstText(item, QStringList() << "ORDER_STATE" << "ORDER_ISCOMPETE" << "status");
        const QString created = FirstText(item, QStringList() << "ORDER_DATE" << "APPOINT_DATE" << "createdAt");
        const QString data = FirstText(item, QStringList() << "SAVE_PATH" << "savePath");

        table->setItem(row, 0, new QTableWidgetItem(orderId));
        table->setItem(row, 1, new QTableWidgetItem(patient));
        table->setItem(row, 2, new QTableWidgetItem(type));
        table->setItem(row, 3, new QTableWidgetItem(doctor));
        table->setItem(row, 4, new QTableWidgetItem(status));
        table->setItem(row, 5, new QTableWidgetItem(created));
        table->setItem(row, 6, new QTableWidgetItem(data));
    }
}

// 从 JSON 对象读取第一个非空字段。
QString CaseUIImpl::FirstText(const QJsonObject& object, const QStringList& keys) const {
    // 这个函数是“字段兼容适配器”：调用方传入多个可能的字段名，
    // UI 就不需要在每个单元格都写一堆 if/else。
    for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isUndefined() || value.isNull()) {
            // 字段不存在或为 null 时继续找下一个候选 key。
            continue;
        }
        if (value.isString()) {
            // trim 后再判断空字符串，避免数据库里只有空格也被当成有效显示内容。
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        } else if (value.isDouble()) {
            // Qt JSON 把整数和浮点都放在 double 类型里；旧库 ID/年龄等按整数显示。
            return QString::number(value.toDouble(), 'f', 0);
        } else if (value.isBool()) {
            // 表格里用 1/0 表示 bool，保持和旧数据库常见字段风格一致。
            return value.toBool() ? "1" : "0";
        }
    }
    return QString();
}

// 判断动作入口是否显示。
// 当前未配置的动作默认显示，避免框架期误隐藏入口。
bool CaseUIImpl::IsActionVisible(int actionId) const {
    if (actionId == CaseActionBackHome) {
        // 返回首页按钮是当前已接入权限控制的第一个动作。
        return m_backHomeVisible;
    }
    if (actionId == CaseActionOpenSettings) {
        // 设置入口复用 home.settings 权限，由 MainExe 下发最终状态。
        return m_settingsVisible;
    }
    // 未接入权限的动作默认可见，方便开发阶段发现入口并逐步接入权限规则。
    return true;
}

// 判断动作入口是否可点击。
// 当前未配置的动作默认可用，避免框架期误禁用入口。
bool CaseUIImpl::IsActionEnabled(int actionId) const {
    if (actionId == CaseActionBackHome) {
        // 返回首页按钮是当前已接入 enabled 控制的第一个动作。
        return m_backHomeEnabled;
    }
    if (actionId == CaseActionOpenSettings) {
        // 设置入口复用 home.settings 权限，由 MainExe 下发最终状态。
        return m_settingsEnabled;
    }
    // 未接入权限的动作默认可用，后续按 actionId 逐步扩展。
    return true;
}

// C ABI 导出函数。
// MainExe 和测试宿主通过该函数获取案例管理模块接口。
extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI() {
    return &CaseUIImpl::Instance();
}

// 返回公共虚接口版本。动态宿主必须在调用 GetCaseUI 前校验该值。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return MEYER_CASE_UI_API_VERSION;
}

// 统一版本导出函数。
// MainExe 写版本清单时通过该函数读取代码版本，不需要创建案例管理界面。
extern "C" MEYERSCAN_CASEUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
