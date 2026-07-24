#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        // 先启动底层异步队列，再通知设备开始发图，避免第一批 B 包无人接收。
        std::int32_t DeviceCommandService::StartCapture(const MeyerDeviceCmdCaptureParams& params)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_profile || (m_profile->capabilities & MeyerDeviceCapability_Capture) == 0U)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare capture capability");
            }
            if (m_transport->IsCaptureActive())
            {
                return SetError(MeyerDeviceCmdResult_Busy, "Image capture is already active");
            }

            const std::int32_t validation = ValidateCaptureParams(params);
            if (validation != MeyerDeviceCmdResult_Ok)
            {
                return validation;
            }

            std::int32_t result = m_transport->StartCapture(params, m_profile->transportDecoderType);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            result = ExecuteCommand(protocol::StartImageTransfer,
                                    nullptr,
                                    0U,
                                    MEYER_DEVICE_CMD_NO_RESPONSE,
                                    nullptr,
                                    0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                // 命令失败时立即停掉已经建立的接收线程，不能留下半启动状态。
                m_transport->StopCapture();
                return result;
            }

            m_state.captureActive = 1;
            m_state.workMode = params.workMode;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StartCapture", "Image capture command chain started");
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCommandService::StopCapture(bool turnLightOff)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_transport->IsCaptureActive())
            {
                // 停止操作设计为幂等，页面重复释放资源不会被当成业务故障。
                m_state.captureActive = 0;
                m_state.workMode = MeyerDeviceWorkMode_Idle;
                m_state.validFields |= MeyerDeviceStateField_Capture;
                AdvanceState();
                return MeyerDeviceCmdResult_Ok;
            }

            const std::int32_t commandResult = SendStopAndOptionalLightOff(turnLightOff);
            const std::int32_t transportResult = m_transport->StopCapture();

            m_state.captureActive = 0;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StopCapture", "Image capture command chain stopped");

            // 即使命令发送失败也已经释放本地采集资源；优先返回最早的命令错误。
            if (commandResult != MeyerDeviceCmdResult_Ok)
            {
                return commandResult;
            }
            if (transportResult != MeyerDeviceCmdResult_Ok)
            {
                return SetError(transportResult, m_transport->LastError());
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 非阻塞取帧，同时把 B 类包内的灯、扫描头和温度写入状态快照。
        std::int32_t DeviceCommandService::GetFrame(unsigned char* buffer,
                                                    std::size_t capacity,
                                                    std::size_t& frameBytes,
                                                    MeyerDeviceCmdFrameInfo& frameInfo)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }

            const std::int32_t result = m_transport->GetFrame(buffer, capacity, frameBytes, frameInfo);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                if (result != MeyerDeviceCmdResult_NotReady && result != MeyerDeviceCmdResult_BufferTooSmall)
                {
                    SetError(result, m_transport->LastError());
                }
                return result;
            }

            m_state.lightRequestedOn = frameInfo.ledOn;
            m_state.captureStatus = frameInfo.captureStatus;
            m_state.scanHeadType = frameInfo.scanHeadType;
            m_state.temperature0 = frameInfo.temperature0;
            m_state.temperature1 = frameInfo.temperature1;
            m_state.temperature2 = frameInfo.temperature2;
            m_state.temperature3 = frameInfo.temperature3;
            m_state.validFields |= MeyerDeviceStateField_LightRequested |
                                   MeyerDeviceStateField_FrameTelemetry;
            AdvanceState();
            return MeyerDeviceCmdResult_Ok;
        }

        // 新采集链路的启动顺序与旧接口相同：先预提交 USB IN，再发 0x0A。
        // 不同点是 Transport 只交付原始 B 包，六图组帧由 CaptureProcessing 负责。
        std::int32_t DeviceCommandService::StartRawCapture(
            const MeyerDeviceCmdCaptureParams& params)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_profile || (m_profile->capabilities & MeyerDeviceCapability_Capture) == 0U)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare capture capability");
            }
            if (m_transport->IsCaptureActive())
            {
                return SetError(MeyerDeviceCmdResult_Busy, "A capture stream is already active");
            }

            const std::int32_t validation = ValidateCaptureParams(params);
            if (validation != MeyerDeviceCmdResult_Ok)
            {
                return validation;
            }

            std::int32_t result = m_transport->StartRawCapture(params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            result = ExecuteCommand(protocol::StartImageTransfer,
                                    nullptr,
                                    0U,
                                    MEYER_DEVICE_CMD_NO_RESPONSE,
                                    nullptr,
                                    0U);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                // 0x0A 未成功发出时立即回收 64 个异步接收槽位。
                m_transport->StopRawCapture();
                return result;
            }

            m_state.captureActive = 1;
            m_state.workMode = params.workMode;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StartRawCapture", "Raw B-packet capture chain started");
            return MeyerDeviceCmdResult_Ok;
        }

        // 停止时先让设备停止传图，再回收本地异步队列，并始终更新快照为 Idle。
        std::int32_t DeviceCommandService::StopRawCapture(bool turnLightOff)
        {
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            if (!m_transport->IsRawCaptureActive())
            {
                m_state.captureActive = 0;
                m_state.workMode = MeyerDeviceWorkMode_Idle;
                m_state.validFields |= MeyerDeviceStateField_Capture;
                AdvanceState();
                return MeyerDeviceCmdResult_Ok;
            }

            const std::int32_t commandResult = SendStopAndOptionalLightOff(turnLightOff);
            const std::int32_t transportResult = m_transport->StopRawCapture();
            m_state.captureActive = 0;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields |= MeyerDeviceStateField_Capture;
            AdvanceState();
            logging::WriteInfo("StopRawCapture", "Raw B-packet capture chain stopped");

            if (commandResult != MeyerDeviceCmdResult_Ok)
            {
                return commandResult;
            }
            if (transportResult != MeyerDeviceCmdResult_Ok)
            {
                return SetError(transportResult, m_transport->LastError());
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 这里不解析图像头，也不把单次 Timeout 写成高级别错误日志。
        // CaptureService 根据连续次数和诊断快照决定是否进入 Faulted。
        std::int32_t DeviceCommandService::ReceiveRawCapturePacket(
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& receivedSize,
            std::uint32_t timeoutMs)
        {
            receivedSize = 0U;
            if (!IsOpen())
            {
                return MeyerDeviceCmdResult_DeviceDisconnected;
            }
            if (!m_transport->IsRawCaptureActive())
            {
                return MeyerDeviceCmdResult_NotReady;
            }
            if (buffer == nullptr || capacity == 0U)
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Raw capture packet buffer is empty");
            }

            const std::int32_t result = m_transport->ReceiveRawCapturePacket(
                buffer, capacity, receivedSize, timeoutMs);
            if (result == MeyerDeviceCmdResult_StreamStalled ||
                result == MeyerDeviceCmdResult_DeviceDisconnected ||
                result == MeyerDeviceCmdResult_IoFailed)
            {
                SetError(result, m_transport->LastError());
            }
            return result;
        }

        // 诊断快照是内存复制，不会占用 USB 命令窗口。
        std::int32_t DeviceCommandService::GetStreamDiagnostics(
            MeyerDeviceCmdStreamDiagnostics& diagnostics)
        {
            // 即使设备已拔出，Transport 仍可能保留最后一份诊断，
            // 因此本函数不先用 IsOpen 拦截，而是尽量复制末次快照。
            const std::int32_t result = m_transport->GetStreamDiagnostics(diagnostics);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // IRawDeviceTransport::IsOpen 最终调用 CyAPI IsOpen，它不发送 A 类命令，适合每组开始前调用。
        bool DeviceCommandService::IsDeviceConnectedLightweight() const
        {
            return m_transport.get() != nullptr && m_transport->IsOpen();
        }

    }
}
