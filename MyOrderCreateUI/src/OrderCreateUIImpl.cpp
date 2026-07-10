#include "OrderCreateUIImpl.h"

#include "MeyerQtModuleUtils.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <functional>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与工程里的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_OrderCreateUI";

// 模块版本用于 GetModuleVersion()，需要和 Version.rc 同步维护。
const char* Version = "MeyerScan_OrderCreateUI v0.4.1 (2026-07-10)";
}

const char* kOcclusionNatural = "natural";
const char* kOcclusionMaxillaTemporary = "maxilla_temporary";
const char* kOcclusionMandibleTemporary = "mandible_temporary";
const char* kOcclusionFullTemporary = "full_temporary";
const char* kOcclusionRecord = "record";

// 治疗方案资源在运行总目录下的标准位置。
// 规则：源码放在模块 Resources 下，构建后复制到 MeyerScan.exe 同级 Resources/Modules/<ModuleName>/ 下。
const char* kTreatmentPlanRuntimeRelativePath = "Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan";

// 治疗方案资源在模块源码目录下的位置，用于测试宿主和开发期 fallback。
const char* kTreatmentPlanSourceRelativePath = "Resources/icon/createModule/sacanPlan";

// 历史资源目录，当前保留 fallback，后续资源迁移完成后可以删除。
const char* kTreatmentPlanLegacyRelativePath = "icon/createModule/sacanPlan";

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

    // 初始化时缓存日志接口。后续所有日志都通过 m_logger 输出，符合"每模块一份日志变量"的规则。
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
    // 桥连接点属于治疗方案的一部分，初始化时也必须清空。
    m_selectedBridgeKeys.clear();
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
    // 重新创建界面前先清空弱引用缓存。
    // 这些指针只指向上一棵 QWidget 树里的控件；如果不清空，重复打开建单页时可能刷新到旧控件。
    m_typeButtons.clear();
    m_currentTypeSummaryLabel = nullptr;
    m_treatmentPlanWidget = nullptr;
    m_selectionTable = nullptr;
    m_bridgeSummaryLabel = nullptr;
    // 根界面不再按 1920x1080 方案写死大尺寸。
    // 工作台壳负责全屏显示，本页只给出可用下限，低分辨率时由滚动区和布局共同适配。
    root->setMinimumSize(960, 600);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 建单页面所有视觉规则从模块 QSS 读取；业务源码只设置 objectName、动态属性和布局。
    // 工作台步骤导航由 OrderScanWorkspaceShell 唯一持有，本页不再复制第二套步骤条。
    MeyerQtModule::ApplyModuleQss(root, "MyOrderCreateUI", "order_create.qss", m_logger);

    // 根布局使用三栏：左基本信息，中间牙位规划，右订单摘要。
    auto* pageLayout = new QVBoxLayout(root);
    pageLayout->setContentsMargins(12, 10, 12, 12);
    pageLayout->setSpacing(10);

    auto* rootLayout = new QHBoxLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);
    pageLayout->addLayout(rootLayout, 1);

    // 左侧工作区按视频组织：上方选择治疗类型，下方显示基础信息。
    // 这样治疗类型不再占用中间牙弓区域，牙弓在 1920x1080 和高分屏下都能成为主视觉。
    QWidget* leftPanel = CreateLeftWorkflowPanel(root);
    leftPanel->setMinimumWidth(310);
    leftPanel->setMaximumWidth(410);
    rootLayout->addWidget(leftPanel, 0);

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
        RefreshTreatmentPlanWidget();
        RefreshBridgeSummary();
    }

    // 页面初次创建完成后立即生成扫描流程 JSON。
    // 这样即使用户直接点击顶部 Scan 步骤，MainExe 也能读取到有效流程。
    RefreshScanProcessPreview(false);

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

    // 保存一份原始 UTF-8 文本，保证"先设置上下文、后创建界面"的调用顺序可用。
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
    // scanProcess 是新增的流程控制输入，第三方或后续 HIS 也可以直接给出统一配置。
    ApplyScanProcessConfig(rootObject.value("scanProcess").toObject());
    RefreshSelectionTable();
    RefreshBasicSummary();
    RefreshScanProcessPreview(false);

    WriteLog(LogLevel::Info,
             "SetOrderContextJson",
             QString("Context applied, thirdPartyType=%1").arg(thirdPartyType));
    return true;
}

// 返回模块版本字符串。
const char* OrderCreateUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 返回当前扫描流程 JSON。
const char* OrderCreateUIImpl::GetCurrentScanProcessJson() {
    // 每次外部读取前都重新生成一遍，避免用户刚修改控件但缓存尚未更新。
    RefreshScanProcessPreview(false);
    WriteLog(LogLevel::Info,
             "GetCurrentScanProcessJson",
             QString("Scan process json bytes: %1").arg(m_currentScanProcessJson.size()));
    return m_currentScanProcessJson.constData();
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
    m_selectedBridgeKeys.clear();
    m_typeButtons.clear();
    ResetWidgetReferences();
    m_pendingContextJson.clear();
    m_hasPendingContext = false;
    m_sourceSummary.clear();
    m_maxillaDiffRodSwitch = nullptr;
    m_mandibleDiffRodSwitch = nullptr;
    m_maxillaSegmentedRodSwitch = nullptr;
    m_mandibleSegmentedRodSwitch = nullptr;
    m_occlusionTypeCombo = nullptr;
    m_scanProcessPreviewLabel = nullptr;
    m_treatmentPlanWidget = nullptr;
    m_bridgeSummaryLabel = nullptr;
    m_currentScanProcessJson.clear();
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

    // 类型按钮先保留"修复/正畸"的可视选择，后续可接权限和建单规则。
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

// 创建左侧工作区。
QWidget* OrderCreateUIImpl::CreateLeftWorkflowPanel(QWidget* parent) {
    // 左侧工作区本身不画边框，只负责把"治疗类型"和"基本信息"两张卡按视频顺序垂直摆放。
    auto* container = new QWidget(parent);
    container->setObjectName("OrderCreateLeftWorkflowPanel");
    container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // 使用 QVBoxLayout 让两张卡共享左侧宽度；底部基础信息给 stretch，低分辨率下仍能获得最大高度。
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // 治疗类型必须放在患者信息上方，这和视频中的操作路径一致：先选修复类型，再点中间牙弓。
    QWidget* typePanel = CreateTreatmentTypePanel(container);
    layout->addWidget(typePanel, 0);

    // 基础信息继续复用原有 CreateBasicInfoPanel，避免表单字段和第三方上下文填充逻辑被重复实现。
    QWidget* basicPanel = CreateBasicInfoPanel(container);
    layout->addWidget(basicPanel, 1);
    return container;
}

// 创建治疗类型选择区。
QWidget* OrderCreateUIImpl::CreateTreatmentTypePanel(QWidget* parent) {
    // 当前软件视频中治疗类型是一张独立卡片：标题、图标按钮网格、当前选中类型提示。
    // 本函数只负责控件创建；真正的选中状态刷新集中在 SetCurrentType。
    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(12, 14, 12, 12);
    layout->setSpacing(10);

    // 4 列网格更接近视频里的横向图标排布，也能减少左侧卡片高度。
    auto* typeGrid = new QGridLayout();
    typeGrid->setHorizontalSpacing(8);
    typeGrid->setVerticalSpacing(8);

    // 每个按钮都是单选入口；checked 由当前编码决定，避免默认类型和按钮高亮不同步。
    typeGrid->addWidget(CreateTypeButton(parent, tr("Implant"), "implant", m_currentTypeCode == "implant"), 0, 0);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Full Crown"), "crown", m_currentTypeCode == "crown"), 0, 1);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Missing Tooth"), "missing", m_currentTypeCode == "missing"), 0, 2);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Inlay"), "inlay", m_currentTypeCode == "inlay"), 0, 3);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Veneer"), "veneer", m_currentTypeCode == "veneer"), 1, 0);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Inner Crown"), "inner_crown", m_currentTypeCode == "inner_crown"), 1, 1);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Bridge"), "bridge", m_currentTypeCode == "bridge"), 1, 2);
    typeGrid->setColumnStretch(0, 1);
    typeGrid->setColumnStretch(1, 1);
    typeGrid->setColumnStretch(2, 1);
    typeGrid->setColumnStretch(3, 1);
    layout->addLayout(typeGrid);

    // 当前类型摘要对应视频左侧底部的大按钮区域。
    // 它是状态提示，不承担点击行为；用户仍然通过上面的图标按钮切换类型。
    m_currentTypeSummaryLabel = new QLabel(CurrentTypeText(), parent);
    m_currentTypeSummaryLabel->setObjectName("OrderCreateCurrentTypeSummary");
    m_currentTypeSummaryLabel->setAlignment(Qt::AlignCenter);
    m_currentTypeSummaryLabel->setMinimumHeight(44);
    m_currentTypeSummaryLabel->setWordWrap(true);
    layout->addWidget(m_currentTypeSummaryLabel);

    return CreateSection(parent, "OrderCreateTreatmentTypePanel", tr("Treatment Type"), layout);
}

// 创建中间牙位扫描方案区。
QWidget* OrderCreateUIImpl::CreateToothPlanPanel(QWidget* parent) {
    auto* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto* planRow = new QHBoxLayout();
    planRow->setContentsMargins(0, 0, 0, 0);
    planRow->setSpacing(0);

    // 治疗方案图片控件使用 maxilla/mandible 图片和 mask 进行命中。
    // 它内部始终按原图 600x400 坐标换算，窗口缩放时不会改变业务坐标。
    m_treatmentPlanWidget = new ToothTreatmentPlanWidget(parent);
    m_treatmentPlanWidget->setObjectName("OrderCreateTreatmentPlanWidget");
    m_treatmentPlanWidget->SetAssetRoot(ResolveTreatmentPlanAssetRoot());
    m_treatmentPlanWidget->SetCurrentTreatmentType(m_currentTypeCode);
    m_treatmentPlanWidget->SetToothClickedCallback([this](int toothNumber) {
        ToggleTooth(toothNumber);
    });
    m_treatmentPlanWidget->SetBridgeClickedCallback([this](const QString& bridgeKey) {
        ToggleBridgeConnector(bridgeKey);
    });
    m_treatmentPlanWidget->SetClearClickedCallback([this]() {
        // 视频中的清空操作需要二次确认，避免用户误删已经配置好的牙位方案。
        const bool skipConfirmForSmoke = qApp && qApp->property("MeyerScanSmokeTest").toBool();
        if (!skipConfirmForSmoke) {
            QMessageBox messageBox(m_root);
            messageBox.setIcon(QMessageBox::Question);
            messageBox.setWindowTitle(tr("Question"));
            messageBox.setText(tr("Are you sure to clear all selections?"));
            QPushButton* confirmButton = messageBox.addButton(tr("Confirm"), QMessageBox::AcceptRole);
            messageBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
            messageBox.exec();
            if (messageBox.clickedButton() != confirmButton) {
                WriteLog(LogLevel::Info, "ClearAllTeethCanceled", "Clear all selections canceled");
                return;
            }
        }

        // 清空牙位是局部 UI 操作，同时向外抛动作，便于上层记录用户行为或刷新流程状态。
        ClearAllTeeth();
        EmitAction(OrderCreateActionClearAllTeeth);
    });
    if (!m_treatmentPlanWidget->HasRequiredAssets()) {
        WriteLog(LogLevel::Warning,
                 "CreateToothPlanPanel",
                 QString("Treatment plan assets unavailable: %1").arg(ResolveTreatmentPlanAssetRoot()));
    }
    planRow->addWidget(m_treatmentPlanWidget, 1);
    mainLayout->addLayout(planRow, 1);

    // 扫描流程配置和牙位选择放在同一个建单页面内。
    // 这些输入决定后续 Scan/Process 页面显示哪些流程按钮，但不直接创建扫描页面。
    mainLayout->addWidget(CreateScanProcessConfigPanel(parent), 0);

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
    m_selectionTable->setMinimumHeight(140);
    mainLayout->addWidget(m_selectionTable, 1);

    // 桥记录单独展示，避免把"相邻牙位连接关系"误放进单颗牙位明细表。
    auto* bridgeGroup = new QGroupBox(tr("Bridge Records"), parent);
    auto* bridgeLayout = new QVBoxLayout(bridgeGroup);
    bridgeLayout->setContentsMargins(12, 16, 12, 12);
    bridgeLayout->setSpacing(6);
    m_bridgeSummaryLabel = new QLabel(tr("No bridge selected"), parent);
    m_bridgeSummaryLabel->setObjectName("OrderCreateBridgeSummaryLabel");
    m_bridgeSummaryLabel->setWordWrap(true);
    bridgeLayout->addWidget(m_bridgeSummaryLabel);
    mainLayout->addWidget(bridgeGroup, 0);

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
QToolButton* OrderCreateUIImpl::CreateTypeButton(QWidget* parent, const QString& text, const QString& code, bool checked) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString("OrderCreateType_%1_Button").arg(code));
    button->setText(text);
    button->setMinimumSize(74, 78);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(48, 48));
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("typeButton", true);
    button->setProperty("typeSelected", checked);
    button->setToolTip(text);
    button->setCursor(Qt::PointingHandCursor);

    // 治疗类型按钮资源来自当前软件的治疗方案选择资源。
    // 如果发布目录缺图，按钮仍保留文字，保证建单主流程不中断。
    const QString iconPath = TypeButtonIconPath(code, checked);
    if (QFileInfo::exists(iconPath)) {
        button->setIcon(QIcon(iconPath));
    }

    m_typeButtons.insert(code, button);

    QObject::connect(button, &QToolButton::clicked, [this, code]() {
        // 类型按钮只改变"后续点击牙位使用的类型"，不会 retroactively 修改已选牙位。
        SetCurrentType(code);
    });
    return button;
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

    // 先清空旧选择，防止默认示例牙位混入第三方方案。
    m_selectedTeeth.clear();
    m_toothTypeCodes.clear();
    m_selectedBridgeKeys.clear();

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
    }

    // 外部上下文如果传入 bridgeConnectors，则按统一 key 恢复已确认桥连接点。
    // 未传时只显示桥牙位，不默认确认连接点，避免第三方数据语义不明。
    const QJsonArray bridgeConnectors = scanPlanObject.value("bridgeConnectors").toArray();
    for (const QJsonValue& bridgeValue : bridgeConnectors) {
        const QString bridgeKey = bridgeValue.toString().trimmed();
        if (bridgeKey.isEmpty()) {
            // 空 key 没有业务含义，直接跳过，避免右侧摘要出现空白桥记录。
            continue;
        }

        const QStringList bridgeParts = bridgeKey.split('-');
        if (bridgeParts.size() != 2) {
            // 桥连接点必须是 "16-17" 这样的稳定格式；格式不对时忽略该条外部数据。
            WriteLog(LogLevel::Warning, "ApplyScanPlanItems", QString("Invalid bridge key: %1").arg(bridgeKey));
            continue;
        }

        bool firstOk = false;
        bool secondOk = false;
        const int firstTooth = bridgeParts.at(0).toInt(&firstOk);
        const int secondTooth = bridgeParts.at(1).toInt(&secondOk);
        if (!firstOk || !secondOk) {
            // 牙位号不是数字时无法和 m_toothTypeCodes 校验，必须跳过。
            WriteLog(LogLevel::Warning, "ApplyScanPlanItems", QString("Invalid bridge tooth number: %1").arg(bridgeKey));
            continue;
        }

        if (m_toothTypeCodes.value(firstTooth) != "bridge"
            || m_toothTypeCodes.value(secondTooth) != "bridge") {
            // 只有相邻两颗牙都确认为 bridge 类型，连接点才允许进入业务状态。
            // 这能避免第三方或旧数据传入孤立 bridgeConnectors 后，右侧桥记录显示假阳性。
            WriteLog(LogLevel::Warning, "ApplyScanPlanItems", QString("Bridge key ignored because teeth are not bridge: %1").arg(bridgeKey));
            continue;
        }

        // 内部统一保存小号牙位在前的 key。
        // 例如外部传入 "18-17" 时归一化为 "17-18"，否则 BuildBridgeRangeTexts 按稳定 key 聚合时会漏掉该连接点。
        const QString normalizedBridgeKey = QString("%1-%2")
            .arg(std::min(firstTooth, secondTooth))
            .arg(std::max(firstTooth, secondTooth));
        m_selectedBridgeKeys.insert(normalizedBridgeKey);
    }

    RefreshTreatmentPlanWidget();
    RefreshBridgeSummary();
}

// 应用外部传入的扫描流程配置。
// 该配置是对界面开关和咬合下拉框的标准 JSON 表达，适合第三方建单或后续规则服务直接填写。
void OrderCreateUIImpl::ApplyScanProcessConfig(const QJsonObject& scanProcessObject) {
    if (scanProcessObject.isEmpty()) {
        return;
    }

    // config 节点用于存放流程输入项；如果外部直接把字段放在 scanProcess 根节点，也兼容读取。
    const QJsonObject configObject = scanProcessObject.value("config").isObject()
        ? scanProcessObject.value("config").toObject()
        : scanProcessObject;

    if (m_maxillaDiffRodSwitch && configObject.contains("maxillaDiffRod")) {
        m_maxillaDiffRodSwitch->setChecked(configObject.value("maxillaDiffRod").toBool(false));
    }
    if (m_mandibleDiffRodSwitch && configObject.contains("mandibleDiffRod")) {
        m_mandibleDiffRodSwitch->setChecked(configObject.value("mandibleDiffRod").toBool(false));
    }
    if (m_maxillaSegmentedRodSwitch && configObject.contains("maxillaSegmentedRod")) {
        m_maxillaSegmentedRodSwitch->setChecked(configObject.value("maxillaSegmentedRod").toBool(false));
    }
    if (m_mandibleSegmentedRodSwitch && configObject.contains("mandibleSegmentedRod")) {
        m_mandibleSegmentedRodSwitch->setChecked(configObject.value("mandibleSegmentedRod").toBool(false));
    }

    if (m_occlusionTypeCombo) {
        const QString occlusionType = ReadString(configObject, "occlusionType", kOcclusionNatural);
        const int index = m_occlusionTypeCombo->findData(occlusionType);
        if (index >= 0) {
            m_occlusionTypeCombo->setCurrentIndex(index);
        }
    }
}

// 清理当前 QWidget 内部控件弱引用。
// 这些指针不拥有对象，只用于刷新 UI；外部删除 QWidget 后必须清空，避免下一次使用悬空地址。
void OrderCreateUIImpl::ResetWidgetReferences() {
    m_root = nullptr;
    m_typeButtons.clear();
    m_currentTypeSummaryLabel = nullptr;
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
    m_maxillaDiffRodSwitch = nullptr;
    m_mandibleDiffRodSwitch = nullptr;
    m_maxillaSegmentedRodSwitch = nullptr;
    m_mandibleSegmentedRodSwitch = nullptr;
    m_occlusionTypeCombo = nullptr;
    m_scanProcessPreviewLabel = nullptr;
    m_treatmentPlanWidget = nullptr;
    m_bridgeSummaryLabel = nullptr;
}

// 返回指定扫描类型显示名。
QString OrderCreateUIImpl::TypeText(const QString& typeCode) const {
    // 每个可见文本都显式写成 tr("English")，便于 lupdate 提取翻译源文案。
    if (typeCode == "implant") {
        return tr("Implant");
    }
    if (typeCode == "crown" || typeCode == "full_crown") {
        return tr("Full Crown");
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
    if (typeCode == "inner_crown") {
        return tr("Inner Crown");
    }
    if (typeCode == "bridge") {
        return tr("Bridge");
    }
    return tr("Unknown");
}

// 返回治疗类型按钮图标路径。
QString OrderCreateUIImpl::TypeButtonIconPath(const QString& typeCode, bool selected) const {
    // 治疗方案资源里约定 b 表示普通态，h 表示高亮态。
    // 这里集中维护编码到文件名的映射，后续替换资源时不用改按钮创建逻辑。
    QString iconName;
    if (typeCode == "implant") {
        iconName = selected ? "planting_h_2x.png" : "planting_b_2x.png";
    } else if (typeCode == "crown" || typeCode == "full_crown") {
        iconName = selected ? "an_full_crown_h_2x.png" : "an_full_crown_b_2x.png";
    } else if (typeCode == "missing") {
        iconName = selected ? "missing_h_2x.png" : "missing_b_2x.png";
    } else if (typeCode == "inlay") {
        iconName = selected ? "inlay_n_h_2x.png" : "inlay_n_b_2x.png";
    } else if (typeCode == "veneer") {
        iconName = selected ? "veneer_n_h_2x.png" : "veneer_n_b_2x.png";
    } else if (typeCode == "inner_crown") {
        iconName = selected ? "inner_crown_h_2x.png" : "inner_crown_b_2x.png";
    } else if (typeCode == "bridge") {
        iconName = selected ? "pontic_n_h_2x.png" : "pontic_n_b_2x.png";
    }

    if (iconName.isEmpty()) {
        return QString();
    }

    const QDir buttonDir(QDir(ResolveTreatmentPlanAssetRoot()).filePath("button"));
    return buttonDir.filePath(iconName);
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
    if (m_treatmentPlanWidget) {
        m_treatmentPlanWidget->SetCurrentTreatmentType(m_currentTypeCode);
    }
    if (m_currentTypeSummaryLabel) {
        // 左侧摘要显示当前工具类型，帮助用户确认"下一次点击牙位会使用什么治疗类型"。
        m_currentTypeSummaryLabel->setText(CurrentTypeText());
    }

    // 刷新所有类型按钮的选中视觉状态，保证同一时间只有一个类型高亮。
    for (auto it = m_typeButtons.begin(); it != m_typeButtons.end(); ++it) {
        const bool selected = (it.key() == typeCode);
        it.value()->setChecked(selected);
        it.value()->setProperty("typeSelected", selected);
        const QString iconPath = TypeButtonIconPath(it.key(), selected);
        if (QFileInfo::exists(iconPath)) {
            it.value()->setIcon(QIcon(iconPath));
        }
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }

    WriteLog(LogLevel::Info, "SetCurrentType", QString("Current type: %1").arg(typeCode));
}

// 切换牙位选择。
void OrderCreateUIImpl::ToggleTooth(int toothNumber) {
    if (m_currentTypeCode.isEmpty()) {
        WriteLog(LogLevel::Warning, "ToggleTooth", "No treatment type selected");
        return;
    }

    const QString oldType = m_toothTypeCodes.value(toothNumber);
    if (m_selectedTeeth.contains(toothNumber) && oldType == m_currentTypeCode) {
        // 已选牙位再次用同一类型点击表示移除。
        m_selectedTeeth.remove(toothNumber);
        // 移除牙位时同步移除类型，避免重新选择时读到旧类型。
        m_toothTypeCodes.remove(toothNumber);
        WriteLog(LogLevel::Info, "RemoveTooth", QString("Tooth removed: %1").arg(toothNumber));
    } else {
        // 未选牙位点击表示加入；已选牙位用不同类型点击表示修改治疗类型。
        m_selectedTeeth.insert(toothNumber);
        // 每颗牙位保存自己被加入时的类型，避免后续切换当前类型影响旧明细。
        m_toothTypeCodes.insert(toothNumber, m_currentTypeCode);
        WriteLog(LogLevel::Info,
                 "SetToothTreatment",
                 QString("Tooth treatment updated: %1, type: %2").arg(toothNumber).arg(m_currentTypeCode));
    }

    // 桥连接点依赖两颗相邻牙都保持 bridge 类型。
    // 牙位被移除或从 bridge 改成其它类型后，已确认连接点必须清理，否则表格会显示无效桥记录。
    const QSet<QString> oldBridgeKeys = m_selectedBridgeKeys;
    for (const QString& bridgeKey : oldBridgeKeys) {
        const QStringList parts = bridgeKey.split('-');
        if (parts.size() != 2) {
            m_selectedBridgeKeys.remove(bridgeKey);
            continue;
        }
        const int firstTooth = parts.at(0).toInt();
        const int secondTooth = parts.at(1).toInt();
        if (m_toothTypeCodes.value(firstTooth) != "bridge"
            || m_toothTypeCodes.value(secondTooth) != "bridge") {
            m_selectedBridgeKeys.remove(bridgeKey);
        }
    }

    // UI 状态变化后同步刷新右侧表格和中间牙弓。
    RefreshSelectionTable();
    RefreshTreatmentPlanWidget();
    RefreshBridgeSummary();
    // 牙位类型可能影响是否生成扫描杆流程，所以牙位变化后同步刷新扫描流程。
    RefreshScanProcessPreview(false);
    EmitAction(OrderCreateActionToothSelectionChanged);
}

// 清空所有牙位选择。
void OrderCreateUIImpl::ClearAllTeeth() {
    // 清空牙位集合；当前牙位视觉由 ToothTreatmentPlanWidget 根据集合快照重绘。
    m_selectedTeeth.clear();
    // 类型映射和牙位集合必须一起清空，保持 UI 状态一致。
    m_toothTypeCodes.clear();
    // 桥连接点依赖牙位，清空牙位时同步清空。
    m_selectedBridgeKeys.clear();

    // 表格清空后仍保留表头。
    RefreshSelectionTable();
    RefreshTreatmentPlanWidget();
    RefreshBridgeSummary();
    // 清空牙位后种植推导结果会变化，必须同步刷新扫描流程。
    RefreshScanProcessPreview(false);
    WriteLog(LogLevel::Info, "ClearAllTeeth", "All tooth selections cleared");
}

// 切换桥连接点选中状态。
void OrderCreateUIImpl::ToggleBridgeConnector(const QString& bridgeKey) {
    if (bridgeKey.trimmed().isEmpty()) {
        return;
    }

    if (m_selectedBridgeKeys.contains(bridgeKey)) {
        m_selectedBridgeKeys.remove(bridgeKey);
        WriteLog(LogLevel::Info, "RemoveBridgeConnector", QString("Bridge connector removed: %1").arg(bridgeKey));
    } else {
        m_selectedBridgeKeys.insert(bridgeKey);
        WriteLog(LogLevel::Info, "AddBridgeConnector", QString("Bridge connector added: %1").arg(bridgeKey));
    }

    RefreshSelectionTable();
    RefreshTreatmentPlanWidget();
    RefreshBridgeSummary();
    EmitAction(OrderCreateActionToothSelectionChanged);
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

// 刷新治疗方案图片控件。
void OrderCreateUIImpl::RefreshTreatmentPlanWidget() {
    if (!m_treatmentPlanWidget) {
        return;
    }

    // 图片控件只接受当前状态快照并重绘，不在内部改业务数据。
    m_treatmentPlanWidget->SetCurrentTreatmentType(m_currentTypeCode);
    m_treatmentPlanWidget->SetPlanState(m_toothTypeCodes, m_selectedBridgeKeys);
}

// 解析治疗方案资源目录。
QString OrderCreateUIImpl::ResolveTreatmentPlanAssetRoot() const {
    QStringList candidateRoots;

    // 第一优先级：调用方传入的 MeyerScan.exe 所在目录。
    if (!m_appDir.isEmpty()) {
        const QDir appDir(QString::fromUtf8(m_appDir));
        candidateRoots << appDir.filePath(kTreatmentPlanRuntimeRelativePath);
        // 历史兼容：老资源直接放在 exe 同级 icon 目录。
        candidateRoots << appDir.filePath(kTreatmentPlanLegacyRelativePath);
    }

    // 第二优先级：当前进程目录，适合 OrderCreateUITest.exe 独立运行。
    const QDir processDir(QCoreApplication::applicationDirPath());
    candidateRoots << processDir.filePath(kTreatmentPlanRuntimeRelativePath);
    candidateRoots << processDir.filePath(kTreatmentPlanLegacyRelativePath);

    // 第三优先级：模块源码 Resources 目录，便于开发环境尚未复制资源时仍能显示。
    // 模块测试宿主运行目录通常是 MyOrderCreateUI/bin/Release，所以先尝试 ../../Resources。
    candidateRoots << processDir.filePath(QString("../../%1").arg(kTreatmentPlanSourceRelativePath));
    // 主程序根输出目录通常是 F:/MeyerScan/bin/Release，所以再尝试 ../../MyOrderCreateUI/Resources。
    candidateRoots << processDir.filePath("../../MyOrderCreateUI/Resources/icon/createModule/sacanPlan");

    // 第四优先级：历史 bin 资源目录，避免尚未迁移干净的旧测试目录打不开。
    candidateRoots << processDir.filePath("../../MyOrderCreateUI/bin/Release/icon/createModule/sacanPlan");

    for (const QString& candidate : candidateRoots) {
        const QDir dir(QDir::cleanPath(candidate));
        if (QFileInfo::exists(dir.filePath("maxilla.png"))
            && QFileInfo::exists(dir.filePath("mandible.png"))
            && QFileInfo::exists(dir.filePath("maskMaxilla.png"))
            && QFileInfo::exists(dir.filePath("maskMandible.png"))) {
            return dir.absolutePath();
        }
    }

    // 都不存在时仍返回第一候选路径，日志能显示模块期望资源放在哪里。
    return QDir::cleanPath(candidateRoots.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : candidateRoots.first());
}

// 生成桥记录区间文本。
QStringList OrderCreateUIImpl::BuildBridgeRangeTexts() const {
    QStringList result;

    // 每一颌按牙弓连续顺序聚合桥连接点。
    // FDI 编号不是简单数值连续，尤其前牙跨中线时要按 12-11-21-22 这种顺序处理。
    const QList<QList<int>> jawOrders = QList<QList<int>>()
        << (QList<int>() << 18 << 17 << 16 << 15 << 14 << 13 << 12 << 11 << 21 << 22 << 23 << 24 << 25 << 26 << 27 << 28)
        << (QList<int>() << 48 << 47 << 46 << 45 << 44 << 43 << 42 << 41 << 31 << 32 << 33 << 34 << 35 << 36 << 37 << 38);

    for (const QList<int>& jawOrder : jawOrders) {
        int rangeStartIndex = -1;
        int rangeEndIndex = -1;

        for (int index = 0; index < jawOrder.size() - 1; ++index) {
            const int firstTooth = jawOrder.at(index);
            const int secondTooth = jawOrder.at(index + 1);
            const QString bridgeKey = QString("%1-%2")
                .arg(std::min(firstTooth, secondTooth))
                .arg(std::max(firstTooth, secondTooth));

            const bool selected = m_selectedBridgeKeys.contains(bridgeKey);
            if (selected) {
                if (rangeStartIndex < 0) {
                    rangeStartIndex = index;
                }
                rangeEndIndex = index;
            }

            // 遇到断点或最后一个连接点时，把当前连续区间输出。
            const bool atLastConnector = (index == jawOrder.size() - 2);
            if ((!selected || atLastConnector) && rangeStartIndex >= 0 && rangeEndIndex >= 0) {
                int displayFirstIndex = rangeStartIndex;
                int displaySecondIndex = rangeEndIndex + 1;

                // 跨中线桥连接在视频/旧软件记录中按中线后的治疗区间显示。
                // 例如用户明确要求 11-12 + 11-21 显示为 11-22，而不是机械端点 12-21。
                const bool crossesMaxillaMidline = jawOrder.contains(11)
                    && jawOrder.contains(21)
                    && rangeStartIndex <= 6
                    && rangeEndIndex >= 7;
                const bool crossesMandibleMidline = jawOrder.contains(41)
                    && jawOrder.contains(31)
                    && rangeStartIndex <= 7
                    && rangeEndIndex >= 7;
                if (crossesMaxillaMidline || crossesMandibleMidline) {
                    displayFirstIndex = std::min(rangeStartIndex + 1, jawOrder.size() - 1);
                    displaySecondIndex = std::min(rangeEndIndex + 2, jawOrder.size() - 1);
                }

                const int displayFirstTooth = jawOrder.at(displayFirstIndex);
                const int displaySecondTooth = jawOrder.at(displaySecondIndex);

                // 牙弓遍历方向可能是 18->17->16，但用户阅读和既有软件记录习惯是 16-18。
                // 因此最终输出时统一把较小端点放在前面，避免出现 18-16 这种反向记录。
                result << QString("%1-%2")
                    .arg(std::min(displayFirstTooth, displaySecondTooth))
                    .arg(std::max(displayFirstTooth, displaySecondTooth));
                rangeStartIndex = -1;
                rangeEndIndex = -1;
            }
        }
    }

    return result;
}

// 刷新桥记录摘要。
void OrderCreateUIImpl::RefreshBridgeSummary() {
    if (!m_bridgeSummaryLabel) {
        return;
    }

    const QStringList bridgeRanges = BuildBridgeRangeTexts();
    if (bridgeRanges.isEmpty()) {
        m_bridgeSummaryLabel->setText(tr("No bridge selected"));
    } else {
        m_bridgeSummaryLabel->setText(tr("Bridge: %1").arg(bridgeRanges.join(", ")));
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

// 创建扫描流程配置开关。
QCheckBox* OrderCreateUIImpl::CreateScanProcessSwitch(QWidget* parent,
                                                      const QString& text,
                                                      const QString& objectName) const {
    auto* checkBox = new QCheckBox(text, parent);
    checkBox->setObjectName(objectName);
    // 高度给到 32，保证点击区域可用，同时不挤压牙位区。
    checkBox->setMinimumHeight(32);
    // 文案宽度交给布局处理，多语言变长时优先换行/扩展。
    checkBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    return checkBox;
}

// 创建扫描流程配置区。
QWidget* OrderCreateUIImpl::CreateScanProcessConfigPanel(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName("OrderCreateScanProcessConfigPanel");

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Scan Process Rules"), frame);
    title->setObjectName("OrderCreateScanProcessTitle");
    layout->addWidget(title);

    auto* switchGrid = new QGridLayout();
    switchGrid->setHorizontalSpacing(12);
    switchGrid->setVerticalSpacing(4);

    m_maxillaDiffRodSwitch = CreateScanProcessSwitch(frame, tr("Maxilla special scanbody"), "OrderCreateMaxillaDiffRodSwitch");
    m_mandibleDiffRodSwitch = CreateScanProcessSwitch(frame, tr("Mandible special scanbody"), "OrderCreateMandibleDiffRodSwitch");
    m_maxillaSegmentedRodSwitch = CreateScanProcessSwitch(frame, tr("Maxilla segmented scanbody"), "OrderCreateMaxillaSegmentedRodSwitch");
    m_mandibleSegmentedRodSwitch = CreateScanProcessSwitch(frame, tr("Mandible segmented scanbody"), "OrderCreateMandibleSegmentedRodSwitch");

    switchGrid->addWidget(m_maxillaDiffRodSwitch, 0, 0);
    switchGrid->addWidget(m_mandibleDiffRodSwitch, 0, 1);
    switchGrid->addWidget(m_maxillaSegmentedRodSwitch, 1, 0);
    switchGrid->addWidget(m_mandibleSegmentedRodSwitch, 1, 1);
    layout->addLayout(switchGrid);

    auto* occlusionRow = new QHBoxLayout();
    occlusionRow->setSpacing(8);
    occlusionRow->addWidget(CreateStandardFieldLabel(frame, tr("Occlusion Type")), 0);

    m_occlusionTypeCombo = CreateStandardComboBox(frame);
    m_occlusionTypeCombo->setObjectName("OrderCreateOcclusionTypeCombo");
    m_occlusionTypeCombo->addItem(tr("Natural occlusion"), QString::fromLatin1(kOcclusionNatural));
    m_occlusionTypeCombo->addItem(tr("Maxilla temporary occlusion"), QString::fromLatin1(kOcclusionMaxillaTemporary));
    m_occlusionTypeCombo->addItem(tr("Mandible temporary occlusion"), QString::fromLatin1(kOcclusionMandibleTemporary));
    m_occlusionTypeCombo->addItem(tr("Full temporary occlusion"), QString::fromLatin1(kOcclusionFullTemporary));
    m_occlusionTypeCombo->addItem(tr("Bite record"), QString::fromLatin1(kOcclusionRecord));
    occlusionRow->addWidget(m_occlusionTypeCombo, 1);
    layout->addLayout(occlusionRow);

    m_scanProcessPreviewLabel = new QLabel(frame);
    m_scanProcessPreviewLabel->setObjectName("OrderCreateScanProcessPreview");
    m_scanProcessPreviewLabel->setWordWrap(true);
    layout->addWidget(m_scanProcessPreviewLabel);

    const QList<QCheckBox*> switches = QList<QCheckBox*>()
        << m_maxillaDiffRodSwitch
        << m_mandibleDiffRodSwitch
        << m_maxillaSegmentedRodSwitch
        << m_mandibleSegmentedRodSwitch;
    for (QCheckBox* checkBox : switches) {
        QObject::connect(checkBox, &QCheckBox::toggled, [this]() {
            RefreshScanProcessPreview(true);
        });
    }
    QObject::connect(m_occlusionTypeCombo,
                     static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     [this](int) {
                         RefreshScanProcessPreview(true);
                     });

    return frame;
}

// 根据当前建单状态生成扫描流程配置对象。
QJsonObject OrderCreateUIImpl::BuildScanProcessConfigObject() const {
    QJsonObject config;
    config.insert("maxillaDiffRod", m_maxillaDiffRodSwitch ? m_maxillaDiffRodSwitch->isChecked() : false);
    config.insert("mandibleDiffRod", m_mandibleDiffRodSwitch ? m_mandibleDiffRodSwitch->isChecked() : false);
    config.insert("maxillaSegmentedRod", m_maxillaSegmentedRodSwitch ? m_maxillaSegmentedRodSwitch->isChecked() : false);
    config.insert("mandibleSegmentedRod", m_mandibleSegmentedRodSwitch ? m_mandibleSegmentedRodSwitch->isChecked() : false);
    config.insert("occlusionType", CurrentOcclusionTypeCode());
    config.insert("maxillaHasImplant", HasImplantTooth(true));
    config.insert("mandibleHasImplant", HasImplantTooth(false));
    config.insert("maxillaTemporary", IsJawTemporary(true));
    config.insert("mandibleTemporary", IsJawTemporary(false));

    // scanPlan 是当前建单页面的治疗方案快照。
    // MainExe/Scan/Process 当前只消费 steps；后续保存订单时可把 scanPlan 转交 ScanSchemaService。
    QJsonArray items;
    QList<int> teeth = m_selectedTeeth.values();
    std::sort(teeth.begin(), teeth.end());
    for (int toothNumber : teeth) {
        QJsonObject item;
        item.insert("tooth", toothNumber);
        item.insert("type", m_toothTypeCodes.value(toothNumber, "crown"));
        items.append(item);
    }

    QJsonArray bridgeConnectors;
    QStringList bridgeKeys = m_selectedBridgeKeys.values();
    std::sort(bridgeKeys.begin(), bridgeKeys.end());
    for (const QString& bridgeKey : bridgeKeys) {
        bridgeConnectors.append(bridgeKey);
    }

    QJsonArray bridgeRanges;
    const QStringList bridgeRangeTexts = BuildBridgeRangeTexts();
    for (const QString& bridgeRange : bridgeRangeTexts) {
        bridgeRanges.append(bridgeRange);
    }

    QJsonObject scanPlan;
    scanPlan.insert("items", items);
    scanPlan.insert("bridgeConnectors", bridgeConnectors);
    scanPlan.insert("bridgeRanges", bridgeRanges);
    config.insert("scanPlan", scanPlan);
    return config;
}

// 根据配置生成扫描按钮步骤。
QJsonArray OrderCreateUIImpl::BuildScanProcessSteps(const QJsonObject& configObject) const {
    QJsonArray steps;
    const bool maxillaDiffRod = configObject.value("maxillaDiffRod").toBool(false);
    const bool mandibleDiffRod = configObject.value("mandibleDiffRod").toBool(false);
    const bool maxillaSegmented = configObject.value("maxillaSegmentedRod").toBool(false);
    const bool mandibleSegmented = configObject.value("mandibleSegmentedRod").toBool(false);
    const bool maxillaImplant = configObject.value("maxillaHasImplant").toBool(false);
    const bool mandibleImplant = configObject.value("mandibleHasImplant").toBool(false);
    const bool maxillaTemporary = configObject.value("maxillaTemporary").toBool(false);
    const bool mandibleTemporary = configObject.value("mandibleTemporary").toBool(false);
    const QString occlusionType = configObject.value("occlusionType").toString(kOcclusionNatural);

    if (maxillaDiffRod) {
        if (maxillaTemporary) {
            AppendScanProcessStep(&steps, "maxilla", "maxilla_natural", tr("Natural maxilla"));
        }
        AppendScanProcessStep(&steps, "maxilla", "maxilla_diff_rod_1", tr("Maxilla special scanbody 1"));
        if (maxillaSegmented) {
            AppendScanProcessStep(&steps, "maxilla", "maxilla_diff_rod_2", tr("Maxilla special scanbody 2"));
        }
        AppendScanProcessStep(&steps, "maxilla", "maxilla_cuff", tr("Maxilla cuff"));
    } else {
        AppendScanProcessStep(&steps, "maxilla", "maxilla_natural", tr("Natural maxilla"));
        // "扫描杆分段"只表示第二扫描杆是否显示，不代表该颌一定存在种植扫描杆。
        // 因此普通扫描杆流程仍然由牙位里的 implant 类型触发，避免用户只勾选分段时凭空生成扫描杆按钮。
        if (maxillaImplant) {
            if (maxillaTemporary) {
                AppendScanProcessStep(&steps, "maxilla", "maxilla_cuff", tr("Maxilla cuff"));
            }
            AppendScanProcessStep(&steps, "maxilla", "maxilla_scanbody_1", tr("Maxilla scanbody 1"));
            if (maxillaSegmented) {
                AppendScanProcessStep(&steps, "maxilla", "maxilla_scanbody_2", tr("Maxilla scanbody 2"));
            }
        }
    }

    const bool needExchange = !maxillaDiffRod
        && !mandibleDiffRod
        && !maxillaSegmented
        && !mandibleSegmented
        && !maxillaTemporary
        && !mandibleTemporary;
    if (needExchange) {
        AppendScanProcessStep(&steps, "exchange", "data_exchange", tr("Exchange"), true);
    }

    if (mandibleDiffRod) {
        if (mandibleTemporary) {
            AppendScanProcessStep(&steps, "mandible", "mandible_natural", tr("Natural mandible"));
        }
        AppendScanProcessStep(&steps, "mandible", "mandible_diff_rod_1", tr("Mandible special scanbody 1"));
        if (mandibleSegmented) {
            AppendScanProcessStep(&steps, "mandible", "mandible_diff_rod_2", tr("Mandible special scanbody 2"));
        }
        AppendScanProcessStep(&steps, "mandible", "mandible_cuff", tr("Mandible cuff"));
    } else {
        AppendScanProcessStep(&steps, "mandible", "mandible_natural", tr("Natural mandible"));
        // 下颌规则与上颌保持完全对称：分段开关只控制第二扫描杆，不单独创建种植扫描流程。
        if (mandibleImplant) {
            if (mandibleTemporary) {
                AppendScanProcessStep(&steps, "mandible", "mandible_cuff", tr("Mandible cuff"));
            }
            AppendScanProcessStep(&steps, "mandible", "mandible_scanbody_1", tr("Mandible scanbody 1"));
            if (mandibleSegmented) {
                AppendScanProcessStep(&steps, "mandible", "mandible_scanbody_2", tr("Mandible scanbody 2"));
            }
        }
    }

    if (occlusionType == kOcclusionRecord) {
        AppendScanProcessStep(&steps, "occlusion", "bite_record", tr("Bite record"));
    } else {
        AppendScanProcessStep(&steps, "occlusion", "natural_occlusion", OcclusionTypeText(occlusionType));
    }

    return steps;
}

// 追加一条扫描流程步骤。
void OrderCreateUIImpl::AppendScanProcessStep(QJsonArray* steps,
                                              const QString& part,
                                              const QString& code,
                                              const QString& label,
                                              bool exchangeStep) const {
    if (!steps) {
        return;
    }

    QJsonObject item;
    item.insert("part", part);
    item.insert("code", code);
    item.insert("label", label);
    item.insert("exchange", exchangeStep);
    item.insert("enabled", true);
    steps->append(item);
}

// 判断当前方案是否包含某一颌的种植牙位。
bool OrderCreateUIImpl::HasImplantTooth(bool maxilla) const {
    for (auto it = m_toothTypeCodes.begin(); it != m_toothTypeCodes.end(); ++it) {
        const int tooth = it.key();
        const bool toothInMaxilla = tooth >= 11 && tooth <= 28;
        const bool toothInMandible = tooth >= 31 && tooth <= 48;
        if (it.value() == "implant" && ((maxilla && toothInMaxilla) || (!maxilla && toothInMandible))) {
            return true;
        }
    }
    return false;
}

// 判断某一颌是否按临时牙逻辑处理。
bool OrderCreateUIImpl::IsJawTemporary(bool maxilla) const {
    const QString occlusionType = CurrentOcclusionTypeCode();
    if (occlusionType == kOcclusionFullTemporary) {
        return true;
    }
    if (maxilla) {
        return occlusionType == kOcclusionMaxillaTemporary;
    }
    return occlusionType == kOcclusionMandibleTemporary;
}

// 读取当前咬合类型编码。
QString OrderCreateUIImpl::CurrentOcclusionTypeCode() const {
    if (!m_occlusionTypeCombo) {
        return kOcclusionNatural;
    }
    const QString code = m_occlusionTypeCombo->currentData().toString();
    return code.isEmpty() ? QString(kOcclusionNatural) : code;
}

// 咬合类型编码转显示文本。
QString OrderCreateUIImpl::OcclusionTypeText(const QString& code) const {
    if (code == kOcclusionMaxillaTemporary) {
        return tr("Maxilla temporary occlusion");
    }
    if (code == kOcclusionMandibleTemporary) {
        return tr("Mandible temporary occlusion");
    }
    if (code == kOcclusionFullTemporary) {
        return tr("Full temporary occlusion");
    }
    if (code == kOcclusionRecord) {
        return tr("Bite record");
    }
    return tr("Natural occlusion");
}

// 统一刷新扫描流程 JSON 和预览。
void OrderCreateUIImpl::RefreshScanProcessPreview(bool emitAction) {
    const QJsonObject config = BuildScanProcessConfigObject();
    const QJsonArray steps = BuildScanProcessSteps(config);

    QJsonObject scanProcess;
    scanProcess.insert("schemaVersion", 1);
    scanProcess.insert("source", "OrderCreateUI");
    scanProcess.insert("config", config);
    scanProcess.insert("steps", steps);
    m_currentScanProcessJson = QJsonDocument(scanProcess).toJson(QJsonDocument::Compact);

    if (m_scanProcessPreviewLabel) {
        QStringList labels;
        for (const QJsonValue& value : steps) {
            const QJsonObject item = value.toObject();
            labels << item.value("label").toString();
        }
        m_scanProcessPreviewLabel->setText(tr("Process: %1").arg(labels.join(tr(" -> "))));
    }

    WriteLog(LogLevel::Info,
             "RefreshScanProcessPreview",
             QString("Scan process refreshed, steps=%1").arg(steps.size()));
    if (emitAction) {
        EmitAction(OrderCreateActionScanProcessChanged);
    }
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
