#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        // 构造阶段只初始化公共状态结构，不加载 DLL 或访问设备，便于先创建句柄再配置参数。
        DeviceCommandService::DeviceCommandService()
            : m_profile(nullptr), m_waitBeforeNextCommand(false)
        {
            std::memset(&m_lastOpenParams, 0, sizeof(m_lastOpenParams));
            std::memset(&m_state, 0, sizeof(m_state));
            m_state.structSize = sizeof(MeyerDeviceStateSnapshot);
            m_state.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
        }

        // 析构统一复用 Close，保证正常关闭和异常销毁走同一套资源释放顺序。
        DeviceCommandService::~DeviceCommandService()
        {
            Close();
        }

        // 选择后端、建立连接并初始化不会触发长命令查询的基础状态。
        std::int32_t DeviceCommandService::Open(const MeyerDeviceCmdOpenParams& params)
        {
            Close();

            m_profile = DeviceModelCatalog::Find(params.modelHint);
            if (m_profile == nullptr)
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Device model is not registered in DeviceModelCatalog");
            }

            if (params.backendType == MeyerDeviceCmdBackend_DeviceTransport)
            {
                m_transport.reset(new DeviceTransportLibrary());
            }
            else if (params.backendType == MeyerDeviceCmdBackend_SimulatorForTest)
            {
                m_transport.reset(new SimulatedDeviceTransport());
            }
            else
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Device command backend type is invalid");
            }

            ResetStateForModel(*m_profile);
            const std::int32_t result = m_transport->Open(params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                m_state.connectionState = MeyerDeviceConnectionState_Faulted;
                m_state.validFields |= MeyerDeviceStateField_Connection;
                AdvanceState();
                return SetError(result, m_transport->LastError());
            }

            // 保存完整 POD 副本供 Reconnect 使用，不持有调用方缓冲区指针。
            m_lastOpenParams = params;
            m_state.connectionState = MeyerDeviceConnectionState_Open;
            m_state.validFields |= MeyerDeviceStateField_Connection;

            std::int32_t deviceCount = 0;
            if (m_transport->GetDeviceCount(deviceCount) == MeyerDeviceCmdResult_Ok)
            {
                m_state.deviceCount = deviceCount;
            }
            std::int32_t isUsb2 = 0;
            if (m_transport->GetIsUsb2(isUsb2) == MeyerDeviceCmdResult_Ok)
            {
                m_state.isUsb2 = isUsb2;
                m_state.validFields |= MeyerDeviceStateField_UsbSpeed;
            }

            AdvanceState();
            m_lastError.clear();
            logging::WriteInfo("Open", "Device command session opened");
            return MeyerDeviceCmdResult_Ok;
        }

        // Close 不下发业务命令，确保异常退出时优先回收线程和 USB 资源。
        std::int32_t DeviceCommandService::Close()
        {
            if (m_transport)
            {
                if (m_transport->IsCaptureActive())
                {
                    m_transport->StopCapture();
                }
                m_transport->Close();
                m_transport.reset();
            }

            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.captureActive = 0;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields |= MeyerDeviceStateField_Connection | MeyerDeviceStateField_Capture;
            AdvanceState();
            m_lastError.clear();
            m_waitBeforeNextCommand = false;
            return MeyerDeviceCmdResult_Ok;
        }

        // 连接状态直接委托给当前 Transport，避免缓存值与真实 USB 状态不一致。
        bool DeviceCommandService::IsOpen() const
        {
            return m_transport && m_transport->IsOpen();
        }

        std::int32_t DeviceCommandService::Reconnect()
        {
            if (!m_transport)
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device command session is not initialized");
            }

            const std::int32_t result = m_transport->Reconnect();
            m_state.connectionState = result == MeyerDeviceCmdResult_Ok
                ? MeyerDeviceConnectionState_Open
                : MeyerDeviceConnectionState_Faulted;
            m_state.validFields |= MeyerDeviceStateField_Connection;
            AdvanceState();
            if (result != MeyerDeviceCmdResult_Ok)
            {
                return SetError(result, m_transport->LastError());
            }

            m_lastError.clear();
            m_waitBeforeNextCommand = false;
            logging::WriteInfo("Reconnect", "Device command session reconnected");
            return MeyerDeviceCmdResult_Ok;
        }


        // 复制 DeviceTransport 的关键内存保护规则，使模拟后端和真实后端行为一致。
        std::int32_t DeviceCommandService::ValidateCaptureParams(const MeyerDeviceCmdCaptureParams& params)
        {
            if (params.workMode < MeyerDeviceWorkMode_Scan ||
                params.workMode > MeyerDeviceWorkMode_CalibrationColor ||
                params.width <= 0 || params.height <= 0 || params.imageCount <= 0 ||
                params.packetsPerImage <= 0 || params.transferSize == 0U ||
                params.queueDepth == 0U || params.packetPayloadSize <= 0 ||
                params.lastPacketValidSize <= 0 ||
                params.lastPacketValidSize > params.packetPayloadSize ||
                params.timeoutMs == 0U || params.maxReadyFrames == 0U)
            {
                return SetError(
                    MeyerDeviceCmdResult_InvalidArgument,
                    "Capture parameters contain zero or out-of-range fields");
            }

            const std::uint64_t planeBytes =
                static_cast<std::uint64_t>(params.width) * static_cast<std::uint64_t>(params.height);
            const std::uint64_t bytesBeforeLast =
                static_cast<std::uint64_t>(params.packetPayloadSize) *
                static_cast<std::uint64_t>(params.packetsPerImage - 1);
            if (planeBytes <= bytesBeforeLast ||
                planeBytes - bytesBeforeLast != static_cast<std::uint64_t>(params.lastPacketValidSize) ||
                planeBytes * static_cast<std::uint64_t>(params.imageCount) > 256ULL * 1024ULL * 1024ULL)
            {
                return SetError(
                    MeyerDeviceCmdResult_InvalidArgument,
                    "Capture packet geometry does not match the image plane size");
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 封装旧代码中散落的机型 if/else；调用方只选择是否关灯。
        std::int32_t DeviceCommandService::SendStopAndOptionalLightOff(bool turnLightOff)
        {
            const auto sendStop = [this]() {
                return ExecuteCommand(protocol::StopImageTransfer,
                                      nullptr,
                                      0U,
                                      MEYER_DEVICE_CMD_NO_RESPONSE,
                                      nullptr,
                                      0U);
            };

            if (!turnLightOff)
            {
                return sendStop();
            }

            std::int32_t firstResult = MeyerDeviceCmdResult_Ok;
            if (m_profile->stopSequence == StopSequence::StopThenLightOff)
            {
                firstResult = sendStop();
                std::this_thread::sleep_for(std::chrono::milliseconds(m_profile->stopCommandDelayMs));
                const std::int32_t lightResult = SetLight(false);
                if (firstResult == MeyerDeviceCmdResult_Ok)
                {
                    firstResult = lightResult;
                }
            }
            else
            {
                firstResult = SetLight(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(m_profile->stopCommandDelayMs));
                const std::int32_t stopResult = sendStop();
                if (firstResult == MeyerDeviceCmdResult_Ok)
                {
                    firstResult = stopResult;
                }
            }
            return firstResult;
        }

        std::int32_t DeviceCommandService::SetError(std::int32_t result,
                                                    const std::string& message)
        {
            m_lastError = message.empty() ? "Unknown device command error" : message;
            logging::WriteError("DeviceCommandFailed", m_lastError.c_str());
            return result;
        }

        void DeviceCommandService::AdvanceState()
        {
            ++m_state.sequence;
        }

        // 切换型号时清除上一个设备的全部动态信息，保留公共结构头和递增序号。
        void DeviceCommandService::ResetStateForModel(const DeviceModelProfile& profile)
        {
            const std::uint64_t nextSequence = m_state.sequence + 1U;
            std::memset(&m_state, 0, sizeof(m_state));
            m_state.structSize = sizeof(MeyerDeviceStateSnapshot);
            m_state.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            m_state.sequence = nextSequence;
            m_state.model = profile.model;
            m_state.protocolFamily = profile.protocolFamily;
            // Unknown 是探测期最小配置，不得被标成宿主已确认型号。
            m_state.modelSource = profile.model == MeyerDeviceModel_Unknown
                ? MeyerDeviceModelSource_Unknown
                : MeyerDeviceModelSource_HostHint;
            m_state.capabilities = profile.capabilities;
            m_state.connectionState = MeyerDeviceConnectionState_Closed;
            m_state.workMode = MeyerDeviceWorkMode_Idle;
            m_state.validFields = MeyerDeviceStateField_Connection |
                                  MeyerDeviceStateField_Capture;
            if (profile.model != MeyerDeviceModel_Unknown)
            {
                m_state.validFields |= MeyerDeviceStateField_Model;
                CopyText(m_state.modelNameUtf8, std::string(profile.modelName));
            }
        }

        // 把设备信息解析出的型号应用到当前会话，同时保留刚刚读取的设备编号、
        // 期限和 USB 状态。与 ResetStateForModel 不同，本函数不能清空动态字段。
        bool DeviceCommandService::ApplyDetectedModel(std::int32_t model,
                                                      std::int32_t source)
        {
            const DeviceModelProfile* detectedProfile = DeviceModelCatalog::Find(model);
            if (detectedProfile == nullptr || model == MeyerDeviceModel_Unknown)
            {
                return false;
            }

            m_profile = detectedProfile;
            m_lastOpenParams.modelHint = model;
            m_state.model = model;
            m_state.protocolFamily = detectedProfile->protocolFamily;
            m_state.modelSource = source;
            m_state.capabilities = detectedProfile->capabilities;
            m_state.validFields |= MeyerDeviceStateField_Model;
            CopyText(m_state.modelNameUtf8, std::string(detectedProfile->modelName));
            AdvanceState();
            return true;
        }
    }
}
