// =============================================================================
// 文件: CaptureSlowProcessor.h
// 作用: 声明异步慢后处理：调换平面、相机 1 水平镜像和减黑图。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <string>

namespace meyer
{
    namespace captureprocessing
    {
        class CaptureSlowProcessor
        {
        public:
            // 无状态处理器不保留上一组图的内容，方便多个后处理线程各自使用。
            static bool Process(const MeyerCaptureProfileConfig& profile,
                                const unsigned char* decryptedGroup,
                                std::size_t decryptedBytes,
                                const MeyerCaptureGroupInfo& inputInfo,
                                unsigned char* processedGroup,
                                std::size_t capacity,
                                std::size_t& requiredBytes,
                                MeyerCaptureGroupInfo& outputInfo,
                                std::string& error);

        private:
            CaptureSlowProcessor();
        };
    }
}
