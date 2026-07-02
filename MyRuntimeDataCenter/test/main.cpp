#include "RuntimeDataCenter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cstdio>
#include <cstring>

#include "Database.h"
#include "Logger.h"

namespace {

// 写出测试用 db_config.json。
// 测试配置放在测试程序输出目录下，避免污染正式发布 config。
QString WriteTestConfig(const QString& baseDir) {
    // baseDir 是 RuntimeDataCenterTest.exe 所在目录，测试配置和测试库都放在它下面。
    QDir dir(baseDir);
    // config 用于模拟发布目录配置结构。
    dir.mkpath("config");
    // Data 用于保存 SQLite 测试库，避免写入源码目录或系统目录。
    dir.mkpath("Data");

    const QString configPath = dir.filePath("config/db_config.json");
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        // QTextStream 设置 UTF-8，确保未来配置里出现中文路径也能正确写入。
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        // 测试强制使用 SQLite，避免开发机没有 MySQL 服务时影响 RuntimeDataCenter 链路验证。
        // sqlitePath 使用相对路径，验证 Database 能按配置文件所在目录解析数据文件。
        stream << "{\n"
               << "  \"databaseType\": \"sqlite\",\n"
               << "  \"mysql\": {\n"
               << "    \"host\": \"127.0.0.1\",\n"
               << "    \"port\": 3308,\n"
               << "    \"service\": \"MSCANDB\",\n"
               << "    \"database\": \"mscan\",\n"
               << "    \"dataDir\": \"../MySQL/data/mscan\"\n"
               << "  },\n"
               << "  \"sqlitePath\": \"../Data/RuntimeDataCenterTest.db\"\n"
               << "}\n";
    }
    // 即使写文件失败也返回目标路径，后续 Database::Init 会给出明确失败原因。
    return configPath;
}

// 执行一条建表或写入 SQL。
// 这里直接使用 Database 是测试准备步骤，不属于 UI 正式业务访问模式。
bool ExecSql(IDatabase* database, const char* sql) {
    // ExecuteUpdate 适合建表、插入、更新、删除这类不需要结果集的 SQL。
    Result<DbResult> result = database->ExecuteUpdate(sql);
    if (result.IsError()) {
        // stderr 让批量脚本能在失败日志里直接看到 SQL 错误。
        std::fprintf(stderr, "SQL failed: %s\n", result.message ? result.message : "unknown");
        return false;
    }
    return true;
}

// 创建 RuntimeDataCenter 需要读取的最小旧表。
// 每张表只放 smoke 验证所需字段，正式旧库字段仍以 MyCaseManager/mysql.sql 为参考。
bool PrepareSchema(IDatabase* database) {
    // RuntimeDataCenter 读取的是旧库表名；这里创建最小字段集合模拟旧库。
    const char* scripts[] = {
        // local.clinics domain 的候选表，测试只保留读取快照需要的最小字段。
        "CREATE TABLE IF NOT EXISTS clinic_tbl (CLINIC_ID TEXT PRIMARY KEY, CLINIC_NAME TEXT, CLINIC_CONTACT TEXT)",
        // local.labs domain 的主候选表。
        "CREATE TABLE IF NOT EXISTS lab_tbl2 (LAB_ID TEXT PRIMARY KEY, LAB_NAME TEXT, LAB_CLOUDID TEXT)",
        // local.software domain 的来源表。
        "CREATE TABLE IF NOT EXISTS meyer_scan (NAME TEXT, VERSION TEXT, COMPANY TEXT)",
        // local.doctors domain 的来源表。
        "CREATE TABLE IF NOT EXISTS dentist_tbl (DENTIST_ID TEXT PRIMARY KEY, DENTIST_NAME TEXT, CLINIC_ID TEXT)",
        // local.settings domain 的来源表。
        "CREATE TABLE IF NOT EXISTS soft_init (ID INTEGER PRIMARY KEY, SWITCH INTEGER, DEFAULT_ORDER_TYPE TEXT)",
        // local.users domain 的候选表。
        "CREATE TABLE IF NOT EXISTS user_tbl (ID INTEGER PRIMARY KEY, USER_NAME TEXT, USER_PASS TEXT, USER_CELLPHONE TEXT)",
        // local.patients domain 的来源表。
        "CREATE TABLE IF NOT EXISTS patient_tbl2 (PATIENT_ID TEXT PRIMARY KEY, PATIENT_NAME TEXT, PATIENT_GENDER TEXT, PATIENT_SEX INTEGER, PATIENT_PHONE TEXT, PATIENT_AGE TEXT)",
        // local.devices domain 的来源表。
        "CREATE TABLE IF NOT EXISTS device_info_tbl2 (DEVICE_ID TEXT PRIMARY KEY, DURATION_CODE TEXT, IS_TEMP TEXT)",
        // local.orders domain 需要订单摘要字段；完整扫描大字段不放在测试表里。
        "CREATE TABLE IF NOT EXISTS order_tbl2 (ORDER_ID TEXT PRIMARY KEY, APPOINT_DATE TEXT, APPOINT_TIEM TEXT, PATIENT_ID TEXT, PATIENT_NAME TEXT, LAB_ID TEXT, DELIVERY_DATE TEXT, DENTIST_ID INTEGER, SAVE_PATH TEXT, ORDER_STATE INTEGER, REMARK TEXT, ORDER_TYPE TEXT, ORDER_DATE TEXT, ORDER_TIME TEXT, SEND_DATETIME TEXT, CLOUDORDERID INTEGER, ORDER_ISCOMPETE INTEGER, MYCLOUD_PATIENT_ID TEXT, MYCLOUD_ORDER_ID TEXT, DEVICE_ID TEXT, MYCLOUD_CLINIC_ID TEXT, MYCLOUD_SEND_LAB_ID TEXT, ORDER_SEND_LAB_NAME TEXT, ACCESSION_NUMBER TEXT, PHYSICIAN_NAME TEXT, STUDY_DATE TEXT, STUDY_TIME TEXT)"
    };

    for (const char* sql : scripts) {
        // 建表逐条执行，失败时立即返回，便于定位是哪张表定义有问题。
        if (!ExecSql(database, sql)) {
            return false;
        }
    }

    const char* inserts[] = {
        // REPLACE 保证测试可重复运行；主键相同时覆盖旧演示数据。
        "REPLACE INTO clinic_tbl (CLINIC_ID, CLINIC_NAME, CLINIC_CONTACT) VALUES ('C001', 'Demo Clinic', 'Alice')",
        "REPLACE INTO lab_tbl2 (LAB_ID, LAB_NAME, LAB_CLOUDID) VALUES ('L001', 'Demo Lab', 'cloud-lab-001')",
        // meyer_scan 没有主键，先 DELETE 再 INSERT 保证只有一行演示数据。
        "DELETE FROM meyer_scan",
        "INSERT INTO meyer_scan (NAME, VERSION, COMPANY) VALUES ('MeyerScan', '0.1.0', 'Meyer')",
        "REPLACE INTO dentist_tbl (DENTIST_ID, DENTIST_NAME, CLINIC_ID) VALUES ('D001', 'Dr Demo', 'C001')",
        "REPLACE INTO soft_init (ID, SWITCH, DEFAULT_ORDER_TYPE) VALUES (1, 0, '0')",
        "REPLACE INTO user_tbl (ID, USER_NAME, USER_PASS, USER_CELLPHONE) VALUES (1, 'admin', 'pass', '10086')",
        "REPLACE INTO patient_tbl2 (PATIENT_ID, PATIENT_NAME, PATIENT_GENDER, PATIENT_SEX, PATIENT_PHONE, PATIENT_AGE) VALUES ('P001', 'Patient Demo', 'O', 2, '10010', '30')",
        "REPLACE INTO device_info_tbl2 (DEVICE_ID, DURATION_CODE, IS_TEMP) VALUES ('SN001', 'duration', '0')",
        "REPLACE INTO order_tbl2 (ORDER_ID, APPOINT_DATE, APPOINT_TIEM, PATIENT_ID, PATIENT_NAME, LAB_ID, DELIVERY_DATE, DENTIST_ID, SAVE_PATH, ORDER_STATE, REMARK, ORDER_TYPE, ORDER_DATE, ORDER_TIME, SEND_DATETIME, CLOUDORDERID, ORDER_ISCOMPETE, MYCLOUD_PATIENT_ID, MYCLOUD_ORDER_ID, DEVICE_ID, MYCLOUD_CLINIC_ID, MYCLOUD_SEND_LAB_ID, ORDER_SEND_LAB_NAME, ACCESSION_NUMBER, PHYSICIAN_NAME, STUDY_DATE, STUDY_TIME) VALUES ('O001', '2026-06-30', '09:00:00', 'P001', 'Patient Demo', 'L001', '2026-07-01', 1, 'case/O001', 0, '', '0', '20260630', '090000', '', 1, 0, 'cp001', 'co001', 'SN001', 'cc001', 'cl001', 'Demo Lab', '', '', '', '')"
    };

    for (const char* sql : inserts) {
        // REPLACE INTO 让测试可重复运行，不会因为主键已存在失败。
        if (!ExecSql(database, sql)) {
            return false;
        }
    }
    return true;
}

// 简单断言函数。
// 失败时打印错误，main 根据返回值决定退出码。
bool Check(bool condition, const char* message) {
    if (condition) {
        // stdout 用于正常测试进度，stderr 只放失败信息。
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// RuntimeDataCenter 测试宿主入口。
// 覆盖 SQLite 默认链路、本地旧表快照读取、云端诊所 JSON 注入和缓冲区读取。
int main(int argc, char* argv[]) {
    // RuntimeDataCenter/Database/Logger 使用 Qt Core，不需要 QWidget，因此 QCoreApplication 足够。
    QCoreApplication app(argc, argv);

    // 所有测试产物都基于 EXE 目录生成，避免 currentPath 影响路径。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString configPath = WriteTestConfig(appDir);
    const QString logDir = QDir(appDir).filePath("logs");
    // 日志目录不存在时创建；Logger 不负责猜测或创建上层业务目录。
    QDir().mkpath(logDir);

    ILogger* logger = GetLogger();
    if (logger) {
        // Logger 接口接收 UTF-8 const char*；toUtf8 临时 QByteArray 在本函数调用期间有效。
        logger->Init(logDir.toUtf8().constData(), LogLevel::Info);
    }

    IDatabase* database = GetDatabase();
    if (!Check(database != nullptr, "Database instance exists")) {
        // Database 是 RuntimeDataCenter 的下游依赖，拿不到数据库单例就无法继续。
        return 1;
    }
    if (!Check(database->Init(configPath.toUtf8().constData()).IsSuccess(), "Database init succeeds")) {
        return 1;
    }
    // Connect 会根据 db_config.json 里的 databaseType 选择 SQLite 驱动。
    if (!Check(database->Connect().IsSuccess(), "SQLite database connects")) {
        return 1;
    }
    if (!Check(PrepareSchema(database), "Test schema prepared")) {
        return 1;
    }

    IRuntimeDataCenter* dataCenter = GetRuntimeDataCenter();
    if (!Check(dataCenter != nullptr, "RuntimeDataCenter instance exists")) {
        // 工厂函数返回空通常说明 DLL 导出或依赖加载失败。
        return 1;
    }
    if (!Check(dataCenter->Init(configPath.toUtf8().constData(), logDir.toUtf8().constData()), "RuntimeDataCenter init succeeds")) {
        return 1;
    }
    // ReloadAll 会遍历本地和云端 domain，本地从数据库读，云端生成 notLoaded 空快照。
    if (!Check(dataCenter->ReloadAll().IsSuccess(), "RuntimeDataCenter reloads all domains")) {
        return 1;
    }

    // 调用方缓冲区模式：测试方分配 256KB，RuntimeDataCenter 只负责复制 JSON。
    // 这种方式避免 RuntimeDataCenter.dll 分配字符串、测试程序或其它 DLL 释放字符串造成 CRT/堆边界问题。
    char buffer[1024 * 256] = {0};
    if (!Check(dataCenter->GetDomainJson("local.patients", buffer, sizeof(buffer)).IsSuccess(), "local.patients snapshot is readable")) {
        return 1;
    }
    // strstr 是最简单的内容验证，说明 JSON 里确实包含测试患者名。
    if (!Check(std::strstr(buffer, "Patient Demo") != nullptr, "local.patients contains demo patient")) {
        return 1;
    }

    // 云端信息不是从本地数据库读，而是由登录/同步流程注入 RuntimeDataCenter。
    // 这里手工构造一段云端诊所 JSON，用来验证“外部注入 -> domain 快照 -> 读取”的完整链路。
    const char* cloudJson = "{\"account\":\"clinic-demo\",\"token\":\"token-demo\",\"partners\":[{\"name\":\"Demo Lab\"}],\"devices\":[{\"deviceId\":\"SN001\"}]}";
    if (!Check(dataCenter->UpdateCloudClinicJson(cloudJson).IsSuccess(), "cloud clinic profile updates")) {
        return 1;
    }
    // 复用 buffer 前先清零，避免后续检查误读上一次 local.patients 残留内容。
    std::memset(buffer, 0, sizeof(buffer));
    if (!Check(dataCenter->GetDomainJson("cloud.clinicProfile", buffer, sizeof(buffer)).IsSuccess(), "cloud.clinicProfile snapshot is readable")) {
        return 1;
    }
    if (!Check(std::strstr(buffer, "clinic-demo") != nullptr, "cloud.clinicProfile contains account")) {
        return 1;
    }

    // 按依赖反向顺序关闭：先关闭数据中心，再关闭数据库，最后关闭日志。
    dataCenter->Shutdown();
    database->Shutdown();
    if (logger) {
        logger->Shutdown();
    }

    std::printf("RuntimeDataCenter smoke passed.\n");
    return 0;
}
