// =============================================================================
// 文件: CaptureProcessingApi.cpp
// 作用: 把固定 C ABI 转成内部组帧器，把异常和缓冲区边界留在 DLL 内部。
// =============================================================================
#include "CaptureProcessing.h"

#include "../core/CaptureGroupAssembler.h"
#include "../core/CaptureSlowProcessor.h"
#include "../support/ModuleInfo.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <string>

namespace
{
    struct ProcessingContext
    {
        meyer::captureprocessing::CaptureGroupAssembler assembler;
        MeyerCaptureProfileConfig profile;
        MeyerCaptureDeviceContext device;
        std::string lastError;
        std::mutex mutex;
        bool configured;

        ProcessingContext()
            : configured(false)
        {
            std::memset(&profile, 0, sizeof(profile));
            std::memset(&device, 0, sizeof(device));
        }
    };

    ProcessingContext* ToContext(MeyerCaptureProcessingHandle handle)
    {
        return static_cast<ProcessingContext*>(handle);
    }

    template<typename Structure>
    bool HasHeader(const Structure* value, std::uint32_t schema)
    {
        return value != nullptr && value->structSize >= sizeof(Structure) &&
               value->schemaVersion == schema;
    }

    template<typename Callable>
    std::int32_t Invoke(MeyerCaptureProcessingHandle handle, const Callable& callable)
    {
        ProcessingContext* context = ToContext(handle);
        if (context == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidHandle;
        }

        std::lock_guard<std::mutex> lock(context->mutex);
        try
        {
            return callable(*context);
        }
        catch (const std::bad_alloc&)
        {
            context->lastError = "CaptureProcessing memory allocation failed";
            return MeyerCaptureProcessingResult_InternalError;
        }
        catch (const std::exception& exception)
        {
            context->lastError = exception.what();
            return MeyerCaptureProcessingResult_InternalError;
        }
        catch (...)
        {
            context->lastError = "Unknown CaptureProcessing exception";
            return MeyerCaptureProcessingResult_InternalError;
        }
    }

    void CopyText(char* destination, std::size_t capacity, const char* source)
    {
        if (destination == nullptr || capacity == 0U)
        {
            return;
        }
        std::memset(destination, 0, capacity);
        if (source != nullptr)
        {
            std::strncpy(destination, source, capacity - 1U);
        }
    }

    bool IsSupportedProfile(std::int32_t deviceProfile)
    {
        return deviceProfile == MeyerCaptureDeviceProfile_MyScan5 ||
               deviceProfile == MeyerCaptureDeviceProfile_MyScan5H ||
               deviceProfile == MeyerCaptureDeviceProfile_MyScan6;
    }

    std::int32_t FillDefaultProfile(std::int32_t deviceProfile,
                                    std::int32_t captureMode,
                                    MeyerCaptureProfileConfig& profile)
    {
        if (!IsSupportedProfile(deviceProfile))
        {
            return MeyerCaptureProcessingResult_InvalidProfile;
        }

        std::memset(&profile, 0, sizeof(profile));
        profile.structSize = sizeof(profile);
        profile.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        profile.deviceProfile = deviceProfile;
        profile.captureMode = captureMode;
        profile.width = 1024;
        profile.height = 455;
        profile.imageCount = 6;
        profile.packetsPerImage = 29;
        profile.packetBytes = 16384U;
        profile.lastPacketValidBytes = 7168U;
        profile.headerBytes = 40U;
        profile.queueDepth = 64U;
        profile.receiveTimeoutMs = 1500U;
        profile.postProcessQueueCapacity = 3U;
        profile.frameRate = 25;
        profile.encryptionMode = MeyerCaptureEncryption_LegacyInverseSubstitution40;

        const char* name = deviceProfile == MeyerCaptureDeviceProfile_MyScan5
            ? "MyScan5_25fps"
            : (deviceProfile == MeyerCaptureDeviceProfile_MyScan5H
                ? "MyScan5H_25fps" : "MyScan6_25fps");
        CopyText(profile.profileNameUtf8, sizeof(profile.profileNameUtf8), name);
        CopyText(profile.profileVersionUtf8, sizeof(profile.profileVersionUtf8), "capture-profile-1");
        return MeyerCaptureProcessingResult_Ok;
    }
}

extern "C"
{
    std::int32_t MeyerCaptureProcessing_InitProfile(MeyerCaptureProfileConfig* profile)
    {
        if (profile == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        std::memset(profile, 0, sizeof(*profile));
        profile->structSize = sizeof(*profile);
        profile->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        return MeyerCaptureProcessingResult_Ok;
    }

    std::int32_t MeyerCaptureProcessing_InitDeviceContext(MeyerCaptureDeviceContext* context)
    {
        if (context == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        std::memset(context, 0, sizeof(*context));
        context->structSize = sizeof(*context);
        context->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        return MeyerCaptureProcessingResult_Ok;
    }

    std::int32_t MeyerCaptureProcessing_InitGroupInfo(MeyerCaptureGroupInfo* info)
    {
        if (info == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        std::memset(info, 0, sizeof(*info));
        info->structSize = sizeof(*info);
        info->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        return MeyerCaptureProcessingResult_Ok;
    }

    std::int32_t MeyerCaptureProcessing_GetDefaultProfile(
        std::int32_t deviceProfile,
        std::int32_t captureMode,
        MeyerCaptureProfileConfig* profile)
    {
        if (profile == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        return FillDefaultProfile(deviceProfile, captureMode, *profile);
    }

    MeyerCaptureProcessingHandle MeyerCaptureProcessing_Create()
    {
        try
        {
            return new ProcessingContext();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void MeyerCaptureProcessing_Destroy(MeyerCaptureProcessingHandle handle)
    {
        delete ToContext(handle);
    }

    std::int32_t MeyerCaptureProcessing_Configure(
        MeyerCaptureProcessingHandle handle,
        const MeyerCaptureProfileConfig* profile,
        const MeyerCaptureDeviceContext* context)
    {
        return Invoke(handle, [profile, context](ProcessingContext& state) -> std::int32_t {
            if (!HasHeader(profile, MEYER_CAPTURE_TYPES_SCHEMA_VERSION) ||
                !HasHeader(context, MEYER_CAPTURE_TYPES_SCHEMA_VERSION))
            {
                state.lastError = "Capture profile or device context header is invalid";
                return MeyerCaptureProcessingResult_InvalidArgument;
            }
            if (!state.assembler.Configure(*profile, *context, state.lastError))
            {
                return MeyerCaptureProcessingResult_InvalidProfile;
            }
            state.profile = *profile;
            state.device = *context;
            state.configured = true;
            return MeyerCaptureProcessingResult_Ok;
        });
    }

    std::int32_t MeyerCaptureProcessing_Reset(MeyerCaptureProcessingHandle handle)
    {
        return Invoke(handle, [](ProcessingContext& state) -> std::int32_t {
            if (!state.configured)
            {
                return MeyerCaptureProcessingResult_InvalidState;
            }
            state.assembler.ResetAll();
            state.lastError.clear();
            return MeyerCaptureProcessingResult_Ok;
        });
    }

    std::int32_t MeyerCaptureProcessing_PushPacket(
        MeyerCaptureProcessingHandle handle,
        const unsigned char* packet,
        std::size_t packetBytes)
    {
        return Invoke(handle, [packet, packetBytes](ProcessingContext& state) -> std::int32_t {
            if (!state.configured)
            {
                state.lastError = "CaptureProcessing is not configured";
                return MeyerCaptureProcessingResult_InvalidState;
            }
            const std::int32_t result = state.assembler.PushPacket(
                packet, packetBytes, state.lastError);
            if (result >= 0)
            {
                state.lastError.clear();
            }
            return result;
        });
    }

    std::int32_t MeyerCaptureProcessing_AbortIncompleteGroup(
        MeyerCaptureProcessingHandle handle)
    {
        return Invoke(handle, [](ProcessingContext& state) -> std::int32_t {
            if (!state.configured)
            {
                return MeyerCaptureProcessingResult_InvalidState;
            }
            state.assembler.AbortIncompleteGroup();
            return MeyerCaptureProcessingResult_Ok;
        });
    }

    std::int32_t MeyerCaptureProcessing_CopyCompletedGroup(
        MeyerCaptureProcessingHandle handle,
        unsigned char* groupBuffer,
        std::size_t capacity,
        std::size_t* requiredBytes,
        MeyerCaptureGroupInfo* groupInfo)
    {
        if (requiredBytes != nullptr)
        {
            *requiredBytes = 0U;
        }
        return Invoke(handle, [groupBuffer, capacity, requiredBytes, groupInfo](ProcessingContext& state) -> std::int32_t {
            if (requiredBytes == nullptr || !HasHeader(groupInfo, MEYER_CAPTURE_TYPES_SCHEMA_VERSION))
            {
                return MeyerCaptureProcessingResult_InvalidArgument;
            }
            return state.assembler.CopyCompletedGroup(
                groupBuffer, capacity, *requiredBytes, *groupInfo, state.lastError);
        });
    }

    std::int32_t MeyerCaptureProcessing_ProcessSlowGroup(
        const MeyerCaptureProfileConfig* profile,
        const unsigned char* decryptedGroup,
        std::size_t decryptedBytes,
        const MeyerCaptureGroupInfo* inputInfo,
        unsigned char* processedGroup,
        std::size_t capacity,
        std::size_t* requiredBytes,
        MeyerCaptureGroupInfo* outputInfo)
    {
        if (requiredBytes != nullptr)
        {
            *requiredBytes = 0U;
        }
        if (!HasHeader(profile, MEYER_CAPTURE_TYPES_SCHEMA_VERSION) ||
            !HasHeader(inputInfo, MEYER_CAPTURE_TYPES_SCHEMA_VERSION) ||
            !HasHeader(outputInfo, MEYER_CAPTURE_TYPES_SCHEMA_VERSION) ||
            requiredBytes == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        std::string error;
        const bool ok = meyer::captureprocessing::CaptureSlowProcessor::Process(
            *profile, decryptedGroup, decryptedBytes, *inputInfo,
            processedGroup, capacity, *requiredBytes, *outputInfo, error);
        return ok ? MeyerCaptureProcessingResult_Ok
                  : MeyerCaptureProcessingResult_InvalidArgument;
    }

    std::int32_t MeyerCaptureProcessing_GetLastError(
        MeyerCaptureProcessingHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize)
    {
        ProcessingContext* context = ToContext(handle);
        if (context == nullptr || requiredSize == nullptr)
        {
            return MeyerCaptureProcessingResult_InvalidArgument;
        }
        std::lock_guard<std::mutex> lock(context->mutex);
        *requiredSize = context->lastError.size() + 1U;
        if (buffer == nullptr || capacity < *requiredSize)
        {
            return MeyerCaptureProcessingResult_BufferTooSmall;
        }
        std::memcpy(buffer, context->lastError.c_str(), *requiredSize);
        return MeyerCaptureProcessingResult_Ok;
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
