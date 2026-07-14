#include "../stdafx.h"

#include "DeviceSession.h"

#include "../model/ImageFrame.h"
#include "../protocol/MeyerProtocolDefs.h"
#include "../transport/TransportFactory.h"
#include "../transport/cyapi/CyApiTransport.h"

#include <algorithm>
#include <cstring>

namespace meyer
{
    namespace device
    {
        // 默认会话不创建传输对象，Open 时由工厂按类型懒创建。
        DeviceSession::DeviceSession()
        {
        }

        // 测试或扩展实现可直接注入一个传输对象，所有权移入会话。
        DeviceSession::DeviceSession(std::unique_ptr<ITransport> transport)
            : m_transport(std::move(transport))
        {
        }

        // 析构统一关闭连接，防止上层遗漏 Close。
        DeviceSession::~DeviceSession()
        {
            Close();
        }

        // 替换传输前关闭旧连接；比较裸指针可避免把同一对象重复移入。
        void DeviceSession::SetTransport(std::unique_ptr<ITransport> transport)
        {
            if (m_transport.get() == transport.get())
            {
                return;
            }

            Close();
            m_transport = std::move(transport);
        }

        // 返回非拥有指针，生命周期仍由 unique_ptr 管理。
        ITransport* DeviceSession::GetTransport()
        {
            return m_transport.get();
        }

        // const 查询版本不允许调用方修改传输实现。
        const ITransport* DeviceSession::GetTransport() const
        {
            return m_transport.get();
        }

        // 查询工厂是否已经创建传输对象。
        bool DeviceSession::HasTransport() const
        {
            return m_transport.get() != nullptr;
        }

        // 无对象时返回 Unknown，调用方无需解引用空指针。
        TransportType DeviceSession::GetTransportType() const
        {
            if (m_transport.get() == nullptr)
            {
                return TransportType::Unknown;
            }

            return m_transport->GetType();
        }

        // 确保传输类型匹配后，用配置打开底层设备。
        bool DeviceSession::Open(const TransportConfig& config)
        {
            // 重复打开前先释放旧设备，避免 CyAPI 同时保留两个底层句柄。
            Close();
            if (!EnsureTransport(config))
            {
                return false;
            }

            m_lastConfig = config;
            return m_transport->Open(config);
        }

        // 关闭当前传输；允许无对象和重复调用。
        void DeviceSession::Close()
        {
            if (m_transport.get() != nullptr)
            {
                m_transport->Close();
            }
        }

        // 把真实连接状态委托给具体传输实现。
        bool DeviceSession::IsOpen() const
        {
            if (m_transport.get() == nullptr)
            {
                return false;
            }

            return m_transport->IsOpen();
        }

        // 请求具体传输恢复最近连接。
        bool DeviceSession::Reconnect()
        {
            if (m_transport.get() == nullptr)
            {
                return false;
            }

            return m_transport->Reconnect();
        }

        // 发送原始命令，不在传输模块解释命令业务字段。
        bool DeviceSession::SendCommand(const Byte* data, std::size_t size, std::uint32_t timeoutMs)
        {
            if (m_transport.get() == nullptr || data == nullptr || size == 0U)
            {
                return false;
            }

            return m_transport->SendCommand(data, size, ResolveCommandTimeout(timeoutMs));
        }

        // 先按调用方容量分配，再按实际接收长度缩小 vector。
        bool DeviceSession::ReceiveCommand(std::vector<Byte>& response, std::size_t capacity, std::uint32_t timeoutMs)
        {
            response.clear();

            if (m_transport.get() == nullptr || capacity == 0)
            {
                return false;
            }

            response.resize(capacity, 0);
            std::size_t receivedSize = 0;
            if (!m_transport->ReceiveCommand(&response[0], capacity, receivedSize, ResolveCommandTimeout(timeoutMs)))
            {
                response.clear();
                return false;
            }

            response.resize(receivedSize);
            return true;
        }

        // 启动原始数据流。
        bool DeviceSession::StartStream()
        {
            if (m_transport.get() == nullptr)
            {
                return false;
            }

            return m_transport->StartStream();
        }

        // 停止原始数据流，允许重复调用。
        void DeviceSession::StopStream()
        {
            if (m_transport.get() != nullptr)
            {
                m_transport->StopStream();
            }
        }

        // 接收一包并按实际长度收缩临时缓冲区。
        bool DeviceSession::ReceiveStreamPacket(std::vector<Byte>& packet, std::size_t capacity, std::uint32_t timeoutMs)
        {
            packet.clear();

            if (m_transport.get() == nullptr || capacity == 0)
            {
                return false;
            }

            packet.resize(capacity, 0);
            std::size_t receivedSize = 0;
            if (!m_transport->ReceiveStreamPacket(&packet[0], capacity, receivedSize, ResolveStreamTimeout(timeoutMs)))
            {
                packet.clear();
                return false;
            }

            packet.resize(receivedSize);
            return true;
        }

        // 类型变化时销毁旧实现并通过工厂创建新实现。
        bool DeviceSession::EnsureTransport(const TransportConfig& config)
        {
            const bool needsNewTransport =
                (m_transport.get() == nullptr) ||
                (m_transport->GetType() != config.type);

            if (!needsNewTransport)
            {
                return true;
            }

            Close();
            m_transport = TransportFactory::Create(config.type);
            return m_transport.get() != nullptr;
        }

        // 0 表示使用 Open 参数中的命令超时。
        std::uint32_t DeviceSession::ResolveCommandTimeout(std::uint32_t timeoutMs) const
        {
            if (timeoutMs != 0)
            {
                return timeoutMs;
            }

            return m_lastConfig.commandTimeoutMs;
        }

        // 0 表示使用 Open 参数中的流超时。
        std::uint32_t DeviceSession::ResolveStreamTimeout(std::uint32_t timeoutMs) const
        {
            if (timeoutMs != 0)
            {
                return timeoutMs;
            }

            return m_lastConfig.streamTimeoutMs;
        }

        // 初始化协议解析状态和默认扫描头类型。
        DeviceFacade::DeviceFacade()
            : m_deviceType(DeviceType::Unknown)
            , m_pictureOrderMode(PictureOrderMode::Old)
            , m_captureScanMode(CaptureScanMode::Scan)
            , m_ahrsEnabled(false)
            , m_gyroscopePauseFlag(false)
            , m_resetImuReferenceRequested(false)
            , m_scanHeadType(3)
            , m_captureActive(false)
        {
        }

        // 服务析构时停止采集并关闭会话。
        DeviceFacade::~DeviceFacade()
        {
            Close();
        }

        // 对外编排层把打开动作转发给会话。
        bool DeviceFacade::Open(const TransportConfig& config)
        {
            return m_session.Open(config);
        }

        // 先停止可能访问会话的采集线程，再关闭底层连接。
        void DeviceFacade::Close()
        {
            StopCapture();
            m_session.Close();
        }

        // 查询底层连接状态。
        bool DeviceFacade::IsOpen() const
        {
            return m_session.IsOpen();
        }

        // 请求会话重连。
        bool DeviceFacade::Reconnect()
        {
            return m_session.Reconnect();
        }

        // 校验原始缓冲区后发送；命令编码由独立 DeviceCmd 模块负责。
        bool DeviceFacade::SendRawCommand(const Byte* cmdSend, std::size_t size, std::uint32_t timeoutMs)
        {
            if (cmdSend == nullptr || size == 0)
            {
                return false;
            }

            return m_session.SendCommand(cmdSend, size, timeoutMs);
        }

        // 使用内部 vector 接收，再复制到调用方拥有的缓冲区。
        bool DeviceFacade::ReceiveCommand(Byte* cmdReceive, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            std::vector<Byte> response;
            receivedSize = 0;

            if (cmdReceive == nullptr || capacity == 0)
            {
                return false;
            }

            if (!m_session.ReceiveCommand(response, capacity, timeoutMs))
            {
                return false;
            }

            receivedSize = response.size();
            if (receivedSize > 0)
            {
                memcpy(cmdReceive, &response[0], receivedSize);
            }

            return true;
        }

        // 启动未组帧的原始设备流。
        bool DeviceFacade::StartStream()
        {
            return m_session.StartStream();
        }

        // 当前只有 CyAPI 实现支持预提交队列，因此安全向下转换后调用专用能力。
        bool DeviceFacade::PrimeStream(std::size_t transferSize, std::size_t queueDepth)
        {
            CyApiTransport* transport = dynamic_cast<CyApiTransport*>(m_session.GetTransport());
            if (transport == nullptr)
            {
                return false;
            }

            return transport->PrimeStream(transferSize, queueDepth);
        }

        void DeviceFacade::StopStream()
        {
            // 完整采集依赖原始流，先等待采集线程退出再回收流资源。
            StopCapture();
            m_session.StopStream();
        }

        // 接收原始包并复制到 ABI 缓冲区。
        bool DeviceFacade::ReceiveStreamPacket(Byte* packetBuffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            std::vector<Byte> packet;
            receivedSize = 0;

            if (packetBuffer == nullptr || capacity == 0)
            {
                return false;
            }

            if (!m_session.ReceiveStreamPacket(packet, capacity, timeoutMs))
            {
                return false;
            }

            receivedSize = packet.size();
            if (receivedSize > 0)
            {
                memcpy(packetBuffer, &packet[0], receivedSize);
            }

            return true;
        }

        // 从 CyAPI 实现读取最近一次枚举数量。
        int DeviceFacade::GetDeviceCount() const
        {
            const CyApiTransport* transport = dynamic_cast<const CyApiTransport*>(m_session.GetTransport());
            if (transport == nullptr)
            {
                return 0;
            }

            return transport->GetDeviceCount();
        }

        // 查询当前设备速度类型。
        bool DeviceFacade::GetIsUSB2() const
        {
            const CyApiTransport* transport = dynamic_cast<const CyApiTransport*>(m_session.GetTransport());
            if (transport == nullptr)
            {
                return false;
            }

            return transport->IsUsb2();
        }

        // 设置后续帧解析使用的设备型号。
        void DeviceFacade::SetDeviceType(DeviceType deviceType)
        {
            m_deviceType = deviceType;
        }

        DeviceType DeviceFacade::GetDeviceType() const
        {
            return m_deviceType;
        }

        // 设置后续平面重排模式。
        void DeviceFacade::SetPictureOrderMode(PictureOrderMode mode)
        {
            m_pictureOrderMode = mode;
        }

        PictureOrderMode DeviceFacade::GetPictureOrderMode() const
        {
            return m_pictureOrderMode;
        }

        // 设置帧元数据中的采集用途。
        void DeviceFacade::SetCaptureScanMode(CaptureScanMode mode)
        {
            m_captureScanMode = mode;
        }

        CaptureScanMode DeviceFacade::GetCaptureScanMode() const
        {
            return m_captureScanMode;
        }

        // 设置启动采集时是否解算 IMU 姿态。
        void DeviceFacade::SetAhrsEnabled(bool enabled)
        {
            m_ahrsEnabled = enabled;
        }

        bool DeviceFacade::GetAhrsEnabled() const
        {
            return m_ahrsEnabled;
        }

        // 同时更新下次启动默认值和当前采集线程的原子状态。
        void DeviceFacade::SetImuPaused(bool paused)
        {
            m_gyroscopePauseFlag = paused;
            m_pureCaptureEngine.SetImuPaused(paused);
        }

        // 同时向当前采集线程投递一次性重置请求。
        void DeviceFacade::RequestImuReferenceReset()
        {
            m_resetImuReferenceRequested = true;
            m_pureCaptureEngine.RequestImuReferenceReset();
        }

        // 保存尚未收到帧时的扫描头默认值。
        void DeviceFacade::SetScanHeadType(int scanHeadType)
        {
            m_scanHeadType = scanHeadType;
        }

        // 有完整帧时优先返回协议实际值，否则返回配置默认值。
        int DeviceFacade::GetScanHeadType() const
        {
            if (!m_lastPureFrame.Empty())
            {
                return m_lastPureFrame.status.scanHeadType;
            }

            return m_scanHeadType;
        }

        // 校验完整配置，停止旧采集后启动唯一正式采集后端。
        bool DeviceFacade::StartCapture(const CaptureConfig& config)
        {
            if (!m_session.IsOpen())
            {
                return false;
            }

            if (config.width <= 0 || config.height <= 0 || config.imageCount <= 0 ||
                config.packetsPerImage <= 0 || config.transferSize == 0 || config.queueDepth == 0 ||
                config.packetPayloadSize <= 0)
            {
                return false;
            }

            StopCapture();
            m_captureConfig = config;
            m_lastPureFrame.Clear();

            return StartPureCaptureBackend(config);
        }

        // 等待工作线程退出并清空最近帧。
        void DeviceFacade::StopCapture()
        {
            m_pureCaptureEngine.Stop();

            m_captureActive = false;
            m_lastPureFrame.Clear();
        }

        // 同时检查编排标志和工作线程，线程异常退出后不会误报运行中。
        bool DeviceFacade::IsCaptureActive() const
        {
            return m_captureActive && m_pureCaptureEngine.IsRunning();
        }

        // 从有界队列取最旧完整帧。
        bool DeviceFacade::GetFrame(ImageFrame& frame)
        {
            frame.Clear();

            if (!IsCaptureActive())
            {
                return false;
            }

            return TryGetPureFrame(frame);
        }

        // 返回最近交付帧的四态采集状态。
        int DeviceFacade::GetCaptureStatus() const
        {
            return static_cast<int>(m_lastPureFrame.status.captureStatus);
        }

        // 返回最近帧的三组曝光和增益；无帧时输出保持为 0。
        bool DeviceFacade::GetExposureState(int& timeW, int& timeC, int& timeX, int& gainW, int& gainC, int& gainX) const
        {
            timeW = 0;
            timeC = 0;
            timeX = 0;
            gainW = 0;
            gainC = 0;
            gainX = 0;

            if (!m_lastPureFrame.Empty())
            {
                timeW = m_lastPureFrame.status.timeW;
                timeC = m_lastPureFrame.status.timeC;
                timeX = m_lastPureFrame.status.timeX;
                gainW = m_lastPureFrame.status.gainW;
                gainC = m_lastPureFrame.status.gainC;
                gainX = m_lastPureFrame.status.gainX;
                return true;
            }

            return false;
        }

        // 返回最近帧温度字段；协议尚未提供数据时仍为初始化值 0。
        bool DeviceFacade::GetTemperatureState(int& temperature0, int& temperature1, int& temperature2, int& temperature3) const
        {
            temperature0 = 0;
            temperature1 = 0;
            temperature2 = 0;
            temperature3 = 0;

            if (!m_lastPureFrame.Empty())
            {
                temperature0 = m_lastPureFrame.status.temperature0;
                temperature1 = m_lastPureFrame.status.temperature1;
                temperature2 = m_lastPureFrame.status.temperature2;
                temperature3 = m_lastPureFrame.status.temperature3;
                return true;
            }

            return false;
        }

        // 把每个平面的 primary/secondary/gain 依次展平到整数数组。
        bool DeviceFacade::GetThreeGainTime(std::vector<int>& expoValues) const
        {
            expoValues.clear();

            if (!m_lastPureFrame.Empty())
            {
                if (m_lastPureFrame.planeInfos.empty())
                {
                    return false;
                }

                const std::size_t planeCount = m_lastPureFrame.planeInfos.size();
                for (std::size_t planeIndex = 0; planeIndex < planeCount; ++planeIndex)
                {
                    expoValues.push_back(m_lastPureFrame.planeInfos[planeIndex].exposure.primary);
                    expoValues.push_back(m_lastPureFrame.planeInfos[planeIndex].exposure.secondary);
                    expoValues.push_back(m_lastPureFrame.planeInfos[planeIndex].exposure.analogGain);
                }

                return !expoValues.empty();
            }

            return false;
        }

        // 返回 roll/pitch/yaw 和 w/x/y/z 七个兼容值。
        bool DeviceFacade::GetGyroscopeData(std::vector<float>& gyroValues) const
        {
            gyroValues.clear();

            if (!m_lastPureFrame.Empty())
            {
                if (!m_lastPureFrame.imu.valid)
                {
                    return false;
                }

                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.roll));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.pitch));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.yaw));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.quatW));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.quatX));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.quatY));
                gyroValues.push_back(static_cast<float>(m_lastPureFrame.imu.quatZ));
                return true;
            }

            return false;
        }

        DeviceSession& DeviceFacade::GetSession()
        {
            return m_session;
        }

        const DeviceSession& DeviceFacade::GetSession() const
        {
            return m_session;
        }

        // 将编排配置复制为采集引擎快照，并推导最后一包有效长度。
        bool DeviceFacade::StartPureCaptureBackend(const CaptureConfig& config)
        {
            CaptureFrameConfig pureConfig;
            pureConfig.width = config.width;
            pureConfig.height = config.height;
            pureConfig.imageCount = config.imageCount;
            pureConfig.packetsPerImage = config.packetsPerImage;
            pureConfig.transferSize = config.transferSize;
            pureConfig.queueDepth = config.queueDepth;
            pureConfig.packetPayloadSize = config.packetPayloadSize;
            pureConfig.lastPacketValidSize = config.lastPacketValidSize;
            pureConfig.timeoutMs = config.timeoutMs;
            pureConfig.maxReadyFrames = config.maxReadyFrames;
            pureConfig.deviceType = m_deviceType;
            pureConfig.pictureOrderMode = m_pictureOrderMode;
            pureConfig.scanMode = m_captureScanMode;
            pureConfig.ahrsEnabled = m_ahrsEnabled;
            pureConfig.gyroscopePauseFlag = m_gyroscopePauseFlag;
            pureConfig.resetImuReferenceRequested = m_resetImuReferenceRequested;
            // 启动配置已经接收本次重置请求，避免下一次启动再次重复消费。
            m_resetImuReferenceRequested = false;

            if (pureConfig.lastPacketValidSize <= 0)
            {
                // 用 64 位中间值推导末包长度，避免 int 乘法在错误内部调用下回绕。
                const std::int64_t planeBytes =
                    static_cast<std::int64_t>(pureConfig.width) * pureConfig.height;
                const std::int64_t bytesBeforeLastPacket =
                    static_cast<std::int64_t>(pureConfig.packetPayloadSize) *
                    (pureConfig.packetsPerImage - 1);
                const std::int64_t lastPacketBytes = planeBytes - bytesBeforeLastPacket;
                if (lastPacketBytes <= 0 ||
                    lastPacketBytes > pureConfig.packetPayloadSize)
                {
                    return false;
                }
                pureConfig.lastPacketValidSize = static_cast<int>(lastPacketBytes);
            }

            if (pureConfig.lastPacketValidSize <= 0)
            {
                return false;
            }

            m_pureCaptureEngine.Configure(pureConfig);
            if (!m_pureCaptureEngine.Start(m_session))
            {
                return false;
            }

            m_captureActive = true;
            return true;
        }

        // 立即尝试从有界队列取帧。等待策略属于 UI/算法调用方的事件循环；传输
        // DLL 在公开调用中休眠会阻塞界面线程，也会让 NotReady 语义失真。
        bool DeviceFacade::TryGetPureFrame(ImageFrame& frame)
        {
            if (!m_pureCaptureEngine.IsRunning() ||
                m_pureCaptureEngine.GetReadyFrameCount() == 0U)
            {
                return false;
            }

            if (!m_pureCaptureEngine.GetFrame(frame))
            {
                return false;
            }

            FillPureFrameRuntimeStatus(frame);
            m_lastPureFrame = frame;
            return true;
        }

        // 用读取时的真实连接状态补充工作线程生成的帧。
        void DeviceFacade::FillPureFrameRuntimeStatus(ImageFrame& frame) const
        {
            frame.status.isOpen = m_session.IsOpen();
            frame.status.isUsb2 = GetIsUSB2();
            frame.status.deviceType = m_deviceType;
        }

    }
}

