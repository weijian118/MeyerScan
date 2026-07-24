// =============================================================================
// 文件: CaptureServicePostProcess.cpp
// 作用: 实现协议级慢处理，并调用场景级 CaptureImagePipeline 生成多路输出。
// =============================================================================
#include "CaptureServiceContext.h"

#include "../support/ModuleLogger.h"

#include <utility>

namespace meyer
{
    namespace captureservice
    {
        // 慢线程等待深复制数据；停止请求到来后仍会处理完已经入队的旧数据。
        void CaptureServiceContext::PostProcessLoop()
        {
            // for(;;) 表示有明确 break 出口的长期工作循环；相比 while(true)
            // 它不会触发 VS2015 对常量条件的 C4127 警告。
            for (;;)
            {
                PostProcessItem item;
                MeyerCapturePipelineOptions pipelineOptions = {};
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_postCondition.wait(lock, [this]() {
                        return m_postStopRequested || !m_postQueue.empty();
                    });
                    if (m_postQueue.empty())
                    {
                        if (m_postStopRequested)
                        {
                            break;
                        }
                        continue;
                    }
                    item = std::move(m_postQueue.front());
                    m_postQueue.pop_front();
                    // 同一组六图只使用这一刻复制出的开关 revision；UI 后续修改
                    // 只影响下一组，避免一组处理中途切换算法配置。
                    pipelineOptions = m_pipelineOptions;
                    m_state.postQueueSize =
                        static_cast<std::int32_t>(m_postQueue.size());
                    ++m_state.sequence;
                }

                std::vector<unsigned char> processed(item.decryptedGroup.size(), 0U);
                MeyerCaptureGroupInfo outputInfo = {};
                m_processing.InitGroupInfo(outputInfo);
                std::size_t processedBytes = 0U;
                const std::int32_t processResult = m_processing.ProcessSlowGroup(
                    m_profile,
                    item.decryptedGroup.empty() ? nullptr : &item.decryptedGroup[0],
                    item.decryptedGroup.size(),
                    item.groupInfo,
                    processed.empty() ? nullptr : &processed[0],
                    processed.size(),
                    processedBytes,
                    outputInfo);
                if (processResult != MeyerCaptureProcessingResult_Ok ||
                    processedBytes != processed.size())
                {
                    PublishEvent(MeyerCaptureServiceEvent_ProcessingError,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 processResult,
                                 item.groupInfo.groupSequence,
                                 m_processing.LastError().empty()
                                     ? "Slow image processing failed"
                                     : m_processing.LastError());
                    WriteError("ProcessSlowGroup", "Slow image processing failed");
                    continue;
                }

                // 协议级标准化完成后交给独立 Pipeline。这里开始才允许按场景、
                // 设备身份和 UI 开关调用颜色/AI/条纹等算法 DLL。
                const std::int32_t pipelineResult = m_imagePipeline.ProcessGroup(
                    processed.empty() ? nullptr : &processed[0],
                    processed.size(),
                    outputInfo,
                    pipelineOptions);
                if (pipelineResult != MeyerCaptureImagePipelineResult_Ok)
                {
                    const bool unavailable = pipelineResult ==
                        MeyerCaptureImagePipelineResult_FeatureUnavailable;
                    PublishEvent(
                                 unavailable
                                     ? MeyerCaptureServiceEvent_ImagePipelineFeatureUnavailable
                                     : MeyerCaptureServiceEvent_ProcessingError,
                                 unavailable
                                     ? MeyerCaptureServiceEventSeverity_Warning
                                     : MeyerCaptureServiceEventSeverity_Error,
                                 pipelineResult,
                                 item.groupInfo.groupSequence,
                                 m_imagePipeline.LastError().empty()
                                     ? "Capture image pipeline processing failed"
                                     : m_imagePipeline.LastError());
                    WriteError("CaptureImagePipeline",
                               "Capture image pipeline processing failed");
                    continue;
                }

                MeyerCapturePipelineOutputInfo displayInfo = {};
                MeyerCapturePipelineOutputInfo reconstructionInfo = {};
                m_imagePipeline.InitOutputInfo(displayInfo);
                m_imagePipeline.InitOutputInfo(reconstructionInfo);
                if (m_imagePipeline.GetOutputInfo(
                        MeyerCapturePipelineOutput_DisplayRgb888, displayInfo) !=
                        MeyerCaptureImagePipelineResult_Ok ||
                    m_imagePipeline.GetOutputInfo(
                        MeyerCapturePipelineOutput_ReconstructionPlanes,
                        reconstructionInfo) != MeyerCaptureImagePipelineResult_Ok)
                {
                    PublishEvent(MeyerCaptureServiceEvent_ProcessingError,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 MeyerCaptureServiceResult_InternalError,
                                 item.groupInfo.groupSequence,
                                 "Capture image pipeline output metadata is missing");
                    continue;
                }

                std::vector<unsigned char> rgb(
                    static_cast<std::size_t>(displayInfo.byteSize), 0U);
                std::vector<unsigned char> reconstruction(
                    static_cast<std::size_t>(reconstructionInfo.byteSize), 0U);
                std::size_t rgbBytes = 0U;
                std::size_t reconstructionBytes = 0U;
                if (m_imagePipeline.CopyOutput(
                        MeyerCapturePipelineOutput_DisplayRgb888,
                        rgb.empty() ? nullptr : &rgb[0], rgb.size(), rgbBytes) !=
                        MeyerCaptureImagePipelineResult_Ok ||
                    m_imagePipeline.CopyOutput(
                        MeyerCapturePipelineOutput_ReconstructionPlanes,
                        reconstruction.empty() ? nullptr : &reconstruction[0],
                        reconstruction.size(), reconstructionBytes) !=
                        MeyerCaptureImagePipelineResult_Ok ||
                    rgbBytes != rgb.size() ||
                    reconstructionBytes != reconstruction.size())
                {
                    PublishEvent(MeyerCaptureServiceEvent_ProcessingError,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 MeyerCaptureServiceResult_InternalError,
                                 item.groupInfo.groupSequence,
                                 "Capture image pipeline output copy failed");
                    continue;
                }

                // 最终结果一次性交换到共享区，UI 不会看到半组旧图和半组新图。
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_latestProcessedGroup.swap(reconstruction);
                    m_latestRgb.swap(rgb);
                    m_latestDisplayOutputInfo = displayInfo;
                    m_latestReconstructionOutputInfo = reconstructionInfo;
                    m_latestGroupInfo = outputInfo;
                    m_state.latestDataAvailable = 1;
                    m_state.latestGroupSequence = outputInfo.groupSequence;
                    m_state.latestLedOn = outputInfo.ledOn;
                    m_state.latestLongPressed = outputInfo.longPressed;
                    m_state.latestScanHeadType = outputInfo.scanHeadType;
                    m_state.latestPipelineOptionsRevision =
                        pipelineOptions.optionsRevision;
                    m_state.latestPipelineUnavailableFeatures =
                        displayInfo.unavailableFeatures;
                    ++m_state.sequence;
                }
                PublishEvent(MeyerCaptureServiceEvent_ImagePipelineReady,
                             MeyerCaptureServiceEventSeverity_Info,
                             MeyerCaptureServiceResult_Ok,
                             outputInfo.groupSequence,
                             "Capture image pipeline outputs are ready");
                PublishEvent(MeyerCaptureServiceEvent_GroupProcessed,
                             MeyerCaptureServiceEventSeverity_Info,
                             MeyerCaptureServiceResult_Ok,
                             outputInfo.groupSequence,
                             "Normalized six-plane and display outputs are ready");
            }
        }
    }
}
