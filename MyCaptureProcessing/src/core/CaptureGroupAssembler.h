// =============================================================================
// 文件: CaptureGroupAssembler.h
// 作用: 声明一个单线程六图组帧状态机。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace meyer
{
    namespace captureprocessing
    {
        class CaptureGroupAssembler
        {
        public:
            // 新对象在 Configure 前不接收任何数据包。
            CaptureGroupAssembler();

            // 复制 Profile 和设备上下文，并一次性分配工作缓冲区。
            bool Configure(const MeyerCaptureProfileConfig& profile,
                           const MeyerCaptureDeviceContext& context,
                           std::string& error);
            // 废弃当前不完整图组，已完成待复制组保持不变。
            void AbortIncompleteGroup();
            // 清空不完整组和待复制组，供采集会话重启。
            void ResetAll();

            // 推入一个完整传输包，返回 MeyerCaptureProcessingResult 值。
            std::int32_t PushPacket(const unsigned char* packet,
                                    std::size_t packetBytes,
                                    std::string& error);

            // 复制已解密完整组；成功后消费待复制标志。
            std::int32_t CopyCompletedGroup(unsigned char* destination,
                                            std::size_t capacity,
                                            std::size_t& requiredBytes,
                                            MeyerCaptureGroupInfo& info,
                                            std::string& error);

            // 查询是否正在等待当前组的后续包。
            bool HasIncompleteGroup() const;

        private:
            // 校验魔数并读取图像序号和三个状态字节。
            bool ParseImageHeader(const unsigned char* packet,
                                  std::size_t packetBytes,
                                  std::int32_t& imageIndex,
                                  MeyerCapturePlaneState& state,
                                  std::string& error) const;
            // 完成当前单图：解密并复制到六图工作缓冲。
            bool CompleteCurrentImage(std::string& error);
            // 六图完成后按确认规则汇总 LED、长按和扫描头。
            bool CompleteCurrentGroup(std::string& error);
            // 仅重置当前不完整组，复用已分配的 vector 容量。
            void ResetWorkingState();

        private:
            MeyerCaptureProfileConfig m_profile;
            MeyerCaptureDeviceContext m_context;
            bool m_configured;
            bool m_groupStarted;
            bool m_imageStarted;
            std::int32_t m_currentImageIndex;
            std::int32_t m_currentPacketIndex;
            std::int32_t m_completedImageCount;
            std::uint64_t m_groupSequence;
            std::vector<unsigned char> m_currentImage;
            std::vector<unsigned char> m_workingGroup;
            MeyerCapturePlaneState m_workingStates[MEYER_CAPTURE_MAX_IMAGE_COUNT];
            bool m_hasCompletedGroup;
            std::vector<unsigned char> m_completedGroup;
            MeyerCaptureGroupInfo m_completedInfo;
        };
    }
}
