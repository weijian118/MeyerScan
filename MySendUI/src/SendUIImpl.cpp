#include "SendUIImpl.h"

#include "MeyerQtModuleUtils.h"
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 日志模块字段使用稳定英文，不受界面语言切换影响。
const char* Name = "MeyerScan_SendUI";

// 代码版本必须和 CMakeLists.txt、Version.rc 同步递增。
const char* Version = "MeyerScan_SendUI v0.1.3 (2026-07-15)";
}
}

// 返回 DLL 内部单例。
// C++11 保证函数内静态对象只初始化一次，因此不需要额外全局锁。
SendUIImpl& SendUIImpl::Instance() {
    static SendUIImpl instance;
    return instance;
}

// 初始化路径、Logger 和共享 UI 组件。
bool SendUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // QByteArray 立即复制调用方缓冲区，调用返回后原始 const char* 可以安全失效。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 应用目录为空时使用 Qt 记录的 EXE 目录；不能使用可能被第三方启动器改变的 currentPath。
    if (m_appDir.isEmpty()) {
        m_appDir = QCoreApplication::applicationDirPath().toUtf8();
    }

    // Logger 接口只获取一次并缓存。Init 是幂等操作，统一日志目录由 MainExe 传入。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        if (!m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
            // 日志失败不能阻断发送页，但后续必须停止使用半初始化 Logger。
            m_logger = nullptr;
        }
    }

    // UIComponents 是可降级依赖，加载失败不阻断发送页框架显示。
    LoadUIComponents();
    WriteLog(LogLevel::Info, "Init", "Send UI initialized");
    return true;
}

// 创建发送页根控件和两段内容区。
QWidget* SendUIImpl::CreateWidget(QWidget* parent) {
    // 返回 QWidget 由 parent 的 Qt 父子树持有，模块只保存非 owning 指针供上下文刷新使用。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanSendUIRoot");
    root->setMinimumSize(920, 560);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // QSS 统一从资源 DLL/模块 Resources 加载，业务源码不直接调用 setStyleSheet。
    MeyerQtModule::ApplyModuleQss(root, "MySendUI", "send.qss", m_logger);

    // 外层布局用上下 stretch 将有限宽度内容垂直居中，窗口放大时不会把表单控件拉得过高。
    auto* outerLayout = new QVBoxLayout(root);
    outerLayout->setContentsMargins(22, 20, 22, 22);
    outerLayout->setSpacing(0);

    // 单个 card 只是发送工具的真实内容容器，不在页面中继续嵌套装饰性卡片。
    auto* card = new QFrame(root);
    card->setObjectName("SendUICard");
    card->setProperty("card", true);
    card->setMaximumWidth(900);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(38, 28, 38, 30);
    cardLayout->setSpacing(22);
    cardLayout->addWidget(CreateCaseInfoSection(card));
    cardLayout->addWidget(CreateSendActionSection(card));

    outerLayout->addStretch(1);
    outerLayout->addWidget(card, 0, Qt::AlignHCenter);
    outerLayout->addStretch(1);

    // 先保存根指针，再把缓存上下文投影到控件；SetSessionContextJson 可以在 CreateWidget 前调用。
    m_root = root;
    ApplyContextToWidgets();
    WriteLog(LogLevel::Info, "CreateWidget", "Send UI widget created");
    return root;
}

// 校验并缓存会话 JSON。
bool SendUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    // 先在局部变量解析。只有全部校验通过才替换成员，失败时界面与内部状态都保留上一份有效值。
    const QByteArray candidateJson(contextJsonUtf8 ? contextJsonUtf8 : "");
    QJsonObject candidateObject;

    if (!candidateJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(candidateJson, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            WriteLog(LogLevel::Warning,
                     "SetSessionContextJson",
                     QString("Invalid context json: %1").arg(parseError.errorString()));
            return false;
        }
        candidateObject = document.object();
    }

    // 空字符串是合法的“清空上下文”；此时 candidateObject 为空，界面字段会被清空。
    m_contextJson = candidateJson;
    m_contextObject = candidateObject;
    ApplyContextToWidgets();
    WriteLog(LogLevel::Info,
             "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

// 保存动作回调。
void SendUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    m_actionCallback = callback;
    m_actionContext = context;
}

// 返回静态代码版本字符串。
const char* SendUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 清理模块缓存和 QWidget 弱引用。
void SendUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Send UI shutdown");
    // Flush 当前为兼容接口；保留调用可表达“退出前完成日志”的生命周期意图。
    if (m_logger) {
        m_logger->Flush();
    }
    // 不 delete m_root。根控件由工作台父子树负责销毁，跨 DLL 重复 delete 会造成崩溃。
    m_root = nullptr;
    m_nameEdit = nullptr;
    m_doctorEdit = nullptr;
    m_orderNoEdit = nullptr;
    m_orderTypeEdit = nullptr;
    m_clinicEdit = nullptr;
    m_dataFormatCombo = nullptr;
    m_noteEdit = nullptr;
    m_contextJson.clear();
    m_contextObject = QJsonObject();
    // QLibrary 设置了 PreventUnloadHint，不主动卸载；这里只清接口指针，避免退出阶段静态析构顺序风险。
    m_uiComponents = nullptr;
    m_logger = nullptr;
    m_actionCallback = nullptr;
    m_actionContext = nullptr;
}

// 运行时加载共享 UI 组件工厂。
void SendUIImpl::LoadUIComponents() {
    if (m_uiComponents) {
        return;
    }

    // 使用应用目录构造绝对 DLL 路径，第三方启动器修改工作目录不会改变加载目标。
    const QString appDir = QString::fromUtf8(m_appDir);
    const QString libraryPath = QDir(appDir).filePath("MeyerScan_UIComponents.dll");
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName(libraryPath);
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 QString("UIComponents unavailable; fallback to Qt controls: %1")
                     .arg(m_uiComponentsLibrary.errorString()));
        return;
    }

    auto uiApiVersion = reinterpret_cast<int (*)()>(
        m_uiComponentsLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!uiApiVersion || uiApiVersion() != 1) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents API version mismatch");
        return;
    }

    // resolve 返回无类型函数地址；reinterpret_cast 只用于已约定的 extern "C" 工厂签名。
    auto getUIComponents = reinterpret_cast<GetUIComponentsFunc>(m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!getUIComponents) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "GetUIComponents export not found");
        return;
    }

    // 工厂对象由 UIComponents DLL 持有；PreventUnloadHint 使该裸指针在进程生命周期内保持有效。
    m_uiComponents = getUIComponents();
    if (m_uiComponents) {
        const QByteArray appDirBytes = m_appDir.isEmpty()
            ? QCoreApplication::applicationDirPath().toUtf8()
            : m_appDir;
        if (!m_uiComponents->Init(appDirBytes.constData())) {
            // 半初始化共享组件不能继续创建控件，主动清空接口并使用本地 Qt 降级路径。
            WriteLog(LogLevel::Warning,
                     "LoadUIComponents",
                     "UIComponents Init returned false; fallback to Qt controls");
            m_uiComponents = nullptr;
        }
    }
}

// 创建病例信息区。
QWidget* SendUIImpl::CreateCaseInfoSection(QWidget* parent) {
    // section 的 parent 指向 card，返回后无需调用方手工释放。
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // 动态属性只提供给 QSS 选择器，不在 C++ 中硬编码字体和颜色。
    auto* title = new QLabel(tr("Case Information"), section);
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    auto* divider = new QFrame(section);
    divider->setProperty("divider", true);
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(14);

    // 这些控件只读展示标准上下文；稳定 objectName 供自动测试和后续 UI 自动化定位。
    m_nameEdit = CreateReadOnlyLineEdit(section);
    m_nameEdit->setObjectName("SendUIPatientNameEdit");
    m_doctorEdit = CreateReadOnlyLineEdit(section);
    m_doctorEdit->setObjectName("SendUIDoctorEdit");
    m_orderNoEdit = CreateReadOnlyLineEdit(section);
    m_orderNoEdit->setObjectName("SendUIOrderNoEdit");
    m_orderTypeEdit = CreateReadOnlyLineEdit(section);
    m_orderTypeEdit->setObjectName("SendUIOrderTypeEdit");
    m_clinicEdit = CreateReadOnlyLineEdit(section);
    m_clinicEdit->setObjectName("SendUIClinicEdit");
    m_dataFormatCombo = m_uiComponents ? m_uiComponents->CreateComboBox(section) : new QComboBox(section);
    m_dataFormatCombo->setObjectName("SendUIDataFormatCombo");
    m_dataFormatCombo->addItem(tr("STL"));
    m_dataFormatCombo->addItem(tr("PLY"));
    m_dataFormatCombo->addItem(tr("OBJ"));
    QObject::connect(m_dataFormatCombo,
                     static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     [this](int index) {
        if (index >= 0) {
            // 只上报格式变化意图；真实导出服务在宿主收到动作后读取/保存选择。
            EmitAction(SendUIActionDataFormatChanged, "DataFormatChanged");
        }
    });

    grid->addWidget(CreateFieldLabel(tr("Name"), section), 0, 0);
    grid->addWidget(CreateFieldLabel(tr("Doctor"), section), 0, 1);
    grid->addWidget(m_nameEdit, 1, 0);
    grid->addWidget(m_doctorEdit, 1, 1);
    grid->addWidget(CreateFieldLabel(tr("Order No."), section), 2, 0);
    grid->addWidget(CreateFieldLabel(tr("Order Type"), section), 2, 1);
    grid->addWidget(m_orderNoEdit, 3, 0);
    grid->addWidget(m_orderTypeEdit, 3, 1);
    grid->addWidget(CreateFieldLabel(tr("Clinic"), section), 4, 0);
    grid->addWidget(CreateFieldLabel(tr("Data Format"), section), 4, 1);
    grid->addWidget(m_clinicEdit, 5, 0);
    grid->addWidget(m_dataFormatCombo, 5, 1);
    grid->addWidget(CreateFieldLabel(tr("Note"), section), 6, 0, 1, 2);

    // Note 是发送阶段可编辑的轻量字段，最终保存仍由后续 Service/Workflow 执行。
    m_noteEdit = m_uiComponents ? m_uiComponents->CreateTextEdit(section) : new QTextEdit(section);
    m_noteEdit->setObjectName("SendUINoteEdit");
    m_noteEdit->setMinimumHeight(78);
    grid->addWidget(m_noteEdit, 7, 0, 1, 2);

    layout->addLayout(grid);
    return section;
}

// 创建发送动作区。
QWidget* SendUIImpl::CreateSendActionSection(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* title = new QLabel(tr("Send"), section);
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    auto* divider = new QFrame(section);
    divider->setProperty("divider", true);
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    auto* actionGrid = new QGridLayout();
    actionGrid->setContentsMargins(0, 0, 0, 0);
    actionGrid->setHorizontalSpacing(16);
    actionGrid->setVerticalSpacing(12);

    auto* localLabel = CreateFieldLabel(tr("Local"), section);
    auto* labLabel = CreateFieldLabel(tr("Lab"), section);
    // 每个按钮设置稳定 objectName，测试和自动化不依赖翻译后的按钮文字。
    auto* exportButton = CreateActionButton(tr("Export"), MeyerButtonRoleSecondary, section);
    exportButton->setObjectName("SendUIExportButton");
    auto* compressButton = CreateActionButton(tr("Compress"), MeyerButtonRoleSecondary, section);
    compressButton->setObjectName("SendUICompressButton");
    auto* emailButton = CreateActionButton(tr("Email Send"), MeyerButtonRoleSecondary, section);
    emailButton->setObjectName("SendUIEmailButton");
    auto* uploadButton = CreateActionButton(tr("Upload"), MeyerButtonRoleSecondary, section);
    uploadButton->setObjectName("SendUIUploadButton");

    // lambda 只捕获 this 并上报稳定 actionId，不在 UI 线程中执行文件或网络重任务。
    QObject::connect(exportButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionExport, "Export");
    });
    QObject::connect(compressButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionCompress, "Compress");
    });
    QObject::connect(emailButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionEmailSend, "EmailSend");
    });
    QObject::connect(uploadButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionUpload, "Upload");
    });

    actionGrid->addWidget(localLabel, 0, 0);
    actionGrid->addWidget(exportButton, 1, 0);
    actionGrid->addWidget(compressButton, 1, 1);
    actionGrid->addWidget(labLabel, 2, 0);
    actionGrid->addWidget(emailButton, 3, 0);
    actionGrid->addWidget(uploadButton, 3, 1);
    actionGrid->setColumnStretch(0, 1);
    actionGrid->setColumnStretch(1, 1);
    layout->addLayout(actionGrid);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 10, 0, 0);
    bottomLayout->setSpacing(12);
    auto* previousButton = CreateActionButton(tr("Previous"), MeyerButtonRoleSecondary, section);
    previousButton->setObjectName("SendUIPreviousButton");
    auto* finishButton = CreateActionButton(tr("Finish"), MeyerButtonRolePrimary, section);
    finishButton->setObjectName("SendUIFinishButton");
    finishButton->setProperty("primary", true);
    finishButton->setMinimumWidth(190);

    QObject::connect(previousButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionPrevious, "Previous");
    });
    QObject::connect(finishButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionFinish, "Finish");
    });

    bottomLayout->addWidget(previousButton, 0, Qt::AlignLeft);
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(finishButton, 0, Qt::AlignRight);
    layout->addLayout(bottomLayout);
    return section;
}

// 创建只读输入框。
QLineEdit* SendUIImpl::CreateReadOnlyLineEdit(QWidget* parent) {
    // UIComponents 返回的 QWidget 同样以 parent 管理；降级路径保持相同父子所有权。
    auto* edit = m_uiComponents ? m_uiComponents->CreateLineEdit("", parent) : new QLineEdit(parent);
    edit->setReadOnly(true);
    edit->setProperty("readonly", true);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return edit;
}

// 创建字段标签。
QLabel* SendUIImpl::CreateFieldLabel(const QString& text, QWidget* parent) {
    // QByteArray 必须作为局部变量活到跨 DLL 调用完成，不能保存 text.toUtf8().constData() 临时指针。
    const QByteArray textBytes = text.toUtf8();
    QLabel* label = m_uiComponents
        ? m_uiComponents->CreateFieldLabel(textBytes.constData(), parent)
        : new QLabel(text, parent);
    label->setProperty("fieldLabel", true);
    return label;
}

// 创建动作按钮。
QPushButton* SendUIImpl::CreateActionButton(const QString& text, int role, QWidget* parent) {
    // role/content 由 UIComponents 统一映射到 QSS，SendUI 只决定文案和点击动作。
    const QByteArray textBytes = text.toUtf8();
    QPushButton* button = m_uiComponents
        ? m_uiComponents->CreateButton(role, MeyerButtonContentTextOnly, textBytes.constData(), "", parent)
        : new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

// 把当前上下文字段刷新到页面控件。
void SendUIImpl::ApplyContextToWidgets() {
    if (!m_root) {
        return;
    }

    // 标准上下文按 patient/order/clinic 分组；未知字段继续保留在原 JSON 中但本页不解释。
    const QJsonObject patient = m_contextObject.value("patient").toObject();
    const QJsonObject order = m_contextObject.value("order").toObject();
    const QJsonObject clinic = m_contextObject.value("clinic").toObject();

    if (m_nameEdit) {
        // 缺少真实数据时显示空字段，禁止把测试患者信息带入生产界面。
        m_nameEdit->setText(ReadString(patient, "name"));
    }
    if (m_doctorEdit) {
        m_doctorEdit->setText(ReadString(order, "doctor"));
    }
    if (m_orderNoEdit) {
        m_orderNoEdit->setText(ReadString(order, "orderId", ReadString(order, "id")));
    }
    if (m_orderTypeEdit) {
        m_orderTypeEdit->setText(ReadString(order, "caseType", ReadString(order, "type")));
    }
    if (m_clinicEdit) {
        m_clinicEdit->setText(ReadString(clinic, "name", ReadString(order, "clinic")));
    }
    if (m_dataFormatCombo) {
        // QSignalBlocker 使用 RAII 临时阻断 currentIndexChanged，程序填充上下文不能伪装成客户操作日志。
        const QSignalBlocker blocker(m_dataFormatCombo);
        const QString format = ReadString(order, "dataFormat", "STL");
        const int index = m_dataFormatCombo->findText(format, Qt::MatchFixedString);
        if (index >= 0) {
            m_dataFormatCombo->setCurrentIndex(index);
        }
    }
    if (m_noteEdit) {
        m_noteEdit->setPlainText(ReadString(order, "note", QString()));
    }
}

// 读取一个允许字符串/数字的轻量字段。
QString SendUIImpl::ReadString(const QJsonObject& object, const char* key, const QString& fallback) const {
    if (!key || !key[0]) {
        return fallback;
    }
    const QJsonValue value = object.value(QString::fromLatin1(key));
    if (value.isString()) {
        const QString text = value.toString().trimmed();
        return text.isEmpty() ? fallback : text;
    }
    // JSON 数值用于兼容纯数字订单号；固定 0 位小数避免显示科学计数法或无意义小数。
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'f', 0);
    }
    return fallback;
}

// 写日志并上报动作。
void SendUIImpl::EmitAction(int actionId, const QString& operation) {
    // 显式保存 UTF-8 缓冲区，保证 constData 在 WriteLog 整个调用期间有效。
    const QByteArray operationBytes = operation.toUtf8();
    WriteLog(LogLevel::Info, operationBytes.constData(), QString("Send UI action: %1").arg(actionId));
    if (m_actionCallback) {
        // 回调只跨越稳定 int 和宿主原样传入的 void*，不暴露 Qt/C++ 容器 ABI。
        m_actionCallback(m_actionContext, actionId);
    }
}

// 写结构化日志。
void SendUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 供 QLibrary::resolve 获取的纯 C 工厂入口。
extern "C" MEYERSCAN_SENDUI_API ISendUI* GetSendUI() {
    return &SendUIImpl::Instance();
}

// 供运行时 versionList 读取的统一代码版本入口。
extern "C" MEYERSCAN_SENDUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}

// 返回发送界面公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return 1;
}
