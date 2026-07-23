#include "SettingsUIInternal.h"

// 创建校准入口卡片。
QWidget* SettingsUIImpl::CreateCalibrationCard(QWidget* parent,
                                               const QString& title,
                                               const QString& description,
                                               int actionId) {
    auto* card = new QFrame(parent);
    card->setObjectName("SettingsCard");
    auto* layout = new QHBoxLayout(card);
    // 横向布局左侧放说明，右侧放动作按钮；说明区域 stretch=1 吃掉剩余宽度。
    layout->setContentsMargins(20, 16, 20, 16);
    auto* textLayout = new QVBoxLayout();
    auto* titleLabel = new QLabel(title, card);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(new QLabel(description, card));
    layout->addLayout(textLayout, 1);

    auto* button = new QPushButton(tr("Calibrate"), card);
    // 每个校准入口使用稳定 objectName，测试宿主和自动化脚本无需依赖翻译后的按钮文字。
    if (actionId == SettingsActionOpenColorCalibration) {
        button->setObjectName("SettingsColorCalibrationButton");
    } else if (actionId == SettingsActionOpen3DCalibration) {
        button->setObjectName("Settings3DCalibrationButton");
    } else {
        button->setObjectName("SettingsPrimary");
    }
    // role 属性让不同 objectName 的校准按钮继续复用同一主按钮视觉规则。
    button->setProperty("settingsRole", "primary");
    // 按钮保持可点击，禁止原因在执行点用提示说明；只做 disabled 会导致客户点击无反馈。
    button->setEnabled(true);
    QObject::connect(button, &QPushButton::clicked, [this, actionId, title, button]() {
        SettingsCalibrationDeviceContext deviceContext = {};
        if (actionId == SettingsActionOpenColorCalibration) {
            // 颜色校准必须依次完成工作台、连接、USB3、设备身份和型号检查；
            // 生产设备编号未写入时可使用带来源的 effective 兼容身份继续。
            // DeviceSessionHost 等待期间会继续处理 Qt 事件，因此临时禁用按钮防止重复点击重入。
            button->setEnabled(false);
            const int status = RunCalibrationPreflight(actionId, &deviceContext);
            button->setEnabled(true);

            if (status != SettingsCalibrationPreflightReady) {
                WriteLog(LogLevel::Warning,
                         "CalibrationBlocked",
                         QString("Color calibration preflight blocked: %1").arg(status));
                ShowCalibrationPreflightMessage(status);
                return;
            }

            // 预检通过后用一个弹窗集中展示设备编号、型号和推断来源。reported 与
            // effective 可能不同，现场人员必须能看出当前是否使用了兼容默认值。
            const QString effectiveNumber = QString::fromUtf8(
                deviceContext.detection.effectiveDeviceNumberUtf8);
            const QString effectiveModelCode = QString::fromUtf8(
                deviceContext.detection.effectiveModelCodeUtf8);
            QStringList informationLines;
            informationLines << tr("Device number: %1").arg(effectiveNumber);
            informationLines << tr("Device model: %1").arg(
                QString::fromUtf8(deviceContext.productNameUtf8));
            informationLines << tr("Device model code: %1").arg(effectiveModelCode);
            informationLines << tr("Main board firmware: %1").arg(
                QString::fromUtf8(
                    deviceContext.firmwareVersions.mainBoardVersionUtf8));
            const QString projectionBoardVersion = QString::fromUtf8(
                deviceContext.firmwareVersions.projectionBoardVersionUtf8);
            if (!projectionBoardVersion.isEmpty()) {
                informationLines << tr("Projection board firmware: %1").arg(
                    projectionBoardVersion);
            }
            if (deviceContext.detection.isProductionMode != 0) {
                informationLines << tr("Production mode: Yes");
            }
            if (deviceContext.detection.usedCompatibilityDefaults != 0) {
                informationLines << tr("Identity source: Compatibility default");
            } else {
                informationLines << tr("Identity source: Device reported");
            }

            // CE 诊断不会阻止旧设备继续使用兼容值，但必须在同一个提示中说明原因。
            switch (deviceContext.detection.modelCodeStatus) {
            case SettingsDeviceModelCodeReadFirmwareTooOld:
                informationLines << tr("The device firmware version is too low; compatibility defaults are in use.");
                break;
            case SettingsDeviceModelCodeReadFrameInvalid:
                informationLines << tr("The device model response is abnormal; compatibility defaults are in use.");
                break;
            case SettingsDeviceModelCodeReadChecksumInvalid:
            case SettingsDeviceModelCodeReadUninitialized:
                informationLines << tr("The device model code is not initialized; compatibility defaults are in use.");
                break;
            case SettingsDeviceModelCodeReadValueInvalid:
                informationLines << tr("The device model code is invalid; compatibility defaults are in use.");
                break;
            default:
                break;
            }
            ShowNoticeDialog(MeyerNoticeDialogInformation,
                             tr("Device Information"),
                             informationLines.join("\n"));
            WriteLog(LogLevel::Info,
                     "DeviceInformationDisplayed",
                     QString("number=%1 modelCode=%2 product=%3 mainBoardVersion=%4 "
                             "projectionBoardVersion=%5 production=%6 compatibility=%7")
                         .arg(effectiveNumber)
                         .arg(effectiveModelCode)
                         .arg(QString::fromUtf8(deviceContext.productNameUtf8))
                         .arg(QString::fromUtf8(
                             deviceContext.firmwareVersions.mainBoardVersionUtf8))
                         .arg(projectionBoardVersion)
                         .arg(deviceContext.detection.isProductionMode)
                         .arg(deviceContext.detection.usedCompatibilityDefaults));

            // MyScan 5/6 在设备信息确认后再提示未校准的扫描头；两头都已校准
            // 时函数不创建弹窗。旧 MyScan 使用共享参数策略，也不会显示双头提示。
            ShowScanHeadColorCalibrationMessage(
                deviceContext.scanHeadColorCalibration);

            // 打开和关闭使用两个动作，MainExe 才能在弹窗整个生命周期内保留唯一设备会话。
            NotifyAction(actionId, title);
            ShowEmbeddedCalibration(actionId, &deviceContext);
            NotifyAction(SettingsActionColorCalibrationClosed, "ColorCalibrationClosed");
            return;
        }

        if (!m_allowCalibration) {
            // 三维校准暂时复用工作台门禁，具体设备预检随后在三维模块接入。
            ShowCalibrationPreflightMessage(SettingsCalibrationPreflightWorkspaceOwnsDevice);
            return;
        }
        ShowEmbeddedCalibration(actionId, nullptr);
        NotifyAction(actionId, title);
    });
    layout->addWidget(button);
    return card;
}


// 在设置窗口上方显示颜色校准遮罩弹窗。
// 参考软件的颜色校准是临时模态流程，不属于设置 QStackedWidget 的并列分类页。
void SettingsUIImpl::ShowColorCalibrationDialog(
    const SettingsCalibrationDeviceContext& deviceContext) {
    if (!m_pages || !m_calibrationColor) {
        WriteLog(LogLevel::Warning,
                 "ShowColorCalibrationDialog",
                 "Color calibration module is not available");
        return;
    }

    // m_pages->window() 返回设置页面所在的顶层客户窗口，遮罩需要覆盖这整个可见区域。
    QWidget* hostWindow = m_pages->window();
    QDialog overlay(hostWindow, Qt::Dialog | Qt::FramelessWindowHint);
    overlay.setObjectName("SettingsCalibrationModalOverlay");
    // 模态窗口阻止用户在校准期间继续点击设置和首页，符合参考图的遮罩交互。
    overlay.setModal(true);
    overlay.setAttribute(Qt::WA_TranslucentBackground, true);
    // 该动态属性是颜色校准模块关闭按钮识别合法弹窗宿主的稳定合同。
    overlay.setProperty("meyerCalibrationDialogHost", true);
    // QDialog 是新的顶层样式作用域，显式加载设置 QSS 才能稳定绘制半透明遮罩。
    MeyerQtModule::ApplyModuleQss(&overlay, "MySettingsUI", "settings.qss", m_logger);

    // 把 SettingsUI 的宿主快照转换为颜色校准模块自己的稳定 POD。
    CalibrationColorDeviceContext colorContext = {};
    colorContext.structSize = sizeof(colorContext);
    colorContext.schemaVersion = MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    colorContext.deviceModel = deviceContext.deviceModel;
    colorContext.modelSource = deviceContext.modelSource;
    colorContext.connectionState = deviceContext.connectionState;
    colorContext.isUsb2 = deviceContext.isUsb2;
    std::strncpy(colorContext.modelNameUtf8,
                 deviceContext.modelNameUtf8,
                 sizeof(colorContext.modelNameUtf8) - 1U);
    std::strncpy(colorContext.deviceIdUtf8,
                 deviceContext.deviceIdUtf8,
                 sizeof(colorContext.deviceIdUtf8) - 1U);
    std::strncpy(colorContext.modelCodeUtf8,
                 deviceContext.modelCodeUtf8,
                 sizeof(colorContext.modelCodeUtf8) - 1U);
    colorContext.productEvidence = deviceContext.productEvidence;
    colorContext.productFamily = deviceContext.productFamily;
    colorContext.productModel = deviceContext.productModel;
    colorContext.productIdentificationStatus = deviceContext.productIdentificationStatus;
    colorContext.protocolProfile = deviceContext.protocolProfile;
    std::strncpy(colorContext.productSeriesNameUtf8,
                 deviceContext.productSeriesNameUtf8,
                 sizeof(colorContext.productSeriesNameUtf8) - 1U);
    std::strncpy(colorContext.productNameUtf8,
                 deviceContext.productNameUtf8,
                 sizeof(colorContext.productNameUtf8) - 1U);
    // 检测上下文逐字段复制，禁止用 reinterpret_cast 依赖两个 DLL 的结构布局偶然一致。
    colorContext.detection.structSize = sizeof(colorContext.detection);
    colorContext.detection.schemaVersion =
        MEYER_CALIBRATION_COLOR_DETECTION_SCHEMA_VERSION;
    colorContext.detection.detectionStatus = deviceContext.detection.detectionStatus;
    colorContext.detection.deviceNumberStatus = deviceContext.detection.deviceNumberStatus;
    colorContext.detection.modelCodeStatus = deviceContext.detection.modelCodeStatus;
    colorContext.detection.seriesProbeStatus = deviceContext.detection.seriesProbeStatus;
    colorContext.detection.isProductionMode = deviceContext.detection.isProductionMode;
    colorContext.detection.usedCompatibilityDefaults =
        deviceContext.detection.usedCompatibilityDefaults;
    colorContext.detection.deviceNumberSource = deviceContext.detection.deviceNumberSource;
    colorContext.detection.modelCodeSource = deviceContext.detection.modelCodeSource;
    std::strncpy(colorContext.detection.reportedDeviceNumberUtf8,
                 deviceContext.detection.reportedDeviceNumberUtf8,
                 sizeof(colorContext.detection.reportedDeviceNumberUtf8) - 1U);
    std::strncpy(colorContext.detection.effectiveDeviceNumberUtf8,
                 deviceContext.detection.effectiveDeviceNumberUtf8,
                 sizeof(colorContext.detection.effectiveDeviceNumberUtf8) - 1U);
    std::strncpy(colorContext.detection.reportedModelCodeUtf8,
                 deviceContext.detection.reportedModelCodeUtf8,
                 sizeof(colorContext.detection.reportedModelCodeUtf8) - 1U);
    std::strncpy(colorContext.detection.effectiveModelCodeUtf8,
                 deviceContext.detection.effectiveModelCodeUtf8,
                 sizeof(colorContext.detection.effectiveModelCodeUtf8) - 1U);
    std::strncpy(colorContext.detection.detailUtf8,
                 deviceContext.detection.detailUtf8,
                 sizeof(colorContext.detection.detailUtf8) - 1U);
    // 版本快照按字段复制，不让颜色校准模块重新建立 DeviceCmd 连接或重复发送命令。
    colorContext.firmwareVersions.structSize =
        sizeof(colorContext.firmwareVersions);
    colorContext.firmwareVersions.schemaVersion =
        MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    colorContext.firmwareVersions.mainBoardStatus =
        deviceContext.firmwareVersions.mainBoardStatus;
    colorContext.firmwareVersions.projectionBoardStatus =
        deviceContext.firmwareVersions.projectionBoardStatus;
    std::strncpy(colorContext.firmwareVersions.mainBoardVersionUtf8,
                 deviceContext.firmwareVersions.mainBoardVersionUtf8,
                 sizeof(colorContext.firmwareVersions.mainBoardVersionUtf8) - 1U);
    std::strncpy(colorContext.firmwareVersions.projectionBoardVersionUtf8,
                 deviceContext.firmwareVersions.projectionBoardVersionUtf8,
                 sizeof(colorContext.firmwareVersions.projectionBoardVersionUtf8) - 1U);
    std::strncpy(colorContext.firmwareVersions.detailUtf8,
                 deviceContext.firmwareVersions.detailUtf8,
                 sizeof(colorContext.firmwareVersions.detailUtf8) - 1U);
    // 大小扫描头状态逐字段复制，避免不同 DLL 之间依赖相同内存布局。
    colorContext.scanHeadColorCalibration.structSize =
        sizeof(colorContext.scanHeadColorCalibration);
    colorContext.scanHeadColorCalibration.schemaVersion =
        MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    colorContext.scanHeadColorCalibration.policy =
        deviceContext.scanHeadColorCalibration.policy;
    colorContext.scanHeadColorCalibration.firmwareCompatibility =
        deviceContext.scanHeadColorCalibration.firmwareCompatibility;
    colorContext.scanHeadColorCalibration.largeHeadStatus =
        deviceContext.scanHeadColorCalibration.largeHeadStatus;
    colorContext.scanHeadColorCalibration.smallHeadStatus =
        deviceContext.scanHeadColorCalibration.smallHeadStatus;
    colorContext.scanHeadColorCalibration.largeHeadCommandResult =
        deviceContext.scanHeadColorCalibration.largeHeadCommandResult;
    colorContext.scanHeadColorCalibration.smallHeadCommandResult =
        deviceContext.scanHeadColorCalibration.smallHeadCommandResult;
    std::strncpy(colorContext.scanHeadColorCalibration.detailUtf8,
                 deviceContext.scanHeadColorCalibration.detailUtf8,
                 sizeof(colorContext.scanHeadColorCalibration.detailUtf8) - 1U);
    if (!m_calibrationColor->SetDeviceContext(&colorContext)) {
        WriteLog(LogLevel::Warning,
                 "ShowColorCalibrationDialog",
                 "CalibrationColorUI rejected the verified device context");
        return;
    }

    // 颜色校准 DLL 只创建自己的面板内容；遮罩、居中和模态生命周期由 SettingsUI 管理。
    QWidget* calibrationWidget = m_calibrationColor->CreateWidget(&overlay);
    if (!calibrationWidget) {
        WriteLog(LogLevel::Warning,
                 "ShowColorCalibrationDialog",
                 "Color calibration widget creation failed");
        return;
    }
    if (hostWindow) {
        // 顶层 QDialog 的 move() 使用屏幕坐标，因此先把宿主左上角转换为全局坐标。
        overlay.resize(hostWindow->size());
        overlay.move(hostWindow->mapToGlobal(QPoint(0, 0)));
    } else {
        // 理论上设置页一定存在宿主；保留降级尺寸防止异常测试环境出现零尺寸窗口。
        overlay.resize(960, 640);
    }

    // 透明顶层 QDialog 在部分 Windows 合成环境下不会绘制 rgba 背景，
    // 因此使用一个明确的子控件承担蒙层绘制，避免出现“面板有了但背景不变暗”。
    auto* dimmer = new QWidget(&overlay);
    dimmer->setObjectName("SettingsCalibrationDimmer");
    dimmer->setAttribute(Qt::WA_StyledBackground, true);
    dimmer->setGeometry(overlay.rect());
    dimmer->show();
    dimmer->lower();

    // 不把颜色面板放进 Layout，标题栏拖动才能真正改变面板位置；每次打开时先居中。
    calibrationWidget->adjustSize();
    const QSize availablePanelSize = QSize(qMax(1, overlay.width() - 32),
                                           qMax(1, overlay.height() - 32));
    calibrationWidget->resize(calibrationWidget->sizeHint().boundedTo(availablePanelSize));
    calibrationWidget->move((overlay.width() - calibrationWidget->width()) / 2,
                            (overlay.height() - calibrationWidget->height()) / 2);
    calibrationWidget->show();
    // 面板必须位于 dimmer 上方，否则蒙层会遮住颜色校准按钮和预览图。
    calibrationWidget->raise();

    WriteLog(LogLevel::Info, "ShowColorCalibrationDialog", "Color calibration dialog opened");
    // 显式置前可避免宿主由第三方程序拉起时，模态弹窗被其它顶层窗口遮挡。
    overlay.show();
    overlay.raise();
    overlay.activateWindow();
    // exec() 使用 Qt 嵌套事件循环：校准弹窗仍响应消息，但调用方在弹窗关闭前不会继续切页。
    overlay.exec();
    WriteLog(LogLevel::Info, "ShowColorCalibrationDialog", "Color calibration dialog closed");
}

// 在设置页面内部嵌入校准模块页面。
// 三维校准当前保留嵌入式工作流；颜色校准按参考软件改由独立遮罩弹窗承载。
void SettingsUIImpl::ShowEmbeddedCalibration(
    int actionId,
    const SettingsCalibrationDeviceContext* deviceContext) {
    if (!m_pages) {
        // 没有页面容器时无法嵌入校准 UI，直接返回避免空指针。
        return;
    }
    if (!m_allowCalibration && actionId != SettingsActionOpenColorCalibration) {
        // 扫描重建来源打开设置时，不允许进入三维/颜色校准。
        WriteLog(LogLevel::Warning, "ShowEmbeddedCalibration", "Calibration is blocked for current open source");
        return;
    }
    // 校准模块按需加载，避免设置页面刚打开就占用额外 DLL/算法资源。
    LoadCalibrationModules();

    if (actionId == SettingsActionOpenColorCalibration) {
        if (!deviceContext || deviceContext->status != SettingsCalibrationPreflightReady) {
            WriteLog(LogLevel::Warning,
                     "ShowEmbeddedCalibration",
                     "Verified color calibration device context is missing");
            return;
        }
        // 颜色校准是短时模态操作，不创建动态 QStackedWidget 页面和返回按钮。
        ShowColorCalibrationDialog(*deviceContext);
        return;
    }

    QWidget* calibrationWidget = nullptr;
    QString title;
    if (actionId == SettingsActionOpen3DCalibration) {
        title = tr("3D Calibration");
        // CreateWidget 的 parent 传 m_pages，让校准页面生命周期挂到设置页容器下。
        calibrationWidget = m_calibration3D ? m_calibration3D->CreateWidget(m_pages) : nullptr;
    }

    auto* wrapper = new QWidget(m_pages);
    auto* layout = new QVBoxLayout(wrapper);
    auto* back = new QPushButton(tr("Back to Calibration Settings"), wrapper);
    QObject::connect(back, &QPushButton::clicked, [this, wrapper]() {
        // 先切回校准总览，再延迟删除 wrapper。
        // deleteLater 避免在按钮 clicked 调用栈中立即销毁按钮自己的父对象。
        RestoreSettingsOverview();
        wrapper->deleteLater();
    });
    layout->addWidget(back, 0, Qt::AlignLeft);
    if (calibrationWidget) {
        // 校准模块返回的页面作为 wrapper 子层内容，占据剩余空间。
        layout->addWidget(calibrationWidget, 1);
    } else {
        // 加载失败时显示占位，而不是让设置页停留在空白区域。
        layout->addWidget(new QLabel(tr("Calibration module is not available."), wrapper), 1);
    }

    const int index = m_pages->addWidget(wrapper);
    // 动态加入 wrapper 后立即切过去，用户看到的是设置壳中的校准子流程。
    m_pages->setCurrentIndex(index);
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

// 返回设置校准分类页。
void SettingsUIImpl::RestoreSettingsOverview() {
    if (!m_pages) {
        // 设置页已释放时不再操作 QStackedWidget。
        return;
    }
    // PageCalibration 是固定分类页，不是动态校准 wrapper。
    m_pages->setCurrentIndex(PageCalibration);
    if (m_titleLabel) {
        m_titleLabel->setText(tr("Settings"));
    }
}

// 根据打开来源刷新校准入口。
// 分类保持可见，真正设备动作在按钮执行点拦截并说明原因，避免 disabled 无反馈。
void SettingsUIImpl::ApplyCalibrationAvailability() {
    if (m_calibrationNavButton) {
        m_calibrationNavButton->setVisible(true);
        m_calibrationNavButton->setEnabled(true);
        m_calibrationNavButton->setProperty("calibrationAllowed", m_allowCalibration);
    }

    if (m_calibrationPage) {
        m_calibrationPage->setEnabled(true);
        m_calibrationPage->setProperty("calibrationAllowed", m_allowCalibration);
    }
}

// 执行颜色校准入口的同步宿主预检。
int SettingsUIImpl::RunCalibrationPreflight(
    int actionId,
    SettingsCalibrationDeviceContext* deviceContext) {
    if (!deviceContext) {
        return SettingsCalibrationPreflightInternalError;
    }
    std::memset(deviceContext, 0, sizeof(*deviceContext));
    deviceContext->structSize = sizeof(*deviceContext);
    deviceContext->schemaVersion = MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;

    // 创建、练习工作台已经持有设备连接，必须在访问 DeviceCmd 前直接拦截。
    if (!m_allowCalibration || m_openSource == SettingsOpenSourceScanReconstruct) {
        deviceContext->status = SettingsCalibrationPreflightWorkspaceOwnsDevice;
        std::strncpy(deviceContext->detailUtf8,
                     "Order/practice workspace owns the device session",
                     sizeof(deviceContext->detailUtf8) - 1U);
        return deviceContext->status;
    }

    if (!m_calibrationPreflightCallback) {
        deviceContext->status = SettingsCalibrationPreflightInternalError;
        std::strncpy(deviceContext->detailUtf8,
                     "Calibration preflight callback is not registered",
                     sizeof(deviceContext->detailUtf8) - 1U);
        return deviceContext->status;
    }

    const int callbackStatus = m_calibrationPreflightCallback(
        m_calibrationPreflightContext, actionId, deviceContext);
    // 宿主返回值和结构状态应一致；不一致时按更保守的回调返回值处理。
    deviceContext->status = callbackStatus;
    if (callbackStatus == SettingsCalibrationPreflightReady &&
        (deviceContext->detection.structSize !=
             sizeof(SettingsDeviceDetectionContext) ||
         deviceContext->detection.schemaVersion !=
             MEYER_SETTINGS_DEVICE_DETECTION_SCHEMA_VERSION ||
         deviceContext->detection.effectiveDeviceNumberUtf8[0] == '\0' ||
         deviceContext->detection.effectiveModelCodeUtf8[0] == '\0' ||
         deviceContext->firmwareVersions.structSize !=
             sizeof(SettingsFirmwareVersionContext) ||
         deviceContext->firmwareVersions.schemaVersion !=
             MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION ||
         deviceContext->firmwareVersions.mainBoardStatus !=
             SettingsFirmwareVersionValid ||
         deviceContext->firmwareVersions.mainBoardVersionUtf8[0] == '\0' ||
         (deviceContext->firmwareVersions.projectionBoardStatus !=
              SettingsFirmwareVersionValid &&
          deviceContext->firmwareVersions.projectionBoardStatus !=
              SettingsFirmwareVersionNotRequired) ||
         (deviceContext->firmwareVersions.projectionBoardStatus ==
              SettingsFirmwareVersionValid &&
          deviceContext->firmwareVersions.projectionBoardVersionUtf8[0] == '\0') ||
         deviceContext->scanHeadColorCalibration.structSize !=
             sizeof(SettingsScanHeadColorCalibrationContext) ||
         deviceContext->scanHeadColorCalibration.schemaVersion !=
             MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION ||
         (deviceContext->scanHeadColorCalibration.policy !=
              SettingsScanHeadColorCalibrationPolicyLargeOnlyShared &&
          deviceContext->scanHeadColorCalibration.policy !=
              SettingsScanHeadColorCalibrationPolicyLargeAndSmall) ||
         (deviceContext->scanHeadColorCalibration.policy ==
              SettingsScanHeadColorCalibrationPolicyLargeAndSmall &&
          (deviceContext->scanHeadColorCalibration.firmwareCompatibility !=
               SettingsColorCalibrationFirmwareSupported ||
           (deviceContext->scanHeadColorCalibration.largeHeadStatus !=
                SettingsScanHeadColorCalibrationCalibrated &&
            deviceContext->scanHeadColorCalibration.largeHeadStatus !=
                SettingsScanHeadColorCalibrationNotCalibrated) ||
           (deviceContext->scanHeadColorCalibration.smallHeadStatus !=
                SettingsScanHeadColorCalibrationCalibrated &&
            deviceContext->scanHeadColorCalibration.smallHeadStatus !=
                SettingsScanHeadColorCalibrationNotCalibrated))) ||
         (deviceContext->scanHeadColorCalibration.policy ==
              SettingsScanHeadColorCalibrationPolicyLargeOnlyShared &&
          (deviceContext->scanHeadColorCalibration.firmwareCompatibility !=
               SettingsColorCalibrationFirmwareNotRequired ||
           deviceContext->scanHeadColorCalibration.smallHeadStatus !=
               SettingsScanHeadColorCalibrationNotRequired)))) {
        // Ready 必须携带完整有效身份。旧 MainExe 或损坏 POD 不能继续弹出一个
        // 空设备信息窗口，更不能把空值注入颜色校准算法入口。
        deviceContext->status = SettingsCalibrationPreflightInternalError;
        std::strncpy(deviceContext->detailUtf8,
                     "Calibration preflight returned an invalid identity or firmware context",
                     sizeof(deviceContext->detailUtf8) - 1U);
        return deviceContext->status;
    }
    return callbackStatus;
}

// 显示颜色校准预检失败原因。
void SettingsUIImpl::ShowCalibrationPreflightMessage(int status) {
    QString message;
    switch (status) {
    case SettingsCalibrationPreflightWorkspaceOwnsDevice:
        message = tr("Please exit the scan module before calibration.");
        break;
    case SettingsCalibrationPreflightDeviceNotConnected:
        message = tr("Device is not connected.");
        break;
    case SettingsCalibrationPreflightUsb2Connected:
        message = tr("Please reconnect the device to a USB 3.0 port.");
        break;
    case SettingsCalibrationPreflightWirelessUnsupported:
        message = tr("The connected device type is not supported for calibration yet.");
        break;
    case SettingsCalibrationPreflightDeviceInfoReadFailed:
    case SettingsCalibrationPreflightModelUnknown:
        message = tr("Unable to read the device model.");
        break;
    case SettingsCalibrationPreflightMachineCodeReadFailed:
        message = tr("Unable to read the device number.");
        break;
    case SettingsCalibrationPreflightProductIdentityConflict:
        message = tr("The device number does not match the device model.");
        break;
    case SettingsCalibrationPreflightDeviceResponseAbnormal:
        message = tr("The device response is abnormal.");
        break;
    case SettingsCalibrationPreflightDeviceNumberInvalid:
        message = tr("The device number is invalid.");
        break;
    case SettingsCalibrationPreflightDeviceModelCodeInvalid:
        message = tr("The device model code is invalid.");
        break;
    case SettingsCalibrationPreflightFirmwareVersionReadFailed:
        message = tr("Unable to read the device firmware version.");
        break;
    case SettingsCalibrationPreflightColorCalibrationFirmwareUnsupported:
        message = tr("Please update the device firmware before color calibration.");
        break;
    case SettingsCalibrationPreflightScanHeadColorCalibrationReadFailed:
        message = tr("Unable to read the scan head color calibration status.");
        break;
    case SettingsCalibrationPreflightProductFamilyUnsupported:
        message = tr("Current software does not support this device series.");
        break;
    default:
        message = tr("Unable to prepare the device for calibration.");
        break;
    }

    const int level = status == SettingsCalibrationPreflightWorkspaceOwnsDevice
        ? MeyerNoticeDialogInformation
        : MeyerNoticeDialogError;
    ShowNoticeDialog(level,
                     level == MeyerNoticeDialogError ? tr("Error") : tr("Notice"),
                     message);
}

// 根据 DeviceCmd 已归一化的双扫描头状态显示提示。该函数不解释校验和，
// 也不把 NotChecked/通信失败降级为“未校准”；这些异常已在预检阶段被拦截。
void SettingsUIImpl::ShowScanHeadColorCalibrationMessage(
    const SettingsScanHeadColorCalibrationContext& context) {
    if (context.policy != SettingsScanHeadColorCalibrationPolicyLargeAndSmall) {
        return;
    }

    const bool largeMissing =
        context.largeHeadStatus == SettingsScanHeadColorCalibrationNotCalibrated;
    const bool smallMissing =
        context.smallHeadStatus == SettingsScanHeadColorCalibrationNotCalibrated;
    if (!largeMissing && !smallMissing) {
        return;
    }

    QString message;
    if (largeMissing && smallMissing) {
        message = tr("Large and small scan heads are not color calibrated.");
    } else if (largeMissing) {
        message = tr("Large scan head is not color calibrated.");
    } else {
        message = tr("Small scan head is not color calibrated.");
    }
    WriteLog(LogLevel::Warning,
             "ScanHeadColorCalibrationMissing",
             QString("largeStatus=%1 smallStatus=%2")
                 .arg(context.largeHeadStatus)
                 .arg(context.smallHeadStatus));
    ShowNoticeDialog(MeyerNoticeDialogInformation,
                     tr("Notice"),
                     message);
}

// 显示统一单按钮提示。
// 公共 DLL 缺失或 ABI 不匹配时保留 QMessageBox，避免提示失败反过来中断设置流程。
void SettingsUIImpl::ShowNoticeDialog(int level,
                                      const QString& title,
                                      const QString& message) {
    QWidget* parent = m_pages ? m_pages->window() : nullptr;
    if (m_showNoticeDialog) {
        // QByteArray 的生命周期覆盖同步弹窗调用，传入 C ABI 的 const char* 始终有效。
        const QByteArray titleBytes = title.toUtf8();
        const QByteArray messageBytes = message.toUtf8();
        const QByteArray confirmBytes = tr("Confirm").toUtf8();
        m_showNoticeDialog(level,
                           titleBytes.constData(),
                           messageBytes.constData(),
                           confirmBytes.constData(),
                           parent);
        return;
    }

    // 降级弹窗只负责保证客户看见信息；正常发布目录应始终使用 UIComponents。
    QMessageBox::information(parent, title, message, QMessageBox::Ok);
}
