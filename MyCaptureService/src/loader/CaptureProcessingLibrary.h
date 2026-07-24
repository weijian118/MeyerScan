// =============================================================================
// 文件: CaptureProcessingLibrary.h
// 作用: 声明 CaptureProcessing DLL 动态调用适配器。
// =============================================================================
#pragma once

#include "CaptureProcessing.h"

#include <string>

namespace meyer
{
    namespace captureservice
    {
        class CaptureProcessingLibrary
        {
        public:
            // 创建空适配器。
            CaptureProcessingLibrary();
            // 销毁处理句柄并卸载 DLL。
            ~CaptureProcessingLibrary();
            // 按绝对路径加载并校验 ABI。
            std::int32_t Load(const std::string& pathUtf8);
            // 创建/销毁处理会话。
            std::int32_t CreateSession();
            void DestroySession();
            // 初始化和配置处理 Profile。
            std::int32_t GetDefaultProfile(std::int32_t deviceProfile,
                                           std::int32_t captureMode,
                                           MeyerCaptureProfileConfig& profile);
            // 初始化共享设备上下文和组信息 POD。
            std::int32_t InitDeviceContext(MeyerCaptureDeviceContext& context);
            std::int32_t InitGroupInfo(MeyerCaptureGroupInfo& info);
            std::int32_t Configure(const MeyerCaptureProfileConfig& profile,
                                   const MeyerCaptureDeviceContext& context);
            std::int32_t Reset();
            // 快速链路接口。
            std::int32_t PushPacket(const unsigned char* packet, std::size_t bytes);
            std::int32_t AbortIncompleteGroup();
            std::int32_t CopyCompletedGroup(unsigned char* buffer,
                                            std::size_t capacity,
                                            std::size_t& required,
                                            MeyerCaptureGroupInfo& info);
            // 慢处理接口。
            std::int32_t ProcessSlowGroup(const MeyerCaptureProfileConfig& profile,
                                          const unsigned char* input,
                                          std::size_t inputBytes,
                                          const MeyerCaptureGroupInfo& inputInfo,
                                          unsigned char* output,
                                          std::size_t capacity,
                                          std::size_t& required,
                                          MeyerCaptureGroupInfo& outputInfo);
            const std::string& LastError() const;

        private:
            struct Functions;
            std::int32_t ResolveFunctions();
            void Unload();
            void ReadLastError();
            void* m_module;
            MeyerCaptureProcessingHandle m_handle;
            Functions* m_functions;
            std::string m_lastError;
        };
    }
}
