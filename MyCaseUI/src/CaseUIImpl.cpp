#include "CaseUIImpl.h"
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_CaseUI";

// 模块版本用于 GetModuleVersion() 和版本清单，必须与 Version.rc 保持一致。
const char* Version = "MeyerScan_CaseUI v0.2.0 (2026-06-26)";
}
}

// 返回案例管理模块单例。
// 当前 DLL 暴露一个 ICaseUI 实例，避免多个页面实例重复初始化基础设施。
CaseUIImpl& CaseUIImpl::Instance() {
    static CaseUIImpl instance;
    return instance;
}

// 初始化案例管理 UI。
// 这里只做日志加载和数据库健康检查；正式患者/订单数据必须通过 CaseOrderService。
bool CaseUIImpl::Init(const char* databaseConfigPath, const char* logDir) {
    // 案例管理 UI 当前只负责界面框架和用户动作上报。
    // 初始化时只确认日志和数据库健康状态，不直接读取业务列表数据。
    LoadLogger(logDir);
    LoadUIComponents();
    InitDatabase(databaseConfigPath);

    // m_lastStatus 会被日志/数据库初始化流程更新。
    // Init 末尾统一输出一次，方便从日志里看到 CaseUI 的启动结论。
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
    m_loggerLibrary.setFileName("MeyerScan_Logger");
    if (!m_loggerLibrary.load()) {
        m_lastStatus = "Logger unavailable; continuing without log output";
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

    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName("MeyerScan_UIComponents");
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local styles");
        return;
    }

    auto getUIComponents = reinterpret_cast<GetUIComponentsFunc>(m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!getUIComponents) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "GetUIComponents export not found");
        return;
    }

    m_uiComponents = getUIComponents();
    if (m_uiComponents) {
        const QByteArray appDirBytes = QCoreApplication::applicationDirPath().toUtf8();
        m_uiComponents->Init(appDirBytes.constData());
    }
}

// 初始化数据库健康检查。
// 如果 MainExe 已经连接数据库，本模块只借用现有连接，不重复初始化。
void CaseUIImpl::InitDatabase(const char* databaseConfigPath) {
    // Database 是进程级单例。CaseUI 只保存引用，用来做健康状态展示和后续服务层复用。
    m_database = GetDatabase();
    if (!m_database) {
        m_lastStatus = "Database instance unavailable";
        return;
    }

    // MainExe 已经连接数据库时直接复用，避免 CaseUI 独立重复连接。
    if (m_database->IsConnected()) {
        m_databaseConnected = true;
        m_lastStatus = QString("Database already connected, %1").arg(m_database->GetModuleVersion());
        return;
    }

    // 测试宿主没有 MainExe 基础设施，所以保留 Init/Connect 能力。
    // 传空路径会让 Database 返回失败，失败信息只展示到状态栏，不让 UI 崩溃。
    VoidResult initResult = m_database->Init(databaseConfigPath ? databaseConfigPath : "");
    if (initResult.IsError()) {
        m_lastStatus = QString("Database init failed: %1").arg(initResult.message ? initResult.message : "unknown");
        return;
    }

    VoidResult connectResult = m_database->Connect();
    if (connectResult.IsError()) {
        m_lastStatus = QString("Database connect failed: %1").arg(connectResult.message ? connectResult.message : "unknown");
        return;
    }

    m_databaseConnected = true;
    m_lastStatus = QString("Database connected, %1").arg(m_database->GetModuleVersion());
}

// 创建案例管理主页面。
// 页面只提供框架和动作入口；列表数据、搜索结果和打开订单规则后续由服务层提供。
QWidget* CaseUIImpl::CreateWidget(QWidget* parent) {
    // root 作为整页根控件，所有子控件都挂在它下面。
    // MainExe 删除 root 时，Qt 会自动删除内部 Layout、按钮、表格等对象。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanCaseUIRoot");
    root->setMinimumSize(900, 560);

    // 使用 Layout 而不是固定坐标，避免多语言文本变长后遮挡或错位。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    // 可见文本必须使用 tr("English source text")，中文显示由 qm 翻译文件处理。
    QPushButton* backButton = m_uiComponents
        ? m_uiComponents->CreateButton(MeyerButtonRoleSecondary,
                                       MeyerButtonContentTextOnly,
                                       tr("Back Home").toUtf8().constData(),
                                       "",
                                       root)
        : new QPushButton(tr("Back Home"), root);
    backButton->setMinimumWidth(120);
    // 返回按钮的显隐来自 MainExe 综合配置中心和权限模块后的结果。
    backButton->setVisible(IsActionVisible(CaseActionBackHome));
    // enabled=false 时保留按钮位置但禁止点击，避免未授权动作进入流程。
    backButton->setEnabled(IsActionEnabled(CaseActionBackHome));
    QObject::connect(backButton, &QPushButton::clicked, [this]() {
        // 按钮本身不直接访问 MainWindow，只上报动作 ID，保持模块间低耦合。
        NotifyAction(CaseActionBackHome, "BackHome");
    });
    QPushButton* settingsButton = m_uiComponents
        ? m_uiComponents->CreateButton(MeyerButtonRoleSecondary,
                                       MeyerButtonContentTextOnly,
                                       tr("Settings").toUtf8().constData(),
                                       "",
                                       root)
        : new QPushButton(tr("Settings"), root);
    settingsButton->setMinimumWidth(120);
    // 设置入口也由 MainExe 统一下发显隐和启用态，CaseUI 不自己读取 Permission。
    settingsButton->setVisible(IsActionVisible(CaseActionOpenSettings));
    settingsButton->setEnabled(IsActionEnabled(CaseActionOpenSettings));
    QObject::connect(settingsButton, &QPushButton::clicked, [this]() {
        // 浏览模块只上报“打开设置”，真正打开设置页面由 MainExe 负责。
        NotifyAction(CaseActionOpenSettings, "OpenSettings");
    });

    auto* header = new QLabel(tr("Case Management"), root);
    QFont headerFont = header->font();
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    header->setFont(headerFont);
    headerLayout->addWidget(header, 1);
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(backButton);
    layout->addLayout(headerLayout);

    // 患者和订单先保留两个 Tab，后续每个 Tab 内部可继续拆成更小的子页面类。
    auto* tabs = new QTabWidget(root);
    tabs->addTab(CreatePatientTab(tabs), tr("Patients"));
    tabs->addTab(CreateOrderTab(tabs), tr("Orders"));
    QObject::connect(tabs, &QTabWidget::currentChanged, [this, tabs](int index) {
        // Tab 切换也按客户操作写日志，后续问题排查可以还原用户路径。
        NotifyAction(CaseActionSwitchTab, QString("SwitchTab:%1").arg(tabs->tabText(index)));
    });
    layout->addWidget(tabs, 1);

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), root);
    // 当前状态标签用于开发期确认数据库是否可用。
    // 正式界面可由 UIComponents 提供统一状态提示样式。
    status->setStyleSheet(m_databaseConnected ? "color:#1f7a3a;" : "color:#9a3412;");
    layout->addWidget(status);

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

    // 表格当前是空数据骨架，占位展示字段。
    // 正式数据加载必须通过 CaseOrderService 返回 JSON 后再填充，避免 UI 直接拼 SQL。
    auto* table = new QTableWidget(0, 6, page);
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
    layout->addWidget(table, 1);
    return page;
}

// 创建订单管理 Tab。
// “Open” 动作会上报给 MainExe，后续必须走 OrderWorkflowService 再进入扫描重建。
QWidget* CaseUIImpl::CreateOrderTab(QWidget* parent) {
    // 订单页与患者页保持相同结构，便于后续拆成独立小类或复用列表组件。
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout();
    auto* search = new QLineEdit(page);
    // 订单搜索后续应传给服务层处理，UI 不直接理解数据库结构。
    search->setPlaceholderText(tr("Search order id, patient, doctor or type"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        tr("Import Order"),
        tr("Export Order"),
        tr("Open"),
        tr("Delete")
    };
    const int actionIds[] = {
        // 与 buttons 数组同下标对应，新增/删除按钮时必须同步维护。
        CaseActionImportOrder,
        CaseActionExportOrder,
        CaseActionOpenOrder,
        CaseActionDeleteOrder,
    };
    for (int i = 0; i < buttons.size(); ++i) {
        const int actionId = actionIds[i];
        const QString actionName = buttons[i];
        const int role = actionId == CaseActionOpenOrder
            ? MeyerButtonRolePrimary
            : (actionId == CaseActionDeleteOrder ? MeyerButtonRoleDanger : MeyerButtonRoleSecondary);
        const QByteArray buttonTextBytes = actionName.toUtf8();
        QPushButton* button = m_uiComponents
            ? m_uiComponents->CreateButton(role,
                                           MeyerButtonContentTextOnly,
                                           buttonTextBytes.constData(),
                                           "",
                                           page)
            : new QPushButton(actionName, page);
        // 复制 actionName 是为了日志中记录用户看到的动作名称。
        QObject::connect(button, &QPushButton::clicked, [this, actionId, actionName]() {
            // Open 动作也不在 CaseUI 内直接打开扫描模块，而是上报给 MainExe 统一编排。
            NotifyAction(actionId, actionName);
        });
        toolbar->addWidget(button);
    }
    QObject::connect(search, &QLineEdit::returnPressed, [this]() {
        // 搜索触发先只写日志和上报，等查询接口稳定后再补参数传递。
        NotifyAction(CaseActionSearchOrder, "SearchOrder");
    });
    layout->addLayout(toolbar);

    // 空表格用于验证布局和表头多语言，真实行数据后续由服务层填充。
    auto* table = new QTableWidget(0, 7, page);
    table->setHorizontalHeaderLabels({
        tr("Order ID"),
        tr("Patient"),
        tr("Type"),
        tr("Doctor"),
        tr("Status"),
        tr("Created"),
        tr("Data")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table, 1);
    return page;
}

// 返回模块版本字符串。
// 修改 Version.rc 时必须同步更新这里，保证版本清单一致。
const char* CaseUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 释放案例 UI 模块状态。
// 不关闭全局 Logger / Database；MainExe 会统一管理基础设施生命周期。
void CaseUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CaseUI shutdown");
    if (m_logger) {
        // 只刷盘，不关闭 Logger；Logger 生命周期由 MainExe 统一控制。
        m_logger->Flush();
    }
    // 这些指针都不是 CaseUI 创建的对象，Shutdown 只清引用，避免误删进程级单例。
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    m_uiComponents = nullptr;
    // QLibrary uses PreventUnloadHint to avoid process-exit unload-order issues.
}

// 写结构化日志。
// UI 内部使用 QString 组织内容，写入 Logger 前统一转 UTF-8。
void CaseUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        // 日志模块不可用时，UI 仍应能正常响应。
        return;
    }
    // CaseUI 是 Qt 模块，直接使用 Logger.h 提供的 QString 便捷接口。
    // 跨 DLL 边界前仍会转成 UTF-8 const char*，不会把 QString 对象传进 Logger.dll。
    m_logger->Write(level,
                    QString::fromLatin1(ModuleInfo::Name),
                    QString::fromLatin1(operation ? operation : ""),
                    QString(),
                    QString(),
                    QString(),
                    content);
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
