// 对已组装帧执行轻量协议后处理：平面排序、LED/拍照状态和曝光字段提取。
#include "../../stdafx.h"

#include "FramePostProcessor.h"

#include "../../protocol/MeyerProtocolDefs.h"

#include <cstring>
#include <vector>

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            namespace
            {
                // 按协议映射重排像素平面及对应元数据；临时 vector 保证源和目标不重叠。
                void ReorderFramePlanes(ImageFrame& frame, const int* order, int orderCount)
                {
                    if (order == nullptr || orderCount <= 0 || frame.width <= 0 || frame.height <= 0)
                    {
                        return;
                    }

                    const std::size_t planeSize = frame.PlaneSize();
                    if (planeSize == 0 || frame.pixels.size() < planeSize * static_cast<std::size_t>(orderCount))
                    {
                        return;
                    }

                    std::vector<Byte> reorderedPixels(frame.pixels.size(), 0);
                    for (int i = 0; i < orderCount; ++i)
                    {
                        const int srcIndex = order[i];
                        if (srcIndex < 0 || srcIndex >= frame.imageCount)
                        {
                            return;
                        }

                        memcpy(&reorderedPixels[static_cast<std::size_t>(i) * planeSize],
                            &frame.pixels[static_cast<std::size_t>(srcIndex) * planeSize],
                            planeSize);
                    }
                    frame.pixels.swap(reorderedPixels);

                    if (frame.planeInfos.size() >= static_cast<std::size_t>(orderCount))
                    {
                        std::vector<ImagePlaneInfo> reorderedInfos(frame.planeInfos.size());
                        for (int i = 0; i < orderCount; ++i)
                        {
                            reorderedInfos[static_cast<std::size_t>(i)] = frame.planeInfos[static_cast<std::size_t>(order[i])];
                            reorderedInfos[static_cast<std::size_t>(i)].imageIndex = i;
                        }
                        for (std::size_t i = static_cast<std::size_t>(orderCount); i < frame.planeInfos.size(); ++i)
                        {
                            reorderedInfos[i] = frame.planeInfos[i];
                        }
                        frame.planeInfos.swap(reorderedInfos);
                    }
                }

                // 根据设备型号和固件排列模式选择固定映射表。
                void ApplyFrameOrdering(ImageFrame& frame, DeviceType deviceType, PictureOrderMode pictureOrderMode)
                {
                    if (deviceType == DeviceType::Three && frame.imageCount == 6)
                    {
                        static const int kThreeOrder[6] = { 1, 0, 2, 4, 3, 5 };
                        ReorderFramePlanes(frame, kThreeOrder, 6);
                        return;
                    }

                    if (deviceType == DeviceType::Skys1000 && frame.imageCount == 9)
                    {
                        if (pictureOrderMode == PictureOrderMode::Old)
                        {
                            static const int kSkysOldOrder[9] = { 5, 1, 2, 0, 3, 4, 6, 7, 8 };
                            ReorderFramePlanes(frame, kSkysOldOrder, 9);
                        }
                        else
                        {
                            static const int kSkysNewOrder[9] = { 6, 0, 2, 1, 7, 8, 3, 4, 5 };
                            ReorderFramePlanes(frame, kSkysNewOrder, 9);
                        }
                    }
                }
            }

            // 新对象在 Configure 前拒绝处理帧。
            FramePostProcessor::FramePostProcessor()
                : m_configured(false)
            {
            }

            // 保存后处理所需的设备型号、模式和尺寸快照。
            void FramePostProcessor::Configure(const CaptureFrameConfig& config)
            {
                m_config = config;
                m_configured = config.IsValid();
            }

            // 返回当前后处理配置。
            const CaptureFrameConfig& FramePostProcessor::GetConfig() const
            {
                return m_config;
            }

            // 查询配置是否有效。
            bool FramePostProcessor::IsConfigured() const
            {
                return m_configured;
            }

            // 处理一帧并回填 DeviceStatus；任一边界检查失败都返回 false。
            bool FramePostProcessor::Process(ImageFrame& frame)
            {
                if (!m_configured || frame.Empty())
                {
                    return false;
                }

                ApplyFrameOrdering(frame, m_config.deviceType, m_config.pictureOrderMode);

                frame.status.deviceType = m_config.deviceType;
                frame.status.scanMode = m_config.scanMode;
                frame.status.pictureOrderMode = m_config.pictureOrderMode;

                const std::size_t planeSize = frame.PlaneSize();
                if (planeSize == 0 || frame.pixels.size() < planeSize)
                {
                    return false;
                }

                bool defaultLed = true;
                bool defaultPhoto = false;
                int scanHeadType = 3;

                for (int imageIndex = 0; imageIndex < frame.imageCount; ++imageIndex)
                {
                    const std::size_t planeOffset = static_cast<std::size_t>(imageIndex) * planeSize;
                    if (planeOffset + planeSize > frame.pixels.size())
                    {
                        return false;
                    }

                    const Byte* planePtr = &frame.pixels[planeOffset];
                    if (!protocol::IsLedOn(planePtr, planeSize))
                    {
                        defaultLed = false;
                    }
                    if (protocol::IsPhotoMode(planePtr, planeSize))
                    {
                        defaultPhoto = true;
                    }

                    scanHeadType = protocol::GetScanHeadType(planePtr, planeSize);
                }

                frame.status.ledOn = defaultLed;
                frame.status.photoMode = defaultPhoto;
                frame.status.scanHeadType = scanHeadType;
                // 两个布尔协议位组合成上层使用的四态 CaptureStatus。
                frame.status.captureStatus =
                    defaultLed ?
                    (defaultPhoto ? CaptureStatus::LedPhoto : CaptureStatus::Led) :
                    (defaultPhoto ? CaptureStatus::OffPhoto : CaptureStatus::Off);

                if (m_config.deviceType == DeviceType::Skys1000)
                {
                    for (int imageIndex = 0; imageIndex < frame.imageCount; ++imageIndex)
                    {
                        const std::size_t planeOffset = static_cast<std::size_t>(imageIndex) * planeSize;
                        const Byte* planePtr = &frame.pixels[planeOffset];
                        const int timeValue = protocol::GetSkysExposureTime(planePtr, planeSize);
                        const int gainValue = protocol::GetSkysExposureGain(planePtr, planeSize);

                        if (m_config.pictureOrderMode == PictureOrderMode::Old)
                        {
                            if (imageIndex == 2)
                            {
                                frame.status.timeW = timeValue;
                                frame.status.gainW = gainValue;
                            }
                            else if (imageIndex == 3)
                            {
                                frame.status.timeC = timeValue;
                                frame.status.gainC = gainValue;
                            }
                            else if (imageIndex == 6)
                            {
                                frame.status.timeX = timeValue;
                                frame.status.gainX = gainValue;
                            }
                        }
                        else
                        {
                            if (imageIndex == 2)
                            {
                                frame.status.timeW = timeValue;
                                frame.status.gainW = gainValue;
                            }
                            else if (imageIndex == 7)
                            {
                                frame.status.timeC = timeValue;
                                frame.status.gainC = gainValue;
                            }
                            else if (imageIndex == 3)
                            {
                                frame.status.timeX = timeValue;
                                frame.status.gainX = gainValue;
                            }
                        }
                    }
                }
                else if (m_config.deviceType == DeviceType::Three)
                {
                    if (frame.planeInfos.size() >= 6)
                    {
                        frame.status.timeW = frame.planeInfos[0].exposure.primary;
                        frame.status.gainW = frame.planeInfos[0].exposure.analogGain;
                        frame.status.timeC = frame.planeInfos[2].exposure.primary;
                        frame.status.gainC = frame.planeInfos[2].exposure.analogGain;
                        frame.status.timeX = frame.planeInfos[5].exposure.primary;
                        frame.status.gainX = frame.planeInfos[5].exposure.analogGain;
                    }
                }

                return true;
            }
        }
    }
}
