#include "OrderCreateUIImpl.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与工程里的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_OrderCreateUI";

// 模块版本用于 GetModuleVersion()，需要和 Version.rc 同步维护。
const char* Version = "MeyerScan_OrderCreateUI v0.2.2 (2026-07-07)";
}

// 建单界面仍保留少量业务专属颜色，例如牙位选中态和页面容器色。
// 通用按钮、输入框和下拉框优先交给 MyUIComponents，避免每个 UI 模块各写一套基础控件样式。
const char* kPrimaryColor = "#007d68";
const char* kPrimaryHoverColor = "#009176";
const char* kBorderColor = "#d8e1e7";
const char* kPanelBackground = "#ffffff";
const char* kPageBackground = "#f3f6f8";
const char* kTextColor = "#23313f";
const char* kSecondaryTextColor = "#607080";
const char* kFieldBackground = "#fbfcfd";

// 从类似 "MeyerScan_UIComponents v0.4.0 (2026-07-05)" 的版本字符串中读取主/次/补丁号。
// 动态加载 DLL 时必须先做版本判断，因为 C++ 虚接口新增方法后，旧 DLL 的 vtable 不包含新槽位。
bool ReadVersionTriplet(const QString& text, int* major, int* minor, int* patch) {
    const int marker = text.indexOf('v');
    if (marker < 0) {
        return false;
    }

    QString versionPart;
    for (int i = marker + 1; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch.isDigit() || ch == '.') {
            versionPart.append(ch);
        } else {
            break;
        }
    }

    const QStringList parts = versionPart.split('.');
    if (parts.size() < 2) {
        return false;
    }

    bool okMajor = false;
    bool okMinor = false;
    bool okPatch = true;
    const int parsedMajor = parts.at(0).toInt(&okMajor);
    const int parsedMinor = parts.at(1).toInt(&okMinor);
    int parsedPatch = 0;
    if (parts.size() >= 3) {
        parsedPatch = parts.at(2).toInt(&okPatch);
    }
    if (!okMajor || !okMinor || !okPatch) {
        return false;
    }

    if (major) {
        *major = parsedMajor;
    }
    if (minor) {
        *minor = parsedMinor;
    }
    if (patch) {
        *patch = parsedPatch;
    }
    return true;
}

// 判断运行时 UIComponents 是否满足当前建单模块需要的 ABI。
// 当前建单模块会调用表格工厂，该接口从 UIComponents v0.4.0 开始提供。
bool IsUIComponentsVersionCompatible(const char* versionUtf8) {
    int major = 0;
    int minor = 0;
    int patch = 0;
    if (!ReadVersionTriplet(QString::fromUtf8(versionUtf8 ? versionUtf8 : ""), &major, &minor, &patch)) {
        return false;
    }

    if (major > 0) {
        return true;
    }
    if (major == 0 && minor > 4) {
        return true;
    }
    return major == 0 && minor == 4 && patch >= 0;
}

}

// 返回建单模块单例。
OrderCreateUIImpl& OrderCreateUIImpl::Instance() {
    // 单例保存当前界面临时状态，MainExe 多次获取接口仍然操作同一份 UI 状态。
    static OrderCreateUIImpl instance;
    return instance;
}

// 初始化建单模块。
bool OrderCreateUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // QByteArray 会复制调用方传入的 UTF-8 字符串，避免外部临时变量销毁后指针失效。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 初始化时缓存日志接口。后续所有日志都通过 m_logger 输出，符合“每模块一份日志变量”的规则。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    // UIComponents 只负责通用控件的视觉统一；建单字段、牙位状态和动作回调仍留在本模块。
    LoadUIComponents();

    // 初版默认选择全冠，让界面打开后有一个明确的操作上下文。
    m_currentTypeCode = "crown";
    // 清空上次可能残留的牙位状态，保证每次 Init 都从干净状态开始。
    m_selectedTeeth.clear();
    // 牙位类型映射也必须同步清空，否则旧类型会污染新建单页面。
    m_toothTypeCodes.clear();
    // 初始化不清空 m_pendingContextJson，因为第三方流程可能先调用 SetOrderContextJson，
    // 再调用 Init/CreateWidget；保留上下文可以让调用顺序更宽容。
    WriteLog(LogLevel::Info, "Init", "Order create UI initialized");
    return true;
}

// 创建建单根界面。
QWidget* OrderCreateUIImpl::CreateWidget(QWidget* parent) {
    // 每次 CreateWidget 都创建新的 QWidget，由调用方决定嵌入到哪个容器。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanOrderCreateUIRoot");
    // 根界面不再按 1920x1080 方案写死大尺寸。
    // 工作台壳负责全屏显示，本页只给出可用下限，低分辨率时由滚动区和布局共同适配。
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 根布局使用三栏：左基本信息，中间牙位规划，右订单摘要。
    auto* pageLayout = new QVBoxLayout(root);
    pageLayout->setContentsMargins(12, 10, 12, 12);
    pageLayout->setSpacing(10);

    // 样式只限定在本模块根对象下，避免污染其它模块的控件外观。
    root->setStyleSheet(QString(
        "#MeyerScanOrderCreateUIRoot{background:%1;}"
        "QGroupBox{background:%2;border:1px solid %3;border-radius:6px;margin-top:16px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:16px;padding:0 6px;color:%4;font-size:14px;font-weight:600;}"
        "QLabel#OrderCreatePageTitle{color:%4;font-size:24px;font-weight:700;}"
        "QLabel#OrderCreatePageSubtitle{color:%5;font-size:13px;}"
        "QLabel{color:%5;font-size:13px;}"
        "QLabel[valueText=\"true\"]{color:%4;font-weight:600;}"
        "QScrollArea{background:transparent;border:0;}"
        "QRadioButton{color:%4;font-size:13px;spacing:6px;}"
        "QCheckBox{color:%4;font-size:13px;spacing:6px;}"
        "QPushButton[typeButton=\"true\"],QPushButton[toothButton=\"true\"]{border:1px solid #cfd8dc;border-radius:4px;background:#f6f8fa;color:%4;min-height:32px;padding:6px 9px;}"
        "QPushButton[typeButton=\"true\"]:hover,QPushButton[toothButton=\"true\"]:hover{background:#edf2f5;border-color:#b7c5ce;}"
        "QPushButton[typeButton=\"true\"]:pressed,QPushButton[toothButton=\"true\"]:pressed{background:#e1e8ec;}"
        "QPushButton[typeSelected=\"true\"],QPushButton[toothSelected=\"true\"]{background:%6;border-color:%6;color:#ffffff;font-weight:600;}"
        "QPushButton[typeSelected=\"true\"]:hover,QPushButton[toothSelected=\"true\"]:hover{background:%7;border-color:%7;}"
        "QPushButton[toothButton=\"true\"]{font-weight:600;min-height:34px;}"
    ).arg(kPageBackground,
          kPanelBackground,
          kBorderColor,
          kTextColor,
          kSecondaryTextColor,
          kPrimaryColor,
          kPrimaryHoverColor));

    // 顶部标题区只说明当前工作台上下文，不承载可点击业务动作。
    // 这让客户进入第三方建单时能立即知道自己处在“建单工作台”，而不是普通弹窗。
    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = m_uiComponents
        ? m_uiComponents->CreatePageTitle(tr("Order Creation").toUtf8().constData(), root)
        : new QLabel(tr("Order Creation"), root);
    titleLabel->setObjectName("OrderCreatePageTitle");
    if (!m_uiComponents) {
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(20);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
    }
    auto* subtitleLabel = new QLabel(tr("Complete patient details, scan plan, and order confirmation in one workspace."), root);
    subtitleLabel->setObjectName("OrderCreatePageSubtitle");
    subtitleLabel->setWordWrap(true);

    auto* titleBox = new QVBoxLayout();
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(3);
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(subtitleLabel);
    headerLayout->addLayout(titleBox, 1);
    pageLayout->addLayout(headerLayout);

    auto* rootLayout = new QHBoxLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);
    pageLayout->addLayout(rootLayout, 1);

    // 左侧信息栏固定一个舒适宽度，文本较长时由输入框内部显示，不挤压牙位区。
    QWidget* basicPanel = CreateBasicInfoPanel(root);
    basicPanel->setMinimumWidth(300);
    basicPanel->setMaximumWidth(410);
    rootLayout->addWidget(basicPanel, 0);

    // 中间牙位区占用最大空间，保证多显示器/高分辨率下牙位按钮仍有良好点击区域。
    QWidget* toothPanel = CreateToothPlanPanel(root);
    rootLayout->addWidget(toothPanel, 1);

    // 右侧摘要区宽度和左侧相近，便于用户边选牙位边确认明细。
    QWidget* summaryPanel = CreateOrderSummaryPanel(root);
    summaryPanel->setMinimumWidth(300);
    summaryPanel->setMaximumWidth(410);
    rootLayout->addWidget(summaryPanel, 0);

    // 保存弱引用，不接管 root 生命周期，真实释放由 Qt 父子关系或调用方完成。
    m_root = root;

    if (m_hasPendingContext && !m_pendingContextJson.isEmpty()) {
        // 如果 MainExe 或第三方流程在页面创建前已经传入标准上下文，
        // 页面创建完成后立即应用，避免客户看到默认数据再跳变成第三方数据。
        const QByteArray pendingContextCopy = m_pendingContextJson;
        SetOrderContextJson(pendingContextCopy.constData());
    } else {
        // 没有外部上下文时才使用截图中的示例牙位，
        // 便于人工双击测试宿主时马上看到右侧明细联动。
        m_selectedTeeth.insert(15);
        m_selectedTeeth.insert(16);
        m_selectedTeeth.insert(47);
        m_toothTypeCodes.insert(15, "crown");
        m_toothTypeCodes.insert(16, "crown");
        m_toothTypeCodes.insert(47, "implant");
        RefreshSelectionTable();
        RefreshBasicSummary();
        for (auto it = m_selectedTeeth.begin(); it != m_selectedTeeth.end(); ++it) {
            UpdateToothButtonState(*it);
        }
    }

    WriteLog(LogLevel::Info, "CreateWidget", "Order create widget created");
    return root;
}

// 设置外部动作回调。
void OrderCreateUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    // 保存函数指针和上下文，Qt 按钮点击后只向外部传动作 ID。
    m_actionCallback = callback;
    m_actionContext = context;
    WriteLog(LogLevel::Info, "SetActionCallback", "Action callback updated");
}

// 接收标准建单上下文 JSON。
// JSON 是跨第三方、HIS、手工建单的统一载体，避免 UI 认识每个外部系统的私有字段。
bool OrderCreateUIImpl::SetOrderContextJson(const char* contextJsonUtf8) {
    if (!contextJsonUtf8 || !contextJsonUtf8[0]) {
        // 空上下文不能猜测，保留当前界面状态并返回失败。
        WriteLog(LogLevel::Warning, "SetOrderContextJson", "Empty order context json");
        return false;
    }

    // 保存一份原始 UTF-8 文本，保证“先设置上下文、后创建界面”的调用顺序可用。
    m_pendingContextJson = QByteArray(contextJsonUtf8);
    m_hasPendingContext = true;

    QJsonParseError parseError;
    // Qt JSON 解析器直接消费 UTF-8 QByteArray。
    // 返回的 QJsonDocument 拥有自己的内部数据，不依赖 contextJsonUtf8 指针生命周期。
    const QJsonDocument document = QJsonDocument::fromJson(m_pendingContextJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteLog(LogLevel::Warning,
                 "SetOrderContextJson",
                 QString("Invalid context json: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject rootObject = document.object();
    const QJsonObject sourceObject = rootObject.value("source").toObject();
    const QJsonObject patientObject = rootObject.value("patient").toObject();
    const QJsonObject orderObject = rootObject.value("order").toObject();
    const QJsonObject scanPlanObject = rootObject.value("scanPlan").toObject();

    // source 记录第三方类型和来源系统，便于后续多个第三方同时接入时排查字段映射。
    const QString launchType = ReadString(sourceObject, "launchType", "manual");
    const QString thirdPartyType = ReadString(sourceObject, "thirdPartyType", "manual");
    const QString sourceSystem = ReadString(sourceObject, "sourceSystem");
    m_sourceSummary = QString("%1 / %2").arg(launchType, thirdPartyType);
    if (!sourceSystem.isEmpty()) {
        m_sourceSummary += QString(" / %1").arg(sourceSystem);
    }

    if (m_patientIdEdit) {
        // 患者编号可能来自第三方，也可能由后续服务生成；当前只显示，不作为数据库主键保存。
        m_patientIdEdit->setText(ReadString(patientObject, "patientId", m_patientIdEdit->text()));
    }
    if (m_patientNameEdit) {
        m_patientNameEdit->setText(ReadString(patientObject, "name", m_patientNameEdit->text()));
    }
    if (m_ageEdit) {
        m_ageEdit->setText(ReadIntText(patientObject, "age", m_ageEdit->text()));
    }
    if (m_birthDateEdit) {
        const QString birthDateText = ReadString(patientObject, "birthDate");
        const QDate parsedDate = QDate::fromString(birthDateText, "yyyy/MM/dd");
        if (parsedDate.isValid()) {
            // QDateEdit 只接收 QDate；第三方传错格式时不强制清空，避免覆盖已有人工输入。
            m_birthDateEdit->setDate(parsedDate);
        }
    }
    SetGender(ReadString(patientObject, "gender", "unknown"));
    if (m_contactEdit) {
        m_contactEdit->setText(ReadString(patientObject, "contact", m_contactEdit->text()));
    }
    if (m_patientNoteEdit) {
        m_patientNoteEdit->setPlainText(ReadString(patientObject, "note", m_patientNoteEdit->toPlainText()));
    }

    if (m_orderIdEdit) {
        m_orderIdEdit->setText(ReadString(orderObject, "orderId", m_orderIdEdit->text()));
    }
    if (m_doctorCombo) {
        SetComboText(m_doctorCombo, ReadString(orderObject, "doctor", m_doctorCombo->currentText()));
    }
    if (m_labCombo) {
        SetComboText(m_labCombo, ReadString(orderObject, "lab", m_labCombo->currentText()));
    }
    if (m_deliveryDateEdit) {
        const QString deliveryDateText = ReadString(orderObject, "deliveryDate");
        const QDate parsedDate = QDate::fromString(deliveryDateText, "yyyy/MM/dd");
        if (parsedDate.isValid()) {
            m_deliveryDateEdit->setDate(parsedDate);
        }
    }
    if (m_orderNoteEdit) {
        m_orderNoteEdit->setPlainText(ReadString(orderObject, "note", m_orderNoteEdit->toPlainText()));
    }

    // 扫描方案中的 items 会覆盖默认牙位示例。
    // 如果 items 为空，保留当前牙位状态，方便第三方只传患者/订单基础信息。
    ApplyScanPlanItems(scanPlanObject);
    RefreshSelectionTable();
    RefreshBasicSummary();

    WriteLog(LogLevel::Info,
             "SetOrderContextJson",
             QString("Context applied, thirdPartyType=%1").arg(thirdPartyType));
    return true;
}

// 返回模块版本字符串。
const char* OrderCreateUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭模块并清理状态。
void OrderCreateUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Order create UI shutdown");
    if (m_logger) {
        // Logger 当前逐条写入并关闭句柄，Flush 是兼容接口；保留调用便于未来调整实现。
        m_logger->Flush();
    }

    // 清空弱引用和临时状态。这里不 delete QWidget，避免和 Qt 父子树双重释放。
    m_root = nullptr;
    m_actionCallback = nullptr;
    m_actionContext = nullptr;
    m_currentTypeCode.clear();
    m_selectedTeeth.clear();
    m_toothTypeCodes.clear();
    m_toothButtons.clear();
    m_typeButtons.clear();
    ResetWidgetReferences();
    m_pendingContextJson.clear();
    m_hasPendingContext = false;
    m_sourceSummary.clear();
    m_uiComponents = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 创建左侧患者和订单基本信息区。
QWidget* OrderCreateUIImpl::CreateBasicInfoPanel(QWidget* parent) {
    auto* outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(12, 14, 12, 12);
    outerLayout->setSpacing(8);

    // 患者编号由上层建单流程生成，初版设为只读，避免用户误改主键类字段。
    m_patientIdEdit = CreateStandardLineEdit(parent, "20260704103001");
    m_patientIdEdit->setReadOnly(true);
    m_patientIdEdit->setObjectName("OrderCreatePatientIdEdit");
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Patient ID")));
    outerLayout->addWidget(m_patientIdEdit);

    // 患者姓名是建单核心字段。textChanged 信号用于立即刷新右侧摘要。
    m_patientNameEdit = CreateStandardLineEdit(parent);
    m_patientNameEdit->setObjectName("OrderCreatePatientNameEdit");
    m_patientNameEdit->setText(tr("Test Patient"));
    QObject::connect(m_patientNameEdit, &QLineEdit::textChanged, [this]() {
        // 用户每次修改姓名时只刷新摘要，不做保存，避免 UI 层越界写数据库。
        RefreshBasicSummary();
        WriteLog(LogLevel::Info, "EditPatientName", "Patient name changed");
    });
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Name *")));
    outerLayout->addWidget(m_patientNameEdit);

    // 年龄和出生日期横向排列，减少左栏纵向空间占用。
    auto* ageBirthLayout = new QHBoxLayout();
    auto* ageBox = new QVBoxLayout();
    m_ageEdit = CreateStandardLineEdit(parent, "23");
    m_ageEdit->setObjectName("OrderCreateAgeEdit");
    ageBox->addWidget(CreateStandardFieldLabel(parent, tr("Age")));
    ageBox->addWidget(m_ageEdit);

    auto* birthBox = new QVBoxLayout();
    m_birthDateEdit = CreateStandardDateEdit(parent);
    m_birthDateEdit->setObjectName("OrderCreateBirthDateEdit");
    m_birthDateEdit->setDisplayFormat("yyyy/MM/dd");
    m_birthDateEdit->setDate(QDate(2003, 6, 29));
    birthBox->addWidget(CreateStandardFieldLabel(parent, tr("Birth Date")));
    birthBox->addWidget(m_birthDateEdit);

    ageBirthLayout->addLayout(ageBox, 1);
    ageBirthLayout->addLayout(birthBox, 1);
    outerLayout->addLayout(ageBirthLayout);

    // 性别使用单选按钮。三项都可见，避免 Unknown 被隐藏在下拉框里。
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Gender")));
    auto* genderLayout = new QHBoxLayout();
    m_genderMale = new QRadioButton(tr("Male"), parent);
    m_genderFemale = new QRadioButton(tr("Female"), parent);
    m_genderUnknown = new QRadioButton(tr("Unknown"), parent);
    m_genderUnknown->setChecked(true);
    genderLayout->addWidget(m_genderMale);
    genderLayout->addWidget(m_genderFemale);
    genderLayout->addWidget(m_genderUnknown);
    outerLayout->addLayout(genderLayout);

    // 类型按钮先保留“修复/正畸”的可视选择，后续可接权限和建单规则。
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Case Type")));
    auto* caseTypeLayout = new QHBoxLayout();
    caseTypeLayout->addWidget(CreateCheckButton(parent, tr("Restoration"), true));
    caseTypeLayout->addWidget(CreateCheckButton(parent, tr("Orthodontics"), false));
    outerLayout->addLayout(caseTypeLayout);

    // 主治医生和技工所使用下拉框，后续由 RuntimeDataCenter 提供真实列表。
    m_doctorCombo = CreateStandardComboBox(parent);
    m_doctorCombo->setObjectName("OrderCreateDoctorCombo");
    m_doctorCombo->addItem(tr("Doctor"));
    m_doctorCombo->addItem(tr("Dr. Wang"));
    m_doctorCombo->addItem(tr("Dr. Li"));
    QObject::connect(m_doctorCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int) {
        // 医生变化会影响右侧摘要，但不会在 UI 层直接写库。
        RefreshBasicSummary();
        WriteLog(LogLevel::Info, "SelectDoctor", "Doctor changed");
    });
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Doctor *")));
    outerLayout->addWidget(m_doctorCombo);

    m_orderIdEdit = CreateStandardLineEdit(parent, "20260704103001-1");
    m_orderIdEdit->setObjectName("OrderCreateOrderIdEdit");
    m_orderIdEdit->setReadOnly(true);
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Order ID")));
    outerLayout->addWidget(m_orderIdEdit);

    m_labCombo = CreateStandardComboBox(parent);
    m_labCombo->setObjectName("OrderCreateLabCombo");
    m_labCombo->addItem(tr("Default Lab"));
    m_labCombo->addItem(tr("Partner Lab"));
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Lab")));
    outerLayout->addWidget(m_labCombo);

    m_deliveryDateEdit = CreateStandardDateEdit(parent);
    m_deliveryDateEdit->setObjectName("OrderCreateDeliveryDateEdit");
    m_deliveryDateEdit->setDisplayFormat("yyyy/MM/dd");
    m_deliveryDateEdit->setDate(QDate::currentDate().addDays(3));
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Delivery Date")));
    outerLayout->addWidget(m_deliveryDateEdit);

    m_contactEdit = CreateStandardLineEdit(parent);
    m_contactEdit->setObjectName("OrderCreateContactEdit");
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Contact")));
    outerLayout->addWidget(m_contactEdit);

    m_patientNoteEdit = CreateStandardTextEdit(parent, 52);
    m_patientNoteEdit->setObjectName("OrderCreatePatientNoteEdit");
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Patient Note")));
    outerLayout->addWidget(m_patientNoteEdit);

    // stretch 把表单推到上方，低分辨率时外层滚动区域负责显示完整内容。
    outerLayout->addStretch(1);

    auto* scrollContent = new QWidget(parent);
    scrollContent->setLayout(outerLayout);

    // 左栏使用滚动区，保证 1366x768 等低高度屏幕下底部字段仍可访问。
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(scrollContent);

    auto* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->addWidget(scroll);
    return CreateSection(parent, "OrderCreateBasicInfoPanel", tr("Basic Information"), scrollLayout);
}

// 创建中间牙位扫描方案区。
QWidget* OrderCreateUIImpl::CreateToothPlanPanel(QWidget* parent) {
    auto* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // 类型选择区域放在牙位上方，用户先选类型再点牙位，符合原界面的使用习惯。
    auto* typeRow = new QHBoxLayout();
    typeRow->setSpacing(6);
    typeRow->addWidget(CreateTypeButton(parent, tr("Crown"), "crown", true));
    typeRow->addWidget(CreateTypeButton(parent, tr("Missing Tooth"), "missing", false));
    typeRow->addWidget(CreateTypeButton(parent, tr("Inlay"), "inlay", false));
    typeRow->addWidget(CreateTypeButton(parent, tr("Veneer"), "veneer", false));
    typeRow->addWidget(CreateTypeButton(parent, tr("Implant Body"), "implant", false));
    mainLayout->addLayout(typeRow);

    // 牙位提示文案放在同一页内，帮助用户理解“先选类型再点牙位”的关系。
    auto* hint = new QLabel(tr("Select a restoration type, then click teeth to add or remove items."), parent);
    hint->setObjectName("OrderCreatePlanHintLabel");
    hint->setStyleSheet("color:#6b7c8f;");
    mainLayout->addWidget(hint);

    auto* toothGrid = new QGridLayout();
    toothGrid->setSpacing(5);
    toothGrid->setContentsMargins(8, 8, 8, 8);

    // 牙位使用 FDI 编号。初版用规整网格表达上下颌，后续可替换为更接近截图的牙弓绘制控件。
    AddToothRow(toothGrid, 0, QList<int>() << 18 << 17 << 16 << 15 << 14 << 13 << 12 << 11 << 21 << 22 << 23 << 24 << 25 << 26 << 27 << 28);
    AddToothRow(toothGrid, 1, QList<int>() << 48 << 47 << 46 << 45 << 44 << 43 << 42 << 41 << 31 << 32 << 33 << 34 << 35 << 36 << 37 << 38);
    mainLayout->addLayout(toothGrid, 1);

    auto* controlRow = new QHBoxLayout();
    controlRow->addStretch(1);

    auto* clearButton = CreateStandardButton(parent, tr("Clear All"), MeyerButtonRoleSecondary);
    clearButton->setObjectName("OrderCreateClearAllButton");
    clearButton->setMinimumWidth(120);
    QObject::connect(clearButton, &QPushButton::clicked, [this]() {
        // 清空牙位是局部 UI 操作，同时向外抛动作，便于上层记录用户行为或刷新流程状态。
        ClearAllTeeth();
        EmitAction(OrderCreateActionClearAllTeeth);
    });
    controlRow->addWidget(clearButton);
    controlRow->addStretch(1);
    mainLayout->addLayout(controlRow);

    return CreateSection(parent, "OrderCreateToothPlanPanel", tr("Scan Plan"), mainLayout);
}

// 创建右侧订单摘要和确认区。
QWidget* OrderCreateUIImpl::CreateOrderSummaryPanel(QWidget* parent) {
    auto* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(12, 14, 12, 12);
    mainLayout->setSpacing(9);

    // 基本摘要让用户不用回到左侧表单也能确认姓名、医生、订单编号。
    auto* summaryGroup = new QGroupBox(tr("Summary"), parent);
    auto* summaryLayout = new QGridLayout(summaryGroup);
    summaryLayout->setContentsMargins(12, 16, 12, 12);
    summaryLayout->setHorizontalSpacing(8);
    summaryLayout->setVerticalSpacing(8);
    m_summaryPatientName = new QLabel(parent);
    m_summaryDoctor = new QLabel(parent);
    m_summaryOrderId = new QLabel(parent);
    m_summarySource = new QLabel(parent);
    // 摘要值使用动态属性加粗，和左侧字段标签形成清晰层级。
    m_summaryPatientName->setProperty("valueText", true);
    m_summaryDoctor->setProperty("valueText", true);
    m_summaryOrderId->setProperty("valueText", true);
    m_summarySource->setProperty("valueText", true);
    m_summarySource->setWordWrap(true);
    summaryLayout->addWidget(CreateStandardFieldLabel(parent, tr("Patient")), 0, 0);
    summaryLayout->addWidget(m_summaryPatientName, 0, 1);
    summaryLayout->addWidget(CreateStandardFieldLabel(parent, tr("Doctor")), 1, 0);
    summaryLayout->addWidget(m_summaryDoctor, 1, 1);
    summaryLayout->addWidget(CreateStandardFieldLabel(parent, tr("Order")), 2, 0);
    summaryLayout->addWidget(m_summaryOrderId, 2, 1);
    summaryLayout->addWidget(CreateStandardFieldLabel(parent, tr("Source")), 3, 0);
    summaryLayout->addWidget(m_summarySource, 3, 1);
    mainLayout->addWidget(summaryGroup);

    // 已选牙位表格是右侧核心内容。只显示 UI 临时状态，保存时交给外部服务。
    m_selectionTable = CreateStandardTableWidget(parent);
    m_selectionTable->setObjectName("OrderCreateSelectionTable");
    m_selectionTable->setColumnCount(4);
    m_selectionTable->setHorizontalHeaderLabels(QStringList() << tr("Tooth") << tr("Type") << tr("Material") << tr("Shade"));
    m_selectionTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_selectionTable->setMinimumHeight(160);
    mainLayout->addWidget(m_selectionTable, 1);

    // 标信息区域先搭骨架，后续可改成颜色模块/材料规则驱动。
    auto* shadeGroup = new QGroupBox(tr("Shade Information"), parent);
    auto* shadeLayout = new QVBoxLayout(shadeGroup);
    shadeLayout->setContentsMargins(12, 16, 12, 12);
    shadeLayout->setSpacing(10);
    auto* shadeRow = new QHBoxLayout();
    const QStringList shades = QStringList() << "A1" << "A2" << "A3" << "B1";
    for (const QString& shade : shades) {
        // QPushButton 作为色号占位控件，后续可替换为 MyUIComponents 中的专用色号按钮。
        auto* shadeButton = CreateStandardButton(parent, shade, MeyerButtonRoleText);
        shadeButton->setObjectName(QString("OrderCreateShade%1Button").arg(shade));
        shadeButton->setMinimumWidth(54);
        shadeRow->addWidget(shadeButton);
    }
    shadeLayout->addLayout(shadeRow);

    auto* prepareScan = new QCheckBox(tr("Prepare tooth scan"), parent);
    prepareScan->setObjectName("OrderCreatePrepareScanCheck");
    shadeLayout->addWidget(prepareScan);
    mainLayout->addWidget(shadeGroup);

    // 订单备注放在右侧确认前，便于最终提交前补充说明。
    m_orderNoteEdit = CreateStandardTextEdit(parent, 56);
    m_orderNoteEdit->setObjectName("OrderCreateOrderNoteEdit");
    mainLayout->addWidget(CreateStandardFieldLabel(parent, tr("Order Note")));
    mainLayout->addWidget(m_orderNoteEdit);

    // 底部操作区：上一步/取消/确认/下一步。上层根据动作 ID 决定流程推进。
    auto* actionRow = new QHBoxLayout();
    auto* previousButton = CreateStandardButton(parent, tr("Previous"), MeyerButtonRoleSecondary);
    previousButton->setObjectName("OrderCreatePreviousButton");
    QObject::connect(previousButton, &QPushButton::clicked, [this]() {
        EmitAction(OrderCreateActionPrevious);
    });

    auto* cancelButton = CreateStandardButton(parent, tr("Cancel"), MeyerButtonRoleSecondary);
    cancelButton->setObjectName("OrderCreateCancelButton");
    QObject::connect(cancelButton, &QPushButton::clicked, [this]() {
        EmitAction(OrderCreateActionCancel);
    });

    auto* confirmButton = CreateStandardButton(parent, tr("Confirm"), MeyerButtonRolePrimary);
    confirmButton->setObjectName("OrderCreateConfirmButton");
    QObject::connect(confirmButton, &QPushButton::clicked, [this]() {
        EmitAction(OrderCreateActionConfirm);
    });

    auto* nextButton = CreateStandardButton(parent, tr("Next"), MeyerButtonRolePrimary);
    nextButton->setObjectName("OrderCreateNextButton");
    QObject::connect(nextButton, &QPushButton::clicked, [this]() {
        EmitAction(OrderCreateActionNext);
    });

    actionRow->addWidget(previousButton);
    actionRow->addWidget(cancelButton);
    actionRow->addStretch(1);
    actionRow->addWidget(confirmButton);
    actionRow->addWidget(nextButton);
    mainLayout->addLayout(actionRow);

    return CreateSection(parent, "OrderCreateSummaryPanel", tr("Order Detail"), mainLayout);
}

// 创建带标题的分组面板。
QWidget* OrderCreateUIImpl::CreateSection(QWidget* parent, const QString& objectName, const QString& title, QLayout* contentLayout) const {
    // 使用 QGroupBox 而不是自绘 QWidget，减少样式和可访问性上的自定义代码。
    auto* section = new QGroupBox(title, parent);
    section->setObjectName(objectName);
    section->setLayout(contentLayout);
    return section;
}

// 创建病例类型按钮。
QPushButton* OrderCreateUIImpl::CreateCheckButton(QWidget* parent, const QString& text, bool checked) const {
    auto* button = new QPushButton(text, parent);
    // checkable 让按钮自己维护选中状态，减少额外变量。
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("typeButton", true);
    button->setProperty("typeSelected", checked);
    QObject::connect(button, &QPushButton::toggled, [button](bool on) {
        // Qt 样式表按动态属性刷新，需要 unpolish/polish 重新应用样式。
        button->setProperty("typeSelected", on);
        button->style()->unpolish(button);
        button->style()->polish(button);
    });
    return button;
}

// 创建扫描类型按钮。
QPushButton* OrderCreateUIImpl::CreateTypeButton(QWidget* parent, const QString& text, const QString& code, bool checked) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QString("OrderCreateType_%1_Button").arg(code));
    button->setMinimumWidth(92);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("typeButton", true);
    button->setProperty("typeSelected", checked);
    m_typeButtons.insert(code, button);

    QObject::connect(button, &QPushButton::clicked, [this, code]() {
        // 类型按钮只改变“后续点击牙位使用的类型”，不会 retroactively 修改已选牙位。
        SetCurrentType(code);
    });
    return button;
}

// 创建单颗牙位按钮。
QPushButton* OrderCreateUIImpl::CreateToothButton(QWidget* parent, int toothNumber) {
    auto* button = new QPushButton(QString::number(toothNumber), parent);
    button->setObjectName(QString("OrderCreateTooth%1Button").arg(toothNumber));
    button->setMinimumSize(34, 34);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setProperty("toothButton", true);
    button->setProperty("toothSelected", false);
    m_toothButtons.insert(toothNumber, button);

    QObject::connect(button, &QPushButton::clicked, [this, toothNumber]() {
        // 牙位点击直接切换本地 UI 状态，并刷新右侧明细表。
        ToggleTooth(toothNumber);
    });
    return button;
}

// 添加一行牙位按钮。
void OrderCreateUIImpl::AddToothRow(QGridLayout* grid, int row, const QList<int>& teeth) {
    // teeth 的顺序就是界面展示顺序，调用方可以按 FDI 上下颌习惯排列。
    for (int column = 0; column < teeth.size(); ++column) {
        grid->addWidget(CreateToothButton(grid->parentWidget(), teeth.at(column)), row, column);
    }
}

// 返回当前扫描类型显示名。
QString OrderCreateUIImpl::CurrentTypeText() const {
    // 当前类型只是 TypeText 的一个便捷入口，避免多处重复判断当前编码。
    return TypeText(m_currentTypeCode);
}

// 从 JSON 对象中读取字符串字段。
// 第三方字段可能是字符串、数字或布尔值；这里统一转成界面可显示文本，减少适配器差异对 UI 的影响。
QString OrderCreateUIImpl::ReadString(const QJsonObject& object, const QString& key, const QString& defaultValue) const {
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        // 对 ID 类字段，第三方偶尔会给数字；用无小数格式转成文本，避免显示成 1.000000。
        return QString::number(static_cast<qint64>(value.toDouble()));
    }
    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }
    return defaultValue;
}

// 从 JSON 对象中读取整数文本。
// 年龄等字段如果第三方传字符串也允许显示，避免因类型不一致导致界面空白。
QString OrderCreateUIImpl::ReadIntText(const QJsonObject& object, const QString& key, const QString& defaultValue) const {
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return QString::number(static_cast<int>(value.toDouble()));
    }
    if (value.isString()) {
        return value.toString();
    }
    return defaultValue;
}

// 设置下拉框当前文本。
// 医生、技工所等参考数据后续会来自 RuntimeDataCenter；第三方传入的新名称先追加到列表，保证信息不丢。
void OrderCreateUIImpl::SetComboText(QComboBox* combo, const QString& text) {
    if (!combo || text.trimmed().isEmpty()) {
        return;
    }
    const QString normalizedText = text.trimmed();
    const int index = combo->findText(normalizedText);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->addItem(normalizedText);
        combo->setCurrentIndex(combo->count() - 1);
    }
}

// 根据上下文设置性别。
// 适配器输出建议使用 male/female/unknown；这里兼容常见大小写和中文，便于测试 JSON 手写。
void OrderCreateUIImpl::SetGender(const QString& gender) {
    const QString value = gender.trimmed().toLower();
    if ((value == "male" || value == "m" || value == QString::fromUtf8("男")) && m_genderMale) {
        m_genderMale->setChecked(true);
        return;
    }
    if ((value == "female" || value == "f" || value == QString::fromUtf8("女")) && m_genderFemale) {
        m_genderFemale->setChecked(true);
        return;
    }
    if (m_genderUnknown) {
        m_genderUnknown->setChecked(true);
    }
}

// 应用扫描方案牙位 items。
// 当前只消费 tooth/type/material/shade 中的 tooth 和 type，材料/齿色后续进入 ScanSchemaService 后再绑定。
void OrderCreateUIImpl::ApplyScanPlanItems(const QJsonObject& scanPlanObject) {
    const QJsonArray items = scanPlanObject.value("items").toArray();
    if (items.isEmpty()) {
        return;
    }

    // 先把旧选中牙位按钮刷回未选，再清空集合，防止默认示例牙位混入第三方方案。
    const QSet<int> oldSelection = m_selectedTeeth;
    m_selectedTeeth.clear();
    m_toothTypeCodes.clear();
    for (int toothNumber : oldSelection) {
        UpdateToothButtonState(toothNumber);
    }

    for (const QJsonValue& itemValue : items) {
        const QJsonObject item = itemValue.toObject();
        const int toothNumber = item.value("tooth").toInt(0);
        if (toothNumber <= 0) {
            // 非法牙位跳过，不让一条坏数据影响整个建单上下文。
            continue;
        }
        const QString typeCode = ReadString(item, "type", "crown").trimmed();
        m_selectedTeeth.insert(toothNumber);
        m_toothTypeCodes.insert(toothNumber, typeCode.isEmpty() ? "crown" : typeCode);
        UpdateToothButtonState(toothNumber);
    }
}

// 清理当前 QWidget 内部控件弱引用。
// 这些指针不拥有对象，只用于刷新 UI；外部删除 QWidget 后必须清空，避免下一次使用悬空地址。
void OrderCreateUIImpl::ResetWidgetReferences() {
    m_root = nullptr;
    m_toothButtons.clear();
    m_typeButtons.clear();
    m_selectionTable = nullptr;
    m_summaryPatientName = nullptr;
    m_summaryDoctor = nullptr;
    m_summaryOrderId = nullptr;
    m_summarySource = nullptr;
    m_patientIdEdit = nullptr;
    m_patientNameEdit = nullptr;
    m_ageEdit = nullptr;
    m_birthDateEdit = nullptr;
    m_genderMale = nullptr;
    m_genderFemale = nullptr;
    m_genderUnknown = nullptr;
    m_doctorCombo = nullptr;
    m_orderIdEdit = nullptr;
    m_labCombo = nullptr;
    m_deliveryDateEdit = nullptr;
    m_contactEdit = nullptr;
    m_patientNoteEdit = nullptr;
    m_orderNoteEdit = nullptr;
}

// 返回指定扫描类型显示名。
QString OrderCreateUIImpl::TypeText(const QString& typeCode) const {
    // 每个可见文本都显式写成 tr("English")，便于 lupdate 提取翻译源文案。
    if (typeCode == "crown") {
        return tr("Crown");
    }
    if (typeCode == "missing") {
        return tr("Missing Tooth");
    }
    if (typeCode == "inlay") {
        return tr("Inlay");
    }
    if (typeCode == "veneer") {
        return tr("Veneer");
    }
    if (typeCode == "implant") {
        return tr("Implant Body");
    }
    return tr("Unknown");
}

// 设置当前扫描类型。
void OrderCreateUIImpl::SetCurrentType(const QString& typeCode) {
    // 未注册的类型编码说明调用方或按钮配置有问题，记录日志后忽略。
    if (!m_typeButtons.contains(typeCode)) {
        WriteLog(LogLevel::Warning, "SetCurrentType", QString("Unknown type: %1").arg(typeCode));
        return;
    }

    // 更新当前类型编码，后续点击牙位会按这个类型加入明细。
    m_currentTypeCode = typeCode;

    // 刷新所有类型按钮的选中视觉状态，保证同一时间只有一个类型高亮。
    for (auto it = m_typeButtons.begin(); it != m_typeButtons.end(); ++it) {
        const bool selected = (it.key() == typeCode);
        it.value()->setChecked(selected);
        it.value()->setProperty("typeSelected", selected);
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }

    WriteLog(LogLevel::Info, "SetCurrentType", QString("Current type: %1").arg(typeCode));
}

// 切换牙位选择。
void OrderCreateUIImpl::ToggleTooth(int toothNumber) {
    if (m_selectedTeeth.contains(toothNumber)) {
        // 已选牙位再次点击表示移除。
        m_selectedTeeth.remove(toothNumber);
        // 移除牙位时同步移除类型，避免重新选择时读到旧类型。
        m_toothTypeCodes.remove(toothNumber);
        WriteLog(LogLevel::Info, "RemoveTooth", QString("Tooth removed: %1").arg(toothNumber));
    } else {
        // 未选牙位点击表示加入当前扫描类型。
        m_selectedTeeth.insert(toothNumber);
        // 每颗牙位保存自己被加入时的类型，避免后续切换当前类型影响旧明细。
        m_toothTypeCodes.insert(toothNumber, m_currentTypeCode);
        WriteLog(LogLevel::Info, "AddTooth", QString("Tooth added: %1, type: %2").arg(toothNumber).arg(m_currentTypeCode));
    }

    // UI 状态变化后同步刷新按钮和右侧表格。
    UpdateToothButtonState(toothNumber);
    RefreshSelectionTable();
    EmitAction(OrderCreateActionToothSelectionChanged);
}

// 清空所有牙位选择。
void OrderCreateUIImpl::ClearAllTeeth() {
    // 先复制当前集合，避免遍历过程中修改 QSet 导致迭代器失效。
    const QSet<int> oldSelection = m_selectedTeeth;
    m_selectedTeeth.clear();
    // 类型映射和牙位集合必须一起清空，保持 UI 状态一致。
    m_toothTypeCodes.clear();

    // 所有旧选中按钮都要刷新成未选状态。
    for (int toothNumber : oldSelection) {
        UpdateToothButtonState(toothNumber);
    }

    // 表格清空后仍保留表头。
    RefreshSelectionTable();
    WriteLog(LogLevel::Info, "ClearAllTeeth", "All tooth selections cleared");
}

// 刷新右侧明细表。
void OrderCreateUIImpl::RefreshSelectionTable() {
    if (!m_selectionTable) {
        return;
    }

    // QSet 无序，先转 QList 排序，确保表格显示顺序稳定，便于人工阅读和自动化测试。
    QList<int> teeth = m_selectedTeeth.values();
    std::sort(teeth.begin(), teeth.end());

    m_selectionTable->setRowCount(teeth.size());
    for (int row = 0; row < teeth.size(); ++row) {
        const int toothNumber = teeth.at(row);

        // 每个单元格都创建 QTableWidgetItem；当前表格只读，所以不需要单独 delegate。
        auto* toothItem = new QTableWidgetItem(QString::number(toothNumber));
        // 每颗牙显示自己的类型，而不是显示当前工具栏类型。
        auto* typeItem = new QTableWidgetItem(TypeText(m_toothTypeCodes.value(toothNumber, m_currentTypeCode)));
        auto* materialItem = new QTableWidgetItem("--");
        auto* shadeItem = new QTableWidgetItem("--");

        m_selectionTable->setItem(row, 0, toothItem);
        m_selectionTable->setItem(row, 1, typeItem);
        m_selectionTable->setItem(row, 2, materialItem);
        m_selectionTable->setItem(row, 3, shadeItem);
    }
}

// 刷新基本信息摘要。
void OrderCreateUIImpl::RefreshBasicSummary() {
    if (m_summaryPatientName && m_patientNameEdit) {
        // 摘要直接读当前输入框文本；后续可改为读取 OrderDraft 数据对象。
        m_summaryPatientName->setText(m_patientNameEdit->text());
    }
    if (m_summaryDoctor && m_doctorCombo) {
        // 医生下拉框展示文本作为摘要，真实 doctorId 后续应保存在数据模型中。
        m_summaryDoctor->setText(m_doctorCombo->currentText());
    }
    if (m_summaryOrderId && m_orderIdEdit) {
        // 订单编号只读，摘要同步显示即可。
        m_summaryOrderId->setText(m_orderIdEdit->text());
    }
    if (m_summarySource) {
        // 来源摘要用于区分手工建单、第三方拉起和未来 HIS/Worklist 建单。
        m_summarySource->setText(m_sourceSummary.isEmpty() ? tr("Manual") : m_sourceSummary);
    }
}

// 更新牙位按钮选中样式。
void OrderCreateUIImpl::UpdateToothButtonState(int toothNumber) {
    QPushButton* button = m_toothButtons.value(toothNumber, nullptr);
    if (!button) {
        return;
    }

    // toothSelected 动态属性由样式表决定按钮颜色。
    const bool selected = m_selectedTeeth.contains(toothNumber);
    button->setProperty("toothSelected", selected);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

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
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName("MeyerScan_UIComponents");
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local order styles");
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
        m_uiComponents->Init(appDirBytes.constData());
        WriteLog(LogLevel::Info, "LoadUIComponents", "UIComponents loaded for order create UI");
    }
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

    // 降级路径只使用 Qt 原生按钮，并在本地套一份最小通用样式。
    auto* button = new QPushButton(text, parent);
    if (role == MeyerButtonRolePrimary) {
        button->setStyleSheet("QPushButton{background:#007d68;color:white;border:0;border-radius:4px;padding:8px 18px;min-height:36px;}"
                              "QPushButton:hover{background:#009176;}"
                              "QPushButton:pressed{background:#006652;}");
    } else if (role == MeyerButtonRoleText) {
        button->setStyleSheet("QPushButton{background:transparent;color:#007d68;border:0;border-radius:4px;padding:6px 10px;min-height:30px;}"
                              "QPushButton:hover{background:#e7f3f0;}"
                              "QPushButton:pressed{background:#d6ebe6;}");
    } else {
        button->setStyleSheet("QPushButton{background:#f6f8fa;color:#23313f;border:1px solid #cfd8dc;border-radius:4px;padding:8px 16px;min-height:36px;}"
                              "QPushButton:hover{background:#edf2f5;}"
                              "QPushButton:pressed{background:#e1e8ec;}");
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
        edit->setStyleSheet("QLineEdit{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:#ffffff;color:#23313f;min-height:34px;}"
                            "QLineEdit:focus{border-color:#007d68;}"
                            "QLineEdit[readOnly=\"true\"]{background:#eef2f5;color:#23313f;}");
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
        combo->setStyleSheet("QComboBox{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:#ffffff;color:#23313f;min-height:34px;}"
                             "QComboBox:focus{border-color:#007d68;}");
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
        edit->setStyleSheet("QDateEdit{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:#ffffff;color:#23313f;min-height:34px;}"
                            "QDateEdit:focus{border-color:#007d68;}");
    }
    return edit;
}

// 创建统一多行文本框；共享 UI 不可用时返回本地降级文本框。
QTextEdit* OrderCreateUIImpl::CreateStandardTextEdit(QWidget* parent, int fixedHeight) const {
    QTextEdit* edit = m_uiComponents
        ? m_uiComponents->CreateTextEdit(parent)
        : new QTextEdit(parent);
    if (!m_uiComponents) {
        edit->setStyleSheet("QTextEdit{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:#ffffff;color:#23313f;min-height:56px;}"
                            "QTextEdit:focus{border-color:#007d68;}");
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
        label->setStyleSheet("QLabel{color:#4f5f6f;font-size:13px;font-weight:500;}");
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
        table->setStyleSheet(
            "QTableWidget{background:#ffffff;border:1px solid #d8e1e7;border-radius:4px;"
            "gridline-color:#edf1f4;color:#23313f;alternate-background-color:#f8fafb;"
            "selection-background-color:#dff1ec;selection-color:#23313f;}"
            "QTableWidget::item{padding:6px;}"
            "QHeaderView::section{background:#edf3f5;color:#23313f;border:0;"
            "border-bottom:1px solid #d8e1e7;padding:7px;font-weight:600;}");
    }
    return table;
}

// 导出建单界面模块单例。
extern "C" MEYERSCAN_ORDERCREATEUI_API IOrderCreateUI* GetOrderCreateUI() {
    // C ABI 导出名保持稳定，便于 MainExe 后续动态加载 DLL。
    return &OrderCreateUIImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取建单 UI 代码版本时，不创建表单控件，也不加载共享 UI。
extern "C" MEYERSCAN_ORDERCREATEUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
