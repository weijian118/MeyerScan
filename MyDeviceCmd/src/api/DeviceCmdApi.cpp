// =============================================================================
// 文件: DeviceCmdApi.cpp
// 作用: 把公共 C ABI 转换为内部 C++ 服务，并在 DLL 边界拦截异常。
// =============================================================================
#include "../../include/DeviceCmd.h"

#include "../core/DeviceCommandService.h"
#include "../model/DeviceModelCatalog.h"
#include "../support/ModuleInfo.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <string>

namespace
{
    struct DeviceCmdContext
    {
        meyer::devicecmd::DeviceCommandService service;
        std::mutex mutex;
    };

    // 将不透明句柄还原为 DLL 内部上下文。
    DeviceCmdContext* ToContext(MeyerDeviceCmdHandle handle)
    {
        return static_cast<DeviceCmdContext*>(handle);
    }

    // 检查公共结构头，避免把旧版本/短结构误解释成当前布局。
    template<typename Structure>
    bool HasValidHeader(const Structure* value)
    {
        return value != nullptr &&
               value->structSize >= sizeof(Structure) &&
               value->schemaVersion == MEYER_DEVICE_CMD_SCHEMA_VERSION;
    }

    // 为新增的固定结构统一写入结构大小、模式版本和清零后的安全默认值。
    template<typename Structure>
    std::int32_t InitializeStructure(Structure* value)
    {
        if (value == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(value, 0, sizeof(*value));
        value->structSize = sizeof(*value);
        value->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        return MeyerDeviceCmdResult_Ok;
    }

    // 所有句柄操作在同一把 mutex 下串行，防止一个线程关闭时另一个线程取帧。
    template<typename Callable>
    std::int32_t Invoke(MeyerDeviceCmdHandle handle, const Callable& callable)
    {
        DeviceCmdContext* context = ToContext(handle);
        if (context == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidHandle;
        }

        std::lock_guard<std::mutex> lock(context->mutex);
        try
        {
            return callable(*context);
        }
        catch (const std::bad_alloc&)
        {
            return MeyerDeviceCmdResult_InternalError;
        }
        catch (const std::exception&)
        {
            return MeyerDeviceCmdResult_InternalError;
        }
        catch (...)
        {
            return MeyerDeviceCmdResult_InternalError;
        }
    }

    // 把服务错误文本复制到调用方缓冲区，支持先探测 requiredSize。
    std::int32_t CopyError(const std::string& error,
                           char* buffer,
                           std::size_t capacity,
                           std::size_t* requiredSize)
    {
        const std::size_t required = error.size() + 1U;
        if (requiredSize != nullptr)
        {
            *requiredSize = required;
        }
        if (buffer == nullptr || capacity < required)
        {
            return MeyerDeviceCmdResult_BufferTooSmall;
        }
        std::memcpy(buffer, error.c_str(), required);
        return MeyerDeviceCmdResult_Ok;
    }
}

extern "C"
{
    // 初始化打开参数，正式后端路径故意留空，迫使调用方显式传入 EXE 同级 DLL 路径。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitOpenParams(MeyerDeviceCmdOpenParams* params)
    {
        if (params == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(params, 0, sizeof(*params));
        params->structSize = sizeof(*params);
        params->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        params->backendType = MeyerDeviceCmdBackend_DeviceTransport;
        params->modelHint = MeyerDeviceModel_Unknown;
        params->vendorId = 0x04B4U;
        params->productId = 0x00F1U;
        params->deviceIndex = 0U;
        params->commandTimeoutMs = 1500U;
        params->streamTimeoutMs = 1500U;
        return MeyerDeviceCmdResult_Ok;
    }

    // 根据型号复制默认采集参数；调用方仍可覆盖尺寸和扫描头字段。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCaptureParamsForModel(
        std::int32_t model,
        MeyerDeviceCmdCaptureParams* params)
    {
        if (params == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        const meyer::devicecmd::DeviceModelProfile* profile =
            meyer::devicecmd::DeviceModelCatalog::Find(model);
        if (profile == nullptr)
        {
            return MeyerDeviceCmdResult_UnsupportedModel;
        }
        *params = profile->defaultCapture;
        return MeyerDeviceCmdResult_Ok;
    }

    // 初始化状态快照并把所有值清为未知/无效。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitStateSnapshot(
        MeyerDeviceStateSnapshot* snapshot)
    {
        if (snapshot == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(snapshot, 0, sizeof(*snapshot));
        snapshot->structSize = sizeof(*snapshot);
        snapshot->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        snapshot->connectionState = MeyerDeviceConnectionState_Closed;
        snapshot->workMode = MeyerDeviceWorkMode_Idle;
        return MeyerDeviceCmdResult_Ok;
    }

    // 初始化帧元数据结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFrameInfo(
        MeyerDeviceCmdFrameInfo* frameInfo)
    {
        if (frameInfo == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(frameInfo, 0, sizeof(*frameInfo));
        frameInfo->structSize = sizeof(*frameInfo);
        frameInfo->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        return MeyerDeviceCmdResult_Ok;
    }

    // 初始化通用响应，防止调用方把旧 payloadSize 当成本次结果。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitRawResponse(
        MeyerDeviceCmdRawResponse* response)
    {
        if (response == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(response, 0, sizeof(*response));
        response->structSize = sizeof(*response);
        response->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        return MeyerDeviceCmdResult_Ok;
    }

    // 初始化型号描述结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitModelDescriptor(
        MeyerDeviceModelDescriptor* descriptor)
    {
        if (descriptor == nullptr)
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        std::memset(descriptor, 0, sizeof(*descriptor));
        descriptor->structSize = sizeof(*descriptor);
        descriptor->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        return MeyerDeviceCmdResult_Ok;
    }

    // 初始化协议扩展结构；所有结构都使用同一套版本头校验规则。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraParameters(
        MeyerDeviceCmdCameraParameters* parameters)
    {
        return InitializeStructure(parameters);
    }

    // 初始化在线开窗位置结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraWindowPosition(
        MeyerDeviceCmdCameraWindowPosition* position)
    {
        return InitializeStructure(position);
    }

    // 初始化颜色校正矩阵结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitColorMatrix(
        MeyerDeviceCmdColorMatrix* matrix)
    {
        return InitializeStructure(matrix);
    }

    // 初始化温度结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitTemperature(
        MeyerDeviceCmdTemperature* temperature)
    {
        return InitializeStructure(temperature);
    }

    // 初始化固件擦除进度结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareEraseProgress(
        MeyerDeviceCmdFirmwareEraseProgress* progress)
    {
        return InitializeStructure(progress);
    }

    // 初始化固件分包写入请求结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareWritePacket(
        MeyerDeviceCmdFirmwareWritePacket* packet)
    {
        return InitializeStructure(packet);
    }

    // 初始化固件分包写入进度结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareWriteProgress(
        MeyerDeviceCmdFirmwareWriteProgress* progress)
    {
        return InitializeStructure(progress);
    }

    // 初始化相机标定参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraCalibration(
        MeyerDeviceCmdCameraCalibration* calibration)
    {
        return InitializeStructure(calibration);
    }

    // 初始化颜色标定参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitColorCalibration(
        MeyerDeviceCmdColorCalibration* calibration)
    {
        return InitializeStructure(calibration);
    }

    // 初始化设备信息结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitDeviceInfo(
        MeyerDeviceCmdDeviceInfo* info)
    {
        return InitializeStructure(info);
    }

    // 初始化曝光参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitExposureParameters(
        MeyerDeviceCmdExposureParameters* parameters)
    {
        return InitializeStructure(parameters);
    }

    // 从型号目录读取稳定的公共描述。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetModelDescriptor(
        std::int32_t model,
        MeyerDeviceModelDescriptor* descriptor)
    {
        if (!HasValidHeader(descriptor))
        {
            return MeyerDeviceCmdResult_InvalidArgument;
        }
        return meyer::devicecmd::DeviceModelCatalog::CopyDescriptor(model, *descriptor)
            ? MeyerDeviceCmdResult_Ok
            : MeyerDeviceCmdResult_UnsupportedModel;
    }

    // 创建只包含一个设备领域服务的上下文。
    MEYERSCAN_DEVICE_CMD_API MeyerDeviceCmdHandle MeyerDeviceCmd_Create()
    {
        try
        {
            return new DeviceCmdContext();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // delete 会触发服务析构，析构负责停止采集和关闭动态后端。
    MEYERSCAN_DEVICE_CMD_API void MeyerDeviceCmd_Destroy(MeyerDeviceCmdHandle handle)
    {
        delete ToContext(handle);
    }

    // 打开会话前校验参数完整性和型号目录。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Open(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdOpenParams* params)
    {
        return Invoke(handle, [params](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(params))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            if (params->backendType == MeyerDeviceCmdBackend_DeviceTransport &&
                params->transportLibraryPathUtf8[0] == '\0')
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.Open(*params);
        });
    }

    // 关闭设备会话；重复调用仍安全，方便宿主统一释放资源。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Close(MeyerDeviceCmdHandle handle)
    {
        return Invoke(handle, [](DeviceCmdContext& context) {
            return context.service.Close();
        });
    }

    // 返回当前设备连接状态；结果 1 表示已打开，0 表示未打开。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_IsOpen(MeyerDeviceCmdHandle handle)
    {
        return Invoke(handle, [](DeviceCmdContext& context) {
            return context.service.IsOpen() ? 1 : 0;
        });
    }

    // 使用服务层保存的最近一次参数重新连接设备。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Reconnect(MeyerDeviceCmdHandle handle)
    {
        return Invoke(handle, [](DeviceCmdContext& context) {
            return context.service.Reconnect();
        });
    }

    // 触发一次基础状态刷新，具体查询顺序由 DeviceCommandService 统一维护。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_RefreshBasicState(MeyerDeviceCmdHandle handle)
    {
        return Invoke(handle, [](DeviceCmdContext& context) {
            return context.service.RefreshBasicState();
        });
    }

    // 从服务层复制状态快照；调用方必须先初始化输出结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetStateSnapshot(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceStateSnapshot* snapshot)
    {
        return Invoke(handle, [snapshot](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(snapshot))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.GetStateSnapshot(*snapshot);
        });
    }

    // 修改普通灯光请求状态，on 非零表示开灯。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetLight(
        MeyerDeviceCmdHandle handle,
        std::int32_t on)
    {
        return Invoke(handle, [on](DeviceCmdContext& context) {
            return context.service.SetLight(on != 0);
        });
    }

    // 修改强制开灯策略，enabled 非零表示启用。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetForceLight(
        MeyerDeviceCmdHandle handle,
        std::int32_t enabled)
    {
        return Invoke(handle, [enabled](DeviceCmdContext& context) {
            return context.service.SetForceLight(enabled != 0);
        });
    }

    // 下发控制器复位命令。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ResetController(
        MeyerDeviceCmdHandle handle)
    {
        return Invoke(handle, [](DeviceCmdContext& context) {
            return context.service.ResetController();
        });
    }

    // 固化 13 位机器码。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreMachineCode(
        MeyerDeviceCmdHandle handle,
        const char* machineCodeUtf8)
    {
        return Invoke(handle, [machineCodeUtf8](DeviceCmdContext& context) {
            return context.service.StoreMachineCode(machineCodeUtf8);
        });
    }

    // 读取 16 字节相机参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCameraParameters(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdCameraParameters* parameters)
    {
        return Invoke(handle, [parameters](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(parameters))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadCameraParameters(*parameters);
        });
    }

    // 固化 16 字节相机参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCameraParameters(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCameraParameters* parameters)
    {
        return Invoke(handle, [parameters](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(parameters))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreCameraParameters(*parameters);
        });
    }

    // 在线设置相机开窗位置。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetCameraWindowPosition(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCameraWindowPosition* position)
    {
        return Invoke(handle, [position](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(position))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.SetCameraWindowPosition(*position);
        });
    }

    // 读取 416 字节颜色校正矩阵。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadColorMatrix(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdColorMatrix* matrix)
    {
        return Invoke(handle, [matrix](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(matrix))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadColorMatrix(*matrix);
        });
    }

    // 固化 416 字节颜色校正矩阵。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreColorMatrix(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdColorMatrix* matrix)
    {
        return Invoke(handle, [matrix](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(matrix))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreColorMatrix(*matrix);
        });
    }

    // 读取三个温度通道的原始毫伏值。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadTemperature(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdTemperature* temperature)
    {
        return Invoke(handle, [temperature](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(temperature))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadTemperature(*temperature);
        });
    }

    // 设置 18、20、22 或 25 帧/秒。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetFrameRate(
        MeyerDeviceCmdHandle handle,
        std::int32_t framesPerSecond)
    {
        return Invoke(handle, [framesPerSecond](DeviceCmdContext& context) {
            return context.service.SetFrameRate(framesPerSecond);
        });
    }

    // 擦除固件并读取一条进度响应。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_EraseFirmware(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdFirmwareEraseProgress* progress,
        std::uint32_t timeoutMs)
    {
        return Invoke(handle, [progress, timeoutMs](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(progress))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.EraseFirmware(*progress, timeoutMs);
        });
    }

    // 写入一个固件分包并读取设备确认的包序信息。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_WriteFirmwarePacket(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdFirmwareWritePacket* packet,
        MeyerDeviceCmdFirmwareWriteProgress* progress,
        std::uint32_t timeoutMs)
    {
        return Invoke(handle, [packet, progress, timeoutMs](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(packet) || !HasValidHeader(progress))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.WriteFirmwarePacket(*packet, *progress, timeoutMs);
        });
    }

    // 读取相机 1 标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCamera1Calibration(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdCameraCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadCamera1Calibration(*calibration);
        });
    }

    // 固化相机 1 标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCamera1Calibration(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCameraCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreCamera1Calibration(*calibration);
        });
    }

    // 读取相机 2 标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCamera2Calibration(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdCameraCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadCamera2Calibration(*calibration);
        });
    }

    // 固化相机 2 标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCamera2Calibration(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCameraCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreCamera2Calibration(*calibration);
        });
    }

    // 读取颜色标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadColorCalibration(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdColorCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadColorCalibration(*calibration);
        });
    }

    // 固化颜色标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreColorCalibration(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdColorCalibration* calibration)
    {
        return Invoke(handle, [calibration](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(calibration))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreColorCalibration(*calibration);
        });
    }

    // 读取设备加密、机器码和期限原始数据。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadDeviceInfo(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdDeviceInfo* info)
    {
        return Invoke(handle, [info](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(info))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadDeviceInfo(*info);
        });
    }

    // 固化设备加密、机器码和期限原始数据。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreDeviceInfo(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdDeviceInfo* info)
    {
        return Invoke(handle, [info](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(info))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StoreDeviceInfo(*info);
        });
    }

    // 在线设置曝光参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetExposureParameters(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdExposureParameters* parameters)
    {
        return Invoke(handle, [parameters](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(parameters))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.SetExposureParameters(*parameters);
        });
    }

    // 读取曝光参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadExposureParameters(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdExposureParameters* parameters)
    {
        return Invoke(handle, [parameters](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(parameters))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ReadExposureParameters(*parameters);
        });
    }

    // 原始命令入口用于协议诊断和暂未封装的低频命令，仍经过参数和异常边界检查。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ExecuteRawCommand(
        MeyerDeviceCmdHandle handle,
        std::uint8_t commandCode,
        const unsigned char* payload,
        std::size_t payloadSize,
        std::int32_t expectedResponseCode,
        MeyerDeviceCmdRawResponse* response,
        std::uint32_t timeoutMs)
    {
        return Invoke(handle, [commandCode, payload, payloadSize, expectedResponseCode, response, timeoutMs](DeviceCmdContext& context) -> std::int32_t {
            if (payloadSize > 0U && payload == nullptr)
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            if (response != nullptr && !HasValidHeader(response))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.ExecuteRawCommand(commandCode,
                                                     payload,
                                                     payloadSize,
                                                     expectedResponseCode,
                                                     response,
                                                     timeoutMs);
        });
    }

    // 启动底层 B 类图像流，并由服务层随后发送 0x0A。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StartCapture(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCaptureParams* params)
    {
        return Invoke(handle, [params](DeviceCmdContext& context) -> std::int32_t {
            if (!HasValidHeader(params))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.StartCapture(*params);
        });
    }

    // 停止底层图像流并按调用方选择决定是否关灯。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StopCapture(
        MeyerDeviceCmdHandle handle,
        std::int32_t turnLightOff)
    {
        return Invoke(handle, [turnLightOff](DeviceCmdContext& context) {
            return context.service.StopCapture(turnLightOff != 0);
        });
    }

    // 非阻塞读取一帧完整图像；没有完整帧时返回 NotReady。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetFrame(
        MeyerDeviceCmdHandle handle,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* frameBytes,
        MeyerDeviceCmdFrameInfo* frameInfo)
    {
        if (frameBytes != nullptr)
        {
            *frameBytes = 0U;
        }
        return Invoke(handle, [buffer, capacity, frameBytes, frameInfo](DeviceCmdContext& context) -> std::int32_t {
            if (buffer == nullptr || capacity == 0U || frameBytes == nullptr || !HasValidHeader(frameInfo))
            {
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return context.service.GetFrame(buffer, capacity, *frameBytes, *frameInfo);
        });
    }

    // 通过调用方缓冲区读取最近错误，支持先传空缓冲区获取所需长度。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetLastError(
        MeyerDeviceCmdHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize)
    {
        return Invoke(handle, [buffer, capacity, requiredSize](DeviceCmdContext& context) {
            return CopyError(context.service.LastError(), buffer, capacity, requiredSize);
        });
    }

    // 返回稳定模块名，字符串由 DLL 静态持有，调用方不得释放。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetModuleName()
    {
        return ::ModuleInfo::Name;
    }

    // 返回公共语义 API 版本字符串，字符串由 DLL 静态持有。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetApiVersion()
    {
        return ::ModuleInfo::ApiVersion;
    }

    // 返回跨自研 DLL 动态加载时使用的整数 ABI 版本。
    MEYERSCAN_DEVICE_CMD_API std::int32_t GetMeyerModuleApiVersion()
    {
        return ::ModuleInfo::ApiVersionNumber;
    }

    // 返回版本清单使用的代码版本字符串。
    MEYERSCAN_DEVICE_CMD_API const char* GetMeyerModuleVersion()
    {
        return ::ModuleInfo::Version;
    }
}
