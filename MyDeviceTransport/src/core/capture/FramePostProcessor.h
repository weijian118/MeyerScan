#pragma once

#include "../../model/CaptureFrameConfig.h"
#include "../../model/ImageFrame.h"

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            class FramePostProcessor
            {
            public:
                // 创建尚未配置的后处理器。
                FramePostProcessor();

                // 保存设备型号、排列模式和尺寸配置。
                void Configure(const CaptureFrameConfig& config);
                // 返回当前配置快照。
                const CaptureFrameConfig& GetConfig() const;
                // 查询配置是否有效。
                bool IsConfigured() const;

                // 汇总旧代码中的协议头解析、设备状态提取、曝光读取和平面重排，
                // 但不在此处进行扫描重建算法处理。
                // 重排平面并提取 LED、拍照、曝光和扫描头状态。
                bool Process(ImageFrame& frame);

            private:
                CaptureFrameConfig m_config;
                bool m_configured;
            };
        }
    }
}
