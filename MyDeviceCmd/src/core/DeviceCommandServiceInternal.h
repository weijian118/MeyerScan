#pragma once

// =============================================================================
// 文件: DeviceCommandService.cpp
// 作用: 实现语义命令、状态快照和跨机型采集启停顺序。
// =============================================================================
#include "DeviceCommandService.h"

#include "../protocol/DeviceProtocolDefs.h"
#include "../model/DeviceProductCatalog.h"
#include "../support/ModuleLogger.h"
#include "../transport/DeviceTransportLibrary.h"
#include "../transport/SimulatedDeviceTransport.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
    // 上一条命令未得到普通合法帧或业务可识别终态时，下一条发送前保留兜底响应窗口。
    // 这不是固定命令间隔：一旦设备返回可识别响应，下一条命令即可立即发送。
    const std::chrono::milliseconds kUnresolvedResponseWaitMs(20);
    const std::chrono::milliseconds kDeviceInformationResponseDelayMs(50);

    // 只有 DeviceCmdTest 主动设置该标志时输出预检步骤耗时，正式 MainExe 默认不打印控制台调试信息。
    bool IsPreflightTimingTraceEnabled()
    {
        char value[2] = {};
        const DWORD length = ::GetEnvironmentVariableA(
            "MEYERSCAN_DEVICE_CMD_TIMING", value, static_cast<DWORD>(sizeof(value)));
        return length == 1U && value[0] == '1';
    }

    // 输出一条统一格式的时序记录。独立函数既供预检编排使用，也供版本命令的内部子步骤使用。
    void ReportPreflightTiming(
        const char* step,
        const char* purpose,
        const std::chrono::steady_clock::time_point& startedAt)
    {
        if (!IsPreflightTimingTraceEnabled())
        {
            return;
        }
        const std::int64_t elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt).count();
        std::cout << "[TIMING] step=" << step
                  << " purpose=" << purpose
                  << " elapsedMs=" << elapsedMs << std::endl;
    }

    std::uint64_t ElapsedMicroseconds(
        const std::chrono::steady_clock::time_point& startedAt)
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - startedAt).count());
    }

    void FinishCommandDiagnostics(
        meyer::devicecmd::CommandExchangeDiagnostics* diagnostics,
        std::int32_t result,
        const std::chrono::steady_clock::time_point& startedAt)
    {
        if (diagnostics != nullptr)
        {
            diagnostics->result = result;
            diagnostics->exchangeTotalUs = ElapsedMicroseconds(startedAt);
        }
    }

    void ReportCommandExchangeTiming(
        std::uint8_t requestCode,
        std::uint8_t responseCode,
        const meyer::devicecmd::CommandExchangeDiagnostics& diagnostics)
    {
        if (!IsPreflightTimingTraceEnabled())
        {
            return;
        }
        std::cout << "[COMMAND_TIMING] request=0x" << std::hex << std::uppercase
                  << static_cast<unsigned int>(requestCode)
                  << " response=0x" << static_cast<unsigned int>(responseCode)
                  << std::dec << " preSendWaitUs=" << diagnostics.preSendWaitUs
                  << " profileSettleWaitUs=" << diagnostics.profileSettleWaitUs
                  << " sendUs=" << diagnostics.sendUs
                  << " postSendWaitUs=" << diagnostics.postSendWaitUs
                  << " receiveUs=" << diagnostics.receiveUs
                  << " frameParseUs=" << diagnostics.frameParseUs
                  << " exchangeTotalUs=" << diagnostics.exchangeTotalUs
                  << " result=" << diagnostics.result << std::endl;
    }

    // 在不修改公共 POD 的前提下为测试宿主输出预检时间。时间包含对应步骤内部的 USB 超时、
    // 20 ms 响应未完成兜底等待和机型特定板间切换等待。
    class PreflightTimingReporter
    {
    public:
        PreflightTimingReporter()
            : m_enabled(IsPreflightTimingTraceEnabled()),
              m_totalStartedAt(std::chrono::steady_clock::now())
        {
        }

        ~PreflightTimingReporter()
        {
            ReportAggregate("Total", "Complete color-calibration device preflight", m_totalStartedAt);
        }

        void Report(const char* step,
                    const char* purpose,
                    const std::chrono::steady_clock::time_point& startedAt) const
        {
            if (m_enabled)
            {
                ReportPreflightTiming(step, purpose, startedAt);
            }
        }

        void ReportSkipped(const char* step, const char* reason) const
        {
            if (m_enabled)
            {
                std::cout << "[TIMING] step=" << step
                          << " status=SKIPPED reason=" << reason << std::endl;
            }
        }

        void ReportAggregate(
            const char* step,
            const char* purpose,
            const std::chrono::steady_clock::time_point& startedAt) const
        {
            if (!m_enabled)
            {
                return;
            }
            const std::int64_t elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startedAt).count();
            std::cout << "[TIMING] step=" << step
                      << " type=AGGREGATE purpose=" << purpose
                      << " elapsedMs=" << elapsedMs << std::endl;
        }

    private:
        bool m_enabled;
        std::chrono::steady_clock::time_point m_totalStartedAt;
    };

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

    // 把设备协议中的逐位数值或 ASCII 数字严格转换为固定长度十进制文本。
    // 与 DecodeMachineCode 不同，本函数遇到任意非数字立即失败，供合法性判断使用。
    bool DecodeFixedDecimalDigits(const std::vector<std::uint8_t>& payload,
                                  std::size_t expectedCount,
                                  std::string& result)
    {
        result.clear();
        if (payload.size() < expectedCount)
        {
            return false;
        }
        result.reserve(expectedCount);
        for (std::size_t index = 0U; index < expectedCount; ++index)
        {
            const std::uint8_t value = payload[index];
            if (value <= 9U)
            {
                result.push_back(static_cast<char>('0' + value));
            }
            else if (value >= static_cast<std::uint8_t>('0') &&
                     value <= static_cast<std::uint8_t>('9'))
            {
                result.push_back(static_cast<char>(value));
            }
            else
            {
                result.clear();
                return false;
            }
        }
        return true;
    }

    // 设备编号必须是 13 位且以 620000 开始；仅“长度正确”仍不能视为合法编号。
    bool IsValidDeviceNumber(const std::string& value)
    {
        return value.size() == MEYER_DEVICE_CMD_MACHINE_CODE_BYTES &&
               value.compare(0U, 6U, "620000") == 0;
    }

    // 当前设备型号代码固定 8 位，并以 62 开头。未知但格式合法的代码仍需记录，
    // 之后由产品目录决定是否已经支持该具体产品。
    bool IsValidModelCode(const std::string& value)
    {
        return value.size() == 8U && value.compare(0U, 2U, "62") == 0;
    }

    // 把诊断短句追加到固定缓冲区，多个降级步骤之间使用分号分隔。
    void AppendDetectionDetail(MeyerDeviceDetectionRecord& record, const std::string& detail)
    {
        if (detail.empty())
        {
            return;
        }
        std::string combined(record.detailUtf8);
        if (!combined.empty())
        {
            combined.append("; ");
        }
        combined.append(detail);
        CopyText(record.detailUtf8, combined);
    }

    // 已写设备编号但型号命令不可用时，优先按已知编号前缀选择同系列标准型号。
    // 无已知前缀时按旧流程回退到最早的 mOS MyScan P1。
    const char* CompatibilityModelCodeForDeviceNumber(const std::string& deviceNumber)
    {
        if (deviceNumber.compare(0U, 8U, "62000055") == 0)
        {
            return "62000055";
        }
        if (deviceNumber.compare(0U, 8U, "62000053") == 0)
        {
            return "62000053";
        }
        if (deviceNumber.compare(0U, 8U, "62000027") == 0)
        {
            return "62000027";
        }
        return "62000020";
    }

    // 按旧软件规则把若干无符号字节逐个转换成十进制文本后直接拼接。
    // 该函数专门用于 0xCE 前 8 字节机型标识，不能与 13 位机器码解码混用。
    std::string DecodeDecimalByteSequence(const std::vector<std::uint8_t>& payload,
                                          std::size_t count)
    {
        std::ostringstream stream;
        const std::size_t safeCount = (std::min)(count, payload.size());
        for (std::size_t index = 0U; index < safeCount; ++index)
        {
            stream << static_cast<unsigned int>(payload[index]);
        }
        return stream.str();
    }

    // 旧有线设备把前 8 字节分别存成 0~9 数值。部分生产工具可能使用 ASCII
    // 数字，因此两种形式都规范化成完整 8 位文本，再交给产品目录精确匹配。
    bool DecodeLegacyModelCode(const std::vector<std::uint8_t>& payload,
                               std::string& modelCode)
    {
        return DecodeFixedDecimalDigits(payload,
                                        MEYER_DEVICE_CMD_MODEL_PREFIX_BYTES,
                                        modelCode);
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
        case meyer::devicecmd::protocol::ReadProjectionBoardVersion:
            return MeyerDeviceCapability_ProjectionBoardFirmwareVersion;
        case meyer::devicecmd::protocol::ReadBattery:
            return MeyerDeviceCapability_Battery;
        case meyer::devicecmd::protocol::ReadCameraParameters:
        case meyer::devicecmd::protocol::StoreCameraParameters:
        case meyer::devicecmd::protocol::SetCameraWindowPosition:
            return MeyerDeviceCapability_CameraParameters;
        case meyer::devicecmd::protocol::ReadColorMatrix:
        case meyer::devicecmd::protocol::StoreColorMatrix:
        case meyer::devicecmd::protocol::ReadSmallScanHeadColorMatrix:
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

    // 解析 MyScan 6 Wireless 授权布局。只有该布局才允许按 encrypted、deviceId、
    // expirationCode 的偏移解释数据，旧有线设备不能调用本函数。
    void DecodeWirelessDeviceInfo(const std::vector<std::uint8_t>& payload,
                                  MeyerDeviceCmdDeviceInfo& info)
    {
        info.responseLayout = MeyerDeviceInfoLayout_WirelessSecurityInfo;
        info.encrypted = payload[0U];
        info.encryptionType = payload[1U];
        std::vector<std::uint8_t> machineCode(payload.begin() + 2U, payload.begin() + 15U);
        CopyText(info.deviceIdUtf8, DecodeMachineCode(machineCode));
        std::memcpy(info.expirationCode, &payload[15U], MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES);
        std::memcpy(info.reservedData, &payload[45U], 337U);
    }

    // 解析旧有线 382 字节布局。现阶段只确认前 8 字节是型号代码，其余字段
    // 原样保留供后续协议核对，不能套用无线授权信息偏移。
    void DecodeLegacyWiredDeviceInfo(const std::vector<std::uint8_t>& payload,
                                     MeyerDeviceCmdDeviceInfo& info)
    {
        info.responseLayout = MeyerDeviceInfoLayout_LegacyWiredModelCode;
        std::string modelCode;
        if (DecodeLegacyModelCode(payload, modelCode))
        {
            CopyText(info.modelCodeUtf8, modelCode);
            info.detectedModel =
                meyer::devicecmd::DeviceProductCatalog::ProtocolProfileForModelCode(
                    modelCode.c_str());
        }
        else
        {
            // 非标准数据仍转成十进制串写日志，但绝不参与具体产品匹配。
            CopyText(info.modelCodeUtf8,
                     DecodeDecimalByteSequence(payload, MEYER_DEVICE_CMD_MODEL_PREFIX_BYTES));
        }

        const std::size_t copySize =
            (std::min)(sizeof(info.reservedData), payload.size());
        if (copySize > 0U)
        {
            std::memcpy(info.reservedData, &payload[0], copySize);
        }
    }

    // 根据当前协议 Profile 选择唯一解析布局。Unknown 探测 Profile 对应当前
    // Cypress 有线链路；无线连接后续会由独立探测入口显式选择无线 Profile。
    void DecodeDeviceInfo(const std::vector<std::uint8_t>& payload,
                          std::int32_t protocolFamily,
                          MeyerDeviceCmdDeviceInfo& info)
    {
        const std::uint32_t structSize = info.structSize;
        const std::uint32_t schemaVersion = info.schemaVersion;
        std::memset(&info, 0, sizeof(info));
        info.structSize = structSize;
        info.schemaVersion = schemaVersion;

        if (protocolFamily == MeyerDeviceProtocolFamily_Wireless20250808)
        {
            DecodeWirelessDeviceInfo(payload, info);
        }
        else
        {
            DecodeLegacyWiredDeviceInfo(payload, info);
        }
    }

    // 把设备信息预留区转换为可搜索的 ASCII 小写文本。不可打印字节替换为空格，
    // 防止二进制数据和相邻可打印片段意外拼成一个伪造型号标记。
    std::string BuildSearchableModelText(const MeyerDeviceCmdDeviceInfo& info)
    {
        std::string text;
        text.reserve(sizeof(info.reservedData));
        for (std::size_t index = 0U; index < sizeof(info.reservedData); ++index)
        {
            const unsigned char value = info.reservedData[index];
            if (value >= 32U && value <= 126U)
            {
                text.push_back(static_cast<char>(std::tolower(value)));
            }
            else
            {
                text.push_back(' ');
            }
        }
        return text;
    }

    // 只识别设备明确上报的稳定标记，不根据设备编号前缀猜测型号。
    // 当前正式协议把 337 字节定义为预留区；实机确认新格式后只需扩展本函数。
    std::int32_t DetectModelFromDeviceInfo(const MeyerDeviceCmdDeviceInfo& info)
    {
        // 先使用旧有线协议前 8 字节得到的可靠候选；无法映射时再兼容新格式
        // 预留区中的明确 ASCII 标记。
        if (info.detectedModel != MeyerDeviceModel_Unknown)
        {
            return info.detectedModel;
        }
        const std::string text = BuildSearchableModelText(info);
        if (text.find("meyerscan_model=60") != std::string::npos ||
            text.find("myscan 6 wireless") != std::string::npos)
        {
            return MeyerDeviceModel_MyScan6Wireless;
        }
        if (text.find("meyerscan_model=50") != std::string::npos ||
            text.find("myscan 5h") != std::string::npos)
        {
            return MeyerDeviceModel_MyScan5H;
        }
        if (text.find("meyerscan_model=3") != std::string::npos ||
            text.find("myscan 3") != std::string::npos)
        {
            return MeyerDeviceModel_MyScan3;
        }
        if (text.find("meyerscan_model=5") != std::string::npos ||
            text.find("myscan 5") != std::string::npos)
        {
            return MeyerDeviceModel_MyScan5;
        }
        if (text.find("meyerscan_model=6") != std::string::npos ||
            text.find("myscan 6") != std::string::npos)
        {
            return MeyerDeviceModel_MyScan6;
        }
        return MeyerDeviceModel_Unknown;
    }

    // 预检详情使用固定 UTF-8 缓冲区，调用方复制结构后不依赖 DLL 内字符串生命周期。
    void SetPreflightDetail(MeyerDeviceCalibrationPreflight& preflight,
                            const std::string& detail)
    {
        std::memset(preflight.detailUtf8, 0, sizeof(preflight.detailUtf8));
        std::strncpy(preflight.detailUtf8, detail.c_str(), sizeof(preflight.detailUtf8) - 1U);
    }
}
