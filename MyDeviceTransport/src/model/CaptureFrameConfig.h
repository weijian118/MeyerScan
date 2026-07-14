#pragma once

#include <cstddef>
#include <cstdint>

#include "../../include/DeviceTransport.h"
#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        struct CaptureFrameConfig
        {
            // 初始化全部字段，防止未赋值的尺寸参与内存大小计算。
            CaptureFrameConfig()
                : width(0)
                , height(0)
                , imageCount(0)
                , packetsPerImage(0)
                , transferSize(0)
                , queueDepth(0)
                , packetPayloadSize(0)
                , lastPacketValidSize(0)
                , timeoutMs(1500)
                , maxReadyFrames(3)
                , deviceType(DeviceType::Unknown)
                , pictureOrderMode(PictureOrderMode::Old)
                , scanMode(CaptureScanMode::Scan)
                , ahrsEnabled(false)
                , gyroscopePauseFlag(false)
                , resetImuReferenceRequested(false)
            {
            }

            // 防御性校验内部配置。公开 API 已经做过更详细的错误说明，这里再次
            // 检查是为了防止后续新增的内部调用绕过 DLL 边界后造成巨量分配。
            bool IsValid() const
            {
                if (width <= 0 || height <= 0 ||
                    width > static_cast<int>(MEYER_DEVICE_TRANSPORT_MAX_DIMENSION) ||
                    height > static_cast<int>(MEYER_DEVICE_TRANSPORT_MAX_DIMENSION) ||
                    imageCount <= 0 ||
                    imageCount > static_cast<int>(MEYER_DEVICE_TRANSPORT_MAX_IMAGE_COUNT) ||
                    packetsPerImage <= 0 ||
                    packetsPerImage > static_cast<int>(MEYER_DEVICE_TRANSPORT_MAX_PACKETS_PER_IMAGE) ||
                    transferSize == 0U ||
                    transferSize > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                    queueDepth == 0U ||
                    queueDepth > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH) ||
                    packetPayloadSize <= 0 ||
                    packetPayloadSize > static_cast<int>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                    lastPacketValidSize <= 0 ||
                    lastPacketValidSize > packetPayloadSize ||
                    timeoutMs == 0U || timeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS ||
                    maxReadyFrames == 0U ||
                    maxReadyFrames > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_READY_FRAMES))
                {
                    return false;
                }

                // 在上面的单项上限约束下，以下 uint64 乘法不会溢出。
                const std::uint64_t planeBytes =
                    static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
                const std::uint64_t frameBytes =
                    planeBytes * static_cast<std::uint64_t>(imageCount);
                const std::uint64_t packetBytesBeforeLast =
                    static_cast<std::uint64_t>(packetPayloadSize) *
                    static_cast<std::uint64_t>(packetsPerImage - 1);
                const std::uint64_t expectedLastPacketBytes = planeBytes - packetBytesBeforeLast;
                const std::uint64_t inFlightBytes =
                    static_cast<std::uint64_t>(transferSize) *
                    static_cast<std::uint64_t>(queueDepth);
                // 除 ready 队列外，为组帧工作区、后处理临时帧、最近帧和 ABI
                // pending 帧保留保守余量，避免只按队列数量低估峰值内存。
                const std::uint64_t retainedFrameCopies =
                    static_cast<std::uint64_t>(maxReadyFrames) + 8ULL;

                return planeBytes > packetBytesBeforeLast &&
                    expectedLastPacketBytes == static_cast<std::uint64_t>(lastPacketValidSize) &&
                    frameBytes <= MEYER_DEVICE_TRANSPORT_MAX_FRAME_BYTES &&
                    inFlightBytes <= MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY &&
                    frameBytes <= MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY / retainedFrameCopies;
            }

            int width;                       // 单个图像平面的宽度。
            int height;
            int imageCount;                  // 一个完整扫描帧包含的平面数。
            int packetsPerImage;             // 每个平面的固定包数。
            std::size_t transferSize;        // 单次 CyAPI 异步读缓冲长度。
            std::size_t queueDepth;          // 预提交异步读请求数量。
            int packetPayloadSize;           // 普通包有效字节数。
            int lastPacketValidSize;         // 每个平面最后一包有效字节数。
            std::uint32_t timeoutMs;          // 单次底层接收超时。
            std::size_t maxReadyFrames;       // 内存中最多保留的完整帧数。
            DeviceType deviceType;
            PictureOrderMode pictureOrderMode;
            CaptureScanMode scanMode;
            bool ahrsEnabled;
            bool gyroscopePauseFlag;
            bool resetImuReferenceRequested;
        };
    }
}
