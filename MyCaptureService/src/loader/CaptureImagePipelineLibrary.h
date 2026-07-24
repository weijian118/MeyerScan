// =============================================================================
// 文件: CaptureImagePipelineLibrary.h
// 作用: 声明 CaptureImagePipeline DLL 的动态加载和多输出复制适配器。
// =============================================================================
#pragma once

#include "CaptureImagePipeline.h"

#include <string>

namespace meyer
{
    namespace captureservice
    {
        class CaptureImagePipelineLibrary
        {
        public:
            CaptureImagePipelineLibrary();
            ~CaptureImagePipelineLibrary();

            // 通过绝对路径加载并校验整数 ABI，成功后创建一个流水线会话。
            std::int32_t Load(const std::string& pathUtf8);
            std::int32_t InitOptions(MeyerCapturePipelineOptions& options);
            std::int32_t InitOutputInfo(MeyerCapturePipelineOutputInfo& info);
            std::int32_t Configure(const MeyerCaptureProfileConfig& profile,
                                   const MeyerCaptureDeviceContext& context);
            std::int32_t ProcessGroup(const unsigned char* normalizedGroup,
                                      std::size_t normalizedBytes,
                                      const MeyerCaptureGroupInfo& groupInfo,
                                      const MeyerCapturePipelineOptions& options);
            std::int32_t GetOutputInfo(std::int32_t outputType,
                                       MeyerCapturePipelineOutputInfo& info);
            std::int32_t CopyOutput(std::int32_t outputType,
                                    unsigned char* buffer,
                                    std::size_t capacity,
                                    std::size_t& requiredBytes);
            const std::string& LastError() const;

        private:
            struct Functions;
            void Unload();
            void ReadLastError();

            void* m_module;
            MeyerCaptureImagePipelineHandle m_handle;
            Functions* m_functions;
            std::string m_lastError;
        };
    }
}
