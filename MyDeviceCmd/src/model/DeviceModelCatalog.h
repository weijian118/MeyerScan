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
            // 创建所有相似协议机型共用的 1024x910 六平面默认采集配置。
            static MeyerDeviceCmdCaptureParams BuildDefaultCapture(std::int32_t workMode);
        };
    }
}
