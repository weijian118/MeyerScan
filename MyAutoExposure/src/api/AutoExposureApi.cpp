// =============================================================================
// 文件: AutoExposureApi.cpp
// 作用: 实现自动曝光预留接口，并保证当前阶段不会下发虚假参数。
// =============================================================================
#include "AutoExposure.h"

#include "../support/ModuleInfo.h"

#include <cstring>
#include <mutex>
#include <new>
#include <string>

namespace
{
    struct AutoExposureContext
    {
        MeyerCaptureDeviceContext device;
        std::string lastError;
        std::mutex mutex;
        bool configured;

        AutoExposureContext()
            : configured(false)
        {
            std::memset(&device, 0, sizeof(device));
        }
    };

    AutoExposureContext* ToContext(MeyerAutoExposureHandle handle)
    {
        return static_cast<AutoExposureContext*>(handle);
    }

    bool HasHeader(const MeyerCaptureProfileConfig* profile)
    {
        return profile != nullptr && profile->structSize >= sizeof(*profile) &&
               profile->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureDeviceContext* context)
    {
        return context != nullptr && context->structSize >= sizeof(*context) &&
               context->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureGroupInfo* info)
    {
        return info != nullptr && info->structSize >= sizeof(*info) &&
               info->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }
}

extern "C"
{
    std::int32_t MeyerAutoExposure_InitOutput(MeyerAutoExposureOutput* output)
    {
        if (output == nullptr)
        {
            return MeyerAutoExposureResult_InvalidArgument;
        }
        std::memset(output, 0, sizeof(*output));
        output->structSize = sizeof(*output);
        output->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        output->diagnosticCode = 1;
        std::strncpy(output->algorithmVersionUtf8,
                     "reserved-not-implemented",
                     sizeof(output->algorithmVersionUtf8) - 1U);
        return MeyerAutoExposureResult_Ok;
    }

    MeyerAutoExposureHandle MeyerAutoExposure_Create()
    {
        try
        {
            return new AutoExposureContext();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void MeyerAutoExposure_Destroy(MeyerAutoExposureHandle handle)
    {
        delete ToContext(handle);
    }

    std::int32_t MeyerAutoExposure_Configure(
        MeyerAutoExposureHandle handle,
        const MeyerCaptureDeviceContext* context)
    {
        AutoExposureContext* state = ToContext(handle);
        if (state == nullptr)
        {
            return MeyerAutoExposureResult_InvalidHandle;
        }
        if (!HasHeader(context))
        {
            return MeyerAutoExposureResult_InvalidArgument;
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        state->device = *context;
        state->configured = true;
        state->lastError.clear();
        return MeyerAutoExposureResult_Ok;
    }

    std::int32_t MeyerAutoExposure_Calculate(
        MeyerAutoExposureHandle handle,
        const MeyerCaptureProfileConfig* profile,
        const MeyerCaptureGroupInfo* groupInfo,
        const unsigned char* decryptedGroup,
        std::size_t decryptedBytes,
        MeyerAutoExposureOutput* output)
    {
        AutoExposureContext* state = ToContext(handle);
        if (state == nullptr)
        {
            return MeyerAutoExposureResult_InvalidHandle;
        }
        if (!HasHeader(profile) || !HasHeader(groupInfo) || decryptedGroup == nullptr ||
            decryptedBytes == 0U || output == nullptr)
        {
            return MeyerAutoExposureResult_InvalidArgument;
        }

        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->configured)
        {
            state->lastError = "AutoExposure session is not configured";
            return MeyerAutoExposureResult_InvalidArgument;
        }

        // 算法尚未确定，返回明确的“未实现”而不是返回一组可能被误使用的默认数值。
        MeyerAutoExposure_InitOutput(output);
        output->diagnosticCode = 1;
        state->lastError = "Automatic exposure calculation is reserved and not implemented";
        return MeyerAutoExposureResult_NotImplemented;
    }

    std::int32_t MeyerAutoExposure_GetLastError(
        MeyerAutoExposureHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize)
    {
        AutoExposureContext* state = ToContext(handle);
        if (state == nullptr || requiredSize == nullptr)
        {
            return MeyerAutoExposureResult_InvalidArgument;
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        *requiredSize = state->lastError.size() + 1U;
        if (buffer == nullptr || capacity < *requiredSize)
        {
            return MeyerAutoExposureResult_InvalidArgument;
        }
        std::memcpy(buffer, state->lastError.c_str(), *requiredSize);
        return MeyerAutoExposureResult_Ok;
    }

    std::int32_t GetMeyerModuleApiVersion()
    {
        return ModuleInfo::ApiVersionNumber;
    }

    const char* GetMeyerModuleVersion()
    {
        return ModuleInfo::Version;
    }
}
