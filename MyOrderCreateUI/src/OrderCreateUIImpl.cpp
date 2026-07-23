#include "OrderCreateUIInternal.h"

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
        if (!m_logger->Init(m_logDir.constData(), LogLevel::Info)) {
            // 建单主流程允许无日志降级，但不能保留半初始化日志接口。
            m_logger = nullptr;
        }
    }

    // UIComponents 只负责通用控件的视觉统一；建单字段、牙位状态和动作回调仍留在本模块。
    LoadUIComponents();

    // 扫描步骤生成属于业务规则，必须由 ScanSchemaService 统一处理。
    LoadScanSchemaService();
    if (!m_scanSchemaService) {
        WriteLog(LogLevel::Error, "Init", "ScanSchemaService is required but unavailable");
        return false;
    }

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

    // QSplitter 让三栏在不同显示器宽度下按比例伸缩，也允许用户临时调整信息区宽度。
    // 这比按 1920x1080 坐标乘系数更稳定，多语言文本变长时也能获得额外空间。
    auto* workspaceSplitter = new QSplitter(Qt::Horizontal, root);
    workspaceSplitter->setObjectName("OrderCreateWorkspaceSplitter");
    workspaceSplitter->setChildrenCollapsible(false);
    pageLayout->addWidget(workspaceSplitter, 1);

    // 左侧工作区按视频组织：上方选择治疗类型，下方显示基础信息。
    // 这样治疗类型不再占用中间牙弓区域，牙弓在 1920x1080 和高分屏下都能成为主视觉。
    QWidget* leftPanel = CreateLeftWorkflowPanel(root);
    // 四个治疗类型按钮在英文等较长语言下需要稳定宽度；380px 可覆盖 1366 宽屏且不裁字。
    leftPanel->setMinimumWidth(380);
    leftPanel->setMaximumWidth(460);
    workspaceSplitter->addWidget(leftPanel);

    // 中间分栏宿主可以继续随窗口扩展，但真正的 Scan Plan 内容限制最大宽度并水平居中。
    // 这样 2K/4K 的额外空间留在背景中，不会把牙弓卡片拉成一大片空白区域。
    auto* toothPanelHost = new QWidget(root);
    toothPanelHost->setObjectName("OrderCreateToothPlanHost");
    toothPanelHost->setMinimumWidth(460);
    auto* toothPanelHostLayout = new QHBoxLayout(toothPanelHost);
    toothPanelHostLayout->setContentsMargins(0, 0, 0, 0);
    QWidget* toothPanel = CreateToothPlanPanel(root);
    toothPanel->setMinimumWidth(460);
    toothPanel->setMaximumWidth(980);
    // 2K/4K 只增加工作区留白，不把 Scan Plan 卡片纵向拉到整屏高度；1060px 接近 1920x1080 基准内容高度。
    // 同时使用水平/垂直居中，让额外空间留在无边框宿主背景中，而不是被牙弓内部上下两半平均分走。
    toothPanel->setMaximumHeight(1060);
    toothPanelHostLayout->addWidget(toothPanel, 1, Qt::AlignHCenter | Qt::AlignVCenter);
    workspaceSplitter->addWidget(toothPanelHost);

    // 右侧摘要区宽度和左侧相近，便于用户边选牙位边确认明细。
    QWidget* summaryPanel = CreateOrderSummaryPanel(root);
    summaryPanel->setMinimumWidth(320);
    summaryPanel->setMaximumWidth(460);
    workspaceSplitter->addWidget(summaryPanel);

    // 使用约 1:2:1 的伸缩权重，让 Scan Plan 保持居中主视觉，同时比旧 360/900/360 略窄。
    // 左右栏仍受 460px 上限保护，因此 2K/4K 下不会无限放大表单。
    workspaceSplitter->setStretchFactor(0, 1);
    workspaceSplitter->setStretchFactor(1, 2);
    workspaceSplitter->setStretchFactor(2, 1);
    workspaceSplitter->setSizes(QList<int>() << 390 << 760 << 390);

    // 保存弱引用，不接管 root 生命周期，真实释放由 Qt 父子关系或调用方完成。
    m_root = root;

    if (m_hasPendingContext && !m_pendingContextJson.isEmpty()) {
        // 如果 MainExe 或第三方流程在页面创建前已经传入标准上下文，
        // 页面创建完成后立即应用，避免客户看到默认数据再跳变成第三方数据。
        const QByteArray pendingContextCopy = m_pendingContextJson;
        if (!SetOrderContextJson(pendingContextCopy.constData())) {
            // pendingContext 正常情况下已经验证过；这里仍检查返回值，防止以后增加 schema 校验时漏掉失败。
            WriteLog(LogLevel::Error,
                     "CreateWidget",
                     "Validated pending order context could not be applied");
        }
    } else {
        // 生产 DLL 没有上下文时必须显示空白建单状态；演示患者和牙位只允许由测试宿主传入。
        m_selectedTeeth.clear();
        m_toothTypeCodes.clear();
        m_selectedBridgeKeys.clear();
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

    // 先把调用方字节复制到候选缓冲区，但此时不覆盖上一份有效 pendingContext。
    // 这样非法 JSON 失败后，CreateWidget 仍会应用此前已经验证的标准上下文。
    const QByteArray candidateContextJson(contextJsonUtf8);
    QJsonParseError parseError;
    // Qt JSON 解析器直接消费 UTF-8 QByteArray。
    // 返回的 QJsonDocument 拥有自己的内部数据，不依赖 contextJsonUtf8 指针生命周期。
    const QJsonDocument document = QJsonDocument::fromJson(candidateContextJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteLog(LogLevel::Warning,
                 "SetOrderContextJson",
                 QString("Invalid context json: %1").arg(parseError.errorString()));
        return false;
    }

    // 只有完整 JSON 解析成功后才提交候选缓存，形成“校验成功后一次替换”的事务式更新。
    m_pendingContextJson = candidateContextJson;
    m_hasPendingContext = true;

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

    // 扫描方案中的 items 会更新当前牙位选择。
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

// 返回用户当前编辑后的完整建单上下文。
// UI 只负责导出表单快照；订单 ID 生成、权限复核和数据库保存仍由 MainExe/CaseOrderService 完成。
const char* OrderCreateUIImpl::GetCurrentOrderContextJson() {
    // 先从最近一次有效输入复制来源信息和未来扩展字段，再覆盖当前可编辑字段。
    QJsonObject root;
    if (m_hasPendingContext) {
        const QJsonDocument pendingDocument = QJsonDocument::fromJson(m_pendingContextJson);
        if (pendingDocument.isObject()) {
            root = pendingDocument.object();
        }
    }

    // 患者对象使用稳定业务字段，不暴露任何数据库表名。
    QJsonObject patient = root.value("patient").toObject();
    patient.insert("patientId", m_patientIdEdit ? m_patientIdEdit->text().trimmed() : QString());
    patient.insert("name", m_patientNameEdit ? m_patientNameEdit->text().trimmed() : QString());
    patient.insert("age", m_ageEdit ? m_ageEdit->text().trimmed() : QString());
    if (m_birthDateEdit && m_birthDateEdit->date() > m_birthDateEdit->minimumDate()) {
        patient.insert("birthDate", m_birthDateEdit->date().toString("yyyy/MM/dd"));
    }
    QString gender = "unknown";
    if (m_genderMale && m_genderMale->isChecked()) {
        gender = "male";
    } else if (m_genderFemale && m_genderFemale->isChecked()) {
        gender = "female";
    }
    patient.insert("gender", gender);
    patient.insert("contact", m_contactEdit ? m_contactEdit->text().trimmed() : QString());
    patient.insert("note", m_patientNoteEdit ? m_patientNoteEdit->toPlainText() : QString());

    // 订单对象保存页面当前值。ID 可以为空，由宿主工作流在正式保存前生成。
    QJsonObject order = root.value("order").toObject();
    order.insert("orderId", m_orderIdEdit ? m_orderIdEdit->text().trimmed() : QString());
    order.insert("doctor", m_doctorCombo ? m_doctorCombo->currentText().trimmed() : QString());
    order.insert("lab", m_labCombo ? m_labCombo->currentText().trimmed() : QString());
    order.insert("deliveryDate", m_deliveryDateEdit ? m_deliveryDateEdit->date().toString("yyyy/MM/dd") : QString());
    order.insert("note", m_orderNoteEdit ? m_orderNoteEdit->toPlainText() : QString());

    // 扫描配置包含牙位治疗方案；扫描流程由 ScanSchemaService 生成稳定步骤编码。
    const QJsonObject scanConfig = BuildScanProcessConfigObject();
    RefreshScanProcessPreview(false);
    const QJsonDocument scanProcessDocument = QJsonDocument::fromJson(m_currentScanProcessJson);
    root.insert("schemaVersion", root.value("schemaVersion").toInt(1));
    root.insert("patient", patient);
    root.insert("order", order);
    root.insert("scanPlan", scanConfig.value("scanPlan").toObject());
    root.insert("scanProcess", scanProcessDocument.isObject() ? scanProcessDocument.object() : QJsonObject());

    // 缓存紧凑 UTF-8 文本，跨 DLL 只返回 const char*，不传递 QJsonObject/QString 所有权。
    m_currentOrderContextJson = QJsonDocument(root).toJson(QJsonDocument::Compact);
    WriteLog(LogLevel::Info,
             "GetCurrentOrderContextJson",
             QString("Order context json bytes: %1").arg(m_currentOrderContextJson.size()));
    return m_currentOrderContextJson.constData();
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
    m_currentOrderContextJson.clear();
    m_uiComponents = nullptr;
    m_showDecisionDialog = nullptr;
    // ScanSchemaService 是模块单例，本 UI 只清借用指针，不关闭可能被其它调用方使用的服务。
    m_scanSchemaService = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}
