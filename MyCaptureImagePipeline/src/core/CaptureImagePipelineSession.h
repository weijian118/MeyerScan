// =============================================================================
// 文件: CaptureImagePipelineSession.h
// 作用: 声明场景级图像流水线会话和多输出缓存。
// =============================================================================
#pragma once

#include "CaptureImagePipeline.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace meyer
{
    namespace capturepipeline
    {
        class CaptureImagePipelineSession
        {
        public:
            // 构造函数只初始化内存，不加载算法 DLL。
            CaptureImagePipelineSession();

            // 保存本采集会话的 Profile 和设备身份快照。
            std::int32_t Configure(const MeyerCaptureProfileConfig& profile,
                                   const MeyerCaptureDeviceContext& context);

            // 对一组标准化六图执行当前可用的流水线并生成多路输出。
            std::int32_t ProcessGroup(const unsigned char* normalizedGroup,
                                      std::size_t normalizedBytes,
                                      const MeyerCaptureGroupInfo& groupInfo,
                                      const MeyerCapturePipelineOptions& options);

            // 查询和深复制一个具名输出。
            std::int32_t GetOutputInfo(std::int32_t outputType,
                                       MeyerCapturePipelineOutputInfo& info) const;
            std::int32_t CopyOutput(std::int32_t outputType,
                                    unsigned char* buffer,
                                    std::size_t capacity,
                                    std::size_t& requiredBytes) const;
            std::string LastError() const;

        private:
            struct OutputSlot
            {
                MeyerCapturePipelineOutputInfo info;
                std::vector<unsigned char> bytes;
            };

            // 生成不依赖外部算法的基础 RGB888 输出。
            bool BuildBaseRgb888(const unsigned char* normalizedGroup,
                                 std::size_t planeBytes,
                                 std::vector<unsigned char>& rgb,
                                 std::string& error) const;
            // 创建并填充输出描述，避免不同输出重复维护字段。
            MeyerCapturePipelineOutputInfo MakeOutputInfo(
                std::int32_t outputType,
                std::int32_t dataFormat,
                std::int32_t imageCount,
                std::int32_t channels,
                std::uint64_t byteSize,
                const MeyerCaptureGroupInfo& groupInfo,
                const MeyerCapturePipelineOptions& options,
                std::uint64_t unavailableFeatures) const;

            mutable std::mutex m_mutex;
            bool m_configured;
            MeyerCaptureProfileConfig m_profile;
            MeyerCaptureDeviceContext m_context;
            std::map<std::int32_t, OutputSlot> m_outputs;
            std::string m_lastError;
        };
    }
}
