// =============================================================================
// 文件: DeviceModelCatalog.cpp
// 作用: 实现多机型能力目录；机型差异只在本文件集中维护。
// =============================================================================
#include "DeviceModelCatalog.h"

#include <cstring>

namespace
{
    // 安全复制 UTF-8 常量，并始终保留字符串结尾 '\0'。
    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const char* source)
    {
        std::memset(destination, 0, Capacity);
        if (source != nullptr && Capacity > 0U)
        {
            std::strncpy(destination, source, Capacity - 1U);
        }
    }
}

namespace meyer
{
    namespace devicecmd
    {
        // 查询协议能力 Profile。MyScan 5H 与 MyScan 5 协议相近，但保留独立能力项。
        const DeviceModelProfile* DeviceModelCatalog::Find(std::int32_t model)
        {
            // 公共基础命令来自用户提供的相似协议说明；大块校准和固件能力只在
            // 已有正式协议文档的 MyScan 6 Wireless 上声明为已知能力。
            static const std::uint64_t commonCapabilities =
                MeyerDeviceCapability_MachineCode |
                MeyerDeviceCapability_FirmwareVersion |
                MeyerDeviceCapability_Battery |
                MeyerDeviceCapability_Light |
                MeyerDeviceCapability_ForceLight |
                MeyerDeviceCapability_Capture |
                // A3/A4 与 B9/BA 用于进入颜色校准前读取参数是否已写入。
                MeyerDeviceCapability_CalibrationData |
                // 用户提供的旧软件实例确认 0xCD/0xCE 也用于有线机型的
                // 设备信息读取，因此该能力属于当前多机型公共探测链路。
                MeyerDeviceCapability_DeviceSecurityInfo;

            static const std::uint64_t wirelessCapabilities =
                commonCapabilities |
                MeyerDeviceCapability_DeviceSecurityInfo |
                MeyerDeviceCapability_CameraParameters |
                MeyerDeviceCapability_Temperature |
                MeyerDeviceCapability_Exposure |
                MeyerDeviceCapability_CalibrationData |
                MeyerDeviceCapability_FirmwareUpdate;

            // 旧 mOS MyScan 设备额外拥有投图板；MyScan 5/5H/6 没有该硬件，
            // 即使固件中保留同名命令，也不能把它登记为可用能力。
            static const std::uint64_t myScanCapabilities =
                commonCapabilities |
                MeyerDeviceCapability_ProjectionBoardFirmwareVersion;

            // transportDecoderType 沿用 DeviceTransport ABI：6=Skys1000，7=Three。
            // 它只选择 B 类图像头解析方式，不作为产品型号对外展示。
            static const DeviceModelProfile profiles[] =
            {
                // Unknown 不是产品型号，而是校准入口探测阶段使用的最小配置。
                // 探测必须先允许 D4/D9 机器码和 CD/CE 设备信息两组只读命令，
                // 型号识别成功后服务再切换到真实 profile。
                { MeyerDeviceModel_Unknown,
                  MeyerDeviceProtocolFamily_LegacySimilar,
                  false,
                  6,
                  MeyerDeviceCapability_MachineCode |
                      MeyerDeviceCapability_DeviceSecurityInfo |
                      // 生产模式编号未写入时，需要用 C2/C7 是否有回包探测系列。
                      MeyerDeviceCapability_CalibrationData,
                  "Unknown",
                  "Legacy device information probe",
                  StopSequence::LightOffThenStop,
                  80U,
                  0U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) },
                { MeyerDeviceModel_MyScan3,
                  MeyerDeviceProtocolFamily_LegacySimilar,
                  false,
                  7,
                  myScanCapabilities,
                  "MyScan 3",
                  "Legacy similar protocol (pending verification)",
                  StopSequence::LightOffThenStop,
                  80U,
                  20U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) },
                { MeyerDeviceModel_MyScan5,
                  MeyerDeviceProtocolFamily_LegacySimilar,
                  false,
                  6,
                  commonCapabilities,
                  "MyScan 5",
                  "Legacy similar protocol (pending verification)",
                  StopSequence::LightOffThenStop,
                  80U,
                  0U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) },
                { MeyerDeviceModel_MyScan5H,
                  MeyerDeviceProtocolFamily_LegacySimilar,
                  false,
                  6,
                  commonCapabilities,
                  "MyScan 5H",
                  "Same protocol profile as MyScan 5 (pending verification)",
                  StopSequence::LightOffThenStop,
                  80U,
                  0U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) },
                { MeyerDeviceModel_MyScan6,
                  MeyerDeviceProtocolFamily_LegacySimilar,
                  false,
                  6,
                  commonCapabilities,
                  "MyScan 6",
                  "Legacy similar protocol (pending verification)",
                  StopSequence::LightOffThenStop,
                  80U,
                  0U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) },
                { MeyerDeviceModel_MyScan6Wireless,
                  MeyerDeviceProtocolFamily_Wireless20250808,
                  true,
                  6,
                  wirelessCapabilities,
                  "MyScan 6 Wireless",
                  "Wireless protocol 2025-08-08",
                  StopSequence::StopThenLightOff,
                  80U,
                  0U,
                  BuildDefaultCapture(MeyerDeviceWorkMode_Scan) }
            };

            // 线性查找只有五个元素，比分配 map 更直观且没有静态初始化复杂度。
            const std::size_t profileCount = sizeof(profiles) / sizeof(profiles[0]);
            for (std::size_t index = 0U; index < profileCount; ++index)
            {
                if (profiles[index].model == model)
                {
                    return &profiles[index];
                }
            }

            return nullptr;
        }

        // 把内部指针和 enum class 转换为不含 C++ 所有权的公共 POD。
        bool DeviceModelCatalog::CopyDescriptor(std::int32_t model,
                                                MeyerDeviceModelDescriptor& descriptor)
        {
            const DeviceModelProfile* profile = Find(model);
            if (profile == nullptr)
            {
                return false;
            }

            descriptor.model = profile->model;
            descriptor.protocolFamily = profile->protocolFamily;
            descriptor.protocolVerified = profile->protocolVerified ? 1 : 0;
            descriptor.transportDecoderType = profile->transportDecoderType;
            descriptor.capabilities = profile->capabilities;
            CopyText(descriptor.modelNameUtf8, profile->modelName);
            CopyText(descriptor.protocolNameUtf8, profile->protocolName);
            return true;
        }

        // 根据协议 B 类包定义生成 1024x910、6 平面、每平面 57 个 16 KiB 包的默认值。
        MeyerDeviceCmdCaptureParams DeviceModelCatalog::BuildDefaultCapture(std::int32_t workMode)
        {
            MeyerDeviceCmdCaptureParams params = {};
            params.structSize = sizeof(MeyerDeviceCmdCaptureParams);
            params.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            params.workMode = workMode;
            params.width = 1024;
            params.height = 910;
            params.imageCount = 6;
            params.packetsPerImage = 57;
            params.transferSize = 16384U;
            params.queueDepth = 16U;
            params.packetPayloadSize = 16384;
            // 1024*910 - 56*16384 = 14336，是最后一包的有效图像字节数。
            params.lastPacketValidSize = 14336;
            params.timeoutMs = 1500U;
            params.maxReadyFrames = 3U;
            params.pictureOrderMode = 1;
            params.scanHeadType = 1;
            params.ahrsEnabled = 1;
            return params;
        }
    }
}
