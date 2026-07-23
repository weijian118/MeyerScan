#include "SettingsUI.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <cstring>
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

// 模拟 MainExe 设备会话宿主返回“USB3 + mOS MyScan 5 国内标准版”。
// 该回调只供 UI 测试，不访问 DeviceCmd 或真实 USB。
int OnCalibrationPreflight(void* context,
                           int actionId,
                           SettingsCalibrationDeviceContext* deviceContext) {
    if (!deviceContext || actionId != SettingsActionOpenColorCalibration) {
        return SettingsCalibrationPreflightInternalError;
    }
    const int requestedStatus = context
        ? *static_cast<int*>(context)
        : SettingsCalibrationPreflightReady;
    if (requestedStatus != SettingsCalibrationPreflightReady) {
        deviceContext->status = requestedStatus;
        std::strcpy(deviceContext->detailUtf8, "Simulated calibration preflight blocked");
        return deviceContext->status;
    }
    deviceContext->status = SettingsCalibrationPreflightReady;
    deviceContext->deviceModel = 5;
    deviceContext->modelSource = 3;
    deviceContext->connectionState = 1;
    deviceContext->isUsb2 = 0;
    std::strcpy(deviceContext->modelNameUtf8, "MyScan 5");
    std::strcpy(deviceContext->deviceIdUtf8, "6200005301203");
    std::strcpy(deviceContext->modelCodeUtf8, "62000053");
    deviceContext->productEvidence = 0x17U;
    deviceContext->productFamily = 5;
    deviceContext->productModel = 5001;
    deviceContext->productIdentificationStatus = 2;
    deviceContext->protocolProfile = 5;
    std::strcpy(deviceContext->productSeriesNameUtf8, "mOS MyScan 5");
    std::strcpy(deviceContext->productNameUtf8, "mOS MyScan 5/mOS MyScan 5");
    // 精确检测场景中 reported 与 effective 相同，且两项来源都标记为设备上报。
    deviceContext->detection.structSize = sizeof(deviceContext->detection);
    deviceContext->detection.schemaVersion =
        MEYER_SETTINGS_DEVICE_DETECTION_SCHEMA_VERSION;
    deviceContext->detection.detectionStatus = 1;
    deviceContext->detection.deviceNumberStatus = 1;
    deviceContext->detection.modelCodeStatus = SettingsDeviceModelCodeReadValid;
    deviceContext->detection.seriesProbeStatus = 1;
    deviceContext->detection.isProductionMode = 0;
    deviceContext->detection.usedCompatibilityDefaults = 0;
    deviceContext->detection.deviceNumberSource = 1;
    deviceContext->detection.modelCodeSource = 1;
    std::strcpy(deviceContext->detection.reportedDeviceNumberUtf8, "6200005301203");
    std::strcpy(deviceContext->detection.effectiveDeviceNumberUtf8, "6200005301203");
    std::strcpy(deviceContext->detection.reportedModelCodeUtf8, "62000053");
    std::strcpy(deviceContext->detection.effectiveModelCodeUtf8, "62000053");
    std::strcpy(deviceContext->detection.detailUtf8, "Exact simulated device identity");
    // MyScan 5 预检必须返回主控板版本，并明确说明投图板不适用。
    deviceContext->firmwareVersions.structSize = sizeof(deviceContext->firmwareVersions);
    deviceContext->firmwareVersions.schemaVersion =
        MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;
    deviceContext->firmwareVersions.mainBoardStatus = SettingsFirmwareVersionValid;
    deviceContext->firmwareVersions.projectionBoardStatus =
        SettingsFirmwareVersionNotRequired;
    std::strcpy(deviceContext->firmwareVersions.mainBoardVersionUtf8, "1.3.1001");
    // MyScan 5 测试设备的大小扫描头都已校准，因此 Ready 分支不会额外弹提示。
    deviceContext->scanHeadColorCalibration.structSize =
        sizeof(deviceContext->scanHeadColorCalibration);
    deviceContext->scanHeadColorCalibration.schemaVersion =
        MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;
    deviceContext->scanHeadColorCalibration.policy =
        SettingsScanHeadColorCalibrationPolicyLargeAndSmall;
    deviceContext->scanHeadColorCalibration.firmwareCompatibility =
        SettingsColorCalibrationFirmwareSupported;
    deviceContext->scanHeadColorCalibration.largeHeadStatus =
        SettingsScanHeadColorCalibrationCalibrated;
    deviceContext->scanHeadColorCalibration.smallHeadStatus =
        SettingsScanHeadColorCalibrationCalibrated;
    deviceContext->scanHeadColorCalibration.largeHeadCommandResult = 0;
    deviceContext->scanHeadColorCalibration.smallHeadCommandResult = 0;
    std::strcpy(deviceContext->detailUtf8, "Simulated calibration preflight passed");
    return deviceContext->status;
}

// 返回各预检状态应显示的英文源文本，测试不依赖系统当前语言或按钮位置。
QString ExpectedPreflightMessage(int status) {
    switch (status) {
    case SettingsCalibrationPreflightWorkspaceOwnsDevice:
        return "Please exit the scan module before calibration.";
    case SettingsCalibrationPreflightDeviceNotConnected:
        return "Device is not connected.";
    case SettingsCalibrationPreflightUsb2Connected:
        return "Please reconnect the device to a USB 3.0 port.";
    case SettingsCalibrationPreflightModelUnknown:
        return "Unable to read the device model.";
    case SettingsCalibrationPreflightMachineCodeReadFailed:
        return "Unable to read the device number.";
    case SettingsCalibrationPreflightProductIdentityConflict:
        return "The device number does not match the device model.";
    case SettingsCalibrationPreflightDeviceResponseAbnormal:
        return "The device response is abnormal.";
    case SettingsCalibrationPreflightFirmwareVersionReadFailed:
        return "Unable to read the device firmware version.";
    case SettingsCalibrationPreflightColorCalibrationFirmwareUnsupported:
        return "Please update the device firmware before color calibration.";
    case SettingsCalibrationPreflightScanHeadColorCalibrationReadFailed:
        return "Unable to read the scan head color calibration status.";
    case SettingsCalibrationPreflightDeviceNumberInvalid:
        return "The device number is invalid.";
    case SettingsCalibrationPreflightDeviceModelCodeInvalid:
        return "The device model code is invalid.";
    case SettingsCalibrationPreflightProductFamilyUnsupported:
        return "Current software does not support this device series.";
    default:
        return "Unable to prepare the device for calibration.";
    }
}

} // namespace

// SettingsUI 测试程序入口。
int main(int argc, char* argv[]) {
    // SettingsUI 创建 QWidget，因此必须使用 QApplication。
    QApplication app(argc, argv);
    const bool smokeMode = app.arguments().contains("--smoke");
    // --test-preflight-status <n> 验证指定失败分支的提示且不会创建校准遮罩。
    int requestedPreflightStatus = SettingsCalibrationPreflightReady;
    const int preflightArgumentIndex = app.arguments().indexOf("--test-preflight-status");
    if (preflightArgumentIndex >= 0 && preflightArgumentIndex + 1 < app.arguments().size()) {
        requestedPreflightStatus = app.arguments().at(preflightArgumentIndex + 1).toInt();
    }
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
    settings->SetCalibrationPreflightCallback(
        &OnCalibrationPreflight, &requestedPreflightStatus);
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
    } else if (preflightArgumentIndex >= 0) {
        QTimer::singleShot(250, &app, [&app,
                                      widget,
                                      requestedPreflightStatus]() {
            QPushButton* openButton = widget->findChild<QPushButton*>(
                "SettingsColorCalibrationButton");
            if (!openButton) {
                app.exit(10);
                return;
            }

            const std::shared_ptr<int> result = std::make_shared<int>(11);
            // UIComponents 公共弹窗使用嵌套事件循环，提前安排验证和关闭任务。
            QTimer::singleShot(150, &app, [result, requestedPreflightStatus]() {
                QDialog* messageDialog = nullptr;
                const QWidgetList topLevels = QApplication::topLevelWidgets();
                for (QWidget* candidate : topLevels) {
                    if (candidate && candidate->objectName() == "MeyerMessageDialog") {
                        messageDialog = qobject_cast<QDialog*>(candidate);
                        break;
                    }
                }
                if (!messageDialog) {
                    *result = 12;
                    return;
                }

                QLabel* messageLabel = messageDialog->findChild<QLabel*>("MeyerDialogMessage");
                const bool textMatches =
                    messageLabel &&
                    messageLabel->text() == ExpectedPreflightMessage(requestedPreflightStatus);
                const bool noCalibrationOverlay = [&]() {
                    const QWidgetList widgets = QApplication::topLevelWidgets();
                    for (QWidget* candidate : widgets) {
                        if (candidate &&
                            candidate->objectName() == "SettingsCalibrationModalOverlay") {
                            return false;
                        }
                    }
                    return true;
                }();
                *result = textMatches && noCalibrationOverlay ? 0 : 13;
                QPushButton* confirmButton = messageDialog->findChild<QPushButton*>(
                    "MeyerDialogConfirmButton");
                if (confirmButton) {
                    confirmButton->click();
                } else {
                    messageDialog->accept();
                }
            });

            openButton->click();
            app.exit(*result);
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
            // Ready 分支会先显示合并后的设备信息弹窗。先验证编号、型号代码和
            // 产品名称，再关闭它，随后才能进入校准遮罩。
            QTimer::singleShot(120, &app, [captureResult]() {
                QDialog* messageDialog = nullptr;
                const QWidgetList topLevels = QApplication::topLevelWidgets();
                for (QWidget* candidate : topLevels) {
                    if (candidate && candidate->objectName() == "MeyerMessageDialog") {
                        messageDialog = qobject_cast<QDialog*>(candidate);
                        break;
                    }
                }
                if (!messageDialog) {
                    *captureResult = 14;
                    return;
                }

                QLabel* messageLabel = messageDialog->findChild<QLabel*>("MeyerDialogMessage");
                if (!messageLabel ||
                    !messageLabel->text().contains("6200005301203") ||
                    !messageLabel->text().contains("62000053") ||
                    !messageLabel->text().contains("mOS MyScan 5/mOS MyScan 5")) {
                    *captureResult = 15;
                    messageDialog->reject();
                    return;
                }

                QPushButton* confirmButton = messageDialog->findChild<QPushButton*>(
                    "MeyerDialogConfirmButton");
                if (confirmButton) {
                    confirmButton->click();
                } else {
                    messageDialog->accept();
                }
            });

            // 设备编号弹窗关闭后 ShowColorCalibrationDialog 才会进入第二层事件循环。
            QTimer::singleShot(450, &app, [captureResult,
                                           colorCalibrationCapturePath,
                                           widget]() {
                if (*captureResult != 7) {
                    return;
                }
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
            // 弹窗关闭后必须收到独立释放动作，MainExe 据此关闭唯一设备会话。
            if (*captureResult == 0 && lastAction != SettingsActionColorCalibrationClosed) {
                *captureResult = 9;
            }
            app.exit(*captureResult);
        });
    }

    return app.exec();
}
