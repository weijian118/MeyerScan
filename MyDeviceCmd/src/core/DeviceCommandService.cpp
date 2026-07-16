// =============================================================================
// 文件: DeviceCommandService.cpp
// 作用: 实现语义命令、状态快照和跨机型采集启停顺序。
// =============================================================================
#include "DeviceCommandService.h"

#include "../protocol/DeviceProtocolDefs.h"
#include "../support/ModuleLogger.h"
#include "../transport/DeviceTransportLibrary.h"
#include "../transport/SimulatedDeviceTransport.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
    // 将内部 UTF-8 文本安全复制到固定长度公共字段，并强制补齐字符串结尾。
    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const std::string& source)
    {
        std::memset(destination, 0, Capacity);
        std::strncpy(destination, source.c_str(), Capacity - 1U);
    }

    // 把协议机器码的逐位数值或 ASCII 数字转换为可显示设备编号。
    std::string DecodeMachineCode(const std::vector<std::uint8_t>& payload)
    {
        std::string result;
        result.reserve(payload.size() * 2U);
        for (std::size_t index = 0U; index < payload.size(); ++index)
        {
            const std::uint8_t value = payload[index];
            if (value <= 9U)
            {
                result.push_back(static_cast<char>('0' + value));
            }
            else if (value >= static_cast<std::uint8_t>('0') && value <= static_cast<std::uint8_t>('9'))
            {
                result.push_back(static_cast<char>(value));
            }
            else
            {
                // 非数字旧协议内容用两位大写十六进制保留，不能静默丢失原始信息。
                char hex[3] = {};
                std::sprintf(hex, "%02X", static_cast<unsigned int>(value));
                result.append(hex);
            }
        }
        return result;
    }

    // 期限码当前仍需后续加解密模块解释，设备层只稳定返回原始十六进制。
    std::string EncodeHex(const std::uint8_t* data, std::size_t size)
    {
        static const char digits[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(size * 2U);
        for (std::size_t index = 0U; index < size; ++index)
        {
            result.push_back(digits[(data[index] >> 4U) & 0x0FU]);
            result.push_back(digits[data[index] & 0x0FU]);
        }
        return result;
    }

    // 按协议使用大端顺序写入 16 位整数。不能直接 memcpy 结构体字段，
    // 因为 Windows x64 主机本身是小端字节序。
    void AppendBigEndian16(std::vector<std::uint8_t>& payload, std::uint16_t value)
    {
        payload.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    // 从协议 payload 读取一个大端 16 位整数，并由调用方先保证边界。
    std::uint16_t ReadBigEndian16(const std::vector<std::uint8_t>& payload, std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[offset]) << 8U) |
            static_cast<std::uint16_t>(payload[offset + 1U]));
    }

    // 把相机参数结构转换为协议要求的 16 字节连续数据。
    void EncodeCameraParameters(const MeyerDeviceCmdCameraParameters& parameters,
                                std::vector<std::uint8_t>& payload)
    {
        payload.clear();
        payload.reserve(MEYER_DEVICE_CMD_CAMERA_PARAMETERS_BYTES);
        AppendBigEndian16(payload, parameters.camera1WindowX);
        AppendBigEndian16(payload, parameters.camera1WindowY);
        AppendBigEndian16(payload, parameters.camera2WindowX);
        AppendBigEndian16(payload, parameters.camera2WindowY);
        payload.push_back(parameters.camera1Exposure);
        payload.push_back(parameters.camera1DigitalGain);
        payload.push_back(parameters.camera2Exposure);
        payload.push_back(parameters.camera2DigitalGain);
        payload.push_back(parameters.smallHeadOffsetX);
        payload.push_back(parameters.smallHeadOffsetY);
        payload.push_back(parameters.largeHeadOffsetX);
        payload.push_back(parameters.largeHeadOffsetY);
    }

    // 把协议返回的 16 字节相机参数填入稳定公共结构。
    void DecodeCameraParameters(const std::vector<std::uint8_t>& payload,
                                MeyerDeviceCmdCameraParameters& parameters)
    {
        parameters.camera1WindowX = ReadBigEndian16(payload, 0U);
        parameters.camera1WindowY = ReadBigEndian16(payload, 2U);
        parameters.camera2WindowX = ReadBigEndian16(payload, 4U);
        parameters.camera2WindowY = ReadBigEndian16(payload, 6U);
        parameters.camera1Exposure = payload[8U];
        parameters.camera1DigitalGain = payload[9U];
        parameters.camera2Exposure = payload[10U];
        parameters.camera2DigitalGain = payload[11U];
        parameters.smallHeadOffsetX = payload[12U];
        parameters.smallHeadOffsetY = payload[13U];
        parameters.largeHeadOffsetX = payload[14U];
        parameters.largeHeadOffsetY = payload[15U];
    }

    // 把窗口位置结构转换为 0xA5 所需的 8 字节数据。
    void EncodeWindowPosition(const MeyerDeviceCmdCameraWindowPosition& position,
                              std::vector<std::uint8_t>& payload)
    {
        payload.clear();
        payload.reserve(8U);
        AppendBigEndian16(payload, position.camera1X);
        AppendBigEndian16(payload, position.camera1Y);
        AppendBigEndian16(payload, position.camera2X);
        AppendBigEndian16(payload, position.camera2Y);
    }

    // 把 8 字节协议窗口位置填入公共结构。
    void DecodeWindowPosition(const std::vector<std::uint8_t>& payload,
                              MeyerDeviceCmdCameraWindowPosition& position)
    {
        position.camera1X = ReadBigEndian16(payload, 0U);
        position.camera1Y = ReadBigEndian16(payload, 2U);
        position.camera2X = ReadBigEndian16(payload, 4U);
        position.camera2Y = ReadBigEndian16(payload, 6U);
    }

    // 将协议使用的 16 字节曝光结构编码为连续字节。
    void EncodeExposureParameters(const MeyerDeviceCmdExposureParameters& parameters,
                                  std::vector<std::uint8_t>& payload)
    {
        payload.clear();
        payload.reserve(MEYER_DEVICE_CMD_EXPOSURE_SET_BYTES);
        payload.push_back(parameters.camera1WhiteExposure);
        payload.push_back(parameters.camera1WhiteLightTime);
        payload.push_back(parameters.camera1WhiteAnalogGain);
        payload.push_back(parameters.camera1WhiteDigitalGain);
        payload.push_back(parameters.camera1StripeExposure);
        payload.push_back(parameters.camera1StripeLightTime);
        payload.push_back(parameters.camera1StripeAnalogGain);
        payload.push_back(parameters.camera1StripeDigitalGain);
        payload.push_back(parameters.camera2WhiteExposure);
        payload.push_back(parameters.camera2WhiteLightTime);
        payload.push_back(parameters.camera2WhiteAnalogGain);
        payload.push_back(parameters.camera2WhiteDigitalGain);
        payload.push_back(parameters.camera2StripeExposure);
        payload.push_back(parameters.camera2StripeLightTime);
        payload.push_back(parameters.camera2StripeAnalogGain);
        payload.push_back(parameters.camera2StripeDigitalGain);
    }

    // 读取曝光响应前 16 字节与设置结构一致，第 17 字节为协议预留。
    void DecodeExposureParameters(const std::vector<std::uint8_t>& payload,
                                  MeyerDeviceCmdExposureParameters& parameters)
    {
        parameters.camera1WhiteExposure = payload[0U];
        parameters.camera1WhiteLightTime = payload[1U];
        parameters.camera1WhiteAnalogGain = payload[2U];
        parameters.camera1WhiteDigitalGain = payload[3U];
        parameters.camera1StripeExposure = payload[4U];
        parameters.camera1StripeLightTime = payload[5U];
        parameters.camera1StripeAnalogGain = payload[6U];
        parameters.camera1StripeDigitalGain = payload[7U];
        parameters.camera2WhiteExposure = payload[8U];
        parameters.camera2WhiteLightTime = payload[9U];
        parameters.camera2WhiteAnalogGain = payload[10U];
        parameters.camera2WhiteDigitalGain = payload[11U];
        parameters.camera2StripeExposure = payload[12U];
        parameters.camera2StripeLightTime = payload[13U];
        parameters.camera2StripeAnalogGain = payload[14U];
        parameters.camera2StripeDigitalGain = payload[15U];
    }

    // 统一校验状态回复。协议约定 0xFF 成功、0x00 失败。
    bool IsSuccessfulStatusPayload(const std::vector<std::uint8_t>& payload)
    {
        return payload.size() == 1U && payload[0] == 0xFFU;
    }

    // 把协议命令映射到机型能力位。所有命令最终都经过 ExecuteCommand，因而
    // 原始命令接口也不能绕过型号能力门禁。
    std::uint64_t RequiredCapabilityForCommand(std::uint8_t commandCode)
    {
        switch (commandCode)
        {
        case meyer::devicecmd::protocol::StartImageTransfer:
        case meyer::devicecmd::protocol::StopImageTransfer:
        case meyer::devicecmd::protocol::SetFrameRate:
            return MeyerDeviceCapability_Capture;
        case meyer::devicecmd::protocol::SetLight:
            return MeyerDeviceCapability_Light;
        case meyer::devicecmd::protocol::ForceLight:
            return MeyerDeviceCapability_ForceLight;
        case meyer::devicecmd::protocol::StoreMachineCode:
        case meyer::devicecmd::protocol::ReadMachineCode:
            return MeyerDeviceCapability_MachineCode;
        case meyer::devicecmd::protocol::ReadMainBoardVersion:
            return MeyerDeviceCapability_FirmwareVersion;
        case meyer::devicecmd::protocol::ReadBattery:
            return MeyerDeviceCapability_Battery;
        case meyer::devicecmd::protocol::ReadCameraParameters:
        case meyer::devicecmd::protocol::StoreCameraParameters:
        case meyer::devicecmd::protocol::SetCameraWindowPosition:
            return MeyerDeviceCapability_CameraParameters;
        case meyer::devicecmd::protocol::ReadColorMatrix:
        case meyer::devicecmd::protocol::StoreColorMatrix:
        case meyer::devicecmd::protocol::ReadCamera1Calibration:
        case meyer::devicecmd::protocol::StoreCamera1Calibration:
        case meyer::devicecmd::protocol::ReadCamera2Calibration:
        case meyer::devicecmd::protocol::StoreCamera2Calibration:
        case meyer::devicecmd::protocol::ReadColorCalibration:
        case meyer::devicecmd::protocol::StoreColorCalibration:
            return MeyerDeviceCapability_CalibrationData;
        case meyer::devicecmd::protocol::ReadTemperature:
            return MeyerDeviceCapability_Temperature;
        case meyer::devicecmd::protocol::EraseFirmware:
        case meyer::devicecmd::protocol::WriteFirmware:
            return MeyerDeviceCapability_FirmwareUpdate;
        case meyer::devicecmd::protocol::ReadDeviceInfo:
        case meyer::devicecmd::protocol::StoreDeviceInfo:
            return MeyerDeviceCapability_DeviceSecurityInfo;
        case meyer::devicecmd::protocol::SetExposure:
        case meyer::devicecmd::protocol::ReadExposure:
            return MeyerDeviceCapability_Exposure;
        default:
            return 0U;
        }
    }

    // 机器码在协议中以 13 个 0~9 数值字节保存，而不是 ASCII 字节。
    bool EncodeMachineCode(const char* machineCodeUtf8, std::vector<std::uint8_t>& payload)
    {
        if (machineCodeUtf8 == nullptr || std::strlen(machineCodeUtf8) != MEYER_DEVICE_CMD_MACHINE_CODE_BYTES)
        {
            return false;
        }
        payload.clear();
        payload.reserve(MEYER_DEVICE_CMD_MACHINE_CODE_BYTES);
        for (std::size_t index = 0U; index < MEYER_DEVICE_CMD_MACHINE_CODE_BYTES; ++index)
        {
            if (machineCodeUtf8[index] < '0' || machineCodeUtf8[index] > '9')
            {
                payload.clear();
                return false;
            }
            payload.push_back(static_cast<std::uint8_t>(machineCodeUtf8[index] - '0'));
        }
        return true;
    }

    // 将带有公共结构头的 382 字节标定数据压缩为协议 payload。
    void EncodeCameraCalibration(const MeyerDeviceCmdCameraCalibration& calibration,
                                 std::vector<std::uint8_t>& payload)
    {
        payload.resize(MEYER_DEVICE_CMD_CALIBRATION_BYTES);
        std::memcpy(&payload[0], calibration.cameraData, 187U);
        std::memcpy(&payload[187U], calibration.projectorData, 187U);
        std::memcpy(&payload[374U], calibration.timestamp, 7U);
        payload[381U] = calibration.padding;
    }

    // 将协议 382 字节标定数据复制到公共结构。
    void DecodeCameraCalibration(const std::vector<std::uint8_t>& payload,
                                 MeyerDeviceCmdCameraCalibration& calibration)
    {
        std::memcpy(calibration.cameraData, &payload[0], 187U);
        std::memcpy(calibration.projectorData, &payload[187U], 187U);
        std::memcpy(calibration.timestamp, &payload[374U], 7U);
        calibration.padding = payload[381U];
    }

    // 将设备信息结构编码为协议定义的 382 字节布局。
    bool EncodeDeviceInfo(const MeyerDeviceCmdDeviceInfo& info,
                          std::vector<std::uint8_t>& payload)
    {
        if (!EncodeMachineCode(info.deviceIdUtf8, payload))
        {
            return false;
        }
        payload.insert(payload.begin(), info.encryptionType);
        payload.insert(payload.begin(), info.encrypted);
        payload.resize(MEYER_DEVICE_CMD_CALIBRATION_BYTES, 0U);
        std::memcpy(&payload[15U], info.expirationCode, MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES);
        std::memcpy(&payload[45U], info.reservedData, 337U);
        return true;
    }

    // 将设备信息响应转换为公共结构，保留未知的预留字节。
    void DecodeDeviceInfo(const std::vector<std::uint8_t>& payload,
                          MeyerDeviceCmdDeviceInfo& info)
    {
        info.encrypted = payload[0U];
        info.encryptionType = payload[1U];
        std::vector<std::uint8_t> machineCode(payload.begin() + 2U, payload.begin() + 15U);
        CopyText(info.deviceIdUtf8, DecodeMachineCode(machineCode));
        std::memcpy(info.expirationCode, &payload[15U], MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES);
        std::memcpy(info.reservedData, &payload[45U], 337U);
    }
}

namespace meyer
{
    namespace devicecmd
    {
        // 构造阶段只初始化公共状态结构，不加载 DLL 或访问设备，便于先创建句柄再配置参数。
        DeviceCommandService::DeviceCommandService()
            : m_profile(nullptr)
        {
            std::memset(&m_lastOpenParams, 0, sizeof(m_lastOpenParams));
            std::memset(&m_state, 0, sizeof(m_state));
            m_state.structSize = sizeof(MeyerDeviceStateSnapshot);
            m_state.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
        }

        // 析构统一复用 Close，保证正常关闭和异常销毁走同一套资源释放顺序。
        DeviceCommandService::~DeviceCommandService()
        {
            Close();
        }

        // 选择后端、建立连接并初始化不会触发长命令查询的基础状态。
        std::int32_t DeviceCommandService::Open(const MeyerDeviceCmdOpenParams& params)
        {
            Close();

            m_profile = DeviceModelCatalog::Find(params.modelHint);
            if (m_profile == nullptr)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Device model is not registered in DeviceModelCatalog");
            }

            if (params.backendType == MeyerDeviceCmdBackend_DeviceTransport)
            {
                m_transport.reset(new DeviceTransportLibrary());
            }
            else if (params.backendType == MeyerDeviceCmdBackend_SimulatorForTest)
            {
                m_transport.reset(new SimulatedDeviceTransport());
            }
            else
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Device command backend type is invalid");
            }

            ResetStateForModel(*m_profile);
            const std::int32_t result = m_transport->Open(params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                m_state.connectionState = MeyerDeviceConnectionState_Faulted;
                m_state.validFields |= MeyerDeviceStateField_Connection;
                AdvanceState();
                return SetError(result, m_transport->LastError());
            }

            // 保存完整 POD 副本供 Reconnect 使用，不持有调用方缓冲区指针。
            m_lastOpenParams = params;
            m_state.connectionState = MeyerDeviceConnectionState_Open;
            m_state.validFields |= MeyerDeviceStateField_Connection;

            std::int32_t deviceCount = 0;
            if (m_transport->GetDeviceCount(deviceCount) == MeyerDeviceCmdResult_Ok)
            {
                m_state.deviceCount = deviceCount;
            }
            std::int32_t isUsb2 = 0;
            if (m_transport->GetIsUsb2(isUsb2) == MeyerDeviceCmdResult_Ok)
            {
                m_state.isUsb2 = isUsb2;
                m_state.validFields |= MeyerDeviceStateField_UsbSpeed;
            }

            AdvanceState();
            m_lastError.clear();
            logging::WriteInfo("Open", "Device command session opened");
            return MeyerDeviceCmdResult_Ok;
        }

        // Close 不下发业务命令，确保异常退出时优先回收线程和 USB 资源。
        std::int32_t DeviceCommandService::Close()
        {
            if (m_transport)
            {
                if (m_transport->IsCaptureActive())
                {
                    m_transport->StopCapture();
                }
                m_transport->Close();
                m_transport.reset();
            }

            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.captureActive = 0;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields |= MeyerDeviceStateField_Connection | MeyerDeviceStateField_Capture;
            AdvanceState();
            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        // 连接状态直接委托给当前 Transport，避免缓存值与真实 USB 状态不一致。
        bool DeviceCommandService::IsOpen() const
        {
            return m_transport && m_transport->IsOpen();
        }

        std::int32_t DeviceCommandService::Reconnect()
        {
            if (!m_transport)
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device command session is not initialized");
            }

            const std::int32_t result = m_transport->Reconnect();
            m_state.connectionState = result == MeyerDeviceCmdResult_Ok
                ? MeyerDeviceConnectionState_Open
                : MeyerDeviceConnectionState_Faulted;
            m_state.validFields |= MeyerDeviceStateField_Connection;
            AdvanceState();
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            m_lastError.clear();
            logging::WriteInfo("Reconnect", "Device command session reconnected");
            return MeyerDeviceCmdResult_Ok;
        }

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
            const std::int32_t machineResult = RefreshMachineCode();
            if (machineResult != MeyerDeviceCmdResult_Ok)
            {
                firstFailure = machineResult;
                firstFailureMessage = m_lastError;
            }

            const std::int32_t firmwareResult = RefreshFirmwareVersion();
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
                                                            std::uint32_t timeoutMs)
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
                                                       timeoutMs);
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
            DecodeDeviceInfo(payload, info);
            m_state.encrypted = info.encrypted != 0U ? 1 : 0;
            m_state.encryptionType = info.encryptionType;
            CopyText(m_state.deviceIdUtf8, std::string(info.deviceIdUtf8));
            CopyText(m_state.expirationCodeHex,
                     EncodeHex(info.expirationCode, MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES));
            m_state.validFields |= MeyerDeviceStateField_DeviceSecurityInfo |
                                   MeyerDeviceStateField_MachineCode;
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

        // 先启动底层异步队列，再通知设备开始发图，避免第一批 B 包无人接收。
        std::int32_t DeviceCommandService::StartCapture(const MeyerDeviceCmdCaptureParams& params)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_profile || (m_profile->capabilities & MeyerDeviceCapability_Capture) == 0U)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare capture capability");
            }
            if (m_transport->IsCaptureActive())
            {
                return SetError(MeyerDeviceCmdResult_Busy, "Image capture is already active");
            }

            const std::int32_t validation = ValidateCaptureParams(params);
            if (validation != MeyerDeviceCmdResult_Ok)
            {
                return validation;
            }

            std::int32_t result = m_transport->StartCapture(params, m_profile->transportDecoderType);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            result = ExecuteCommand(protocol::StartImageTransfer,
                                    nullptr,
                                    0U,
                                    MEYER_DEVICE_CMD_NO_RESPONSE,
                                    nullptr,
                                    0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                // 命令失败时立即停掉已经建立的接收线程，不能留下半启动状态。
                m_transport->StopCapture();
                return result;
            }

            m_state.captureActive = 1;
            m_state.workMode = params.workMode;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StartCapture", "Image capture command chain started");
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::StopCapture(bool turnLightOff)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_transport->IsCaptureActive())
            {
                // 停止操作设计为幂等，页面重复释放资源不会被当成业务故障。
                m_state.captureActive = 0;
                m_state.workMode = MeyerDeviceWorkMode_Idle;
                m_state.validFields |= MeyerDeviceStateField_Capture;
                AdvanceState();
                return MeyerDeviceCmdResult_Ok;
            }

            const std::int32_t commandResult = SendStopAndOptionalLightOff(turnLightOff);
            const std::int32_t transportResult = m_transport->StopCapture();

            m_state.captureActive = 0;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StopCapture", "Image capture command chain stopped");

            // 即使命令发送失败也已经释放本地采集资源；优先返回最早的命令错误。
            if (commandResult != MeyerDeviceCmdResult_Ok)
            {
                return commandResult;
            }
            if (transportResult != MeyerDeviceCmdResult_Ok)
            {
                return SetError(transportResult, m_transport->LastError());
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 非阻塞取帧，同时把 B 类包内的灯、扫描头和温度写入状态快照。
        std::int32_t DeviceCommandService::GetFrame(unsigned char* buffer,
                                                    std::size_t capacity,
                                                    std::size_t& frameBytes,
                                                    MeyerDeviceCmdFrameInfo& frameInfo)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }

            const std::int32_t result = m_transport->GetFrame(buffer, capacity, frameBytes, frameInfo);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                if (result != MeyerDeviceCmdResult_NotReady && result != MeyerDeviceCmdResult_BufferTooSmall)
                {
                    SetError(result, m_transport->LastError());
                }
                return result;
            }

            m_state.lightRequestedOn = frameInfo.ledOn;
            m_state.captureStatus = frameInfo.captureStatus;
            m_state.scanHeadType = frameInfo.scanHeadType;
            m_state.temperature0 = frameInfo.temperature0;
            m_state.temperature1 = frameInfo.temperature1;
            m_state.temperature2 = frameInfo.temperature2;
            m_state.temperature3 = frameInfo.temperature3;
            m_state.validFields |= MeyerDeviceStateField_LightRequested |
                                   MeyerDeviceStateField_FrameTelemetry;
            AdvanceState();
            return MeyerDeviceCmdResult_Ok;
        }

        const std::string& DeviceCommandService::LastError() const
        {
            return m_lastError;
        }

        // 所有 A 类命令都从这里经过，保证编码、日志、响应校验和超时行为一致。
        std::int32_t DeviceCommandService::ExecuteCommand(std::uint8_t commandCode,
                                                          const unsigned char* payload,
                                                          std::size_t payloadSize,
                                                          std::int32_t expectedResponseCode,
                                                          protocol::CommandFrame* response,
                                                          std::uint32_t timeoutMs)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            const std::uint64_t requiredCapability = RequiredCapabilityForCommand(commandCode);
            if (requiredCapability != 0U &&
                (m_profile == nullptr || (m_profile->capabilities & requiredCapability) == 0U))
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare capability for this command");
            }
            if (m_transport->IsCaptureActive() && expectedResponseCode != MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Commands with responses are not allowed while capture is active");
            }
            if (expectedResponseCode < MEYER_DEVICE_CMD_NO_RESPONSE || expectedResponseCode > 0xFF)
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Expected response code must be -1 or one byte");
            }

            std::vector<std::uint8_t> encoded;
            std::string codecError;
            if (!protocol::DeviceCommandCodec::Build(commandCode,
                                                      payload,
                                                      payloadSize,
                                                      protocol::kHostTrailerZeroCount,
                                                      encoded,
                                                      codecError))
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument, codecError);
            }

            const std::uint32_t effectiveTimeout = timeoutMs == 0U
                ? m_lastOpenParams.commandTimeoutMs
                : timeoutMs;
            std::int32_t result = m_transport->SendCommand(encoded, effectiveTimeout);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            std::ostringstream sentMessage;
            sentMessage << "Command 0x" << std::hex << std::uppercase
                        << static_cast<unsigned int>(commandCode) << " sent";
            logging::WriteInfo("CommandSent", sentMessage.str().c_str());

            if (expectedResponseCode == MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            std::vector<std::uint8_t> received;
            result = m_transport->ReceiveCommand(received,
                                                 MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES + 10U,
                                                 effectiveTimeout);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            protocol::CommandFrame decoded;
            if (!protocol::DeviceCommandCodec::Parse(&received[0], received.size(), decoded, codecError))
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError, codecError);
            }
            if (decoded.commandCode != static_cast<std::uint8_t>(expectedResponseCode))
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device returned an unexpected command response code");
            }
            if (response != nullptr)
            {
                *response = decoded;
            }

            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::RefreshMachineCode()
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

            CopyText(m_state.deviceIdUtf8, DecodeMachineCode(response.payload));
            m_state.validFields |= MeyerDeviceStateField_MachineCode;
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::RefreshFirmwareVersion()
        {
            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(protocol::ReadMainBoardVersion,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadMainBoardVersion,
                                                       &response,
                                                       0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 4U)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Firmware response must contain exactly four bytes");
            }

            const std::uint16_t revision =
                static_cast<std::uint16_t>((static_cast<std::uint16_t>(response.payload[2]) << 8U) |
                                           response.payload[3]);
            char version[32] = {};
            std::sprintf(version,
                         "%u.%u.%u",
                         static_cast<unsigned int>(response.payload[0]),
                         static_cast<unsigned int>(response.payload[1]),
                         static_cast<unsigned int>(revision));
            CopyText(m_state.firmwareVersionUtf8, std::string(version));
            m_state.validFields |= MeyerDeviceStateField_FirmwareVersion;
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
            protocol::CommandFrame response;
            const std::int32_t result = ExecuteCommand(protocol::ReadDeviceInfo,
                                                       nullptr,
                                                       0U,
                                                       protocol::UploadDeviceInfo,
                                                       &response,
                                                       0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return result;
            }
            if (response.payload.size() != 382U)
            {
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device security response must contain exactly 382 bytes");
            }

            m_state.encrypted = response.payload[0] != 0U ? 1 : 0;
            m_state.encryptionType = response.payload[1];
            const std::vector<std::uint8_t> deviceId(response.payload.begin() + 2,
                                                     response.payload.begin() + 15);
            CopyText(m_state.deviceIdUtf8, DecodeMachineCode(deviceId));
            CopyText(m_state.expirationCodeHex, EncodeHex(&response.payload[15], 30U));
            m_state.validFields |= MeyerDeviceStateField_DeviceSecurityInfo |
                                   MeyerDeviceStateField_MachineCode;
            return MeyerDeviceCmdResult_Ok;
        }

        // 复制 DeviceTransport 的关键内存保护规则，使模拟后端和真实后端行为一致。
        std::int32_t DeviceCommandService::ValidateCaptureParams(const MeyerDeviceCmdCaptureParams& params)
        {
            if (params.workMode < MeyerDeviceWorkMode_Scan ||
                params.workMode > MeyerDeviceWorkMode_CalibrationColor ||
                params.width <= 0 || params.height <= 0 || params.imageCount <= 0 ||
                params.packetsPerImage <= 0 || params.transferSize == 0U ||
                params.queueDepth == 0U || params.packetPayloadSize <= 0 ||
                params.lastPacketValidSize <= 0 ||
                params.lastPacketValidSize > params.packetPayloadSize ||
                params.timeoutMs == 0U || params.maxReadyFrames == 0U)
            {
                return SetError(
                    MeyerDeviceCmdResult_InvalidArgument,
                    "Capture parameters contain zero or out-of-range fields");
            }

            const std::uint64_t planeBytes =
                static_cast<std::uint64_t>(params.width) * static_cast<std::uint64_t>(params.height);
            const std::uint64_t bytesBeforeLast =
                static_cast<std::uint64_t>(params.packetPayloadSize) *
                static_cast<std::uint64_t>(params.packetsPerImage - 1);
            if (planeBytes <= bytesBeforeLast ||
                planeBytes - bytesBeforeLast != static_cast<std::uint64_t>(params.lastPacketValidSize) ||
                planeBytes * static_cast<std::uint64_t>(params.imageCount) > 256ULL * 1024ULL * 1024ULL)
            {
                return SetError(
                    MeyerDeviceCmdResult_InvalidArgument,
                    "Capture packet geometry does not match the image plane size");
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 封装旧代码中散落的机型 if/else；调用方只选择是否关灯。
        std::int32_t DeviceCommandService::SendStopAndOptionalLightOff(bool turnLightOff)
        {
            const auto sendStop = [this]() {
                return ExecuteCommand(protocol::StopImageTransfer,
                                      nullptr,
                                      0U,
                                      MEYER_DEVICE_CMD_NO_RESPONSE,
                                      nullptr,
                                      0U);
            };

            if (!turnLightOff)
            {
                return sendStop();
            }

            std::int32_t firstResult = MeyerDeviceCmdResult_Ok;
            if (m_profile->stopSequence == StopSequence::StopThenLightOff)
            {
                firstResult = sendStop();
                std::this_thread::sleep_for(std::chrono::milliseconds(m_profile->stopCommandDelayMs));
                const std::int32_t lightResult = SetLight(false);
                if (firstResult == MeyerDeviceCmdResult_Ok)
                {
                    firstResult = lightResult;
                }
            }
            else
            {
                firstResult = SetLight(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(m_profile->stopCommandDelayMs));
                const std::int32_t stopResult = sendStop();
                if (firstResult == MeyerDeviceCmdResult_Ok)
                {
                    firstResult = stopResult;
                }
            }
            return firstResult;
        }

        std::int32_t DeviceCommandService::SetError(std::int32_t result,
                                                    const std::string& message)
        {
            m_lastError = message.empty() ? "Unknown device command error" : message;
            logging::WriteError("DeviceCommandFailed", m_lastError.c_str());
            return result;
        }

        void DeviceCommandService::AdvanceState()
        {
            ++m_state.sequence;
        }

        // 切换型号时清除上一个设备的全部动态信息，保留公共结构头和递增序号。
        void DeviceCommandService::ResetStateForModel(const DeviceModelProfile& profile)
        {
            const std::uint64_t nextSequence = m_state.sequence + 1U;
            std::memset(&m_state, 0, sizeof(m_state));
            m_state.structSize = sizeof(MeyerDeviceStateSnapshot);
            m_state.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            m_state.sequence = nextSequence;
            m_state.model = profile.model;
            m_state.protocolFamily = profile.protocolFamily;
            // 当前协议没有可靠产品型号上报字段，因此只能标记为宿主选择的配置。
            m_state.modelSource = MeyerDeviceModelSource_HostHint;
            m_state.capabilities = profile.capabilities;
            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields = MeyerDeviceStateField_Model |
                                  MeyerDeviceStateField_Connection |
                                  MeyerDeviceStateField_Capture;
            CopyText(m_state.modelNameUtf8, std::string(profile.modelName));
        }
    }
}
