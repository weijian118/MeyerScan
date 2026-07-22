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

namespace meyer
{
    namespace devicecmd
    {
        // 构造阶段只初始化公共状态结构，不加载 DLL 或访问设备，便于先创建句柄再配置参数。
        DeviceCommandService::DeviceCommandService()
            : m_profile(nullptr), m_waitBeforeNextCommand(false)
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
            m_waitBeforeNextCommand = false;
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
            m_waitBeforeNextCommand = false;
            logging::WriteInfo("Reconnect", "Device command session reconnected");
            return MeyerDeviceCmdResult_Ok;
        }

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
                                                          std::uint32_t timeoutMs,
                                                          CommandExchangeDiagnostics* diagnostics)
        {
            const std::chrono::steady_clock::time_point exchangeStartedAt =
                std::chrono::steady_clock::now();
            // 每次调用先重置诊断，调用方不会误读上一次命令留下的回包状态。
            if (diagnostics != nullptr)
            {
                *diagnostics = CommandExchangeDiagnostics();
            }
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
            // 只有上一条请求没有完成“收到、校验并解析期望回包”的完整交换时，
            // 才在本次发送前等待 20 ms。若上一条回包已处理完成，设备已经响应，
            // 无需再人为补足固定间隔，本次命令可立即发送。
            if (m_waitBeforeNextCommand)
            {
                if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport)
                {
                    const std::chrono::steady_clock::time_point waitStartedAt =
                        std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(kUnresolvedResponseWaitMs);
                    if (diagnostics != nullptr)
                    {
                        diagnostics->preSendWaitUs = ElapsedMicroseconds(waitStartedAt);
                    }
                }

                // 等待结束后，上一条请求的兜底响应窗口已经结束。随后仅由本次
                // SendCommand 的成功结果决定是否要求下一条命令再次等待。
                m_waitBeforeNextCommand = false;
            }

            // 实机确认旧 mOS MyScan 在主控板 0x15 已返回后，仍不能立即处理
            // 紧随其后的投图板 0x12。该等待属于机型硬件板间切换时序，集中
            // 保存在 Profile，不得重新扩大为所有命令之间的固定间隔。
            if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport &&
                commandCode == protocol::ReadProjectionBoardVersion &&
                m_profile != nullptr &&
                m_profile->projectionBoardSwitchDelayMs > 0U)
            {
                const std::chrono::steady_clock::time_point settleStartedAt =
                    std::chrono::steady_clock::now();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(m_profile->projectionBoardSwitchDelayMs));
                if (diagnostics != nullptr)
                {
                    diagnostics->profileSettleWaitUs = ElapsedMicroseconds(settleStartedAt);
                }
            }

            const std::chrono::steady_clock::time_point sendStartedAt =
                std::chrono::steady_clock::now();
            std::int32_t result = m_transport->SendCommand(encoded, effectiveTimeout);
            if (diagnostics != nullptr)
            {
                diagnostics->sendUs = ElapsedMicroseconds(sendStartedAt);
            }
            if (result != MeyerDeviceCmdResult_Ok)
            {
                FinishCommandDiagnostics(diagnostics, result, exchangeStartedAt);
                return SetError(result, m_transport->LastError());
            }
            if (diagnostics != nullptr)
            {
                diagnostics->requestSent = true;
            }
            // 请求一旦成功发出，在收到并解析期望回包前，都必须认为设备仍可能响应。
                // 无响应命令也保持该状态，使下一条命令发送前补足 20 ms 响应窗口。
            m_waitBeforeNextCommand = true;

            std::ostringstream sentMessage;
            sentMessage << "Command 0x" << std::hex << std::uppercase
                        << static_cast<unsigned int>(commandCode) << " sent";
            logging::WriteInfo("CommandSent", sentMessage.str().c_str());

            if (expectedResponseCode == MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                FinishCommandDiagnostics(diagnostics, MeyerDeviceCmdResult_Ok, exchangeStartedAt);
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            // D4/D9 和 CD/CE 按实机要求在当前命令发送后等待 50 ms，再提交 Bulk IN。
            // 这是当前请求的接收时序，不是两条命令之间的固定间隔。版本命令发送后
            // 直接提交阻塞式 Bulk IN，由 ReceiveCommand 等待设备回包。
            if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport)
            {
                if (commandCode == protocol::ReadMachineCode ||
                    commandCode == protocol::ReadDeviceInfo)
                {
                    const std::chrono::steady_clock::time_point waitStartedAt =
                        std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(kDeviceInformationResponseDelayMs);
                    if (diagnostics != nullptr)
                    {
                        diagnostics->postSendWaitUs = ElapsedMicroseconds(waitStartedAt);
                    }
                }
            }

            std::vector<std::uint8_t> received;
            const std::chrono::steady_clock::time_point receiveStartedAt =
                std::chrono::steady_clock::now();
            result = m_transport->ReceiveCommand(received,
                                                 MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES + 10U,
                                                 effectiveTimeout);
            if (diagnostics != nullptr)
            {
                diagnostics->receiveUs = ElapsedMicroseconds(receiveStartedAt);
            }
            if (result != MeyerDeviceCmdResult_Ok)
            {
                FinishCommandDiagnostics(diagnostics, result, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    // 身份探测会把“无回包”解释为旧固件/命令能力证据，因此先按
                    // warning 保存诊断，最终是否失败由上层检测流程决定。
                    m_lastError = m_transport->LastError();
                    logging::WriteWarning("CommandReceiveDiagnostic", m_lastError.c_str());
                    return result;
                }
                return SetError(result, m_transport->LastError());
            }
            if (diagnostics != nullptr)
            {
                diagnostics->responseReceived = true;
                diagnostics->rawResponse = received;
            }

            protocol::CommandFrame decoded;
            const std::chrono::steady_clock::time_point parseStartedAt =
                std::chrono::steady_clock::now();
            const protocol::CommandParseStatus parseStatus =
                protocol::DeviceCommandCodec::ParseDetailed(
                    received.empty() ? nullptr : &received[0],
                    received.size(),
                    decoded,
                    codecError);
            if (diagnostics != nullptr)
            {
                diagnostics->frameParseUs = ElapsedMicroseconds(parseStartedAt);
            }
            if (diagnostics != nullptr)
            {
                diagnostics->parseStatus = parseStatus;
            }
            if (parseStatus != protocol::CommandParseStatus::Ok)
            {
                // 0xFFFF 未初始化和求和校验失败虽然不能生成普通 CommandFrame，
                // 但身份识别流程能够把它们解析成明确的设备终态。只要帧头和
                // 命令码表明它确实是当前请求的期望回包，就说明设备已经响应，
                // 下一条命令无需再为本请求保留 20 ms 响应窗口。
                const bool isRecognizedTerminalResponse =
                    received.size() >= 5U &&
                    received[0] == protocol::kHeader0 &&
                    received[1] == protocol::kHeader1 &&
                    received[2] == static_cast<std::uint8_t>(expectedResponseCode) &&
                    (parseStatus == protocol::CommandParseStatus::UninitializedLength ||
                     parseStatus == protocol::CommandParseStatus::ChecksumMismatch);
                if (isRecognizedTerminalResponse)
                {
                    m_waitBeforeNextCommand = false;
                }

                FinishCommandDiagnostics(
                    diagnostics, MeyerDeviceCmdResult_ProtocolError, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    m_lastError = codecError;
                    logging::WriteWarning("CommandParseDiagnostic", m_lastError.c_str());
                    return MeyerDeviceCmdResult_ProtocolError;
                }
                return SetError(MeyerDeviceCmdResult_ProtocolError, codecError);
            }
            if (decoded.commandCode != static_cast<std::uint8_t>(expectedResponseCode))
            {
                FinishCommandDiagnostics(
                    diagnostics, MeyerDeviceCmdResult_ProtocolError, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    diagnostics->parseStatus = protocol::CommandParseStatus::UnexpectedCommand;
                    m_lastError = "Device returned an unexpected command response code";
                    logging::WriteWarning("CommandParseDiagnostic", m_lastError.c_str());
                    return MeyerDeviceCmdResult_ProtocolError;
                }
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device returned an unexpected command response code");
            }
            if (response != nullptr)
            {
                *response = decoded;
            }

            // 已经收到、校验并解析出期望命令码，上一条请求的响应窗口在此结束。
            // 下一条命令可以立即发送，无需再等待 20 ms。
            m_waitBeforeNextCommand = false;
            FinishCommandDiagnostics(diagnostics, MeyerDeviceCmdResult_Ok, exchangeStartedAt);
            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

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
            // Unknown 是探测期最小配置，不得被标成宿主已确认型号。
            m_state.modelSource = profile.model == MeyerDeviceModel_Unknown
                ? MeyerDeviceModelSource_Unknown
                : MeyerDeviceModelSource_HostHint;
            m_state.capabilities = profile.capabilities;
            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields = MeyerDeviceStateField_Connection |
                                  MeyerDeviceStateField_Capture;
            if (profile.model != MeyerDeviceModel_Unknown)
            {
                m_state.validFields |= MeyerDeviceStateField_Model;
                CopyText(m_state.modelNameUtf8, std::string(profile.modelName));
            }
        }

        // 把设备信息解析出的型号应用到当前会话，同时保留刚刚读取的设备编号、
        // 期限和 USB 状态。与 ResetStateForModel 不同，本函数不能清空动态字段。
        bool DeviceCommandService::ApplyDetectedModel(std::int32_t model,
                                                      std::int32_t source)
        {
            const DeviceModelProfile* detectedProfile = DeviceModelCatalog::Find(model);
            if (detectedProfile == nullptr || model == MeyerDeviceModel_Unknown)
            {
                return false;
            }

            m_profile = detectedProfile;
            m_lastOpenParams.modelHint = model;
            m_state.model = model;
            m_state.protocolFamily = detectedProfile->protocolFamily;
            m_state.modelSource = source;
            m_state.capabilities = detectedProfile->capabilities;
            m_state.validFields |= MeyerDeviceStateField_Model;
            CopyText(m_state.modelNameUtf8, std::string(detectedProfile->modelName));
            AdvanceState();
            return true;
        }
    }
}
