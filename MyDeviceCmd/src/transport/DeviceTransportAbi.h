// =============================================================================
// 文件: DeviceTransportAbi.h
// 作用: 保存 DeviceTransport API v1 的最小私有 ABI 镜像。
//
// 说明:
//   MyDeviceCmd 运行时只依赖 MeyerScan_DeviceTransport.dll，不链接 import lib。
//   为保证整个 MyDeviceCmd 文件夹移动后仍可独立编译，这里不包含相邻项目头文件，
//   而是镜像本模块实际调用的 POD 结构。加载时必须先校验
//   GetMeyerModuleApiVersion()==1；DeviceTransport 升级 ABI 后应同步更新本文件。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>

namespace meyer
{
    namespace devicecmd
    {
        namespace transportabi
        {
            // 私有镜像的结构版本和整数 ABI 版本，必须与 DeviceTransport 一致。
            static const std::uint32_t kSchemaVersion = 1U;
            static const std::int32_t kApiVersion = 1;

            // Transport 结果码只在动态适配器内部使用，随后转换为 DeviceCmd 结果码。
            enum Result : std::int32_t
            {
                Ok = 0,
                InvalidHandle = -1,
                InvalidArgument = -2,
                UnsupportedTransport = -3,
                DeviceNotFound = -4,
                NotOpen = -5,
                IoFailed = -6,
                Timeout = -7,
                BufferTooSmall = -8,
                AlreadyRunning = -9,
                NotRunning = -10,
                NotReady = -11,
                InternalError = -12
            };

            // 对应 DeviceTransport 的设备打开参数布局。
            struct OpenParams
            {
                std::uint32_t structSize;
                std::uint32_t schemaVersion;
                std::int32_t transportType;
                std::uint16_t vendorId;
                std::uint16_t productId;
                std::uint32_t deviceIndex;
                std::uint32_t commandTimeoutMs;
                std::uint32_t streamTimeoutMs;
                char host[128];
                std::uint16_t port;
                std::uint16_t reserved16;
                std::uint32_t reserved[8];
            };

            // 对应底层 B 类流的尺寸、分包和超时参数布局。
            struct CaptureParams
            {
                std::uint32_t structSize;
                std::uint32_t schemaVersion;
                std::int32_t width;
                std::int32_t height;
                std::int32_t imageCount;
                std::int32_t packetsPerImage;
                std::uint64_t transferSize;
                std::uint32_t queueDepth;
                std::int32_t packetPayloadSize;
                std::int32_t lastPacketValidSize;
                std::uint32_t timeoutMs;
                std::uint32_t maxReadyFrames;
                std::uint32_t reserved[8];
            };

            // 对应底层完整帧元数据布局，适配器随后复制到公共结构。
            struct FrameInfo
            {
                std::uint32_t structSize;
                std::uint32_t schemaVersion;
                std::int32_t width;
                std::int32_t height;
                std::int32_t imageCount;
                std::int32_t deviceType;
                std::int32_t captureStatus;
                std::int32_t scanMode;
                std::int32_t pictureOrderMode;
                std::int32_t scanHeadType;
                std::int32_t ledOn;
                std::int32_t photoMode;
                std::int32_t timeW;
                std::int32_t timeC;
                std::int32_t timeX;
                std::int32_t gainW;
                std::int32_t gainC;
                std::int32_t gainX;
                std::int32_t temperature0;
                std::int32_t temperature1;
                std::int32_t temperature2;
                std::int32_t temperature3;
                std::uint64_t frameBytes;
                std::uint32_t reserved[8];
            };

            // 底层句柄只在动态适配器内部传递，不能越过 DeviceCmd 公共 ABI。
            typedef void* Handle;
        }
    }
}
