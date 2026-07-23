
#include "MainWindow.h"
#include "MainWindowInternal.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <vector>

// 调用第三方拉起适配器，把外部 JSON 文件归一化为 OrderCreateUI 标准上下文。
bool MainWindow::NormalizeExternalOrderContext(const QString& inputJsonPath,
                                               const QString& thirdPartyType,
                                               QString* outputContextJson) {
    if (outputContextJson) {
        outputContextJson->clear();
    }
    if (inputJsonPath.trimmed().isEmpty()) {
        WriteUserAction("ExternalOrderFailed", "External order json path is empty");
        return false;
    }

    m_externalLaunchAdapter = ExternalLaunchAdapterModule();
    if (!m_externalLaunchAdapter) {
        WriteUserAction("ExternalOrderFailed", "ExternalLaunchAdapter unavailable");
        WriteStatus(tr("External launch adapter unavailable"));
        return false;
    }

    if (!m_externalLaunchAdapterInitialized) {
        m_externalLaunchAdapterInitialized = m_externalLaunchAdapter->Init(m_appDirUtf8.constData(),
                                                                           m_logDirUtf8.constData());
    }
    if (!m_externalLaunchAdapterInitialized) {
        WriteUserAction("ExternalOrderFailed", "ExternalLaunchAdapter Init returned false");
        WriteStatus(tr("External launch adapter initialize failed"));
        return false;
    }

    // 先给 64KB 缓冲区。患者/订单/扫描方案字段正常远小于这个尺寸；
    // 若第三方字段扩展导致不够，适配器会通过 requiredBufferSize 告诉调用方重新分配。
    std::vector<char> outputBuffer(64 * 1024, '\0');
    ExternalLaunchResult result = {};
    const QByteArray inputPathBytes = QDir::fromNativeSeparators(inputJsonPath).toUtf8();
    const QByteArray thirdPartyTypeBytes = thirdPartyType.toUtf8();
    bool ok = m_externalLaunchAdapter->NormalizeOrderFile(inputPathBytes.constData(),
                                                          thirdPartyTypeBytes.constData(),
                                                          outputBuffer.data(),
                                                          static_cast<int>(outputBuffer.size()),
                                                          &result);
    if (!ok && result.errorCode == 5 && result.requiredBufferSize > static_cast<int>(outputBuffer.size())) {
        // 缓冲区不足是可恢复错误，按适配器提示大小重试一次。
        outputBuffer.assign(static_cast<size_t>(result.requiredBufferSize), '\0');
        ok = m_externalLaunchAdapter->NormalizeOrderFile(inputPathBytes.constData(),
                                                         thirdPartyTypeBytes.constData(),
                                                         outputBuffer.data(),
                                                         static_cast<int>(outputBuffer.size()),
                                                         &result);
    }

    if (!ok) {
        const QString message = QString::fromUtf8(result.message);
        WriteUserAction("ExternalOrderFailed", message.isEmpty() ? "Normalize external order failed" : message);
        WriteStatus(tr("External order failed"));
        return false;
    }

    if (outputContextJson) {
        *outputContextJson = QString::fromUtf8(outputBuffer.data());
    }
    WriteUserAction("ExternalOrderNormalized",
                    QString("thirdPartyType=%1, sourceSystem=%2")
                        .arg(QString::fromUtf8(result.thirdPartyType))
                        .arg(QString::fromUtf8(result.sourceSystem)));
    return true;
}

// 下发首页入口规则。
// ConfigCenter 表达产品默认策略，Permission 表达授权结果；MainExe 集中合并，HomeUI 只接收最终 UI 状态。
void MainWindow::ApplyHomeEntryRules() {
    if (!m_home) {
        // HomeUI 尚未加载时没有规则可下发。
        return;
    }

    // 设置入口：配置默认可见 && 权限可见；enabled 只由权限决定。
    m_home->SetEntryVisible(HomeEntrySettings,
                            IsFeatureVisible("home.settings", "feature.home.settingsVisible", true));
    // visible 控制是否显示，enabled 控制显示后能否点击；两者含义不同，不能互相替代。
    m_home->SetEntryEnabled(HomeEntrySettings, IsFeatureEnabled("home.settings", true));

    // 浏览入口当前没有 runtime_config 默认项，先由 Permission 控制 visible/enabled。
    m_home->SetEntryVisible(HomeEntryBrowse, IsFeatureVisible("case.browse", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryBrowse, IsFeatureEnabled("case.browse", true));

    // 建单和练习入口先接入 Permission，后续对应模块落地后即可继续复用。
    m_home->SetEntryVisible(HomeEntryCreate, IsFeatureVisible("order.create", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryCreate, IsFeatureEnabled("order.create", true));
    m_home->SetEntryVisible(HomeEntryPractice, IsFeatureVisible("scan.practice", nullptr, true));
    m_home->SetEntryEnabled(HomeEntryPractice, IsFeatureEnabled("scan.practice", true));
}

// 下发案例管理动作规则。
// 不同动作后续可能有不同判断时机；当前先把页面创建前即可确定的按钮状态集中到这里。
void MainWindow::ApplyCaseActionRules() {
    if (!m_case) {
        // CaseUI 尚未加载时没有按钮状态可设置。
        return;
    }

    // 返回首页按钮：配置默认可见 && 权限可见；enabled 由权限控制禁用态。
    m_case->SetActionVisible(CaseActionBackHome,
                             IsFeatureVisible("case.backHome", "feature.case.backHomeVisible", true));
    // enabled=false 时按钮仍可见但不可点，适合让用户知道有此功能但当前无权限/不可用。
    m_case->SetActionEnabled(CaseActionBackHome, IsFeatureEnabled("case.backHome", true));
    // 浏览页设置入口复用 home.settings 权限，避免同一设置模块出现两套授权规则。
    m_case->SetActionVisible(CaseActionOpenSettings,
                             IsFeatureVisible("home.settings", "feature.home.settingsVisible", true));
    m_case->SetActionEnabled(CaseActionOpenSettings, IsFeatureEnabled("home.settings", true));
}

// 从 RuntimeDataCenter 组装给 UI 的版本化只读上下文。
// MainExe 是进程级数据中心的唯一 UI 编排者，CaseUI/SettingsUI 不再自行加载或初始化数据库链路。
QString MainWindow::BuildRuntimeDataContextJson(const QStringList& domains) {
    QJsonObject domainObjects;

    for (const QString& domain : domains) {
        // 默认对象始终包含 items，UI 即使遇到数据库不可用也能稳定显示空状态。
        QJsonObject domainObject;
        domainObject.insert("items", QJsonArray());
        domainObject.insert("status", "unavailable");

        if (m_runtimeDataCenter && !domain.trimmed().isEmpty()) {
            QByteArray buffer;
            RuntimeDataCenterResult result;
            std::memset(&result, 0, sizeof(result));
            const QByteArray domainBytes = domain.toUtf8();

            // RuntimeDataCenter 采用调用方缓冲区，有限倍增可兼顾字段扩展和内存上限。
            for (int bufferSize = kInitialRuntimeDomainBufferSize;
                 bufferSize <= kMaxRuntimeDomainBufferSize;
                 bufferSize *= 2) {
                buffer.fill('\0', bufferSize);
                result = m_runtimeDataCenter->GetDomainJson(domainBytes.constData(), buffer.data(), buffer.size());
                if (result.IsSuccess()) {
                    QJsonParseError parseError;
                    const QJsonDocument document = QJsonDocument::fromJson(buffer.constData(), &parseError);
                    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                        // 保留 domain 的 source/status/revision 等元数据，UI 当前只消费 items。
                        domainObject = document.object();
                    } else {
                        WriteUserAction("RuntimeDataContextParseFailed", domain);
                    }
                    break;
                }

                const QString errorText = QString::fromUtf8(result.message);
                if (!errorText.contains("too small", Qt::CaseInsensitive)) {
                    WriteUserAction("RuntimeDataContextReadFailed",
                                    QString("%1: %2").arg(domain, errorText));
                    break;
                }
            }
        }
        // 患者和订单正在从旧表迁移到 CaseOrderService 自有表。
        // 在宿主编排层合并两个读模型，可保持 UI 纯展示边界并让新建记录立即可见。
        MergeCaseOrderServiceReadModel(domain, &domainObject);
        domainObjects.insert(domain, domainObject);
    }

    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("generatedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("domains", domainObjects);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// 将 CaseOrderService 的患者/订单摘要合并到 RuntimeDataCenter 旧库快照。
// 该函数只理解稳定 queryName 和 JSON，不拼 SQL，也不把服务对象传给 CaseUI。
void MainWindow::MergeCaseOrderServiceReadModel(const QString& domain, QJsonObject* domainObject) {
    if (!domainObject || !m_caseOrderService || !m_caseOrderServiceInitialized) {
        // 服务不可用时保留 RuntimeDataCenter 原快照，浏览页仍可显示旧库数据或空状态。
        return;
    }

    QString queryName;
    if (domain == "local.orders") {
        queryName = "patientOrder.listOrders";
    } else if (domain == "local.patients") {
        queryName = "patientOrder.listPatients";
    } else {
        // 诊所、医生、技工所等 domain 当前继续完全由 RuntimeDataCenter 提供。
        return;
    }

    QByteArray buffer;
    CaseOrderServiceResult result;
    std::memset(&result, 0, sizeof(result));
    const QByteArray queryNameUtf8 = queryName.toUtf8();

    // 患者/订单列表字段可能逐步扩展，使用与 RuntimeDataCenter 相同的有限倍增策略。
    // 到达 32MB 仍不够时必须改分页接口，不能继续扩大启动期内存占用。
    for (int bufferSize = kInitialRuntimeDomainBufferSize;
         bufferSize <= kMaxRuntimeDomainBufferSize;
         bufferSize *= 2) {
        buffer.fill('\0', bufferSize);
        result = m_caseOrderService->QueryJson(queryNameUtf8.constData(),
                                               "{}",
                                               buffer.data(),
                                               buffer.size());
        if (result.IsSuccess()) {
            break;
        }

        const QString errorText = QString::fromUtf8(result.message);
        if (!errorText.contains("too small", Qt::CaseInsensitive)) {
            // 非容量错误重试没有意义；写日志后保留旧库快照继续运行。
            WriteUserAction("CaseOrderReadModelFailed",
                            QString("%1: %2").arg(queryName, errorText));
            return;
        }
    }

    if (result.IsError()) {
        WriteUserAction("CaseOrderReadModelFailed",
                        QString("%1 exceeded the read model buffer limit").arg(queryName));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(buffer.constData(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteUserAction("CaseOrderReadModelParseFailed",
                        QString("%1: %2").arg(queryName, parseError.errorString()));
        return;
    }

    const QJsonArray serviceItems = document.object().value("items").toArray();
    const QJsonArray legacyItems = domainObject->value("items").toArray();
    const QJsonArray mergedItems = MergeReadModelItems(domain, serviceItems, legacyItems);
    domainObject->insert("items", mergedItems);
    domainObject->insert("rowCount", mergedItems.size());
    domainObject->insert("serviceRowCount", serviceItems.size());
    domainObject->insert("legacyRowCount", legacyItems.size());
    domainObject->insert("source", "RuntimeDataCenter+CaseOrderService");
    domainObject->insert("loadStatus", "ok");
    WriteUserAction("CaseOrderReadModelMerged",
                    QString("%1: service=%2, legacy=%3, merged=%4")
                        .arg(domain)
                        .arg(serviceItems.size())
                        .arg(legacyItems.size())
                        .arg(mergedItems.size()));
}

// 保存当前建单表单。
// ID 由宿主工作流补齐，CaseOrderService 负责数据库规则，OrderCreateUI 不参与持久化。
bool MainWindow::SaveCurrentOrderContext() {
    if (!m_orderCreate || !m_caseOrderService || !m_caseOrderServiceInitialized) {
        WriteUserAction("OrderSaveFailed", "OrderCreateUI or CaseOrderService is unavailable");
        return false;
    }

    const char* contextText = m_orderCreate->GetCurrentOrderContextJson();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(contextText ? contextText : ""), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteUserAction("OrderSaveFailed", QString("Invalid order context: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject root = document.object();
    QJsonObject patient = root.value("patient").toObject();
    QJsonObject order = root.value("order").toObject();
    if (patient.value("name").toString().trimmed().isEmpty()) {
        WriteUserAction("OrderSaveRejected", "Patient name is empty");
        return false;
    }

    // 手工建单可能没有外部 ID。工作流在持久化边界生成一次并回写 UI，后续重复保存保持同一 ID。
    QString patientId = patient.value("patientId").toString().trimmed();
    QString orderId = order.value("orderId").toString().trimmed();
    if (patientId.isEmpty()) {
        patientId = QString("P-%1").arg(QUuid::createUuid().toString().remove('{').remove('}'));
        patient.insert("patientId", patientId);
    }
    if (orderId.isEmpty()) {
        orderId = QString("O-%1").arg(QUuid::createUuid().toString().remove('{').remove('}'));
        order.insert("orderId", orderId);
    }
    // caseId 在当前骨架中默认复用 orderId；以后若产品需要一病例多订单，可由 Workflow 单独生成。
    if (order.value("caseId").toString().trimmed().isEmpty()) {
        order.insert("caseId", orderId);
    }
    // 创建时间只在首次保存时写入，重复点击 Confirm/Next 不覆盖原始创建时间。
    if (order.value("createdAt").toString().trimmed().isEmpty()) {
        order.insert("createdAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    }
    // 状态进入 order 对象，CaseOrderService 可直接提取索引；顶层 status 仅保留合同兼容。
    const QString orderStatus = order.value("status").toString(
        root.value("status").toString("created"));
    order.insert("status", orderStatus);
    root.insert("patient", patient);
    root.insert("order", order);
    root.insert("status", orderStatus);

    const QByteArray normalizedContext = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const CaseOrderServiceResult saveResult = m_caseOrderService->SavePatientOrderJson(normalizedContext.constData());
    if (saveResult.IsError()) {
        WriteUserAction("OrderSaveFailed", QString::fromUtf8(saveResult.message));
        return false;
    }

    // 把服务接受的 ID 回写建单页和工作台上下文，保证进入 Scan/Process 后使用同一订单引用。
    m_workspaceContextJson = QString::fromUtf8(normalizedContext);
    m_orderCreate->SetOrderContextJson(normalizedContext.constData());
    if (m_runtimeDataCenter) {
        // 写成功后刷新相关读模型。刷新失败不回滚已提交订单，但必须写日志供排查。
        const RuntimeDataCenterResult orderReload = m_runtimeDataCenter->ReloadDomain("local.orders");
        const RuntimeDataCenterResult patientReload = m_runtimeDataCenter->ReloadDomain("local.patients");
        if (orderReload.IsError() || patientReload.IsError()) {
            WriteUserAction("RuntimeDataReloadWarning",
                            QString("orders=%1, patients=%2")
                                .arg(QString::fromUtf8(orderReload.message))
                                .arg(QString::fromUtf8(patientReload.message)));
        }
    }
    WriteUserAction("OrderSaved", QString("orderId=%1, patientId=%2").arg(orderId, patientId));
    return true;
}

// 统一合并配置显隐和权限显隐。
// configKey 允许为空，表示该功能暂时没有产品默认项，只读取 Permission。
bool MainWindow::IsFeatureVisible(const char* featureId, const char* configKey, bool defaultVisible) const {
    // ConfigCenter 控制产品/客户默认策略，例如某客户版本默认隐藏某入口。
    const bool visibleByConfig = (m_config && configKey && configKey[0])
        ? m_config->GetBool(configKey, defaultVisible)
        : defaultVisible;
    // Permission 控制授权结果，例如设备/账号/版本不满足时隐藏入口。
    const bool visibleByPermission = m_permission
        ? m_permission->IsFeatureVisible(featureId, defaultVisible)
        : defaultVisible;
    // 两者取 AND：产品策略不开放或授权不允许，最终都不可见。
    // 这里把合并逻辑放在 MainExe，是为了让 UI 模块只接收最终状态，不关心配置/权限来源。
    return visibleByConfig && visibleByPermission;
}

// 统一读取 Permission enabled。
// enabled=false 必须真实生效：UI 设置禁用态，后续动作执行入口还要继续复核。
bool MainWindow::IsFeatureEnabled(const char* featureId, bool defaultEnabled) const {
    // enabled 只表达“可执行/可点击”，不会改变可见性。
    // 动作执行前也会再次调用它复核，避免 UI 状态异常时越权执行。
    return m_permission ? m_permission->IsFeatureEnabled(featureId, defaultEnabled) : defaultEnabled;
}

// 将首页入口 ID 映射为权限 featureId。
// 映射集中在 MainExe，后续新增入口时只需要在这里和 ApplyHomeEntryRules 中维护。
const char* MainWindow::HomeEntryFeatureId(int entryId) const {
    switch (entryId) {
    case HomeEntryCreate:   return "order.create";
    case HomeEntryBrowse:   return "case.browse";
    case HomeEntryPractice: return "scan.practice";
    case HomeEntrySettings: return "home.settings";
    default:                return nullptr;
    }
}

// 将案例管理动作 ID 映射为权限 featureId。
// 当前只对已纳入权限文件的动作做复核，未映射动作默认继续走开发期流程。
const char* MainWindow::CaseActionFeatureId(int actionId) const {
    switch (actionId) {
    case CaseActionBackHome: return "case.backHome";
    case CaseActionOpenSettings: return "home.settings";
    default:                 return nullptr;
    }
}

// 构造默认练习扫描流程。
QJsonObject MainWindow::BuildDefaultScanProcessObject() const {
    QJsonArray steps;

    auto appendStep = [&steps](const QString& part, const QString& code, bool exchange) {
        QJsonObject item;
        item.insert("part", part);
        item.insert("code", code);
        // 跨模块合同只保存稳定编码；Scan/Process UI 根据 code 使用自己的 tr() 显示文本。
        item.insert("labelKey", code);
        item.insert("exchange", exchange);
        item.insert("enabled", true);
        steps.append(item);
    };

    appendStep("maxilla", "maxilla_natural", false);
    appendStep("exchange", "data_exchange", true);
    appendStep("mandible", "mandible_natural", false);
    appendStep("occlusion", "natural_occlusion", false);

    QJsonObject config;
    config.insert("occlusionType", "natural");
    config.insert("practiceDefault", true);

    QJsonObject scanProcess;
    scanProcess.insert("schemaVersion", 1);
    scanProcess.insert("source", "MainExePracticeDefault");
    scanProcess.insert("config", config);
    scanProcess.insert("steps", steps);
    return scanProcess;
}

// 从建单页面读取最新扫描流程并合并到工作台上下文。
void MainWindow::RefreshWorkspaceScanProcessFromOrder() {
    if (!m_orderCreate) {
        return;
    }

    const char* scanProcessJson = m_orderCreate->GetCurrentScanProcessJson();
    if (!scanProcessJson || !scanProcessJson[0]) {
        WriteUserAction("ScanProcessRefresh", "OrderCreateUI returned empty scan process");
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(scanProcessJson), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        WriteUserAction("ScanProcessRefresh", QString("Invalid scan process json: %1").arg(parseError.errorString()));
        return;
    }

    SetWorkspaceScanProcess(document.object());
    WriteUserAction("ScanProcessRefresh", "Workspace scan process refreshed from OrderCreateUI");
}

// 把扫描流程对象写入工作台上下文，并同步已经创建的 Scan/Process 页面。
void MainWindow::SetWorkspaceScanProcess(const QJsonObject& scanProcessObject) {
    QJsonObject context;
    if (!m_workspaceContextJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(m_workspaceContextJson.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            context = document.object();
        }
    }

    if (context.isEmpty()) {
        const QJsonDocument fallback = QJsonDocument::fromJson(BuildDefaultWorkspaceContextJson("order").toUtf8());
        context = fallback.object();
    }

    context.insert("scanProcess", scanProcessObject);
    m_workspaceContextJson = QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));

    RefreshWorkspaceContextConsumers();
}
