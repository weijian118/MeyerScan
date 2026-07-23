#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        // 读取颜色校准预检使用的设备编号。长度 0xFFFF 和求和校验失败
        // 都表示生产未写号，但必须保存为两种不同状态以便追踪下位机差异。
        std::int32_t DeviceCommandService::ReadDeviceNumberForDetection(
            MeyerDeviceCmdMachineCode& machineCode,
            MeyerDeviceDetectionRecord& record)
        {
            protocol::CommandFrame response;
            CommandExchangeDiagnostics diagnostics;
            const std::int32_t result = ExecuteCommand(protocol::ReadMachineCode,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadMachineCode,
                                                       &response,
                                                       0U,
                                                       &diagnostics);
            // 实机预检开启时输出完整交换时序，直接区分“当前 D4 发送后等待”
            // 与“上一条命令未完成响应导致的发送前等待”。正式程序默认不输出。
            ReportCommandExchangeTiming(
                protocol::ReadMachineCode, protocol::UploadMachineCode, diagnostics);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                // 只有原始回包中的命令码确实是 0xD9，才能把特殊帧解释为
                // “设备编号未写入”。其它命令的异常回包仍属于通信故障。
                const bool isMachineCodeResponse =
                    diagnostics.rawResponse.size() >= 3U &&
                    diagnostics.rawResponse[2] == protocol::UploadMachineCode;

                if (diagnostics.responseReceived && isMachineCodeResponse &&
                    diagnostics.parseStatus == protocol::CommandParseStatus::UninitializedLength)
                {
                    // 部分下位机用 payload 长度 0xFFFF 明确表示设备编号参数
                    // 尚未初始化。记录独立原因后继续 C7/CE 探测，不伪造 reported 值。
                    record.deviceNumberStatus = MeyerDeviceNumberRead_UninitializedLength;
                    record.isProductionMode = 1;
                    AppendDetectionDetail(
                        record,
                        "0xD9 payload length 0xFFFF indicates an uninitialized device number");
                    m_lastError.clear();
                    return MeyerDeviceCmdResult_Ok;
                }

                if (diagnostics.responseReceived && isMachineCodeResponse &&
                    diagnostics.parseStatus == protocol::CommandParseStatus::ChecksumMismatch)
                {
                    // 旧生产流程故意用无效校验表示 13 位编号尚未写入。这里把它
                    // 记录为生产模式并继续 C7/CE 探测，而不是误报普通通信故障。
                    record.deviceNumberStatus =
                        MeyerDeviceNumberRead_ChecksumIndicatesUnprogrammed;
                    record.isProductionMode = 1;
                    AppendDetectionDetail(record,
                                          "0xD9 checksum indicates an unprogrammed device number");
                    m_lastError.clear();
                    return MeyerDeviceCmdResult_Ok;
                }

                record.deviceNumberStatus = diagnostics.responseReceived
                    ? MeyerDeviceNumberRead_FrameInvalid
                    : MeyerDeviceNumberRead_ResponseMissing;
                AppendDetectionDetail(record, m_lastError);
                return result;
            }

            if (response.payload.size() != MEYER_DEVICE_CMD_MACHINE_CODE_BYTES)
            {
                record.deviceNumberStatus = MeyerDeviceNumberRead_ValueInvalid;
                AppendDetectionDetail(record, "0xD9 device number payload length is invalid");
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device number response must contain exactly 13 bytes");
            }

            std::string deviceNumber;
            if (!DecodeFixedDecimalDigits(response.payload,
                                          MEYER_DEVICE_CMD_MACHINE_CODE_BYTES,
                                          deviceNumber) ||
                !IsValidDeviceNumber(deviceNumber))
            {
                record.deviceNumberStatus = MeyerDeviceNumberRead_ValueInvalid;
                CopyText(record.reportedDeviceNumberUtf8, DecodeMachineCode(response.payload));
                AppendDetectionDetail(record,
                                      "0xD9 device number must be 13 digits with prefix 620000");
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device number value is invalid");
            }

            std::memcpy(machineCode.rawDigits,
                        &response.payload[0],
                        MEYER_DEVICE_CMD_MACHINE_CODE_BYTES);
            CopyText(machineCode.machineCodeUtf8, deviceNumber);
            record.deviceNumberStatus = MeyerDeviceNumberRead_Valid;
            record.deviceNumberSource = MeyerDeviceIdentityValueSource_DeviceReported;
            CopyText(record.reportedDeviceNumberUtf8, deviceNumber);
            CopyText(record.effectiveDeviceNumberUtf8, deviceNumber);
            CopyText(m_state.deviceIdUtf8, deviceNumber);
            m_state.validFields |= MeyerDeviceStateField_MachineCode;
            AdvanceState();
            return MeyerDeviceCmdResult_Ok;
        }

        // 设备编号未写入时，用 C2/C7 命令能力探测系列。MyScan 6 的进一步区分
        // 尚无规则，因此当前 5/6 候选按文档暂用 MyScan 5 兼容默认值并保留来源。
        std::int32_t DeviceCommandService::ProbeProductionSeries(
            MeyerDeviceDetectionRecord& record)
        {
            protocol::CommandFrame response;
            CommandExchangeDiagnostics diagnostics;
            const std::int32_t result = ExecuteCommand(protocol::ReadCamera1Calibration,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadCamera1Calibration,
                                                       &response,
                                                       0U,
                                                       &diagnostics);
            // C2/C7 的发送前等待可证明上一条 D9 是否完成有效响应交换；
            // C7 自身成功后应清除等待标记，使后续 CD 可以立即发送。
            ReportCommandExchangeTiming(
                protocol::ReadCamera1Calibration,
                protocol::UploadCamera1Calibration,
                diagnostics);

            if (result == MeyerDeviceCmdResult_Ok || diagnostics.responseReceived)
            {
                // 能收到任意 C7 回包说明下位机具备该命令能力。即使帧本身异常，
                // 也按旧流程记录响应异常并继续由 CE 给出最终型号代码。
                record.seriesProbeStatus = MeyerDeviceSeriesProbe_MyScan5Or6;
                record.usedCompatibilityDefaults = 1;
                record.deviceNumberSource = MeyerDeviceIdentityValueSource_CompatibilityDefault;
                CopyText(record.effectiveDeviceNumberUtf8, "6200005301200");
                CopyText(record.effectiveModelCodeUtf8, "62000053");
                if (result != MeyerDeviceCmdResult_Ok)
                {
                    AppendDetectionDetail(record,
                                          "0xC7 was received but its frame is abnormal");
                }
                else
                {
                    AppendDetectionDetail(record,
                                          "0xC7 capability indicates the MyScan 5/6 family");
                }
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            if (diagnostics.requestSent && !diagnostics.responseReceived &&
                result == MeyerDeviceCmdResult_Timeout)
            {
                // 旧 mOS MyScan 不实现 C2/C7；请求超时是该流程定义的能力缺失证据。
                record.seriesProbeStatus = MeyerDeviceSeriesProbe_MyScan;
                record.usedCompatibilityDefaults = 1;
                record.deviceNumberSource = MeyerDeviceIdentityValueSource_CompatibilityDefault;
                CopyText(record.effectiveDeviceNumberUtf8, "6200002001200");
                CopyText(record.effectiveModelCodeUtf8, "62000020");
                AppendDetectionDetail(record,
                                      "0xC7 timeout indicates the legacy mOS MyScan family");
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            record.seriesProbeStatus = MeyerDeviceSeriesProbe_ResponseAbnormal;
            AppendDetectionDetail(record, m_lastError);
            return result;
        }

        // 读取型号代码。旧固件无回包、0xFFFF 未初始化和普通校验失败均保留
        // 兼容默认值，但真实字段保持为空，调用方可通过 source/status 区分。
        std::int32_t DeviceCommandService::ReadModelCodeForDetection(
            MeyerDeviceCmdDeviceInfo& info,
            MeyerDeviceDetectionRecord& record)
        {
            protocol::CommandFrame response;
            CommandExchangeDiagnostics diagnostics;
            const std::int32_t result = ExecuteCommand(protocol::ReadDeviceInfo,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadDeviceInfo,
                                                       &response,
                                                       0U,
                                                       &diagnostics);
            // CD/CE 的 preSendWaitUs 用于验证合法 C7 后不再固定等待；
            // postSendWaitUs 则单独表示当前 CD 发送后、开始接收前的等待。
            ReportCommandExchangeTiming(
                protocol::ReadDeviceInfo, protocol::UploadDeviceInfo, diagnostics);

            bool useCompatibilityDefault = false;
            if (result != MeyerDeviceCmdResult_Ok)
            {
                if (!diagnostics.requestSent)
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_FrameInvalid;
                    AppendDetectionDetail(record, m_lastError);
                    return result;
                }
                if (!diagnostics.responseReceived && result == MeyerDeviceCmdResult_Timeout)
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_FirmwareTooOld;
                    AppendDetectionDetail(record,
                                          "0xCE was not returned; device firmware is too old");
                    useCompatibilityDefault = true;
                }
                else if (diagnostics.parseStatus ==
                         protocol::CommandParseStatus::UninitializedLength)
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_Uninitialized;
                    AppendDetectionDetail(record,
                                          "0xCE reports that the model code is not initialized");
                    useCompatibilityDefault = true;
                }
                else if (diagnostics.parseStatus ==
                         protocol::CommandParseStatus::ChecksumMismatch)
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_ChecksumInvalid;
                    AppendDetectionDetail(record, "0xCE checksum is invalid");
                    useCompatibilityDefault = true;
                }
                else if (diagnostics.responseReceived)
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_FrameInvalid;
                    AppendDetectionDetail(record, "0xCE response frame is abnormal");
                    useCompatibilityDefault = true;
                }
                else
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_FrameInvalid;
                    AppendDetectionDetail(record, m_lastError);
                    return result;
                }
            }
            else if (response.payload.size() != MEYER_DEVICE_CMD_CALIBRATION_BYTES)
            {
                record.modelCodeStatus = MeyerDeviceModelCodeRead_FrameInvalid;
                AppendDetectionDetail(record, "0xCE payload length is not 382 bytes");
                useCompatibilityDefault = true;
            }
            else
            {
                const std::int32_t protocolFamily = m_profile == nullptr
                    ? MeyerDeviceProtocolFamily_Unknown
                    : m_profile->protocolFamily;
                DecodeDeviceInfo(response.payload, protocolFamily, info);

                std::string modelCode;
                if (protocolFamily == MeyerDeviceProtocolFamily_Wireless20250808)
                {
                    // 无线授权布局当前不含已确认的 8 位型号代码，继续等待无线机型规则。
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_ValueInvalid;
                    AppendDetectionDetail(record,
                                          "Wireless model-code extraction is not defined yet");
                    useCompatibilityDefault = true;
                }
                else if (!DecodeLegacyModelCode(response.payload, modelCode) ||
                         !IsValidModelCode(modelCode))
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_ValueInvalid;
                    CopyText(record.reportedModelCodeUtf8,
                             std::string(info.modelCodeUtf8));
                    AppendDetectionDetail(record,
                                          "0xCE model code must be 8 digits with prefix 62");
                    useCompatibilityDefault = true;
                }
                else
                {
                    record.modelCodeStatus = MeyerDeviceModelCodeRead_Valid;
                    record.modelCodeSource = MeyerDeviceIdentityValueSource_DeviceReported;
                    CopyText(record.reportedModelCodeUtf8, modelCode);
                    CopyText(record.effectiveModelCodeUtf8, modelCode);
                    CopyText(m_state.modelCodeUtf8, modelCode);
                    m_state.validFields |= MeyerDeviceStateField_ModelCode;
                    m_lastError.clear();
                    return MeyerDeviceCmdResult_Ok;
                }
            }

            if (useCompatibilityDefault)
            {
                if (record.effectiveModelCodeUtf8[0] == '\0')
                {
                    CopyText(record.effectiveModelCodeUtf8,
                             CompatibilityModelCodeForDeviceNumber(
                                 std::string(record.effectiveDeviceNumberUtf8)));
                }
                record.modelCodeSource = MeyerDeviceIdentityValueSource_CompatibilityDefault;
                record.usedCompatibilityDefaults = 1;
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            return result;
        }

    }
}
