#pragma once

#include <string>
#include <cstdint>

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        struct TransportConfig
        {
            // 默认遍历全部 Cypress 设备，超时均为 1500 ms。
            TransportConfig()
                : type(TransportType::CyApiUsb)
                , preferredDeviceType(DeviceType::Unknown)
                , vendorId(0x04B4)
                , productId(0x00F1)
                , deviceIndex(0xFFFFFFFFU)
                , commandTimeoutMs(1500)
                , streamTimeoutMs(1500)
                , host("")
                , port(0)
            {
            }

            TransportType type;
            DeviceType preferredDeviceType;

            std::uint16_t vendorId;
            std::uint16_t productId;
            std::uint32_t deviceIndex;

            std::uint32_t commandTimeoutMs;
            std::uint32_t streamTimeoutMs;

            std::string host;
            std::uint16_t port;
        };
    }
}
