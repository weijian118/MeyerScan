// 根据图像头序号和固定分包数量，把 USB 包拼成图像平面和完整扫描帧。
#include "../../stdafx.h"

#include "FrameSyncAssembler.h"

#include "../../protocol/MeyerProtocolDefs.h"

#include <algorithm>
#include <cstring>

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            // 初始化同步状态机；尚未收到 imageIndex=0 时不接受后续包。
            FrameSyncAssembler::FrameSyncAssembler()
                : m_configured(false)
                , m_frameStarted(false)
                , m_imageSynchronized(false)
                , m_currentImageIndex(-1)
                , m_currentPacketIndex(0)
                , m_completedImageCount(0)
                , m_syncErrorCount(0)
            {
            }

            // 保存配置并一次性分配工作缓冲区，热路径不反复扩容。
            void FrameSyncAssembler::Configure(const CaptureFrameConfig& config)
            {
                m_config = config;
                m_configured = config.IsValid();
                ResetWorkingBuffers();
            }

            // 返回当前组帧配置快照。
            const CaptureFrameConfig& FrameSyncAssembler::GetConfig() const
            {
                return m_config;
            }

            // 查询组帧配置是否有效。
            bool FrameSyncAssembler::IsConfigured() const
            {
                return m_configured;
            }

            // 清空同步状态和工作缓冲区，供重启采集时复用对象。
            void FrameSyncAssembler::Reset()
            {
                ResetWorkingBuffers();
            }

            // 将一个有序包推进状态机；只有 FrameCompleted 时 completedFrame 才有效。
            FrameSyncResult FrameSyncAssembler::PushPacket(const CapturePacket& packet, ImageFrame& completedFrame)
            {
                completedFrame.Clear();

                if (!m_configured || packet.Empty())
                {
                    return FrameSyncResult::Error;
                }

                if (packet.isImageHeader)
                {
                    if (!m_frameStarted)
                    {
                        if (packet.imageIndex != 0)
                        {
                            ++m_syncErrorCount;
                            ResetCurrentFrameState();
                            return FrameSyncResult::SyncLost;
                        }

                        m_frameStarted = true;
                        m_completedImageCount = 0;
                        m_workingFrame.Clear();
                        m_workingFrame.width = m_config.width;
                        m_workingFrame.height = m_config.height;
                        m_workingFrame.imageCount = m_config.imageCount;
                        m_workingFrame.deviceType = m_config.deviceType;
                        // 以配置尺寸预分配整帧，避免每个平面完成时扩大 vector。
                        m_workingFrame.pixels.assign(m_frameBuffer.size(), 0);
                        m_workingFrame.planeInfos.assign(static_cast<std::size_t>(m_config.imageCount), ImagePlaneInfo());
                    }
                    else if (packet.imageIndex != m_completedImageCount)
                    {
                        ++m_syncErrorCount;
                        ResetCurrentFrameState();

                        if (packet.imageIndex != 0)
                        {
                            return FrameSyncResult::SyncLost;
                        }

                        m_frameStarted = true;
                        m_workingFrame.Clear();
                        m_workingFrame.width = m_config.width;
                        m_workingFrame.height = m_config.height;
                        m_workingFrame.imageCount = m_config.imageCount;
                        m_workingFrame.deviceType = m_config.deviceType;
                        m_workingFrame.pixels.assign(m_frameBuffer.size(), 0);
                        m_workingFrame.planeInfos.assign(static_cast<std::size_t>(m_config.imageCount), ImagePlaneInfo());
                    }

                    m_imageSynchronized = true;
                    m_currentImageIndex = packet.imageIndex;
                    m_currentPacketIndex = 0;
                    std::fill(m_currentImageBuffer.begin(), m_currentImageBuffer.end(), static_cast<Byte>(0));
                }
                else if (!m_imageSynchronized || !m_frameStarted || m_currentImageIndex < 0)
                {
                    ++m_syncErrorCount;
                    return FrameSyncResult::SyncLost;
                }

                const std::size_t planeSize =
                    static_cast<std::size_t>(m_config.width) * static_cast<std::size_t>(m_config.height);
                const std::size_t packetOffset = static_cast<std::size_t>(m_currentPacketIndex) * static_cast<std::size_t>(m_config.packetPayloadSize);
                if (packetOffset >= planeSize)
                {
                    ++m_syncErrorCount;
                    ResetCurrentFrameState();
                    return FrameSyncResult::Error;
                }

                std::size_t bytesToCopy = packet.Size();
                if (m_currentPacketIndex == m_config.packetsPerImage - 1 && m_config.lastPacketValidSize > 0)
                {
                    bytesToCopy = static_cast<std::size_t>(m_config.lastPacketValidSize);
                }

                bytesToCopy = (std::min)(bytesToCopy, planeSize - packetOffset);
                if (bytesToCopy == 0)
                {
                    ++m_syncErrorCount;
                    ResetCurrentFrameState();
                    return FrameSyncResult::Error;
                }

                // packetOffset 已经通过 planeSize 上界校验，可以安全复制当前有效负载。
                memcpy(&m_currentImageBuffer[packetOffset], packet.Data(), bytesToCopy);
                ++m_currentPacketIndex;

                if (m_currentPacketIndex < m_config.packetsPerImage)
                {
                    return FrameSyncResult::NeedMorePackets;
                }

                return HandleImageCompletion(completedFrame);
            }

            // 返回当前正在组装的图像序号，未同步时为 -1。
            int FrameSyncAssembler::GetCurrentImageIndex() const
            {
                return m_currentImageIndex;
            }

            // 返回当前平面已经接收的包数。
            int FrameSyncAssembler::GetCurrentPacketIndex() const
            {
                return m_currentPacketIndex;
            }

            // 表示是否已经收到当前平面的合法头包。
            bool FrameSyncAssembler::IsImageSynchronized() const
            {
                return m_imageSynchronized;
            }

            // 返回本轮配置以来发生的同步错误数。
            int FrameSyncAssembler::GetSyncErrorCount() const
            {
                return m_syncErrorCount;
            }

            // 按 width*height*imageCount 重新分配平面和整帧缓冲区。
            void FrameSyncAssembler::ResetWorkingBuffers()
            {
                m_syncErrorCount = 0;
                m_workingFrame.Clear();

                if (!m_config.IsValid())
                {
                    m_frameStarted = false;
                    m_imageSynchronized = false;
                    m_currentImageIndex = -1;
                    m_currentPacketIndex = 0;
                    m_completedImageCount = 0;
                    m_currentImageBuffer.clear();
                    m_frameBuffer.clear();
                    return;
                }

                m_currentImageBuffer.assign(
                    static_cast<std::size_t>(m_config.width) * static_cast<std::size_t>(m_config.height),
                    0);
                m_frameBuffer.assign(
                    static_cast<std::size_t>(m_config.width) *
                    static_cast<std::size_t>(m_config.height) *
                    static_cast<std::size_t>(m_config.imageCount),
                    0);
                ResetCurrentFrameState();
            }

            // 保留已分配容量，只把状态和字节内容归零，降低反复采集的堆抖动。
            void FrameSyncAssembler::ResetCurrentFrameState()
            {
                m_frameStarted = false;
                m_imageSynchronized = false;
                m_currentImageIndex = -1;
                m_currentPacketIndex = 0;
                m_completedImageCount = 0;

                if (!m_currentImageBuffer.empty())
                {
                    std::fill(m_currentImageBuffer.begin(), m_currentImageBuffer.end(), static_cast<Byte>(0));
                }

                if (!m_frameBuffer.empty())
                {
                    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), static_cast<Byte>(0));
                }
            }

            // 把当前平面复制进整帧偏移；最后一个平面完成时交付 ImageFrame。
            FrameSyncResult FrameSyncAssembler::HandleImageCompletion(ImageFrame& completedFrame)
            {
                if (m_currentImageIndex < 0 || m_currentImageIndex >= m_config.imageCount)
                {
                    ++m_syncErrorCount;
                    ResetCurrentFrameState();
                    return FrameSyncResult::Error;
                }

                const std::size_t planeSize =
                    static_cast<std::size_t>(m_config.width) * static_cast<std::size_t>(m_config.height);
                const std::size_t frameOffset = static_cast<std::size_t>(m_currentImageIndex) * planeSize;
                memcpy(&m_frameBuffer[frameOffset], &m_currentImageBuffer[0], planeSize);
                UpdateCurrentImageMetadata();

                ++m_completedImageCount;
                m_imageSynchronized = false;
                m_currentPacketIndex = 0;

                if (m_completedImageCount < m_config.imageCount)
                {
                    return FrameSyncResult::ImageCompleted;
                }

                m_workingFrame.pixels = m_frameBuffer;
                completedFrame = m_workingFrame;
                ResetCurrentFrameState();
                return FrameSyncResult::FrameCompleted;
            }

            // 从平面协议头读取曝光参数，元数据和像素保持同一图像索引。
            void FrameSyncAssembler::UpdateCurrentImageMetadata()
            {
                if (m_currentImageIndex < 0 || m_currentImageIndex >= static_cast<int>(m_workingFrame.planeInfos.size()))
                {
                    return;
                }

                ImagePlaneInfo& planeInfo = m_workingFrame.planeInfos[static_cast<std::size_t>(m_currentImageIndex)];
                planeInfo.imageIndex = m_currentImageIndex;

                if (m_config.deviceType == DeviceType::Three)
                {
                    planeInfo.exposure.primary = protocol::GetThreeExposurePrimary(&m_currentImageBuffer[0], m_currentImageBuffer.size());
                    planeInfo.exposure.secondary = protocol::GetThreeExposureSecondary(&m_currentImageBuffer[0], m_currentImageBuffer.size());
                    planeInfo.exposure.analogGain = protocol::GetThreeExposureAnalogGain(&m_currentImageBuffer[0], m_currentImageBuffer.size());
                }
                else if (m_config.deviceType == DeviceType::Skys1000)
                {
                    planeInfo.exposure.primary = protocol::GetSkysExposureTime(&m_currentImageBuffer[0], m_currentImageBuffer.size());
                    planeInfo.exposure.secondary = 0;
                    planeInfo.exposure.analogGain = protocol::GetSkysExposureGain(&m_currentImageBuffer[0], m_currentImageBuffer.size());
                }
            }
        }
    }
}
