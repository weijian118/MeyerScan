#include "SettingsUI.h"

#include "DatabaseQtAdapter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

// 从测试 EXE 所在目录向上寻找仓库根目录。
// 技术实现：仓库根目录下固定存在 MeyerScan_AllModules.sln，
// 所以该逻辑同时适配单模块输出和根聚合输出。
QString ResolveRepoRoot() {
    // applicationDirPath() 指向 EXE 实际所在目录，不受 VS 调试工作目录影响。
    QDir dir(QCoreApplication::applicationDirPath());
    // 逐级向上搜索根方案文件，命中后就是 F:/MeyerScan。
    while (!dir.isRoot()) {
        if (QFileInfo::exists(dir.filePath("MeyerScan_AllModules.sln"))) {
            return dir.absolutePath();
        }
        dir.cdUp();
    }

    // 保底返回 EXE 目录，避免异常环境下返回空路径。
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

// 解析模块根目录。
// 根聚合输出时 EXE 在 F:/MeyerScan/bin/Release，不能再简单向上两级。
QString ResolveModuleRoot() {
    // 优先通过仓库根目录拼出模块目录，保证总方案和单模块方案行为一致。
    QDir moduleDir(QDir(ResolveRepoRoot()).filePath("MySettingsUI"));
    if (moduleDir.exists()) {
        return moduleDir.absolutePath();
    }

    // 保留旧推导作为兜底，兼容临时拷贝出来的单模块测试目录。
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}

// 解析测试宿主应使用的数据库配置。
// 技术实现：测试宿主在 EXE 输出目录下生成自己的配置文件，
// 并让 sqlitePath 指向独立的 SettingsUITest_<pid>.db。
// 这样做的原因是根输出目录会同时运行多个测试程序，如果大家共用
// config/db_config.json 和同一个 SQLite 文件，旧表结构会互相污染。
QString ResolveDatabaseConfigPath() {
    // applicationDirPath() 指向测试 EXE 所在目录；测试配置只写在这个输出目录下。
    QDir appDir(QCoreApplication::applicationDirPath());
    // config/SettingsUITest 用来放测试专用 db_config.json，避免覆盖正式配置模板。
    if (!appDir.mkpath("config/SettingsUITest")) {
        return QString();
    }
    // Data 用来放测试专用 SQLite 文件，所有测试现场数据都留在输出目录。
    if (!appDir.mkpath("Data")) {
        return QString();
    }

    // PID 让每次测试使用独立数据库文件，即使上一次异常退出留下文件也不会影响本轮。
    const QString databaseFileName = QString("SettingsUITest_%1.db").arg(QCoreApplication::applicationPid());
    const QString configPath = appDir.filePath("config/SettingsUITest/db_config.json");

    // sqlitePath 是相对配置文件所在目录解析的路径：
    // Release/config/SettingsUITest/../../Data/SettingsUITest_<pid>.db -> Release/Data/...
    QByteArray configJson;
    configJson += "{\n";
    configJson += "  \"databaseType\": \"sqlite\",\n";
    configJson += "  \"mysql\": {\n";
    configJson += "    \"host\": \"127.0.0.1\",\n";
    configJson += "    \"port\": 3308,\n";
    configJson += "    \"service\": \"MSCANDB\",\n";
    configJson += "    \"database\": \"mscan\",\n";
    configJson += "    \"dataDir\": \"../MySQL/data/mscan\"\n";
    configJson += "  },\n";
    configJson += "  \"sqlitePath\": \"../../Data/";
    configJson += databaseFileName.toUtf8();
    configJson += "\"\n";
    configJson += "}\n";

    QFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 配置无法写入时直接让调用方失败，不能回退到公共配置造成交叉污染。
        return QString();
    }

    // QFile::write 写入 UTF-8 字节；配置中没有中文，所以不需要额外 BOM。
    configFile.write(configJson);
    configFile.close();
    return configPath;
}

// 解析测试日志目录。
// 单模块运行时写模块 logs；根聚合运行时写仓库 logs，便于批量测试集中排查。
QString ResolveTestLogDir() {
    // 根聚合输出目录形如 F:/MeyerScan/bin/Release，向上两级后就是仓库根。
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    if (QFileInfo::exists(dir.filePath("MeyerScan_AllModules.sln"))) {
        return dir.filePath("logs");
    }

    // 单模块输出目录形如 F:/MeyerScan/MySettingsUI/bin/Release。
    return QDir(ResolveModuleRoot()).filePath("logs");
}

// 准备设置页 Information 标签需要的最小演示数据。
// 测试宿主造数据只用于链路验证，正式 SettingsUI 仍通过 RuntimeDataCenter 读取快照。
bool PrepareRuntimeDemoData(const QString& databaseConfigPath) {
    if (databaseConfigPath.isEmpty()) {
        // 测试配置创建失败时直接失败，避免误连公共数据库或开发机配置。
        return false;
    }

    // 测试宿主也通过 DatabaseQtAdapter 准备数据，保持和正式 Qt 模块一致的调用边界。
    DatabaseQtAdapter* databaseAdapter = GetDatabaseQtAdapter();
    if (!databaseAdapter) {
        // 适配器返回空通常表示 Adapter DLL 没有正确加载，或者底层 Database 依赖缺失。
        return false;
    }

    // 设置页独立测试固定使用 SQLite，避免离线环境依赖 MySQL 服务。
    QString databaseError;
    if (!databaseAdapter->EnsureConnected(QDir::fromNativeSeparators(databaseConfigPath),
                                          "sqlite",
                                          &databaseError)) {
        // 连接失败通常说明 sqlite3.dll、配置路径或数据库目录权限异常。
        return false;
    }

    const char* scripts[] = {
        // 诊所表：Information/Clinics 标签页会读取名称、联系人、电话、城市和地址。
        "CREATE TABLE IF NOT EXISTS clinic_tbl ("
        "CLINIC_ID TEXT PRIMARY KEY,"
        "CLINIC_NAME TEXT,"
        "CLINIC_CONTACT TEXT,"
        "CLINIC_TEL TEXT,"
        "CLINIC_CITY TEXT,"
        "CLINIC_DETAILADDRESS TEXT)",

        // 技工所表：Information/Dental Labs 标签页会读取名称、联系人、电话和地址。
        "CREATE TABLE IF NOT EXISTS lab_tbl2 ("
        "LAB_ID TEXT PRIMARY KEY,"
        "LAB_NAME TEXT,"
        "LAB_ADDRESS TEXT,"
        "LAB_TEL TEXT,"
        "LAB_CONTACT TEXT,"
        "LAB_EMAIL TEXT,"
        "LAB_CLOUDID TEXT)",

        // 医生表：Information/Doctors 标签页会读取医生姓名、性别、电话和专业方向。
        "CREATE TABLE IF NOT EXISTS dentist_tbl ("
        "DENTIST_ID TEXT PRIMARY KEY,"
        "DENTIST_NAME TEXT,"
        "DENTIST_SEX INTEGER,"
        "DENTIST_TEL TEXT,"
        "DENTIST_PRO TEXT,"
        "CLINIC_ID TEXT)",

        // 患者表：当前设置页不直接展示，但 RuntimeDataCenter 全 domain 刷新会读取。
        "CREATE TABLE IF NOT EXISTS patient_tbl2 ("
        "PATIENT_ID TEXT PRIMARY KEY,"
        "PATIENT_NAME TEXT,"
        "PATIENT_GENDER TEXT,"
        "PATIENT_SEX INTEGER,"
        "PATIENT_PHONE TEXT,"
        "PATIENT_AGE TEXT,"
        "PATIENT_ORDERCOUNTS INTEGER,"
        "PATIENT_CREATETIME TEXT,"
        "PATIENT_UPDATETIME TEXT)",

        // 订单表：RuntimeDataCenter 读取订单摘要字段，避免 CaseUI 测试链路缺表。
        "CREATE TABLE IF NOT EXISTS order_tbl2 ("
        "ORDER_ID TEXT PRIMARY KEY,"
        "APPOINT_DATE TEXT,"
        "APPOINT_TIEM TEXT,"
        "PATIENT_ID TEXT,"
        "PATIENT_NAME TEXT,"
        "LAB_ID TEXT,"
        "DELIVERY_DATE TEXT,"
        "DENTIST_ID INTEGER,"
        "SAVE_PATH TEXT,"
        "ORDER_STATE INTEGER,"
        "REMARK TEXT,"
        "ORDER_TYPE TEXT,"
        "ORDER_DATE TEXT,"
        "ORDER_TIME TEXT,"
        "SEND_DATETIME TEXT,"
        "CLOUDORDERID INTEGER,"
        "ORDER_ISCOMPETE INTEGER,"
        "MYCLOUD_PATIENT_ID TEXT,"
        "MYCLOUD_ORDER_ID TEXT,"
        "DEVICE_ID TEXT,"
        "MYCLOUD_CLINIC_ID TEXT,"
        "MYCLOUD_SEND_LAB_ID TEXT,"
        "ORDER_SEND_LAB_NAME TEXT,"
        "ACCESSION_NUMBER TEXT,"
        "PHYSICIAN_NAME TEXT,"
        "STUDY_DATE TEXT,"
        "STUDY_TIME TEXT)",

        // SettingsUI 当前只展示医生、诊所、技工所三类参考数据。
        // 但 RuntimeDataCenter 会统一刷新全部本地 domain，
        // 测试库补齐这些轻量表后，联调日志不会混入预期缺表噪声。
        // 软件信息表：模拟旧库中的软件基础信息 domain。
        "CREATE TABLE IF NOT EXISTS meyer_scan ("
        "NAME TEXT,"
        "VERSION TEXT,"
        "COMPANY TEXT)",

        // 软件设置表：模拟旧库中的 soft_init domain。
        "CREATE TABLE IF NOT EXISTS soft_init ("
        "ID INTEGER PRIMARY KEY,"
        "SWITCH INTEGER,"
        "DEFAULT_ORDER_TYPE TEXT)",

        // 本地账号表：RuntimeDataCenter 会读取 user_tbl 作为 local.users 候选表。
        "CREATE TABLE IF NOT EXISTS user_tbl ("
        "ID INTEGER PRIMARY KEY,"
        "USER_NAME TEXT,"
        "USER_PASS TEXT,"
        "USER_CELLPHONE TEXT)",

        // 第二账号表：兼容旧版本数据库里使用 user_tbl2 的情况。
        "CREATE TABLE IF NOT EXISTS user_tbl2 ("
        "ID INTEGER PRIMARY KEY,"
        "USER_NAME TEXT,"
        "USER_PASS TEXT,"
        "USER_CELLPHONE TEXT)",

        // 设备信息表：后续权限、设置或设备页可读取 local.devices domain。
        "CREATE TABLE IF NOT EXISTS device_info_tbl2 ("
        "DEVICE_ID TEXT PRIMARY KEY,"
        "DEVICE_MODEL TEXT,"
        "DEVICE_STATUS TEXT,"
        "DURATION_CODE TEXT,"
        "IS_TEMP TEXT)",

        // 插入诊所演示数据，保证 Clinics 表格至少有一行可验证内容。
        "REPLACE INTO clinic_tbl (CLINIC_ID, CLINIC_NAME, CLINIC_CONTACT, CLINIC_TEL, CLINIC_CITY, CLINIC_DETAILADDRESS) "
        "VALUES ('C001', 'Meyer Demo Clinic', 'Alice', '010-10001000', 'Beijing', 'No. 1 Demo Road')",

        // 插入技工所演示数据，保证 Dental Labs 表格至少有一行可验证内容。
        "REPLACE INTO lab_tbl2 (LAB_ID, LAB_NAME, LAB_ADDRESS, LAB_TEL, LAB_CONTACT, LAB_EMAIL, LAB_CLOUDID) "
        "VALUES ('L001', 'Meyer Demo Lab', 'No. 8 Lab Road', '021-20002000', 'Bob', 'lab@example.com', 'cloud-lab-001')",

        // 插入医生演示数据，保证 Doctors 表格至少有一行可验证内容。
        "REPLACE INTO dentist_tbl (DENTIST_ID, DENTIST_NAME, DENTIST_SEX, DENTIST_TEL, DENTIST_PRO, CLINIC_ID) "
        "VALUES ('D001', 'Dr. Demo', 1, '13800000000', 'Orthodontics', 'C001')",

        // 患者数据用于 RuntimeDataCenter local.patients domain，也给 CaseUI 复用同一测试库打基础。
        "REPLACE INTO patient_tbl2 (PATIENT_ID, PATIENT_NAME, PATIENT_GENDER, PATIENT_SEX, PATIENT_PHONE, PATIENT_AGE, PATIENT_ORDERCOUNTS, PATIENT_CREATETIME, PATIENT_UPDATETIME) "
        "VALUES ('P001', 'Patient Demo', 'O', 2, '13900000000', '30', 1, '2026-06-30 10:00:00', '2026-06-30 10:00:00')",

        // 订单通过 PATIENT_ID/PATIENT_NAME 与患者形成最小关联，验证订单摘要读取链路。
        "REPLACE INTO order_tbl2 (ORDER_ID, APPOINT_DATE, APPOINT_TIEM, PATIENT_ID, PATIENT_NAME, LAB_ID, DELIVERY_DATE, DENTIST_ID, SAVE_PATH, ORDER_STATE, REMARK, ORDER_TYPE, ORDER_DATE, ORDER_TIME, SEND_DATETIME, CLOUDORDERID, ORDER_ISCOMPETE, MYCLOUD_PATIENT_ID, MYCLOUD_ORDER_ID, DEVICE_ID, MYCLOUD_CLINIC_ID, MYCLOUD_SEND_LAB_ID, ORDER_SEND_LAB_NAME, ACCESSION_NUMBER, PHYSICIAN_NAME, STUDY_DATE, STUDY_TIME) "
        "VALUES ('O001', '2026-06-30', '10:00:00', 'P001', 'Patient Demo', 'L001', '2026-07-01', 1, 'case/O001', 0, '', '0', '20260630', '100000', '', 1, 0, 'cp001', 'co001', 'SN001', 'cc001', 'cl001', 'Meyer Demo Lab', '', 'Dr. Demo', '', '')",

        // meyer_scan 先清空再插入，避免重复运行测试时软件信息累积多行。
        "DELETE FROM meyer_scan",
        "INSERT INTO meyer_scan (NAME, VERSION, COMPANY) "
        "VALUES ('MeyerScan', '0.1.0', 'Meyer')",

        // 其它 REPLACE 使用固定主键，让测试可重复运行且结果稳定。
        "REPLACE INTO soft_init (ID, SWITCH, DEFAULT_ORDER_TYPE) "
        "VALUES (1, 1, 'Restoration')",

        "REPLACE INTO user_tbl (ID, USER_NAME, USER_PASS, USER_CELLPHONE) "
        "VALUES (1, 'demo_user', 'demo_password_placeholder', '13600000000')",

        "REPLACE INTO user_tbl2 (ID, USER_NAME, USER_PASS, USER_CELLPHONE) "
        "VALUES (1, 'demo_user', 'demo_password_placeholder', '13600000000')",

        "REPLACE INTO device_info_tbl2 (DEVICE_ID, DEVICE_MODEL, DEVICE_STATUS, DURATION_CODE, IS_TEMP) "
        "VALUES ('SN001', 'Demo Scanner', 'Normal', 'demo-duration', '0')"
    };

    const int scriptCount = static_cast<int>(sizeof(scripts) / sizeof(scripts[0]));
    QList<QByteArray> scriptList;
    scriptList.reserve(scriptCount);
    for (int i = 0; i < scriptCount; ++i) {
        // QByteArray 保存 SQL 脚本字节，确保 Adapter 调用底层 ExecuteScript 时指针仍有效。
        scriptList.append(QByteArray(scripts[i]));
    }
    // 静态数组元素个数由 sizeof 计算，避免手工维护脚本数量。
    return databaseAdapter->ExecuteScript(scriptList) == scriptCount;
}

// 冒烟测试检查 Information 页面三张表是否都有数据。
bool HasInformationRows(QWidget* widget) {
    // 递归查找 Information 页内所有 QTableWidget，不依赖具体 objectName，降低测试脆弱性。
    const QList<QTableWidget*> tables = widget->findChildren<QTableWidget*>();
    int nonEmptyTableCount = 0;
    for (QTableWidget* table : tables) {
        if (table && table->rowCount() > 0) {
            // 只统计非空表，避免界面创建了空表但数据链路没有真正打通也误判通过。
            ++nonEmptyTableCount;
        }
    }

    // SettingsUI Information 当前有医生、诊所、技工所三张表。
    return nonEmptyTableCount >= 3;
}

// SettingsUI 回调测试函数。
// 独立测试宿主只验证动作能上报，不执行 MainExe 页面切换。
void OnSettingsAction(void* context, int actionId) {
    // context 是 C ABI 回调常用技巧：调用方把自己的状态指针传入，回调再转回真实类型。
    int* lastAction = static_cast<int*>(context);
    if (lastAction) {
        // 这里只记录最后一次 actionId，验证回调链路可达，不执行正式页面切换。
        *lastAction = actionId;
    }
}

// 设置模块测试入口。
// --smoke 模式创建页面后立即退出，用于自动化验证 DLL 装载和 QWidget 创建。
int main(int argc, char* argv[]) {
    // SettingsUI 是 QWidget 模块，测试宿主必须创建 QApplication 而不是 QCoreApplication。
    QApplication app(argc, argv);

    const QString databaseConfigPath = ResolveDatabaseConfigPath();
    const QString logDir = ResolveTestLogDir();
    // 测试日志目录不存在时主动创建，避免 Logger 初始化失败干扰 UI 冒烟。
    QDir().mkpath(logDir);

    if (!PrepareRuntimeDemoData(databaseConfigPath)) {
        // 数据准备失败时直接退出，避免后续把空表误认为 UI 渲染问题。
        return 5;
    }

    int lastAction = 0;
    // 通过 DLL 导出函数获取设置模块接口，验证链接和导出符号正常。
    ISettingsUI* settings = GetSettingsUI();
    if (!settings) {
        return 2;
    }

    const QByteArray appDirBytes = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath()).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    // SettingsUI 只在 Init 调用期间读取 constData()；QByteArray 保证本行调用期间指针有效。
    if (!settings->Init(appDirBytes.constData(), logDirBytes.constData())) {
        return 3;
    }
    // 注册回调用于验证设置页动作能上报到宿主。
    settings->SetActionCallback(&OnSettingsAction, &lastAction);

    QWidget* widget = settings->CreateWidget();
    if (!widget) {
        // CreateWidget 返回空说明 SettingsUI 内部对象创建失败或依赖初始化异常。
        return 4;
    }
    // 用模块版本作为标题，人工运行测试宿主时能直接确认加载的 DLL 版本。
    widget->setWindowTitle(settings->GetModuleVersion());
    widget->resize(1180, 760);
    widget->show();

    const QStringList args = QCoreApplication::arguments();
    if (args.contains("--smoke")) {
        QTimer::singleShot(300, &app, [&]() {
            // 事件循环运行 300ms 后再检查表格，给 RuntimeDataCenter 刷新和 UI 填表留时间。
            const bool hasRows = HasInformationRows(widget);
            // smoke 模式主动关闭窗口和模块，避免批量测试留下隐藏窗口或未刷新的日志。
            widget->close();
            settings->Shutdown();
            app.exit(hasRows ? 0 : 6);
        });
    }

    return app.exec();
}
