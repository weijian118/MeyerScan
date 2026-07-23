#include "OrderCreateUIInternal.h"

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
        const QString rawTypeCode = ReadString(item, "type", "crown");
        const QString typeCode = NormalizeTreatmentTypeCode(rawTypeCode);
        if (typeCode.isEmpty()) {
            // 当前方案只允许五种修复类型；旧 inner_crown/bridge 或未知第三方编码不能进入 UI 状态。
            WriteLog(LogLevel::Warning,
                     "ApplyScanPlanItems",
                     QString("Unsupported treatment type ignored: %1").arg(rawTypeCode));
            continue;
        }
        m_selectedTeeth.insert(toothNumber);
        m_toothTypeCodes.insert(toothNumber, typeCode);
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

        const QString normalizedBridgeKey = NormalizeAdjacentBridgeKey(firstTooth, secondTooth);
        if (normalizedBridgeKey.isEmpty()) {
            // 只允许同一牙弓中直接相邻的两颗牙，避免跨区域连接污染桥记录。
            WriteLog(LogLevel::Warning,
                     "ApplyScanPlanItems",
                     QString("Bridge key ignored because teeth are not adjacent: %1").arg(bridgeKey));
            continue;
        }

        if (!m_selectedTeeth.contains(firstTooth) || !m_selectedTeeth.contains(secondTooth)) {
            // 桥连接点只依赖相邻牙是否已选择，不要求额外的 bridge 修复类型。
            WriteLog(LogLevel::Warning,
                     "ApplyScanPlanItems",
                     QString("Bridge key ignored because one endpoint is not selected: %1").arg(bridgeKey));
            continue;
        }

        // 内部统一保存稳定 key，例如外部 "17-16" 归一化为 "16-17"。
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
