// =============================================================================
// 文件: DeviceModelCatalog.h
// 作用: 把产品型号、协议族、能力和默认采集参数集中在一处维护。
// =============================================================================
#pragma once

#include "../../include/DeviceCmd.h"

#include <cstdint>

namespace meyer
{
    namespace devicecmd
    {
        enum class StopSequence
        {
            StopThenLightOff,
            LightOffThenStop
        };

        struct DeviceModelProfile
        {
            std::int32_t model;
            std::int32_t protocolFamily;
            bool protocolVerified;
            std::int32_t transportDecoderType;
            std::uint64_t capabilities;
            const char* modelName;
            const char* protocolName;
            StopSequence stopSequence;
            std::uint32_t stopCommandDelayMs;
            // 主控板版本回包后切换到投图板命令所需的机型特定稳定时间。
            // 仅旧 mOS MyScan 实机确认需要，其它无投图板机型保持 0。
            std::uint32_t projectionBoardSwitchDelayMs;
            MeyerDeviceCmdCaptureParams defaultCapture;
        };

        class DeviceModelCatalog
        {
        public:
            // 返回模块静态持有的只读型号配置；未知型号返回 nullptr。
            static const DeviceModelProfile* Find(std::int32_t model);

            // 将内部配置复制到稳定公共结构。
            static bool CopyDescriptor(std::int32_t model, MeyerDeviceModelDescriptor& descriptor);

        private:
            // 创建一份 MyScan 5/5H/6 当前共值的 1024x455 六图采集配置。
            // 每个 Profile 按值保存自己的副本，后续修改 MyScan 6 不会改动 MyScan 5。
            static MeyerDeviceCmdCaptureParams BuildDefaultCapture(std::int32_t workMode);
        };
    }
}
