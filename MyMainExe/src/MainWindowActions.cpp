
#include "MainWindow.h"

#include <QTimer>
#include <cstring>

#include "device/DeviceSessionHost.h"

// 首页入口统一从这里分发，避免 HomeUI 自己切换其他模块页面。
void MainWindow::HandleHomeEntryClicked(int entryId) {
    // 每一次客户点击都先写日志，再进入具体页面流程。
    WriteUserAction("HomeEntryClicked", QString("Home entry clicked: %1").arg(entryId));

    const char* featureId = HomeEntryFeatureId(entryId);
    if (featureId && !IsFeatureEnabled(featureId, true)) {
        // enabled=false 时即使 UI 误触发回调，MainExe 也不继续执行动作。
        WriteUserAction("PermissionDenied", QString("Home entry disabled: %1").arg(featureId));
        WriteStatus(tr("Feature is disabled"));
        return;
    }

    switch (entryId) {
    case HomeEntryBrowse:
        // 浏览入口进入 CaseUI，ShowCase 内部会懒加载并释放首页资源。
        ShowCase();
        break;
    case HomeEntrySettings:
        // 首页设置入口进入 SettingsUI，设置关闭后返回首页。
        ShowSettings(SettingsOpenSourceHome);
        break;
    case HomeEntryCreate:
        // 创建入口进入统一建单/扫描工作台，工作台第一步挂载 OrderCreateUI。
        ShowOrderWorkspace();
        break;
    case HomeEntryPractice:
        // 练习入口进入同一个工作台壳，但只显示 Scan/Process 两步。
        ShowPracticeWorkspace();
        break;
    case HomeActionMinimize:
        // 页面模块只上报动作，真正的顶层窗口操作始终由 MainExe 执行。
        showMinimized();
        break;
    case HomeActionClose:
        close();
        break;
    case HomeActionCalibration:
        // 校准入口当前先进入设置模块；具体打开三维还是颜色校准由设置页继续选择。
        ShowSettings(SettingsOpenSourceHome);
        break;
    case HomeActionCloud:
        // 云端页面尚未拆分，先保留完整日志和状态，不在 HomeUI 中伪造业务流程。
        WriteStatus(tr("Cloud action recorded"));
        break;
    case HomeActionHelp:
        // 帮助中心后续按独立页面/外部文档入口接入，当前只记录动作。
        WriteStatus(tr("Help action recorded"));
        break;
    default:
        WriteStatus(tr("Home entry %1 is not implemented yet").arg(entryId));
        break;
    }
}

// 案例管理动作统一从这里分发，避免 CaseUI 自己切换其他模块页面。
void MainWindow::HandleCaseAction(int actionId) {
    // CaseUI 的按钮、Tab 切换、打开订单等都走这里统一分发。
    WriteUserAction("CaseAction", QString("Case action clicked: %1").arg(actionId));

    const char* featureId = CaseActionFeatureId(actionId);
    if (featureId && !IsFeatureEnabled(featureId, true)) {
        // CaseUI 的按钮禁用只是第一层体验，MainExe 仍要做执行前复核。
        WriteUserAction("PermissionDenied", QString("Case action disabled: %1").arg(featureId));
        WriteStatus(tr("Feature is disabled"));
        return;
    }

    switch (actionId) {
    case CaseActionBackHome:
        // 返回首页时释放案例页，避免浏览模块长期占用资源。
        ShowHome();
        break;
    case CaseActionOpenOrder:
        // 打开订单是进入扫描重建前的关键入口，必须先释放案例页。
        PrepareForScanReconstruct();
        break;
    case CaseActionOpenSettings:
        // 浏览页打开设置，关闭设置后回到浏览页。
        ShowSettings(SettingsOpenSourceCase);
        break;
    case CaseActionMinimize:
        showMinimized();
        break;
    case CaseActionClose:
        // 浏览页右上角关闭按钮的产品语义是关闭当前浏览页并回到首页。
        ShowHome();
        break;
    case CaseActionCloud:
        // 云端病例同步尚未接入；先保留稳定动作 ID、日志和状态反馈。
        WriteStatus(tr("Cloud action recorded"));
        break;
    case CaseActionScreenshot:
        // 截图文件策略后续由统一截图/导出服务实现，当前不让 CaseUI 直接写文件。
        WriteStatus(tr("Screenshot action recorded"));
        break;
    case CaseActionSwitchTab:
        break;
    default:
        WriteStatus(tr("Case action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 设置模块动作统一从这里分发，避免 SettingsUI 自己切换 MainExe 页面。
void MainWindow::HandleSettingsAction(int actionId) {
    WriteUserAction("SettingsAction", QString("Settings action clicked: %1").arg(actionId));

    switch (actionId) {
    case SettingsActionConfirm:
    case SettingsActionClose:
        // 当前骨架期 Confirm/Close 都只关闭设置并返回来源页面。
        if (m_settingsOpenSource == SettingsOpenSourceCase) {
            ShowCase();
        } else {
            ShowHome();
        }
        break;
    case SettingsActionApply:
        // Apply 先只记录日志，后续接 ConfigCenter/SettingsService 保存配置。
        WriteStatus(tr("Settings applied"));
        break;
    case SettingsActionRestore:
        // Restore 先只记录日志，后续接 ConfigCenter 默认值恢复。
        WriteStatus(tr("Settings restored"));
        break;
    case SettingsActionOpen3DCalibration:
    case SettingsActionOpenColorCalibration:
        // SettingsUI 已通过同步回调完成设备预检；这里记录弹窗开始显示。
        WriteStatus(tr("Calibration page opened"));
        break;
    case SettingsActionColorCalibrationClosed:
        // 颜色校准弹窗关闭后立即释放唯一 DeviceCmd/Transport 会话，避免设置页空闲时占用设备。
        if (m_deviceSessionHost) {
            m_deviceSessionHost->CloseSession();
        }
        WriteStatus(tr("Color calibration closed"));
        break;
    default:
        WriteStatus(tr("Settings action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 完成颜色校准入口的工作台和设备预检。
int MainWindow::HandleCalibrationPreflight(
    int actionId,
    SettingsCalibrationDeviceContext* deviceContext) {
    if (!deviceContext || actionId != SettingsActionOpenColorCalibration) {
        return SettingsCalibrationPreflightInternalError;
    }

    std::memset(deviceContext, 0, sizeof(*deviceContext));
    deviceContext->structSize = sizeof(*deviceContext);
    deviceContext->schemaVersion = MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;

    // 创建工作台的 Order/Scan/Process/Send 和练习工作台的 Scan/Process
    // 都共用扫描设备。即使 SettingsUI 已经释放工作台 QWidget，也要按打开来源拦截。
    if (m_settingsOpenedFromActiveWorkspace ||
        m_settingsOpenSource == SettingsOpenSourceScanReconstruct) {
        deviceContext->status = SettingsCalibrationPreflightWorkspaceOwnsDevice;
        std::strncpy(deviceContext->detailUtf8,
                     "Order or practice workspace owns the device session",
                     sizeof(deviceContext->detailUtf8) - 1U);
        WriteUserAction("ColorCalibrationBlocked",
                        "Order/practice workspace owns the device session");
        return deviceContext->status;
    }

    if (!m_deviceSessionHost) {
        deviceContext->status = SettingsCalibrationPreflightInternalError;
        std::strncpy(deviceContext->detailUtf8,
                     "MainExe device session host is unavailable",
                     sizeof(deviceContext->detailUtf8) - 1U);
        return deviceContext->status;
    }

    MeyerDeviceCalibrationPreflight preflight = {};
    const std::int32_t result =
        m_deviceSessionHost->PrepareColorCalibration(&preflight);
    if (result != MeyerDeviceCmdResult_Ok) {
        deviceContext->status = SettingsCalibrationPreflightInternalError;
        std::strncpy(deviceContext->detailUtf8,
                     preflight.detailUtf8,
                     sizeof(deviceContext->detailUtf8) - 1U);
        WriteUserAction("ColorCalibrationPreflightFailed",
                        QString("Device host API result: %1").arg(result));
        return deviceContext->status;
    }

    // Settings 和颜色校准模块只接收所需字段副本，不获得 DeviceCmd 句柄或函数表。
    deviceContext->status = preflight.status;
    deviceContext->deviceModel = preflight.state.model;
    deviceContext->modelSource = preflight.state.modelSource;
    deviceContext->connectionState = preflight.state.connectionState;
    deviceContext->isUsb2 = preflight.state.isUsb2;
    std::strncpy(deviceContext->modelNameUtf8,
                 preflight.state.modelNameUtf8,
                 sizeof(deviceContext->modelNameUtf8) - 1U);
    std::strncpy(deviceContext->deviceIdUtf8,
                 preflight.state.deviceIdUtf8,
                 sizeof(deviceContext->deviceIdUtf8) - 1U);
    std::strncpy(deviceContext->detailUtf8,
                 preflight.detailUtf8,
                 sizeof(deviceContext->detailUtf8) - 1U);
    std::strncpy(deviceContext->modelCodeUtf8,
                 preflight.state.modelCodeUtf8,
                 sizeof(deviceContext->modelCodeUtf8) - 1U);
    // 产品身份由 DeviceCmd 结合设备编号和完整型号代码生成；MainExe 只复制 POD，
    // SettingsUI/CalibrationColorUI 不再重复维护型号映射表。
    deviceContext->productEvidence = preflight.productIdentity.evidence;
    deviceContext->productFamily = preflight.productIdentity.productFamily;
    deviceContext->productModel = preflight.productIdentity.productModel;
    deviceContext->productIdentificationStatus =
        preflight.productIdentity.identificationStatus;
    deviceContext->protocolProfile = preflight.productIdentity.protocolProfile;
    std::strncpy(deviceContext->productSeriesNameUtf8,
                 preflight.productIdentity.seriesNameUtf8,
                 sizeof(deviceContext->productSeriesNameUtf8) - 1U);
    std::strncpy(deviceContext->productNameUtf8,
                 preflight.productIdentity.productNameUtf8,
                 sizeof(deviceContext->productNameUtf8) - 1U);

    // 检测记录必须逐字段复制到 SettingsUI 自有 POD。两个模块不共享 DeviceCmd
    // 结构定义，避免设置 DLL 因设备层头文件变化形成静态链接依赖。
    deviceContext->detection.structSize = sizeof(deviceContext->detection);
    deviceContext->detection.schemaVersion =
        MEYER_SETTINGS_DEVICE_DETECTION_SCHEMA_VERSION;
    deviceContext->detection.detectionStatus =
        preflight.detectionRecord.detectionStatus;
    deviceContext->detection.deviceNumberStatus =
        preflight.detectionRecord.deviceNumberStatus;
    deviceContext->detection.modelCodeStatus =
        preflight.detectionRecord.modelCodeStatus;
    deviceContext->detection.seriesProbeStatus =
        preflight.detectionRecord.seriesProbeStatus;
    deviceContext->detection.isProductionMode =
        preflight.detectionRecord.isProductionMode;
    deviceContext->detection.usedCompatibilityDefaults =
        preflight.detectionRecord.usedCompatibilityDefaults;
    deviceContext->detection.deviceNumberSource =
        preflight.detectionRecord.deviceNumberSource;
    deviceContext->detection.modelCodeSource =
        preflight.detectionRecord.modelCodeSource;
    std::strncpy(deviceContext->detection.reportedDeviceNumberUtf8,
                 preflight.detectionRecord.reportedDeviceNumberUtf8,
                 sizeof(deviceContext->detection.reportedDeviceNumberUtf8) - 1U);
    std::strncpy(deviceContext->detection.effectiveDeviceNumberUtf8,
                 preflight.detectionRecord.effectiveDeviceNumberUtf8,
                 sizeof(deviceContext->detection.effectiveDeviceNumberUtf8) - 1U);
    std::strncpy(deviceContext->detection.reportedModelCodeUtf8,
                 preflight.detectionRecord.reportedModelCodeUtf8,
                 sizeof(deviceContext->detection.reportedModelCodeUtf8) - 1U);
    std::strncpy(deviceContext->detection.effectiveModelCodeUtf8,
                 preflight.detectionRecord.effectiveModelCodeUtf8,
                 sizeof(deviceContext->detection.effectiveModelCodeUtf8) - 1U);
    std::strncpy(deviceContext->detection.detailUtf8,
                 preflight.detectionRecord.detailUtf8,
                 sizeof(deviceContext->detection.detailUtf8) - 1U);

    // 版本信息与身份信息一起复制，保证颜色校准模块只消费一次预检结果；
    // 不把主控板/投图板版本重新查询分散到 SettingsUI 或 CalibrationColorUI。
    deviceContext->firmwareVersions.structSize =
        sizeof(deviceContext->firmwareVersions);
    deviceContext->firmwareVersions.schemaVersion =
        MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;
    deviceContext->firmwareVersions.mainBoardStatus =
        preflight.firmwareVersions.mainBoardStatus;
    deviceContext->firmwareVersions.projectionBoardStatus =
        preflight.firmwareVersions.projectionBoardStatus;
    std::strncpy(deviceContext->firmwareVersions.mainBoardVersionUtf8,
                 preflight.firmwareVersions.mainBoardVersionUtf8,
                 sizeof(deviceContext->firmwareVersions.mainBoardVersionUtf8) - 1U);
    std::strncpy(deviceContext->firmwareVersions.projectionBoardVersionUtf8,
                 preflight.firmwareVersions.projectionBoardVersionUtf8,
                 sizeof(deviceContext->firmwareVersions.projectionBoardVersionUtf8) - 1U);
    std::strncpy(deviceContext->firmwareVersions.detailUtf8,
                 preflight.firmwareVersions.detailUtf8,
                 sizeof(deviceContext->firmwareVersions.detailUtf8) - 1U);

    // 扫描头颜色参数状态与版本快照一样逐字段复制。MainExe 不根据 A4/BA
    // 校验和自行推断“未校准”，只转发 DeviceCmd 已归一化的结果。
    deviceContext->scanHeadColorCalibration.structSize =
        sizeof(deviceContext->scanHeadColorCalibration);
    deviceContext->scanHeadColorCalibration.schemaVersion =
        MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION;
    deviceContext->scanHeadColorCalibration.policy =
        preflight.scanHeadColorCalibration.policy;
    deviceContext->scanHeadColorCalibration.firmwareCompatibility =
        preflight.scanHeadColorCalibration.firmwareCompatibility;
    deviceContext->scanHeadColorCalibration.largeHeadStatus =
        preflight.scanHeadColorCalibration.largeHeadStatus;
    deviceContext->scanHeadColorCalibration.smallHeadStatus =
        preflight.scanHeadColorCalibration.smallHeadStatus;
    deviceContext->scanHeadColorCalibration.largeHeadCommandResult =
        preflight.scanHeadColorCalibration.largeHeadCommandResult;
    deviceContext->scanHeadColorCalibration.smallHeadCommandResult =
        preflight.scanHeadColorCalibration.smallHeadCommandResult;
    std::strncpy(deviceContext->scanHeadColorCalibration.detailUtf8,
                 preflight.scanHeadColorCalibration.detailUtf8,
                 sizeof(deviceContext->scanHeadColorCalibration.detailUtf8) - 1U);

    WriteUserAction("ColorCalibrationPreflight",
                    QString("status=%1 detection=%2 profile=%3 usb2=%4 mainBoardVersion=%5 "
                            "projectionBoardVersion=%6 reportedNumber=%7 "
                            "effectiveNumber=%8 reportedModelCode=%9 effectiveModelCode=%10 "
                            "product=%11 identityStatus=%12 production=%13 compatibility=%14 "
                            "scanHeadPolicy=%15 largeHeadStatus=%16 smallHeadStatus=%17")
                        .arg(deviceContext->status)
                        .arg(deviceContext->detection.detectionStatus)
                        .arg(deviceContext->deviceModel)
                        .arg(deviceContext->isUsb2)
                        .arg(QString::fromUtf8(
                            deviceContext->firmwareVersions.mainBoardVersionUtf8))
                        .arg(QString::fromUtf8(
                            deviceContext->firmwareVersions.projectionBoardVersionUtf8))
                        .arg(QString::fromUtf8(
                            deviceContext->detection.reportedDeviceNumberUtf8))
                        .arg(QString::fromUtf8(
                            deviceContext->detection.effectiveDeviceNumberUtf8))
                        .arg(QString::fromUtf8(
                            deviceContext->detection.reportedModelCodeUtf8))
                        .arg(QString::fromUtf8(
                            deviceContext->detection.effectiveModelCodeUtf8))
                        .arg(QString::fromUtf8(deviceContext->productNameUtf8))
                        .arg(deviceContext->productIdentificationStatus)
                        .arg(deviceContext->detection.isProductionMode)
                        .arg(deviceContext->detection.usedCompatibilityDefaults)
                        .arg(deviceContext->scanHeadColorCalibration.policy)
                        .arg(deviceContext->scanHeadColorCalibration.largeHeadStatus)
                        .arg(deviceContext->scanHeadColorCalibration.smallHeadStatus));
    return deviceContext->status;
}

// 建单页面动作统一由 MainExe 分发。
// OrderCreateUI 只负责表单和动作 ID，不保存数据库、不启动扫描进程。
void MainWindow::HandleOrderCreateAction(int actionId) {
    WriteUserAction("OrderCreateAction", QString("Order create action clicked: %1").arg(actionId));

    switch (actionId) {
    case OrderCreateActionCancel:
    case OrderCreateActionPrevious:
        // 当前初版把取消/上一步都回到首页。
        // 后续如果从浏览页或第三方入口进入，可在上下文中记录 returnPage 再做精确返回。
        ShowHome();
        break;
    case OrderCreateActionConfirm:
        // UI 只导出表单快照；MainExe 在动作入口复核权限，再交给 CaseOrderService 保存。
        if (!IsFeatureEnabled("order.create", true)) {
            WriteUserAction("PermissionDenied", "Order create confirm is disabled");
            WriteStatus(tr("Order creation is disabled"));
            break;
        }
        WriteStatus(SaveCurrentOrderContext()
            ? tr("Order saved")
            : tr("Order save failed"));
        break;
    case OrderCreateActionNext:
        // 进入扫描前必须先通过权限复核并成功保存，避免产生只有扫描数据、没有订单记录的孤立目录。
        if (!IsFeatureEnabled("order.create", true)) {
            WriteUserAction("PermissionDenied", "Order create next is disabled");
            WriteStatus(tr("Order creation is disabled"));
            break;
        }
        if (!SaveCurrentOrderContext()) {
            WriteStatus(tr("Order save failed"));
            break;
        }
        if (m_orderWorkspace) {
            RefreshWorkspaceScanProcessFromOrder();
            // 创建模式先完成设备准入，再创建 ScanWorkflowUI 的 VTK/OpenGL 页面。
            // 生产设备没有真实编号时保持在建单页，不加载扫描资源。
            if (!PrepareWorkspaceDeviceSession()) {
                WriteStatus(tr("Device preparation failed"));
                break;
            }
            if (!EnsureScanWorkflowPage()) {
                WriteStatus(tr("Scan workflow unavailable"));
                break;
            }
            m_orderWorkspace->SetStep(WorkspaceStepScan);
        }
        WriteStatus(tr("Scan step opened"));
        break;
    case OrderCreateActionClearAllTeeth:
        WriteStatus(tr("Tooth selection cleared"));
        break;
    case OrderCreateActionToothSelectionChanged:
        WriteStatus(tr("Tooth selection changed"));
        break;
    case OrderCreateActionScanProcessChanged:
        RefreshWorkspaceScanProcessFromOrder();
        WriteStatus(tr("Scan process updated"));
        break;
    default:
        WriteStatus(tr("Order create action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 处理工作台右上角按钮动作。
// Minimize 操作主窗口；Close 关闭当前工作台并返回首页，不退出整个 MeyerScan。
void MainWindow::HandleWorkspaceShellAction(int actionId) {
    WriteUserAction("WorkspaceShellAction", QString("Workspace shell action clicked: %1").arg(actionId));
    switch (actionId) {
    case WorkspaceShellActionMinimize:
        showMinimized();
        break;
    case WorkspaceShellActionClose:
        ShowHome();
        break;
    case WorkspaceShellActionBack:
        // 创建和练习工作台的返回按钮统一回到首页，子页面不直接操作 MainWindow。
        ShowHome();
        break;
    default:
        WriteStatus(tr("Workspace action %1 is not implemented yet").arg(actionId));
        break;
    }
}

// 工作台步骤变化后的资源调度。
// 进入 Scan/Process 时懒加载对应页面；离开时释放隐藏页的 VTK/OpenGL 资源。
void MainWindow::HandleWorkspaceStepChanged(int step) {
    WriteUserAction("WorkspaceStepChanged", QString("Workspace step changed: %1").arg(step));

    // Shell 的步骤按钮会先更新内部当前步骤，再同步回调 MainExe。这里在创建
    // 任何重页面前执行设备准入；失败后同一事件回调内切回 Order，Qt 尚未发生
    // 下一轮绘制，因此客户不会看到被拒绝的 Scan/Process/Send 页面闪现。
    if (step == WorkspaceStepScan ||
        step == WorkspaceStepProcess ||
        step == WorkspaceStepSend) {
        if (!PrepareWorkspaceDeviceSession()) {
            if (m_currentWorkspaceMode == WorkspaceModeOrderCreate && m_orderWorkspace) {
                m_orderWorkspace->SetStep(WorkspaceStepOrderCreate);
            } else {
                // 练习模式没有 Order 步骤。设备准入失败时延迟返回首页，避免在
                // Shell 的 clicked 信号栈内同步销毁发送者所在控件树。
                QTimer::singleShot(0, this, [this]() { ShowHome(); });
            }
            return;
        }
    }

    if (step == WorkspaceStepScan) {
        if (m_currentWorkspaceMode == WorkspaceModeOrderCreate) {
            RefreshWorkspaceScanProcessFromOrder();
        }
        // 进入扫描页时确保页面挂载，并让扫描模块恢复必要状态。
        if (!EnsureScanWorkflowPage()) {
            // 扫描页加载失败时不要继续释放当前可用页面。
            // 例如运行目录缺失扫描 DLL 时，保留原页面比把工作台切成空占位页更容易排查。
            WriteStatus(tr("Scan workflow unavailable"));
            return;
        }
        if (m_scanWorkflow) {
            m_scanWorkflow->Activate();
        }
        // 处理页不可见时释放整个处理页面，让 QVTK/OpenGL 资源真正归还。
        ReleaseDataProcessPage();
        ReleaseSendPage();
        WriteStatus(tr("Scan"));
        return;
    }

    if (step == WorkspaceStepProcess) {
        if (m_currentWorkspaceMode == WorkspaceModeOrderCreate) {
            RefreshWorkspaceScanProcessFromOrder();
        }
        // 进入处理页时确保页面挂载，并释放扫描页重资源。
        if (!EnsureDataProcessPage()) {
            // 数据处理页加载失败时同样不释放扫描页。
            // 这样用户还能回到上一步，并且日志中会保留真实失败原因。
            WriteStatus(tr("Data process unavailable"));
            return;
        }
        if (m_dataProcess) {
            m_dataProcess->Activate();
        }
        ReleaseScanWorkflowPage();
        ReleaseSendPage();
        WriteStatus(tr("Process"));
        return;
    }

    if (step == WorkspaceStepSend) {
        // 发送页是轻量 UI，但进入发送前扫描/处理重资源都要释放，给后续导出/上传留出资源。
        if (!EnsureSendPage()) {
            WriteStatus(tr("Send page unavailable"));
            return;
        }
        ReleaseScanWorkflowPage();
        ReleaseDataProcessPage();
        WriteStatus(tr("Send"));
        return;
    }

    // 回到建单或发送步骤时，扫描/处理两个重页面都不应继续占用显存。
    ReleaseScanWorkflowPage();
    ReleaseDataProcessPage();
    if (step != WorkspaceStepSend) {
        ReleaseSendPage();
    }
}

// 处理扫描页面动作。
// 当前只把流程推进/回退接起来，真实设备和算法动作仍留在扫描模块内部。
void MainWindow::HandleScanWorkflowAction(int actionId) {
    WriteUserAction("ScanWorkflowAction", QString("Scan workflow action clicked: %1").arg(actionId));
    switch (actionId) {
    case ScanWorkflowActionPrevious:
        if (m_orderWorkspace && m_currentWorkspaceMode == WorkspaceModeOrderCreate) {
            m_orderWorkspace->SetStep(WorkspaceStepOrderCreate);
        }
        break;
    case ScanWorkflowActionNext:
    case ScanWorkflowActionComplete:
        if (m_orderWorkspace) {
            // 目标页面创建成功后才能推进步骤，避免把工作台切到空占位页。
            if (EnsureDataProcessPage()) {
                m_orderWorkspace->SetStep(WorkspaceStepProcess);
            }
        }
        break;
    default:
        WriteStatus(tr("Scan action %1 recorded").arg(actionId));
        break;
    }
}

// 处理数据处理页面动作。
// 当前骨架只支持回到 Scan 或记录工具动作，发送模块后续接入后再推进到 Send。
void MainWindow::HandleDataProcessAction(int actionId) {
    WriteUserAction("DataProcessAction", QString("Data process action clicked: %1").arg(actionId));
    switch (actionId) {
    case DataProcessActionPrevious:
        if (m_orderWorkspace) {
            if (EnsureScanWorkflowPage()) {
                m_orderWorkspace->SetStep(WorkspaceStepScan);
            }
        }
        break;
    case DataProcessActionNext:
        if (m_orderWorkspace && m_currentWorkspaceMode == WorkspaceModeOrderCreate) {
            if (EnsureSendPage()) {
                m_orderWorkspace->SetStep(WorkspaceStepSend);
            }
        } else {
            WriteStatus(tr("Practice process completed"));
        }
        break;
    default:
        WriteStatus(tr("Process action %1 recorded").arg(actionId));
        break;
    }
}

// 处理发送页面动作。
// 当前初版只负责页面流转和日志记录，真实导出/压缩/上传后续接服务模块。
void MainWindow::HandleSendAction(int actionId) {
    WriteUserAction("SendAction", QString("Send action clicked: %1").arg(actionId));
    switch (actionId) {
    case SendUIActionPrevious:
        if (m_orderWorkspace) {
            EnsureDataProcessPage();
            m_orderWorkspace->SetStep(WorkspaceStepProcess);
        }
        break;
    case SendUIActionExport:
        WriteStatus(tr("Export action recorded"));
        break;
    case SendUIActionCompress:
        WriteStatus(tr("Compress action recorded"));
        break;
    case SendUIActionEmailSend:
        WriteStatus(tr("Email send action recorded"));
        break;
    case SendUIActionUpload:
        WriteStatus(tr("Upload action recorded"));
        break;
    case SendUIActionFinish:
        ShowHome();
        break;
    case SendUIActionDataFormatChanged:
        // 发送页只上报选择变化；后续保存/导出服务再读取并持久化真实格式。
        WriteStatus(tr("Data format updated"));
        break;
    default:
        WriteStatus(tr("Send action %1 recorded").arg(actionId));
        break;
    }
}
