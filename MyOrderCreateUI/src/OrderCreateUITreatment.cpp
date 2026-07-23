#include "OrderCreateUIInternal.h"


// 清理当前 QWidget 内部控件弱引用。
// 这些指针不拥有对象，只用于刷新 UI；外部删除 QWidget 后必须清空，避免下一次使用悬空地址。
void OrderCreateUIImpl::ResetWidgetReferences() {
    m_root = nullptr;
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
    return tr("Unknown");
}

// 返回治疗类型按钮图标路径。
QString OrderCreateUIImpl::TypeButtonIconPath(const QString& typeCode,
                                               bool highlighted,
                                               bool highResolution) const {
    // 治疗方案资源里约定 b 表示普通态，h 表示高亮态。
    // 2x 文件只用于 2560x1440 及以上屏幕；逻辑尺寸不变，只提高图标清晰度。
    QString baseName;
    if (typeCode == "implant") {
        baseName = "planting";
    } else if (typeCode == "crown" || typeCode == "full_crown") {
        baseName = "an_full_crown";
    } else if (typeCode == "missing") {
        baseName = "missing";
    } else if (typeCode == "inlay") {
        baseName = "inlay_n";
    } else if (typeCode == "veneer") {
        baseName = "veneer_n";
    }

    if (baseName.isEmpty()) {
        return QString();
    }

    const QString state = highlighted ? "h" : "b";
    const QString scaleSuffix = highResolution ? "_2x" : "";
    const QString iconName = QString("%1_%2%3.png").arg(baseName).arg(state).arg(scaleSuffix);
    const QDir buttonDir(QDir(ResolveTreatmentPlanAssetRoot()).filePath("button"));
    return buttonDir.filePath(iconName);
}

// 根据按钮所在显示器选择图标资源倍率。
bool OrderCreateUIImpl::UseHighResolutionTreatmentIcons(const QWidget* widget) const {
    // 截图测试使用不显示到桌面的固定画布，不能从真实显示器推导目标截图档位。
    // 测试宿主通过 QApplication 动态属性传入视口尺寸；正式程序没有该属性，仍走实际显示器逻辑。
    if (qApp) {
        const QSize testViewportSize =
            qApp->property("MeyerScanTreatmentIconTestViewportSize").toSize();
        if (testViewportSize.isValid()) {
            return MeyerScanTreatmentPlanRules::ShouldUseHighResolutionIcons(testViewportSize);
        }
    }

    QDesktopWidget* desktop = QApplication::desktop();
    if (!desktop) {
        return false;
    }

    int screenNumber = widget ? desktop->screenNumber(widget) : -1;
    if (screenNumber < 0) {
        screenNumber = desktop->primaryScreen();
    }
    return MeyerScanTreatmentPlanRules::ShouldUseHighResolutionIcons(
        desktop->screenGeometry(screenNumber).size());
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
    // 刷新所有类型按钮的选中视觉状态，保证同一时间只有一个类型高亮。
    for (auto it = m_typeButtons.begin(); it != m_typeButtons.end(); ++it) {
        const bool selected = (it.key() == typeCode);
        auto* treatmentButton = static_cast<TreatmentTypeButton*>(it.value());
        const bool highResolution = UseHighResolutionTreatmentIcons(treatmentButton);
        treatmentButton->SetIconPaths(TypeButtonIconPath(it.key(), false, highResolution),
                                      TypeButtonIconPath(it.key(), true, highResolution),
                                      it.key() != "implant");
        treatmentButton->SetTypeSelected(selected);
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

    // 桥连接点依赖两颗相邻牙都仍处于已选择状态。
    // 修改修复类型不会删除桥；只有移除任一端牙位时才清理已确认连接点。
    const QSet<QString> oldBridgeKeys = m_selectedBridgeKeys;
    for (const QString& bridgeKey : oldBridgeKeys) {
        const QStringList parts = bridgeKey.split('-');
        if (parts.size() != 2) {
            m_selectedBridgeKeys.remove(bridgeKey);
            continue;
        }
        const int firstTooth = parts.at(0).toInt();
        const int secondTooth = parts.at(1).toInt();
        if (NormalizeAdjacentBridgeKey(firstTooth, secondTooth).isEmpty()
            || !m_selectedTeeth.contains(firstTooth)
            || !m_selectedTeeth.contains(secondTooth)) {
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
    const QStringList parts = bridgeKey.trimmed().split('-');
    if (parts.size() != 2) {
        return;
    }

    bool firstOk = false;
    bool secondOk = false;
    const int firstTooth = parts.at(0).toInt(&firstOk);
    const int secondTooth = parts.at(1).toInt(&secondOk);
    const QString normalizedKey = (firstOk && secondOk)
        ? NormalizeAdjacentBridgeKey(firstTooth, secondTooth)
        : QString();
    if (normalizedKey.isEmpty()
        || !m_selectedTeeth.contains(firstTooth)
        || !m_selectedTeeth.contains(secondTooth)) {
        WriteLog(LogLevel::Warning,
                 "ToggleBridgeConnector",
                 QString("Bridge connector ignored because endpoints are invalid: %1").arg(bridgeKey));
        return;
    }

    if (m_selectedBridgeKeys.contains(normalizedKey)) {
        m_selectedBridgeKeys.remove(normalizedKey);
        WriteLog(LogLevel::Info, "RemoveBridgeConnector", QString("Bridge connector removed: %1").arg(normalizedKey));
    } else {
        m_selectedBridgeKeys.insert(normalizedKey);
        WriteLog(LogLevel::Info, "AddBridgeConnector", QString("Bridge connector added: %1").arg(normalizedKey));
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

    // 第一优先级：统一资源 DLL 注册后的 Qt 资源目录。
    // QDir/QFileInfo/QPixmap/QImage 都支持 :/ 路径，因此牙位命中和叠加逻辑无需改写。
    candidateRoots << MeyerQtModule::ModuleResourceFile(
        "MyOrderCreateUI", "icon/createModule", "sacanPlan");

    // 第二优先级：调用方传入的 MeyerScan.exe 所在目录，兼容旧安装包散文件。
    if (!m_appDir.isEmpty()) {
        const QDir appDir(QString::fromUtf8(m_appDir));
        candidateRoots << appDir.filePath(kTreatmentPlanRuntimeRelativePath);
        // 历史兼容：老资源直接放在 exe 同级 icon 目录。
        candidateRoots << appDir.filePath(kTreatmentPlanLegacyRelativePath);
    }

    // 第三优先级：当前进程目录，适合旧版 OrderCreateUITest.exe 独立运行。
    const QDir processDir(QCoreApplication::applicationDirPath());
    candidateRoots << processDir.filePath(kTreatmentPlanRuntimeRelativePath);
    candidateRoots << processDir.filePath(kTreatmentPlanLegacyRelativePath);

    // 第四优先级：模块源码 Resources 目录，便于资源 DLL 尚未构建时继续开发。
    // 模块测试宿主运行目录通常是 MyOrderCreateUI/bin/Release，所以先尝试 ../../Resources。
    candidateRoots << processDir.filePath(QString("../../%1").arg(kTreatmentPlanSourceRelativePath));
    // 主程序根输出目录通常是 F:/MeyerScan/bin/Release，所以再尝试 ../../MyOrderCreateUI/Resources。
    candidateRoots << processDir.filePath("../../MyOrderCreateUI/Resources/icon/createModule/sacanPlan");

    // 第五优先级：历史 bin 资源目录，避免尚未迁移干净的旧测试目录打不开。
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
