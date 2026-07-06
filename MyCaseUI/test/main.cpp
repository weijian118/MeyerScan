#include "CaseUI.h"

#include "DatabaseQtAdapter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QRect>
#include <QSize>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <cstring>

// =============================================================================
// 文件说明:
//   CaseUI 的界面冒烟测试宿主。
//
// 测试链路:
//   1. 解析仓库根目录、模块目录、测试数据库配置和日志目录。
//   2. 通过 DatabaseQtAdapter 准备患者、订单、医生、诊所、技工所等最小数据。
//   3. 初始化 CaseUI，创建 QWidget，并检查患者表和订单表是否有数据。
//
// 阅读重点:
//   - CaseUI 本身只负责显示，不负责建表/造数；测试宿主造数只是为了验证链路。
//   - 所有路径从 applicationDirPath() 推导，不依赖 QDir::currentPath()。
//   - --smoke 模式不会长时间停留界面，适合批量自动化回归。
// =============================================================================

namespace {
// 将测试窗口居中显示到当前屏幕。
// 测试宿主没有 MainExe 的统一窗口，因此这里做一个最小窗口摆放逻辑。
void ShowOnCurrentScreen(QWidget* widget) {
    // 使用当前屏幕可用区域，避免窗口覆盖任务栏或跑到屏幕外。
    QRect available = QApplication::desktop()->availableGeometry(widget);
    // sizeHint 由 Layout 计算，minimumSize 由模块约束，两者取较大值作为初始尺寸。
    QSize initialSize = widget->sizeHint().expandedTo(widget->minimumSize());
    // 小屏幕下给四周留白，防止窗口尺寸超过可用区域。
    // qMin 限制最大尺寸，qMax 保证即使屏幕很小也不低于模块最小宽高。
    initialSize.setWidth(qMin(initialSize.width(), qMax(available.width() - 80, widget->minimumWidth())));
    initialSize.setHeight(qMin(initialSize.height(), qMax(available.height() - 80, widget->minimumHeight())));

    // 先 resize 再 move，居中坐标才能基于最终窗口尺寸计算。
    widget->resize(initialSize);
    // 多显示器坐标可能为负数，所以不能假设屏幕左上角是 0,0。
    const int x = available.left() + qMax(0, (available.width() - widget->width()) / 2);
    const int y = available.top() + qMax(0, (available.height() - widget->height()) / 2);
    widget->move(x, y);
    widget->show();
}

// 从测试 EXE 所在目录向上寻找仓库根目录。
// 技术实现：根目录下固定存在 MeyerScan_AllModules.sln，
// 所以不依赖 currentPath，也不依赖测试程序是单模块输出还是根聚合输出。
QString ResolveRepoRoot() {
    // applicationDirPath() 是 EXE 实际所在目录，比 currentPath() 更稳定。
    QDir dir(QCoreApplication::applicationDirPath());
    // 逐级向上搜索根方案文件；root 聚合输出和单模块输出都能命中 F:/MeyerScan。
    while (!dir.isRoot()) {
        if (QFileInfo::exists(dir.filePath("MeyerScan_AllModules.sln"))) {
            return dir.absolutePath();
        }
        dir.cdUp();
    }

    // 理论上不会走到这里；保底返回 EXE 目录，避免返回空字符串导致后续路径更难排查。
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

// 解析 MyCaseUI 模块根目录。
// 根聚合输出时 EXE 在 F:/MeyerScan/bin/Release，不能再简单向上两级。
QString ResolveModuleRoot() {
    // 优先通过仓库根目录拼出模块目录，适配总方案输出目录。
    QDir moduleDir(QDir(ResolveRepoRoot()).filePath("MyCaseUI"));
    if (moduleDir.exists()) {
        return moduleDir.absolutePath();
    }

    // 保留旧路径推导作为兜底，兼容临时拷贝出来的单模块测试目录。
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}

// 解析测试宿主使用的数据库配置文件。
// 技术实现：测试宿主在 EXE 输出目录下生成自己的配置文件，
// 并让 sqlitePath 指向独立的 CaseUITest_<pid>.db。
// 这样做的原因是根输出目录会同时运行多个测试程序，如果大家共用
// config/db_config.json 和同一个 SQLite 文件，旧表结构会互相污染。
QString ResolveDatabaseConfigPath() {
    // applicationDirPath() 指向测试 EXE 所在目录；测试配置只写在这个输出目录下。
    QDir appDir(QCoreApplication::applicationDirPath());
    // config/CaseUITest 用来放测试专用 db_config.json，避免覆盖正式配置模板。
    if (!appDir.mkpath("config/CaseUITest")) {
        return QString();
    }
    // Data 用来放测试专用 SQLite 文件，所有测试现场数据都留在输出目录。
    if (!appDir.mkpath("Data")) {
        return QString();
    }

    // PID 让每次测试使用独立数据库文件，即使上一次异常退出留下文件也不会影响本轮。
    const QString databaseFileName = QString("CaseUITest_%1.db").arg(QCoreApplication::applicationPid());
    const QString configPath = appDir.filePath("config/CaseUITest/db_config.json");

    // sqlitePath 是相对配置文件所在目录解析的路径：
    // Release/config/CaseUITest/../../Data/CaseUITest_<pid>.db -> Release/Data/...
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
// 单模块运行时写模块 logs；根聚合运行时写仓库 logs，便于批量测试集中查看。
QString ResolveTestLogDir() {
    // 根聚合输出目录形如 F:/MeyerScan/bin/Release，向上两级后就是仓库根。
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    if (QFileInfo::exists(dir.filePath("MeyerScan_AllModules.sln"))) {
        return dir.filePath("logs");
    }

    // 单模块输出目录形如 F:/MeyerScan/MyCaseUI/bin/Release，日志仍留在模块目录下。
    return QDir(ResolveModuleRoot()).filePath("logs");
}

// 使用数据库模块准备最小演示数据。
// 测试宿主负责造数据，正式 CaseUI 仍只读 RuntimeDataCenter 快照，不在 UI 模块内建表/插表。
bool PrepareRuntimeDemoData(const QString& databaseConfigPath) {
    if (databaseConfigPath.isEmpty()) {
        // 测试配置创建失败时直接失败，避免误连公共数据库或开发机配置。
        return false;
    }

    // 测试宿主也走 DatabaseQtAdapter，避免给后续开发者示范 UI/测试直接包含 Database.h 的旧写法。
    // Adapter 内部负责 QString 路径、SQL 脚本和纯 C++ Database 接口之间的转换。
    DatabaseQtAdapter* databaseAdapter = GetDatabaseQtAdapter();
    if (!databaseAdapter) {
        // 适配器导出失败或依赖 DLL 缺失时，无法继续造数据。
        return false;
    }

    // 测试链路固定使用 SQLite，确保离线机器也能跑通 UI 和 RuntimeDataCenter。
    QString databaseError;
    if (!databaseAdapter->EnsureConnected(QDir::fromNativeSeparators(databaseConfigPath),
                                          "sqlite",
                                          &databaseError)) {
        // 连接失败通常是 sqlite3.dll 缺失、配置文件路径错误或数据库目录无权限。
        return false;
    }

    // 这些表是 RuntimeDataCenter 当前读取的旧表名。
    // 字段只创建 UI 链路需要的最小集合，后续真实迁移/建表不能放在测试宿主中。
    const char* scripts[] = {
        // 诊所、技工所、医生表不是 CaseUI 直接展示内容，但 RuntimeDataCenter 全量刷新会读取。
        "CREATE TABLE IF NOT EXISTS clinic_tbl ("
        "CLINIC_ID TEXT PRIMARY KEY,"
        "CLINIC_NAME TEXT,"
        "CLINIC_CONTACT TEXT,"
        "CLINIC_TEL TEXT,"
        "CLINIC_CITY TEXT,"
        "CLINIC_DETAILADDRESS TEXT)",

        // 技工所表也用于订单摘要中的发送技工所名称/云端 ID 兼容。
        "CREATE TABLE IF NOT EXISTS lab_tbl2 ("
        "LAB_ID TEXT PRIMARY KEY,"
        "LAB_NAME TEXT,"
        "LAB_ADDRESS TEXT,"
        "LAB_TEL TEXT,"
        "LAB_CONTACT TEXT,"
        "LAB_EMAIL TEXT,"
        "LAB_CLOUDID TEXT)",

        // 医生表给订单摘要中的医生字段提供基础关联数据。
        "CREATE TABLE IF NOT EXISTS dentist_tbl ("
        "DENTIST_ID TEXT PRIMARY KEY,"
        "DENTIST_NAME TEXT,"
        "DENTIST_SEX INTEGER,"
        "DENTIST_TEL TEXT,"
        "DENTIST_PRO TEXT,"
        "CLINIC_ID TEXT)",

        // 患者表是 CaseUI 患者 Tab 的主要数据源。
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

        // 订单表是 CaseUI 订单 Tab 的主要数据源，测试只保留列表摘要字段。
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

        // 写入参考数据，保证 RuntimeDataCenter 刷新所有本地 domain 时不会出现缺表噪声。
        // RuntimeDataCenter 刷新时会统一读取所有本地 domain。
        // CaseUI 当前只显示患者和订单，但测试库仍补齐这些轻量表，
        // 避免 MainExe 集成冒烟时出现“预期缺表”的 Warning 干扰真实问题。
        "CREATE TABLE IF NOT EXISTS meyer_scan ("
        "NAME TEXT,"
        "VERSION TEXT,"
        "COMPANY TEXT)",

        "CREATE TABLE IF NOT EXISTS soft_init ("
        "ID INTEGER PRIMARY KEY,"
        "SWITCH INTEGER,"
        "DEFAULT_ORDER_TYPE TEXT)",

        "CREATE TABLE IF NOT EXISTS user_tbl ("
        "ID INTEGER PRIMARY KEY,"
        "USER_NAME TEXT,"
        "USER_PASS TEXT,"
        "USER_CELLPHONE TEXT)",

        "CREATE TABLE IF NOT EXISTS user_tbl2 ("
        "ID INTEGER PRIMARY KEY,"
        "USER_NAME TEXT,"
        "USER_PASS TEXT,"
        "USER_CELLPHONE TEXT)",

        "CREATE TABLE IF NOT EXISTS device_info_tbl2 ("
        "DEVICE_ID TEXT PRIMARY KEY,"
        "DEVICE_MODEL TEXT,"
        "DEVICE_STATUS TEXT,"
        "DURATION_CODE TEXT,"
        "IS_TEMP TEXT)",

        // 插入诊所/技工所/医生基础数据，给订单和设置页复用同一测试库。
        "REPLACE INTO clinic_tbl (CLINIC_ID, CLINIC_NAME, CLINIC_CONTACT, CLINIC_TEL, CLINIC_CITY, CLINIC_DETAILADDRESS) "
        "VALUES ('C001', 'Meyer Demo Clinic', 'Alice', '010-10001000', 'Beijing', 'No. 1 Demo Road')",

        "REPLACE INTO lab_tbl2 (LAB_ID, LAB_NAME, LAB_ADDRESS, LAB_TEL, LAB_CONTACT, LAB_EMAIL, LAB_CLOUDID) "
        "VALUES ('L001', 'Meyer Demo Lab', 'No. 8 Lab Road', '021-20002000', 'Bob', 'lab@example.com', 'cloud-lab-001')",

        "REPLACE INTO dentist_tbl (DENTIST_ID, DENTIST_NAME, DENTIST_SEX, DENTIST_TEL, DENTIST_PRO, CLINIC_ID) "
        "VALUES ('D001', 'Dr. Demo', 1, '13800000000', 'Orthodontics', 'C001')",

        // 患者演示数据必须包含患者名、性别、年龄、更新时间，CaseUI 患者表会显示这些字段。
        "REPLACE INTO patient_tbl2 (PATIENT_ID, PATIENT_NAME, PATIENT_GENDER, PATIENT_SEX, PATIENT_PHONE, PATIENT_AGE, PATIENT_ORDERCOUNTS, PATIENT_CREATETIME, PATIENT_UPDATETIME) "
        "VALUES ('P001', 'Patient Demo', 'O', 2, '13900000000', '30', 1, '2026-06-30 10:00:00', '2026-06-30 10:00:00')",

        // 订单演示数据必须包含订单 ID、患者名、类型、医生、状态、创建时间等摘要字段。
        "REPLACE INTO order_tbl2 (ORDER_ID, APPOINT_DATE, APPOINT_TIEM, PATIENT_ID, PATIENT_NAME, LAB_ID, DELIVERY_DATE, DENTIST_ID, SAVE_PATH, ORDER_STATE, REMARK, ORDER_TYPE, ORDER_DATE, ORDER_TIME, SEND_DATETIME, CLOUDORDERID, ORDER_ISCOMPETE, MYCLOUD_PATIENT_ID, MYCLOUD_ORDER_ID, DEVICE_ID, MYCLOUD_CLINIC_ID, MYCLOUD_SEND_LAB_ID, ORDER_SEND_LAB_NAME, ACCESSION_NUMBER, PHYSICIAN_NAME, STUDY_DATE, STUDY_TIME) "
        "VALUES ('O001', '2026-06-30', '10:00:00', 'P001', 'Patient Demo', 'L001', '2026-07-01', 1, 'case/O001', 0, '', '0', '20260630', '100000', '', 1, 0, 'cp001', 'co001', 'SN001', 'cc001', 'cl001', 'Meyer Demo Lab', '', 'Dr. Demo', '', '')",

        "DELETE FROM meyer_scan",
        "INSERT INTO meyer_scan (NAME, VERSION, COMPANY) "
        "VALUES ('MeyerScan', '0.1.0', 'Meyer')",

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
        // QByteArray 保存脚本文本的 UTF-8 字节，确保 ExecuteScript 调用期间 constData() 有效。
        scriptList.append(QByteArray(scripts[i]));
    }
    // ExecuteScript 返回成功执行条数；只有全部成功才认为测试数据准备完成。
    return databaseAdapter->ExecuteScript(scriptList) == scriptCount;
}

// 冒烟测试检查案例管理表格是否已经显示患者和订单数据。
bool HasRuntimeRows(QWidget* widget) {
    // findChildren 会递归搜索 widget 的整个子控件树，适合黑盒检查 UI 是否填充了表格。
    const QList<QTableWidget*> tables = widget->findChildren<QTableWidget*>();
    int nonEmptyTableCount = 0;
    for (QTableWidget* table : tables) {
        if (table && table->rowCount() > 0) {
            // 只统计有行的表，避免空表也让冒烟测试误通过。
            ++nonEmptyTableCount;
        }
    }

    // CaseUI 当前有患者表和订单表两个 QTableWidget，二者都要有数据才算链路通过。
    return nonEmptyTableCount >= 2;
}
}

// CaseUI 测试宿主入口。
// 测试目标:
//   1. 验证案例管理 DLL 可加载。
//   2. 验证案例管理界面可创建和显示。
//   3. 验证日志和数据库配置路径能按重构约定传入。
int main(int argc, char* argv[]) {
    // 启用 Qt 高 DPI 支持，必须放在 QApplication 构造之前。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    // 通过 C ABI 工厂函数获取 CaseUI 接口，验证 DLL 导出和依赖加载正常。
    ICaseUI* caseUi = GetCaseUI();
    if (!caseUi) {
        // 非 0 退出码让批量脚本知道测试失败。
        return 1;
    }

    // 所有运行路径都从 applicationDirPath() 推导，避免 currentPath 被外部进程污染。
    const QString databaseConfigPath = ResolveDatabaseConfigPath();
    const QString logDir = ResolveTestLogDir();
    // 日志目录不存在时主动创建，避免 Logger 初始化失败掩盖真正的 UI/数据库问题。
    QDir().mkpath(logDir);

    // 测试链路要求 CaseUI 能展示患者/订单；空库时由测试宿主写入最小演示数据。
    if (!PrepareRuntimeDemoData(databaseConfigPath)) {
        // 数据准备失败时退出 2，便于区分 DLL 加载失败和数据库准备失败。
        return 2;
    }

    // DLL 公共接口使用 UTF-8 char*，QByteArray 保证字节在 Init 调用期间有效。
    const QByteArray databaseConfigBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    // Init 不接管 QByteArray 内存，只在调用期间读取 constData()。
    caseUi->Init(databaseConfigBytes.constData(), logDirBytes.constData());

    // 创建案例管理界面并显示。窗口标题使用模块版本，便于人工确认 DLL 版本。
    QWidget* widget = caseUi->CreateWidget();
    // CreateWidget 当前应总是返回有效 QWidget；如果后续改成可失败，测试宿主应补空指针判断。
    // 测试宿主没有 MainExe 标题栏上下文，所以直接展示模块版本。
    widget->setWindowTitle(caseUi->GetModuleVersion());
    ShowOnCurrentScreen(widget);

    // 冒烟模式用于自动化测试，只停留 300ms 验证窗口创建成功。
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        // 让事件循环至少执行一次，再检查表格行数，避免只验证“窗口能创建”。
        QTimer::singleShot(300, &app, [&app, widget]() {
            // lambda 捕获 widget 指针用于检查表格，捕获 app 引用用于退出事件循环。
            app.exit(HasRuntimeRows(widget) ? 0 : 5);
        });
    }

    // 退出前主动关闭窗口并调用 Shutdown，便于暴露资源释放问题。
    int result = app.exec();
    // close 触发 Qt 关闭事件，delete 释放整个控件树。
    widget->close();
    delete widget;
    // 模块 Shutdown 单独调用，验证 CaseUI 不依赖进程直接退出清理资源。
    caseUi->Shutdown();
    return result;
}
