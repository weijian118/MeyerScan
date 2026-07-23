#include "SettingsUIInternal.h"

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
