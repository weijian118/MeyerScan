// =============================================================================
// 文件: CaptureServiceCapture.cpp
// 作用: 实现快速采集线程、超时/断连上报和组六图命令窗口。
// =============================================================================
#include "CaptureServiceContext.h"

#include "../support/ModuleLogger.h"

#include <chrono>
#include <cstring>
#include <exception>
#include <thread>

namespace
{
    // 把 CaptureMode 映射成 DeviceCmd 工作模式；两个扫描入口共用 Scan 值。
    std::int32_t WorkModeForCapture(std::int32_t captureMode)
    {
        if (captureMode == MeyerCaptureMode_ColorCalibration)
        {
            return MeyerDeviceWorkMode_CalibrationColor;
        }
        if (captureMode == MeyerCaptureMode_Calibration3D)
        {
            return MeyerDeviceWorkMode_Calibration3D;
        }
        return MeyerDeviceWorkMode_Scan;
    }
}

namespace meyer
{
    namespace captureservice
    {
        // StartCapture 只在准入成功并保持同一 DeviceCmd 会话时允许执行。
        std::int32_t CaptureServiceContext::StartCapture()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_configured)
                {
                    return MeyerCaptureServiceResult_NotConfigured;
                }
                if (!m_preflightReady)
                {
                    return MeyerCaptureServiceResult_NotReady;
                }
                if (m_captureActive || m_captureThread.joinable() || m_postThread.joinable())
                {
                    return MeyerCaptureServiceResult_AlreadyRunning;
                }
            }

            MeyerDeviceCmdCaptureParams params = {};
            if (m_deviceCmd.InitCaptureParamsForModel(
                    m_captureDeviceContext.deviceProfile, params) !=
                MeyerDeviceCmdResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_InternalError,
                         "DeviceCmd has no capture parameters for the detected profile");
                return MeyerCaptureServiceResult_InternalError;
            }

            // DeviceCmd 与 CaptureProcessing 必须使用完全相同的几何和分包参数。
            params.workMode = WorkModeForCapture(m_config.captureMode);
            params.width = m_profile.width;
            params.height = m_profile.height;
            params.imageCount = m_profile.imageCount;
            params.packetsPerImage = m_profile.packetsPerImage;
            params.transferSize = m_profile.packetBytes;
            params.queueDepth = m_profile.queueDepth;
            params.packetPayloadSize = static_cast<std::int32_t>(m_profile.packetBytes);
            params.lastPacketValidSize =
                static_cast<std::int32_t>(m_profile.lastPacketValidBytes);
            params.timeoutMs = m_profile.receiveTimeoutMs;
            // 颜色校准暂不消费 IMU，关闭上层姿态计算可减少无关状态处理。
            params.ahrsEnabled = m_config.captureMode == MeyerCaptureMode_ColorCalibration
                ? 0 : params.ahrsEnabled;

            if (m_processing.Reset() != MeyerCaptureProcessingResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_ProcessingError,
                         m_processing.LastError());
                return MeyerCaptureServiceResult_InternalError;
            }
            ResetRuntimeData();

            const std::int32_t startResult = m_deviceCmd.StartRawCapture(params);
            if (startResult != MeyerDeviceCmdResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_Faulted,
                         MeyerCaptureServiceEvent_InternalError,
                         m_deviceCmd.LastError());
                return MeyerCaptureServiceResult_Faulted;
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopRequested.store(false);
                m_postStopRequested = false;
                m_captureActive = true;
                m_autoExposureReservedReported = false;
                m_state.captureActive = 1;
                ++m_state.sequence;
            }
            SetState(MeyerCaptureServiceState_Running,
                     MeyerCaptureServiceResult_Ok);

            try
            {
                // 后处理线程先进入等待状态，再启动快速线程，避免第一组结果无人消费。
                m_postThread = std::thread(&CaptureServiceContext::PostProcessLoop, this);
                m_captureThread = std::thread(&CaptureServiceContext::CaptureLoop, this);
            }
            catch (const std::exception& exception)
            {
                m_stopRequested.store(true);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_postStopRequested = true;
                }
                m_postCondition.notify_all();
                if (m_postThread.joinable())
                {
                    m_postThread.join();
                }
                m_deviceCmd.StopRawCapture(true);
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_InternalError,
                         exception.what());
                return MeyerCaptureServiceResult_InternalError;
            }

            PublishEvent(MeyerCaptureServiceEvent_CaptureStarted,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "Raw image capture started");
            WriteInfo("StartCapture", "Raw image capture threads started");
            return MeyerCaptureServiceResult_Ok;
        }

        // StopCapture 先让快速线程离开接收循环，再让慢线程处理完已入队副本。
        std::int32_t CaptureServiceContext::StopCapture()
        {
            bool hasThreads = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                hasThreads = m_captureThread.joinable() || m_postThread.joinable();
                if (!m_captureActive && !hasThreads)
                {
                    return MeyerCaptureServiceResult_Ok;
                }
                if (m_state.state != MeyerCaptureServiceState_Faulted)
                {
                    m_state.state = MeyerCaptureServiceState_Stopping;
                    ++m_state.sequence;
                }
                m_stopRequested.store(true);
            }

            if (m_captureThread.joinable() &&
                m_captureThread.get_id() != std::this_thread::get_id())
            {
                m_captureThread.join();
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_postStopRequested = true;
            }
            m_postCondition.notify_all();
            if (m_postThread.joinable() &&
                m_postThread.get_id() != std::this_thread::get_id())
            {
                m_postThread.join();
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_captureActive = false;
                m_state.captureActive = 0;
                if (m_state.state != MeyerCaptureServiceState_Faulted)
                {
                    m_state.state = m_preflightReady
                        ? MeyerCaptureServiceState_Ready
                        : MeyerCaptureServiceState_Configured;
                    m_state.lastResult = MeyerCaptureServiceResult_Ok;
                }
                ++m_state.sequence;
            }
            return MeyerCaptureServiceResult_Ok;
        }

        // UI 线程只把灯命令放入队列；采集中真正的 USB OUT 由快速线程完成。
        std::int32_t CaptureServiceContext::RequestLight(bool on)
        {
            bool sendImmediately = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_preflightReady)
                {
                    return MeyerCaptureServiceResult_NotReady;
                }
                if (m_captureActive)
                {
                    // 队列只吸收短时间 UI 操作，防止持续点击无限占用内存。
                    if (m_lightRequests.size() >= 16U)
                    {
                        m_lastError = "Light command queue is full";
                        return MeyerCaptureServiceResult_QueueFull;
                    }
                    LightRequest request = {};
                    request.on = on;
                    request.sequence = ++m_lightRequestSequence;
                    m_lightRequests.push_back(request);
                }
                else
                {
                    sendImmediately = true;
                }
            }

            if (sendImmediately)
            {
                const std::int32_t result = m_deviceCmd.SetLight(on);
                if (result != MeyerDeviceCmdResult_Ok)
                {
                    PublishEvent(MeyerCaptureServiceEvent_CommandFailed,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 result, 0U, m_deviceCmd.LastError());
                    return MeyerCaptureServiceResult_Faulted;
                }
                PublishEvent(MeyerCaptureServiceEvent_LightCommandSent,
                             MeyerCaptureServiceEventSeverity_Info,
                             MeyerCaptureServiceResult_Ok, 0U,
                             on ? "Light-on command sent" : "Light-off command sent");
                return MeyerCaptureServiceResult_Ok;
            }

            PublishEvent(MeyerCaptureServiceEvent_LightCommandQueued,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         on ? "Light-on command queued" : "Light-off command queued");
            return MeyerCaptureServiceResult_Ok;
        }

        // UI 每次修改算法开关都提交完整结构；服务只保存副本，不在 UI 线程
        // 加载算法或处理图像。慢线程取出组六图时再复制一次形成不可变快照。
        std::int32_t CaptureServiceContext::SetPipelineOptions(
            const MeyerCapturePipelineOptions& options)
        {
            const std::uint64_t knownFeatures =
                MeyerCapturePipelineFeature_ColorCalibration |
                MeyerCapturePipelineFeature_AiSoftTissue |
                MeyerCapturePipelineFeature_ColorRemoval |
                MeyerCapturePipelineFeature_CoarseStripe;
            if (options.structSize < sizeof(options) ||
                options.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                options.captureMode != m_config.captureMode ||
                (options.enabledFeatures & ~knownFeatures) != 0ULL ||
                (options.requiredFeatures & ~options.enabledFeatures) != 0ULL)
            {
                return MeyerCaptureServiceResult_InvalidArgument;
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_configured)
            {
                return MeyerCaptureServiceResult_NotConfigured;
            }
            m_pipelineOptions = options;
            ++m_state.sequence;
            return MeyerCaptureServiceResult_Ok;
        }

        // 快速线程每次准备接收新组六图前做一次不发送命令的轻量连接检查。
        void CaptureServiceContext::CaptureLoop()
        {
            const std::size_t packetBytes = static_cast<std::size_t>(m_profile.packetBytes);
            const std::size_t groupBytes =
                static_cast<std::size_t>(m_profile.width) *
                static_cast<std::size_t>(m_profile.height) *
                static_cast<std::size_t>(m_profile.imageCount);
            std::vector<unsigned char> packet(packetBytes, 0U);
            std::vector<unsigned char> decrypted(groupBytes, 0U);
            bool awaitingGroupStart = true;

            while (!m_stopRequested.load())
            {
                if (awaitingGroupStart)
                {
                    const std::int32_t connected =
                        m_deviceCmd.IsDeviceConnectedLightweight();
                    if (connected != 1)
                    {
                        m_processing.AbortIncompleteGroup();
                        SetFault(MeyerCaptureServiceResult_DeviceDisconnected,
                                 MeyerCaptureServiceEvent_DeviceDisconnected,
                                 "Device connection was lost before a new image group");
                        break;
                    }
                }

                std::size_t received = 0U;
                const std::int32_t receiveResult =
                    m_deviceCmd.ReceiveRawCapturePacket(
                        &packet[0], packet.size(), received,
                        m_profile.receiveTimeoutMs);
                if (receiveResult == MeyerDeviceCmdResult_Timeout ||
                    receiveResult == MeyerDeviceCmdResult_StreamStalled)
                {
                    // 任意超时都会使当前组失去连续性，必须废弃后从 0 号图重同步。
                    m_processing.AbortIncompleteGroup();
                    awaitingGroupStart = true;
                    UpdateStreamDiagnostics();
                    MeyerCaptureServiceStateSnapshot snapshot = {};
                    snapshot.structSize = sizeof(snapshot);
                    snapshot.schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
                    GetStateSnapshot(snapshot);
                    if (receiveResult == MeyerDeviceCmdResult_Timeout)
                    {
                        PublishEvent(MeyerCaptureServiceEvent_ReceiveTimeout,
                                     MeyerCaptureServiceEventSeverity_Warning,
                                     receiveResult, 0U,
                                     "No raw capture packet was received before timeout");
                        WriteWarning("ReceiveRawCapturePacket",
                                     "First raw stream timeout; incomplete group discarded");
                        continue;
                    }
                    SetFault(MeyerCaptureServiceResult_Faulted,
                             MeyerCaptureServiceEvent_StreamStalled,
                             "Two consecutive receive timeouts stalled the raw stream");
                    break;
                }
                if (receiveResult == MeyerDeviceCmdResult_DeviceDisconnected)
                {
                    m_processing.AbortIncompleteGroup();
                    UpdateStreamDiagnostics();
                    SetFault(MeyerCaptureServiceResult_DeviceDisconnected,
                             MeyerCaptureServiceEvent_DeviceDisconnected,
                             "Device disconnected while receiving an image group");
                    break;
                }
                if (receiveResult != MeyerDeviceCmdResult_Ok)
                {
                    m_processing.AbortIncompleteGroup();
                    UpdateStreamDiagnostics();
                    SetFault(MeyerCaptureServiceResult_Faulted,
                             MeyerCaptureServiceEvent_InternalError,
                             m_deviceCmd.LastError().empty()
                                 ? "Raw stream I/O failed"
                                 : m_deviceCmd.LastError());
                    break;
                }
                if (received != packetBytes)
                {
                    m_processing.AbortIncompleteGroup();
                    awaitingGroupStart = true;
                    UpdateStreamDiagnostics();
                    PublishEvent(MeyerCaptureServiceEvent_PartialPacket,
                                 MeyerCaptureServiceEventSeverity_Warning,
                                 MeyerCaptureServiceResult_Faulted, 0U,
                                 "Raw stream returned a partial 16384-byte packet");
                    WriteWarning("ReceiveRawCapturePacket",
                                 "Partial B packet discarded with the incomplete group");
                    continue;
                }

                const std::int32_t pushResult =
                    m_processing.PushPacket(&packet[0], received);
                if (pushResult == MeyerCaptureProcessingResult_NeedMorePackets ||
                    pushResult == MeyerCaptureProcessingResult_ImageCompleted)
                {
                    awaitingGroupStart = false;
                    continue;
                }
                if (pushResult == MeyerCaptureProcessingResult_SyncReset)
                {
                    awaitingGroupStart = true;
                    PublishEvent(MeyerCaptureServiceEvent_SequenceReset,
                                 MeyerCaptureServiceEventSeverity_Warning,
                                 pushResult, 0U,
                                 "Image sequence was reset while waiting for index zero");
                    continue;
                }
                if (pushResult != MeyerCaptureProcessingResult_GroupCompleted)
                {
                    m_processing.AbortIncompleteGroup();
                    awaitingGroupStart = true;
                    PublishEvent(MeyerCaptureServiceEvent_InvalidPacket,
                                 MeyerCaptureServiceEventSeverity_Warning,
                                 pushResult, 0U,
                                 m_processing.LastError().empty()
                                     ? "Raw image packet header or content is invalid"
                                     : m_processing.LastError());
                    continue;
                }

                MeyerCaptureGroupInfo groupInfo = {};
                m_processing.InitGroupInfo(groupInfo);
                std::size_t required = 0U;
                const std::int32_t copyResult = m_processing.CopyCompletedGroup(
                    &decrypted[0], decrypted.size(), required, groupInfo);
                if (copyResult != MeyerCaptureProcessingResult_Ok ||
                    required != decrypted.size())
                {
                    SetFault(MeyerCaptureServiceResult_InternalError,
                             MeyerCaptureServiceEvent_ProcessingError,
                             "Completed decrypted group could not be copied safely");
                    break;
                }

                UpdateStreamDiagnostics();
                HandleCompletedGroup(decrypted, groupInfo);
                awaitingGroupStart = true;
            }

            // 无论正常停止还是故障，都由快速线程按固定顺序发送 0x0B/关灯并停流。
            const std::int32_t stopResult = m_deviceCmd.StopRawCapture(true);
            m_processing.AbortIncompleteGroup();
            if (stopResult != MeyerDeviceCmdResult_Ok &&
                stopResult != MeyerDeviceCmdResult_NotOpen)
            {
                PublishEvent(MeyerCaptureServiceEvent_CommandFailed,
                             MeyerCaptureServiceEventSeverity_Error,
                             stopResult, 0U, m_deviceCmd.LastError());
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_captureActive = false;
                m_state.captureActive = 0;
                m_postStopRequested = true;
                if (m_state.state != MeyerCaptureServiceState_Faulted)
                {
                    m_state.state = m_preflightReady
                        ? MeyerCaptureServiceState_Ready
                        : MeyerCaptureServiceState_Configured;
                }
                ++m_state.sequence;
            }
            m_postCondition.notify_all();
            PublishEvent(MeyerCaptureServiceEvent_CaptureStopped,
                         MeyerCaptureServiceEventSeverity_Info,
                         stopResult, 0U,
                         "Raw image capture stopped and resources were released");
            WriteInfo("StopCapture", "Raw image capture thread stopped");
        }

        // 完整组先完成曝光占位/命令窗口，再把独立副本交给慢处理线程。
        void CaptureServiceContext::HandleCompletedGroup(
            std::vector<unsigned char>& decrypted,
            const MeyerCaptureGroupInfo& groupInfo)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state.latestGroupSequence = groupInfo.groupSequence;
                m_state.latestLedOn = groupInfo.ledOn;
                m_state.latestLongPressed = groupInfo.longPressed;
                m_state.latestScanHeadType = groupInfo.scanHeadType;
                ++m_state.sequence;
            }
            PublishEvent(MeyerCaptureServiceEvent_GroupReady,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok,
                         groupInfo.groupSequence,
                         "A complete decrypted image group is ready");

            MeyerAutoExposureOutput exposure = {};
            const MeyerAutoExposureOutput* validExposure = nullptr;
            if (groupInfo.ledOn == 0)
            {
                // 关灯组跳过自动曝光，但下面仍会进入慢处理队列并发布 UI 数据。
                PublishEvent(MeyerCaptureServiceEvent_AutoExposureSkipped,
                             MeyerCaptureServiceEventSeverity_Info,
                             MeyerCaptureServiceResult_Ok,
                             groupInfo.groupSequence,
                             "Automatic exposure skipped because the group is not fully lit");
            }
            else
            {
                const std::int32_t exposureResult = m_autoExposure.Calculate(
                    m_profile, groupInfo, &decrypted[0], decrypted.size(), exposure);
                if (exposureResult == MeyerAutoExposureResult_Ok && exposure.valid != 0)
                {
                    validExposure = &exposure;
                }
                else if (exposureResult == MeyerAutoExposureResult_NotImplemented)
                {
                    bool publishReserved = false;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        if (!m_autoExposureReservedReported)
                        {
                            m_autoExposureReservedReported = true;
                            publishReserved = true;
                        }
                    }
                    if (publishReserved)
                    {
                        PublishEvent(MeyerCaptureServiceEvent_AutoExposureReserved,
                                     MeyerCaptureServiceEventSeverity_Info,
                                     exposureResult,
                                     groupInfo.groupSequence,
                                     "Automatic exposure interface is reserved and not implemented");
                    }
                }
                else
                {
                    PublishEvent(MeyerCaptureServiceEvent_ProcessingError,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 exposureResult,
                                 groupInfo.groupSequence,
                                 m_autoExposure.LastError());
                }
            }

            RunCommandWindow(validExposure, groupInfo.groupSequence);

            bool dropped = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                const std::size_t capacity =
                    static_cast<std::size_t>(m_profile.postProcessQueueCapacity);
                if (m_postQueue.size() >= capacity)
                {
                    // 已入队旧数据保留，只丢弃当前新副本，快速链路继续工作。
                    ++m_state.droppedPostProcessGroups;
                    dropped = true;
                }
                else
                {
                    PostProcessItem item;
                    item.decryptedGroup = decrypted;
                    item.groupInfo = groupInfo;
                    m_postQueue.push_back(std::move(item));
                    m_state.postQueueSize = static_cast<std::int32_t>(m_postQueue.size());
                }
                ++m_state.sequence;
            }
            if (dropped)
            {
                PublishEvent(MeyerCaptureServiceEvent_PostProcessDropped,
                             MeyerCaptureServiceEventSeverity_Warning,
                             MeyerCaptureServiceResult_QueueFull,
                             groupInfo.groupSequence,
                             "Post-process queue is full; newest group copy was dropped");
                WriteWarning("PostProcessQueue",
                             "Newest decrypted group copy dropped because the queue is full");
            }
            else
            {
                m_postCondition.notify_one();
            }
        }

        // 命令窗口严格限制两条无回包命令；第二条之前等待至少 5 ms。
        void CaptureServiceContext::RunCommandWindow(
            const MeyerAutoExposureOutput* exposureOutput,
            std::uint64_t groupSequence)
        {
            std::int32_t sentCount = 0;
            if (exposureOutput != nullptr && exposureOutput->valid != 0)
            {
                MeyerDeviceCmdExposureParameters parameters = {};
                DecodeExposureOutput(*exposureOutput, parameters);
                const std::int32_t result =
                    m_deviceCmd.SetExposureParameters(parameters);
                if (result == MeyerDeviceCmdResult_Ok)
                {
                    ++sentCount;
                }
                else
                {
                    PublishEvent(MeyerCaptureServiceEvent_CommandFailed,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 result, groupSequence,
                                 "Automatic exposure command failed at the group boundary");
                }
            }

            LightRequest request = {};
            while (sentCount < 2 && PopLightRequest(request))
            {
                if (sentCount > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                const std::int32_t result = m_deviceCmd.SetLight(request.on);
                if (result == MeyerDeviceCmdResult_Ok)
                {
                    ++sentCount;
                    PublishEvent(MeyerCaptureServiceEvent_LightCommandSent,
                                 MeyerCaptureServiceEventSeverity_Info,
                                 result, groupSequence,
                                 request.on ? "Queued light-on command sent"
                                            : "Queued light-off command sent");
                }
                else
                {
                    PublishEvent(MeyerCaptureServiceEvent_CommandFailed,
                                 MeyerCaptureServiceEventSeverity_Error,
                                 result, groupSequence, m_deviceCmd.LastError());
                }
            }
        }

        // AutoExposure 输出字段顺序与 DeviceCmd 曝光结构的 16 个协议字节一致。
        void CaptureServiceContext::DecodeExposureOutput(
            const MeyerAutoExposureOutput& output,
            MeyerDeviceCmdExposureParameters& parameters)
        {
            std::memset(&parameters, 0, sizeof(parameters));
            parameters.structSize = sizeof(parameters);
            parameters.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            parameters.camera1WhiteExposure = output.commandPayload[0];
            parameters.camera1WhiteLightTime = output.commandPayload[1];
            parameters.camera1WhiteAnalogGain = output.commandPayload[2];
            parameters.camera1WhiteDigitalGain = output.commandPayload[3];
            parameters.camera1StripeExposure = output.commandPayload[4];
            parameters.camera1StripeLightTime = output.commandPayload[5];
            parameters.camera1StripeAnalogGain = output.commandPayload[6];
            parameters.camera1StripeDigitalGain = output.commandPayload[7];
            parameters.camera2WhiteExposure = output.commandPayload[8];
            parameters.camera2WhiteLightTime = output.commandPayload[9];
            parameters.camera2WhiteAnalogGain = output.commandPayload[10];
            parameters.camera2WhiteDigitalGain = output.commandPayload[11];
            parameters.camera2StripeExposure = output.commandPayload[12];
            parameters.camera2StripeLightTime = output.commandPayload[13];
            parameters.camera2StripeAnalogGain = output.commandPayload[14];
            parameters.camera2StripeDigitalGain = output.commandPayload[15];
        }

        bool CaptureServiceContext::PopLightRequest(LightRequest& request)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_lightRequests.empty())
            {
                return false;
            }
            request = m_lightRequests.front();
            m_lightRequests.pop_front();
            return true;
        }
    }
}
