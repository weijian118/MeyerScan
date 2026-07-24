// =============================================================================
// 文件: CaptureServiceApi.cpp
// 作用: 把固定 C ABI 参数转换为内部 CaptureServiceContext 调用，并统一拦截
//       空句柄、结构版本错误和 C++ 异常。
// =============================================================================
#include "../../include/CaptureService.h"

#include "../core/CaptureServiceContext.h"
#include "../support/ModuleInfo.h"

#include <cstring>
#include <exception>
#include <new>

namespace
{
    meyer::captureservice::CaptureServiceContext* ToContext(
        MeyerCaptureServiceHandle handle)
    {
        return static_cast<meyer::captureservice::CaptureServiceContext*>(handle);
    }

    bool HasHeader(const MeyerCaptureServiceConfig* config)
    {
        return config != nullptr && config->structSize >= sizeof(*config) &&
               config->schemaVersion == MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureServiceDeviceInfo* info)
    {
        return info != nullptr && info->structSize >= sizeof(*info) &&
               info->schemaVersion == MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureServiceStateSnapshot* snapshot)
    {
        return snapshot != nullptr && snapshot->structSize >= sizeof(*snapshot) &&
               snapshot->schemaVersion == MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureServiceEvent* eventInfo)
    {
        return eventInfo != nullptr && eventInfo->structSize >= sizeof(*eventInfo) &&
               eventInfo->schemaVersion == MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCaptureGroupInfo* info)
    {
        return info != nullptr &&
               info->structSize >= sizeof(*info) &&
               info->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCapturePipelineOptions* options)
    {
        return options != nullptr && options->structSize >= sizeof(*options) &&
               options->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }

    bool HasHeader(const MeyerCapturePipelineOutputInfo* info)
    {
        return info != nullptr && info->structSize >= sizeof(*info) &&
               info->schemaVersion == MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    }

    template<typename Function>
    std::int32_t Invoke(MeyerCaptureServiceHandle handle, Function function)
    {
        meyer::captureservice::CaptureServiceContext* context = ToContext(handle);
        if (context == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidHandle;
        }
        try
        {
            return function(*context);
        }
        catch (const std::bad_alloc&)
        {
            return MeyerCaptureServiceResult_InternalError;
        }
        catch (...)
        {
            return MeyerCaptureServiceResult_InternalError;
        }
    }
}

extern "C"
{
    std::int32_t MeyerCaptureService_InitConfig(
        MeyerCaptureServiceConfig* config)
    {
        if (config == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(config, 0, sizeof(*config));
        config->structSize = sizeof(*config);
        config->schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
        config->backendType = MeyerCaptureServiceBackend_DeviceTransport;
        config->modelHint = MeyerDeviceModel_Unknown;
        config->captureMode = MeyerCaptureMode_ColorCalibration;
        config->allowProductionMode = 1;
        config->vendorId = 0x04B4U;
        config->productId = 0x00F1U;
        config->commandTimeoutMs = 200U;
        config->streamTimeoutMs = 1500U;
        config->eventQueueCapacity = 256U;
        config->postProcessQueueCapacity = 3U;
        return MeyerCaptureServiceResult_Ok;
    }

    std::int32_t MeyerCaptureService_InitDeviceInfo(
        MeyerCaptureServiceDeviceInfo* info)
    {
        if (info == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(info, 0, sizeof(*info));
        info->structSize = sizeof(*info);
        info->schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
        return MeyerCaptureServiceResult_Ok;
    }

    std::int32_t MeyerCaptureService_InitStateSnapshot(
        MeyerCaptureServiceStateSnapshot* snapshot)
    {
        if (snapshot == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(snapshot, 0, sizeof(*snapshot));
        snapshot->structSize = sizeof(*snapshot);
        snapshot->schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
        snapshot->state = MeyerCaptureServiceState_Created;
        return MeyerCaptureServiceResult_Ok;
    }

    std::int32_t MeyerCaptureService_InitEvent(
        MeyerCaptureServiceEvent* eventInfo)
    {
        if (eventInfo == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(eventInfo, 0, sizeof(*eventInfo));
        eventInfo->structSize = sizeof(*eventInfo);
        eventInfo->schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
        return MeyerCaptureServiceResult_Ok;
    }

    std::int32_t MeyerCaptureService_InitPipelineOptions(
        MeyerCapturePipelineOptions* options)
    {
        if (options == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(options, 0, sizeof(*options));
        options->structSize = sizeof(*options);
        options->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        options->captureMode = MeyerCaptureMode_Unknown;
        return MeyerCaptureServiceResult_Ok;
    }

    std::int32_t MeyerCaptureService_InitPipelineOutputInfo(
        MeyerCapturePipelineOutputInfo* info)
    {
        if (info == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        std::memset(info, 0, sizeof(*info));
        info->structSize = sizeof(*info);
        info->schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        return MeyerCaptureServiceResult_Ok;
    }

    MeyerCaptureServiceHandle MeyerCaptureService_Create()
    {
        try
        {
            return new meyer::captureservice::CaptureServiceContext();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void MeyerCaptureService_Destroy(MeyerCaptureServiceHandle handle)
    {
        delete ToContext(handle);
    }

    std::int32_t MeyerCaptureService_Configure(
        MeyerCaptureServiceHandle handle,
        const MeyerCaptureServiceConfig* config)
    {
        if (!HasHeader(config))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [config](meyer::captureservice::CaptureServiceContext& context) {
            return context.Configure(*config);
        });
    }

    std::int32_t MeyerCaptureService_Shutdown(MeyerCaptureServiceHandle handle)
    {
        return Invoke(handle, [](meyer::captureservice::CaptureServiceContext& context) {
            return context.Shutdown();
        });
    }

    std::int32_t MeyerCaptureService_PrepareColorCalibration(
        MeyerCaptureServiceHandle handle)
    {
        return Invoke(handle, [](meyer::captureservice::CaptureServiceContext& context) {
            return context.PrepareColorCalibration();
        });
    }

    std::int32_t MeyerCaptureService_StartCapture(
        MeyerCaptureServiceHandle handle)
    {
        return Invoke(handle, [](meyer::captureservice::CaptureServiceContext& context) {
            return context.StartCapture();
        });
    }

    std::int32_t MeyerCaptureService_StopCapture(
        MeyerCaptureServiceHandle handle)
    {
        return Invoke(handle, [](meyer::captureservice::CaptureServiceContext& context) {
            return context.StopCapture();
        });
    }

    std::int32_t MeyerCaptureService_RequestLight(
        MeyerCaptureServiceHandle handle, std::int32_t on)
    {
        return Invoke(handle, [on](meyer::captureservice::CaptureServiceContext& context) {
            return context.RequestLight(on != 0);
        });
    }

    std::int32_t MeyerCaptureService_SetPipelineOptions(
        MeyerCaptureServiceHandle handle,
        const MeyerCapturePipelineOptions* options)
    {
        if (!HasHeader(options))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [options](meyer::captureservice::CaptureServiceContext& context) {
            return context.SetPipelineOptions(*options);
        });
    }

    std::int32_t MeyerCaptureService_GetDeviceInfo(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceDeviceInfo* info)
    {
        if (!HasHeader(info))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [info](meyer::captureservice::CaptureServiceContext& context) {
            return context.GetDeviceInfo(*info);
        });
    }

    std::int32_t MeyerCaptureService_GetStateSnapshot(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceStateSnapshot* snapshot)
    {
        if (!HasHeader(snapshot))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [snapshot](meyer::captureservice::CaptureServiceContext& context) {
            return context.GetStateSnapshot(*snapshot);
        });
    }

    std::int32_t MeyerCaptureService_PollEvent(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceEvent* eventInfo)
    {
        if (!HasHeader(eventInfo))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [eventInfo](meyer::captureservice::CaptureServiceContext& context) {
            return context.PollEvent(*eventInfo);
        });
    }

    std::int32_t MeyerCaptureService_CopyLatestPlane(
        MeyerCaptureServiceHandle handle,
        std::int32_t index,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes)
    {
        if (requiredBytes == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        *requiredBytes = 0U;
        return Invoke(handle, [index, buffer, capacity, requiredBytes](meyer::captureservice::CaptureServiceContext& context) {
            return context.CopyLatestPlane(index, buffer, capacity, *requiredBytes);
        });
    }

    std::int32_t MeyerCaptureService_CopyLatestRgb888(
        MeyerCaptureServiceHandle handle,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes)
    {
        if (requiredBytes == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        *requiredBytes = 0U;
        return Invoke(handle, [buffer, capacity, requiredBytes](meyer::captureservice::CaptureServiceContext& context) {
            return context.CopyLatestRgb888(buffer, capacity, *requiredBytes);
        });
    }

    std::int32_t MeyerCaptureService_GetLatestPipelineOutputInfo(
        MeyerCaptureServiceHandle handle,
        std::int32_t outputType,
        MeyerCapturePipelineOutputInfo* info)
    {
        if (!HasHeader(info))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [outputType, info](meyer::captureservice::CaptureServiceContext& context) {
            return context.GetLatestPipelineOutputInfo(outputType, *info);
        });
    }

    std::int32_t MeyerCaptureService_CopyLatestPipelineOutput(
        MeyerCaptureServiceHandle handle,
        std::int32_t outputType,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes)
    {
        if (requiredBytes == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        *requiredBytes = 0U;
        return Invoke(handle, [outputType, buffer, capacity, requiredBytes](meyer::captureservice::CaptureServiceContext& context) {
            return context.CopyLatestPipelineOutput(
                outputType, buffer, capacity, *requiredBytes);
        });
    }

    std::int32_t MeyerCaptureService_CopyLatestGroupInfo(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureGroupInfo* groupInfo)
    {
        if (!HasHeader(groupInfo))
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        return Invoke(handle, [groupInfo](meyer::captureservice::CaptureServiceContext& context) {
            return context.CopyLatestGroupInfo(*groupInfo);
        });
    }

    std::int32_t MeyerCaptureService_GetLastError(
        MeyerCaptureServiceHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize)
    {
        if (requiredSize == nullptr)
        {
            return MeyerCaptureServiceResult_InvalidArgument;
        }
        *requiredSize = 0U;
        return Invoke(handle, [buffer, capacity, requiredSize](meyer::captureservice::CaptureServiceContext& context) {
            const std::string error = context.LastError();
            *requiredSize = error.size() + 1U;
            if (buffer == nullptr || capacity < *requiredSize)
            {
                return MeyerCaptureServiceResult_InvalidArgument;
            }
            std::memcpy(buffer, error.c_str(), *requiredSize);
            return MeyerCaptureServiceResult_Ok;
        });
    }

    const char* MeyerCaptureService_GetModuleName()
    {
        return ModuleInfo::Name;
    }

    const char* MeyerCaptureService_GetApiVersion()
    {
        return ModuleInfo::ApiVersion;
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
