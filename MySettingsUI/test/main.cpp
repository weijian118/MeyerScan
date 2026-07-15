#include "SettingsUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

// =============================================================================
// 文件说明:
//   SettingsUI 的独立界面测试宿主。
//
// 测试边界:
//   - 测试宿主直接注入医生、诊所、技工所快照，不连接数据库。
//   - 测试验证 SettingsUI 只消费宿主数据上下文，并能独立创建完整设置页面。
// =============================================================================

namespace {

// 构造单个 domain 对象，统一保持 {items:[...]} 结构。
QJsonObject MakeDomain(const QJsonObject& item) {
    QJsonObject domain;
    domain.insert("items", QJsonArray() << item);
    return domain;
}

// 构造设置页 Information 分类所需的最小数据上下文。
QByteArray BuildSettingsDataContext() {
    // 医生字段同时覆盖姓名、性别、电话和科室四个可见列。
    QJsonObject doctor;
    doctor.insert("DENTIST_ID", "D001");
    doctor.insert("DENTIST_NAME", "Dr. Demo");
    doctor.insert("DENTIST_SEX", 1);
    doctor.insert("DENTIST_TEL", "13800000000");
    doctor.insert("DENTIST_PRO", "Orthodontics");

    // 诊所字段覆盖名称、地址、电话和城市。
    QJsonObject clinic;
    clinic.insert("CLINIC_ID", "C001");
    clinic.insert("CLINIC_NAME", "Meyer Demo Clinic");
    clinic.insert("CLINIC_DETAILADDRESS", "No. 1 Demo Road");
    clinic.insert("CLINIC_TEL", "010-10001000");
    clinic.insert("CLINIC_CITY", "Beijing");

    // 技工所字段覆盖名称、联系人、电话和地址。
    QJsonObject lab;
    lab.insert("LAB_ID", "L001");
    lab.insert("LAB_NAME", "Meyer Demo Lab");
    lab.insert("LAB_CONTACT", "Bob");
    lab.insert("LAB_TEL", "021-20002000");
    lab.insert("LAB_ADDRESS", "No. 8 Lab Road");

    // domain 名称是 UI 与宿主之间的稳定业务合同，不暴露数据库表名。
    QJsonObject domains;
    domains.insert("local.doctors", MakeDomain(doctor));
    domains.insert("local.clinics", MakeDomain(clinic));
    domains.insert("local.labs", MakeDomain(lab));

    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("domains", domains);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// 检查 Information 页的三张表是否都获得至少一条注入数据。
bool HasInformationRows(QWidget* widget) {
    if (!widget) {
        return false;
    }

    int nonEmptyTableCount = 0;
    const QList<QTableWidget*> tables = widget->findChildren<QTableWidget*>();
    for (QTableWidget* table : tables) {
        if (table && table->rowCount() > 0) {
            ++nonEmptyTableCount;
        }
    }
    return nonEmptyTableCount >= 3;
}

// 接收设置模块动作回调，验证跨 DLL 回调合同。
void OnSettingsAction(void* context, int actionId) {
    int* lastAction = static_cast<int*>(context);
    if (lastAction) {
        *lastAction = actionId;
    }
}

} // namespace

// SettingsUI 测试程序入口。
int main(int argc, char* argv[]) {
    // SettingsUI 创建 QWidget，因此必须使用 QApplication。
    QApplication app(argc, argv);
    const bool smokeMode = app.arguments().contains("--smoke");

    ISettingsUI* settings = GetSettingsUI();
    if (!settings) {
        return 1;
    }

    // 所有路径都从测试 EXE 所在目录推导，不依赖 currentPath。
    const QString appDir = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath());
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();
    if (!settings->Init(appDirBytes.constData(), logDirBytes.constData())) {
        settings->Shutdown();
        return 2;
    }

    int lastAction = 0;
    settings->SetActionCallback(&OnSettingsAction, &lastAction);
    // 模拟从首页打开设置；该来源允许校准入口显示。
    settings->SetOpenContext(SettingsOpenSourceHome, true);

    // 首次创建页面前注入快照，避免先显示空表再刷新造成视觉闪动。
    const QByteArray dataContext = BuildSettingsDataContext();
    if (!settings->SetDataContextJson(dataContext.constData())) {
        settings->Shutdown();
        return 3;
    }

    QWidget* widget = settings->CreateWidget();
    if (!widget) {
        settings->Shutdown();
        return 4;
    }
    widget->setWindowTitle(settings->GetModuleVersion());
    widget->resize(1180, 760);
    widget->show();

    // 退出时先通知模块清理页面弱引用，再由测试宿主删除 QWidget。
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [settings, widget]() {
        settings->DestroyWidget();
        delete widget;
        settings->Shutdown();
    });

    if (smokeMode) {
        QTimer::singleShot(250, &app, [&app, widget]() {
            // 页面在事件循环中完成布局后再检查表格，降低时序相关误报。
            app.exit(HasInformationRows(widget) ? 0 : 5);
        });
    }

    return app.exec();
}
