#pragma once

#include <cstddef>
#include <vector>

#include "../../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        namespace imu
        {
            // 从 Skys1000 协议固定偏移读取六轴有符号值。
            bool TryDecodeSkysPacketSample(const Byte* packet, std::size_t size, std::vector<double>& sample);
            // 从 Three 型设备协议固定偏移读取六轴有符号值。
            bool TryDecodeThreePacketSample(const Byte* packet, std::size_t size, std::vector<double>& sample);
        }
    }
}
