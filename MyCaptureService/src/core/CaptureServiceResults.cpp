// =============================================================================
// 文件: CaptureServiceResults.cpp
// 作用: 实现事件队列、状态快照、结果复制和统一故障记录。
// =============================================================================
#include "CaptureServiceContext.h"

#include "../support/ModuleLogger.h"

#include <cstring>

namespace
{
    // 固定事件文本数组始终清零并保留字符串结尾。
    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const std::string& source)
    {
        std::memset(destination, 0, Capacity);
        // 原生数组模板参数不可能为 0；始终只复制 Capacity-1 个字节，最后
        // 一个字节由上面的清零保留为 UTF-8 字符串终止符。
        std::strncpy(destination, source.c_str(), Capacity - 1U);
    }
}

namespace meyer
{
    namespace captureservice
    {
        std::int32_t CaptureServiceContext::GetDeviceInfo(
            MeyerCaptureServiceDeviceInfo& info) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            info = m_deviceInfo;
            return MeyerCaptureServiceResult_Ok;
        }

        std::int32_t CaptureServiceContext::GetStateSnapshot(
            MeyerCaptureServiceStateSnapshot& snapshot) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot = m_state;
            return MeyerCaptureServiceResult_Ok;
        }

        std::int32_t CaptureServiceContext::PollEvent(
            MeyerCaptureServiceEvent& eventInfo)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_events.empty())
            {
                return MeyerCaptureServiceResult_NotReady;
            }
            eventInfo = m_events.front();
            m_events.pop_front();
            return MeyerCaptureServiceResult_Ok;
        }

        // 单图在慢处理组六图中连续存储，偏移等于 index*width*height。
        std::int32_t CaptureServiceContext::CopyLatestPlane(
            std::int32_t index,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& required) const
        {
            required = 0U;
            if (index < 0 || index > 6)
            {
                return MeyerCaptureServiceResult_InvalidArgument;
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state.latestDataAvailable == 0 || m_latestProcessedGroup.empty())
            {
                return MeyerCaptureServiceResult_NoData;
            }
            // 当前 Profile 只有 0~5 六张图；选项 6 是测试界面的显式保留项。
            if (index >= m_profile.imageCount)
            {
                return MeyerCaptureServiceResult_NoData;
            }
            required = static_cast<std::size_t>(m_profile.width) *
                       static_cast<std::size_t>(m_profile.height);
            if (buffer == nullptr || capacity < required)
            {
                return MeyerCaptureServiceResult_BufferTooSmall;
            }
            const std::size_t offset = static_cast<std::size_t>(index) * required;
            if (offset > m_latestProcessedGroup.size() ||
                required > m_latestProcessedGroup.size() - offset)
            {
                required = 0U;
                return MeyerCaptureServiceResult_InternalError;
            }
            std::memcpy(buffer, &m_latestProcessedGroup[offset], required);
            return MeyerCaptureServiceResult_Ok;
        }

        std::int32_t CaptureServiceContext::CopyLatestRgb888(
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& required) const
        {
            required = 0U;
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state.latestDataAvailable == 0 || m_latestRgb.empty())
            {
                return MeyerCaptureServiceResult_NoData;
            }
            required = m_latestRgb.size();
            if (buffer == nullptr || capacity < required)
            {
                return MeyerCaptureServiceResult_BufferTooSmall;
            }
            std::memcpy(buffer, &m_latestRgb[0], required);
            return MeyerCaptureServiceResult_Ok;
        }

        std::int32_t CaptureServiceContext::GetLatestPipelineOutputInfo(
            std::int32_t outputType,
            MeyerCapturePipelineOutputInfo& info) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state.latestDataAvailable == 0)
            {
                return MeyerCaptureServiceResult_NoData;
            }
            if (outputType == MeyerCapturePipelineOutput_DisplayRgb888)
            {
                info = m_latestDisplayOutputInfo;
                return MeyerCaptureServiceResult_Ok;
            }
            if (outputType == MeyerCapturePipelineOutput_ReconstructionPlanes)
            {
                info = m_latestReconstructionOutputInfo;
                return MeyerCaptureServiceResult_Ok;
            }
            return MeyerCaptureServiceResult_NoData;
        }

        std::int32_t CaptureServiceContext::CopyLatestPipelineOutput(
            std::int32_t outputType,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& required) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            required = 0U;
            const std::vector<unsigned char>* source = nullptr;
            if (outputType == MeyerCapturePipelineOutput_DisplayRgb888)
            {
                source = &m_latestRgb;
            }
            else if (outputType == MeyerCapturePipelineOutput_ReconstructionPlanes)
            {
                source = &m_latestProcessedGroup;
            }
            else
            {
                return MeyerCaptureServiceResult_NoData;
            }
            if (m_state.latestDataAvailable == 0 || source->empty())
            {
                return MeyerCaptureServiceResult_NoData;
            }
            required = source->size();
            if (buffer == nullptr || capacity < required)
            {
                return MeyerCaptureServiceResult_BufferTooSmall;
            }
            std::memcpy(buffer, &(*source)[0], required);
            return MeyerCaptureServiceResult_Ok;
        }

        std::int32_t CaptureServiceContext::CopyLatestGroupInfo(
            MeyerCaptureGroupInfo& info) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state.latestDataAvailable == 0)
            {
                return MeyerCaptureServiceResult_NoData;
            }
            info = m_latestGroupInfo;
            return MeyerCaptureServiceResult_Ok;
        }

        std::string CaptureServiceContext::LastError() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_lastError;
        }

        // PublishEvent 只做内存队列操作，不弹窗、不阻塞采集线程。
        void CaptureServiceContext::PublishEvent(
            std::int32_t type,
            std::int32_t severity,
            std::int32_t result,
            std::uint64_t groupSequence,
            const std::string& text)
        {
            MeyerCaptureServiceEvent eventInfo = {};
            eventInfo.structSize = sizeof(eventInfo);
            eventInfo.schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
            eventInfo.type = type;
            eventInfo.severity = severity;
            eventInfo.resultCode = result;
            eventInfo.groupSequence = groupSequence;
            eventInfo.groupSequenceLow = static_cast<std::int32_t>(
                groupSequence & 0x7FFFFFFFULL);
            CopyText(eventInfo.textUtf8, text);

            std::lock_guard<std::mutex> lock(m_mutex);
            eventInfo.sequence = ++m_eventSequence;
            eventInfo.packetCount = m_state.totalPackets;
            eventInfo.consecutiveTimeouts = m_state.consecutiveTimeouts;
            const std::size_t capacity = m_config.eventQueueCapacity == 0U
                ? 256U : static_cast<std::size_t>(m_config.eventQueueCapacity);
            if (m_events.size() >= capacity)
            {
                // UI 长时间不消费时丢最旧事件，保留最新故障和状态。
                m_events.pop_front();
            }
            m_events.push_back(eventInfo);
        }

        // 诊断读取只复制 Transport 内存，不发送设备命令或占用采集命令窗口。
        void CaptureServiceContext::UpdateStreamDiagnostics()
        {
            MeyerDeviceCmdStreamDiagnostics diagnostics = {};
            if (m_deviceCmd.InitStreamDiagnostics(diagnostics) !=
                    MeyerDeviceCmdResult_Ok ||
                m_deviceCmd.GetStreamDiagnostics(diagnostics) !=
                    MeyerDeviceCmdResult_Ok)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state.totalPackets = diagnostics.totalPackets;
            m_state.totalTimeouts = diagnostics.totalTimeouts;
            m_state.totalPartialPackets = diagnostics.totalPartialPackets;
            m_state.consecutiveTimeouts = diagnostics.consecutiveTimeouts;
            ++m_state.sequence;
        }

        // 故障会让快速线程在下一检查点退出；UI 通过事件决定显示何种提示。
        void CaptureServiceContext::SetFault(
            std::int32_t result,
            std::int32_t eventType,
            const std::string& text)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastError = text;
                m_state.state = MeyerCaptureServiceState_Faulted;
                m_state.lastResult = result;
                ++m_state.sequence;
                m_stopRequested.store(true);
            }
            PublishEvent(eventType,
                         MeyerCaptureServiceEventSeverity_Error,
                         result, 0U, text);
            WriteError("CaptureFault", text.c_str());
        }

        void CaptureServiceContext::SetState(
            std::int32_t state, std::int32_t lastResult)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state.state = state;
                m_state.lastResult = lastResult;
                ++m_state.sequence;
            }
            PublishEvent(MeyerCaptureServiceEvent_StateChanged,
                         MeyerCaptureServiceEventSeverity_Info,
                         lastResult, 0U,
                         "Capture service state changed");
        }

        // 新采集会话清空图像和队列，但保留准入和模块加载事件供 UI 查看。
        void CaptureServiceContext::ResetRuntimeData()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_postQueue.clear();
            m_lightRequests.clear();
            m_latestProcessedGroup.clear();
            m_latestRgb.clear();
            std::memset(&m_latestGroupInfo, 0, sizeof(m_latestGroupInfo));
            std::memset(&m_latestDisplayOutputInfo, 0,
                        sizeof(m_latestDisplayOutputInfo));
            std::memset(&m_latestReconstructionOutputInfo, 0,
                        sizeof(m_latestReconstructionOutputInfo));
            m_state.latestDataAvailable = 0;
            m_state.postQueueSize = 0;
            m_state.latestGroupSequence = 0U;
            m_state.droppedPostProcessGroups = 0U;
            m_state.totalPackets = 0U;
            m_state.totalTimeouts = 0U;
            m_state.totalPartialPackets = 0U;
            m_state.consecutiveTimeouts = 0;
            m_state.latestLedOn = 0;
            m_state.latestLongPressed = 0;
            m_state.latestScanHeadType = MeyerCaptureScanHead_Unknown;
            m_state.latestPipelineOptionsRevision = m_pipelineOptions.optionsRevision;
            m_state.latestPipelineUnavailableFeatures = 0ULL;
            ++m_state.sequence;
        }
    }
}
