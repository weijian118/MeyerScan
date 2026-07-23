#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        // 读取并解析 0xD4/0xD9 设备编号，同时更新可共享状态快照。
        std::int32_t DeviceCommandService::ReadMachineCode(MeyerDeviceCmdMachineCode& machineCode)
        {
            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(protocol::ReadMachineCode,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadMachineCode,
                                                       &response,
                                                       0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 13U)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Machine code response must contain exactly 13 bytes");
            }

            std::memcpy(machineCode.rawDigits,
                        &response.payload[0],
                        MEYER_DEVICE_CMD_MACHINE_CODE_BYTES);
            const std::string decoded = DecodeMachineCode(response.payload);
            CopyText(machineCode.machineCodeUtf8, decoded);
            CopyText(m_state.deviceIdUtf8, decoded);
            m_state.validFields |= MeyerDeviceStateField_MachineCode;
            AdvanceState();
            return MeyerDeviceCmdResult_Ok;
        }

        // 读取一个四字节版本回包。协议前两个字节是主/次版本，后两个字节
        // 组成大端修订号；主控板和投图板共用此解析规则，但使用不同命令码。
        std::int32_t DeviceCommandService::ReadFirmwareVersionPayload(
            std::uint8_t requestCode,
            std::uint8_t responseCode,
            char* destination,
            std::size_t destinationCapacity,
            std::uint64_t stateField,
            const char* operation)
        {
            const std::chrono::steady_clock::time_point versionStartedAt =
                std::chrono::steady_clock::now();
            std::vector<std::uint8_t> payload;
            CommandExchangeDiagnostics diagnostics;
            const std::int32_t result = ReadFixedPayload(
                requestCode,
                responseCode,
                4U,
                payload,
                operation,
                0U,
                &diagnostics);
            ReportCommandExchangeTiming(requestCode, responseCode, diagnostics);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReportPreflightTiming(
                    requestCode == protocol::ReadMainBoardVersion ? "14-15" : "12-13",
                    "Attempt the firmware-version command and preserve its failure diagnosis",
                    versionStartedAt);
                return result;
            }

            const std::chrono::steady_clock::time_point semanticParseStartedAt =
                std::chrono::steady_clock::now();

            const std::uint16_t revision =
                static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[2]) << 8U) |
                                           payload[3]);
            char version[32] = {};
            std::sprintf(version,
                         "%u.%u.%u",
                         static_cast<unsigned int>(payload[0]),
                         static_cast<unsigned int>(payload[1]),
                         static_cast<unsigned int>(revision));
            if (destination == nullptr || destinationCapacity == 0U)
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            std::memset(destination, 0, destinationCapacity);
            std::strncpy(destination, version, destinationCapacity - 1U);
            m_state.validFields |= stateField;
            if (IsPreflightTimingTraceEnabled())
            {
                std::cout << "[SEMANTIC_TIMING] step="
                          << (requestCode == protocol::ReadMainBoardVersion ? "14-15" : "12-13")
                          << " purpose=Validate four-byte payload and format the firmware version"
                          << " semanticParseUs=" << ElapsedMicroseconds(semanticParseStartedAt)
                          << std::endl;
            }
            ReportPreflightTiming(
                requestCode == protocol::ReadMainBoardVersion ? "14-15" : "12-13",
                requestCode == protocol::ReadMainBoardVersion
                    ? "Send, receive, validate and parse the main-board firmware version"
                    : "Send, receive, validate and parse the projection-board firmware version",
                versionStartedAt);
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::RefreshFirmwareVersion()
        {
            return ReadFirmwareVersionPayload(
                protocol::ReadMainBoardVersion,
                protocol::UploadMainBoardVersion,
                m_state.firmwareVersionUtf8,
                sizeof(m_state.firmwareVersionUtf8),
                MeyerDeviceStateField_FirmwareVersion,
                "ReadMainBoardFirmwareVersion");
        }

        std::int32_t DeviceCommandService::RefreshProjectionBoardFirmwareVersion()
        {
            return ReadFirmwareVersionPayload(
                protocol::ReadProjectionBoardVersion,
                protocol::UploadProjectionBoardVersion,
                m_state.projectionBoardFirmwareVersionUtf8,
                sizeof(m_state.projectionBoardFirmwareVersionUtf8),
                MeyerDeviceStateField_ProjectionBoardFirmwareVersion,
                "ReadProjectionBoardFirmwareVersion");
        }

        // 版本读取必须在型号识别之后执行。只有 mOS MyScan 使用投图板命令；
        // 其它系列即使固件保留 0x12，也不能把异常回包当成必需信息。
        std::int32_t DeviceCommandService::RefreshFirmwareVersions()
        {
            const std::int32_t mainResult = RefreshFirmwareVersion();
            if (mainResult != MeyerDeviceCmdResult_Ok)
            {
                return mainResult;
            }

            if (m_profile != nullptr &&
                (m_profile->capabilities &
                 MeyerDeviceCapability_ProjectionBoardFirmwareVersion) != 0U)
            {
                return RefreshProjectionBoardFirmwareVersion();
            }

            // 切换到没有投图板的系列时清掉旧值和旧有效位，防止页面显示上一次设备。
            std::memset(m_state.projectionBoardFirmwareVersionUtf8,
                        0,
                        sizeof(m_state.projectionBoardFirmwareVersionUtf8));
            m_state.validFields &=
                ~static_cast<std::uint64_t>(MeyerDeviceStateField_ProjectionBoardFirmwareVersion);
            return MeyerDeviceCmdResult_Ok;
        }

        // 把设备层状态整理成可跨 DLL 复制的版本快照。有效位决定“值是否可靠”，
        // lastResult 只用于在失败时给出稳定的响应/帧异常分类。
        void DeviceCommandService::FillFirmwareVersionSnapshot(
            MeyerDeviceFirmwareVersionSnapshot& snapshot,
            std::int32_t lastResult) const
        {
            std::memset(&snapshot, 0, sizeof(snapshot));
            snapshot.structSize = sizeof(snapshot);
            snapshot.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            snapshot.mainBoardStatus =
                (m_state.validFields & MeyerDeviceStateField_FirmwareVersion) != 0U
                ? MeyerDeviceFirmwareVersion_Valid
                : (lastResult == MeyerDeviceCmdResult_Timeout
                   ? MeyerDeviceFirmwareVersion_ResponseMissing
                   : MeyerDeviceFirmwareVersion_FrameInvalid);
            snapshot.projectionBoardStatus =
                (m_profile != nullptr &&
                 (m_profile->capabilities &
                  MeyerDeviceCapability_ProjectionBoardFirmwareVersion) != 0U)
                ? (((m_state.validFields &
                     MeyerDeviceStateField_ProjectionBoardFirmwareVersion) != 0U)
                   ? MeyerDeviceFirmwareVersion_Valid
                   : (lastResult == MeyerDeviceCmdResult_Timeout
                      ? MeyerDeviceFirmwareVersion_ResponseMissing
                      : MeyerDeviceFirmwareVersion_FrameInvalid))
                : MeyerDeviceFirmwareVersion_NotRequired;
            CopyText(snapshot.mainBoardVersionUtf8,
                     std::string(m_state.firmwareVersionUtf8));
            CopyText(snapshot.projectionBoardVersionUtf8,
                     std::string(m_state.projectionBoardFirmwareVersionUtf8));
            CopyText(snapshot.detailUtf8, m_lastError);
        }

        // 解析版本文本并执行截图中规定的版本门禁：1.1.x/1.2.x 以及更低版本
        // 不支持双扫描头颜色校准；无法解析时按失败处理，避免误放行未知固件。
        std::int32_t DeviceCommandService::CheckColorCalibrationFirmwareCompatibility(
            MeyerDeviceScanHeadColorCalibrationSnapshot& snapshot) const
        {
            snapshot.structSize = sizeof(snapshot);
            snapshot.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            snapshot.policy = MeyerDeviceScanHeadColorCalibrationPolicy_LargeAndSmall;
            snapshot.firmwareCompatibility =
                MeyerDeviceColorCalibrationFirmware_ParseFailed;
            snapshot.largeHeadStatus = MeyerDeviceScanHeadColorCalibration_NotChecked;
            snapshot.smallHeadStatus = MeyerDeviceScanHeadColorCalibration_NotChecked;
            snapshot.largeHeadCommandResult = MeyerDeviceCmdResult_NotReady;
            snapshot.smallHeadCommandResult = MeyerDeviceCmdResult_NotReady;

            unsigned int major = 0U;
            unsigned int minor = 0U;
            unsigned int revision = 0U;
            const int parsed = std::sscanf(m_state.firmwareVersionUtf8,
                                           "%u.%u.%u",
                                           &major,
                                           &minor,
                                           &revision);
            if (parsed != 3 || major > 255U || minor > 255U || revision > 65535U)
            {
                CopyText(snapshot.detailUtf8,
                         "Main-board firmware version cannot be parsed for dual-head color calibration");
                return MeyerDeviceCmdResult_ProtocolError;
            }

            // 1.3.0 及以上、以及未来主版本大于 1 的版本允许双扫描头校准。
            if (major < 1U || (major == 1U && minor < 3U))
            {
                snapshot.firmwareCompatibility =
                    MeyerDeviceColorCalibrationFirmware_Unsupported;
                CopyText(snapshot.detailUtf8,
                         "Main-board firmware 1.1/1.2 does not support small-head color calibration");
                return MeyerDeviceCmdResult_UnsupportedModel;
            }

            snapshot.firmwareCompatibility = MeyerDeviceColorCalibrationFirmware_Supported;
            return MeyerDeviceCmdResult_Ok;
        }

        // 对一条扫描头读取命令进行统一的“校准存在性”判断。协议层只有在
        // 响应码正确且校验和正确时才返回 CommandFrame；校验和失败则按参考
        // 旧软件语义记录为未校准，而不是把它误当作通信故障。
        std::int32_t DeviceCommandService::ReadOneScanHeadColorCalibration(
            std::uint8_t requestCode,
            std::uint8_t responseCode,
            std::int32_t& status,
            std::int32_t& commandResult)
        {
            protocol::CommandFrame response;
            CommandExchangeDiagnostics diagnostics;
            const std::int32_t result = ExecuteCommand(requestCode,
                                                       nullptr,
                                                       0U,
                                                       responseCode,
                                                       &response,
                                                       0U,
                                                       &diagnostics);
            commandResult = result;
            ReportCommandExchangeTiming(requestCode, responseCode, diagnostics);

            if (result == MeyerDeviceCmdResult_Ok)
            {
                if (response.payload.size() != MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES)
                {
                    status = MeyerDeviceScanHeadColorCalibration_PayloadInvalid;
                    return MeyerDeviceCmdResult_ProtocolError;
                }
                status = MeyerDeviceScanHeadColorCalibration_Calibrated;
                return MeyerDeviceCmdResult_Ok;
            }

            // 只有头、响应码和 0xFFFF/校验失败语义均符合当前请求时，才可把
            // 求和失败解释成“未校准”；错误帧头或错误响应码必须继续报通信异常。
            const bool recognizedChecksumFailure =
                diagnostics.responseReceived &&
                diagnostics.rawResponse.size() >= 5U &&
                diagnostics.rawResponse[0] == protocol::kHeader0 &&
                diagnostics.rawResponse[1] == protocol::kHeader1 &&
                diagnostics.rawResponse[2] == responseCode &&
                diagnostics.parseStatus == protocol::CommandParseStatus::ChecksumMismatch;
            if (recognizedChecksumFailure)
            {
                status = MeyerDeviceScanHeadColorCalibration_NotCalibrated;
                commandResult = MeyerDeviceCmdResult_Ok;
                return MeyerDeviceCmdResult_Ok;
            }

            status = result == MeyerDeviceCmdResult_Timeout
                ? MeyerDeviceScanHeadColorCalibration_ResponseMissing
                : MeyerDeviceScanHeadColorCalibration_FrameInvalid;
            return result;
        }

        // 按固定顺序读取大头 A3/A4，再读取小头 B9/BA；任何非“未校准”异常
        // 都停止预检，避免 UI 给出不准确的校准状态提示。
        std::int32_t DeviceCommandService::ReadScanHeadColorCalibrationSnapshot(
            MeyerDeviceScanHeadColorCalibrationSnapshot& snapshot)
        {
            snapshot.largeHeadStatus = MeyerDeviceScanHeadColorCalibration_NotChecked;
            snapshot.smallHeadStatus = MeyerDeviceScanHeadColorCalibration_NotChecked;
            snapshot.largeHeadCommandResult = MeyerDeviceCmdResult_NotReady;
            snapshot.smallHeadCommandResult = MeyerDeviceCmdResult_NotReady;

            const std::int32_t largeResult = ReadOneScanHeadColorCalibration(
                protocol::ReadColorMatrix,
                protocol::UploadColorMatrix,
                snapshot.largeHeadStatus,
                snapshot.largeHeadCommandResult);
            if (largeResult != MeyerDeviceCmdResult_Ok)
            {
                CopyText(snapshot.detailUtf8, "Large scan-head color calibration status read failed");
                return largeResult;
            }

            const std::int32_t smallResult = ReadOneScanHeadColorCalibration(
                protocol::ReadSmallScanHeadColorMatrix,
                protocol::UploadSmallScanHeadColorMatrix,
                snapshot.smallHeadStatus,
                snapshot.smallHeadCommandResult);
            if (smallResult != MeyerDeviceCmdResult_Ok)
            {
                CopyText(snapshot.detailUtf8, "Small scan-head color calibration status read failed");
                return smallResult;
            }

            if (snapshot.largeHeadStatus == MeyerDeviceScanHeadColorCalibration_NotCalibrated &&
                snapshot.smallHeadStatus == MeyerDeviceScanHeadColorCalibration_NotCalibrated)
            {
                CopyText(snapshot.detailUtf8, "Large and small scan heads are not color calibrated");
            }
            else if (snapshot.largeHeadStatus == MeyerDeviceScanHeadColorCalibration_NotCalibrated)
            {
                CopyText(snapshot.detailUtf8, "Large scan head is not color calibrated");
            }
            else if (snapshot.smallHeadStatus == MeyerDeviceScanHeadColorCalibration_NotCalibrated)
            {
                CopyText(snapshot.detailUtf8, "Small scan head is not color calibrated");
            }
            else
            {
                CopyText(snapshot.detailUtf8, "Large and small scan heads are color calibrated");
            }
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::RefreshBattery()
        {
            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(protocol::ReadBattery,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadBattery,
                                                       &response,
                                                       0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 3U || response.payload[1] > 100U || response.payload[2] > 100U)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Battery response has an invalid length or percentage");
            }

            m_state.batteryConnected = response.payload[0] == 0xFFU ? 1 : 0;
            m_state.batteryLevel = response.payload[1];
            m_state.batteryHealth = response.payload[2];
            m_state.validFields |= MeyerDeviceStateField_Battery;
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::RefreshDeviceSecurityInfo()
        {
            MeyerDeviceCmdDeviceInfo info = {};
            info.structSize = sizeof(MeyerDeviceCmdDeviceInfo);
            info.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            return ReadDeviceInfo(info);
        }
    }
}
