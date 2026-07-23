#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {

        // 执行颜色校准入口所需的完整设备预检。
        // 该函数只在扫描/练习工作台未持有设备时调用；工作台门禁由 MainExe 先完成。
        std::int32_t DeviceCommandService::PrepareColorCalibration(
            const MeyerDeviceCmdOpenParams& params,
            MeyerDeviceCalibrationPreflight& preflight)
        {
            PreflightTimingReporter timing;
            preflight.status = MeyerDeviceCalibrationPreflight_NotRun;
            preflight.commandResult = MeyerDeviceCmdResult_Ok;
            SetPreflightDetail(preflight, "Color calibration device preflight started");

            // MyScan 6 Wireless 使用另一套尚未开发的连接方法。调用方明确指定该
            // 型号时必须返回未支持，不能错误落入 Cypress USB 并报告已连接。
            if (params.backendType == MeyerDeviceCmdBackend_DeviceTransport &&
                params.modelHint == MeyerDeviceModel_MyScan6Wireless)
            {
                preflight.status = MeyerDeviceCalibrationPreflight_WirelessProbeUnsupported;
                SetPreflightDetail(preflight, "MyScan 6 Wireless connection probe is not implemented");
                return MeyerDeviceCmdResult_Ok;
            }

            const std::chrono::steady_clock::time_point openStartedAt =
                std::chrono::steady_clock::now();
            const std::int32_t openResult = Open(params);
            timing.Report("Open", "Enumerate and open the matching USB device", openStartedAt);
            if (openResult != MeyerDeviceCmdResult_Ok)
            {
                preflight.commandResult = openResult;
                preflight.status = openResult == MeyerDeviceCmdResult_DeviceNotFound
                    ? MeyerDeviceCalibrationPreflight_DeviceNotConnected
                    : MeyerDeviceCalibrationPreflight_InternalError;
                SetPreflightDetail(preflight, m_lastError);
                preflight.state = m_state;
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // Open 已从 DeviceTransport 取得 USB 速率。USB2 状态只需读取内存
            // 快照，不再访问设备；失败分支复制快照后立即关闭唯一会话。
            const std::chrono::steady_clock::time_point usbCheckStartedAt =
                std::chrono::steady_clock::now();
            const bool usbSpeedKnown =
                (m_state.validFields & MeyerDeviceStateField_UsbSpeed) != 0U;
            const bool usb2Connected = usbSpeedKnown && m_state.isUsb2 != 0;
            timing.Report("USB", "Read USB speed from the open-session snapshot", usbCheckStartedAt);
            if (usb2Connected)
            {
                preflight.status = MeyerDeviceCalibrationPreflight_Usb2Connected;
                preflight.state = m_state;
                SetPreflightDetail(preflight, "Device is connected through USB 2.x");
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // 连接和 USB3 检查通过后，按文档固定执行 D9 -> 必要时 C7 -> CE。
            // detectionRecord 分别保存设备真实上报值和旧流程兼容值。
            MeyerDeviceCmdMachineCode machineCode = {};
            machineCode.structSize = sizeof(MeyerDeviceCmdMachineCode);
            machineCode.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            const std::chrono::steady_clock::time_point machineStartedAt =
                std::chrono::steady_clock::now();
            const std::int32_t machineResult =
                ReadDeviceNumberForDetection(machineCode, preflight.detectionRecord);
            timing.Report("D4-D9", "Read and parse the 13-digit device number", machineStartedAt);
            if (machineResult != MeyerDeviceCmdResult_Ok)
            {
                preflight.commandResult = machineResult;
                preflight.status = preflight.detectionRecord.deviceNumberStatus ==
                    MeyerDeviceNumberRead_ValueInvalid
                    ? MeyerDeviceCalibrationPreflight_DeviceNumberInvalid
                    : MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal;
                preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                preflight.state = m_state;
                SetPreflightDetail(preflight, preflight.detectionRecord.detailUtf8);
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            if (preflight.detectionRecord.isProductionMode != 0)
            {
                const std::chrono::steady_clock::time_point probeStartedAt =
                    std::chrono::steady_clock::now();
                const std::int32_t probeResult =
                    ProbeProductionSeries(preflight.detectionRecord);
                timing.Report("C2-C7", "Probe the production-device family capability", probeStartedAt);
                if (probeResult != MeyerDeviceCmdResult_Ok)
                {
                    preflight.commandResult = probeResult;
                    preflight.status = MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal;
                    preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                    preflight.state = m_state;
                    SetPreflightDetail(preflight, preflight.detectionRecord.detailUtf8);
                    Close();
                    return MeyerDeviceCmdResult_Ok;
                }
            }
            else
            {
                preflight.detectionRecord.seriesProbeStatus =
                    MeyerDeviceSeriesProbe_NotRequired;
                timing.ReportSkipped("C2-C7", "A valid device number was already reported");
            }

            MeyerDeviceCmdDeviceInfo info = {};
            info.structSize = sizeof(MeyerDeviceCmdDeviceInfo);
            info.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            const std::chrono::steady_clock::time_point infoStartedAt =
                std::chrono::steady_clock::now();
            const std::int32_t infoResult =
                ReadModelCodeForDetection(info, preflight.detectionRecord);
            timing.Report("CD-CE", "Read and parse the product model code", infoStartedAt);
            if (infoResult != MeyerDeviceCmdResult_Ok)
            {
                preflight.commandResult = infoResult;
                preflight.status = MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal;
                preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                preflight.state = m_state;
                SetPreflightDetail(preflight, preflight.detectionRecord.detailUtf8);
                Close();
                return MeyerDeviceCmdResult_Ok;
            }
            preflight.deviceInfo = info;

            // 产品目录使用真实设备编号校验冲突；生产模式没有真实编号时传空串。
            // 型号代码使用 effective 值，但通过 source/evidence 明确是否为兼容默认。
            const std::chrono::steady_clock::time_point productStartedAt =
                std::chrono::steady_clock::now();
            std::uint64_t evidence = MeyerDeviceProductEvidence_ConnectionType |
                MeyerDeviceProductEvidence_CommandCapability;
            if (preflight.detectionRecord.seriesProbeStatus !=
                MeyerDeviceSeriesProbe_NotRequired)
            {
                evidence |= MeyerDeviceProductEvidence_CalibrationCommandProbe;
            }
            DeviceProductCatalog::Identify(
                preflight.detectionRecord.reportedDeviceNumberUtf8,
                preflight.detectionRecord.effectiveModelCodeUtf8,
                evidence,
                preflight.productIdentity);

            // 生产模式没有真实编号前缀可供 ProductCatalog 交叉校验，因此还要把
            // C7 命令能力候选与 CE 精确型号所属系列比较。两者冲突时不能静默
            // 采用任意一方，否则校准参数可能套用到错误硬件系列。
            const bool productionSeriesConflict =
                preflight.detectionRecord.modelCodeSource ==
                    MeyerDeviceIdentityValueSource_DeviceReported &&
                ((preflight.detectionRecord.seriesProbeStatus ==
                      MeyerDeviceSeriesProbe_MyScan &&
                  preflight.productIdentity.productFamily !=
                      MeyerDeviceProductFamily_MyScan) ||
                 (preflight.detectionRecord.seriesProbeStatus ==
                      MeyerDeviceSeriesProbe_MyScan5Or6 &&
                  preflight.productIdentity.productFamily !=
                      MeyerDeviceProductFamily_MyScan5 &&
                  preflight.productIdentity.productFamily !=
                      MeyerDeviceProductFamily_MyScan6));
            if (productionSeriesConflict)
            {
                preflight.productIdentity.identificationStatus =
                    MeyerDeviceProductIdentification_Conflict;
                CopyText(preflight.productIdentity.detailUtf8,
                         "0xC7 command capability conflicts with the reported model code");
            }

            if (preflight.detectionRecord.modelCodeSource ==
                MeyerDeviceIdentityValueSource_CompatibilityDefault)
            {
                // ProductCatalog 会把格式合法代码标成 ModelCode 证据，这里根据真实
                // 来源改成 CompatibilityDefault，避免日志把推断值写成设备上报。
                preflight.productIdentity.evidence &=
                    ~static_cast<std::uint64_t>(MeyerDeviceProductEvidence_ModelCode);
                preflight.productIdentity.evidence |=
                    MeyerDeviceProductEvidence_CompatibilityDefault;
                // Identify 已发现的编号/型号冲突必须保留，兼容来源不能把冲突
                // 状态覆盖掉；只有得到具体产品时才改写为“兼容推断”。
                if (preflight.productIdentity.identificationStatus !=
                        MeyerDeviceProductIdentification_Conflict &&
                    preflight.productIdentity.productModel !=
                        MeyerDeviceProductModel_Unknown)
                {
                    preflight.productIdentity.identificationStatus =
                        MeyerDeviceProductIdentification_CompatibilityInferred;
                    // 产品目录仍会根据 effective 型号代码填充一个可用型号，
                    // 但该型号不是 CE 真实回包确认的结果，详情必须明确标记推断来源。
                    CopyText(preflight.productIdentity.detailUtf8,
                             "Product model inferred from a compatibility model code; 0xCE model code was unavailable");
                }
            }

            // 无线或后续固件可能在预留区放置明确协议标记。该标记只补充系列和
            // 协议 Profile，不允许覆盖产品目录已经发现的证据冲突。
            const std::int32_t explicitProfile = DetectModelFromDeviceInfo(info);
            DeviceProductCatalog::MergeProtocolProfileHint(explicitProfile,
                                                           preflight.productIdentity);
            timing.Report("ProductCatalog",
                          "Combine device evidence and select the product identity",
                          productStartedAt);
            if (preflight.productIdentity.identificationStatus ==
                MeyerDeviceProductIdentification_Conflict)
            {
                preflight.status = MeyerDeviceCalibrationPreflight_ProductIdentityConflict;
                preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Conflict;
                preflight.state = m_state;
                SetPreflightDetail(preflight, preflight.productIdentity.detailUtf8);
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // 系列候选只说明后续应尝试哪组协议，不能代替具体产品识别。
            // 颜色校准后续可能按国内/海外/贴牌/医院版选择参数，因此必须由完整
            // 型号代码得到具体产品；设备编号未写入但型号代码精确时仍可继续。
            if (preflight.productIdentity.productModel ==
                MeyerDeviceProductModel_Unknown)
            {
                preflight.status = MeyerDeviceCalibrationPreflight_ModelUnknown;
                preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                preflight.state = m_state;
                SetPreflightDetail(preflight, preflight.productIdentity.detailUtf8);
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // 2027 年投放的重构软件只适配 mOS MyScan 5 和 mOS MyScan 6。
            // 这里仍先执行 D9/C7/CE 和 ProductCatalog，是为了保留设备编号、
            // 型号代码、证据来源及冲突诊断；确认旧 mOS MyScan 后再统一拦截。
            // 门禁必须位于 ApplyDetectedModel 和固件读取之前，避免已明确不支持的
            // 设备继续占用会话、发送 0x14/0x12 或被误认为可进入校准。
            if (preflight.productIdentity.productFamily ==
                MeyerDeviceProductFamily_MyScan)
            {
                preflight.status =
                    MeyerDeviceCalibrationPreflight_ProductFamilyUnsupported;
                preflight.detectionRecord.detectionStatus =
                    preflight.detectionRecord.isProductionMode != 0
                    ? (preflight.detectionRecord.modelCodeSource ==
                               MeyerDeviceIdentityValueSource_DeviceReported
                           ? MeyerDeviceDetection_ProductionExactModel
                           : MeyerDeviceDetection_ProductionInferred)
                    : (preflight.detectionRecord.usedCompatibilityDefaults != 0
                           ? MeyerDeviceDetection_CompatibilityInferred
                           : MeyerDeviceDetection_Exact);
                preflight.state = m_state;
                SetPreflightDetail(
                    preflight,
                    "Current software does not support this device series");
                logging::WriteWarning(
                    "PrepareColorCalibration",
                    "Rejected the detected legacy mOS MyScan product family");
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            if (preflight.detectionRecord.isProductionMode != 0)
            {
                preflight.detectionRecord.detectionStatus =
                    preflight.detectionRecord.modelCodeSource ==
                        MeyerDeviceIdentityValueSource_DeviceReported
                    ? MeyerDeviceDetection_ProductionExactModel
                    : MeyerDeviceDetection_ProductionInferred;
            }
            else
            {
                preflight.detectionRecord.detectionStatus =
                    preflight.detectionRecord.usedCompatibilityDefaults != 0
                    ? MeyerDeviceDetection_CompatibilityInferred
                    : MeyerDeviceDetection_Exact;
            }

            const std::int32_t detectedModel = preflight.productIdentity.protocolProfile;
            // 精确 CE 型号属于设备上报；兼容默认值是主机根据旧流程推断，只能
            // 标成 AutoDetected，避免其它模块把默认值当成真实设备数据。
            const std::int32_t detectedModelSource =
                preflight.detectionRecord.modelCodeSource ==
                    MeyerDeviceIdentityValueSource_DeviceReported
                ? MeyerDeviceModelSource_DeviceReported
                : MeyerDeviceModelSource_AutoDetected;
            if (!ApplyDetectedModel(detectedModel, detectedModelSource))
            {
                preflight.status = MeyerDeviceCalibrationPreflight_ModelUnknown;
                preflight.state = m_state;
                SetPreflightDetail(
                    preflight,
                    "0xCE device information has no recognized model code or explicit marker");
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // 设备身份确定后再读取下位机版本。主控板版本是所有已支持系列的
            // 必需信息；只有 mOS MyScan 还必须读取独立投图板版本。其它系列
            // 不发送 0x12，避免把“没有投图板”误判为设备故障。
            const std::chrono::steady_clock::time_point firmwareStartedAt =
                std::chrono::steady_clock::now();
            const std::int32_t firmwareResult = RefreshFirmwareVersions();
            timing.ReportAggregate("FirmwareTotal",
                                   "Aggregate of main-board and projection-board version steps",
                                   firmwareStartedAt);
            FillFirmwareVersionSnapshot(preflight.firmwareVersions, firmwareResult);
            if (firmwareResult != MeyerDeviceCmdResult_Ok)
            {
                preflight.commandResult = firmwareResult;
                preflight.status = MeyerDeviceCalibrationPreflight_FirmwareVersionReadFailed;
                preflight.state = m_state;
                preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                SetPreflightDetail(
                    preflight,
                    std::string("Firmware version read failed: ") + m_lastError);
                Close();
                return MeyerDeviceCmdResult_Ok;
            }

            // MyScan 5/6 的主控板低于 1.3 不支持小扫描头颜色校准，版本不满足
            // 时必须在发送 A3/B9 前拦截，避免把不支持的命令当成未校准。
            if (preflight.productIdentity.productFamily ==
                    MeyerDeviceProductFamily_MyScan5 ||
                preflight.productIdentity.productFamily ==
                    MeyerDeviceProductFamily_MyScan6)
            {
                const std::int32_t compatibilityResult =
                    CheckColorCalibrationFirmwareCompatibility(
                        preflight.scanHeadColorCalibration);
                if (compatibilityResult != MeyerDeviceCmdResult_Ok)
                {
                    preflight.commandResult = compatibilityResult;
                    preflight.status =
                        MeyerDeviceCalibrationPreflight_ColorCalibrationFirmwareUnsupported;
                    preflight.state = m_state;
                    preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                    SetPreflightDetail(preflight,
                                       preflight.scanHeadColorCalibration.detailUtf8);
                    Close();
                    return MeyerDeviceCmdResult_Ok;
                }

                const std::int32_t scanHeadResult =
                    ReadScanHeadColorCalibrationSnapshot(
                        preflight.scanHeadColorCalibration);
                if (scanHeadResult != MeyerDeviceCmdResult_Ok)
                {
                    preflight.commandResult = scanHeadResult;
                    preflight.status =
                        MeyerDeviceCalibrationPreflight_ScanHeadColorCalibrationReadFailed;
                    preflight.state = m_state;
                    preflight.detectionRecord.detectionStatus = MeyerDeviceDetection_Failed;
                    SetPreflightDetail(preflight,
                                       preflight.scanHeadColorCalibration.detailUtf8);
                    Close();
                    return MeyerDeviceCmdResult_Ok;
                }
            }
            else
            {
                // 旧 mOS MyScan 使用大扫描头参数覆盖小扫描头，进入校准前不需要
                // 发送 B9；保留策略和 NotRequired 状态供后续 UI/算法读取。
                preflight.scanHeadColorCalibration.policy =
                    MeyerDeviceScanHeadColorCalibrationPolicy_LargeOnlyShared;
                preflight.scanHeadColorCalibration.firmwareCompatibility =
                    MeyerDeviceColorCalibrationFirmware_NotRequired;
                preflight.scanHeadColorCalibration.largeHeadStatus =
                    MeyerDeviceScanHeadColorCalibration_NotChecked;
                preflight.scanHeadColorCalibration.smallHeadStatus =
                    MeyerDeviceScanHeadColorCalibration_NotRequired;
                CopyText(preflight.scanHeadColorCalibration.detailUtf8,
                         "mOS MyScan shares large-head color parameters with the small head");
            }

            preflight.status = MeyerDeviceCalibrationPreflight_Ready;
            preflight.state = m_state;
            SetPreflightDetail(
                preflight,
                std::string("Color calibration device preflight passed: ") +
                    preflight.productIdentity.detailUtf8 + "; " +
                    preflight.detectionRecord.detailUtf8);
            logging::WriteInfo("PrepareColorCalibration",
                               preflight.detectionRecord.detailUtf8);
            // Ready 分支保留当前 DeviceCmd/Transport 会话，颜色校准关闭后由宿主 Close。
            return MeyerDeviceCmdResult_Ok;
        }

    }
}
