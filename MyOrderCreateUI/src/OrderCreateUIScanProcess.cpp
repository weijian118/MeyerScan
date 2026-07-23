#include "OrderCreateUIInternal.h"

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
    // 扫描步骤会因种植体、分段扫描杆等输入而变长。若允许 QLabel 自动换行，标签会增高并向上挤压牙弓，
    // 用户就会看到“选择种植体后牙位图缩小”。固定为单行高度后，业务状态变化只改文字，不改页面几何。
    m_scanProcessPreviewLabel->setWordWrap(false);
    m_scanProcessPreviewLabel->setTextFormat(Qt::PlainText);
    m_scanProcessPreviewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_scanProcessPreviewLabel->setFixedHeight(m_scanProcessPreviewLabel->fontMetrics().lineSpacing() + 10);
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

// 读取当前咬合类型编码。
QString OrderCreateUIImpl::CurrentOcclusionTypeCode() const {
    if (!m_occlusionTypeCombo) {
        return kOcclusionNatural;
    }
    const QString code = m_occlusionTypeCombo->currentData().toString();
    return code.isEmpty() ? QString(kOcclusionNatural) : code;
}

// 把稳定步骤编码转换为当前语言显示文本。
QString OrderCreateUIImpl::ScanStepDisplayText(const QString& code) const {
    if (code == "maxilla_natural") return tr("Natural maxilla");
    if (code == "maxilla_diff_rod_1") return tr("Maxilla special scanbody 1");
    if (code == "maxilla_diff_rod_2") return tr("Maxilla special scanbody 2");
    if (code == "maxilla_cuff") return tr("Maxilla cuff");
    if (code == "maxilla_scanbody_1") return tr("Maxilla scanbody 1");
    if (code == "maxilla_scanbody_2") return tr("Maxilla scanbody 2");
    if (code == "data_exchange") return tr("Exchange");
    if (code == "mandible_natural") return tr("Natural mandible");
    if (code == "mandible_diff_rod_1") return tr("Mandible special scanbody 1");
    if (code == "mandible_diff_rod_2") return tr("Mandible special scanbody 2");
    if (code == "mandible_cuff") return tr("Mandible cuff");
    if (code == "mandible_scanbody_1") return tr("Mandible scanbody 1");
    if (code == "mandible_scanbody_2") return tr("Mandible scanbody 2");
    if (code == "bite_record") return tr("Bite record");
    if (code == "natural_occlusion") return tr("Natural occlusion");
    return code;
}

// 统一刷新扫描流程 JSON 和预览。
void OrderCreateUIImpl::RefreshScanProcessPreview(bool emitAction) {
    const QJsonObject config = BuildScanProcessConfigObject();
    const QByteArray configJson = QJsonDocument(config).toJson(QJsonDocument::Compact);
    QJsonObject scanProcess;
    int stepCount = 0;

    if (m_scanSchemaService) {
        // 先使用常规容量；服务返回 requiredSize 后只按明确大小重试一次，避免无限扩容。
        QByteArray output(64 * 1024, '\0');
        ScanSchemaServiceResult result = m_scanSchemaService->BuildScanProcessJson(
            configJson.constData(), output.data(), output.size());
        if (result.IsError() && result.requiredSize > output.size()) {
            output.fill('\0', result.requiredSize);
            result = m_scanSchemaService->BuildScanProcessJson(
                configJson.constData(), output.data(), output.size());
        }
        if (result.IsSuccess()) {
            const QJsonDocument outputDocument = QJsonDocument::fromJson(output.constData());
            scanProcess = outputDocument.object();
            m_currentScanProcessJson = outputDocument.toJson(QJsonDocument::Compact);
            stepCount = scanProcess.value("steps").toArray().size();
        } else {
            m_currentScanProcessJson.clear();
            WriteLog(LogLevel::Error,
                     "RefreshScanProcessPreview",
                     QString("ScanSchemaService failed: %1").arg(QString::fromUtf8(result.message)));
        }
    } else {
        m_currentScanProcessJson.clear();
        WriteLog(LogLevel::Error, "RefreshScanProcessPreview", "ScanSchemaService is unavailable");
    }

    if (m_scanProcessPreviewLabel) {
        QStringList labels;
        const QJsonArray steps = scanProcess.value("steps").toArray();
        for (const QJsonValue& value : steps) {
            const QJsonObject item = value.toObject();
            labels << ScanStepDisplayText(item.value("code").toString());
        }
        const QString previewText = labels.isEmpty()
            ? tr("Process unavailable")
            : tr("Process: %1").arg(labels.join(tr(" -> ")));
        // 标签保持稳定单行；宽度不足时 Qt 会裁剪绘制，tooltip 则保留完整流程供用户查看。
        // 这种做法不会按语言或分辨率写坐标分支，也不会让动态文本反向改变牙弓区域大小。
        m_scanProcessPreviewLabel->setText(previewText);
        m_scanProcessPreviewLabel->setToolTip(previewText);
    }

    WriteLog(LogLevel::Info,
             "RefreshScanProcessPreview",
             QString("Scan process refreshed, steps=%1").arg(stepCount));
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

// 返回建单 UI 公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return MEYER_ORDER_CREATE_UI_API_VERSION;
}
