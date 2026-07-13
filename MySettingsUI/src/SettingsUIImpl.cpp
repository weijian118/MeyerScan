#include "SettingsUIImpl.h"

#include "Calibration3DUI.h"
#include "CalibrationColorUI.h"
#include "MeyerQtModuleUtils.h"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstring>

// =============================================================================
// 文件说明:
//   设置模块 UI 实现，负责创建设置主界面、读取运行时只读数据快照、
//   管理校准入口可用性，并把确认/应用/关闭等动作回调给 MainExe。
//
// 模块边界:
//   - 不直接拼 SQL，不直接包含 Database.h。
//   - 医生/诊所/技工所等列表通过 RuntimeDataCenter 读取。
//   - 3D 校准和颜色校准按需懒加载，扫描重建来源打开设置时禁止校准。
//   - Logger、RuntimeDataCenter、Calibration DLL 都是借用接口，不由 SettingsUI delete。
//
// 阅读重点:
//   - UI 文案统一写英文并用 tr() 包裹，后续由 qm 翻译。
//   - 界面布局使用 Qt Layout，不使用固定坐标，降低多语言和多分辨率维护成本。
//   - 设置页内部 QStackedWidget 只管理设置分类页，不代表 MainExe 级页面常驻缓存。
// =============================================================================

namespace {
const int kInitialRuntimeDomainBufferSize = 512 * 1024;
const int kMaxRuntimeDomainBufferSize = 32 * 1024 * 1024;

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_SettingsUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_SettingsUI v0.2.2 (2026-07-13)";
}

// 根据安装目录解析数据库配置路径。
// SettingsUI 独立测试时 appDir 指向测试 exe 目录；正式运行时 appDir 指向 MeyerScan.exe 目录。
// 两种场景都只从传入路径附近查找，不依赖 currentPath。
QString ResolveDatabaseConfigPath(const QString& appDir) {
    // 传入空路径时直接返回空字符串，让调用方写日志并降级，而不是在这里猜测路径。
    if (appDir.isEmpty()) {
        return QString();
    }

    // 正式发布目录优先：MainExe 会把 db_config.json 复制到 MeyerScan.exe 同级 config。
    const QString deployedPath = QDir(appDir).filePath("config/db_config.json");
    if (QFileInfo::exists(deployedPath)) {
        return deployedPath;
    }

    // 开发期 SettingsUITest.exe 位于 MySettingsUI/bin/Release。
    // 向上三级可回到 F:/MeyerScan，再定位 MyDatabase/config/db_config.json。
    QDir repoDir(appDir);
    // QDir::cdUp() 会原地修改目录对象，三个条件短路失败时不会继续拼错误路径。
    if (repoDir.cdUp() && repoDir.cdUp() && repoDir.cdUp()) {
        const QString devPath = repoDir.filePath("MyDatabase/config/db_config.json");
        if (QFileInfo::exists(devPath)) {
            return devPath;
        }
    }

    // 返回 deployedPath 即使文件不存在，也能让下游日志看到期望路径，方便排查缺文件。
    return deployedPath;
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
    LoadRuntimeDataCenter();
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
    // objectName 给 QSS 精确选择器使用，避免把同一进程里其它 QWidget 背景也改掉。
    root->setObjectName("MeyerScanSettingsUIRoot");
    // minimumSize 是下限，不是固定尺寸；窗口放大缩小时仍由 layout 自动伸缩。
    root->setMinimumSize(980, 620);
    // 样式统一从 Resources/Modules/MySettingsUI/qss/settings.qss 加载。
    // 业务源码只保留 objectName/property，避免后续改视觉效果时重新编译 DLL。
    MeyerQtModule::ApplyModuleQss(root, "MySettingsUI", "settings.qss", m_logger);

    auto* rootLayout = new QVBoxLayout(root);
    // 根布局外边距设为 0，让设置页填满 MainExe 内容区；内边距由 header/body/footer 自己控制。
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* header = new QWidget(root);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 12, 16, 12);
    m_titleLabel = new QLabel(tr("Settings"), header);
    QFont titleFont = m_titleLabel->font();
    // 复制一份 QFont 再修改，避免直接改全局应用字体。
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    // 标题在 header 中居中显示，左右 addStretch 抵消关闭按钮宽度带来的视觉偏移。
    m_titleLabel->setAlignment(Qt::AlignCenter);
    auto* closeButton = new QPushButton(tr("Close"), header);
    // flat 按钮减少标题栏右侧的视觉重量，适合“关闭当前页面”这类轻操作。
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
    // body 内边距决定左侧导航和右侧内容与窗口边缘的安全距离。
    bodyLayout->setContentsMargins(16, 12, 16, 12);
    bodyLayout->setSpacing(18);

    auto* nav = new QWidget(body);
    // 导航列固定宽度，右侧内容区吃掉剩余空间；这样多语言下内容区不会被导航挤压。
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
    // QStackedWidget 只用于设置模块内部分类页，不用于 MainExe 首页/浏览这种业务级页面并列缓存。
    // 内部分类页常驻能保证左侧导航切换足够快，且设置页整体关闭时统一释放。
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
    // addStretch 把操作按钮推到右侧，符合设置窗口常见交互习惯。
    footerLayout->addStretch();
    footerLayout->addWidget(CreateFooterButton(footer, tr("Confirm"), SettingsActionConfirm));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Apply"), SettingsActionApply));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Restore"), SettingsActionRestore));
    footerLayout->addWidget(CreateFooterButton(footer, tr("Cancel"), SettingsActionClose));
    rootLayout->addWidget(footer);

    SwitchToPage(PageGeneral, "General");
    // 页面创建后立即根据来源刷新校准入口，避免扫描重建来源短暂看到校准分类。
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
    // RuntimeDataCenter 是进程级只读缓存，SettingsUI 只清引用，不关闭模块。
    m_runtimeDataCenter = nullptr;
    DestroyWidget();
}

// 加载日志模块并缓存日志接口。
void SettingsUIImpl::LoadLogger() {
    if (m_logger) {
        // 已缓存 Logger 指针时直接复用，避免重复 load DLL 和重复 Init。
        return;
    }
    if (m_logDir.isEmpty()) {
        // 日志目录为空说明 Init 没拿到有效路径，设置页仍可运行，只是降级为无日志。
        return;
    }
    // 设置模块通过 QLibrary 动态借用 Logger，避免静态链接导致测试宿主必须固定加载顺序。
    m_loggerLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    // setFileName 不带扩展名时，Qt 会按当前平台自动补 .dll。
    // DLL 查找目录由 EXE 所在目录、PATH 等运行时环境决定。
    m_loggerLibrary.setFileName("MeyerScan_Logger");
    if (!m_loggerLibrary.load()) {
        // 这里不弹窗。Logger 缺失本身应由 MainExe 启动检查和安装包检查发现。
        return;
    }
    // resolve 找 C ABI 导出的 GetLogger；函数名稳定，不受 C++ 类名/命名空间影响。
    auto getLogger = reinterpret_cast<GetLoggerFunc>(m_loggerLibrary.resolve("GetLogger"));
    if (!getLogger) {
        return;
    }
    // 返回的是 Logger 内部单例，SettingsUI 不拥有其生命周期。
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
        // 校准模块用懒加载：只有用户进入校准页时才加载 DLL。
        // 这样从首页/浏览打开普通设置时不会提前占用算法、设备或 UI 资源。
        m_calibration3DLibrary.setLoadHints(QLibrary::PreventUnloadHint);
        m_calibration3DLibrary.setFileName("MeyerScan_Calibration3DUI");
        if (m_calibration3DLibrary.load()) {
            // 工厂函数返回 ICalibration3DUI 接口，SettingsUI 不包含实现类头文件。
            auto getter = reinterpret_cast<GetCalibration3DUIFunc>(m_calibration3DLibrary.resolve("GetCalibration3DUI"));
            if (getter) {
                m_calibration3D = getter();
                if (m_calibration3D) {
                    // appDir/logDir 字节数组必须在 Init 调用期间保持有效。
                    const QByteArray appDirBytes = m_appDir.toUtf8();
                    const QByteArray logDirBytes = m_logDir.toUtf8();
                    // 校准模块初始化失败不阻断设置页，因为设置页还包含大量非校准功能。
                    // Init 只说明接口指针是否真正可用，不能把“DLL 已加载”当成初始化成功。
                    if (!m_calibration3D->Init(appDirBytes.constData(), logDirBytes.constData())) {
                        // 先让子模块清理可能已经创建的半初始化资源，再清空本模块保存的接口。
                        m_calibration3D->Shutdown();
                        m_calibration3D = nullptr;
                        WriteLog(LogLevel::Warning,
                                 "LoadCalibrationModules",
                                 "Calibration3DUI init failed");
                    }
                }
            }
        }
    }
    if (!m_calibrationColor) {
        // 颜色校准与三维校准是两个独立 DLL，互相加载失败不影响另一个入口。
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
                    // 与 3D 校准相同，颜色校准是可选子能力，加载失败时设置页继续可用。
                    if (!m_calibrationColor->Init(appDirBytes.constData(), logDirBytes.constData())) {
                        // Shutdown 处理部分初始化状态；清空指针后 UI 会显示不可用占位内容。
                        m_calibrationColor->Shutdown();
                        m_calibrationColor = nullptr;
                        WriteLog(LogLevel::Warning,
                                 "LoadCalibrationModules",
                                 "CalibrationColorUI init failed");
                    }
                }
            }
        }
    }
}

// 加载运行时数据中心。
// SettingsUI 只读取医生/诊所/技工所等只读快照，不直接连接数据库或拼业务 SQL。
void SettingsUIImpl::LoadRuntimeDataCenter() {
    if (m_runtimeDataCenter) {
        return;
    }
    if (m_appDir.isEmpty()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "Application directory is empty");
        return;
    }

    // RuntimeDataCenter 是进程级读模型缓存，SettingsUI 只需要接口指针。
    // PreventUnloadHint 避免设置页关闭时卸载 DLL，减少 Qt 对象和静态单例析构顺序风险。
    m_runtimeDataCenterLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_runtimeDataCenterLibrary.setFileName("MeyerScan_RuntimeDataCenter");
    if (!m_runtimeDataCenterLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "RuntimeDataCenter DLL is unavailable");
        return;
    }

    // 通过 C ABI 工厂函数拿接口，避免跨 DLL 暴露 C++ 实现类。
    auto getter = reinterpret_cast<GetRuntimeDataCenterFunc>(
        m_runtimeDataCenterLibrary.resolve("GetRuntimeDataCenter"));
    if (!getter) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "GetRuntimeDataCenter export not found");
        return;
    }

    // getter 返回 RuntimeDataCenter 内部单例，不需要 SettingsUI delete。
    m_runtimeDataCenter = getter();
    if (!m_runtimeDataCenter) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "RuntimeDataCenter instance is null");
        return;
    }

    // 独立测试时 MainExe 可能没有初始化 RuntimeDataCenter，所以这里传入数据库配置路径让它可自举。
    const QString databaseConfigPath = ResolveDatabaseConfigPath(m_appDir);
    // QDir::fromNativeSeparators 把反斜杠转为斜杠，跨 Qt/日志/JSON 显示时更稳定。
    const QByteArray databaseConfigBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(m_logDir).toUtf8();
    if (!m_runtimeDataCenter->Init(databaseConfigBytes.constData(), logDirBytes.constData())) {
        WriteLog(LogLevel::Warning, "LoadRuntimeDataCenter", "RuntimeDataCenter init failed");
        m_runtimeDataCenter = nullptr;
        return;
    }

    // SettingsUI 不在这里 ReloadAll。
    // MainExe 启动期会做全量刷新；独立测试或缓存为空时，LoadRuntimeItems()
    // 调用 GetDomainJson() 会按当前页面需要懒加载 local.doctors / local.clinics / local.labs。
    WriteLog(LogLevel::Info,
             "LoadRuntimeDataCenter",
             "RuntimeDataCenter initialized for lazy domain reads");
}

// 写结构化日志。
void SettingsUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // 公共 Qt 日志工具会自动使用 MEYER_MODULE_NAME 填充模块名。
    // 设置模块只关心“发生了什么”，不再重复维护日志字段拼装。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 上报动作给 MainExe。
void SettingsUIImpl::NotifyAction(int actionId, const QString& content) {
    // 先写日志，再回调 MainExe；即使 MainExe 后续处理失败，也能从日志看到用户动作。
    WriteLog(LogLevel::Info, "SettingsAction", QString("%1 (%2)").arg(content).arg(actionId));
    if (m_actionCallback) {
        // context 是 MainExe 传入的不透明指针，SettingsUI 不解释它，只原样传回。
        m_actionCallback(m_actionCallbackContext, actionId);
    }
}

// 创建导航按钮。
QPushButton* SettingsUIImpl::CreateNavButton(QWidget* parent, const QString& text, int pageIndex) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("SettingsNav");
    // checkable 让按钮能表现当前页选中态；具体选中/取消在 SwitchToPage 中统一维护。
    button->setCheckable(true);
    QObject::connect(button, &QPushButton::clicked, [this, pageIndex, text]() {
        // pageIndex/text 按值捕获，按钮点击时即使外部局部变量已销毁也安全。
        SwitchToPage(pageIndex, text);
    });
    return button;
}

// 创建底部操作按钮。
QPushButton* SettingsUIImpl::CreateFooterButton(QWidget* parent, const QString& text, int actionId) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("SettingsPrimary");
    QObject::connect(button, &QPushButton::clicked, [this, actionId, text]() {
        // 底部按钮只上报动作，保存/应用/恢复的真实配置写入后续走 ConfigCenter。
        NotifyAction(actionId, text);
    });
    return button;
}

// 创建"一般"设置页面。
QWidget* SettingsUIImpl::CreateGeneralPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    // 设置页内部也全部使用 Layout，避免不同语言下固定坐标错位。
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

    // 当前骨架使用系统文档目录生成安全默认值，避免界面出现开发机 D:/ 路径。
    // 正式值应由 MainExe/设置服务读取 ConfigCenter 后通过版本化设置上下文注入；
    // SettingsUI 不直接读取 runtime_config.json，防止 UI 层和配置层形成隐式耦合。
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    // 如果系统文档目录取不到，就退回 appDir。这里不是开发机路径回退，而是运行目录兜底。
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
QWidget* SettingsUIImpl::CreateInfoTabPage(QWidget* parent,
                                           const QStringList& headers,
                                           const QList<QStringList>& rows) {
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    // 标签页内部保留 8px 留白，防止表格线贴到 Tab 边界。
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 搜索栏 + 添加按钮行。
    auto* searchRow = new QHBoxLayout();
    auto* searchEdit = new QLineEdit(tab);
    // placeholder 使用 tr 包裹英文源文案，后续 lupdate 可提取翻译。
    searchEdit->setPlaceholderText(QWidget::tr("Search..."));
    searchEdit->setMinimumWidth(280);
    // 搜索框占左侧，右侧按钮固定宽度，Stretch 把两者分开。
    searchRow->addWidget(searchEdit);
    searchRow->addStretch();
    auto* addBtn = new QPushButton(QWidget::tr("Add"), tab);
    addBtn->setObjectName("SettingsPrimary");
    searchRow->addWidget(addBtn);
    layout->addLayout(searchRow);

    // 数据表格只展示 RuntimeDataCenter 提供的只读快照。
    // 新增、编辑、删除按钮目前只保留入口；保存逻辑后续统一走服务层。
    auto* table = new QTableWidget(tab);
    // QTableWidget 适合当前骨架期的少量只读快照；正式大数据量应切换到 model/view + 分页。
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows[r].size() && c < headers.size(); ++c) {
            // setItem 会接管 QTableWidgetItem 所有权，表格销毁时自动释放。
            // c < headers.size() 是防御字段扩展时行数据比表头多，避免越界写列。
            table->setItem(r, c, new QTableWidgetItem(rows[r][c]));
        }
    }
    // 选择整行比单格选择更适合后续“编辑/删除当前记录”的交互。
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 最后一列拉伸到剩余宽度，减少多分辨率下右侧空白。
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(200);
    layout->addWidget(table, 1);

    // 底部编辑/删除按钮。
    auto* actionRow = new QHBoxLayout();
    actionRow->addStretch();
    auto* editBtn = new QPushButton(QWidget::tr("Edit"), tab);
    auto* deleteBtn = new QPushButton(QWidget::tr("Delete"), tab);
    // 当前按钮只搭框架，不直接写数据库；后续应通过服务层弹出编辑对话框或执行删除。
    actionRow->addWidget(editBtn);
    actionRow->addWidget(deleteBtn);
    layout->addLayout(actionRow);

    return tab;
}

// 创建"信息管理"设置页面。
// 使用 QTabWidget 展示医生/诊所/技工所三个标签页，每个标签页包含搜索栏、
// 数据表格和编辑/删除按钮。数据来源是 RuntimeDataCenter 的只读快照。
QWidget* SettingsUIImpl::CreateInfoPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    // 外层不留边距，让内部 QTabWidget 与设置内容区自然对齐。
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* tabs = new QTabWidget(page);
    // documentMode 让 Tab 更像设置页的内部标签，而不是主页面级导航。
    tabs->setDocumentMode(true);

    // 医生管理标签页。
    {
        QStringList headers;
        // 表头是 UI 层稳定文案；真实字段名由 BuildDoctorRows 内部兼容旧库字段。
        headers << tr("Name") << tr("Gender") << tr("Phone") << tr("Department");
        const QList<QStringList> rows = BuildDoctorRows(LoadRuntimeItems("local.doctors"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Doctors"));
    }

    // 诊所管理标签页。
    {
        QStringList headers;
        // 诊所页只展示常用字段，完整字段后续可在编辑弹窗或详情页按需展示。
        headers << tr("Name") << tr("Address") << tr("Phone") << tr("City");
        const QList<QStringList> rows = BuildClinicRows(LoadRuntimeItems("local.clinics"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Clinics"));
    }

    // 技工所管理标签页。
    {
        QStringList headers;
        // 技工所页展示合作方基础信息，云端 ID 等字段暂不直接放在列表里。
        headers << tr("Name") << tr("Contact") << tr("Phone") << tr("Address");
        const QList<QStringList> rows = BuildLabRows(LoadRuntimeItems("local.labs"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Dental Labs"));
    }

    layout->addWidget(tabs, 1);
    return page;
}

// 从 RuntimeDataCenter 读取指定 domain 的 items 数组。
QJsonArray SettingsUIImpl::LoadRuntimeItems(const char* domain) {
    // SettingsUI 只认 RuntimeDataCenter 的 domain，不认数据库表名和 SQL。
    // 这能保证信息页只是“读快照”，不会滑向直接操作数据库。
    if (!m_runtimeDataCenter || !domain || !domain[0]) {
        return QJsonArray();
    }

    QByteArray buffer;
    RuntimeDataCenterResult result;
    std::memset(&result, 0, sizeof(result));

    // RuntimeDataCenter 要求调用方提供缓冲区。
    // 信息管理字段后续会扩展，所以这里采用有限扩容重试，避免固定小缓冲导致误显示为空。
    for (int bufferSize = kInitialRuntimeDomainBufferSize;
         bufferSize <= kMaxRuntimeDomainBufferSize;
         bufferSize *= 2) {
        // QByteArray 填充 '\0' 后作为输出缓冲区传给 RuntimeDataCenter。
        // 成功时缓冲区前半段是 JSON，后面仍是空字节。
        buffer.fill('\0', bufferSize);
        // 调用方缓冲区模式解决跨 DLL 字符串内存所有权问题。
        result = m_runtimeDataCenter->GetDomainJson(domain, buffer.data(), buffer.size());
        if (result.IsSuccess()) {
            // 成功后不继续扩容，buffer 中已经包含完整 JSON。
            break;
        }
        // 只有 “too small” 才表示可以扩容重试；其它错误立即返回空数组并写日志。
        if (!QString::fromUtf8(result.message).contains("too small", Qt::CaseInsensitive)) {
            WriteLog(LogLevel::Warning, "LoadRuntimeItems",
                     QString("%1 failed: %2").arg(domain).arg(QString::fromUtf8(result.message)));
            return QJsonArray();
        }
        // 运行到这里说明只是缓冲区太小，for 循环会把 bufferSize 翻倍后重试。
    }

    if (result.IsError()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeItems",
                 QString("%1 failed: %2").arg(domain).arg(QString::fromUtf8(result.message)));
        return QJsonArray();
    }

    QJsonParseError parseError;
    // RuntimeDataCenter 写入的是以 '\0' 结尾的 UTF-8 JSON。
    // buffer 本身比真实 JSON 大很多，必须按 C 字符串读取，避免尾部空字节导致 JSON 解析失败。
    const QJsonDocument document = QJsonDocument::fromJson(buffer.constData(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteLog(LogLevel::Warning, "LoadRuntimeItems",
                 QString("%1 returned invalid JSON").arg(domain));
        return QJsonArray();
    }

    // RuntimeDataCenter 统一把行数据放在 items，缺失时 toArray 返回空数组，表格保持空状态。
    return document.object().value("items").toArray();
}

// 把医生快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildDoctorRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // 每个 value 是 RuntimeDataCenter 从旧表读出的单行 JSON 对象。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 非对象或空对象不展示，避免把坏数据渲染成一行空白。
            continue;
        }

        const QString genderValue = FirstText(item, QStringList() << "DENTIST_SEX" << "PATIENT_SEX" << "gender");
        QString genderText = genderValue;
        // 旧库中性别常用数字编码；这里在 UI 层转成可翻译文本。
        if (genderValue == "1") {
            genderText = tr("Male");
        } else if (genderValue == "2") {
            genderText = tr("Female");
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "DENTIST_NAME" << "name")
                 << genderText
                 << FirstText(item, QStringList() << "DENTIST_TEL" << "phone")
                 << FirstText(item, QStringList() << "DENTIST_PRO" << "department"));
    }
    return rows;
}

// 把诊所快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildClinicRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // FirstText 支持多个字段名，便于旧表字段和未来新字段共存。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 旧库数据异常时跳过这一行，表格仍然能显示其它正常记录。
            continue;
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "CLINIC_NAME" << "name")
                 << FirstText(item, QStringList() << "CLINIC_DETAILADDRESS" << "CLINIC_ADDRESS" << "CLINIC_LOCATION" << "address")
                 << FirstText(item, QStringList() << "CLINIC_TEL" << "phone")
                 << FirstText(item, QStringList() << "CLINIC_CITY" << "city"));
    }
    return rows;
}

// 把技工所快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildLabRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // 技工所字段在不同版本旧库中可能不一致，候选 key 放在 FirstText 中集中处理。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 跳过空对象，避免表格出现用户无法理解的空白记录。
            continue;
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "LAB_NAME" << "name")
                 << FirstText(item, QStringList() << "LAB_CONTACT" << "contact")
                 << FirstText(item, QStringList() << "LAB_TEL" << "phone")
                 << FirstText(item, QStringList() << "LAB_ADDRESS" << "address"));
    }
    return rows;
}

// 从 JSON 对象读取第一个非空字段。
QString SettingsUIImpl::FirstText(const QJsonObject& object, const QStringList& keys) const {
    // 这是信息页的字段兼容工具：按候选 key 顺序取第一个可显示值。
    // 这样新增字段或旧字段改名时，只改这里调用处的 key 列表，不改表格填充流程。
    for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString()) {
            // 空字符串不适合显示，trim 后仍为空就继续尝试下一个候选字段。
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        } else if (value.isDouble()) {
            // Qt JSON 用 double 表示所有数字，旧库 ID/编码按整数文本展示。
            return QString::number(value.toDouble(), 'f', 0);
        } else if (value.isBool()) {
            // bool 转 true/false，便于后续配置类字段直观看到状态。
            return value.toBool() ? "true" : "false";
        }
    }
    return QString();
}

// 创建"校准"设置页面。
QWidget* SettingsUIImpl::CreateCalibrationPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    // 校准页是设置页内部分类，不单独弹窗口；这能保持设置模块整体视觉一致。
    layout->setSpacing(12);
    layout->addWidget(new QLabel(tr("Calibration is available only outside scan reconstruct workflow."), page));
    // 每张卡片负责一个校准入口，点击后在本设置页内部嵌入对应校准模块。
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
    // 页面内统一使用 16px 外边距，卡片自身再提供 20px 内边距。
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // 云端账号卡片。
    auto* loginCard = new QFrame(page);
    loginCard->setObjectName("SettingsCard");
    auto* loginLayout = new QVBoxLayout(loginCard);
    // QFrame + objectName 让卡片样式由根 QSS 控制，不需要给每个控件单独 setStyleSheet。
    loginLayout->setContentsMargins(20, 20, 20, 20);
    loginLayout->setSpacing(12);

    auto* accountTitle = new QLabel(tr("Cloud Account"), loginCard);
    QFont titleFont = accountTitle->font();
    // 标题字体只作用于当前 label，后续 serverTitle 复用这份字体值。
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    accountTitle->setFont(titleFont);
    loginLayout->addWidget(accountTitle);

    auto* accountStatusRow = new QHBoxLayout();
    accountStatusRow->addWidget(new QLabel(tr("Status:"), loginCard));
    auto* statusLabel = new QLabel(tr("Not logged in"), loginCard);
    // 颜色只是骨架期提示，后续应收敛到 UIComponents 的状态标签样式。
    statusLabel->setObjectName("SettingsStatusMutedLabel");
    accountStatusRow->addWidget(statusLabel);
    accountStatusRow->addStretch();
    loginLayout->addLayout(accountStatusRow);

    // 登录表单。
    auto* userEdit = new QLineEdit(loginCard);
    userEdit->setPlaceholderText(tr("Username / Email"));
    loginLayout->addWidget(userEdit);
    auto* passEdit = new QLineEdit(loginCard);
    passEdit->setPlaceholderText(tr("Password"));
    // Password 模式让 Qt 自动用平台默认密码掩码绘制，不需要手写字符替换。
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
    // 云端地址不在 UI 内硬编码。后续由 MainExe/设置服务读取 ConfigCenter 后注入；
    // 当前只显示可翻译提示，避免占位 URL 被误认为真实生产配置并保存。
    serverEdit->setPlaceholderText(tr("Cloud server URL"));
    serverEdit->setMinimumWidth(400);
    serverLayout->addWidget(serverEdit);

    auto* uploadRow = new QHBoxLayout();
    uploadRow->addWidget(new QLabel(tr("Auto upload after completion:"), serverCard));
    auto* uploadCombo = new QComboBox(serverCard);
    // ComboBox 用于有限选项，避免用户输入不受支持的自由文本。
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
    // 扫描设置页当前只是配置界面骨架，不直接影响扫描重建模块。
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* scanCard = new QFrame(page);
    scanCard->setObjectName("SettingsCard");
    auto* cardLayout = new QVBoxLayout(scanCard);
    // 使用垂直布局逐项堆叠，比固定坐标更容易适配多语言文本长度。
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
    // 默认勾选只代表当前界面默认值，真正配置读写后续由 ConfigCenter 接管。
    hintCheck->setChecked(true);
    cardLayout->addWidget(hintCheck);

    // 可续扫时间。
    auto* rescanRow = new QHBoxLayout();
    rescanRow->addWidget(new QLabel(tr("Rescan available period:"), scanCard));
    auto* rescanCombo = new QComboBox(scanCard);
    // 可续扫时间是枚举型配置，ComboBox 比数字输入更能限制非法值。
    rescanCombo->addItems(QStringList() << "3" << "5" << "7" << "15" << tr("Unlimited"));
    rescanCombo->setCurrentIndex(2);
    rescanRow->addWidget(rescanCombo);
    rescanRow->addWidget(new QLabel(tr("days"), scanCard));
    rescanRow->addStretch();
    cardLayout->addLayout(rescanRow);

    // 录屏开关。
    auto* screenRecordCheck = new QCheckBox(tr("Enable screen recording during scan"), scanCard);
    // 录屏会影响磁盘和性能，骨架期默认关闭。
    screenRecordCheck->setChecked(false);
    cardLayout->addWidget(screenRecordCheck);

    // 默认订单类型。
    auto* orderTypeRow = new QHBoxLayout();
    orderTypeRow->addWidget(new QLabel(tr("Default order type:"), scanCard));
    auto* orderTypeCombo = new QComboBox(scanCard);
    // 订单类型后续应从 CaseOrderService 或配置中心读取，而不是硬编码在 UI 中。
    orderTypeCombo->addItems(QStringList() << tr("Restoration") << tr("Orthodontics") << tr("Implant"));
    orderTypeRow->addWidget(orderTypeCombo);
    orderTypeRow->addStretch();
    cardLayout->addLayout(orderTypeRow);

    // 完成后跳转。
    auto* afterScanRow = new QHBoxLayout();
    afterScanRow->addWidget(new QLabel(tr("After scan completion:"), scanCard));
    auto* afterScanCombo = new QComboBox(scanCard);
    // 完成后跳转属于工作流策略，当前只搭界面入口。
    afterScanCombo->addItems(QStringList() << tr("Stay on scan page") << tr("Go to data processing") << tr("Return to home"));
    afterScanRow->addWidget(afterScanCombo);
    afterScanRow->addStretch();
    cardLayout->addLayout(afterScanRow);

    // 体感控制。
    auto* gestureCheck = new QCheckBox(tr("Enable gesture control"), scanCard);
    // 体感控制可能依赖设备能力，后续应由权限/设备信息共同决定可用状态。
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
    // 数据处理页只搭参数框架，真正算法参数应由扫描/算法模块定义并校验。
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* dataCard = new QFrame(page);
    dataCard->setObjectName("SettingsCard");
    auto* cardLayout = new QVBoxLayout(dataCard);
    // cardLayout 管理卡片内部项目间距，避免不同 DPI 下手工坐标错位。
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
    // 处理配置属于有限集合，用下拉框比自由输入更容易维护配置兼容性。
    profileCombo->addItems(QStringList() << tr("Standard") << tr("High quality") << tr("Fast"));
    profileRow->addWidget(profileCombo);
    profileRow->addStretch();
    cardLayout->addLayout(profileRow);

    // 上下颌补洞范围。
    auto* jawRangeRow = new QHBoxLayout();
    jawRangeRow->addWidget(new QLabel(tr("Jaw hole-filling range:"), dataCard));
    auto* jawSpin = new QComboBox(dataCard);
    // 当前使用 ComboBox 代替数值 SpinBox，是因为范围语义更接近业务等级而非连续数值。
    jawSpin->addItems(QStringList() << tr("None") << tr("Small") << tr("Medium") << tr("Large"));
    jawSpin->setCurrentIndex(2);
    jawRangeRow->addWidget(jawSpin);
    jawRangeRow->addStretch();
    cardLayout->addLayout(jawRangeRow);

    // 扫描杆补洞范围。
    auto* scanBodyRow = new QHBoxLayout();
    scanBodyRow->addWidget(new QLabel(tr("Scan body hole-filling range:"), dataCard));
    auto* scanBodySpin = new QComboBox(dataCard);
    // 扫描杆补洞范围和上下颌范围保持相同选项，便于用户理解和后续配置保存。
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
    // 关于页内容居中显示；这些文本后续应来自版本清单、许可和设备信息。
    layout->setAlignment(Qt::AlignCenter);
    auto* brand = new QLabel(tr("MEYER"), page);
    QFont brandFont = brand->font();
    brandFont.setPointSize(24);
    brandFont.setBold(true);
    brand->setFont(brandFont);
    // QLabel 自身居中，配合外层 layout 居中，保证不同窗口宽度下品牌名仍在视觉中心。
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
    // 横向布局左侧放说明，右侧放动作按钮；说明区域 stretch=1 吃掉剩余宽度。
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
    // 按钮初始可用态来自当前打开来源，ApplyCalibrationAvailability 后还会统一刷新分类入口。
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
        // m_pages 为空说明当前 SettingsUI widget 尚未创建或已经销毁。
        return;
    }
    if (pageIndex >= 0 && pageIndex < m_pages->count()) {
        // QStackedWidget 只显示当前 index 对应页面，其它内部页保留但不绘制。
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
        // 没有页面容器时无法嵌入校准 UI，直接返回避免空指针。
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
        // CreateWidget 的 parent 传 m_pages，让校准页面生命周期挂到设置页容器下。
        calibrationWidget = m_calibration3D ? m_calibration3D->CreateWidget(m_pages) : nullptr;
    } else if (actionId == SettingsActionOpenColorCalibration) {
        title = tr("Color Calibration");
        calibrationWidget = m_calibrationColor ? m_calibrationColor->CreateWidget(m_pages) : nullptr;
    }

    auto* wrapper = new QWidget(m_pages);
    auto* layout = new QVBoxLayout(wrapper);
    auto* back = new QPushButton(tr("Back to Calibration Settings"), wrapper);
    QObject::connect(back, &QPushButton::clicked, [this, wrapper]() {
        // 先切回校准总览，再延迟删除 wrapper。
        // deleteLater 避免在按钮 clicked 调用栈中立即销毁按钮自己的父对象。
        RestoreSettingsOverview();
        wrapper->deleteLater();
    });
    layout->addWidget(back, 0, Qt::AlignLeft);
    if (calibrationWidget) {
        // 校准模块返回的页面作为 wrapper 子层内容，占据剩余空间。
        layout->addWidget(calibrationWidget, 1);
    } else {
        // 加载失败时显示占位，而不是让设置页停留在空白区域。
        layout->addWidget(new QLabel(tr("Calibration module is not available."), wrapper), 1);
    }

    const int index = m_pages->addWidget(wrapper);
    // 动态加入 wrapper 后立即切过去，用户看到的是设置壳中的校准子流程。
    m_pages->setCurrentIndex(index);
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

// 返回设置校准分类页。
void SettingsUIImpl::RestoreSettingsOverview() {
    if (!m_pages) {
        // 设置页已释放时不再操作 QStackedWidget。
        return;
    }
    // PageCalibration 是固定分类页，不是动态校准 wrapper。
    m_pages->setCurrentIndex(PageCalibration);
    if (m_titleLabel) {
        m_titleLabel->setText(tr("Settings"));
    }
}

// 根据打开来源刷新校准入口。
// 当前策略是扫描重建来源直接隐藏左侧 Calibration 分类，并禁用已创建的校准页。
void SettingsUIImpl::ApplyCalibrationAvailability() {
    if (m_calibrationNavButton) {
        // 扫描重建来源不仅禁用按钮，还直接隐藏左侧分类，减少误操作可能。
        m_calibrationNavButton->setVisible(m_allowCalibration);
        m_calibrationNavButton->setEnabled(m_allowCalibration);
    }

    if (m_calibrationPage) {
        // 禁用页面本身可以让页面内已有按钮也无法响应鼠标/键盘输入。
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
        // 返回英文是为了日志机器分析稳定，不受界面语言切换影响。
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
    // C 导出函数返回接口基类指针，调用方不用知道 SettingsUIImpl 的类定义。
    return &SettingsUIImpl::Instance();
}

// 统一版本导出函数。
// 该函数只返回静态版本字符串，不触发设置页面创建或校准模块懒加载。
extern "C" MEYERSCAN_SETTINGSUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
