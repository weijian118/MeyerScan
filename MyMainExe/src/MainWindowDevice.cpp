
#include "MainWindow.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include "device/DeviceSessionHost.h"

// 构造工作台默认上下文。
// 这里用 JSON 作为跨模块轻量载体：模块只读取需要的字段，新增字段不会破坏旧模块。
QString MainWindow::BuildDefaultWorkspaceContextJson(const QString& mode) const {
    const bool practiceMode = mode == "practice";

    // source 说明上下文来自软件内部入口；手工建单和练习仍使用同一标准 JSON 结构。
    QJsonObject source;
    source.insert("launchType", "internal");
    source.insert("thirdPartyType", practiceMode ? "practice" : "manual");
    source.insert("sourceSystem", "MeyerScan");

    QJsonObject patient;
    // 练习模式允许明确的非真实占位信息；手工创建必须从空白表单开始。
    patient.insert("patientId", practiceMode ? "PRACTICE_PATIENT" : "");
    patient.insert("name", practiceMode ? tr("Practice Patient") : "");
    patient.insert("gender", "unknown");

    QJsonObject order;
    order.insert("orderId", practiceMode ? "PRACTICE_ORDER" : "");
    order.insert("source", mode);
    order.insert("doctor", practiceMode ? tr("Practice Doctor") : "");
    order.insert("lab", practiceMode ? tr("Practice Lab") : "");

    QJsonObject context;
    context.insert("schemaVersion", 1);
    context.insert("mode", mode);
    context.insert("source", source);
    context.insert("patient", patient);
    context.insert("order", order);
    context.insert("scanProcess", BuildDefaultScanProcessObject());
    context.insert("createdAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    return QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
}

// 准备当前工作台使用的唯一设备会话。
// 创建/练习是否允许生产兼容身份分别由 ConfigCenter 控制；缺失配置时使用
// “练习允许、创建禁止”的安全默认值。颜色/三维校准不读取这两个工作台开关。
bool MainWindow::PrepareWorkspaceDeviceSession() {
    if (m_skipWorkspaceDevicePreflightForSmoke) {
        // 自动化 smoke 只检查页面切换，不伪造 reported/effective 设备值。
        // 明确写入 automationBypass，防止测试上下文被误解为真实设备检测结果。
        if (!m_workspaceDeviceSessionReady) {
            QJsonObject context;
            const QJsonDocument current = QJsonDocument::fromJson(
                m_workspaceContextJson.toUtf8());
            if (current.isObject()) {
                context = current.object();
            }
            QJsonObject identity;
            identity.insert("schemaVersion", 1);
            identity.insert("automationBypass", true);
            identity.insert("admissionMode",
                            m_currentWorkspaceMode == WorkspaceModePractice
                                ? "practice"
                                : "orderCreate");
            context.insert("deviceIdentity", identity);
            m_workspaceContextJson = QString::fromUtf8(
                QJsonDocument(context).toJson(QJsonDocument::Compact));
            RefreshWorkspaceContextConsumers();
            m_workspaceDeviceSessionReady = true;
            WriteUserAction("WorkspaceDevicePreflightBypassed",
                            "smoke-main bypassed physical USB device checks");
        }
        return true;
    }

    if (!m_deviceSessionHost) {
        WriteUserAction("WorkspaceDevicePreflightFailed",
                        "MainExe device session host is unavailable");
        ShowWorkspaceDevicePreflightMessage(
            MeyerDeviceCalibrationPreflight_InternalError);
        return false;
    }

    // 已通过准入且连接仍打开时直接复用，避免步骤来回切换时重复发送设备命令。
    if (m_workspaceDeviceSessionReady && m_deviceSessionHost->IsSessionOpen()) {
        return true;
    }
    m_workspaceDeviceSessionReady = false;

    MeyerDeviceCalibrationPreflight preflight = {};
    const bool practiceMode = m_currentWorkspaceMode == WorkspaceModePractice;
    const char* productionModeConfigKey = practiceMode
        ? "device.practiceAllowProductionMode"
        : "device.orderCreateAllowProductionMode";
    const bool productionModeDefault = practiceMode;
    const bool allowProductionIdentity = m_config
        ? m_config->GetBool(productionModeConfigKey, productionModeDefault)
        : productionModeDefault;
    WriteUserAction(
        "WorkspaceProductionModePolicy",
        QString("mode=%1 configKey=%2 allow=%3 default=%4")
            .arg(practiceMode ? "practice" : "orderCreate")
            .arg(QString::fromLatin1(productionModeConfigKey))
            .arg(allowProductionIdentity)
            .arg(productionModeDefault));
    const std::int32_t result = m_deviceSessionHost->PrepareWorkspaceSession(
        allowProductionIdentity, &preflight);

    // 即使业务状态被拦截，也先保存完整身份诊断，便于日志和后续现场问题页读取。
    if (result == MeyerDeviceCmdResult_Ok) {
        SetWorkspaceDeviceIdentity(preflight);
    }

    WriteUserAction(
        "WorkspaceDevicePreflight",
        QString("mode=%1 result=%2 status=%3 production=%4 compatibility=%5 "
                "mainBoardVersion=%6 projectionBoardVersion=%7 reportedNumber=%8 "
                "effectiveNumber=%9 reportedModelCode=%10 effectiveModelCode=%11")
            .arg(practiceMode ? "practice" : "orderCreate")
            .arg(result)
            .arg(preflight.status)
            .arg(preflight.detectionRecord.isProductionMode)
            .arg(preflight.detectionRecord.usedCompatibilityDefaults)
            .arg(QString::fromUtf8(
                preflight.firmwareVersions.mainBoardVersionUtf8))
            .arg(QString::fromUtf8(
                preflight.firmwareVersions.projectionBoardVersionUtf8))
            .arg(QString::fromUtf8(
                preflight.detectionRecord.reportedDeviceNumberUtf8))
            .arg(QString::fromUtf8(
                preflight.detectionRecord.effectiveDeviceNumberUtf8))
            .arg(QString::fromUtf8(
                preflight.detectionRecord.reportedModelCodeUtf8))
            .arg(QString::fromUtf8(
                preflight.detectionRecord.effectiveModelCodeUtf8)));

    if (result != MeyerDeviceCmdResult_Ok ||
        preflight.status != MeyerDeviceCalibrationPreflight_Ready) {
        const int displayStatus = result == MeyerDeviceCmdResult_Ok
            ? preflight.status
            : MeyerDeviceCalibrationPreflight_InternalError;
        ShowWorkspaceDevicePreflightMessage(displayStatus);
        m_deviceSessionHost->CloseSession();
        return false;
    }

    m_workspaceDeviceSessionReady = true;
    return true;
}

// 把 DeviceCmd 的固定 POD 转成跨 UI 模块使用的版本化 JSON。
// JSON 同时保留真实值和 effective 值；下游读取 effective 时必须结合 source/标志判断来源。
void MainWindow::SetWorkspaceDeviceIdentity(
    const MeyerDeviceCalibrationPreflight& preflight) {
    QJsonObject context;
    const QJsonDocument current = QJsonDocument::fromJson(
        m_workspaceContextJson.toUtf8());
    if (current.isObject()) {
        context = current.object();
    }
    if (context.isEmpty()) {
        const QString mode = m_currentWorkspaceMode == WorkspaceModePractice
            ? "practice"
            : "order";
        context = QJsonDocument::fromJson(
            BuildDefaultWorkspaceContextJson(mode).toUtf8()).object();
    }

    QJsonObject identity;
    identity.insert("schemaVersion", 1);
    identity.insert("admissionMode",
                    m_currentWorkspaceMode == WorkspaceModePractice
                        ? "practice"
                        : "orderCreate");
    const bool practiceMode = m_currentWorkspaceMode == WorkspaceModePractice;
    const char* productionModeConfigKey = practiceMode
        ? "device.practiceAllowProductionMode"
        : "device.orderCreateAllowProductionMode";
    identity.insert("productionModeAllowed",
                    m_config
                        ? m_config->GetBool(productionModeConfigKey, practiceMode)
                        : practiceMode);
    identity.insert("preflightStatus", preflight.status);
    identity.insert("commandResult", preflight.commandResult);
    identity.insert("connectionState", preflight.state.connectionState);
    identity.insert("isUsb2", preflight.state.isUsb2 != 0);
    identity.insert("protocolProfile", preflight.productIdentity.protocolProfile);
    identity.insert("model", preflight.state.model);
    identity.insert("modelSource", preflight.state.modelSource);
    identity.insert("modelName", QString::fromUtf8(preflight.state.modelNameUtf8));
    identity.insert("mainBoardFirmwareVersion",
                    QString::fromUtf8(
                        preflight.firmwareVersions.mainBoardVersionUtf8));
    identity.insert("projectionBoardFirmwareVersion",
                    QString::fromUtf8(
                        preflight.firmwareVersions.projectionBoardVersionUtf8));
    identity.insert("mainBoardFirmwareStatus",
                    preflight.firmwareVersions.mainBoardStatus);
    identity.insert("projectionBoardFirmwareStatus",
                    preflight.firmwareVersions.projectionBoardStatus);
    identity.insert("productFamily", preflight.productIdentity.productFamily);
    identity.insert("productModel", preflight.productIdentity.productModel);
    identity.insert("productIdentificationStatus",
                    preflight.productIdentity.identificationStatus);
    // evidence 是 uint64 位标记，使用十进制字符串可避免 JSON double 丢失高位精度。
    identity.insert("productEvidence",
                    QString::number(preflight.productIdentity.evidence));
    identity.insert("productSeriesName",
                    QString::fromUtf8(preflight.productIdentity.seriesNameUtf8));
    identity.insert("productName",
                    QString::fromUtf8(preflight.productIdentity.productNameUtf8));
    identity.insert("reportedDeviceNumber",
                    QString::fromUtf8(
                        preflight.detectionRecord.reportedDeviceNumberUtf8));
    identity.insert("effectiveDeviceNumber",
                    QString::fromUtf8(
                        preflight.detectionRecord.effectiveDeviceNumberUtf8));
    identity.insert("reportedModelCode",
                    QString::fromUtf8(
                        preflight.detectionRecord.reportedModelCodeUtf8));
    identity.insert("effectiveModelCode",
                    QString::fromUtf8(
                        preflight.detectionRecord.effectiveModelCodeUtf8));
    identity.insert("deviceNumberSource",
                    preflight.detectionRecord.deviceNumberSource);
    identity.insert("modelCodeSource",
                    preflight.detectionRecord.modelCodeSource);
    identity.insert("deviceNumberStatus",
                    preflight.detectionRecord.deviceNumberStatus);
    identity.insert("modelCodeStatus",
                    preflight.detectionRecord.modelCodeStatus);
    identity.insert("seriesProbeStatus",
                    preflight.detectionRecord.seriesProbeStatus);
    identity.insert("detectionStatus",
                    preflight.detectionRecord.detectionStatus);
    identity.insert("isProductionMode",
                    preflight.detectionRecord.isProductionMode != 0);
    identity.insert("usedCompatibilityDefaults",
                    preflight.detectionRecord.usedCompatibilityDefaults != 0);
    identity.insert("detail", QString::fromUtf8(preflight.detailUtf8));

    context.insert("deviceIdentity", identity);
    m_workspaceContextJson = QString::fromUtf8(
        QJsonDocument(context).toJson(QJsonDocument::Compact));
    RefreshWorkspaceContextConsumers();
}

// 同步最新工作台上下文到已经创建的各步骤模块。
void MainWindow::RefreshWorkspaceContextConsumers() {
    const QByteArray contextBytes = m_workspaceContextJson.toUtf8();
    if (m_scanWorkflow &&
        !m_scanWorkflow->SetSessionContextJson(contextBytes.constData())) {
        WriteUserAction("ContextRejected",
                        "ScanWorkflowUI rejected refreshed workspace context");
    }
    if (m_dataProcess &&
        !m_dataProcess->SetSessionContextJson(contextBytes.constData())) {
        WriteUserAction("ContextRejected",
                        "DataProcessUI rejected refreshed workspace context");
    }
    if (m_send &&
        !m_send->SetSessionContextJson(contextBytes.constData())) {
        WriteUserAction("ContextRejected",
                        "SendUI rejected refreshed workspace context");
    }
}

// 显示设备准入失败原因。所有客户可见源文本均使用 tr() 包裹英文。
void MainWindow::ShowWorkspaceDevicePreflightMessage(int status) {
    QString message;
    switch (status) {
    case MeyerDeviceCalibrationPreflight_DeviceNotConnected:
        message = tr("Device is not connected.");
        break;
    case MeyerDeviceCalibrationPreflight_Usb2Connected:
        message = tr("Please reconnect the device to a USB 3.0 port.");
        break;
    case MeyerDeviceCalibrationPreflight_ProductionDeviceNumberRequired:
        message = tr("The device number has not been programmed. A programmed device number is required for order scanning.");
        break;
    case MeyerDeviceCalibrationPreflight_ProductIdentityConflict:
        message = tr("The device number does not match the device model.");
        break;
    case MeyerDeviceCalibrationPreflight_DeviceNumberInvalid:
        message = tr("The device number is invalid.");
        break;
    case MeyerDeviceCalibrationPreflight_DeviceModelCodeInvalid:
    case MeyerDeviceCalibrationPreflight_ModelUnknown:
        message = tr("Unable to read the device model.");
        break;
    case MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal:
        message = tr("The device response is abnormal.");
        break;
    case MeyerDeviceCalibrationPreflight_FirmwareVersionReadFailed:
        message = tr("Unable to read the device firmware version.");
        break;
    case MeyerDeviceCalibrationPreflight_ProductFamilyUnsupported:
        message = tr("Current software does not support this device series.");
        break;
    default:
        message = tr("Unable to prepare the device for scanning.");
        break;
    }

    QWidget* parent = m_orderWorkspaceWidget
        ? m_orderWorkspaceWidget->window()
        : this;
    const MeyerShowNoticeDialogFunc showNotice =
        reinterpret_cast<MeyerShowNoticeDialogFunc>(
            m_uiComponentsLibrary.resolve("MeyerUIComponents_ShowNoticeDialog"));
    if (showNotice) {
        const QByteArray titleBytes = tr("Error").toUtf8();
        const QByteArray messageBytes = message.toUtf8();
        const QByteArray confirmBytes = tr("Confirm").toUtf8();
        showNotice(MeyerNoticeDialogError,
                   titleBytes.constData(),
                   messageBytes.constData(),
                   confirmBytes.constData(),
                   parent);
        return;
    }

    // UIComponents 缺失不应吞掉关键门禁提示，使用 Qt 标准弹窗作为最后降级。
    QMessageBox::critical(parent, tr("Error"), message, QMessageBox::Ok);
}
