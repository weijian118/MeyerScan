#include "HomeUIImpl.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_HomeUI";

// 模块版本用于 GetModuleVersion() 和版本清单，必须与 Version.rc 保持一致。
const char* Version = "MeyerScan_HomeUI v0.1.0 (2026-06-17)";
}
}

// 返回首页模块单例。
// 当前 DLL 只暴露一个 IHomeUI 实例，避免多个首页对象重复初始化 Logger/Database。
HomeUIImpl& HomeUIImpl::Instance() {
    static HomeUIImpl instance;
    return instance;
}

// 初始化首页模块。
// 这里只做日志加载和数据库健康检查；正式患者/订单数据必须由服务层提供。
bool HomeUIImpl::Init(const char* databaseConfigPath, const char* logDir) {
    // 首页模块本身不保存业务数据，只在启动时确认日志和数据库是否可用。
    // 这样首页既能独立测试，也能在 MainExe 中复用已经初始化好的基础设施。
    LoadLogger(logDir);
    InitDatabase(databaseConfigPath);

    // m_lastStatus 在 LoadLogger/InitDatabase 内部会被更新，
    // 这里统一写一次 Init 结果，方便排查模块初始化卡在哪一步。
    WriteLog(LogLevel::Info, "Init", m_lastStatus);
    return true;
}

// 保存 MainExe 传入的入口回调。
// HomeUI 不直接切换页面，只把 HomeEntryCreate/HomeEntryBrowse 等 ID 上报出去。
void HomeUIImpl::SetEntryCallback(void (*callback)(void* context, int entryId), void* context) {
    // C ABI 回调不能捕获 C++ 对象，所以把 MainExe 的 this 指针作为 context 原样保存。
    // 用户点击按钮时再把 context 传回去，由 MainExe 自己转换成 MainWindow*。
    m_entryCallback = callback;
    m_entryCallbackContext = context;
}

// 设置入口显隐。
// entryId 必须是 HomeEntry 枚举中的有效值；无效值直接忽略。
void HomeUIImpl::SetEntryVisible(int entryId, bool visible) {
    // m_entryVisible 的下标与 HomeEntry 枚举值一一对应。
    // 0 号位不用，合法入口是 1..4，超出范围说明调用方传错了 ID。
    if (entryId > 0 && entryId < 5) {
        m_entryVisible[entryId] = visible;
    }
}

// 设置入口启用态。
// entryId 必须是 HomeEntry 枚举中的有效值；无效值直接忽略。
void HomeUIImpl::SetEntryEnabled(int entryId, bool enabled) {
    // enabled 与 visible 分开保存。
    // 这样客户包可以把某个入口显示出来但暂时禁用，后续再配合 tooltip/弹窗解释原因。
    if (entryId > 0 && entryId < 5) {
        m_entryEnabled[entryId] = enabled;
    }
}

// 加载日志模块并缓存 ILogger 指针。
// 稳定性优先：加载失败只记录状态，不阻止首页创建，避免日志问题拖垮 UI。
void HomeUIImpl::LoadLogger(const char* logDir) {
    // 首页模块只在第一次初始化时缓存 ILogger*。
    // 后续反复 CreateWidget/Shutdown 不反复 GetLogger，减少 DLL 生命周期复杂度。
    if (m_logger) {
        return;
    }
    // logDir 必须由 MainExe 基于 applicationDirPath() 传入。
    // 这里不自己猜路径，是为了避免第三方软件启动时 currentPath 错误。
    if (!logDir || !logDir[0]) {
        m_lastStatus = "Logger log directory not configured; continuing without log output";
        return;
    }

    // PreventUnloadHint 表示进程退出前尽量不要卸载 Logger DLL。
    // 这样可以规避 Qt/Windows 退出阶段 DLL 卸载顺序导致的悬空函数指针。
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

    // GetLogger 返回的是日志模块内部单例，HomeUI 只借用，不负责 delete。
    m_logger = getLogger();
    if (!m_logger->Init(logDir, LogLevel::Info)) {
        // 日志初始化失败不影响页面显示，避免日志路径异常导致主流程不可用。
        m_logger = nullptr;
        m_lastStatus = "Logger init failed; continuing without log output";
        return;
    }
}

// 初始化数据库健康检查。
// 如果 MainExe 已经完成数据库连接，本模块只借用已有连接，不重复 Init/Connect。
void HomeUIImpl::InitDatabase(const char* databaseConfigPath) {
    // Database 模块也是进程级单例，HomeUI 只保存裸指针引用。
    // 数据库连接的真实生命周期由 MainExe 统一管理。
    m_database = GetDatabase();
    if (!m_database) {
        m_lastStatus = "Database instance unavailable";
        return;
    }

    // 如果 MainExe 已经连好数据库，UI 模块直接复用连接。
    // 这样可以避免多个模块重复 Connect，减少 Qt connectionName 和事务状态问题。
    if (m_database->IsConnected()) {
        m_databaseConnected = true;
        m_lastStatus = QString("Database already connected, %1").arg(m_database->GetModuleVersion());
        return;
    }

    // 独立测试宿主运行时 MainExe 不存在，所以这里保留自初始化能力。
    // databaseConfigPath 仍必须由测试宿主从 EXE 目录推导后传入。
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

// 创建首页主界面。
// 当前是框架页：保留四个入口和状态展示，后续视觉细节继续迁入 UIComponents。
QWidget* HomeUIImpl::CreateWidget(QWidget* parent) {
    // root 的 parent 由 MainExe 的内容区容器或测试窗口传入。
    // Qt 会按父子关系释放子控件，后面只需要 delete root 即可释放整页。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanHomeUIRoot");
    root->setMinimumSize(760, 480);

    // 这里先使用 Qt Layout，而不是固定坐标。
    // 多语言文本变长、多分辨率缩放时，Layout 会自动重新分配空间。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);

    auto* title = new QLabel(tr("MeyerScan"), root);
    QFont titleFont = title->font();
    // 标题字体只设置语义大小，不按屏幕比例直接拉伸；
    // 后续统一缩放策略会放到 UIComponents，避免各模块各算一套比例。
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* subtitle = new QLabel(tr("Create, browse, practice, and settings entry shell"), root);
    subtitle->setStyleSheet("color:#555;");
    layout->addWidget(subtitle);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    const QStringList names = {
        // UI 可见文本统一使用 tr("English source text")，
        // 后续中文/英文/其他语言都通过 qm 文件翻译，不在代码里写 if/else 调位置。
        tr("Create"),
        tr("Browse"),
        tr("Practice"),
        tr("Settings")
    };
    const QStringList descs = {
        tr("Create patient and order information"),
        tr("Manage patients, orders, import/export and delete operations"),
        tr("Open scan practice without formal case data"),
        tr("Account, scan, calibration and common settings")
    };

    const int entryIds[] = {
        // entryIds 与 names/descs 使用相同下标。
        // 新增入口时必须三组数组一起扩展，否则按钮显示和上报 ID 会错位。
        HomeEntryCreate,
        HomeEntryBrowse,
        HomeEntryPractice,
        HomeEntrySettings,
    };

    // 每个入口按钮只负责上报 ID，具体进入建单/浏览/练习/设置由 MainExe 和 Workflow 决定。
    for (int i = 0; i < names.size(); ++i) {
        // 按钮文本临时用两行展示：第一行入口名，第二行说明。
        // 正式视觉组件落地后，应迁到 UIComponents 的标准入口卡片/按钮。
        auto* button = new QPushButton(names[i] + "\n" + descs[i], root);
        button->setMinimumSize(220, 110);
        button->setStyleSheet("QPushButton{text-align:left;padding:14px;font-size:15px;} QPushButton:hover{background:#eef5ff;}");
        // lambda 捕获 entryId 的值，而不是捕获 i。
        // 如果捕获 i，循环结束后所有按钮可能都读到同一个最终下标。
        const int entryId = entryIds[i];
        QObject::connect(button, &QPushButton::clicked, [this, entryId]() { NotifyEntryClicked(entryId); });
        // 权限/配置在 MainExe 中先调用 SetEntryVisible 写入状态，
        // CreateWidget 时再按状态决定是否显示按钮。
        button->setVisible(IsEntryVisible(entryId));
        // enabled=false 时按钮保留在界面中但不可点击。
        // 这让客户知道功能存在，同时避免执行未授权动作。
        button->setEnabled(IsEntryEnabled(entryId));
        grid->addWidget(button, i / 2, i % 2);
    }

    layout->addLayout(grid);
    layout->addStretch();

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), root);
    // 状态标签只用于开发期快速看数据库是否连通。
    // 绿色表示可用，棕色表示降级运行，正式 UI 后续可换成统一状态组件。
    status->setStyleSheet(m_databaseConnected ? "color:#1f7a3a;" : "color:#9a3412;");
    layout->addWidget(status);

    WriteLog(LogLevel::Info, "CreateWidget", "Home widget created");
    return root;
}

// 返回模块版本字符串。
// 版本清单和现场排查会读取该值，修改 Version.rc 时也要同步修改这里。
const char* HomeUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 释放首页模块状态。
// 不调用 Logger::Shutdown 或 Database::Shutdown，因为这些进程级单例由 MainExe 统一管理。
void HomeUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "HomeUI shutdown");
    if (m_logger) {
        // Flush 只把本模块已写入的缓冲刷盘，不关闭 Logger。
        // Logger 是进程级资源，关闭动作必须留给 MainExe。
        m_logger->Flush();
    }
    // m_database/m_logger 都是外部单例裸指针，这里只清空引用，不释放对象。
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    // QLibrary uses PreventUnloadHint to avoid process-exit unload-order issues.
}

// 写结构化日志。
// content 使用 QString 便于 UI 模块内部组织文本，跨 DLL 边界前再转成 UTF-8。
void HomeUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        // 日志不可用时直接返回，UI 模块不能因为日志问题影响用户操作。
        return;
    }
    // HomeUI 是 Qt 模块，直接使用 Logger.h 提供的 QString 便捷接口。
    // 跨 DLL 边界前仍会转成 UTF-8 const char*，不会把 QString 对象传进 Logger.dll。
    m_logger->Write(level,
                    QString::fromLatin1(ModuleInfo::Name),
                    QString::fromLatin1(operation ? operation : ""),
                    QString(),
                    QString(),
                    QString(),
                    content);
}

// 入口点击统一处理。
// 先记录用户操作日志，再把入口 ID 上报给 MainExe。
void HomeUIImpl::NotifyEntryClicked(int entryId) {
    WriteLog(LogLevel::Info, "EntryClicked", QString("Home entry clicked: %1").arg(entryId));
    if (m_entryCallback) {
        // HomeUI 不知道 MainExe 如何切页面，只上报入口 ID，保持 UI 模块边界简单。
        m_entryCallback(m_entryCallbackContext, entryId);
    }
}

// 查询入口是否可见。
// 默认返回 true，保证新增入口在没有权限规则时先能被开发人员看到。
bool HomeUIImpl::IsEntryVisible(int entryId) const {
    // 无效 entryId 默认 true，是为了开发期新增按钮时不被错误隐藏。
    // 真正的权限控制仍由 MainExe + Permission 在合法入口上设置。
    return entryId > 0 && entryId < 5 ? m_entryVisible[entryId] : true;
}

// 查询入口是否可点击。
// 默认返回 true，保证新增入口在权限规则未补齐时不会被误禁用。
bool HomeUIImpl::IsEntryEnabled(int entryId) const {
    // 无效 entryId 默认 true，是为了开发期新增按钮时先保持可用。
    // 正式授权控制由 MainExe + Permission 对合法入口统一设置。
    return entryId > 0 && entryId < 5 ? m_entryEnabled[entryId] : true;
}

// C ABI 导出函数。
// MainExe 和测试宿主通过该函数拿到首页模块接口。
extern "C" MEYERSCAN_HOMEUI_API IHomeUI* GetHomeUI() {
    return &HomeUIImpl::Instance();
}
