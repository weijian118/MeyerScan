#include "OrderCreateUIInternal.h"

// 触发外部动作回调。
void OrderCreateUIImpl::EmitAction(int actionId) {
    WriteLog(LogLevel::Info, "EmitAction", QString("Action emitted: %1").arg(actionId));
    if (m_actionCallback) {
        // 只向外部传递动作 ID 和调用方上下文，避免 DLL 边界传递 Qt 对象。
        m_actionCallback(m_actionContext, actionId);
    }
}

// 写结构化日志。
void OrderCreateUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }

    // Logger ABI 使用 UTF-8 C 字符串，QString 在调用前转换为 QByteArray。
    const QByteArray bytes = content.toUtf8();
    // 当前建单 UI 尚未拿到设备、病例、操作员上下文，所以这些字段传空字符串，让日志模块省略空字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// 动态加载共享 UI 组件模块。
void OrderCreateUIImpl::LoadUIComponents() {
    if (m_uiComponents) {
        return;
    }

    // UIComponents 是视觉增强依赖，加载失败时必须允许建单页面用本地样式继续运行。
    // 这能避免发布目录漏复制共享 UI DLL 时，建单主流程直接不可用。
    // 使用应用目录下的绝对路径，第三方拉起时即使 currentPath 改变也不会加载错误 DLL。
    const QString appDir = m_appDir.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : QString::fromUtf8(m_appDir);
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName(QDir(appDir).filePath("MeyerScan_UIComponents.dll"));
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local order styles");
        return;
    }

    auto uiApiVersion = reinterpret_cast<int (*)()>(
        m_uiComponentsLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!uiApiVersion || uiApiVersion() != 1) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents API version mismatch");
        return;
    }

    // GetUIComponents 使用 extern "C" 导出，resolve 后转成固定函数指针。
    // 这样 OrderCreateUI 只依赖接口头文件，不依赖 UIComponents 的实现类。
    auto getUIComponents = reinterpret_cast<GetUIComponentsFunc>(m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!getUIComponents) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "GetUIComponents export not found");
        return;
    }

    // 工厂函数返回 UIComponents 内部单例，建单模块只借用，不 delete。
    IUIComponents* loadedUIComponents = getUIComponents();
    if (loadedUIComponents) {
        const char* uiComponentsVersion = loadedUIComponents->GetModuleVersion();
        if (!IsUIComponentsVersionCompatible(uiComponentsVersion)) {
            // 旧版 DLL 虽然能加载，但 vtable 不含新增表格接口；这里主动降级，避免运行时崩溃。
            WriteLog(LogLevel::Warning,
                     "LoadUIComponents",
                     QString("UIComponents version incompatible: %1").arg(QString::fromUtf8(uiComponentsVersion ? uiComponentsVersion : "")));
            m_uiComponents = nullptr;
            return;
        }

        m_uiComponents = loadedUIComponents;
        // Init 需要应用目录以便后续加载统一图标/样式资源。
        // m_appDir 来自 MainExe 的 Init 参数；若独立测试未传，则退回 applicationDirPath。
        const QByteArray appDirBytes = !m_appDir.isEmpty()
            ? m_appDir
            : QCoreApplication::applicationDirPath().toUtf8();
        if (!m_uiComponents->Init(appDirBytes.constData())) {
            // 共享组件初始化失败时主动退回本地 Qt 控件，不能继续使用半初始化接口。
            WriteLog(LogLevel::Warning,
                     "LoadUIComponents",
                     "UIComponents Init returned false; fallback to local order controls");
            m_uiComponents = nullptr;
            return;
        }
        // 弹窗导出从 UIComponents v0.5.0 起提供；解析失败不影响已有控件工厂。
        m_showDecisionDialog = reinterpret_cast<MeyerShowDecisionDialogFunc>(
            m_uiComponentsLibrary.resolve("MeyerUIComponents_ShowDecisionDialog"));
        if (!m_showDecisionDialog) {
            WriteLog(LogLevel::Warning,
                     "LoadUIComponents",
                     "Decision-dialog export unavailable; QMessageBox fallback enabled");
        }
        WriteLog(LogLevel::Info, "LoadUIComponents", "UIComponents loaded for order create UI");
    }
}

// 从 MeyerScan.exe 同级目录加载扫描方案服务。
void OrderCreateUIImpl::LoadScanSchemaService() {
    if (m_scanSchemaService) {
        return;
    }

    // DLL 路径必须从 Init 注入的应用目录推导，不能依赖 current directory 或 PATH。
    const QString appDir = m_appDir.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : QString::fromUtf8(m_appDir);
    const QString libraryPath = QDir(appDir).filePath("MeyerScan_ScanSchemaService.dll");
    m_scanSchemaServiceLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_scanSchemaServiceLibrary.setFileName(libraryPath);
    if (!m_scanSchemaServiceLibrary.load()) {
        WriteLog(LogLevel::Error,
                 "LoadScanSchemaService",
                 QString("ScanSchemaService load failed: %1").arg(m_scanSchemaServiceLibrary.errorString()));
        return;
    }

    // 在取得 C++ 虚接口前先校验 API 版本，防止旧 DLL 的短 vtable 被新 UI 调用。
    typedef int (*GetApiVersionFunc)();
    const GetApiVersionFunc getApiVersion = reinterpret_cast<GetApiVersionFunc>(
        m_scanSchemaServiceLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!getApiVersion || getApiVersion() != MEYER_SCAN_SCHEMA_SERVICE_API_VERSION) {
        WriteLog(LogLevel::Error, "LoadScanSchemaService", "ScanSchemaService API version mismatch");
        return;
    }

    const GetScanSchemaServiceFunc getter = reinterpret_cast<GetScanSchemaServiceFunc>(
        m_scanSchemaServiceLibrary.resolve("GetScanSchemaService"));
    if (!getter) {
        WriteLog(LogLevel::Error, "LoadScanSchemaService", "GetScanSchemaService export not found");
        return;
    }

    m_scanSchemaService = getter();
    if (!m_scanSchemaService || !m_scanSchemaService->Init(m_logDir.constData())) {
        m_scanSchemaService = nullptr;
        WriteLog(LogLevel::Error, "LoadScanSchemaService", "ScanSchemaService initialization failed");
        return;
    }
    WriteLog(LogLevel::Info, "LoadScanSchemaService", "ScanSchemaService loaded");
}

// 创建统一按钮；共享 UI 不可用时返回本地降级按钮。
QPushButton* OrderCreateUIImpl::CreateStandardButton(QWidget* parent, const QString& text, int role) const {
    const QByteArray textBytes = text.toUtf8();
    if (m_uiComponents) {
        // UIComponents 负责普通按钮的统一高度、颜色、边距和 hover/pressed 状态。
        // 建单模块仍负责 objectName、信号连接和业务动作 ID。
        return m_uiComponents->CreateButton(role,
                                            MeyerButtonContentTextOnly,
                                            textBytes.constData(),
                                            "",
                                            parent);
    }

    // 降级路径只创建 Qt 原生按钮并写语义属性，视觉仍由当前页面根 QSS 统一处理。
    auto* button = new QPushButton(text, parent);
    button->setObjectName("OrderCreateFallbackButton");
    if (role == MeyerButtonRolePrimary) {
        button->setProperty("role", "primary");
    } else if (role == MeyerButtonRoleText) {
        button->setProperty("role", "text");
    } else {
        button->setProperty("role", "secondary");
    }
    return button;
}

// 创建统一单行输入框；共享 UI 不可用时返回本地降级输入框。
QLineEdit* OrderCreateUIImpl::CreateStandardLineEdit(QWidget* parent, const QString& text, const QString& placeholder) const {
    const QByteArray placeholderBytes = placeholder.toUtf8();
    QLineEdit* edit = m_uiComponents
        ? m_uiComponents->CreateLineEdit(placeholderBytes.constData(), parent)
        : new QLineEdit(parent);
    if (!m_uiComponents) {
        edit->setObjectName("OrderCreateFallbackLineEdit");
        edit->setProperty("meyerInput", true);
    }

    // 文本值由建单模块设置，UIComponents 只负责输入框基础视觉。
    edit->setText(text);
    return edit;
}

// 创建统一下拉框；共享 UI 不可用时返回本地降级下拉框。
QComboBox* OrderCreateUIImpl::CreateStandardComboBox(QWidget* parent) const {
    QComboBox* combo = m_uiComponents
        ? m_uiComponents->CreateComboBox(parent)
        : new QComboBox(parent);
    if (!m_uiComponents) {
        combo->setObjectName("OrderCreateFallbackComboBox");
        combo->setProperty("meyerInput", true);
    }
    return combo;
}

// 创建统一日期输入框；共享 UI 不可用时返回本地降级日期框。
QDateEdit* OrderCreateUIImpl::CreateStandardDateEdit(QWidget* parent) const {
    QDateEdit* edit = m_uiComponents
        ? m_uiComponents->CreateDateEdit(parent)
        : new QDateEdit(parent);
    if (!m_uiComponents) {
        edit->setCalendarPopup(true);
        edit->setObjectName("OrderCreateFallbackDateEdit");
        edit->setProperty("meyerInput", true);
    }
    return edit;
}

// 创建统一多行文本框；共享 UI 不可用时返回本地降级文本框。
QTextEdit* OrderCreateUIImpl::CreateStandardTextEdit(QWidget* parent, int fixedHeight) const {
    QTextEdit* edit = m_uiComponents
        ? m_uiComponents->CreateTextEdit(parent)
        : new QTextEdit(parent);
    if (!m_uiComponents) {
        edit->setObjectName("OrderCreateFallbackTextEdit");
        edit->setProperty("meyerInput", true);
    }

    // 建单页面需要让备注框在三栏布局内保持稳定高度，避免输入内容改变时挤压牙位区。
    if (fixedHeight > 0) {
        edit->setFixedHeight(fixedHeight);
    }
    return edit;
}

// 创建统一字段标签；共享 UI 不可用时返回本地降级标签。
QLabel* OrderCreateUIImpl::CreateStandardFieldLabel(QWidget* parent, const QString& text) const {
    const QByteArray textBytes = text.toUtf8();
    QLabel* label = m_uiComponents
        ? m_uiComponents->CreateFieldLabel(textBytes.constData(), parent)
        : new QLabel(text, parent);
    if (!m_uiComponents) {
        label->setObjectName("OrderCreateFallbackFieldLabel");
        label->setProperty("fieldLabel", true);
    }

    // 字段标签允许换行，多语言翻译变长时优先扩展高度，而不是挤压相邻控件。
    label->setWordWrap(true);
    return label;
}

// 创建统一表格；共享 UI 不可用时返回本地降级表格。
QTableWidget* OrderCreateUIImpl::CreateStandardTableWidget(QWidget* parent) const {
    QTableWidget* table = m_uiComponents
        ? m_uiComponents->CreateTableWidget(parent)
        : new QTableWidget(parent);
    if (!m_uiComponents) {
        // 本地降级样式只在 UIComponents 缺失时使用，正常发布应由共享 UI 统一表格视觉。
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setShowGrid(false);
        table->setObjectName("OrderCreateFallbackTable");
        table->setProperty("meyerTable", true);
    }
    return table;
}
