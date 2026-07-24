// =============================================================================
// 文件: CaptureServiceContext.h
// 作用: 声明采集服务的会话状态、线程、队列和业务编排入口。
//
// 线程模型:
//   - CaptureLoop 是唯一快速线程，串行执行接收、组帧、解密、曝光占位调用和
//     无回包命令，保证 DeviceCmd 不会被多个采集线程并发调用。
//   - PostProcessLoop 消费已解密数据的深副本，执行排序、镜像、减黑图和 RGB。
//   - UI 线程只调用公共查询/请求函数，不直接访问下层 DLL。
// =============================================================================
#pragma once

#include "CaptureService.h"

#include "../loader/AutoExposureLibrary.h"
#include "../loader/CaptureProcessingLibrary.h"
#include "../loader/CaptureImagePipelineLibrary.h"
#include "../loader/DeviceCmdLibrary.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace meyer
{
    namespace captureservice
    {
        class CaptureServiceContext
        {
        public:
            // 构造仅初始化内存状态，不加载 DLL。
            CaptureServiceContext();
            // 析构停止线程并关闭设备。
            ~CaptureServiceContext();

            // 保存配置并动态加载下层模块。
            std::int32_t Configure(const MeyerCaptureServiceConfig& config);
            // 完整释放当前设备和采集资源。
            std::int32_t Shutdown();
            // 执行颜色校准准入并建立处理 Profile。
            std::int32_t PrepareColorCalibration();
            // 启动/停止采集流水线。
            std::int32_t StartCapture();
            std::int32_t StopCapture();
            // 请求开灯或关灯。
            std::int32_t RequestLight(bool on);
            std::int32_t SetPipelineOptions(
                const MeyerCapturePipelineOptions& options);

            // 复制只读状态和事件。
            std::int32_t GetDeviceInfo(MeyerCaptureServiceDeviceInfo& info) const;
            std::int32_t GetStateSnapshot(MeyerCaptureServiceStateSnapshot& snapshot) const;
            std::int32_t PollEvent(MeyerCaptureServiceEvent& eventInfo);
            // 复制最近慢处理结果。
            std::int32_t CopyLatestPlane(std::int32_t index,
                                         unsigned char* buffer,
                                         std::size_t capacity,
                                         std::size_t& required) const;
            std::int32_t CopyLatestRgb888(unsigned char* buffer,
                                          std::size_t capacity,
                                          std::size_t& required) const;
            std::int32_t GetLatestPipelineOutputInfo(
                std::int32_t outputType,
                MeyerCapturePipelineOutputInfo& info) const;
            std::int32_t CopyLatestPipelineOutput(
                std::int32_t outputType,
                unsigned char* buffer,
                std::size_t capacity,
                std::size_t& required) const;
            std::int32_t CopyLatestGroupInfo(MeyerCaptureGroupInfo& info) const;
            // 返回最近错误文本副本。
            std::string LastError() const;

        private:
            // 慢处理队列元素拥有独立六图副本，快速线程离开后仍可安全使用。
            struct PostProcessItem
            {
                std::vector<unsigned char> decryptedGroup;
                MeyerCaptureGroupInfo groupInfo;
            };

            // 灯光请求只有一个布尔参数，实际命令在组六图边界发送。
            struct LightRequest
            {
                bool on;
                std::uint64_t sequence;
            };

            // 快速采集线程入口。
            void CaptureLoop();
            // 慢速后处理线程入口。
            void PostProcessLoop();
            // 处理一组完整解密图的曝光占位和命令窗口。
            void HandleCompletedGroup(std::vector<unsigned char>& decrypted,
                                      const MeyerCaptureGroupInfo& groupInfo);
            // 发送自动曝光命令和最多两条无回包命令。
            void RunCommandWindow(const MeyerAutoExposureOutput* exposureOutput,
                                  std::uint64_t groupSequence);
            // 将曝光 DLL 的 16 字节结果转换为 DeviceCmd 固定结构。
            static void DecodeExposureOutput(
                const MeyerAutoExposureOutput& output,
                MeyerDeviceCmdExposureParameters& parameters);

            // 从预检结果构建服务设备快照和处理上下文。
            void FillDeviceInfo(const MeyerDeviceCalibrationPreflight& preflight);
            void FillCaptureDeviceContext(
                const MeyerDeviceCalibrationPreflight& preflight);
            // 创建并写入结构化事件。
            void PublishEvent(std::int32_t type,
                              std::int32_t severity,
                              std::int32_t result,
                              std::uint64_t groupSequence,
                              const std::string& text);
            // 刷新原始流统计到服务状态。
            void UpdateStreamDiagnostics();
            // 设置故障状态、错误文本和错误事件。
            void SetFault(std::int32_t result,
                          std::int32_t eventType,
                          const std::string& text);
            // 修改状态并推进快照序号。
            void SetState(std::int32_t state, std::int32_t lastResult);
            // 从命令队列取出下一条灯光请求。
            bool PopLightRequest(LightRequest& request);
            // 清空上次采集结果和临时队列。
            void ResetRuntimeData();

            mutable std::mutex m_mutex;
            std::condition_variable m_postCondition;
            std::thread m_captureThread;
            std::thread m_postThread;
            std::atomic<bool> m_stopRequested;
            bool m_postStopRequested;
            bool m_configured;
            bool m_preflightReady;
            bool m_captureActive;
            bool m_autoExposureReservedReported;

            MeyerCaptureServiceConfig m_config;
            MeyerCaptureServiceDeviceInfo m_deviceInfo;
            MeyerCaptureServiceStateSnapshot m_state;
            MeyerDeviceCmdOpenParams m_openParams;
            MeyerDeviceCalibrationPreflight m_preflight;
            MeyerCaptureProfileConfig m_profile;
            MeyerCaptureDeviceContext m_captureDeviceContext;
            MeyerCaptureGroupInfo m_latestGroupInfo;

            DeviceCmdLibrary m_deviceCmd;
            CaptureProcessingLibrary m_processing;
            CaptureImagePipelineLibrary m_imagePipeline;
            AutoExposureLibrary m_autoExposure;

            std::deque<MeyerCaptureServiceEvent> m_events;
            std::deque<PostProcessItem> m_postQueue;
            std::deque<LightRequest> m_lightRequests;
            std::vector<unsigned char> m_latestProcessedGroup;
            std::vector<unsigned char> m_latestRgb;
            MeyerCapturePipelineOptions m_pipelineOptions;
            MeyerCapturePipelineOutputInfo m_latestDisplayOutputInfo;
            MeyerCapturePipelineOutputInfo m_latestReconstructionOutputInfo;
            std::string m_lastError;
            std::uint64_t m_eventSequence;
            std::uint64_t m_lightRequestSequence;
        };
    }
}
