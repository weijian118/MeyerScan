#include "CaseUIImpl.h"
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cstring>

namespace {
// CaseUI 只展示 RuntimeDataCenter 的只读快照，不拥有数据缓存。
// 读取 domain 时从 512KB 起步，不够则倍增到 32MB，防止患者/订单字段扩展后列表突然变空。
const int kInitialRuntimeDomainBufferSize = 1024 * 512;
const int kMaxRuntimeDomainBufferSize = 1024 * 1024 * 32;

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
// 这里只加载日志、共享 UI 和 RuntimeDataCenter；正式患者/订单写流程必须通过 CaseOrderService。
bool CaseUIImpl::Init(const char* databaseConfigPath, const char* logDir) {
    // 案例管理 UI 当前只负责界面框架和用户动作上报。
    // 初始化时只加载日志、共享 UI 和 RuntimeDataCenter，不直接读取或连接 Database。
    LoadLogger(logDir);
    LoadUIComponents();
    LoadRuntimeDataCenter();
    if (m_runtimeDataCenter) {
        // CaseUI 只初始化 RuntimeDataCenter，不在 UI 模块里 ReloadAll。
        // MainExe 启动期会做全量刷新；独立测试或缓存为空时，LoadRuntimeItems()
        // 调用 GetDomainJson() 会触发 RuntimeDataCenter 对当前 domain 的懒加载。
        const bool runtimeInitOk = m_runtimeDataCenter->Init(databaseConfigPath ? databaseConfigPath : "",
                                                            logDir ? logDir : "");
        WriteLog(runtimeInitOk ? LogLevel::Info : LogLevel::Warning,
                 "RuntimeDataCenter",
                 runtimeInitOk
                     ? "RuntimeDataCenter initialized for lazy domain reads"
                     : "RuntimeDataCenter init failed");
        m_runtimeDataReady = runtimeInitOk;
        m_lastStatus = runtimeInitOk ? "RuntimeDataCenter ready" : "RuntimeDataCenter init failed";
        if (!runtimeInitOk) {
            m_runtimeDataCenter = nullptr;
        }
    } else {
        m_runtimeDataReady = false;
        m_lastStatus = "RuntimeDataCenter unavailable";
    }

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

    // QLibrary 按 DLL 名称在当前 exe 目录等 Qt 搜索路径中查找模块。
    // 这种动态加载方式让共享 UI 组件缺失时可以降级，而不是让 CaseUI 进程启动失败。
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName("MeyerScan_UIComponents");
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local styles");
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
        const QByteArray appDirBytes = QCoreApplication::applicationDirPath().toUtf8();
        m_uiComponents->Init(appDirBytes.constData());
    }
}

// 加载运行时数据中心模块。
// 该模块把旧数据库常用表读取到内存 JSON 快照，CaseUI 后续只读 domain，不拼 SQL。
void CaseUIImpl::LoadRuntimeDataCenter() {
    if (m_runtimeDataCenter) {
        return;
    }

    // RuntimeDataCenter 提供只读 JSON 快照。加载失败时 CaseUI 仍能显示空列表，
    // 这样 UI 框架调试不被数据库读模型模块阻断。
    m_runtimeDataCenterLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_runtimeDataCenterLibrary.setFileName("MeyerScan_RuntimeDataCenter");
    if (!m_runtimeDataCenterLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "RuntimeDataCenter unavailable; table data will be empty");
        return;
    }

    // 和 UIComponents 一样，通过 C ABI 工厂函数拿接口，避免 CaseUI 依赖实现类。
    auto getRuntimeDataCenter = reinterpret_cast<GetRuntimeDataCenterFunc>(
        m_runtimeDataCenterLibrary.resolve("GetRuntimeDataCenter"));
    if (!getRuntimeDataCenter) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "GetRuntimeDataCenter export not found");
        return;
    }

    // 返回的是 RuntimeDataCenter 内部单例；Shutdown 时只清引用，不 unload DLL。
    m_runtimeDataCenter = getRuntimeDataCenter();
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
    // setMinimumWidth 是下限，不是固定宽度；多语言文本更长时按钮仍可被布局拉伸。
    backButton->setMinimumWidth(120);
    // 返回按钮的显隐来自 MainExe 综合配置中心和权限模块后的结果。
    backButton->setVisible(IsActionVisible(CaseActionBackHome));
    // enabled=false 时保留按钮位置但禁止点击，避免未授权动作进入流程。
    backButton->setEnabled(IsActionEnabled(CaseActionBackHome));
    QObject::connect(backButton, &QPushButton::clicked, [this]() {
        // clicked 信号没有业务参数，所以用 lambda 把固定 actionId 包进去。
        // 这种写法比给每个按钮写单独槽函数更短，但仍然保持动作 ID 集中管理。
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
        // lambda 捕获 this 后调用成员函数；Qt 会在 sender 或 receiver 销毁时自动断开连接。
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
        // 捕获 tabs 是为了读取当前 tab 文案；tabs 是 root 子对象，连接生命周期内有效。
        // Tab 切换也按客户操作写日志，后续问题排查可以还原用户路径。
        NotifyAction(CaseActionSwitchTab, QString("SwitchTab:%1").arg(tabs->tabText(index)));
    });
    layout->addWidget(tabs, 1);

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), root);
    // 当前状态标签用于开发期确认只读快照链路是否可用。
    // 正式界面可由 UIComponents 提供统一状态提示样式。
    status->setStyleSheet(m_runtimeDataReady ? "color:#1f7a3a;" : "color:#9a3412;");
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

    // 表格当前读取 RuntimeDataCenter 的只读快照。
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
    FillPatientTable(table, LoadRuntimeItems("local.patients"));
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
        // 和患者页一样，动作 ID 与按钮文案按同一下标绑定。
        // 这种局部表驱动写法比散落多个 connect 更容易检查按钮和 actionId 是否一致。
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

    // 订单表当前读取 RuntimeDataCenter 的只读快照。
    // 打开订单、状态修改、删除等写流程后续由 CaseOrderService / OrderWorkflowService 承担。
    auto* table = new QTableWidget(0, 7, page);
    // 订单摘要只显示轻量字段；完整扫描数据路径/矩阵等不能在列表页一次性加载。
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
    // local.orders 是 RuntimeDataCenter 对外 domain，不是数据库表名。
    FillOrderTable(table, LoadRuntimeItems("local.orders"));
    layout->addWidget(table, 1);
    return page;
}

// 返回模块版本字符串。
// 修改 Version.rc 时必须同步更新这里，保证版本清单一致。
const char* CaseUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 释放案例 UI 模块状态。
// 不关闭全局 Logger / RuntimeDataCenter；MainExe 会统一管理基础设施生命周期。
void CaseUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CaseUI shutdown");
    if (m_logger) {
        // 只刷盘，不关闭 Logger；Logger 生命周期由 MainExe 统一控制。
        m_logger->Flush();
    }
    // 这些指针都不是 CaseUI 创建的对象，Shutdown 只清引用，避免误删进程级单例。
    m_runtimeDataReady = false;
    m_logger = nullptr;
    m_uiComponents = nullptr;
    m_runtimeDataCenter = nullptr;
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

// 从 RuntimeDataCenter 读取指定 domain 的 items 数组。
QJsonArray CaseUIImpl::LoadRuntimeItems(const char* domain) {
    // 这里故意只接受 domain 字符串，不接受 SQL 或表名。
    // 这样 CaseUI 即使要扩展列表字段，也不会越过 RuntimeDataCenter 白名单直接访问数据库。
    if (!m_runtimeDataCenter || !domain || !domain[0]) {
        return QJsonArray();
    }

    QByteArray buffer;
    RuntimeDataCenterResult result;
    std::memset(&result, 0, sizeof(result));

    // RuntimeDataCenter 的接口由调用方提供缓冲区。
    // 为了保持跨 DLL 内存所有权简单，CaseUI 在本模块内做有限扩容重试。
    for (int bufferSize = kInitialRuntimeDomainBufferSize;
         bufferSize <= kMaxRuntimeDomainBufferSize;
         bufferSize *= 2) {
        // buffer.fill 会调整 QByteArray 大小并填充 '\0'。
        // 预清零能保证 RuntimeDataCenter 写入短字符串时 buffer.constData() 仍是合法 C 字符串。
        buffer.fill('\0', bufferSize);
        // 跨 DLL 大文本采用调用方缓冲区模式，避免“哪个 DLL 分配、哪个 DLL 释放”的 ABI 问题。
        result = m_runtimeDataCenter->GetDomainJson(domain,
                                                    buffer.data(),
                                                    buffer.size());
        if (result.IsSuccess()) {
            break;
        }

        const QString message = QString::fromUtf8(result.message);
        // 只有缓冲区太小才扩容重试；其它错误通常是未初始化、未知 domain 或数据库不可用。
        if (!message.contains("too small", Qt::CaseInsensitive) ||
            bufferSize == kMaxRuntimeDomainBufferSize) {
            WriteLog(LogLevel::Warning, "LoadRuntimeItems",
                     QString("%1: %2").arg(domain, message));
            return QJsonArray();
        }
    }

    QJsonParseError parseError;
    // RuntimeDataCenter 写入 UTF-8 JSON 并以 '\0' 结尾。
    // fromJson(buffer.constData()) 会按真实字符串解析，不会把 QByteArray 尾部预留空间当作 JSON 内容。
    const QJsonDocument document = QJsonDocument::fromJson(buffer.constData(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeItems",
                 QString("%1 returned invalid JSON").arg(domain));
        return QJsonArray();
    }

    // 标准快照结构约定数据行在 items 数组中；字段缺失时 toArray() 返回空数组，UI 显示空表即可。
    return document.object().value("items").toArray();
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
        const QString doctor = FirstText(item, QStringList() << "DENTIST_ID" << "PHYSICIAN_NAME" << "doctorName");
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

// 统一版本导出函数。
// MainExe 写版本清单时通过该函数读取代码版本，不需要创建案例管理界面。
extern "C" MEYERSCAN_CASEUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
