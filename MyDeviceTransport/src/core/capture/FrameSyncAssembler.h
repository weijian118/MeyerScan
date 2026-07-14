#pragma once

#include <vector>

#include "../../model/CaptureFrameConfig.h"
#include "../../model/CapturePacket.h"
#include "../../model/ImageFrame.h"

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            enum class FrameSyncResult
            {
                NeedMorePackets = 0,
                ImageCompleted,
                FrameCompleted,
                SyncLost,
                Error
            };

            class FrameSyncAssembler
            {
            public:
                // 初始化为等待第 0 张图像头的状态。
                FrameSyncAssembler();

                // 保存配置并一次性分配组帧工作缓冲区。
                void Configure(const CaptureFrameConfig& config);
                // 返回当前配置快照。
                const CaptureFrameConfig& GetConfig() const;
                // 查询配置和缓冲区是否可用于组帧。
                bool IsConfigured() const;

                // 用显式状态机替代旧 CaptureFrame 双重循环，并吸收图像头同步与
                // 当前平面追加逻辑，使每次调用只消费一个数据包。
                // 清空同步状态并复用既有缓冲区容量。
                void Reset();
                // 消费一个有序包并返回本次状态机推进结果。
                FrameSyncResult PushPacket(const CapturePacket& packet, ImageFrame& completedFrame);

                // 返回当前图像索引，未同步时为 -1。
                int GetCurrentImageIndex() const;
                // 返回当前平面已接收包数。
                int GetCurrentPacketIndex() const;
                // 查询是否已与当前图像头同步。
                bool IsImageSynchronized() const;
                // 返回本轮配置后的累计同步错误数。
                int GetSyncErrorCount() const;

            private:
                // 根据尺寸重新分配平面和整帧工作缓冲区。
                void ResetWorkingBuffers();
                // 保留容量并把当前帧状态与字节内容归零。
                void ResetCurrentFrameState();
                // 把完成平面复制到整帧，最后一个平面完成时交付帧。
                FrameSyncResult HandleImageCompletion(ImageFrame& completedFrame);
                // 从当前平面的协议头提取曝光元数据。
                void UpdateCurrentImageMetadata();

            private:
                CaptureFrameConfig m_config;
                bool m_configured;
                bool m_frameStarted;
                bool m_imageSynchronized;
                int m_currentImageIndex;
                int m_currentPacketIndex;
                int m_completedImageCount;
                int m_syncErrorCount;
                std::vector<Byte> m_currentImageBuffer;
                std::vector<Byte> m_frameBuffer;
                ImageFrame m_workingFrame;
            };
        }
    }
}
