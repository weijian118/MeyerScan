#include "CaseOrderServiceImpl.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QHash>
#include <QStringList>
#include <cstring>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_CaseOrderService";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_CaseOrderService v0.2.2 (2026-07-15)";
}

// 将短消息复制到固定长度返回结构中。
// 接口当前采用 C ABI 友好的固定数组，避免跨 DLL 分配字符串所有权。
void CopyMessage(char* target, int targetSize, const char* message) {
    if (!target || targetSize <= 0) {
        // 调用方缓冲区无效时不能写入，直接返回避免访问非法内存。
        return;
    }
    // 先清零整个固定数组，保证即使 message 较短，尾部也不会残留旧内容。
    std::memset(target, 0, static_cast<size_t>(targetSize));
    if (!message) {
        // nullptr 表示没有错误文本或成功文本，保持空字符串即可。
        return;
    }
    // strncpy 最多复制 targetSize - 1 个字符，最后一个字节保留给 '\0'。
    // 这样 C ABI 调用方可以安全按普通字符串读取。
    std::strncpy(target, message, static_cast<size_t>(targetSize - 1));
}
}

// 返回病例订单服务单例。
// 当前服务是进程内共享服务，避免多个调用方重复连接数据库或重复创建 schema。
CaseOrderServiceImpl& CaseOrderServiceImpl::Instance() {
    // 服务层需要共享 Database/Logger 引用，使用进程内单例能避免多个服务对象重复连接。
    static CaseOrderServiceImpl instance;
    // 返回引用，后续 C ABI 导出函数再转成接口指针。
    return instance;
}

// 初始化服务。
// 服务内部可以使用 Qt，但对外接口保持 const char* / 调用方缓冲区，方便长期 ABI 稳定。
bool CaseOrderServiceImpl::Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) {
    // 保存调用方传入的路径字节数组，后续独立测试或重连时可以复用。
    // 路径由 MainExe 基于 applicationDirPath() 计算，服务层不使用 currentPath。
    // QByteArray 会复制传入字节，避免调用方临时 QByteArray 析构后这里留下悬空 const char*。
    m_databaseConfigPath = QByteArray(databaseConfigPathUtf8 ? databaseConfigPathUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 缓存 ILogger 指针；后续写日志都复用 m_logger，不反复 GetLogger()。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        // Init 是幂等预期：Logger 已初始化时再次 Init 应复用同一日志目录和级别。
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    // 服务层通过 DatabaseQtAdapter 访问纯 C++ Database。
    // 这样 QString/QJson 与 UTF-8/POD 的转换集中在适配层，Database 本身不依赖 Qt。
    m_databaseAdapter = GetDatabaseQtAdapter();
    if (!m_databaseAdapter) {
        WriteLog(LogLevel::Error, "Init", "DatabaseQtAdapter instance unavailable");
        return false;
    }

    // MainExe 已经完成数据库连接时，服务层直接复用连接。
    // 独立测试宿主运行时，Database 可能尚未初始化，所以这里保留 Init/Connect。
    QString databaseError;
    if (!m_databaseAdapter->EnsureConnected(QString::fromUtf8(m_databaseConfigPath), &databaseError)) {
        WriteLog(LogLevel::Error, "Init", databaseError);
        return false;
    }

    // m_initialized 只表示服务层基础设施可用，不代表业务表一定已经创建。
    // 业务表创建由 EnsureSchema 显式执行，便于后续迁移到 migration。
    m_initialized = true;
    WriteLog(LogLevel::Info, "Init", "CaseOrderService initialized");
    return true;
}

// 检查并创建当前骨架所需 schema。
// 正式版本后续应迁移到版本化 migration，Database 只负责执行 SQL。
CaseOrderServiceResult CaseOrderServiceImpl::EnsureSchema() {
    if (!m_initialized || !m_databaseAdapter) {
        // 防止调用方跳过 Init 直接建表，避免适配层为空时崩溃。
        return Fail(5, "CaseOrderService is not initialized");
    }

    const char* scripts[] = {
        // ms_patient_order 当前按“患者 + 订单组合 JSON”保存。
        // 变化频繁的字段先放在 payload_json，稳定后再抽成正式列。
        "CREATE TABLE IF NOT EXISTS ms_patient_order ("
        "order_id VARCHAR(64) PRIMARY KEY,"
        "patient_id VARCHAR(64),"
        "case_id VARCHAR(64),"
        "status VARCHAR(32),"
        "payload_json TEXT NOT NULL,"
        "schema_version INTEGER NOT NULL DEFAULT 1,"
        "created_at VARCHAR(32),"
        "updated_at VARCHAR(32)"
        ")",
        // 医生、诊所、技工所、操作人等参考数据先放一张通用表。
        // category 做分类，payload_json 保存扩展字段，避免为每类字典过早建表。
        "CREATE TABLE IF NOT EXISTS ms_reference_data ("
        "id VARCHAR(64) PRIMARY KEY,"
        "category VARCHAR(32) NOT NULL,"
        "display_name VARCHAR(256) NOT NULL,"
        "payload_json TEXT,"
        "enabled INTEGER NOT NULL DEFAULT 1,"
        "sort_index INTEGER NOT NULL DEFAULT 0,"
        "updated_at VARCHAR(32)"
        ")"
    };

    // ExecuteScript 返回成功执行数量，不抛异常。
    // 这里要求所有 schema SQL 都成功，否则认为服务层 schema 未准备好。
    // sizeof(scripts) / sizeof(scripts[0]) 是 C/C++ 里计算静态数组元素个数的常用写法。
    QList<QByteArray> scriptList;
    const int scriptTotal = static_cast<int>(sizeof(scripts) / sizeof(scripts[0]));
    for (int i = 0; i < scriptTotal; ++i) {
        scriptList.append(QByteArray(scripts[i]));
    }
    const int successCount = m_databaseAdapter->ExecuteScript(scriptList);
    if (successCount != scriptTotal) {
        WriteLog(LogLevel::Error, "EnsureSchema", "Case/order schema creation failed");
        return Fail(10002, "Case/order schema creation failed");
    }

    WriteLog(LogLevel::Info, "EnsureSchema", "Case/order schema checked");
    return Ok("Case/order schema checked");
}

// 保存患者/订单组合 JSON。
// 当前用 orderId 作为主键保存完整 payload，后续再逐步拆正式字段和扩展字段。
CaseOrderServiceResult CaseOrderServiceImpl::SavePatientOrderJson(const char* patientOrderJsonUtf8) {
    if (!m_initialized || !m_databaseAdapter) {
        // Save 依赖数据库连接，必须先 Init。
        return Fail(5, "CaseOrderService is not initialized");
    }
    if (!patientOrderJsonUtf8 || !patientOrderJsonUtf8[0]) {
        // 空 JSON 没有任何业务意义，直接返回参数错误。
        return Fail(2, "patientOrderJsonUtf8 is empty");
    }

    // 外部传入的是一组患者/订单信息，使用 JSON 便于字段增删。
    // 这里先验证它确实是 JSON 对象，避免无效文本写入数据库。
    QJsonParseError parseError;
    // QByteArray(patientOrderJsonUtf8) 会把 C 字符串复制为 Qt 字节数组，供 JSON 解析器读取。
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(patientOrderJsonUtf8), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return Fail(2, "patientOrderJsonUtf8 must be a JSON object");
    }

    // document.object() 得到 JSON 根对象；标准建单合同把患者和订单分别放在嵌套对象中。
    const QJsonObject root = document.object();
    const QJsonObject orderObject = root.value("order").toObject();
    const QJsonObject patientObject = root.value("patient").toObject();
    // orderId 是当前骨架表的主键，也是 UI/扫描流程之间最稳定的引用 ID。
    // 顶层字段只作为旧测试/旧调用方兼容回退，新代码统一使用 order.orderId。
    const QString orderId = orderObject.value("orderId").toString(
        root.value("orderId").toString()).trimmed();
    if (orderId.isEmpty()) {
        return Fail(2, "patientOrderJsonUtf8.order.orderId is empty");
    }

    // patientId/caseId/status 提取成索引候选字段，方便列表和过滤；
    // 原始完整 JSON 仍保存在 payload_json，保证字段新增时不用马上改表。
    const QString patientId = patientObject.value("patientId").toString(
        root.value("patientId").toString()).trimmed();
    const QString caseId = orderObject.value("caseId").toString(
        root.value("caseId").toString()).trimmed();
    const QString status = orderObject.value("status").toString(
        root.value("status").toString("created")).trimmed();
    // Compact 保留完整 JSON 语义但去掉空白，适合存库和跨模块传输。
    const QString payload = QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    const QString escapedPayload = DatabaseQtAdapter::EscapeSqlText(payload);
    // 时间统一用 UTC，避免多时区/系统时区变化导致排序和同步混乱。
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // REPLACE INTO 是骨架期的简化 upsert。
    // 正式实现应按数据库类型封装 DAO，并在 Database/DAO 层提供参数绑定能力。
    // 这里不能再写 QSqlQuery：Database 已经去 Qt，Qt 类型转换只允许集中在 MyDatabaseQtAdapter。
    const QString sql = QString(
        "REPLACE INTO ms_patient_order "
        "(order_id, patient_id, case_id, status, payload_json, schema_version, created_at, updated_at) "
        "VALUES ('%1', '%2', '%3', '%4', '%5', 1, '%6', '%6')")
        .arg(DatabaseQtAdapter::EscapeSqlText(orderId))
        .arg(DatabaseQtAdapter::EscapeSqlText(patientId))
        .arg(DatabaseQtAdapter::EscapeSqlText(caseId))
        .arg(DatabaseQtAdapter::EscapeSqlText(status))
        .arg(escapedPayload)
        .arg(DatabaseQtAdapter::EscapeSqlText(now));

    QString updateError;
    if (!m_databaseAdapter->ExecuteUpdate(sql, &updateError)) {
        // 返回适配层整理后的错误消息，让调用方能定位数据库写入失败原因。
        WriteLog(LogLevel::Error, "SavePatientOrderJson", updateError);
        // Fail 会立即复制消息，但仍使用命名缓冲区明确 const char* 的有效期。
        const QByteArray updateErrorUtf8 = updateError.toUtf8();
        return Fail(10004, updateErrorUtf8.constData());
    }

    WriteLog(LogLevel::Info, "SavePatientOrderJson", QString("orderId=%1").arg(orderId));
    return Ok("Patient/order JSON saved");
}

// 按订单 ID 读取患者/订单组合 JSON。
// 调用方不需要知道数据库表结构，只拿到稳定 JSON 字符串。
CaseOrderServiceResult CaseOrderServiceImpl::GetPatientOrderJson(const char* orderIdUtf8, char* buffer, int bufferSize) {
    if (!m_initialized || !m_databaseAdapter) {
        // 未初始化时不访问适配层，防止空指针。
        return Fail(5, "CaseOrderService is not initialized");
    }
    if (!orderIdUtf8 || !orderIdUtf8[0]) {
        return Fail(2, "orderIdUtf8 is empty");
    }

    const QString orderId = QString::fromUtf8(orderIdUtf8).trimmed();
    // orderId 来自外部输入，拼 SQL 前必须转义单引号。
    // 这仍是骨架期方案，正式 DAO 层应改为参数绑定。
    const QString sql = QString("SELECT payload_json FROM ms_patient_order WHERE order_id='%1'")
        .arg(DatabaseQtAdapter::EscapeSqlText(orderId));

    // Adapter 负责分配和扩大调用方缓冲区，服务层只拿到完整 UTF-8 JSON。
    QByteArray jsonBuffer;
    QString queryError;
    if (!m_databaseAdapter->ExecuteQueryJson(sql, &jsonBuffer, &queryError, 1024 * 256, 1024 * 1024)) {
        WriteLog(LogLevel::Error, "GetPatientOrderJson", queryError);
        // 跨入纯 C ABI 结果构造前先保存 UTF-8 字节，避免示范临时指针写法。
        const QByteArray queryErrorUtf8 = queryError.toUtf8();
        return Fail(10002, queryErrorUtf8.constData());
    }

    // ExecuteQueryJson 返回统一表格 JSON：{ columns:[], rows:[] }。
    // 服务层只从 rows[0].payload_json 取回业务 JSON，隐藏数据库表结构。
    // QByteArray 的 constData() 是以 '\0' 结尾的缓冲区，可直接给 fromJson 解析。
    QJsonDocument tableDocument = QJsonDocument::fromJson(jsonBuffer);
    const QJsonArray rows = DatabaseQtAdapter::RowsFromTableJson(tableDocument);
    if (rows.isEmpty()) {
        return Fail(10006, "Patient/order record not found");
    }

    const QString payload = rows.first().toObject().value("payload_json").toString();
    WriteLog(LogLevel::Info, "GetPatientOrderJson", QString("orderId=%1").arg(orderId));
    // 最后统一通过 CopyToBuffer 写入调用方缓冲区，处理截断和空指针问题。
    return CopyToBuffer(payload, buffer, bufferSize);
}

// 读取医生、诊所、技工所、操作人等参考数据。
// category 经过白名单映射，避免 UI 传任意表名或任意 SQL。
CaseOrderServiceResult CaseOrderServiceImpl::ListReferenceDataJson(const char* categoryUtf8, char* buffer, int bufferSize) {
    if (!m_initialized || !m_databaseAdapter) {
        return Fail(5, "CaseOrderService is not initialized");
    }

    // category 使用小写归一化，外部可传 doctor/doctors 等友好名称。
    const QString category = QString::fromUtf8(categoryUtf8 ? categoryUtf8 : "").trimmed().toLower();
    // ReferenceCategoryToTable 不是返回真实表名，而是返回允许的内部分类值。
    // 这样能防止外部把任意字符串拼进 SQL。
    const QString tableCategory = category.isEmpty() ? "" : ReferenceCategoryToTable(category);
    if (!category.isEmpty() && tableCategory.isEmpty()) {
        return Fail(2, "Unsupported reference data category");
    }

    // enabled=1 表示只返回当前可用字典项，隐藏/停用项不进入 UI 下拉框。
    QString sql = "SELECT id, category, display_name, payload_json, enabled, sort_index "
                  "FROM ms_reference_data WHERE enabled=1";
    if (!tableCategory.isEmpty()) {
        sql += QString(" AND category='%1'").arg(DatabaseQtAdapter::EscapeSqlText(tableCategory));
    }
    sql += " ORDER BY category, sort_index, display_name";

    // 与患者/订单查询一样，Database 返回通用表格 JSON，服务层再包一层业务语义。
    QByteArray jsonBuffer;
    QString queryError;
    if (!m_databaseAdapter->ExecuteQueryJson(sql, &jsonBuffer, &queryError, 1024 * 256, 1024 * 1024)) {
        WriteLog(LogLevel::Error, "ListReferenceDataJson", queryError);
        // Fail 在返回结构体中复制错误文本，命名缓冲区让复制来源生命周期一目了然。
        const QByteArray queryErrorUtf8 = queryError.toUtf8();
        return Fail(10002, queryErrorUtf8.constData());
    }

    // 输出统一结构，方便 UI 只关心 items，不关心底层表名和 SQL。
    QJsonObject root;
    // schemaVersion 是业务 JSON 的版本，不等于 DLL 文件版本；用于未来字段结构兼容。
    root.insert("schemaVersion", 1);
    root.insert("category", category.isEmpty() ? "all" : category);
    root.insert("source", "CaseOrderService");
    root.insert("items", DatabaseQtAdapter::RowsFromTableJson(jsonBuffer));

    WriteLog(LogLevel::Info, "ListReferenceDataJson", QString("category=%1").arg(category.isEmpty() ? "all" : category));
    // 最终输出依然是紧凑 JSON，调用方只需解析 items 数组。
    return CopyToBuffer(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)), buffer, bufferSize);
}

// 稳定查询入口。
// UI/外部适配器只传 queryName 和 JSON 参数，不允许直接拼 SQL 访问 Database。
CaseOrderServiceResult CaseOrderServiceImpl::QueryJson(const char* queryNameUtf8,
                                                       const char* queryArgsJsonUtf8,
                                                       char* buffer,
                                                       int bufferSize) {
    if (!m_initialized || !m_databaseAdapter) {
        return Fail(5, "CaseOrderService is not initialized");
    }

    // queryName 是服务层稳定入口，调用方不能传 SQL。
    // 后续新增查询只在这里增加白名单分支，便于人工审查边界。
    const QString queryName = QString::fromUtf8(queryNameUtf8 ? queryNameUtf8 : "").trimmed();
    if (queryName == "patientOrder.byOrderId") {
        // 参数也使用 JSON 对象，后续字段扩展不会改变 C ABI 函数签名。
        QJsonParseError parseError;
        const QJsonDocument args = QJsonDocument::fromJson(QByteArray(queryArgsJsonUtf8 ? queryArgsJsonUtf8 : "{}"), &parseError);
        if (parseError.error != QJsonParseError::NoError || !args.isObject()) {
            return Fail(2, "queryArgsJsonUtf8 must be a JSON object");
        }
        const QString orderId = args.object().value("orderId").toString().trimmed();
        // 使用命名缓冲区把 QString 转成稳定 UTF-8；被调函数只在本次同步调用内读取该指针。
        const QByteArray orderIdUtf8 = orderId.toUtf8();
        return GetPatientOrderJson(orderIdUtf8.constData(), buffer, bufferSize);
    }

    if (queryName == "referenceData.list") {
        // referenceData.list 用同一个入口读取医生/诊所/技工所等字典数据。
        QJsonParseError parseError;
        const QJsonDocument args = QJsonDocument::fromJson(QByteArray(queryArgsJsonUtf8 ? queryArgsJsonUtf8 : "{}"), &parseError);
        if (parseError.error != QJsonParseError::NoError || !args.isObject()) {
            return Fail(2, "queryArgsJsonUtf8 must be a JSON object");
        }
        const QString category = args.object().value("category").toString();
        // category 继续交给 ListReferenceDataJson 做白名单映射，QueryJson 不重复规则。
        const QByteArray categoryUtf8 = category.toUtf8();
        return ListReferenceDataJson(categoryUtf8.constData(), buffer, bufferSize);
    }

    if (queryName == "patientOrder.listOrders") {
        // 列表查询不接收 SQL、表名或分页表达式；当前骨架只返回案例页需要的轻量订单摘要。
        return ListPatientOrderReadModelJson("orders", buffer, bufferSize);
    }

    if (queryName == "patientOrder.listPatients") {
        // 患者列表由同一批患者/订单组合数据归并得到，保证新建订单保存后能立即形成患者读模型。
        return ListPatientOrderReadModelJson("patients", buffer, bufferSize);
    }

    // 未登记的 queryName 直接拒绝，防止服务层变成任意查询通道。
    return Fail(2, "Unsupported queryNameUtf8");
}

// 查询服务自有患者/订单表，并转换成 UI 可消费的轻量列表。
// 该函数是写模型到读模型的适配边界：数据库仍保存完整 payload_json，UI 只接收稳定摘要字段。
CaseOrderServiceResult CaseOrderServiceImpl::ListPatientOrderReadModelJson(const QString& queryKind,
                                                                           char* buffer,
                                                                           int bufferSize) {
    if (queryKind != "orders" && queryKind != "patients") {
        // 内部调用也保留白名单校验，避免后续重构时误把任意类型带到 SQL 分支。
        return Fail(2, "Unsupported patient/order read model kind");
    }

    // 只读取列表转换需要的索引列和完整 JSON，不读取任何扫描模型或二进制文件。
    // updated_at 倒序保证新创建/新修改记录优先出现在案例页前部。
    const QString sql =
        "SELECT order_id, patient_id, case_id, status, payload_json, created_at, updated_at "
        "FROM ms_patient_order ORDER BY updated_at DESC, order_id DESC";

    QByteArray tableJson;
    QString queryError;
    if (!m_databaseAdapter->ExecuteQueryJson(sql,
                                             &tableJson,
                                             &queryError,
                                             1024 * 256,
                                             1024 * 1024 * 32)) {
        // Adapter 已经把底层数据库错误转换为 QString；服务层记录后再复制进固定返回结构。
        WriteLog(LogLevel::Error, "ListPatientOrderReadModelJson", queryError);
        const QByteArray queryErrorUtf8 = queryError.toUtf8();
        return Fail(10002, queryErrorUtf8.constData());
    }

    // DatabaseQtAdapter 返回 {columns, rows} 表格 JSON；这里逐行解析 payload_json，
    // 把数据库列名和存储形式留在服务内部，不泄漏给 MainExe 或 CaseUI。
    const QJsonArray rows = DatabaseQtAdapter::RowsFromTableJson(tableJson);
    QJsonArray orderItems;
    QHash<QString, QJsonObject> patientItemsById;
    QStringList patientOrder;
    QHash<QString, int> patientOrderCounts;

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const QJsonObject databaseRow = rows.at(rowIndex).toObject();
        const QByteArray payloadBytes = databaseRow.value("payload_json").toString().toUtf8();
        QJsonParseError parseError;
        const QJsonDocument payloadDocument = QJsonDocument::fromJson(payloadBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !payloadDocument.isObject()) {
            // 单条历史脏数据不能阻断整个列表；跳过该行并写 Warning，便于后续修复数据。
            WriteLog(LogLevel::Warning,
                     "ListPatientOrderReadModelJson",
                     QString("Skip invalid payload for orderId=%1: %2")
                         .arg(databaseRow.value("order_id").toString(), parseError.errorString()));
            continue;
        }

        const QJsonObject payload = payloadDocument.object();
        const QJsonObject patient = payload.value("patient").toObject();
        const QJsonObject order = payload.value("order").toObject();

        // 数据库索引列是最终回退值，标准 JSON 对象中的业务字段优先。
        const QString orderId = order.value("orderId").toString(
            databaseRow.value("order_id").toString()).trimmed();
        const QString patientId = patient.value("patientId").toString(
            databaseRow.value("patient_id").toString()).trimmed();
        const QString caseId = order.value("caseId").toString(
            databaseRow.value("case_id").toString()).trimmed();
        const QString status = order.value("status").toString(
            payload.value("status").toString(databaseRow.value("status").toString())).trimmed();
        const QString createdAt = order.value("createdAt").toString(
            payload.value("createdAt").toString(databaseRow.value("created_at").toString())).trimmed();
        const QString updatedAt = databaseRow.value("updated_at").toString().trimmed();

        // 订单读模型只保留案例卡片和搜索需要的轻量字段。
        QJsonObject orderItem;
        orderItem.insert("orderId", orderId);
        orderItem.insert("patientId", patientId);
        orderItem.insert("patientName", patient.value("name").toString());
        orderItem.insert("caseId", caseId);
        orderItem.insert("doctorName", order.value("doctor").toString());
        orderItem.insert("labName", order.value("lab").toString());
        orderItem.insert("orderType", order.value("orderType").toString("Restoration"));
        orderItem.insert("status", status.isEmpty() ? "created" : status);
        orderItem.insert("createdAt", createdAt);
        orderItem.insert("updatedAt", updatedAt);
        orderItem.insert("savePath", order.value("savePath").toString());
        orderItems.append(orderItem);

        if (patientId.isEmpty()) {
            // MainExe 正常保存时一定会生成 patientId；这里只跳过无法稳定归并的旧脏记录。
            continue;
        }

        if (!patientItemsById.contains(patientId)) {
            // rows 已按更新时间倒序，第一次遇到某患者时就是该患者最新的一份基本信息。
            QJsonObject patientItem;
            patientItem.insert("patientId", patientId);
            patientItem.insert("patientName", patient.value("name").toString());
            patientItem.insert("gender", patient.value("gender").toString());
            patientItem.insert("age", patient.value("age"));
            patientItem.insert("caseId", caseId);
            patientItem.insert("register_date", createdAt);
            patientItem.insert("updatedAt", updatedAt);
            patientItemsById.insert(patientId, patientItem);
            patientOrder.append(patientId);
            patientOrderCounts.insert(patientId, 0);
        }
        // 同一患者可能有多个订单；计数在归并时累加，供患者列表显示。
        patientOrderCounts.insert(patientId, patientOrderCounts.value(patientId) + 1);
    }

    QJsonArray items;
    if (queryKind == "orders") {
        items = orderItems;
    } else {
        // 使用 patientOrder 保存首次出现顺序，避免直接遍历 QHash 导致患者列表顺序随机变化。
        for (int patientIndex = 0; patientIndex < patientOrder.size(); ++patientIndex) {
            const QString patientId = patientOrder.at(patientIndex);
            QJsonObject patientItem = patientItemsById.value(patientId);
            patientItem.insert("orderCount", patientOrderCounts.value(patientId));
            items.append(patientItem);
        }
    }

    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("source", "CaseOrderService");
    root.insert("query", queryKind == "orders" ? "patientOrder.listOrders" : "patientOrder.listPatients");
    root.insert("generatedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("rowCount", items.size());
    root.insert("items", items);

    WriteLog(LogLevel::Info,
             "ListPatientOrderReadModelJson",
             QString("kind=%1, rowCount=%2").arg(queryKind).arg(items.size()));
    return CopyToBuffer(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)),
                        buffer,
                        bufferSize);
}

// 返回模块版本字符串。
const char* CaseOrderServiceImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 释放服务状态。
// 不关闭 Database/Logger 单例，避免影响同进程其他模块。
void CaseOrderServiceImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CaseOrderService shutdown");
    if (m_logger) {
        // 只刷新日志缓冲，不关闭 Logger；Logger 是进程级单例。
        m_logger->Flush();
    }
    // Database/Logger 都不是本服务创建的对象，这里只清空引用。
    m_databaseAdapter = nullptr;
    m_logger = nullptr;
    m_databaseConfigPath.clear();
    m_logDir.clear();
    m_initialized = false;
}

// 构造成功返回值。
CaseOrderServiceResult CaseOrderServiceImpl::Ok(const char* message) const {
    // 结果结构体每次重新创建，避免返回未初始化内存给调用方。
    CaseOrderServiceResult result;
    // errorCode == 0 是当前服务层成功约定。
    result.errorCode = 0;
    CopyMessage(result.message, sizeof(result.message), message);
    return result;
}

// 构造失败返回值。
// errorCode 当前使用服务局部错误码；只有多个服务出现语义一致的稳定错误合同后，
// 才评估抽取公共错误码，避免把病例业务错误过早固化到通用层。
CaseOrderServiceResult CaseOrderServiceImpl::Fail(int errorCode, const char* message) const {
    // 失败也用同一个结构体返回，调用方只需要检查 errorCode 是否为 0。
    CaseOrderServiceResult result;
    // 非 0 错误码保留给调用方做分支处理；message 给日志/提示使用。
    result.errorCode = errorCode;
    CopyMessage(result.message, sizeof(result.message), message);
    return result;
}

// 将 UTF-8 JSON 文本复制到调用方缓冲区。
// 缓冲区不足时返回失败，避免调用方误以为拿到了完整 JSON。
CaseOrderServiceResult CaseOrderServiceImpl::CopyToBuffer(const QString& text, char* buffer, int bufferSize) const {
    if (!buffer || bufferSize <= 0) {
        // 调用方没有提供有效缓冲区，不能写入结果。
        return Fail(2, "Output buffer is invalid");
    }

    const QByteArray bytes = text.toUtf8();
    // 预留一个字节给 C 字符串结束符。
    const int copySize = qMin(bufferSize - 1, bytes.size());
    // 先清零整个缓冲区，保证调用方按字符串读取时一定有结束符。
    std::memset(buffer, 0, static_cast<size_t>(bufferSize));
    if (copySize > 0) {
        std::memcpy(buffer, bytes.constData(), static_cast<size_t>(copySize));
    }
    // 如果 copySize 小于原始大小，说明结果被截断，必须返回失败。
    return copySize == bytes.size() ? Ok() : Fail(2, "Output buffer is too small");
}

// 将外部传入的参考数据分类统一成内部分类名。
// 只允许白名单分类，避免参考数据查询退化成任意 SQL 入口。
QString CaseOrderServiceImpl::ReferenceCategoryToTable(const QString& category) const {
    // 同义词统一映射到内部分类名，UI/第三方适配器不需要完全记住内部命名。
    if (category == "doctor" || category == "doctors") {
        return "doctor";
    }
    if (category == "clinic" || category == "clinics") {
        return "clinic";
    }
    if (category == "lab" || category == "labs" || category == "dentallab" || category == "dental_labs") {
        return "lab";
    }
    if (category == "operator" || category == "operators") {
        return "operator";
    }
    // 返回空字符串表示不在白名单中，调用方会收到 Unsupported 错误。
    return QString();
}

// 写服务层日志。
// operation 使用稳定英文 key，便于检索；说明文字可保持英文或后续改为更结构化字段。
void CaseOrderServiceImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        // 日志不可用时服务仍可工作，不能因为日志模块缺失影响数据库读写。
        return;
    }
    const QByteArray bytes = content.toUtf8();
    // bytes 必须是局部变量，不能直接 content.toUtf8().constData() 后跨多行使用。
    // 这里 Write 会立即读取 constData()，因此局部 QByteArray 的生命周期足够。
    // 服务层当前没有真实操作员上下文，传空字符串让 Logger 省略 Op 字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// C ABI 导出函数。
// UI、外部适配器和测试宿主通过该函数获取病例订单服务接口。
extern "C" MEYERSCAN_CASEORDERSERVICE_API ICaseOrderService* GetCaseOrderService() {
    // 对外只暴露接口指针，调用方不需要包含实现类头文件。
    return &CaseOrderServiceImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取服务代码版本时不需要初始化数据库适配器或 Logger。
extern "C" MEYERSCAN_CASEORDERSERVICE_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}

// 返回患者订单服务公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return 1;
}
