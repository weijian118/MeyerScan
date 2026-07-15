#include "CaseUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPixmap>
#include <QSize>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

// =============================================================================
// 文件说明:
//   CaseUI 的独立界面测试宿主。
//
// 测试边界:
//   - 测试宿主直接构造版本化 JSON 快照，不连接数据库，也不初始化 RuntimeDataCenter。
//   - 这样可以单独验证“宿主注入数据 -> CaseUI 展示”的 UI 合同。
//   - --smoke 用于自动回归；--capture-screenshot <path> 用于人工比对布局。
// =============================================================================

namespace {

// 保存最近一次动作 ID，用来验证按钮只上报动作而不直接控制宿主窗口。
int g_lastCaseAction = 0;

// 接收 CaseUI 的 C ABI 动作回调。
void OnCaseAction(void* /*context*/, int actionId) {
    // 测试只记录动作，不执行 MainExe 的页面切换逻辑。
    g_lastCaseAction = actionId;
}

// 构造 CaseUI 所需的最小版本化数据快照。
QByteArray BuildCaseDataContext() {
    // 患者行使用 RuntimeDataCenter 当前兼容的旧库字段名，验证 UI 字段映射仍然有效。
    QJsonObject patient;
    patient.insert("PATIENT_ID", "P001");
    patient.insert("PATIENT_NAME", "Patient Demo");
    patient.insert("PATIENT_GENDER", "Female");
    patient.insert("PATIENT_AGE", "30");
    patient.insert("PATIENT_ORDERCOUNTS", 1);
    patient.insert("PATIENT_UPDATETIME", "2026-07-15 10:00:00");

    // 订单行包含浏览卡片和订单表格使用的常用摘要字段。
    QJsonObject order;
    order.insert("ORDER_ID", "O001");
    order.insert("PATIENT_ID", "P001");
    order.insert("PATIENT_NAME", "Patient Demo");
    order.insert("ORDER_TYPE", "Restoration");
    order.insert("ORDER_STATE", 0);
    order.insert("ORDER_DATE", "20260715");
    order.insert("ORDER_TIME", "100000");
    order.insert("PHYSICIAN_NAME", "Dr. Demo");

    // 每个 domain 保留独立对象，后续可增加 revision/source 等元数据而不改变 items 合同。
    QJsonObject patientDomain;
    patientDomain.insert("items", QJsonArray() << patient);
    QJsonObject orderDomain;
    orderDomain.insert("items", QJsonArray() << order);

    // domains 使用稳定业务名称，CaseUI 不知道底层真实表名。
    QJsonObject domains;
    domains.insert("local.patients", patientDomain);
    domains.insert("local.orders", orderDomain);

    // schemaVersion 由宿主维护，便于未来升级快照结构时做兼容判断。
    QJsonObject root;
    root.insert("schemaVersion", 1);
    root.insert("domains", domains);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// 检查患者表和订单卡片是否确实展示了注入数据。
bool HasInjectedRows(QWidget* widget) {
    if (!widget) {
        // 空页面不能继续查找子控件。
        return false;
    }

    // 患者页使用 QTableWidget，至少一张表必须包含数据行。
    bool hasPatientRows = false;
    const QList<QTableWidget*> tables = widget->findChildren<QTableWidget*>();
    for (QTableWidget* table : tables) {
        if (table && table->rowCount() > 0) {
            hasPatientRows = true;
            break;
        }
    }

    // 订单页使用卡片列表，objectName 比按子控件顺序定位更稳定。
    QListWidget* orderList = widget->findChild<QListWidget*>("CaseOrderCardList");
    const bool hasOrderRows = orderList && orderList->count() > 0;
    return hasPatientRows && hasOrderRows;
}

// 验证顶部按钮通过稳定动作 ID 回调宿主。
bool VerifyTopActions(QWidget* widget) {
    if (!widget) {
        return false;
    }

    // 使用动态属性定位动作，避免按钮视觉排列调整导致测试失效。
    QToolButton* cloudButton = nullptr;
    QToolButton* screenshotButton = nullptr;
    QToolButton* closeButton = nullptr;
    const QList<QToolButton*> buttons = widget->findChildren<QToolButton*>("CaseTopToolButton");
    for (QToolButton* button : buttons) {
        if (!button) {
            continue;
        }
        const int actionId = button->property("caseActionId").toInt();
        if (actionId == CaseActionCloud) {
            cloudButton = button;
        } else if (actionId == CaseActionScreenshot) {
            screenshotButton = button;
        } else if (actionId == CaseActionClose) {
            closeButton = button;
        }
    }

    // 逐个点击并立即检查回调值，防止只验证到最后一个按钮。
    if (!cloudButton || !screenshotButton || !closeButton) {
        return false;
    }
    cloudButton->click();
    if (g_lastCaseAction != CaseActionCloud) {
        return false;
    }
    screenshotButton->click();
    if (g_lastCaseAction != CaseActionScreenshot) {
        return false;
    }
    closeButton->click();
    return g_lastCaseAction == CaseActionClose;
}

} // namespace

// CaseUI 测试程序入口。
int main(int argc, char* argv[]) {
    // CaseUI 创建 QWidget，因此测试宿主必须使用 QApplication。
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const bool smokeMode = arguments.contains("--smoke");

    // 解析可选截图参数；尺寸只影响测试窗口，不改变模块布局规则。
    const int captureIndex = arguments.indexOf("--capture-screenshot");
    const bool captureMode = captureIndex >= 0 && captureIndex + 1 < arguments.size();
    const QString capturePath = captureMode ? arguments.at(captureIndex + 1) : QString();
    QSize captureSize(1920, 1080);
    const int sizeIndex = arguments.indexOf("--capture-size");
    if (sizeIndex >= 0 && sizeIndex + 1 < arguments.size()) {
        const QStringList parts = arguments.at(sizeIndex + 1).toLower().split('x');
        bool widthOk = false;
        bool heightOk = false;
        const int width = parts.size() == 2 ? parts.at(0).toInt(&widthOk) : 0;
        const int height = parts.size() == 2 ? parts.at(1).toInt(&heightOk) : 0;
        if (widthOk && heightOk && width >= 960 && height >= 600) {
            captureSize = QSize(width, height);
        }
    }

    // 通过导出工厂获取接口，验证 DLL 的链接和导出符号。
    ICaseUI* caseUi = GetCaseUI();
    if (!caseUi) {
        return 1;
    }

    // 路径全部基于 EXE 实际目录，第三方启动改变工作目录也不会影响测试。
    const QString appDir = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath());
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();
    if (!caseUi->Init(appDirBytes.constData(), logDirBytes.constData())) {
        caseUi->Shutdown();
        return 2;
    }

    // 先注册动作回调并注入数据，再创建页面，保证首次绘制就能显示真实快照。
    caseUi->SetActionCallback(&OnCaseAction, nullptr);
    const QByteArray dataContext = BuildCaseDataContext();
    if (!caseUi->SetDataContextJson(dataContext.constData())) {
        caseUi->Shutdown();
        return 3;
    }

    QWidget* widget = caseUi->CreateWidget();
    if (!widget) {
        caseUi->Shutdown();
        return 4;
    }
    widget->setWindowTitle(caseUi->GetModuleVersion());
    widget->resize(captureMode ? captureSize : QSize(1280, 760));
    widget->show();

    // aboutToQuit 统一释放页面和模块引用，避免 smoke/截图/人工三条退出路径重复清理。
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [caseUi, widget]() {
        delete widget;
        caseUi->Shutdown();
    });

    if (smokeMode || captureMode) {
        QTimer::singleShot(250, &app, [&app, widget, smokeMode, captureMode, capturePath]() {
            // 先验证数据和动作合同，再决定截图或退出码。
            bool success = HasInjectedRows(widget) && VerifyTopActions(widget);
            if (success && captureMode) {
                success = widget->grab().save(capturePath);
            }
            app.exit(success ? 0 : (smokeMode ? 5 : 6));
        });
    }

    return app.exec();
}
