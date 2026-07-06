#include "RuntimeDataCenterImpl.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <cstring>

namespace {
// RuntimeDataCenter 是只读快照缓存，不应该无限制把大表拖进内存。
// 这里给所有本地 domain 查询设置一个明确上限：从 1MB 起步，不够时倍增到 32MB。
// 如果仍然不够，说明该 domain 已经不适合用“启动期全量快照”，后续应改为分页或服务查询。
const int kInitialQueryJsonBufferSize = 1024 * 1024;
const int kMaxQueryJsonBufferSize = 1024 * 1024 * 32;

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_RuntimeDataCenter";

// 模块版本用于 GetModuleVersion() 和版本清单，必须与 Version.rc 同步维护。
const char* Version = "MeyerScan_RuntimeDataCenter v0.1.0 (2026-06-30)";
}

// 将短消息复制到固定长度返回结构中。
// 这样调用方可以安全按 C 字符串读取，不涉及跨 DLL 字符串释放。
void CopyMessage(char* target, int targetSize, const char* message) {
    // target 是调用方传进来的 C 缓冲区，先做边界检查，避免 DLL 内部写越界。
    if (!target || targetSize <= 0) {
        return;
    }
    // 清零后再复制，保证字符串总有 '\0' 结尾，也不会残留上一次的长消息尾巴。
    std::memset(target, 0, static_cast<size_t>(targetSize));
    if (!message) {
        return;
    }
    // 只复制 targetSize - 1 个字符，最后一位留给 C 字符串结束符。
    std::strncpy(target, message, static_cast<size_t>(targetSize - 1));
}
}

// 返回运行时数据中心单例。
RuntimeDataCenterImpl& RuntimeDataCenterImpl::Instance() {
    static RuntimeDataCenterImpl instance;
    return instance;
}

// 初始化运行时数据中心。
bool RuntimeDataCenterImpl::Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) {
    // Init 会同时修改路径、Logger、DatabaseAdapter 和初始化标记，所以整个函数加锁。
    QMutexLocker locker(&m_mutex);

    // 保存路径字节，后续独立测试宿主或延迟连接数据库时可复用。
    // 路径必须由调用方基于 applicationDirPath() 推导，模块内不使用 currentPath。
    m_databaseConfigPath = QByteArray(databaseConfigPathUtf8 ? databaseConfigPathUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // Logger 是进程级单例，本模块只缓存指针并复用，不拥有其生命周期。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    // RuntimeDataCenter 通过 DatabaseQtAdapter 访问纯 C++ Database。
    // QString SQL 和 UTF-8/POD Database 接口之间的转换集中在适配层。
    m_databaseAdapter = GetDatabaseQtAdapter();
    if (!m_databaseAdapter) {
        WriteLog(LogLevel::Error, "Init", "DatabaseQtAdapter instance unavailable");
        return false;
    }

    m_initialized = true;
    WriteLog(LogLevel::Info, "Init", "RuntimeDataCenter initialized");
    return true;
}

// 重新加载全部本地/云端 domain。
RuntimeDataCenterResult RuntimeDataCenterImpl::ReloadAll() {
    if (!m_initialized) {
        // 所有公开方法先检查 Init 状态，避免调用方忘记初始化后读到半空对象。
        return Fail(5, "RuntimeDataCenter is not initialized");
    }

    int failedCount = 0;
    // KnownDomains 返回逻辑域名列表；这里不直接写旧表名，是为了隔离 UI 和数据库表结构。
    const QStringList domains = KnownDomains();
    for (const QString& domain : domains) {
        // 每个 domain 单独加载，避免某张旧表缺失导致全部缓存不可用。
        const QByteArray domainBytes = domain.toUtf8();
        RuntimeDataCenterResult result = ReloadDomain(domainBytes.constData());
        if (result.IsError()) {
            // 这里累计失败数而不是立即返回，是为了尽量让其它可用 domain 仍然有缓存。
            ++failedCount;
        }
    }

    if (failedCount > 0) {
        WriteLog(LogLevel::Warning, "ReloadAll",
                 QString("Reload completed with %1 failed domains").arg(failedCount));
        return Fail(10002, "RuntimeDataCenter reload completed with warnings");
    }

    WriteLog(LogLevel::Info, "ReloadAll", "All runtime domains reloaded");
    return Ok("All runtime domains reloaded");
}

// 重新加载指定 domain。
RuntimeDataCenterResult RuntimeDataCenterImpl::ReloadDomain(const char* domainUtf8) {
    if (!m_initialized) {
        return Fail(5, "RuntimeDataCenter is not initialized");
    }
    // 外部传入 UTF-8 domain 名，先转 QString 并去掉首尾空白，减少配置书写小错误。
    const QString domain = QString::fromUtf8(domainUtf8 ? domainUtf8 : "").trimmed();
    if (domain.isEmpty()) {
        return Fail(2, "domainUtf8 is empty");
    }

    if (IsCloudDomain(domain)) {
        // 云端 domain 由 UpdateCloudClinicJson 注入；ReloadDomain 只保证有默认空快照。
        // 只在访问 m_domainCache 时加锁，避免把无关逻辑放到临界区里。
        QMutexLocker locker(&m_mutex);
        if (!m_domainCache.contains(domain)) {
            // notLoaded 与 queryFailed 区分开，调用方能判断这是“尚未同步云端”而不是数据库错误。
            m_domainCache.insert(domain, BuildEmptyDomainJson(domain, "notLoaded", "Cloud data has not been injected"));
        }
        return Ok("Cloud domain is cached");
    }

    if (!KnownDomains().contains(domain)) {
        return Fail(2, "Unsupported runtime data domain");
    }
    if (!EnsureDatabaseReady()) {
        QMutexLocker locker(&m_mutex);
        m_domainCache.insert(domain, BuildEmptyDomainJson(domain, "databaseUnavailable", "Database is not ready"));
        return Fail(10001, "Database is not ready");
    }

    QString lastError;
    // 一个 domain 可以对应多个旧表名，循环尝试可兼容不同历史版本数据库。
    const QStringList tables = TablesForDomain(domain);
    for (const QString& tableName : tables) {
        QByteArray tableJson;
        if (QueryTableJson(tableName, &tableJson, &lastError)) {
            // 第一个可查询成功的旧表即作为该 domain 来源，用于兼容旧版本表名差异。
            const QString wrappedJson = WrapTableJson(domain, tableName, tableJson, "ok", QString());
            QMutexLocker locker(&m_mutex);
            // QHash::insert 会覆盖同名 domain，保证刷新后读到的是最新快照。
            m_domainCache.insert(domain, wrappedJson);
            WriteLog(LogLevel::Info, "ReloadDomain",
                     QString("%1 loaded from %2").arg(domain, tableName));
            return Ok("Runtime domain reloaded");
        }
    }

    // 全部候选旧表都失败时缓存空数组，UI 仍能安全显示空列表。
    {
        QMutexLocker locker(&m_mutex);
        m_domainCache.insert(domain, BuildEmptyDomainJson(domain, "queryFailed", lastError));
    }
    WriteLog(LogLevel::Warning, "ReloadDomain",
             QString("%1 load failed: %2").arg(domain, lastError));
    return Fail(10002, "Runtime domain query failed");
}

// 获取指定 domain 的 JSON 快照。
RuntimeDataCenterResult RuntimeDataCenterImpl::GetDomainJson(const char* domainUtf8,
                                                             char* buffer,
                                                             int bufferSize) {
    // GetDomainJson 是读接口，不直接暴露 QJsonDocument/QJsonObject，避免 Qt 对象跨 DLL 传递。
    const QString domain = QString::fromUtf8(domainUtf8 ? domainUtf8 : "").trimmed();
    if (domain.isEmpty()) {
        return Fail(2, "domainUtf8 is empty");
    }

    QString json;
    {
        QMutexLocker locker(&m_mutex);
        // value 不存在时返回空 QString；拷贝出来后马上释放锁，避免 JSON 复制期间长期占锁。
        json = m_domainCache.value(domain);
    }

    if (json.isEmpty()) {
        // 调用方未显式 Reload 时尝试懒加载一次，降低使用门槛。
        // 云端 domain 尚未注入时，ReloadDomain 会补一个 notLoaded 空快照，
        // 这样 UI/主程序可以按统一 JSON 结构读取，不需要为云端数据额外写分支。
        const QByteArray domainBytes = domain.toUtf8();
        ReloadDomain(domainBytes.constData());
        QMutexLocker locker(&m_mutex);
        json = m_domainCache.value(domain);
    }

    if (json.isEmpty()) {
        return Fail(10006, "Runtime domain cache not found");
    }

    return CopyToBuffer(json, buffer, bufferSize);
}

// 更新云端诊所信息快照。
RuntimeDataCenterResult RuntimeDataCenterImpl::UpdateCloudClinicJson(const char* cloudClinicJsonUtf8) {
    if (!m_initialized) {
        return Fail(5, "RuntimeDataCenter is not initialized");
    }
    if (!cloudClinicJsonUtf8 || !cloudClinicJsonUtf8[0]) {
        return Fail(2, "cloudClinicJsonUtf8 is empty");
    }

    QJsonParseError parseError;
    // 云端数据先解析为 JSON 对象，再包成 RuntimeDataCenter 统一快照格式。
    // 这样 SettingsUI/其它模块读取 cloud domain 时不用关心数据来自云端还是本地。
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(cloudClinicJsonUtf8), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return Fail(2, "cloudClinicJsonUtf8 must be a JSON object");
    }

    QJsonObject root;
    // root 是统一 envelope，profile 是原始云端诊所对象。
    // 统一 envelope 让本地数据库快照和云端快照拥有同样的状态字段。
    root.insert("schemaVersion", 1);
    root.insert("domain", "cloud.clinicProfile");
    root.insert("source", ModuleInfo::Name);
    root.insert("loadedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("loadStatus", "ok");
    root.insert("profile", document.object());

    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    {
        QMutexLocker locker(&m_mutex);
        // 整个 domain 快照一次性替换，读者要么看到旧快照，要么看到新快照，不会看到半写状态。
        m_domainCache.insert("cloud.clinicProfile", json);
    }

    WriteLog(LogLevel::Info, "UpdateCloudClinicJson", "Cloud clinic profile updated");
    return Ok("Cloud clinic profile updated");
}

// 返回模块版本字符串。
const char* RuntimeDataCenterImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭模块并释放缓存。
void RuntimeDataCenterImpl::Shutdown() {
    // 先写日志再清空 m_logger，便于排查模块关闭时机。
    WriteLog(LogLevel::Info, "Shutdown", "RuntimeDataCenter shutdown");
    if (m_logger) {
        // 只 Flush，不 Shutdown Logger；Logger 生命周期由 MainExe 管理。
        m_logger->Flush();
    }

    QMutexLocker locker(&m_mutex);
    // 清空缓存释放内存，尤其是订单/患者列表快照可能比配置类 domain 大得多。
    m_domainCache.clear();
    m_databaseAdapter = nullptr;
    m_logger = nullptr;
    m_databaseConfigPath.clear();
    m_logDir.clear();
    m_initialized = false;
}

// 构造成功返回值。
RuntimeDataCenterResult RuntimeDataCenterImpl::Ok(const char* message) const {
    RuntimeDataCenterResult result;
    // errorCode=0 是所有 Result 结构约定的成功状态。
    result.errorCode = 0;
    CopyMessage(result.message, sizeof(result.message), message);
    return result;
}

// 构造失败返回值。
RuntimeDataCenterResult RuntimeDataCenterImpl::Fail(int errorCode, const char* message) const {
    RuntimeDataCenterResult result;
    // 非 0 错误码给调用方做分支判断，message 给日志和人工排查使用。
    result.errorCode = errorCode;
    CopyMessage(result.message, sizeof(result.message), message);
    return result;
}

// 将 QString 复制到调用方缓冲区。
RuntimeDataCenterResult RuntimeDataCenterImpl::CopyToBuffer(const QString& text,
                                                            char* buffer,
                                                            int bufferSize) const {
    // 跨 DLL 返回字符串时，不能把 QString / QByteArray 直接返回给调用方。
    // 因为对象内存由哪个 CRT/Qt 实例释放会变复杂；这里改为“调用方给缓冲区，本模块只复制字节”。
    if (!buffer || bufferSize <= 0) {
        return Fail(2, "Output buffer is invalid");
    }

    // 统一转换成 UTF-8，保证 C++/Qt/后续非 Qt 调用方都能按普通字节串解析 JSON。
    const QByteArray bytes = text.toUtf8();
    // 预留 1 个字节给 '\0'，这样调用方可以直接把 buffer 当 C 字符串使用。
    const int copySize = qMin(bufferSize - 1, bytes.size());
    // 先清零整块缓冲区，能避免调用方读到上一次残留内容，也天然补上字符串结尾。
    std::memset(buffer, 0, static_cast<size_t>(bufferSize));
    if (copySize > 0) {
        // memcpy 只复制 UTF-8 原始字节，不做字符级截断；如果缓冲区不足，返回错误让调用方扩容重试。
        // 如果这里截断了多字节 UTF-8，下面会返回 buffer too small，调用方不应使用截断内容。
        std::memcpy(buffer, bytes.constData(), static_cast<size_t>(copySize));
    }

    // copySize 等于 bytes.size() 才表示完整复制；否则说明 JSON 被截断，必须让调用方知道。
    return copySize == bytes.size() ? Ok("Runtime domain copied") : Fail(2, "Output buffer is too small");
}

// 确保数据库可用。
bool RuntimeDataCenterImpl::EnsureDatabaseReady() {
    // RuntimeDataCenter 不拥有 Database，只从 DatabaseQtAdapter 获取统一适配入口。
    // 这样 Qt 类型转换不会散落到各个函数中。
    if (!m_databaseAdapter) {
        // 允许 Init 时未拿到适配层、后续 DLL 可用后再次尝试。
        m_databaseAdapter = GetDatabaseQtAdapter();
    }
    if (!m_databaseAdapter) {
        return false;
    }
    // 如果 MainExe 已经完成连接，直接复用，避免重复 Init/Connect 改变连接状态。
    if (m_databaseAdapter->IsConnected()) {
        return true;
    }
    // 没有配置路径时不能猜测默认路径，防止第三方拉起时误读开发目录或当前工作目录。
    if (m_databaseConfigPath.isEmpty()) {
        return false;
    }

    // 测试宿主或独立模块运行时，MainExe 可能尚未初始化 Database。
    // 这里保留最小 Init/Connect 能力，但通过适配层完成 QString -> UTF-8 转换。
    QString databaseError;
    // 把 Init 保存的 UTF-8 路径还原为 QString，再交给 Adapter 统一处理 Qt/C++ 类型转换。
    if (!m_databaseAdapter->EnsureConnected(QString::fromUtf8(m_databaseConfigPath), &databaseError)) {
        WriteLog(LogLevel::Error, "EnsureDatabaseReady", databaseError);
        return false;
    }
    // 再问一次 IsConnected，而不是只信 EnsureConnected 返回值，避免底层连接对象实际未打开但返回结构误判。
    return m_databaseAdapter->IsConnected();
}

// 判断是否为云端 domain。
bool RuntimeDataCenterImpl::IsCloudDomain(const QString& domain) const {
    return domain == "cloud.clinicProfile";
}

// 返回支持的 domain。
QStringList RuntimeDataCenterImpl::KnownDomains() const {
    // domain 是对外稳定契约，调用方只认这些逻辑名字，不需要知道旧数据库表名。
    // 后续旧表迁移或表名变化时，只改 TablesForDomain/查询映射，不改 UI 和 MainExe。
    return QStringList()
        // 本地诊所信息，来源旧表 clinic_tbl。
        << "local.clinics"
        // 本地技工所信息，来源旧表 lab_tbl2/lab_tbl。
        << "local.labs"
        // 本地软件基础信息，来源旧表 meyer_scan。
        << "local.software"
        // 医生基础信息，来源旧表 dentist_tbl。
        << "local.doctors"
        // 软件初始化/设置类信息，来源旧表 soft_init。
        << "local.settings"
        // 本地账号信息，兼容 user_tbl/user_tbl2。
        << "local.users"
        // 订单摘要信息，来源旧表 order_tbl2/order_tbl。
        << "local.orders"
        // 患者信息，来源旧表 patient_tbl2/patient_tbl。
        << "local.patients"
        // 设备信息，来源旧表 device_info_tbl2/device_info_tbl。
        << "local.devices"
        // 云端诊所 profile，由登录/同步流程注入，不从本地数据库读取。
        << "cloud.clinicProfile";
}

// 根据 domain 返回旧表候选列表。
QStringList RuntimeDataCenterImpl::TablesForDomain(const QString& domain) const {
    // 每个 domain 可以有多个候选旧表，是为了兼容旧版本数据库表名差异。
    // ReloadDomain 会按顺序尝试，第一张可查询成功的表就作为该 domain 的来源。
    if (domain == "local.clinics") {
        return QStringList() << "clinic_tbl";
    }
    if (domain == "local.labs") {
        return QStringList() << "lab_tbl2" << "lab_tbl";
    }
    if (domain == "local.software") {
        return QStringList() << "meyer_scan";
    }
    if (domain == "local.doctors") {
        return QStringList() << "dentist_tbl";
    }
    if (domain == "local.settings") {
        return QStringList() << "soft_init";
    }
    if (domain == "local.users") {
        return QStringList() << "user_tbl" << "user_tbl2";
    }
    if (domain == "local.orders") {
        return QStringList() << "order_tbl2" << "order_tbl";
    }
    if (domain == "local.patients") {
        return QStringList() << "patient_tbl2" << "patient_tbl";
    }
    if (domain == "local.devices") {
        return QStringList() << "device_info_tbl2" << "device_info_tbl";
    }
    return QStringList();
}

// 按旧表名生成安全 SELECT。
QString RuntimeDataCenterImpl::SelectSqlForTable(const QString& tableName) const {
    // 大多数旧表字段数量较少，SELECT * 更利于后续字段自然扩展。
    // order_tbl2 含多段 mediumtext 扫描变换数据，不适合作为列表缓存全部读入内存。
    if (tableName == "order_tbl2") {
        // 这里显式列出订单列表需要的轻量字段，避免把大块扫描矩阵/二进制文本读入 RuntimeDataCenter。
        // 这是一种“读模型裁剪”：列表页需要摘要，完整订单详情以后走 CaseOrderService 分页/详情查询。
        return "SELECT ORDER_ID, APPOINT_DATE, APPOINT_TIEM, PATIENT_ID, PATIENT_NAME, "
               "LAB_ID, DELIVERY_DATE, DENTIST_ID, SAVE_PATH, ORDER_STATE, REMARK, "
               "ORDER_TYPE, ORDER_DATE, ORDER_TIME, SEND_DATETIME, CLOUDORDERID, "
               "ORDER_ISCOMPETE, MYCLOUD_PATIENT_ID, MYCLOUD_ORDER_ID, DEVICE_ID, "
               "MYCLOUD_CLINIC_ID, MYCLOUD_SEND_LAB_ID, ORDER_SEND_LAB_NAME, "
               "ACCESSION_NUMBER, PHYSICIAN_NAME, STUDY_DATE, STUDY_TIME "
               "FROM `order_tbl2` ORDER BY ORDER_DATE DESC, ORDER_TIME DESC";
    }
    if (tableName == "order_tbl") {
        // 旧 order_tbl 暂无字段裁剪规则，先按旧表全量读取；后续如发现大字段再单独裁剪。
        return "SELECT * FROM `order_tbl` ORDER BY APPOINT_DATE DESC";
    }

    // 其他旧表先读取全字段，字段新增后缓存 JSON 自动携带新字段，调用方按需读取。
    // tableName 来自 TablesForDomain 白名单，不接受外部输入，因此这里拼 SQL 是可控的。
    return QString("SELECT * FROM `%1`").arg(tableName);
}

// 查询某个旧表并返回 Database 的通用表格 JSON。
bool RuntimeDataCenterImpl::QueryTableJson(const QString& tableName,
                                           QByteArray* output,
                                           QString* errorMessage) const {
    // output 由调用方传入，函数内部只负责填充；这样失败时调用方可以保留自己的上下文。
    if (!m_databaseAdapter || !output) {
        if (errorMessage) {
            *errorMessage = "Database adapter or output buffer is invalid";
        }
        return false;
    }

    // tableName 来自本模块白名单，不接收外部输入，因此 SelectSqlForTable 内的 SQL 可审查。
    // 这里不接受调用方传 SQL，防止 RuntimeDataCenter 退化成任意查询通道。
    const QString sql = SelectSqlForTable(tableName);

    // 字段会经常扩展，固定小缓冲很容易让 UI 误以为“没有数据”。
    // 因此这里采用有限重试：缓冲区不足时倍增重查；达到上限后仍失败才返回错误。
    for (int bufferSize = kInitialQueryJsonBufferSize;
         bufferSize <= kMaxQueryJsonBufferSize;
         bufferSize *= 2) {
        // 每一轮重新创建缓冲区，避免失败重试时残留上一次内容。
        QByteArray jsonBuffer;
        QString queryError;
        if (m_databaseAdapter->ExecuteQueryJson(sql,
                                                &jsonBuffer,
                                                &queryError,
                                                bufferSize,
                                                bufferSize)) {
            // Adapter 返回的是已经按真实长度截断后的 UTF-8 JSON，不包含尾部预分配空字节。
            *output = jsonBuffer;
            return true;
        }

        // 只有缓冲区不足才允许扩大后重试；缺表、字段不存在、SQL 错误等不能重复刷日志。
        if (!queryError.contains("too small", Qt::CaseInsensitive) &&
            !queryError.contains("larger than", Qt::CaseInsensitive)) {
            if (errorMessage) {
                // 错误消息带上表名，ReloadDomain 日志能直接定位是哪张候选表失败。
                *errorMessage = QString("%1: %2").arg(tableName, queryError);
            }
            return false;
        }

        if (bufferSize == kMaxQueryJsonBufferSize) {
            if (errorMessage) {
                // 达到上限仍放不下，说明该 domain 不应继续走全量快照，应改分页或详情服务。
                *errorMessage = QString("%1: JSON output is larger than %2 bytes")
                    .arg(tableName)
                    .arg(kMaxQueryJsonBufferSize);
            }
            return false;
        }

        // 走到这里说明只是缓冲区太小，for 循环会把 bufferSize 翻倍后重试同一条查询。
    }

    if (errorMessage) {
        *errorMessage = QString("%1: query failed").arg(tableName);
    }
    return false;
}

// 包装查询结果为运行时 domain 快照。
QString RuntimeDataCenterImpl::WrapTableJson(const QString& domain,
                                             const QString& tableName,
                                             const QByteArray& tableJson,
                                             const QString& loadStatus,
                                             const QString& lastError) const {
    // Database 返回的是通用表格 JSON：columns + rows + rowCount。
    // RuntimeDataCenter 再包一层 domain 元数据，让 UI 知道数据来源、加载时间和状态。
    const QJsonDocument tableDocument = QJsonDocument::fromJson(tableJson);
    // 如果 tableJson 解析失败，object() 会得到空对象；后续 value() 会返回默认空值，
    // 调用方仍能得到结构完整的空快照，而不是崩溃。
    const QJsonObject tableObject = tableDocument.object();

    QJsonObject root;
    // schemaVersion 用来给未来字段结构升级留钩子，调用方可以按版本做兼容解析。
    root.insert("schemaVersion", 1);
    root.insert("domain", domain);
    root.insert("source", ModuleInfo::Name);
    root.insert("loadedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("databaseTable", tableName);
    root.insert("loadStatus", loadStatus);
    root.insert("lastError", lastError);
    root.insert("rowCount", tableObject.value("rowCount").toInt());
    // columns 保留数据库字段名，便于 UI 以外的调试工具直接查看原始列。
    root.insert("columns", tableObject.value("columns").toArray());
    // items 是外部模块主要读取的数组，字段新增时会自然出现在每个 item 对象中。
    root.insert("items", tableObject.value("rows").toArray());

    // Compact JSON 体积更小，适合跨 DLL 缓冲区传递；调试时可用日志或工具格式化查看。
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// 构造空 domain 快照。
QString RuntimeDataCenterImpl::BuildEmptyDomainJson(const QString& domain,
                                                    const QString& loadStatus,
                                                    const QString& lastError) const {
    // 空快照也保持和正常快照完全相同的字段结构。
    // UI 只需要读取 items 数组，不必为“没加载、缺表、数据库不可用”分别写不同解析代码。
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("domain", domain);
    root.insert("source", ModuleInfo::Name);
    root.insert("loadedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("databaseTable", QString());
    root.insert("loadStatus", loadStatus);
    root.insert("lastError", lastError);
    root.insert("rowCount", 0);
    // 空快照也返回数组字段，UI 可以始终按数组遍历，不用判空字段是否存在。
    root.insert("columns", QJsonArray());
    root.insert("items", QJsonArray());
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// 写结构化日志。
void RuntimeDataCenterImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // 日志是辅助能力，不能因为 Logger 未初始化影响数据读取。
    if (!m_logger) {
        return;
    }
    // Logger 的 Qt 便利重载在头文件内联层完成 UTF-8 转换，跨 DLL 仍走 const char* ABI。
    m_logger->Write(level,
                    QString::fromLatin1(ModuleInfo::Name),
                    QString::fromLatin1(operation ? operation : ""),
                    QString(),
                    QString(),
                    QString(),
                    content);
}

// C ABI 工厂函数。
extern "C" MEYERSCAN_RUNTIMEDATACENTER_API IRuntimeDataCenter* GetRuntimeDataCenter() {
    return &RuntimeDataCenterImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取 RuntimeDataCenter 代码版本时不加载任何 domain 缓存。
extern "C" MEYERSCAN_RUNTIMEDATACENTER_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
