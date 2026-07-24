// =============================================================================
// 文件: CaptureImagePipelineSession.cpp
// 作用: 实现场景级图像处理、功能门禁和多输出缓存。
// =============================================================================
#include "CaptureImagePipelineSession.h"

#include "../support/ModuleInfo.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    const std::uint64_t kKnownFeatureMask =
        MeyerCapturePipelineFeature_ColorCalibration |
        MeyerCapturePipelineFeature_AiSoftTissue |
        MeyerCapturePipelineFeature_ColorRemoval |
        MeyerCapturePipelineFeature_CoarseStripe;

    // 第一版还没有接入外部算法 DLL，因此高级功能全部属于已知但不可用能力。
    const std::uint64_t kImplementedFeatureMask = 0ULL;

    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const char* source)
    {
        std::memset(destination, 0, Capacity);
        if (source != nullptr)
        {
            std::strncpy(destination, source, Capacity - 1U);
        }
    }
}

namespace meyer
{
    namespace capturepipeline
    {
        CaptureImagePipelineSession::CaptureImagePipelineSession()
            : m_configured(false)
        {
            std::memset(&m_profile, 0, sizeof(m_profile));
            std::memset(&m_context, 0, sizeof(m_context));
        }

        std::int32_t CaptureImagePipelineSession::Configure(
            const MeyerCaptureProfileConfig& profile,
            const MeyerCaptureDeviceContext& context)
        {
            if (profile.structSize < sizeof(profile) ||
                profile.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                profile.width <= 0 || profile.height <= 0 ||
                profile.imageCount != 6 ||
                context.structSize < sizeof(context) ||
                context.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION)
            {
                return MeyerCaptureImagePipelineResult_InvalidArgument;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            m_profile = profile;
            m_context = context;
            m_outputs.clear();
            m_lastError.clear();
            m_configured = true;
            return MeyerCaptureImagePipelineResult_Ok;
        }

        std::int32_t CaptureImagePipelineSession::ProcessGroup(
            const unsigned char* normalizedGroup,
            std::size_t normalizedBytes,
            const MeyerCaptureGroupInfo& groupInfo,
            const MeyerCapturePipelineOptions& options)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_outputs.clear();
            m_lastError.clear();

            if (!m_configured)
            {
                m_lastError = "Capture image pipeline is not configured";
                return MeyerCaptureImagePipelineResult_NotConfigured;
            }
            if (normalizedGroup == nullptr ||
                groupInfo.structSize < sizeof(groupInfo) ||
                groupInfo.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                options.structSize < sizeof(options) ||
                options.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                options.captureMode != m_context.captureMode ||
                (options.enabledFeatures & ~kKnownFeatureMask) != 0ULL ||
                (options.requiredFeatures & ~options.enabledFeatures) != 0ULL)
            {
                m_lastError = "Capture image pipeline input or options are invalid";
                return MeyerCaptureImagePipelineResult_InvalidArgument;
            }

            const std::size_t width = static_cast<std::size_t>(m_profile.width);
            const std::size_t height = static_cast<std::size_t>(m_profile.height);
            if (height != 0U && width > (std::numeric_limits<std::size_t>::max)() / height)
            {
                m_lastError = "Capture image dimensions overflow size_t";
                return MeyerCaptureImagePipelineResult_InvalidArgument;
            }
            const std::size_t planeBytes = width * height;
            if (planeBytes > (std::numeric_limits<std::size_t>::max)() / 6U ||
                normalizedBytes != planeBytes * 6U)
            {
                m_lastError = "Normalized six-plane byte count does not match the profile";
                return MeyerCaptureImagePipelineResult_InvalidArgument;
            }

            const std::uint64_t unavailableFeatures =
                options.enabledFeatures & ~kImplementedFeatureMask;
            if ((options.requiredFeatures & unavailableFeatures) != 0ULL)
            {
                m_lastError = "One or more required capture image algorithms are unavailable";
                return MeyerCaptureImagePipelineResult_FeatureUnavailable;
            }

            // 重建输出当前是标准化六图的深副本。以后算法接入后仍使用同一个
            // outputType，但必须通过 producerVersion/appliedFeatures 说明处理版本。
            OutputSlot reconstruction = {};
            reconstruction.bytes.assign(normalizedGroup,
                                        normalizedGroup + normalizedBytes);
            reconstruction.info = MakeOutputInfo(
                MeyerCapturePipelineOutput_ReconstructionPlanes,
                MeyerCapturePipelineDataFormat_Gray8Planes,
                6, 1, static_cast<std::uint64_t>(normalizedBytes),
                groupInfo, options, unavailableFeatures);
            m_outputs[reconstruction.info.outputType] = reconstruction;

            OutputSlot display = {};
            std::string rgbError;
            if (!BuildBaseRgb888(normalizedGroup, planeBytes,
                                 display.bytes, rgbError))
            {
                m_outputs.clear();
                m_lastError = rgbError;
                return MeyerCaptureImagePipelineResult_InternalError;
            }
            display.info = MakeOutputInfo(
                MeyerCapturePipelineOutput_DisplayRgb888,
                MeyerCapturePipelineDataFormat_Rgb888,
                1, 3, static_cast<std::uint64_t>(display.bytes.size()),
                groupInfo, options, unavailableFeatures);
            m_outputs[display.info.outputType] = display;
            return MeyerCaptureImagePipelineResult_Ok;
        }

        bool CaptureImagePipelineSession::BuildBaseRgb888(
            const unsigned char* normalizedGroup,
            std::size_t planeBytes,
            std::vector<unsigned char>& rgb,
            std::string& error) const
        {
            if (normalizedGroup == nullptr || planeBytes == 0U ||
                planeBytes > (std::numeric_limits<std::size_t>::max)() / 3U)
            {
                error = "RGB888 input dimensions are invalid";
                return false;
            }

            rgb.assign(planeBytes * 3U, 0U);
            // 标准化顺序固定为 黑/R/G/B/激光G/激光B，因此白图平面是 1/2/3。
            const unsigned char* red = normalizedGroup + planeBytes;
            const unsigned char* green = normalizedGroup + planeBytes * 2U;
            const unsigned char* blue = normalizedGroup + planeBytes * 3U;
            for (std::size_t pixel = 0U; pixel < planeBytes; ++pixel)
            {
                rgb[pixel * 3U + 0U] = red[pixel];
                rgb[pixel * 3U + 1U] = green[pixel];
                rgb[pixel * 3U + 2U] = blue[pixel];
            }

            // 每个单图前 headerBytes 是协议头，不属于可显示像素；彩色输出将
            // 对应起始像素清零，同时标准化六图仍完整保留原始头供算法诊断。
            const std::size_t headerPixels =
                (std::min)(static_cast<std::size_t>(m_profile.headerBytes),
                           planeBytes);
            std::memset(&rgb[0], 0, headerPixels * 3U);
            error.clear();
            return true;
        }

        MeyerCapturePipelineOutputInfo CaptureImagePipelineSession::MakeOutputInfo(
            std::int32_t outputType,
            std::int32_t dataFormat,
            std::int32_t imageCount,
            std::int32_t channels,
            std::uint64_t byteSize,
            const MeyerCaptureGroupInfo& groupInfo,
            const MeyerCapturePipelineOptions& options,
            std::uint64_t unavailableFeatures) const
        {
            MeyerCapturePipelineOutputInfo info = {};
            info.structSize = sizeof(info);
            info.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
            info.outputType = outputType;
            info.available = 1;
            info.width = m_profile.width;
            info.height = m_profile.height;
            info.imageCount = imageCount;
            info.channels = channels;
            info.dataFormat = dataFormat;
            info.captureMode = options.captureMode;
            info.byteSize = byteSize;
            info.groupSequence = groupInfo.groupSequence;
            info.appliedFeatures = options.enabledFeatures & kImplementedFeatureMask;
            info.unavailableFeatures = unavailableFeatures;
            info.optionsRevision = options.optionsRevision;
            CopyText(info.producerVersionUtf8, ModuleInfo::Version);
            return info;
        }

        std::int32_t CaptureImagePipelineSession::GetOutputInfo(
            std::int32_t outputType,
            MeyerCapturePipelineOutputInfo& info) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto found = m_outputs.find(outputType);
            if (found == m_outputs.end())
            {
                std::memset(&info, 0, sizeof(info));
                info.structSize = sizeof(info);
                info.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
                info.outputType = outputType;
                return MeyerCaptureImagePipelineResult_NoData;
            }
            info = found->second.info;
            return MeyerCaptureImagePipelineResult_Ok;
        }

        std::int32_t CaptureImagePipelineSession::CopyOutput(
            std::int32_t outputType,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& requiredBytes) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            requiredBytes = 0U;
            const auto found = m_outputs.find(outputType);
            if (found == m_outputs.end())
            {
                return MeyerCaptureImagePipelineResult_NoData;
            }
            requiredBytes = found->second.bytes.size();
            if (buffer == nullptr || capacity < requiredBytes)
            {
                return MeyerCaptureImagePipelineResult_BufferTooSmall;
            }
            if (requiredBytes > 0U)
            {
                std::memcpy(buffer, &found->second.bytes[0], requiredBytes);
            }
            return MeyerCaptureImagePipelineResult_Ok;
        }

        std::string CaptureImagePipelineSession::LastError() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_lastError;
        }
    }
}
