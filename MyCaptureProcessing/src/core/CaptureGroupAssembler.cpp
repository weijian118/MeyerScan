// =============================================================================
// 文件: CaptureGroupAssembler.cpp
// 作用: 实现固定分包的单图和六图同步状态机。
// =============================================================================
#include "CaptureGroupAssembler.h"

#include "CaptureProcessing.h"
#include "../crypto/LegacyImageDecryptor.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    const unsigned char kImageHeader[8] = {
        0xA5U, 0xCCU, 0x00U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U
    };

    // 只接受协议明确的二值状态，其它字节不能被当作关灯或非长按。
    bool IsBinaryProtocolFlag(std::int32_t value)
    {
        return value == 0x00 || value == 0xFF;
    }
}

namespace meyer
{
    namespace captureprocessing
    {
        CaptureGroupAssembler::CaptureGroupAssembler()
            : m_configured(false)
            , m_groupStarted(false)
            , m_imageStarted(false)
            , m_currentImageIndex(-1)
            , m_currentPacketIndex(0)
            , m_completedImageCount(0)
            , m_groupSequence(0U)
            , m_hasCompletedGroup(false)
        {
            std::memset(&m_profile, 0, sizeof(m_profile));
            std::memset(&m_context, 0, sizeof(m_context));
            std::memset(m_workingStates, 0, sizeof(m_workingStates));
            std::memset(&m_completedInfo, 0, sizeof(m_completedInfo));
        }

        bool CaptureGroupAssembler::Configure(const MeyerCaptureProfileConfig& profile,
                                              const MeyerCaptureDeviceContext& context,
                                              std::string& error)
        {
            error.clear();
            const std::uint64_t planeBytes =
                static_cast<std::uint64_t>(profile.width) *
                static_cast<std::uint64_t>(profile.height);
            const std::uint64_t wireImageBytes =
                static_cast<std::uint64_t>(profile.packetBytes) *
                    static_cast<std::uint64_t>(profile.packetsPerImage - 1) +
                static_cast<std::uint64_t>(profile.lastPacketValidBytes);

            if (profile.structSize < sizeof(profile) ||
                profile.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                context.structSize < sizeof(context) ||
                context.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                profile.width <= 0 || profile.height <= 0 ||
                profile.imageCount <= 0 ||
                profile.imageCount > static_cast<std::int32_t>(MEYER_CAPTURE_MAX_IMAGE_COUNT) ||
                profile.packetsPerImage <= 0 || profile.packetBytes == 0U ||
                profile.lastPacketValidBytes == 0U ||
                profile.lastPacketValidBytes > profile.packetBytes ||
                profile.headerBytes < 16U || profile.headerBytes > planeBytes ||
                wireImageBytes != planeBytes ||
                planeBytes > 64ULL * 1024ULL * 1024ULL)
            {
                error = "Capture profile dimensions or packet fields are inconsistent";
                return false;
            }

            m_profile = profile;
            m_context = context;
            const std::size_t logicalPlaneBytes = static_cast<std::size_t>(planeBytes);
            const std::size_t groupBytes =
                logicalPlaneBytes * static_cast<std::size_t>(profile.imageCount);
            m_currentImage.assign(logicalPlaneBytes, 0U);
            m_workingGroup.assign(groupBytes, 0U);
            m_completedGroup.assign(groupBytes, 0U);
            m_groupSequence = 0U;
            m_configured = true;
            m_hasCompletedGroup = false;
            ResetWorkingState();
            return true;
        }

        void CaptureGroupAssembler::AbortIncompleteGroup()
        {
            ResetWorkingState();
        }

        void CaptureGroupAssembler::ResetAll()
        {
            ResetWorkingState();
            m_hasCompletedGroup = false;
            std::fill(m_completedGroup.begin(), m_completedGroup.end(), 0U);
            std::memset(&m_completedInfo, 0, sizeof(m_completedInfo));
        }

        std::int32_t CaptureGroupAssembler::PushPacket(const unsigned char* packet,
                                                       std::size_t packetBytes,
                                                       std::string& error)
        {
            error.clear();
            if (!m_configured)
            {
                error = "Capture group assembler is not configured";
                return MeyerCaptureProcessingResult_InvalidState;
            }
            if (packet == nullptr || packetBytes != m_profile.packetBytes)
            {
                ResetWorkingState();
                error = "Raw stream packet length does not match the capture profile";
                return MeyerCaptureProcessingResult_InvalidPacket;
            }

            // 只在等待新单图时解释头魔数，避免图像内容偶然出现 A5 CC 被误判。
            if (!m_imageStarted)
            {
                std::int32_t imageIndex = -1;
                MeyerCapturePlaneState state = {};
                if (!ParseImageHeader(packet, packetBytes, imageIndex, state, error))
                {
                    ResetWorkingState();
                    return MeyerCaptureProcessingResult_InvalidHeader;
                }

                const std::int32_t expectedImageIndex = m_groupStarted ? m_completedImageCount : 0;
                if (imageIndex != expectedImageIndex)
                {
                    // 错序后只有当前包是 0 时才立即把它作为新组开始。
                    ResetWorkingState();
                    if (imageIndex != 0)
                    {
                        error = "Image sequence is not the expected next index";
                        return MeyerCaptureProcessingResult_SyncReset;
                    }
                }

                if (!m_groupStarted)
                {
                    m_groupStarted = true;
                    m_completedImageCount = 0;
                }
                m_imageStarted = true;
                m_currentImageIndex = imageIndex;
                m_currentPacketIndex = 0;
                m_workingStates[static_cast<std::size_t>(imageIndex)] = state;
                std::fill(m_currentImage.begin(), m_currentImage.end(), 0U);
            }

            const std::size_t packetOffset =
                static_cast<std::size_t>(m_currentPacketIndex) *
                static_cast<std::size_t>(m_profile.packetBytes);
            const std::size_t bytesToCopy =
                m_currentPacketIndex == m_profile.packetsPerImage - 1
                    ? static_cast<std::size_t>(m_profile.lastPacketValidBytes)
                    : static_cast<std::size_t>(m_profile.packetBytes);
            if (packetOffset > m_currentImage.size() ||
                bytesToCopy > m_currentImage.size() - packetOffset)
            {
                ResetWorkingState();
                error = "Packet offset exceeds the logical image buffer";
                return MeyerCaptureProcessingResult_InvalidPacket;
            }

            std::memcpy(&m_currentImage[packetOffset], packet, bytesToCopy);
            ++m_currentPacketIndex;
            if (m_currentPacketIndex < m_profile.packetsPerImage)
            {
                return MeyerCaptureProcessingResult_NeedMorePackets;
            }

            if (!CompleteCurrentImage(error))
            {
                ResetWorkingState();
                return MeyerCaptureProcessingResult_DecryptFailed;
            }
            if (m_completedImageCount < m_profile.imageCount)
            {
                return MeyerCaptureProcessingResult_ImageCompleted;
            }
            if (!CompleteCurrentGroup(error))
            {
                ResetWorkingState();
                return MeyerCaptureProcessingResult_InvalidState;
            }
            return MeyerCaptureProcessingResult_GroupCompleted;
        }

        std::int32_t CaptureGroupAssembler::CopyCompletedGroup(
            unsigned char* destination,
            std::size_t capacity,
            std::size_t& requiredBytes,
            MeyerCaptureGroupInfo& info,
            std::string& error)
        {
            error.clear();
            requiredBytes = m_completedGroup.size();
            if (!m_hasCompletedGroup)
            {
                requiredBytes = 0U;
                return MeyerCaptureProcessingResult_NoCompletedGroup;
            }
            if (destination == nullptr || capacity < requiredBytes)
            {
                error = "Completed group destination buffer is too small";
                return MeyerCaptureProcessingResult_BufferTooSmall;
            }

            std::memcpy(destination, &m_completedGroup[0], requiredBytes);
            info = m_completedInfo;
            m_hasCompletedGroup = false;
            return MeyerCaptureProcessingResult_Ok;
        }

        bool CaptureGroupAssembler::HasIncompleteGroup() const
        {
            return m_groupStarted || m_imageStarted;
        }

        bool CaptureGroupAssembler::ParseImageHeader(
            const unsigned char* packet,
            std::size_t packetBytes,
            std::int32_t& imageIndex,
            MeyerCapturePlaneState& state,
            std::string& error) const
        {
            if (packet == nullptr || packetBytes < m_profile.headerBytes ||
                std::memcmp(packet, kImageHeader, sizeof(kImageHeader)) != 0)
            {
                error = "Image packet magic header is invalid";
                return false;
            }

            imageIndex = static_cast<std::int32_t>(packet[12]);
            const std::int32_t led = static_cast<std::int32_t>(packet[13]);
            const std::int32_t longPress = static_cast<std::int32_t>(packet[14]);
            const std::int32_t scanHead = static_cast<std::int32_t>(packet[15]);
            if (imageIndex < 0 || imageIndex >= m_profile.imageCount ||
                !IsBinaryProtocolFlag(led) || !IsBinaryProtocolFlag(longPress) ||
                scanHead < MeyerCaptureScanHead_Large ||
                scanHead > MeyerCaptureScanHead_NotInserted)
            {
                error = "Image header contains an invalid index or state value";
                return false;
            }

            state.imageIndex = imageIndex;
            state.sourceImageIndex = imageIndex;
            state.ledRaw = led;
            state.longPressRaw = longPress;
            state.scanHeadRaw = scanHead;
            state.valid = 1;
            return true;
        }

        bool CaptureGroupAssembler::CompleteCurrentImage(std::string& error)
        {
            if (m_currentImageIndex < 0 || m_currentImageIndex >= m_profile.imageCount)
            {
                error = "Current image index is outside the configured group";
                return false;
            }

            if (m_profile.encryptionMode == MeyerCaptureEncryption_LegacyInverseSubstitution40 &&
                !LegacyImageDecryptor::DecryptPlane(
                    &m_currentImage[0], m_currentImage.size(), m_profile.headerBytes))
            {
                error = "Legacy image decryption failed";
                return false;
            }
            if (m_profile.encryptionMode != MeyerCaptureEncryption_None &&
                m_profile.encryptionMode != MeyerCaptureEncryption_LegacyInverseSubstitution40)
            {
                error = "Capture profile selects an unsupported image encryption mode";
                return false;
            }

            const std::size_t planeOffset =
                static_cast<std::size_t>(m_currentImageIndex) * m_currentImage.size();
            std::memcpy(&m_workingGroup[planeOffset], &m_currentImage[0], m_currentImage.size());
            ++m_completedImageCount;
            m_imageStarted = false;
            m_currentImageIndex = -1;
            m_currentPacketIndex = 0;
            return true;
        }

        bool CaptureGroupAssembler::CompleteCurrentGroup(std::string& error)
        {
            if (m_completedImageCount != m_profile.imageCount)
            {
                error = "Capture group does not contain all configured images";
                return false;
            }

            bool allLedOn = true;
            bool allLongPressed = true;
            bool allSmall = true;
            bool allNotInserted = true;
            bool anyLarge = false;
            for (std::int32_t index = 0; index < m_profile.imageCount; ++index)
            {
                const MeyerCapturePlaneState& state = m_workingStates[static_cast<std::size_t>(index)];
                if (state.valid == 0)
                {
                    error = "One or more image state records are missing";
                    return false;
                }
                allLedOn = allLedOn && state.ledRaw == 0xFF;
                allLongPressed = allLongPressed && state.longPressRaw == 0xFF;
                allSmall = allSmall && state.scanHeadRaw == MeyerCaptureScanHead_Small;
                allNotInserted = allNotInserted &&
                    state.scanHeadRaw == MeyerCaptureScanHead_NotInserted;
                anyLarge = anyLarge || state.scanHeadRaw == MeyerCaptureScanHead_Large;
            }

            std::memset(&m_completedInfo, 0, sizeof(m_completedInfo));
            m_completedInfo.structSize = sizeof(m_completedInfo);
            m_completedInfo.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
            m_completedInfo.groupSequence = ++m_groupSequence;
            m_completedInfo.groupBytes = m_workingGroup.size();
            m_completedInfo.width = m_profile.width;
            m_completedInfo.height = m_profile.height;
            m_completedInfo.imageCount = m_profile.imageCount;
            m_completedInfo.ledOn = allLedOn ? 1 : 0;
            m_completedInfo.longPressed = allLongPressed ? 1 : 0;
            // 确认规则：全无=无，全小=小，其余任意混杂统一判为大。
            m_completedInfo.scanHeadType = allNotInserted
                ? MeyerCaptureScanHead_NotInserted
                : (allSmall && !anyLarge ? MeyerCaptureScanHead_Small
                                         : MeyerCaptureScanHead_Large);
            m_completedInfo.slowProcessed = 0;
            m_completedInfo.stateRuleVersion = 1;
            m_completedInfo.device = m_context;
            for (std::int32_t index = 0; index < m_profile.imageCount; ++index)
            {
                m_completedInfo.planes[static_cast<std::size_t>(index)] =
                    m_workingStates[static_cast<std::size_t>(index)];
            }

            m_completedGroup = m_workingGroup;
            m_hasCompletedGroup = true;
            ResetWorkingState();
            return true;
        }

        void CaptureGroupAssembler::ResetWorkingState()
        {
            m_groupStarted = false;
            m_imageStarted = false;
            m_currentImageIndex = -1;
            m_currentPacketIndex = 0;
            m_completedImageCount = 0;
            if (!m_currentImage.empty())
            {
                std::fill(m_currentImage.begin(), m_currentImage.end(), 0U);
            }
            if (!m_workingGroup.empty())
            {
                std::fill(m_workingGroup.begin(), m_workingGroup.end(), 0U);
            }
            std::memset(m_workingStates, 0, sizeof(m_workingStates));
        }
    }
}
