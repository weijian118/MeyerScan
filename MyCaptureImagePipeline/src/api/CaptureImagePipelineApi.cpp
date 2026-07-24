// =============================================================================
// 文件: CaptureImagePipelineApi.cpp
// 作用: 实现公共 C ABI、参数头校验和异常边界。
// =============================================================================
#include "../../include/CaptureImagePipeline.h"

#include "../core/CaptureImagePipelineSession.h"
#include "../support/ModuleInfo.h"

#include <cstring>
#include <exception>
#include <new>
#include <string>

namespace
{
    meyer::capturepipeline::CaptureImagePipelineSession* ToSession(
        MeyerCaptureImagePipelineHandle handle)
    {
        return static_cast<meyer::capturepipeline::CaptureImagePipelineSession*>(handle);
    }

    std::int32_t CopyError(const std::string& error,
                           char* buffer,
                           std::size_t capacity,
                           std::size_t* requiredSize)
    {
        if (requiredSize == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        *requiredSize = error.size() + 1U;
        if (buffer == nullptr || capacity < *requiredSize)
        {
            return MeyerCaptureImagePipelineResult_BufferTooSmall;
        }
        std::memcpy(buffer, error.c_str(), *requiredSize);
        return MeyerCaptureImagePipelineResult_Ok;
    }
}

extern "C"
{
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_InitOptions(MeyerCapturePipelineOptions* options)
    {
        if (options == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        std::memset(options, 0, sizeof(*options));
        options->structSize = sizeof(*options);
        options->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        options->captureMode = MeyerCaptureMode_Unknown;
        return MeyerCaptureImagePipelineResult_Ok;
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_InitOutputInfo(MeyerCapturePipelineOutputInfo* info)
    {
        if (info == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        std::memset(info, 0, sizeof(*info));
        info->structSize = sizeof(*info);
        info->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        return MeyerCaptureImagePipelineResult_Ok;
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API MeyerCaptureImagePipelineHandle
        MeyerCaptureImagePipeline_Create()
    {
        try
        {
            return new meyer::capturepipeline::CaptureImagePipelineSession();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API void MeyerCaptureImagePipeline_Destroy(
        MeyerCaptureImagePipelineHandle handle)
    {
        delete ToSession(handle);
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_Configure(
            MeyerCaptureImagePipelineHandle handle,
            const MeyerCaptureProfileConfig* profile,
            const MeyerCaptureDeviceContext* context)
    {
        if (handle == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidHandle;
        }
        if (profile == nullptr || context == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        try
        {
            return ToSession(handle)->Configure(*profile, *context);
        }
        catch (...)
        {
            return MeyerCaptureImagePipelineResult_InternalError;
        }
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_ProcessGroup(
            MeyerCaptureImagePipelineHandle handle,
            const unsigned char* normalizedGroup,
            std::size_t normalizedBytes,
            const MeyerCaptureGroupInfo* groupInfo,
            const MeyerCapturePipelineOptions* options)
    {
        if (handle == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidHandle;
        }
        if (groupInfo == nullptr || options == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        try
        {
            return ToSession(handle)->ProcessGroup(
                normalizedGroup, normalizedBytes, *groupInfo, *options);
        }
        catch (...)
        {
            return MeyerCaptureImagePipelineResult_InternalError;
        }
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_GetOutputInfo(
            MeyerCaptureImagePipelineHandle handle,
            std::int32_t outputType,
            MeyerCapturePipelineOutputInfo* info)
    {
        if (handle == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidHandle;
        }
        if (info == nullptr || info->structSize < sizeof(*info) ||
            info->schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        try
        {
            return ToSession(handle)->GetOutputInfo(outputType, *info);
        }
        catch (...)
        {
            return MeyerCaptureImagePipelineResult_InternalError;
        }
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_CopyOutput(
            MeyerCaptureImagePipelineHandle handle,
            std::int32_t outputType,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t* requiredBytes)
    {
        if (handle == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidHandle;
        }
        if (requiredBytes == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidArgument;
        }
        try
        {
            return ToSession(handle)->CopyOutput(
                outputType, buffer, capacity, *requiredBytes);
        }
        catch (...)
        {
            return MeyerCaptureImagePipelineResult_InternalError;
        }
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_GetLastError(
            MeyerCaptureImagePipelineHandle handle,
            char* buffer,
            std::size_t capacity,
            std::size_t* requiredSize)
    {
        if (handle == nullptr)
        {
            return MeyerCaptureImagePipelineResult_InvalidHandle;
        }
        return CopyError(ToSession(handle)->LastError(),
                         buffer, capacity, requiredSize);
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t GetMeyerModuleApiVersion()
    {
        return ModuleInfo::ApiVersionNumber;
    }

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API const char* GetMeyerModuleVersion()
    {
        return ModuleInfo::Version;
    }
}
