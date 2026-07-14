// 采集传输循环只负责从 DeviceSession 取原始包并补充包序号，不做组帧。
#include "../../stdafx.h"

#include "CaptureTransportLoop.h"

#include "../DeviceSession.h"
#include "../../protocol/MeyerProtocolDefs.h"
#include "../../transport/cyapi/CyApiTransport.h"

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            // 初始化为空闲状态和零统计值。
            CaptureTransportLoop::CaptureTransportLoop()
                : m_session(nullptr)
                , m_configured(false)
                , m_running(false)
                , m_packetSequence(0)
                , m_successCount(0)
                , m_failureCount(0)
                , m_partialSuccessCount(0)
            {
            }

            // 保存一份不可变采集配置快照，避免运行期间读取上层临时对象。
            void CaptureTransportLoop::Configure(const CaptureFrameConfig& config)
            {
                m_config = config;
                m_configured = config.IsValid();
            }

            // 返回内部配置快照，仅供同模块诊断使用。
            const CaptureFrameConfig& CaptureTransportLoop::GetConfig() const
            {
                return m_config;
            }

            // 表示尺寸和分包字段是否通过基本校验。
            bool CaptureTransportLoop::IsConfigured() const
            {
                return m_configured;
            }

            // 绑定会话并重置统计；真正提交异步传输由 Prime 完成。
            bool CaptureTransportLoop::Start(DeviceSession& session)
            {
                if (!m_configured)
                {
                    return false;
                }

                m_session = &session;
                m_running = false;
                m_packetSequence = 0;
                m_successCount = 0;
                m_failureCount = 0;
                m_partialSuccessCount = 0;
                return true;
            }

            // 停止底层流并将本循环置为非运行状态。
            void CaptureTransportLoop::Stop()
            {
                if (m_session != nullptr)
                {
                    m_session->StopStream();
                }
                m_running = false;
            }

            // 启动流后通过 dynamic_cast 访问 CyAPI 专用的异步队列能力。
            bool CaptureTransportLoop::Prime()
            {
                if (m_session == nullptr || !m_configured)
                {
                    return false;
                }

                if (!m_session->StartStream())
                {
                    return false;
                }

                CyApiTransport* cyApiTransport = dynamic_cast<CyApiTransport*>(m_session->GetTransport());
                if (cyApiTransport == nullptr)
                {
                    m_session->StopStream();
                    return false;
                }

                if (!cyApiTransport->PrimeStream(m_config.transferSize, m_config.queueDepth))
                {
                    m_session->StopStream();
                    return false;
                }

                m_running = true;
                return true;
            }

            // 接收一个包，并从协议头第 12 字节提取当前图像序号。
            bool CaptureTransportLoop::ReceiveNextPacket(CapturePacket& packet, bool& isPartialPacket)
            {
                packet.Clear();
                isPartialPacket = false;

                if (!m_running || m_session == nullptr)
                {
                    ++m_failureCount;
                    return false;
                }

                std::vector<Byte> bytes;
                if (!m_session->ReceiveStreamPacket(bytes, m_config.transferSize, m_config.timeoutMs))
                {
                    ++m_failureCount;
                    return false;
                }

                // swap 只交换 vector 内部指针，避免把一整个 USB 包再复制一次。
                packet.bytes.swap(bytes);
                packet.packetIndex = static_cast<int>(m_packetSequence++);
                packet.isImageHeader = protocol::IsImageHeader(packet.Data(), packet.Size());
                if (packet.isImageHeader && packet.Size() > 12)
                {
                    packet.imageIndex = static_cast<int>(packet.bytes[12]);
                }

                const std::size_t expectedPacketSize = GetExpectedPacketSize();
                if (expectedPacketSize > 0 && packet.Size() != expectedPacketSize)
                {
                    isPartialPacket = true;
                    ++m_partialSuccessCount;
                }

                ++m_successCount;
                return true;
            }

            // CyApiTransport 在每次 Receive 后自动重新提交当前槽位；此接口仅保留状态查询语义。
            bool CaptureTransportLoop::RequeueCurrentTransfer()
            {
                return m_running;
            }

            // 查询传输循环是否已经完成 Prime。
            bool CaptureTransportLoop::IsRunning() const
            {
                return m_running;
            }

            // 返回成功接收包数。
            std::size_t CaptureTransportLoop::GetSuccessCount() const
            {
                return m_successCount;
            }

            // 返回底层接收失败次数。
            std::size_t CaptureTransportLoop::GetFailureCount() const
            {
                return m_failureCount;
            }

            // 返回长度与配置不一致的包数。
            std::size_t CaptureTransportLoop::GetPartialSuccessCount() const
            {
                return m_partialSuccessCount;
            }

            // 协议负载长度优先；未设置时回退为一次 USB 传输长度。
            std::size_t CaptureTransportLoop::GetExpectedPacketSize() const
            {
                if (m_config.packetPayloadSize > 0)
                {
                    return static_cast<std::size_t>(m_config.packetPayloadSize);
                }

                return m_config.transferSize;
            }
        }
    }
}
