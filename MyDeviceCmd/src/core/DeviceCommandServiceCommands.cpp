#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        // 顺序查询多个低频状态；单项失败不会清除此前已成功读取的有效字段。
        std::int32_t DeviceCommandService::RefreshBasicState()
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (m_transport->IsCaptureActive())
            {
                // A 类响应和 B 类图像共用 Bulk IN，采集中读取命令响应可能误吃图像包。
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Basic state query is not allowed while image capture is active");
            }

            std::int32_t firstFailure = MeyerDeviceCmdResult_Ok;
            std::string firstFailureMessage;
            MeyerDeviceCmdMachineCode machineCode = {};
            machineCode.structSize = sizeof(MeyerDeviceCmdMachineCode);
            machineCode.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            const std::int32_t machineResult = ReadMachineCode(machineCode);
            if (machineResult != MeyerDeviceCmdResult_Ok)
            {
                firstFailure = machineResult;
                firstFailureMessage = m_lastError;
            }

            // 版本读取必须在当前 profile 已确定后执行；mOS MyScan 会自动追加
            // 投图板 0x12/0x13，其他系列只读取主控板 0x14/0x15。
            const std::int32_t firmwareResult = RefreshFirmwareVersions();
            if (firstFailure == MeyerDeviceCmdResult_Ok && firmwareResult != MeyerDeviceCmdResult_Ok)
            {
                firstFailure = firmwareResult;
                firstFailureMessage = m_lastError;
            }

            if ((m_profile->capabilities & MeyerDeviceCapability_Battery) != 0U)
            {
                const std::int32_t batteryResult = RefreshBattery();
                if (firstFailure == MeyerDeviceCmdResult_Ok && batteryResult != MeyerDeviceCmdResult_Ok)
                {
                    firstFailure = batteryResult;
                    firstFailureMessage = m_lastError;
                }
            }

            if ((m_profile->capabilities & MeyerDeviceCapability_DeviceSecurityInfo) != 0U)
            {
                const std::int32_t securityResult = RefreshDeviceSecurityInfo();
                if (firstFailure == MeyerDeviceCmdResult_Ok && securityResult != MeyerDeviceCmdResult_Ok)
                {
                    firstFailure = securityResult;
                    firstFailureMessage = m_lastError;
                }
            }

            AdvanceState();
            if (firstFailure == MeyerDeviceCmdResult_Ok)
            {
                m_lastError.clear();
                logging::WriteInfo("RefreshBasicState", "Device basic state refreshed");
            }
            else
            {
                // 后续成功查询可能清空 m_lastError；恢复第一次失败文本，保证错误码和诊断一致。
                m_lastError = firstFailureMessage;
            }
            return firstFailure;
        }

        // 只复制内存中的 POD 快照，不执行设备 IO，因此可以安全供 UI 高频读取。
        std::int32_t DeviceCommandService::GetStateSnapshot(MeyerDeviceStateSnapshot& snapshot) const
        {
            snapshot = m_state;
            return MeyerDeviceCmdResult_Ok;
        }

        // 普通开关灯使用 0xFF/0x00 单字节 payload；设备真实灯状态以后续 B 类帧为准。
        std::int32_t DeviceCommandService::SetLight(bool on)
        {
            if (!m_profile || (m_profile->capabilities & MeyerDeviceCapability_Light) == 0U)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare light control capability");
            }

            const unsigned char value = on ? 0xFFU : 0x00U;
            const std::int32_t result = ExecuteCommand(
                protocol::SetLight, &value, 1U, MEYER_DEVICE_CMD_NO_RESPONSE, nullptr, 0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                // 该字段表示最近成功下发的请求；真实灯状态可在 B 类帧到达后再次校正。
                m_state.lightRequestedOn = on ? 1 : 0;
                m_state.validFields |= MeyerDeviceStateField_LightRequested;
                AdvanceState();
                logging::WriteInfo("SetLight", on ? "Light on command sent" : "Light off command sent");
            }
            return result;
        }

        // 强制开灯只改变设备侧策略，是否真正亮灯仍受设备硬件状态影响。
        std::int32_t DeviceCommandService::SetForceLight(bool enabled)
        {
            if (!m_profile || (m_profile->capabilities & MeyerDeviceCapability_ForceLight) == 0U)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare force-light capability");
            }

            const unsigned char value = enabled ? 0xFFU : 0x00U;
            const std::int32_t result = ExecuteCommand(
                protocol::ForceLight, &value, 1U, MEYER_DEVICE_CMD_NO_RESPONSE, nullptr, 0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                logging::WriteInfo("SetForceLight",
                                   enabled ? "Force-light mode enabled" : "Force-light mode disabled");
            }
            return result;
        }

        // 下发控制器复位命令。设备复位后 USB 会话通常会短暂断开，调用方不能
        // 继续使用旧状态，应该在命令返回后关闭并重新建立设备会话。
        std::int32_t DeviceCommandService::ResetController()
        {
            const std::int32_t result = ExecuteCommand(protocol::ResetController,
                                                        nullptr,
                                                        0U,
                                                        MEYER_DEVICE_CMD_NO_RESPONSE,
                                                        nullptr,
                                                        0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                logging::WriteInfo("ResetController", "Controller reset command sent");
            }
            return result;
        }

        // 把可读的 13 位设备编号转换为协议数值字节，并检查设备返回的固化状态。
        std::int32_t DeviceCommandService::StoreMachineCode(const char* machineCodeUtf8)
        {
            std::vector<std::uint8_t> payload;
            if (!EncodeMachineCode(machineCodeUtf8, payload))
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Machine code must contain exactly 13 decimal digits");
            }
            return ExecuteStatusCommand(protocol::StoreMachineCode,
                                        &payload[0],
                                        payload.size(),
                                        protocol::MachineCodeStoreReply,
                                        0U,
                                        "StoreMachineCode");
        }

        // 统一读取具有固定 payload 长度的命令，避免每个命令重复实现响应码、
        // 长度和采集中互斥检查。
        std::int32_t DeviceCommandService::ReadFixedPayload(std::uint8_t commandCode,
                                                            std::uint8_t expectedResponseCode,
                                                            std::size_t expectedPayloadSize,
                                                            std::vector<std::uint8_t>& payload,
                                                            const char* operation,
                                                            std::uint32_t timeoutMs,
                                                            CommandExchangeDiagnostics* diagnostics)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (m_transport->IsCaptureActive())
            {
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Response-based command is not allowed while image capture is active");
            }

            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(commandCode,
                                                       nullptr,
                                                       0U,
                                                       expectedResponseCode,
                                                       &response,
                                                       timeoutMs,
                                                       diagnostics);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != expectedPayloadSize)
            {
                std::ostringstream message;
                message << operation << " response payload length is invalid";
                return SetError(MeyerDeviceCmdResult_ProtocolError, message.str());
            }
            payload = response.payload;
            logging::WriteInfo(operation, "Fixed-length device response received");
            return MeyerDeviceCmdResult_Ok;
        }

        // 统一执行带 0x00/0xFF 状态应答的固化命令，并把设备拒绝区分为独立错误码。
        std::int32_t DeviceCommandService::ExecuteStatusCommand(std::uint8_t commandCode,
                                                                const unsigned char* payload,
                                                                std::size_t payloadSize,
                                                                std::uint8_t expectedResponseCode,
                                                                std::uint32_t timeoutMs,
                                                                const char* operation)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (m_transport->IsCaptureActive())
            {
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Status-response command is not allowed while image capture is active");
            }

            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(commandCode,
                                                       payload,
                                                       payloadSize,
                                                       expectedResponseCode,
                                                       &response,
                                                       timeoutMs);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 1U ||
                (response.payload[0] != 0x00U && response.payload[0] != 0xFFU))
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Persistent command returned an invalid status payload");
            }
            if (!IsSuccessfulStatusPayload(response.payload))
            {
                return SetError(MeyerDeviceCmdResult_DeviceRejected,
                                "Device rejected the persistent command");
            }
            logging::WriteInfo(operation, "Persistent device command succeeded");
            return MeyerDeviceCmdResult_Ok;
        }

        // 读取相机 1/2 的窗口坐标、曝光、增益和扫描头偏移量。
        std::int32_t DeviceCommandService::ReadCameraParameters(MeyerDeviceCmdCameraParameters& parameters)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadCameraParameters,
                                                         protocol::UploadCameraParameters,
                                                         MEYER_DEVICE_CMD_CAMERA_PARAMETERS_BYTES,
                                                         payload,
                                                         "ReadCameraParameters");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                DecodeCameraParameters(payload, parameters);
            }
            return result;
        }

        // 固化相机参数前只序列化协议字段，公共结构中的版本头和 reserved 不发送。
        std::int32_t DeviceCommandService::StoreCameraParameters(const MeyerDeviceCmdCameraParameters& parameters)
        {
            std::vector<std::uint8_t> payload;
            EncodeCameraParameters(parameters, payload);
            return ExecuteStatusCommand(protocol::StoreCameraParameters,
                                        &payload[0],
                                        payload.size(),
                                        protocol::CameraParametersStoreReply,
                                        0U,
                                        "StoreCameraParameters");
        }

        // 在线修改两个相机的开窗起点，不等待设备响应。
        std::int32_t DeviceCommandService::SetCameraWindowPosition(const MeyerDeviceCmdCameraWindowPosition& position)
        {
            std::vector<std::uint8_t> payload;
            EncodeWindowPosition(position, payload);
            const std::int32_t result = ExecuteCommand(protocol::SetCameraWindowPosition,
                                                       &payload[0],
                                                       payload.size(),
                                                       MEYER_DEVICE_CMD_NO_RESPONSE,
                                                       nullptr,
                                                       0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                logging::WriteInfo("SetCameraWindowPosition", "Camera window position command sent");
            }
            return result;
        }

        // 读取 416 字节颜色校正矩阵。
        std::int32_t DeviceCommandService::ReadColorMatrix(MeyerDeviceCmdColorMatrix& matrix)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadColorMatrix,
                                                         protocol::UploadColorMatrix,
                                                         MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES,
                                                         payload,
                                                         "ReadColorMatrix");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                std::memcpy(matrix.data, &payload[0], MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES);
            }
            return result;
        }

        // 固化 416 字节颜色校正矩阵并验证 0xAE 回复。
        std::int32_t DeviceCommandService::StoreColorMatrix(const MeyerDeviceCmdColorMatrix& matrix)
        {
            return ExecuteStatusCommand(protocol::StoreColorMatrix,
                                        matrix.data,
                                        MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES,
                                        protocol::ColorMatrixStoreReply,
                                        0U,
                                        "StoreColorMatrix");
        }

        // 读取镜头、基板和扫描头三个通道的原始毫伏值。
        std::int32_t DeviceCommandService::ReadTemperature(MeyerDeviceCmdTemperature& temperature)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadTemperature,
                                                         protocol::UploadTemperature,
                                                         7U,
                                                         payload,
                                                         "ReadTemperature");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                temperature.lensMillivolts = ReadBigEndian16(payload, 0U);
                temperature.boardMillivolts = ReadBigEndian16(payload, 2U);
                temperature.scanHeadMillivolts = ReadBigEndian16(payload, 4U);
                temperature.reservedByte = payload[6U];
            }
            return result;
        }

        // 协议只接受四种帧率，先转换为协议规定的单字节十六进制值。
        std::int32_t DeviceCommandService::SetFrameRate(std::int32_t framesPerSecond)
        {
            std::uint8_t protocolValue = 0U;
            switch (framesPerSecond)
            {
            case 18: protocolValue = 0x12U; break;
            case 20: protocolValue = 0x14U; break;
            case 22: protocolValue = 0x16U; break;
            case 25: protocolValue = 0x19U; break;
            default:
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Frame rate must be 18, 20, 22 or 25");
            }

            const std::int32_t result = ExecuteCommand(protocol::SetFrameRate,
                                                       &protocolValue,
                                                       1U,
                                                       MEYER_DEVICE_CMD_NO_RESPONSE,
                                                       nullptr,
                                                       0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                logging::WriteInfo("SetFrameRate", "Frame rate command sent");
            }
            return result;
        }

        // 发送固件擦除请求并解析一条擦除进度响应。
        std::int32_t DeviceCommandService::EraseFirmware(MeyerDeviceCmdFirmwareEraseProgress& progress,
                                                          std::uint32_t timeoutMs)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::EraseFirmware,
                                                         protocol::EraseFirmwareProgress,
                                                         4U,
                                                         payload,
                                                         "EraseFirmware",
                                                         timeoutMs);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                progress.totalSectors = ReadBigEndian16(payload, 0U);
                progress.erasedSectors = ReadBigEndian16(payload, 2U);
            }
            return result;
        }

        // 编码 262 字节烧写包，检查应答中的包序和有效长度，防止静默错包。
        std::int32_t DeviceCommandService::WriteFirmwarePacket(const MeyerDeviceCmdFirmwareWritePacket& packet,
                                                               MeyerDeviceCmdFirmwareWriteProgress& progress,
                                                               std::uint32_t timeoutMs)
        {
            if (packet.totalPackets == 0U || packet.actualDataSize > MEYER_DEVICE_CMD_FIRMWARE_PACKET_BYTES)
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Firmware packet count or actual data size is invalid");
            }

            std::vector<std::uint8_t> payload;
            payload.reserve(6U + MEYER_DEVICE_CMD_FIRMWARE_PACKET_BYTES);
            AppendBigEndian16(payload, packet.totalPackets);
            AppendBigEndian16(payload, packet.packetIndex);
            AppendBigEndian16(payload, packet.actualDataSize);
            payload.insert(payload.end(), packet.data, packet.data + MEYER_DEVICE_CMD_FIRMWARE_PACKET_BYTES);

            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(protocol::WriteFirmware,
                                                       &payload[0],
                                                       payload.size(),
                                                       protocol::WriteFirmwareProgress,
                                                       &response,
                                                       timeoutMs);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 6U)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Firmware write response payload length is invalid");
            }

            progress.totalPackets = ReadBigEndian16(response.payload, 0U);
            progress.packetIndex = ReadBigEndian16(response.payload, 2U);
            progress.actualDataSize = ReadBigEndian16(response.payload, 4U);
            if (progress.totalPackets != packet.totalPackets ||
                progress.packetIndex != packet.packetIndex ||
                progress.actualDataSize != packet.actualDataSize)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Firmware write response does not match the sent packet");
            }
            logging::WriteInfo("WriteFirmwarePacket", "Firmware packet write acknowledged");
            return MeyerDeviceCmdResult_Ok;
        }

        // 读取相机 1 标定参数。
        std::int32_t DeviceCommandService::ReadCamera1Calibration(MeyerDeviceCmdCameraCalibration& calibration)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadCamera1Calibration,
                                                         protocol::UploadCamera1Calibration,
                                                         MEYER_DEVICE_CMD_CALIBRATION_BYTES,
                                                         payload,
                                                         "ReadCamera1Calibration");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                DecodeCameraCalibration(payload, calibration);
            }
            return result;
        }

        // 固化相机 1 标定参数。
        std::int32_t DeviceCommandService::StoreCamera1Calibration(const MeyerDeviceCmdCameraCalibration& calibration)
        {
            std::vector<std::uint8_t> payload;
            EncodeCameraCalibration(calibration, payload);
            return ExecuteStatusCommand(protocol::StoreCamera1Calibration,
                                        &payload[0],
                                        payload.size(),
                                        protocol::Camera1CalibrationStoreReply,
                                        0U,
                                        "StoreCamera1Calibration");
        }

        // 读取相机 2 标定参数。
        std::int32_t DeviceCommandService::ReadCamera2Calibration(MeyerDeviceCmdCameraCalibration& calibration)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadCamera2Calibration,
                                                         protocol::UploadCamera2Calibration,
                                                         MEYER_DEVICE_CMD_CALIBRATION_BYTES,
                                                         payload,
                                                         "ReadCamera2Calibration");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                DecodeCameraCalibration(payload, calibration);
            }
            return result;
        }

        // 固化相机 2 标定参数。
        std::int32_t DeviceCommandService::StoreCamera2Calibration(const MeyerDeviceCmdCameraCalibration& calibration)
        {
            std::vector<std::uint8_t> payload;
            EncodeCameraCalibration(calibration, payload);
            return ExecuteStatusCommand(protocol::StoreCamera2Calibration,
                                        &payload[0],
                                        payload.size(),
                                        protocol::Camera2CalibrationStoreReply,
                                        0U,
                                        "StoreCamera2Calibration");
        }

        // 读取 72 字节颜色标定参数。
        std::int32_t DeviceCommandService::ReadColorCalibration(MeyerDeviceCmdColorCalibration& calibration)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadColorCalibration,
                                                         protocol::UploadColorCalibration,
                                                         MEYER_DEVICE_CMD_COLOR_CALIBRATION_BYTES,
                                                         payload,
                                                         "ReadColorCalibration");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                std::memcpy(calibration.data, &payload[0], MEYER_DEVICE_CMD_COLOR_CALIBRATION_BYTES);
            }
            return result;
        }

        // 固化 72 字节颜色标定参数。
        std::int32_t DeviceCommandService::StoreColorCalibration(const MeyerDeviceCmdColorCalibration& calibration)
        {
            return ExecuteStatusCommand(protocol::StoreColorCalibration,
                                        calibration.data,
                                        MEYER_DEVICE_CMD_COLOR_CALIBRATION_BYTES,
                                        protocol::ColorCalibrationStoreReply,
                                        0U,
                                        "StoreColorCalibration");
        }

        // 读取设备授权信息，并同步更新状态快照中的设备编号和原始期限码。
        std::int32_t DeviceCommandService::ReadDeviceInfo(MeyerDeviceCmdDeviceInfo& info)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadDeviceInfo,
                                                         protocol::UploadDeviceInfo,
                                                         MEYER_DEVICE_CMD_CALIBRATION_BYTES,
                                                         payload,
                                                         "ReadDeviceInfo");
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            const std::int32_t protocolFamily = m_profile == nullptr
                ? MeyerDeviceProtocolFamily_Unknown
                : m_profile->protocolFamily;
            DecodeDeviceInfo(payload, protocolFamily, info);
            // 只有无线授权布局包含已确认的加密和期限字段；旧有线布局不能
            // 把型号代码前两位误写成 encrypted/encryptionType。
            if (info.responseLayout == MeyerDeviceInfoLayout_WirelessSecurityInfo)
            {
                m_state.encrypted = info.encrypted != 0U ? 1 : 0;
                m_state.encryptionType = info.encryptionType;
                CopyText(m_state.expirationCodeHex,
                         EncodeHex(info.expirationCode,
                                   MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES));
                m_state.validFields |= MeyerDeviceStateField_DeviceSecurityInfo;
            }
            // 机器码优先由独立 0xD4/0xD9 命令提供。只有尚未读取机器码时，
            // 才使用正式无线协议 0xCE 中的设备编号字段作为兼容回退。
            if ((m_state.validFields & MeyerDeviceStateField_MachineCode) == 0U &&
                info.responseLayout == MeyerDeviceInfoLayout_WirelessSecurityInfo &&
                info.deviceIdUtf8[0] != '\0')
            {
                CopyText(m_state.deviceIdUtf8, std::string(info.deviceIdUtf8));
                m_state.validFields |= MeyerDeviceStateField_MachineCode;
            }
            CopyText(m_state.modelCodeUtf8, std::string(info.modelCodeUtf8));
            m_state.validFields |= MeyerDeviceStateField_ModelCode;
            return MeyerDeviceCmdResult_Ok;
        }

        // 固化设备授权信息，设备编号仍以 13 位十进制字符串输入。
        std::int32_t DeviceCommandService::StoreDeviceInfo(const MeyerDeviceCmdDeviceInfo& info)
        {
            std::vector<std::uint8_t> payload;
            if (!EncodeDeviceInfo(info, payload))
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Device info contains an invalid 13-digit device number");
            }
            return ExecuteStatusCommand(protocol::StoreDeviceInfo,
                                        &payload[0],
                                        payload.size(),
                                        protocol::DeviceInfoStoreReply,
                                        0U,
                                        "StoreDeviceInfo");
        }

        // 在线下发两路相机的 16 字节曝光参数。
        std::int32_t DeviceCommandService::SetExposureParameters(const MeyerDeviceCmdExposureParameters& parameters)
        {
            std::vector<std::uint8_t> payload;
            EncodeExposureParameters(parameters, payload);
            const std::int32_t result = ExecuteCommand(protocol::SetExposure,
                                                       &payload[0],
                                                       payload.size(),
                                                       MEYER_DEVICE_CMD_NO_RESPONSE,
                                                       nullptr,
                                                       0U);
            if (result == MeyerDeviceCmdResult_Ok)
            {
                logging::WriteInfo("SetExposureParameters", "Exposure parameters command sent");
            }
            return result;
        }

        // 读取 17 字节曝光参数响应，忽略末尾协议预留字节。
        std::int32_t DeviceCommandService::ReadExposureParameters(MeyerDeviceCmdExposureParameters& parameters)
        {
            std::vector<std::uint8_t> payload;
            const std::int32_t result = ReadFixedPayload(protocol::ReadExposure,
                                                         protocol::UploadExposure,
                                                         MEYER_DEVICE_CMD_EXPOSURE_READ_BYTES,
                                                         payload,
                                                         "ReadExposureParameters");
            if (result == MeyerDeviceCmdResult_Ok)
            {
                DecodeExposureParameters(payload, parameters);
            }
            return result;
        }

        // 通用命令入口仍复用同一编解码、串行会话和错误边界。
        std::int32_t DeviceCommandService::ExecuteRawCommand(std::uint8_t commandCode,
                                                             const unsigned char* payload,
                                                             std::size_t payloadSize,
                                                             std::int32_t expectedResponseCode,
                                                             MeyerDeviceCmdRawResponse* response,
                                                             std::uint32_t timeoutMs)
        {
            if (m_transport && m_transport->IsCaptureActive() &&
                expectedResponseCode != MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Commands with responses are not allowed while capture is active");
            }

            protocol::CommandFrame decoded;
            const std::int32_t result = ExecuteCommand(commandCode,
                                                       payload,
                                                       payloadSize,
                                                       expectedResponseCode,
                                                       expectedResponseCode == MEYER_DEVICE_CMD_NO_RESPONSE ? nullptr : &decoded,
                                                       timeoutMs);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }

            if (response != nullptr && expectedResponseCode != MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                response->commandCode = decoded.commandCode;
                response->payloadSize = static_cast<std::uint32_t>(decoded.payload.size());
                std::copy(decoded.payload.begin(), decoded.payload.end(), response->payload);
            }
            return MeyerDeviceCmdResult_Ok;
        }

    }
}
