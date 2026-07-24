// =============================================================================
// 文件: AutoExposureLibrary.h
// 作用: 声明自动曝光占位 DLL 的动态调用适配器。
// =============================================================================
#pragma once

#include "AutoExposure.h"

#include <string>

namespace meyer
{
    namespace captureservice
    {
        class AutoExposureLibrary
        {
        public:
            // 构造空适配器；自动曝光 DLL 缺失时服务仍可运行并明确上报保留状态。
            AutoExposureLibrary();
            // 释放自动曝光会话和 DLL。
            ~AutoExposureLibrary();
            // 加载占位接口。
            std::int32_t Load(const std::string& pathUtf8);
            // 保存采集会话设备上下文。
            std::int32_t Configure(const MeyerCaptureDeviceContext& context);
            // 调用当前未实现的计算接口。
            std::int32_t Calculate(const MeyerCaptureProfileConfig& profile,
                                   const MeyerCaptureGroupInfo& groupInfo,
                                   const unsigned char* decrypted,
                                   std::size_t bytes,
                                   MeyerAutoExposureOutput& output);
            // 返回最近错误文本和是否完成动态加载。
            const std::string& LastError() const;
            bool IsLoaded() const;

        private:
            struct Functions;
            void Unload();
            void ReadLastError();
            void* m_module;
            MeyerAutoExposureHandle m_handle;
            Functions* m_functions;
            std::string m_lastError;
        };
    }
}
