#include "SettingsUIImpl.h"

#include "Calibration3DUI.h"
#include "CalibrationColorUI.h"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_SettingsUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_SettingsUI v0.2.0 (2026-06-25)";
}

// 设置主页面内部页索引。只在 SettingsUI 内部使用，不暴露给 MainExe。
enum SettingsPageIndex {
    PageGeneral = 0,
    PageInfo = 1,
    PageCalibration = 2,
    PageCloud = 3,
    PageScan = 4,
    PageData = 5,
    PageAbout = 6,
};
}

// 返回设置模块单例。
SettingsUIImpl& SettingsUIImpl::Instance() {
    static SettingsUIImpl instance;
    return instance;
}

// 初始化设置模块。
bool SettingsUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 路径由 MainExe 基于 applicationDirPath() 传入，避免第三方启动时 currentPath 错误。
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QString::fromUtf8(logDirUtf8 ? logDirUtf8 : "");

    // 日志允许降级。设置主页面应该先能打开，后续再补真实配置读写。
    // 校准 DLL 不在 Init 阶段加载，而是在用户真正点击校准入口时懒加载；
    // 这样从扫描重建打开设置时不会提前占用校准/算法/设备相关资源。
    LoadLogger();
    WriteLog(LogLevel::Info, "Init", "SettingsUI initialized");
    return true;
}

// 保存设置动作回调。
void SettingsUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // SettingsUI 不直接依赖 MainWindow 类型，只保存 C ABI 回调和不透明上下文指针。
    m_actionCallback = callback;
    m_actionCallbackContext = context;
}

// 保存打开设置的来源。
// MainExe、案例管理、扫描重建都可能打开设置；设置模块必须知道来源，才能决定校准入口是否可用。
void SettingsUIImpl::SetOpenContext(int openSource, bool allowCalibration) {
    // 只接受已定义的来源值，避免外部传错数字后污染模块状态。
    if (openSource == SettingsOpenSourceHome ||
        openSource == SettingsOpenSourceCase ||
        openSource == SettingsOpenSourceScanReconstruct) {
        m_openSource = openSource;
    } else {
        m_openSource = SettingsOpenSourceHome;
    }

    // 扫描重建来源打开设置时，校准会占用设备/算法资源，也可能破坏扫描流程，
    // 因此即使调用方误传 allowCalibration=true，这里也强制关闭校准入口。
    m_allowCalibration = allowCalibration && m_openSource != SettingsOpenSourceScanReconstruct;

    WriteLog(LogLevel::Info,
             "SetOpenContext",
             QString("Settings opened from %1, calibration allowed: %2")
                 .arg(OpenSourceName())
                 .arg(m_allowCalibration ? "true" : "false"));

    // 如果设置页面已经创建，立即刷新导航和校准页状态。
    // 这允许同一个 SettingsUI 单例被不同来源重复打开，而不用重启进程。
    ApplyCalibrationAvailability();
}

// 创建设置主界面。
QWidget* SettingsUIImpl::CreateWidget(QWidget* parent) {
    // CreateWidget 可能在页面释放后再次调用，先清空旧页面缓存指针。
    // 真正的 QWidget 生命周期由 MainExe/Qt 父子关系负责，SettingsUI 只维护可刷新状态所需的弱引用。
    DestroyWidget();

    // root 是整页根控件，MainExe 释放 root 时 Qt 会释放内部所有控件。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanSettingsUIRoot");
    root->setMinimumSize(980, 620);
    root->setStyleSheet(
        "QWidget#MeyerScanSettingsUIRoot{background:#f3f5f8;}"
        "QPushButton{font-size:15px;min-height:34px;}"
        "QPushButton#SettingsPrimary{background:#00856b;color:white;border:0;border-radius:4px;padding:8px 22px;}"
        "QPushButton#SettingsNav{border:0;text-align:left;padding:12px 16px;background:transparent;}"
        "QPushButton#SettingsNav:checked{background:#dcefed;color:#063f36;border-radius:4px;}"
        "QFrame#SettingsCard{background:white;border:1px solid #e3e7ed;border-radius:4px;}");

    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* header = new QWidget(root);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 12, 16, 12);
    m_titleLabel = new QLabel(tr("Settings"), header);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    auto* closeButton = new QPushButton(tr("Close"), header);
    closeButton->setFlat(true);
    QObject::connect(closeButton, &QPushButton::clicked, [this]() {
        // 设置模块不直接切回首页/浏览，只通知 MainExe 关闭设置。
        NotifyAction(SettingsActionClose, "Close settings");
    });
    headerLayout->addStretch();
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);
    rootLayout->addWidget(header);

    auto* body = new QWidget(root);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(16, 12, 16, 12);
    bodyLayout->setSpacing(18);

    auto* nav = new QWidget(body);
    nav->setFixedWidth(200);
    auto* navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(8);
    navLayout->addWidget(CreateNavButton(nav, tr("General"), PageGeneral));
    navLayout->addWidget(CreateNavButton(nav, tr("Information"), PageInfo));
    m_calibrationNavButton = CreateNavButton(nav, tr("Calibration"), PageCalibration);
    navLayout->addWidget(m_calibrationNavButton);
    navLayout->addWidget(CreateNavButton(nav, tr("Cloud"), PageCloud));
    navLayout->addWidget(CreateNavButton(nav, tr("Scan"), PageScan));
    navLayout->addWidget(CreateNavButton(nav, tr("Data Processing"), PageData));
    navLayout->addWidget(CreateNavButton(nav, tr("About"), PageAbout));
    navLayout->addStretch();

    m_pages = new QStackedWidget(body);
    m_pages->addWidget(CreateGeneralPage(m_pages));
    m_pages->addWidget(CreateInfoPage(m_pages));
    m_calibrationPage = CreateCalibrationPage(m_pages);
    m_pages->addWidget(m_calibrationPage);
    m_pages->addWidget(CreateCloudPage(m_pages));
    m_pages->addWidget(CreateScanPage(m_pages));
    m_pages->addWidget(CreateDataPage(m_pages));
    m_pages->addWidget(CreateAboutPage(m_pages));

    bodyLayout->addWidget(nav);
    bodyLayout->addWidget(m_pages, 1);
    rootLayout->addWidget(body, 1);

    auto* footer = new QWidget(root);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(18, 10, 18, 14);
    footerLayout->addStretch();
    footerLayout->addWidget(CreateFooterButton(footer, tr("Confirm"), SettingsActionConfirm));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Apply"), SettingsActionApply));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Restore"), SettingsActionRestore));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Cancel"), SettingsActionClose));
    rootLayout->addWidget(footer);

    SwitchToPage(PageGeneral, "General");
    ApplyCalibrationAvailability();
    WriteLog(LogLevel::Info, "CreateWidget", "Settings widget created");
    return root;
}

// 页面销毁前清理内部弱引用。
// 这些指针都指向 CreateWidget() 过程中创建的子控件，不能跨页面生命周期继续使用。
void SettingsUIImpl::DestroyWidget() {
    m_pages = nullptr;
    m_titleLabel = nullptr;
    m_calibrationNavButton = nullptr;
    m_calibrationPage = nullptr;
}

// 返回模块版本字符串。
const char* SettingsUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭设置模块。
void SettingsUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "SettingsUI shutdown");
    if (m_calibration3D) {
        m_calibration3D->Shutdown();
        m_calibration3D = nullptr;
    }
    if (m_calibrationColor) {
        m_calibrationColor->Shutdown();
        m_calibrationColor = nullptr;
    }
    if (m_logger) {
        // 只 Flush，不关闭进程级 Logger；Logger 生命周期由 MainExe 管理。
        m_logger->Flush();
        m_logger = nullptr;
    }
    DestroyWidget();
}

// 加载日志模块并缓存日志接口。
void SettingsUIImpl::LoadLogger() {
    if (m_logger) {
        return;
    }
    if (m_logDir.isEmpty()) {
        return;
    }
    m_loggerLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_loggerLibrary.setFileName("MeyerScan_Logger");
    if (!m_loggerLibrary.load()) {
        return;
    }
    auto getLogger = reinterpret_cast<GetLoggerFunc>(m_loggerLibrary.resolve("GetLogger"));
    if (!getLogger) {
        return;
    }
    m_logger = getLogger();
    // QByteArray 局部变量保证 constData() 在 Init 调用期间有效。
    const QByteArray logDirBytes = m_logDir.toUtf8();
    if (m_logger && !m_logger->Init(logDirBytes.constData(), LogLevel::Info)) {
        m_logger = nullptr;
    }
}

// 加载设置内需要嵌入的校准模块。
// TODO(正式阶段): 考虑加入加载超时机制（如 QTimer::singleShot 兜底），
//                 避免校准 DLL 较长时间未响应时 UI 卡住。
//                 加载慢的情况下建议上方显示 loading 状态占位，而非直接显示
//                 "not available"。
// 当前骨架阶段：静默加载，加载失败不阻断设置页面，设置页会显示占位提示。
void SettingsUIImpl::LoadCalibrationModules() {
    if (!m_calibration3D) {
        m_calibration3DLibrary.setLoadHints(QLibrary::PreventUnloadHint);
        m_calibration3DLibrary.setFileName("MeyerScan_Calibration3DUI");
        if (m_calibration3DLibrary.load()) {
            auto getter = reinterpret_cast<GetCalibration3DUIFunc>(m_calibration3DLibrary.resolve("GetCalibration3DUI"));
            if (getter) {
                m_calibration3D = getter();
                if (m_calibration3D) {
                    // appDir/logDir 字节数组必须在 Init 调用期间保持有效。
                    const QByteArray appDirBytes = m_appDir.toUtf8();
                    const QByteArray logDirBytes = m_logDir.toUtf8();
                    m_calibration3D->Init(appDirBytes.constData(), logDirBytes.constData());
                }
            }
        }
    }
    if (!m_calibrationColor) {
        m_calibrationColorLibrary.setLoadHints(QLibrary::PreventUnloadHint);
        m_calibrationColorLibrary.setFileName("MeyerScan_CalibrationColorUI");
        if (m_calibrationColorLibrary.load()) {
            auto getter = reinterpret_cast<GetCalibrationColorUIFunc>(m_calibrationColorLibrary.resolve("GetCalibrationColorUI"));
            if (getter) {
                m_calibrationColor = getter();
                if (m_calibrationColor) {
                    // appDir/logDir 字节数组必须在 Init 调用期间保持有效。
                    const QByteArray appDirBytes = m_appDir.toUtf8();
                    const QByteArray logDirBytes = m_logDir.toUtf8();
                    m_calibrationColor->Init(appDirBytes.constData(), logDirBytes.constData());
                }
            }
        }
    }
}

// 写结构化日志。
void SettingsUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }
    m_logger->Write(level,
                    QString::fromLatin1(ModuleInfo::Name),
                    QString::fromLatin1(operation ? operation : ""),
                    QString(),
                    QString(),
                    QString(),
                    content);
}

// 上报动作给 MainExe。
void SettingsUIImpl::NotifyAction(int actionId, const QString& content) {
    WriteLog(LogLevel::Info, "SettingsAction", QString("%1 (%2)").arg(content).arg(actionId));
    if (m_actionCallback) {
        m_actionCallback(m_actionCallbackContext, actionId);
    }
}

// 创建导航按钮。
QPushButton* SettingsUIImpl::CreateNavButton(QWidget* parent, const QString& text, int pageIndex) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("SettingsNav");
    button->setCheckable(true);
    QObject::connect(button, &QPushButton::clicked, [this, pageIndex, text]() {
        SwitchToPage(pageIndex, text);
    });
    return button;
}

// 创建底部操作按钮。
QPushButton* SettingsUIImpl::CreateFooterButton(QWidget* parent, const QString& text, int actionId) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("SettingsPrimary");
    QObject::connect(button, &QPushButton::clicked, [this, actionId, text]() {
        NotifyAction(actionId, text);
    });
    return button;
}

// 创建"一般"设置页面。
QWidget* SettingsUIImpl::CreateGeneralPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* guideLabel = new QLabel(tr("New user guide"), page);
    layout->addWidget(guideLabel);
    auto* guideRow = new QHBoxLayout();
    guideRow->addWidget(new QPushButton(tr("Close all"), page));
    guideRow->addWidget(new QPushButton(tr("Open all"), page));
    guideRow->addStretch();
    layout->addLayout(guideRow);

    auto* formatLabel = new QLabel(tr("Data format"), page);
    layout->addWidget(formatLabel);
    auto* formatCombo = new QComboBox(page);
    // VS2015 对 initializer_list 的模板推导比较挑剔，QStringList 写法更稳。
    formatCombo->addItems(QStringList() << "PLY" << "OBJ" << "STL");
    formatCombo->setMinimumWidth(360);
    layout->addWidget(formatCombo);

    // TODO: 路径应从 ConfigCenter 读取（runtime_config.json）。
    // 骨架期先使用系统文档目录作为可显示占位，避免界面中出现开发机 D:/ 路径。
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString basePath = documentsPath.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : documentsPath;
    auto* orderPath = new QLineEdit(basePath + "/MeyerScan/Orders", page);
    auto* packPath = new QLineEdit(basePath + "/MeyerScan/Packages", page);
    layout->addWidget(new QLabel(tr("Order storage path"), page));
    layout->addWidget(orderPath);
    layout->addWidget(new QLabel(tr("Order package path"), page));
    layout->addWidget(packPath);
    layout->addStretch();
    return page;
}

// 创建信息管理页的单个标签页（医生/诊所/技工所共用布局）。
// 骨架期使用占位数据展示表格结构，正式阶段应接入 CaseOrderService。
static QWidget* CreateInfoTabPage(QWidget* parent,
                                   const QStringList& headers,
                                   const QList<QStringList>& rows) {
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 搜索栏 + 添加按钮行。
    auto* searchRow = new QHBoxLayout();
    auto* searchEdit = new QLineEdit(tab);
    searchEdit->setPlaceholderText(QWidget::tr("Search..."));
    searchEdit->setMinimumWidth(280);
    searchRow->addWidget(searchEdit);
    searchRow->addStretch();
    auto* addBtn = new QPushButton(QWidget::tr("Add"), tab);
    addBtn->setObjectName("SettingsPrimary");
    searchRow->addWidget(addBtn);
    layout->addLayout(searchRow);

    // 数据表格。
    auto* table = new QTableWidget(tab);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows[r].size() && c < headers.size(); ++c) {
            table->setItem(r, c, new QTableWidgetItem(rows[r][c]));
        }
    }
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(200);
    layout->addWidget(table, 1);

    // 底部编辑/删除按钮。
    auto* actionRow = new QHBoxLayout();
    actionRow->addStretch();
    auto* editBtn = new QPushButton(QWidget::tr("Edit"), tab);
    auto* deleteBtn = new QPushButton(QWidget::tr("Delete"), tab);
    actionRow->addWidget(editBtn);
    actionRow->addWidget(deleteBtn);
    layout->addLayout(actionRow);

    return tab;
}

// 创建"信息管理"设置页面。
// 使用 QTabWidget 展示医生/诊所/技工所三个标签页，每个标签页包含搜索栏、
// 数据表格和编辑/删除按钮。骨架期使用占位数据，不绑定真实数据库。
QWidget* SettingsUIImpl::CreateInfoPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* tabs = new QTabWidget(page);
    tabs->setDocumentMode(true);

    // 医生管理标签页。
    {
        QStringList headers;
        headers << tr("Name") << tr("Gender") << tr("Phone") << tr("Department");
        QList<QStringList> rows;
        rows << (QStringList() << tr("Zhang San") << tr("Male") << "138****1234" << tr("Orthodontics"));
        rows << (QStringList() << tr("Li Si") << tr("Female") << "139****5678" << tr("General"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Doctors"));
    }

    // 诊所管理标签页。
    {
        QStringList headers;
        headers << tr("Name") << tr("Address") << tr("Phone") << tr("City");
        QList<QStringList> rows;
        rows << (QStringList() << tr("Sunshine Dental") << tr("No.123, Main St") << "010-****8888" << tr("Beijing"));
        rows << (QStringList() << tr("Bright Smile") << tr("No.456, Oak Ave") << "021-****6666" << tr("Shanghai"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Clinics"));
    }

    // 技工所管理标签页。
    {
        QStringList headers;
        headers << tr("Name") << tr("Contact") << tr("Phone") << tr("Address");
        QList<QStringList> rows;
        rows << (QStringList() << tr("Precision Lab") << tr("Wang Wu") << "0755-****2222" << tr("Shenzhen"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Dental Labs"));
    }

    layout->addWidget(tabs, 1);
    return page;
}

// 创建"校准"设置页面。
QWidget* SettingsUIImpl::CreateCalibrationPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(new QLabel(tr("Calibration is available only outside scan reconstruct workflow."), page));
    layout->addWidget(CreateCalibrationCard(page,
                                            tr("3D Calibration"),
                                            tr("Open the 3D calibration workflow"),
                                            SettingsActionOpen3DCalibration));
    layout->addWidget(CreateCalibrationCard(page,
                                            tr("Color Calibration"),
                                            tr("Open the color calibration workflow"),
                                            SettingsActionOpenColorCalibration));
    layout->addStretch();
    return page;
}

// 创建"云端"设置页面。
// 展示云端账号登录状态和服务器配置。骨架期不绑定真实登录逻辑，仅做界面展示。
QWidget* SettingsUIImpl::CreateCloudPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // 云端账号卡片。
    auto* loginCard = new QFrame(page);
    loginCard->setObjectName("SettingsCard");
    auto* loginLayout = new QVBoxLayout(loginCard);
    loginLayout->setContentsMargins(20, 20, 20, 20);
    loginLayout->setSpacing(12);

    auto* accountTitle = new QLabel(tr("Cloud Account"), loginCard);
    QFont titleFont = accountTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    accountTitle->setFont(titleFont);
    loginLayout->addWidget(accountTitle);

    auto* accountStatusRow = new QHBoxLayout();
    accountStatusRow->addWidget(new QLabel(tr("Status:"), loginCard));
    auto* statusLabel = new QLabel(tr("Not logged in"), loginCard);
    statusLabel->setStyleSheet("color:#999999;");
    accountStatusRow->addWidget(statusLabel);
    accountStatusRow->addStretch();
    loginLayout->addLayout(accountStatusRow);

    // 登录表单。
    auto* userEdit = new QLineEdit(loginCard);
    userEdit->setPlaceholderText(tr("Username / Email"));
    loginLayout->addWidget(userEdit);
    auto* passEdit = new QLineEdit(loginCard);
    passEdit->setPlaceholderText(tr("Password"));
    passEdit->setEchoMode(QLineEdit::Password);
    loginLayout->addWidget(passEdit);

    auto* loginBtn = new QPushButton(tr("Log In"), loginCard);
    loginBtn->setObjectName("SettingsPrimary");
    loginLayout->addWidget(loginBtn);

    layout->addWidget(loginCard);

    // 云服务器配置卡片。
    auto* serverCard = new QFrame(page);
    serverCard->setObjectName("SettingsCard");
    auto* serverLayout = new QVBoxLayout(serverCard);
    serverLayout->setContentsMargins(20, 20, 20, 20);
    serverLayout->setSpacing(12);

    auto* serverTitle = new QLabel(tr("Server Configuration"), serverCard);
    serverTitle->setFont(titleFont);
    serverLayout->addWidget(serverTitle);

    auto* serverEdit = new QLineEdit(serverCard);
    serverEdit->setText(tr("https://cloud.meyerscan.com"));
    serverEdit->setMinimumWidth(400);
    serverLayout->addWidget(serverEdit);

    auto* uploadRow = new QHBoxLayout();
    uploadRow->addWidget(new QLabel(tr("Auto upload after completion:"), serverCard));
    auto* uploadCombo = new QComboBox(serverCard);
    uploadCombo->addItems(QStringList() << tr("Enabled") << tr("Disabled"));
    uploadRow->addWidget(uploadCombo);
    uploadRow->addStretch();
    serverLayout->addLayout(uploadRow);

    layout->addWidget(serverCard);
    layout->addStretch();
    return page;
}

// 创建"扫描"设置页面。
// 提供扫描行为相关的配置选项。骨架期所有控件仅展示界面，不持久化配置。
QWidget* SettingsUIImpl::CreateScanPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* scanCard = new QFrame(page);
    scanCard->setObjectName("SettingsCard");
    auto* cardLayout = new QVBoxLayout(scanCard);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(14);

    auto* scanTitle = new QLabel(tr("Scan Behavior"), scanCard);
    QFont titleFont = scanTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    scanTitle->setFont(titleFont);
    cardLayout->addWidget(scanTitle);

    // 扫描提示图开关。
    auto* hintCheck = new QCheckBox(tr("Show scan prompt image"), scanCard);
    hintCheck->setChecked(true);
    cardLayout->addWidget(hintCheck);

    // 可续扫时间。
    auto* rescanRow = new QHBoxLayout();
    rescanRow->addWidget(new QLabel(tr("Rescan available period:"), scanCard));
    auto* rescanCombo = new QComboBox(scanCard);
    rescanCombo->addItems(QStringList() << "3" << "5" << "7" << "15" << tr("Unlimited"));
    rescanCombo->setCurrentIndex(2);
    rescanRow->addWidget(rescanCombo);
    rescanRow->addWidget(new QLabel(tr("days"), scanCard));
    rescanRow->addStretch();
    cardLayout->addLayout(rescanRow);

    // 录屏开关。
    auto* screenRecordCheck = new QCheckBox(tr("Enable screen recording during scan"), scanCard);
    screenRecordCheck->setChecked(false);
    cardLayout->addWidget(screenRecordCheck);

    // 默认订单类型。
    auto* orderTypeRow = new QHBoxLayout();
    orderTypeRow->addWidget(new QLabel(tr("Default order type:"), scanCard));
    auto* orderTypeCombo = new QComboBox(scanCard);
    orderTypeCombo->addItems(QStringList() << tr("Restoration") << tr("Orthodontics") << tr("Implant"));
    orderTypeRow->addWidget(orderTypeCombo);
    orderTypeRow->addStretch();
    cardLayout->addLayout(orderTypeRow);

    // 完成后跳转。
    auto* afterScanRow = new QHBoxLayout();
    afterScanRow->addWidget(new QLabel(tr("After scan completion:"), scanCard));
    auto* afterScanCombo = new QComboBox(scanCard);
    afterScanCombo->addItems(QStringList() << tr("Stay on scan page") << tr("Go to data processing") << tr("Return to home"));
    afterScanRow->addWidget(afterScanCombo);
    afterScanRow->addStretch();
    cardLayout->addLayout(afterScanRow);

    // 体感控制。
    auto* gestureCheck = new QCheckBox(tr("Enable gesture control"), scanCard);
    gestureCheck->setChecked(false);
    cardLayout->addWidget(gestureCheck);

    layout->addWidget(scanCard);
    layout->addStretch();
    return page;
}

// 创建"数据处理"设置页面。
// 提供数据处理相关的参数配置。骨架期仅展示界面，不绑定真实处理引擎。
QWidget* SettingsUIImpl::CreateDataPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* dataCard = new QFrame(page);
    dataCard->setObjectName("SettingsCard");
    auto* cardLayout = new QVBoxLayout(dataCard);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(14);

    auto* dataTitle = new QLabel(tr("Data Processing Profile"), dataCard);
    QFont titleFont = dataTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    dataTitle->setFont(titleFont);
    cardLayout->addWidget(dataTitle);

    // 处理配置选择。
    auto* profileRow = new QHBoxLayout();
    profileRow->addWidget(new QLabel(tr("Processing profile:"), dataCard));
    auto* profileCombo = new QComboBox(dataCard);
    profileCombo->addItems(QStringList() << tr("Standard") << tr("High quality") << tr("Fast"));
    profileRow->addWidget(profileCombo);
    profileRow->addStretch();
    cardLayout->addLayout(profileRow);

    // 上下颌补洞范围。
    auto* jawRangeRow = new QHBoxLayout();
    jawRangeRow->addWidget(new QLabel(tr("Jaw hole-filling range:"), dataCard));
    auto* jawSpin = new QComboBox(dataCard);
    jawSpin->addItems(QStringList() << tr("None") << tr("Small") << tr("Medium") << tr("Large"));
    jawSpin->setCurrentIndex(2);
    jawRangeRow->addWidget(jawSpin);
    jawRangeRow->addStretch();
    cardLayout->addLayout(jawRangeRow);

    // 扫描杆补洞范围。
    auto* scanBodyRow = new QHBoxLayout();
    scanBodyRow->addWidget(new QLabel(tr("Scan body hole-filling range:"), dataCard));
    auto* scanBodySpin = new QComboBox(dataCard);
    scanBodySpin->addItems(QStringList() << tr("None") << tr("Small") << tr("Medium") << tr("Large"));
    scanBodySpin->setCurrentIndex(1);
    scanBodyRow->addWidget(scanBodySpin);
    scanBodyRow->addStretch();
    cardLayout->addLayout(scanBodyRow);

    layout->addWidget(dataCard);
    layout->addStretch();
    return page;
}

// 创建"关于"页面。
QWidget* SettingsUIImpl::CreateAboutPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    auto* brand = new QLabel(tr("MEYER"), page);
    QFont brandFont = brand->font();
    brandFont.setPointSize(24);
    brandFont.setBold(true);
    brand->setFont(brandFont);
    brand->setAlignment(Qt::AlignCenter);
    layout->addWidget(brand);
    layout->addWidget(new QLabel(tr("Software name: MeyerScan Digital Dental Scanner"), page));
    layout->addWidget(new QLabel(tr("Product version: V2"), page));
    layout->addWidget(new QLabel(tr("Authorized user: Hefei Meyer Optoelectronic Technology Co., Ltd."), page));
    layout->addWidget(new QLabel(tr("Valid until: 2099-01-01"), page));
    layout->addWidget(new QLabel(tr("Device number: 6200002000000"), page));
    return page;
}

// 创建校准入口卡片。
QWidget* SettingsUIImpl::CreateCalibrationCard(QWidget* parent,
                                               const QString& title,
                                               const QString& description,
                                               int actionId) {
    auto* card = new QFrame(parent);
    card->setObjectName("SettingsCard");
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 16);
    auto* textLayout = new QVBoxLayout();
    auto* titleLabel = new QLabel(title, card);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(new QLabel(description, card));
    layout->addLayout(textLayout, 1);

    auto* button = new QPushButton(tr("Calibrate"), card);
    button->setObjectName("SettingsPrimary");
    button->setEnabled(m_allowCalibration);
    QObject::connect(button, &QPushButton::clicked, [this, actionId, title]() {
        if (!m_allowCalibration) {
            // 扫描重建来源禁止校准，即使按钮状态异常也要在执行前拦截。
            WriteLog(LogLevel::Warning, "CalibrationBlocked", "Calibration is blocked for current open source");
            return;
        }
        // 先在设置模块内嵌入校准页面，再上报动作给 MainExe 记录跨模块行为。
        ShowEmbeddedCalibration(actionId);
        NotifyAction(actionId, title);
    });
    layout->addWidget(button);
    return card;
}

// 切换设置内部分类页。
void SettingsUIImpl::SwitchToPage(int pageIndex, const QString& pageName) {
    if (!m_pages) {
        return;
    }
    if (pageIndex >= 0 && pageIndex < m_pages->count()) {
        m_pages->setCurrentIndex(pageIndex);
        if (m_titleLabel) {
            m_titleLabel->setText(tr("Settings"));
        }
        WriteLog(LogLevel::Info, "SwitchPage", pageName);
    }
}

// 在设置页面内部嵌入校准模块页面。
void SettingsUIImpl::ShowEmbeddedCalibration(int actionId) {
    if (!m_pages) {
        return;
    }
    if (!m_allowCalibration) {
        // 扫描重建来源打开设置时，不允许进入三维/颜色校准。
        WriteLog(LogLevel::Warning, "ShowEmbeddedCalibration", "Calibration is blocked for current open source");
        return;
    }
    // 校准模块按需加载，避免设置页面刚打开就占用额外 DLL/算法资源。
    LoadCalibrationModules();

    QWidget* calibrationWidget = nullptr;
    QString title;
    if (actionId == SettingsActionOpen3DCalibration) {
        title = tr("3D Calibration");
        calibrationWidget = m_calibration3D ? m_calibration3D->CreateWidget(m_pages) : nullptr;
    } else if (actionId == SettingsActionOpenColorCalibration) {
        title = tr("Color Calibration");
        calibrationWidget = m_calibrationColor ? m_calibrationColor->CreateWidget(m_pages) : nullptr;
    }

    auto* wrapper = new QWidget(m_pages);
    auto* layout = new QVBoxLayout(wrapper);
    auto* back = new QPushButton(tr("Back to Calibration Settings"), wrapper);
    QObject::connect(back, &QPushButton::clicked, [this, wrapper]() {
        RestoreSettingsOverview();
        wrapper->deleteLater();
    });
    layout->addWidget(back, 0, Qt::AlignLeft);
    if (calibrationWidget) {
        layout->addWidget(calibrationWidget, 1);
    } else {
        layout->addWidget(new QLabel(tr("Calibration module is not available."), wrapper), 1);
    }

    const int index = m_pages->addWidget(wrapper);
    m_pages->setCurrentIndex(index);
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

// 返回设置校准分类页。
void SettingsUIImpl::RestoreSettingsOverview() {
    if (!m_pages) {
        return;
    }
    m_pages->setCurrentIndex(PageCalibration);
    if (m_titleLabel) {
        m_titleLabel->setText(tr("Settings"));
    }
}

// 根据打开来源刷新校准入口。
// 当前策略是扫描重建来源直接隐藏左侧 Calibration 分类，并禁用已创建的校准页。
void SettingsUIImpl::ApplyCalibrationAvailability() {
    if (m_calibrationNavButton) {
        m_calibrationNavButton->setVisible(m_allowCalibration);
        m_calibrationNavButton->setEnabled(m_allowCalibration);
    }

    if (m_calibrationPage) {
        m_calibrationPage->setEnabled(m_allowCalibration);
    }

    if (!m_allowCalibration && m_pages && m_pages->currentIndex() == PageCalibration) {
        // 如果来源切换后当前正停在校准页，立即退回 General，避免用户继续操作旧页面。
        SwitchToPage(PageGeneral, "General");
    }
}

// 把来源枚举转为日志可读文本。
QString SettingsUIImpl::OpenSourceName() const {
    switch (m_openSource) {
    case SettingsOpenSourceHome:
        return "Home";
    case SettingsOpenSourceCase:
        return "Case";
    case SettingsOpenSourceScanReconstruct:
        return "ScanReconstruct";
    default:
        return "Unknown";
    }
}

// 导出设置模块接口。
extern "C" MEYERSCAN_SETTINGSUI_API ISettingsUI* GetSettingsUI() {
    return &SettingsUIImpl::Instance();
}
