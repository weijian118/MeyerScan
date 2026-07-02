#include "CaseUI.h"

#include "Database.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QRect>
#include <QSize>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <cstring>

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

// 从测试 EXE 所在目录反推出 MyCaseUI 模块根目录。
// 不能使用 currentPath，因为第三方启动或 VS 调试时工作目录可能不同。
QString ResolveModuleRoot() {
    // applicationDirPath 指向 MyCaseUI/bin/Release。
    QDir dir(QCoreApplication::applicationDirPath());
    // Release -> bin。
    dir.cdUp();
    // bin -> MyCaseUI。
    dir.cdUp();
    return dir.absolutePath();
}

// 使用数据库模块准备最小演示数据。
// 测试宿主负责造数据，正式 CaseUI 仍只读 RuntimeDataCenter 快照，不在 UI 模块内建表/插表。
bool PrepareRuntimeDemoData(const QString& databaseConfigPath) {
    // 测试宿主直接调用 Database 是为了准备演示数据；
    // 正式 CaseUI 不能这样做，正式界面应通过 RuntimeDataCenter/服务层读取数据。
    IDatabase* database = GetDatabase();
    if (!database) {
        // 数据库模块导出失败或依赖 DLL 缺失时，无法继续造数据。
        return false;
    }

    // Database 接口使用 UTF-8 路径；配置文件内的 SQLite 相对路径由 Database 按配置目录解析。
    const QByteArray configBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    if (database->Init(configBytes.constData()).IsError()) {
        // Init 失败通常是配置文件不存在、JSON 错误或路径编码问题。
        return false;
    }
    if (database->Connect().IsError()) {
        // Connect 失败通常是 SQLite 驱动缺失、文件路径无权限或 MySQL 服务不可用。
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
    // ExecuteScript 返回成功执行条数；只有全部成功才认为测试数据准备完成。
    return database->ExecuteScript(scripts, scriptCount) == scriptCount;
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
    const QString moduleRoot = ResolveModuleRoot();
    QDir repoDir(moduleRoot);
    // MyCaseUI 的上级就是 F:/MeyerScan，用于定位其它模块配置。
    repoDir.cdUp();
    // CaseUI 测试直接复用 MyDatabase 的配置，避免每个 UI 测试维护一份数据库配置。
    const QString databaseConfigPath = repoDir.filePath("MyDatabase/config/db_config.json");
    const QString logDir = QDir(moduleRoot).filePath("logs");

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
