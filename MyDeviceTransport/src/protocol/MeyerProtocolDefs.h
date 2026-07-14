#pragma once

// 本文件只描述既有设备包中的固定字节位置，不负责 USB 传输或业务流程。

#include <cstddef>

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        namespace protocol
        {
            static const Byte kImageHeader0 = 0xA5;
            static const Byte kImageHeader1 = 0xCC;
            static const Byte kImageHeader2 = 0x00;
            static const Byte kImageHeader3 = 0x00;
            static const Byte kImageHeader4 = 0x01;
            static const Byte kImageHeader5 = 0x02;
            static const Byte kImageHeader6 = 0x03;
            static const Byte kImageHeader7 = 0x04;

            static const std::size_t kImageIndexOffset = 12;
            static const std::size_t kExposure0OffsetForThree = 9;
            static const std::size_t kExposure1OffsetForThree = 10;
            static const std::size_t kExposure2OffsetForThree = 11;
            static const std::size_t kExposureTimeOffsetForSkys = 10;
            static const std::size_t kExposureGainOffsetForSkys = 11;
            static const std::size_t kLedFlagOffset = 13;
            static const std::size_t kPhotoFlagOffset = 14;
            static const std::size_t kScanHeadTypeOffset = 15;

            // 比较前 8 字节魔数，确认当前包是一个图像平面的起始包。
            inline bool IsImageHeader(const Byte* packet, std::size_t size)
            {
                return
                    packet != nullptr &&
                    size >= 8 &&
                    packet[0] == kImageHeader0 &&
                    packet[1] == kImageHeader1 &&
                    packet[2] == kImageHeader2 &&
                    packet[3] == kImageHeader3 &&
                    packet[4] == kImageHeader4 &&
                    packet[5] == kImageHeader5 &&
                    packet[6] == kImageHeader6 &&
                    packet[7] == kImageHeader7;
            }

            // 读取 LED 标志；先检查指针和偏移，避免短包越界。
            inline bool IsLedOn(const Byte* imageBuffer, std::size_t size)
            {
                return imageBuffer != nullptr &&
                    size > kLedFlagOffset &&
                    imageBuffer[kLedFlagOffset] != 0x00;
            }

            // 0xFF 表示拍照模式。
            inline bool IsPhotoMode(const Byte* imageBuffer, std::size_t size)
            {
                return imageBuffer != nullptr &&
                    size > kPhotoFlagOffset &&
                    imageBuffer[kPhotoFlagOffset] == 0xFF;
            }

            // 把协议字节映射为扫描头类型；未知值沿用历史默认类型 3。
            inline int GetScanHeadType(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kScanHeadTypeOffset)
                {
                    return 3;
                }

                if (imageBuffer[kScanHeadTypeOffset] == 0x01)
                {
                    return 1;
                }

                if (imageBuffer[kScanHeadTypeOffset] == 0x02)
                {
                    return 2;
                }

                return 3;
            }

            // 读取 Three 型设备主曝光值。
            inline int GetThreeExposurePrimary(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kExposure0OffsetForThree)
                {
                    return 0;
                }

                return static_cast<int>(imageBuffer[kExposure0OffsetForThree]);
            }

            // 读取 Three 型设备次曝光值。
            inline int GetThreeExposureSecondary(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kExposure1OffsetForThree)
                {
                    return 0;
                }

                return static_cast<int>(imageBuffer[kExposure1OffsetForThree]);
            }

            // 读取 Three 型设备模拟增益。
            inline int GetThreeExposureAnalogGain(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kExposure2OffsetForThree)
                {
                    return 0;
                }

                return static_cast<int>(imageBuffer[kExposure2OffsetForThree]);
            }

            // 读取 Skys1000 曝光时间。
            inline int GetSkysExposureTime(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kExposureTimeOffsetForSkys)
                {
                    return 0;
                }

                return static_cast<int>(imageBuffer[kExposureTimeOffsetForSkys]);
            }

            // 读取 Skys1000 曝光增益。
            inline int GetSkysExposureGain(const Byte* imageBuffer, std::size_t size)
            {
                if (imageBuffer == nullptr || size <= kExposureGainOffsetForSkys)
                {
                    return 0;
                }

                return static_cast<int>(imageBuffer[kExposureGainOffsetForSkys]);
            }
        }
    }
}
