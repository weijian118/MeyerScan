#pragma once

#include <cstddef>
#include <vector>

#include "../api/DeviceTypes.h"
#include "DeviceStatus.h"
#include "ExposureParams.h"
#include "ImuSample.h"

namespace meyer
{
    namespace device
    {
        struct ImagePlaneInfo
        {
            // 未绑定平面时索引为 -1。
            ImagePlaneInfo()
                : imageIndex(-1)
            {
            }

            int imageIndex;
            ExposureParams exposure;
        };

        struct ImageFrame
        {
            // 初始化空帧元数据。
            ImageFrame()
                : width(0)
                , height(0)
                , imageCount(0)
                , deviceType(DeviceType::Unknown)
            {
            }

            // 计算单平面字节数；当前每像素占一个 Byte。
            std::size_t PlaneSize() const
            {
                return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            }

            // pixels 为空表示没有可交付的完整帧。
            bool Empty() const
            {
                return pixels.empty();
            }

            // 同时清空像素、平面元数据、状态和 IMU，防止沿用上一帧数据。
            void Clear()
            {
                width = 0;
                height = 0;
                imageCount = 0;
                deviceType = DeviceType::Unknown;
                pixels.clear();
                planeInfos.clear();
                status = DeviceStatus();
                imu = ImuSample();
            }

            int width;
            int height;
            int imageCount;
            DeviceType deviceType;

            std::vector<Byte> pixels;
            std::vector<ImagePlaneInfo> planeInfos;
            DeviceStatus status;
            ImuSample imu;
        };
    }
}
