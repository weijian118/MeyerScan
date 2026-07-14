// 从不同设备协议头的固定偏移读取六轴 IMU 原始值。
#include "../../stdafx.h"

#include "ImuPacketDecoder.h"

namespace
{
    // 设备使用“最高位表示符号、其余 15 位表示绝对值”，并非二进制补码。
    int DecodeSignedMagnitude16(unsigned char high, unsigned char low)
    {
        unsigned int value = (static_cast<unsigned int>(high) << 8) | static_cast<unsigned int>(low);
        if ((value & 0x8000U) != 0)
        {
            value &= 0x7FFFU;
            return -static_cast<int>(value);
        }

        return static_cast<int>(value);
    }

    // 共用解码循环：前三项按加速度比例换算，后三项按角速度比例换算。
    bool TryDecodePacketSample(
        const meyer::device::Byte* packet,
        std::size_t size,
        int startOffset,
        std::vector<double>& sample)
    {
        sample.clear();

        if (packet == nullptr || size <= static_cast<std::size_t>(startOffset + 10))
        {
            return false;
        }

        for (int offset = startOffset; offset < startOffset + 11; offset += 2)
        {
            const int decodedValue = DecodeSignedMagnitude16(packet[offset], packet[offset + 1]);
            if (offset <= startOffset + 4)
            {
                sample.push_back(static_cast<double>(decodedValue) * 0.00239);
            }
            else
            {
                sample.push_back(static_cast<double>(decodedValue) * 0.001065);
            }
        }

        return sample.size() == 6;
    }
}

namespace meyer
{
    namespace device
    {
        namespace imu
        {
            // Skys1000 的六轴数据从包偏移 20 开始。
            bool TryDecodeSkysPacketSample(const Byte* packet, std::size_t size, std::vector<double>& sample)
            {
                return TryDecodePacketSample(packet, size, 20, sample);
            }

            // Three 型设备的六轴数据从包偏移 24 开始。
            bool TryDecodeThreePacketSample(const Byte* packet, std::size_t size, std::vector<double>& sample)
            {
                return TryDecodePacketSample(packet, size, 24, sample);
            }
        }
    }
}
