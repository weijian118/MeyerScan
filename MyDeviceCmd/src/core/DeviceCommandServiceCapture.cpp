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

    }
}
