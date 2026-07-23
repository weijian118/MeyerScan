#include "OrderCreateUIInternal.h"


// 创建左侧患者和订单基本信息区。
QWidget* OrderCreateUIImpl::CreateBasicInfoPanel(QWidget* parent) {
    auto* outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(12, 14, 12, 12);
    outerLayout->setSpacing(8);

    // 患者编号由上层建单流程生成，初版设为只读，避免用户误改主键类字段。
    m_patientIdEdit = CreateStandardLineEdit(parent);
    m_patientIdEdit->setReadOnly(true);
    m_patientIdEdit->setObjectName("OrderCreatePatientIdEdit");
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Patient ID")));
    outerLayout->addWidget(m_patientIdEdit);

    // 患者姓名是建单核心字段。textChanged 信号用于立即刷新右侧摘要。
    m_patientNameEdit = CreateStandardLineEdit(parent);
    m_patientNameEdit->setObjectName("OrderCreatePatientNameEdit");
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
    m_ageEdit = CreateStandardLineEdit(parent);
    m_ageEdit->setObjectName("OrderCreateAgeEdit");
    ageBox->addWidget(CreateStandardFieldLabel(parent, tr("Age")));
    ageBox->addWidget(m_ageEdit);

    auto* birthBox = new QVBoxLayout();
    m_birthDateEdit = CreateStandardDateEdit(parent);
    m_birthDateEdit->setObjectName("OrderCreateBirthDateEdit");
    m_birthDateEdit->setDisplayFormat("yyyy/MM/dd");
    // QDateEdit 没有真正的空值；把最小日期作为占位哨兵，并用可翻译文字显示“尚未选择”。
    m_birthDateEdit->setMinimumDate(QDate(1900, 1, 1));
    m_birthDateEdit->setSpecialValueText(tr("Select date"));
    m_birthDateEdit->setDate(m_birthDateEdit->minimumDate());
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
    // 当前只放一个无业务 ID 的占位项；真实医生列表后续由 RuntimeDataCenter 注入。
    m_doctorCombo->addItem(tr("Select doctor"));
    QObject::connect(m_doctorCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int) {
        // 医生变化会影响右侧摘要，但不会在 UI 层直接写库。
        RefreshBasicSummary();
        WriteLog(LogLevel::Info, "SelectDoctor", "Doctor changed");
    });
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Doctor *")));
    outerLayout->addWidget(m_doctorCombo);

    m_orderIdEdit = CreateStandardLineEdit(parent);
    m_orderIdEdit->setObjectName("OrderCreateOrderIdEdit");
    m_orderIdEdit->setReadOnly(true);
    outerLayout->addWidget(CreateStandardFieldLabel(parent, tr("Order ID")));
    outerLayout->addWidget(m_orderIdEdit);

    m_labCombo = CreateStandardComboBox(parent);
    m_labCombo->setObjectName("OrderCreateLabCombo");
    // 不在生产 UI 硬编码技工所测试名称，避免客户误把演示项当成真实合作方。
    m_labCombo->addItem(tr("Select lab"));
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
    // 当前软件截图中治疗类型是一张独立卡片：首行四个常用类型，第二行是宽种植体按钮。
    // 本函数只负责控件创建；真正的选中状态刷新集中在 SetCurrentType。
    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(12, 14, 12, 12);
    layout->setSpacing(10);

    // 四列顺序与当前软件一致；按钮 tooltip 保留完整翻译，窄语言可通过 qm 使用短显示词。
    auto* typeGrid = new QGridLayout();
    typeGrid->setHorizontalSpacing(8);
    typeGrid->setVerticalSpacing(8);

    // 每个按钮都是单选入口；checked 由当前编码决定，避免默认类型和按钮高亮不同步。
    typeGrid->addWidget(CreateTypeButton(parent, tr("Full Crown"), "crown", m_currentTypeCode == "crown"), 0, 0);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Missing Tooth"), "missing", m_currentTypeCode == "missing"), 0, 1);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Inlay"), "inlay", m_currentTypeCode == "inlay"), 0, 2);
    typeGrid->addWidget(CreateTypeButton(parent, tr("Veneer"), "veneer", m_currentTypeCode == "veneer"), 0, 3);

    QToolButton* implantButton = CreateTypeButton(parent, tr("Implant"), "implant", m_currentTypeCode == "implant");
    implantButton->setProperty("wideTypeButton", true);
    implantButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    implantButton->setIconSize(QSize(24, 24));
    implantButton->setMinimumSize(120, 46);
    implantButton->setMaximumHeight(50);
    implantButton->style()->unpolish(implantButton);
    implantButton->style()->polish(implantButton);
    typeGrid->addWidget(implantButton, 1, 0, 1, 4);
    typeGrid->setColumnStretch(0, 1);
    typeGrid->setColumnStretch(1, 1);
    typeGrid->setColumnStretch(2, 1);
    typeGrid->setColumnStretch(3, 1);
    layout->addLayout(typeGrid);

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
            bool confirmed = false;
            if (m_showDecisionDialog) {
                // 公共弹窗只负责视觉和按钮返回值；是否清空仍由建单模块决定。
                const QByteArray titleBytes = tr("Warning").toUtf8();
                const QByteArray messageBytes = tr("Are you sure to clear all selections?").toUtf8();
                const QByteArray confirmBytes = tr("Confirm").toUtf8();
                const QByteArray cancelBytes = tr("Cancel").toUtf8();
                confirmed = m_showDecisionDialog(MeyerDecisionDialogWarning,
                                                  titleBytes.constData(),
                                                  messageBytes.constData(),
                                                  confirmBytes.constData(),
                                                  cancelBytes.constData(),
                                                  m_root) == MeyerDialogAccepted;
            } else {
                // 发布目录缺少新版 UIComponents 时仍保留功能完整的 Qt 标准弹窗降级路径。
                QMessageBox messageBox(m_root);
                messageBox.setIcon(QMessageBox::Question);
                messageBox.setWindowTitle(tr("Warning"));
                messageBox.setText(tr("Are you sure to clear all selections?"));
                QPushButton* confirmButton = messageBox.addButton(tr("Confirm"), QMessageBox::AcceptRole);
                messageBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
                messageBox.exec();
                confirmed = messageBox.clickedButton() == confirmButton;
            }
            if (!confirmed) {
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
    summaryGroup->setProperty("subsection", true);
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
    bridgeGroup->setProperty("subsection", true);
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
    shadeGroup->setProperty("subsection", true);
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

    // 底部操作区使用两行网格。
    // 右栏在 1366x768 下只有约 320px，四个按钮横排会被压缩；两行布局无需分辨率分支即可保持文字完整。
    auto* actionGrid = new QGridLayout();
    actionGrid->setHorizontalSpacing(8);
    actionGrid->setVerticalSpacing(8);
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

    actionGrid->addWidget(previousButton, 0, 0);
    actionGrid->addWidget(cancelButton, 0, 1);
    actionGrid->addWidget(confirmButton, 1, 0);
    actionGrid->addWidget(nextButton, 1, 1);
    actionGrid->setColumnStretch(0, 1);
    actionGrid->setColumnStretch(1, 1);
    mainLayout->addLayout(actionGrid);

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

// 创建治疗类型按钮。
QToolButton* OrderCreateUIImpl::CreateTypeButton(QWidget* parent, const QString& text, const QString& code, bool checked) {
    auto* button = new TreatmentTypeButton(parent);
    button->setObjectName(QString("OrderCreateType_%1_Button").arg(code));
    button->setText(text);
    // 82px 能完整容纳英文 "Missing Tooth"；更长翻译仍可在 qm 中采用业务认可的短标签。
    button->setMinimumSize(82, 78);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    // 资源普通态为 40x40、2x 为 80x80；两者都按 40 逻辑像素显示，仅清晰度不同。
    button->setIconSize(QSize(40, 40));
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("typeButton", true);
    button->setProperty("typeSelected", checked);
    // 把稳定的业务类型编码交给 QSS，样式层即可分别设置五种类型的文字色和种植体宽按钮状态。
    // 这里传编码而不是中文显示文字，避免语言切换后 QSS 选择器失效。
    button->setProperty("treatmentCode", code);
    button->setToolTip(text);
    button->setCursor(Qt::PointingHandCursor);

    // 2K/4K 使用 80x80 的 2x 源图，1920x1080 等屏幕使用 40x40 源图。
    // 按钮子类在 enter/leave 时切换 b/h 图，避免只改变背景而图标不变。
    const bool highResolution = UseHighResolutionTreatmentIcons(parent);
    button->SetIconPaths(TypeButtonIconPath(code, false, highResolution),
                         TypeButtonIconPath(code, true, highResolution),
                         code != "implant");
    button->SetTypeSelected(checked);

    m_typeButtons.insert(code, button);

    QObject::connect(button, &QToolButton::clicked, [this, code]() {
        // 类型按钮只改变"后续点击牙位使用的类型"，不会 retroactively 修改已选牙位。
        SetCurrentType(code);
    });
    return button;
}
