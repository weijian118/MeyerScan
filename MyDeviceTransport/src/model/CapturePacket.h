#pragma once

#include <cstddef>
#include <vector>

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        struct CapturePacket
        {
            // 新包默认没有序号，也不视为图像头。
            CapturePacket()
                : imageIndex(-1)
                , packetIndex(-1)
                , isImageHeader(false)
            {
            }

            // 清空字节和解析状态，允许采集循环复用对象。
            void Clear()
            {
                bytes.clear();
                imageIndex = -1;
                packetIndex = -1;
                isImageHeader = false;
            }

            // 返回当前有效字节数。
            std::size_t Size() const
            {
                return bytes.size();
            }

            // 判断是否尚未接收到字节。
            bool Empty() const
            {
                return bytes.empty();
            }

            // const 版本返回首字节，空 vector 返回 nullptr，避免 &bytes[0] 越界。
            const Byte* Data() const
            {
                return bytes.empty() ? nullptr : &bytes[0];
            }

            // 可写版本返回首字节。
            Byte* Data()
            {
                return bytes.empty() ? nullptr : &bytes[0];
            }

            std::vector<Byte> bytes;
            int imageIndex;
            int packetIndex;
            bool isImageHeader;
        };
    }
}
