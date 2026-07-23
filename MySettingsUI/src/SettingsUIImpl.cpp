#include "SettingsUIInternal.h"

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
    // 公共弹窗属于轻量 UI 基础设施，初始化阶段只解析函数地址，不创建任何窗口。
    LoadUIComponents();
    WriteLog(LogLevel::Info, "Init", "SettingsUI initialized");
    return true;
}

// 保存设置动作回调。
void SettingsUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // SettingsUI 不直接依赖 MainWindow 类型，只保存 C ABI 回调和不透明上下文指针。
    m_actionCallback = callback;
    m_actionCallbackContext = context;
}

// 保存宿主提供的校准设备预检回调。
// 回调是同步函数，SettingsUI 不创建线程，也不持有 DeviceCmd/Transport 句柄。
void SettingsUIImpl::SetCalibrationPreflightCallback(
    SettingsCalibrationPreflightCallback callback,
    void* context) {
    m_calibrationPreflightCallback = callback;
    m_calibrationPreflightContext = context;
    WriteLog(LogLevel::Info,
             "SetCalibrationPreflightCallback",
             callback ? "Calibration preflight callback registered"
                      : "Calibration preflight callback cleared");
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
    // 数据快照由宿主拥有。设置页关闭时清除本地副本，防止下次打开误用旧数据。
    m_dataContext = QJsonObject();
    m_actionCallback = nullptr;
    m_actionCallbackContext = nullptr;
    m_calibrationPreflightCallback = nullptr;
    m_calibrationPreflightContext = nullptr;
    // UIComponents 单例由其 DLL 自己管理；SettingsUI 只清空借用的导出函数指针。
    m_showNoticeDialog = nullptr;
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
    // 使用安装目录下的绝对路径，避免第三方启动改变 currentPath 后加载失败或误加载同名 DLL。
    m_loggerLibrary.setFileName(QDir(m_appDir).filePath("MeyerScan_Logger.dll"));
    if (!m_loggerLibrary.load()) {
        // 这里不弹窗。Logger 缺失本身应由 MainExe 启动检查和安装包检查发现。
        return;
    }
    auto loggerApiVersion = reinterpret_cast<int (*)()>(
        m_loggerLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!loggerApiVersion || loggerApiVersion() != 1) {
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
        m_calibration3DLibrary.setFileName(QDir(m_appDir).filePath("MeyerScan_Calibration3DUI.dll"));
        if (m_calibration3DLibrary.load()) {
            auto apiVersion = reinterpret_cast<int (*)()>(
                m_calibration3DLibrary.resolve("GetMeyerModuleApiVersion"));
            if (!apiVersion || apiVersion() != 1) {
                WriteLog(LogLevel::Warning, "LoadCalibrationModules", "Calibration3DUI API version mismatch");
                return;
            }
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
        m_calibrationColorLibrary.setFileName(QDir(m_appDir).filePath("MeyerScan_CalibrationColorUI.dll"));
        if (m_calibrationColorLibrary.load()) {
            auto apiVersion = reinterpret_cast<int (*)()>(
                m_calibrationColorLibrary.resolve("GetMeyerModuleApiVersion"));
            if (!apiVersion ||
                apiVersion() != MEYER_CALIBRATION_COLOR_UI_API_VERSION) {
                WriteLog(LogLevel::Warning, "LoadCalibrationModules", "CalibrationColorUI API version mismatch");
                return;
            }
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

// 保存宿主注入的版本化只读数据上下文。
// 使用“先校验、后替换”的事务式更新，输入无效时保留上一份有效快照。
bool SettingsUIImpl::SetDataContextJson(const char* contextJsonUtf8) {
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
    WriteLog(LogLevel::Info, "SetDataContextJson", "Host data context accepted");
    return true;
}

// 动态加载共享 UI 弹窗导出。
// 弹窗使用独立 C ABI 函数，避免仅为增加弹窗就修改 IUIComponents 的虚函数表布局。
void SettingsUIImpl::LoadUIComponents() {
    if (m_showNoticeDialog || m_appDir.isEmpty()) {
        // 已解析成功或应用目录无效时无需重复 load。
        return;
    }

    // 使用 MainExe 注入的安装目录构造绝对路径，不受第三方启动进程工作目录影响。
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName(
        QDir(m_appDir).filePath("MeyerScan_UIComponents.dll"));
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 QString("UIComponents load failed: %1").arg(m_uiComponentsLibrary.errorString()));
        return;
    }

    // 整数 API 门禁先于任何接口调用，避免误用同名旧 DLL。
    auto apiVersion = reinterpret_cast<int (*)()>(
        m_uiComponentsLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!apiVersion || apiVersion() != 1) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "UIComponents API version mismatch");
        return;
    }

    // 先初始化共享控件单例，使弹窗按钮能使用当前屏幕的辅助缩放系数。
    auto getUIComponents = reinterpret_cast<IUIComponents* (*)()>(
        m_uiComponentsLibrary.resolve("GetUIComponents"));
    IUIComponents* uiComponents = getUIComponents ? getUIComponents() : nullptr;
    const QByteArray appDirBytes = m_appDir.toUtf8();
    if (!uiComponents || !uiComponents->Init(appDirBytes.constData())) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "UIComponents initialization failed");
        return;
    }

    // 只缓存本模块实际使用的单按钮弹窗函数；业务模块不拥有 UIComponents 单例。
    m_showNoticeDialog = reinterpret_cast<MeyerShowNoticeDialogFunc>(
        m_uiComponentsLibrary.resolve("MeyerUIComponents_ShowNoticeDialog"));
    if (!m_showNoticeDialog) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "UIComponents notice-dialog export is unavailable");
        return;
    }
    WriteLog(LogLevel::Info,
             "LoadUIComponents",
             "UIComponents notice dialog loaded");
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

// 返回公共虚接口版本。动态宿主必须在调用 GetSettingsUI 前校验该值。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return MEYER_SETTINGS_UI_API_VERSION;
}

// 统一版本导出函数。
// 该函数只返回静态版本字符串，不触发设置页面创建或校准模块懒加载。
extern "C" MEYERSCAN_SETTINGSUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
