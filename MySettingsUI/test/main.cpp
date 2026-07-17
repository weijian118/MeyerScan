#include "SettingsUI.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <memory>

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
    // --capture-color-calibration <png> 打开真实设置入口并抓取遮罩弹窗，用于视觉回归。
    QString colorCalibrationCapturePath;
    const int captureArgumentIndex = app.arguments().indexOf("--capture-color-calibration");
    if (captureArgumentIndex >= 0 && captureArgumentIndex + 1 < app.arguments().size()) {
        colorCalibrationCapturePath = app.arguments().at(captureArgumentIndex + 1);
    }

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
    } else if (!colorCalibrationCapturePath.isEmpty()) {
        QTimer::singleShot(250, &app, [&app,
                                      widget,
                                      &lastAction,
                                      colorCalibrationCapturePath]() {
            // objectName 是设置模块为自动化提供的稳定锚点，不受中英文翻译变化影响。
            QPushButton* openButton = widget->findChild<QPushButton*>(
                "SettingsColorCalibrationButton");
            if (!openButton) {
                app.exit(6);
                return;
            }

            // shared_ptr 让结果在嵌套事件循环和延迟 lambda 之间拥有明确生命周期。
            const std::shared_ptr<int> captureResult = std::make_shared<int>(7);
            // click() 会进入颜色校准 QDialog 的嵌套事件循环，因此提前安排抓图和关闭任务。
            QTimer::singleShot(300, &app, [captureResult,
                                           colorCalibrationCapturePath,
                                           widget]() {
                QWidget* overlay = nullptr;
                const QWidgetList topLevels = QApplication::topLevelWidgets();
                for (QWidget* candidate : topLevels) {
                    if (candidate && candidate->objectName() == "SettingsCalibrationModalOverlay") {
                        overlay = candidate;
                        break;
                    }
                }

                if (overlay) {
                    // Windows 不允许后台测试程序强抢前台焦点，因此屏幕抓图可能截到其它软件。
                    // 这里用 Qt 离屏合成：设置页截图 + 同透明度遮罩 + 实际校准面板截图。
                    QPixmap screenshot = widget->grab();
                    QPainter painter(&screenshot);
                    painter.fillRect(screenshot.rect(), QColor(0, 0, 0, 145));
                    QWidget* calibrationPanel = overlay->findChild<QWidget*>(
                        "MeyerScanCalibrationColorUIRoot");
                    if (calibrationPanel) {
                        const QPoint panelPosition = calibrationPanel->mapTo(overlay, QPoint(0, 0));
                        painter.drawPixmap(panelPosition, calibrationPanel->grab());
                    }
                    painter.end();
                    *captureResult = screenshot.save(colorCalibrationCapturePath, "PNG") ? 0 : 8;
                    // 关闭遮罩会结束 ShowColorCalibrationDialog 内的 exec() 嵌套事件循环。
                    overlay->close();
                }
            });

            openButton->click();
            // 设置动作回调应在弹窗关闭后收到 ColorCalibration 动作，顺便验证模块交互链路。
            if (*captureResult == 0 && lastAction != SettingsActionOpenColorCalibration) {
                *captureResult = 9;
            }
            app.exit(*captureResult);
        });
    }

    return app.exec();
}
