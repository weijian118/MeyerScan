// =============================================================================
// 文件: DeviceTransportApi.cpp
// 作用: 把稳定的 C ABI 参数转换为内部 C++ 会话调用，并在 DLL 边界统一完成
//       参数校验、互斥、异常拦截和错误文本保存。
// =============================================================================

#include "../../include/DeviceTransport.h"

#include "DeviceTypes.h"
#include "../core/DeviceSession.h"
#include "../model/ImageFrame.h"
#include "../model/TransportConfig.h"
#include "../support/ModuleInfo.h"
#include "../support/ModuleLogger.h"

#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace
{
    // 一个公共句柄对应一个内部服务对象。互斥锁保证同一句柄的公开调用串行执行。
    struct DeviceTransportContext
    {
        meyer::device::DeviceFacade facade;
        std::mutex mutex;
        std::string lastError;
        meyer::device::ImageFrame pendingFrame;
        bool hasPendingFrame;

        DeviceTransportContext()
            : hasPendingFrame(false)
        {
        }
    };

    // 把不透明 C 句柄恢复成 DLL 内部对象指针。
    DeviceTransportContext* ToContext(MeyerDeviceTransportHandle handle)
    {
        return static_cast<DeviceTransportContext*>(handle);
    }

    // 记录本次失败原因并返回对应错误码，避免每个接口重复相同代码。
    std::int32_t Fail(DeviceTransportContext* context,
                      MeyerDeviceTransportResult result,
                      const char* message)
    {
        if (context != nullptr)
        {
            context->lastError = message == nullptr ? "Unknown device transport error" : message;
        }
        return static_cast<std::int32_t>(result);
    }

    // 成功后清空旧错误，防止调用方误读上一次失败信息。
    std::int32_t Succeed(DeviceTransportContext* context)
    {
        if (context != nullptr)
        {
            context->lastError.clear();
        }
        return static_cast<std::int32_t>(MeyerDeviceTransportResult_Ok);
    }

    // 所有可能分配内存或调用第三方 SDK 的公开操作都经过此函数。
    // 该技巧把 C++ 异常截留在 DLL 内部，防止异常跨 C ABI 破坏调用进程。
    template<typename Callable>
    std::int32_t Invoke(MeyerDeviceTransportHandle handle,
                        const char* operation,
                        const Callable& callable)
    {
        DeviceTransportContext* context = ToContext(handle);
        if (context == nullptr)
        {
            return static_cast<std::int32_t>(MeyerDeviceTransportResult_InvalidHandle);
        }

        std::lock_guard<std::mutex> lock(context->mutex);
        try
        {
            const std::int32_t result = callable(*context);
            // NotReady/Timeout/BufferTooSmall 是轮询和两次缓冲区调用的正常控制流，
            // 其余失败统一进入日志，避免每个公开函数重复错误日志代码。
            if (result < 0 &&
                result != MeyerDeviceTransportResult_NotReady &&
                result != MeyerDeviceTransportResult_Timeout &&
                result != MeyerDeviceTransportResult_BufferTooSmall)
            {
                meyer::device::logging::WriteError(
                    operation == nullptr ? "UnknownOperation" : operation,
                    context->lastError.c_str());
            }
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return Fail(context, MeyerDeviceTransportResult_InternalError,
                        "Memory allocation failed in device transport");
        }
        catch (const std::exception& exception)
        {
            context->lastError = operation == nullptr ? "Device transport exception" : operation;
            context->lastError += ": ";
            context->lastError += exception.what();
            return static_cast<std::int32_t>(MeyerDeviceTransportResult_InternalError);
        }
        catch (...)
        {
            context->lastError = operation == nullptr ? "Unknown device transport exception" : operation;
            return static_cast<std::int32_t>(MeyerDeviceTransportResult_InternalError);
        }
    }

    // 校验调用方是否使用了本模块支持的结构版本和完整结构长度。
    template<typename Structure>
    bool HasValidHeader(const Structure* value)
    {
        return value != nullptr &&
               value->structSize >= sizeof(Structure) &&
               value->schemaVersion == MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION;
    }

    // 将公共打开参数复制到内部类型。固定数组 host 会在边界内转成 std::string。
    meyer::device::TransportConfig BuildTransportConfig(const MeyerDeviceTransportOpenParams& params)
    {
        meyer::device::TransportConfig config;
        config.type = static_cast<meyer::device::TransportType>(params.transportType);
        config.vendorId = params.vendorId;
        config.productId = params.productId;
        config.deviceIndex = params.deviceIndex;
        config.commandTimeoutMs = params.commandTimeoutMs;
        config.streamTimeoutMs = params.streamTimeoutMs;
        config.host.assign(params.host,
                           params.host + strnlen(params.host, sizeof(params.host)));
        config.port = params.port;
        return config;
    }

    // 将公共采集参数转换为内部组帧配置。
    meyer::device::CaptureConfig BuildCaptureConfig(const MeyerDeviceCaptureStartParams& params)
    {
        meyer::device::CaptureConfig config;
        config.width = params.width;
        config.height = params.height;
        config.imageCount = params.imageCount;
        config.packetsPerImage = params.packetsPerImage;
        config.transferSize = static_cast<std::size_t>(params.transferSize);
        config.queueDepth = params.queueDepth;
        config.packetPayloadSize = params.packetPayloadSize;
        config.lastPacketValidSize = params.lastPacketValidSize;
        config.timeoutMs = params.timeoutMs;
        config.maxReadyFrames = params.maxReadyFrames;
        return config;
    }

    // 把内部帧元数据复制到初始化过的公共 POD 结构中。
    void FillFrameInfo(const meyer::device::ImageFrame& frame,
                       MeyerDeviceFrameInfo& frameInfo)
    {
        frameInfo.width = frame.width;
        frameInfo.height = frame.height;
        frameInfo.imageCount = frame.imageCount;
        frameInfo.deviceType = static_cast<std::int32_t>(frame.deviceType);
        frameInfo.captureStatus = static_cast<std::int32_t>(frame.status.captureStatus);
        frameInfo.scanMode = static_cast<std::int32_t>(frame.status.scanMode);
        frameInfo.pictureOrderMode = static_cast<std::int32_t>(frame.status.pictureOrderMode);
        frameInfo.scanHeadType = frame.status.scanHeadType;
        frameInfo.ledOn = frame.status.ledOn ? 1 : 0;
        frameInfo.photoMode = frame.status.photoMode ? 1 : 0;
        frameInfo.timeW = frame.status.timeW;
        frameInfo.timeC = frame.status.timeC;
        frameInfo.timeX = frame.status.timeX;
        frameInfo.gainW = frame.status.gainW;
        frameInfo.gainC = frame.status.gainC;
        frameInfo.gainX = frame.status.gainX;
        frameInfo.temperature0 = frame.status.temperature0;
        frameInfo.temperature1 = frame.status.temperature1;
        frameInfo.temperature2 = frame.status.temperature2;
        frameInfo.temperature3 = frame.status.temperature3;
        frameInfo.frameBytes = static_cast<std::uint64_t>(frame.pixels.size());
    }

    // 校验设备型号，禁止未经校验的整数直接 static_cast 后进入协议解析。
    bool IsValidDeviceType(std::int32_t value)
    {
        return value == MeyerDeviceType_Unknown ||
               value == MeyerDeviceType_Skys1000 ||
               value == MeyerDeviceType_Three;
    }

    // 校验历史图像排列枚举。
    bool IsValidPictureOrder(std::int32_t value)
    {
        return value == MeyerPictureOrderMode_Old ||
               value == MeyerPictureOrderMode_New ||
               value == MeyerPictureOrderMode_Aes;
    }

    // 校验采集用途枚举。
    bool IsValidScanMode(std::int32_t value)
    {
        return value == MeyerCaptureScanMode_Scan ||
               value == MeyerCaptureScanMode_Calibration3D ||
               value == MeyerCaptureScanMode_CalibrationColor;
    }

    // 在创建线程或分配缓冲区之前验证采集参数。这里使用 uint64_t 计算中间值，
    // 并先限制每个乘数，因此 width*height*imageCount 不会发生整数回绕。
    bool ValidateCaptureParams(const MeyerDeviceCaptureStartParams& params,
                               const char*& errorMessage)
    {
        errorMessage = "Capture configuration is invalid";
        if (params.width <= 0 || params.height <= 0 ||
            params.imageCount <= 0 || params.packetsPerImage <= 0 ||
            params.packetPayloadSize <= 0 || params.transferSize == 0U ||
            params.queueDepth == 0U || params.timeoutMs == 0U ||
            params.maxReadyFrames == 0U)
        {
            errorMessage = "Capture dimensions, packet values, timeout and queue sizes must be greater than zero";
            return false;
        }
        if (params.width > static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_DIMENSION) ||
            params.height > static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_DIMENSION) ||
            params.imageCount > static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_IMAGE_COUNT) ||
            params.packetsPerImage > static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_PACKETS_PER_IMAGE))
        {
            errorMessage = "Capture dimensions, image count or packet count exceeds the supported limit";
            return false;
        }
        if (params.transferSize > MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE ||
            params.packetPayloadSize > static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
            params.transferSize < static_cast<std::uint64_t>(params.packetPayloadSize) ||
            params.queueDepth > MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH ||
            params.maxReadyFrames > MEYER_DEVICE_TRANSPORT_MAX_READY_FRAMES ||
            params.timeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS)
        {
            errorMessage = "Capture transfer, queue, ready-frame or timeout value exceeds the supported limit";
            return false;
        }

        const std::uint64_t planeBytes =
            static_cast<std::uint64_t>(params.width) * static_cast<std::uint64_t>(params.height);
        const std::uint64_t frameBytes =
            planeBytes * static_cast<std::uint64_t>(params.imageCount);
        const std::uint64_t packetBytesBeforeLast =
            static_cast<std::uint64_t>(params.packetPayloadSize) *
            static_cast<std::uint64_t>(params.packetsPerImage - 1);
        if (planeBytes <= packetBytesBeforeLast)
        {
            errorMessage = "Packet count and payload size exceed one image plane";
            return false;
        }

        const std::uint64_t expectedLastPacketBytes = planeBytes - packetBytesBeforeLast;
        if (expectedLastPacketBytes > static_cast<std::uint64_t>(params.packetPayloadSize) ||
            (params.lastPacketValidSize > 0 &&
             expectedLastPacketBytes != static_cast<std::uint64_t>(params.lastPacketValidSize)))
        {
            errorMessage = "Packet count, payload size and last packet size do not cover one image plane exactly";
            return false;
        }
        if (params.lastPacketValidSize < 0 ||
            params.lastPacketValidSize > params.packetPayloadSize)
        {
            errorMessage = "Last packet size is outside the payload range";
            return false;
        }

        const std::uint64_t inFlightBytes =
            params.transferSize * static_cast<std::uint64_t>(params.queueDepth);
        // 额外八份是对组帧工作区、后处理临时帧、最近帧和 ABI pending 帧
        // 的保守估算；宁可提前拒绝异常配置，也不在采集期间接近内存上限。
        const std::uint64_t retainedFrameCopies =
            static_cast<std::uint64_t>(params.maxReadyFrames) + 8ULL;
        if (frameBytes > MEYER_DEVICE_TRANSPORT_MAX_FRAME_BYTES ||
            inFlightBytes > MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY ||
            frameBytes > MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY / retainedFrameCopies)
        {
            errorMessage = "Capture configuration exceeds the module memory budget";
            return false;
        }
        return true;
    }
}

extern "C"
{
    // 初始化打开参数，并把全部预留字节清零。
    std::int32_t MeyerDeviceTransport_InitOpenParams(MeyerDeviceTransportOpenParams* params)
    {
        if (params == nullptr)
        {
            return MeyerDeviceTransportResult_InvalidArgument;
        }

        std::memset(params, 0, sizeof(*params));
        params->structSize = sizeof(*params);
        params->schemaVersion = MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION;
        params->transportType = MeyerDeviceTransportType_CyApiUsb;
        params->vendorId = 0x04B4;
        params->productId = 0x00F1;
        // 正式入口默认遍历全部 CyAPI 设备，避免其它 Cypress 设备占用索引 0
        // 时漏掉实际口扫设备；测试工具仍可覆盖为明确索引。
        params->deviceIndex = MEYER_DEVICE_TRANSPORT_AUTO_DEVICE_INDEX;
        params->commandTimeoutMs = 1500U;
        params->streamTimeoutMs = 1500U;
        return MeyerDeviceTransportResult_Ok;
    }

    // 初始化采集参数；调用方仍必须设置真实尺寸和分包信息。
    std::int32_t MeyerDeviceTransport_InitCaptureParams(MeyerDeviceCaptureStartParams* params)
    {
        if (params == nullptr)
        {
            return MeyerDeviceTransportResult_InvalidArgument;
        }

        std::memset(params, 0, sizeof(*params));
        params->structSize = sizeof(*params);
        params->schemaVersion = MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION;
        params->transferSize = 16384U;
        params->queueDepth = 16U;
        params->packetPayloadSize = 16384;
        params->timeoutMs = 1500U;
        params->maxReadyFrames = 3U;
        return MeyerDeviceTransportResult_Ok;
    }

    // 初始化帧信息结构，供 GetFrame 校验和回填。
    std::int32_t MeyerDeviceTransport_InitFrameInfo(MeyerDeviceFrameInfo* frameInfo)
    {
        if (frameInfo == nullptr)
        {
            return MeyerDeviceTransportResult_InvalidArgument;
        }

        std::memset(frameInfo, 0, sizeof(*frameInfo));
        frameInfo->structSize = sizeof(*frameInfo);
        frameInfo->schemaVersion = MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION;
        return MeyerDeviceTransportResult_Ok;
    }

    // 创建内部上下文；nothrow 保证内存不足时返回空句柄而不是抛出异常。
    MeyerDeviceTransportHandle MeyerDeviceTransport_Create()
    {
        return new (std::nothrow) DeviceTransportContext();
    }

    // 销毁句柄会触发内部析构，析构负责停止采集并关闭 USB 连接。
    void MeyerDeviceTransport_Destroy(MeyerDeviceTransportHandle handle)
    {
        delete ToContext(handle);
    }

    // 打开指定设备。当前正式支持 CyAPI USB，其他传输类型明确返回不支持。
    std::int32_t MeyerDeviceTransport_Open(MeyerDeviceTransportHandle handle,
                                           const MeyerDeviceTransportOpenParams* params)
    {
        return Invoke(handle, "Open", [params](DeviceTransportContext& context) -> std::int32_t {
            if (!HasValidHeader(params))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Open parameters have an invalid size or schema version");
            }
            if (params->transportType != MeyerDeviceTransportType_CyApiUsb)
            {
                return Fail(&context, MeyerDeviceTransportResult_UnsupportedTransport,
                            "Only CyAPI USB transport is implemented");
            }
            if (params->commandTimeoutMs == 0U || params->streamTimeoutMs == 0U)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Command and stream timeouts must be greater than zero");
            }
            if (params->commandTimeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS ||
                params->streamTimeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Command or stream timeout exceeds the supported limit");
            }

            context.facade.Close();
            context.pendingFrame.Clear();
            context.hasPendingFrame = false;
            if (!context.facade.Open(BuildTransportConfig(*params)))
            {
                return Fail(&context, MeyerDeviceTransportResult_DeviceNotFound,
                            "No matching CyAPI USB device could be opened");
            }
            meyer::device::logging::WriteInfo("Open", "Device connection opened");
            return Succeed(&context);
        });
    }

    // 停止采集并关闭设备，允许重复调用。
    std::int32_t MeyerDeviceTransport_Close(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "Close", [](DeviceTransportContext& context) -> std::int32_t {
            context.facade.Close();
            context.pendingFrame.Clear();
            context.hasPendingFrame = false;
            meyer::device::logging::WriteInfo("Close", "Device connection closed");
            return Succeed(&context);
        });
    }

    // 返回 1/0 表示连接状态；负数仍表示句柄无效。
    std::int32_t MeyerDeviceTransport_IsOpen(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "IsOpen", [](DeviceTransportContext& context) -> std::int32_t {
            context.lastError.clear();
            return context.facade.IsOpen() ? 1 : 0;
        });
    }

    // 使用最近一次成功接收的配置重新连接设备。
    std::int32_t MeyerDeviceTransport_Reconnect(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "Reconnect", [](DeviceTransportContext& context) -> std::int32_t {
            if (!context.facade.Reconnect())
            {
                return Fail(&context, MeyerDeviceTransportResult_DeviceNotFound,
                            "Device reconnect failed");
            }
            meyer::device::logging::WriteInfo("Reconnect", "Device connection restored");
            return Succeed(&context);
        });
    }

    // 发送原始设备命令字节。
    std::int32_t MeyerDeviceTransport_SendCommand(MeyerDeviceTransportHandle handle,
                                                  const unsigned char* data,
                                                  std::size_t size,
                                                  std::uint32_t timeoutMs)
    {
        return Invoke(handle, "SendCommand", [data, size, timeoutMs](DeviceTransportContext& context) -> std::int32_t {
            if (data == nullptr || size == 0U ||
                size > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                timeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Command data or timeout is outside the supported range");
            }
            if (!context.facade.IsOpen())
            {
                return Fail(&context, MeyerDeviceTransportResult_NotOpen,
                            "Device is not open");
            }
            if (!context.facade.SendRawCommand(data, size, timeoutMs))
            {
                return Fail(&context, MeyerDeviceTransportResult_IoFailed,
                            "Command transfer failed");
            }
            meyer::device::logging::WriteInfo("SendCommand", "Device command sent");
            return Succeed(&context);
        });
    }

    // 接收命令响应并返回实际字节数。
    std::int32_t MeyerDeviceTransport_ReceiveCommand(MeyerDeviceTransportHandle handle,
                                                     unsigned char* buffer,
                                                     std::size_t capacity,
                                                     std::size_t* receivedSize,
                                                     std::uint32_t timeoutMs)
    {
        if (receivedSize != nullptr)
        {
            *receivedSize = 0U;
        }
        return Invoke(handle, "ReceiveCommand", [buffer, capacity, receivedSize, timeoutMs](DeviceTransportContext& context) -> std::int32_t {
            if (buffer == nullptr || capacity == 0U || receivedSize == nullptr ||
                capacity > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                timeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Command receive buffer or timeout is outside the supported range");
            }
            if (!context.facade.IsOpen())
            {
                return Fail(&context, MeyerDeviceTransportResult_NotOpen,
                            "Device is not open");
            }
            if (!context.facade.ReceiveCommand(buffer, capacity, *receivedSize, timeoutMs))
            {
                return Fail(&context, MeyerDeviceTransportResult_Timeout,
                            "Command response was not received before timeout");
            }
            return Succeed(&context);
        });
    }

    // 启动原始数据流。
    std::int32_t MeyerDeviceTransport_StartStream(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "StartStream", [](DeviceTransportContext& context) -> std::int32_t {
            if (!context.facade.IsOpen())
            {
                return Fail(&context, MeyerDeviceTransportResult_NotOpen,
                            "Device is not open");
            }
            if (!context.facade.StartStream())
            {
                return Fail(&context, MeyerDeviceTransportResult_IoFailed,
                            "Failed to start stream");
            }
            meyer::device::logging::WriteInfo("StartStream", "Raw device stream started");
            return Succeed(&context);
        });
    }

    // 预提交 CyAPI 异步读请求；队列深度由底层再次限制，防止异常内存占用。
    std::int32_t MeyerDeviceTransport_PrimeStream(MeyerDeviceTransportHandle handle,
                                                  std::size_t transferSize,
                                                  std::size_t queueDepth)
    {
        return Invoke(handle, "PrimeStream", [transferSize, queueDepth](DeviceTransportContext& context) -> std::int32_t {
            if (transferSize == 0U || queueDepth == 0U ||
                transferSize > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                queueDepth > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH) ||
                transferSize > static_cast<std::size_t>(
                    MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY / queueDepth))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Transfer size or queue depth is outside the supported range");
            }
            if (!context.facade.PrimeStream(transferSize, queueDepth))
            {
                return Fail(&context, MeyerDeviceTransportResult_IoFailed,
                            "Failed to prime stream transfer queue");
            }
            return Succeed(&context);
        });
    }

    // 停止原始流并回收全部异步请求。
    std::int32_t MeyerDeviceTransport_StopStream(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "StopStream", [](DeviceTransportContext& context) -> std::int32_t {
            context.facade.StopStream();
            meyer::device::logging::WriteInfo("StopStream", "Raw device stream stopped");
            return Succeed(&context);
        });
    }

    // 接收一个原始流数据包。
    std::int32_t MeyerDeviceTransport_ReceiveStreamPacket(MeyerDeviceTransportHandle handle,
                                                          unsigned char* buffer,
                                                          std::size_t capacity,
                                                          std::size_t* receivedSize,
                                                          std::uint32_t timeoutMs)
    {
        if (receivedSize != nullptr)
        {
            *receivedSize = 0U;
        }
        return Invoke(handle, "ReceiveStreamPacket", [buffer, capacity, receivedSize, timeoutMs](DeviceTransportContext& context) -> std::int32_t {
            if (buffer == nullptr || capacity == 0U || receivedSize == nullptr ||
                capacity > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
                timeoutMs > MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Stream receive buffer or timeout is outside the supported range");
            }
            if (!context.facade.ReceiveStreamPacket(buffer, capacity, *receivedSize, timeoutMs))
            {
                return Fail(&context, MeyerDeviceTransportResult_Timeout,
                            "Stream packet was not received before timeout");
            }
            return Succeed(&context);
        });
    }

    // 查询 CyAPI 枚举到的设备数量。
    std::int32_t MeyerDeviceTransport_GetDeviceCount(MeyerDeviceTransportHandle handle,
                                                     std::int32_t* deviceCount)
    {
        return Invoke(handle, "GetDeviceCount", [deviceCount](DeviceTransportContext& context) -> std::int32_t {
            if (deviceCount == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "deviceCount output is null");
            }
            *deviceCount = context.facade.GetDeviceCount();
            return Succeed(&context);
        });
    }

    // 查询当前打开设备是否运行在 USB 2 模式。
    std::int32_t MeyerDeviceTransport_GetIsUsb2(MeyerDeviceTransportHandle handle,
                                                std::int32_t* isUsb2)
    {
        return Invoke(handle, "GetIsUsb2", [isUsb2](DeviceTransportContext& context) -> std::int32_t {
            if (isUsb2 == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "isUsb2 output is null");
            }
            *isUsb2 = context.facade.GetIsUSB2() ? 1 : 0;
            return Succeed(&context);
        });
    }

    // 设置设备型号，并在转换前校验枚举范围。
    std::int32_t MeyerDeviceTransport_SetDeviceType(MeyerDeviceTransportHandle handle,
                                                    std::int32_t deviceType)
    {
        return Invoke(handle, "SetDeviceType", [deviceType](DeviceTransportContext& context) -> std::int32_t {
            if (!IsValidDeviceType(deviceType))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Unknown device type");
            }
            context.facade.SetDeviceType(static_cast<meyer::device::DeviceType>(deviceType));
            return Succeed(&context);
        });
    }

    // 设置图像排列方式。
    std::int32_t MeyerDeviceTransport_SetPictureOrderMode(MeyerDeviceTransportHandle handle,
                                                          std::int32_t pictureOrderMode)
    {
        return Invoke(handle, "SetPictureOrderMode", [pictureOrderMode](DeviceTransportContext& context) -> std::int32_t {
            if (!IsValidPictureOrder(pictureOrderMode))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Unknown picture order mode");
            }
            context.facade.SetPictureOrderMode(static_cast<meyer::device::PictureOrderMode>(pictureOrderMode));
            return Succeed(&context);
        });
    }

    // 设置采集用途。
    std::int32_t MeyerDeviceTransport_SetCaptureScanMode(MeyerDeviceTransportHandle handle,
                                                         std::int32_t captureScanMode)
    {
        return Invoke(handle, "SetCaptureScanMode", [captureScanMode](DeviceTransportContext& context) -> std::int32_t {
            if (!IsValidScanMode(captureScanMode))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Unknown capture scan mode");
            }
            context.facade.SetCaptureScanMode(static_cast<meyer::device::CaptureScanMode>(captureScanMode));
            return Succeed(&context);
        });
    }

    // 启用或关闭 IMU 姿态解算。
    std::int32_t MeyerDeviceTransport_SetAhrsEnabled(MeyerDeviceTransportHandle handle,
                                                    std::int32_t enabled)
    {
        return Invoke(handle, "SetAhrsEnabled", [enabled](DeviceTransportContext& context) -> std::int32_t {
            context.facade.SetAhrsEnabled(enabled != 0);
            return Succeed(&context);
        });
    }

    // 暂停时保持参考姿态，供上层控制扫描过程中的姿态基准。
    std::int32_t MeyerDeviceTransport_SetImuPaused(MeyerDeviceTransportHandle handle,
                                                  std::int32_t paused)
    {
        return Invoke(handle, "SetImuPaused", [paused](DeviceTransportContext& context) -> std::int32_t {
            context.facade.SetImuPaused(paused != 0);
            return Succeed(&context);
        });
    }

    // 请求在下一份有效 IMU 数据到达时重置参考四元数。
    std::int32_t MeyerDeviceTransport_RequestImuReferenceReset(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "RequestImuReferenceReset", [](DeviceTransportContext& context) -> std::int32_t {
            context.facade.RequestImuReferenceReset();
            return Succeed(&context);
        });
    }

    // 设置扫描头类型。
    std::int32_t MeyerDeviceTransport_SetScanHeadType(MeyerDeviceTransportHandle handle,
                                                      std::int32_t scanHeadType)
    {
        return Invoke(handle, "SetScanHeadType", [scanHeadType](DeviceTransportContext& context) -> std::int32_t {
            if (scanHeadType < 0)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Scan head type cannot be negative");
            }
            context.facade.SetScanHeadType(scanHeadType);
            return Succeed(&context);
        });
    }

    // 启动采集线程和组帧流水线。
    std::int32_t MeyerDeviceTransport_StartCapture(MeyerDeviceTransportHandle handle,
                                                   const MeyerDeviceCaptureStartParams* params)
    {
        return Invoke(handle, "StartCapture", [params](DeviceTransportContext& context) -> std::int32_t {
            if (!HasValidHeader(params))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Capture parameters have an invalid size or schema version");
            }
            const char* validationError = nullptr;
            if (!ValidateCaptureParams(*params, validationError))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            validationError);
            }
            if (!context.facade.IsOpen())
            {
                return Fail(&context, MeyerDeviceTransportResult_NotOpen,
                            "Device is not open");
            }
            if (context.facade.IsCaptureActive())
            {
                return Fail(&context, MeyerDeviceTransportResult_AlreadyRunning,
                            "Capture is already active");
            }
            if (!context.facade.StartCapture(BuildCaptureConfig(*params)))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "Capture configuration is invalid or stream startup failed");
            }
            context.pendingFrame.Clear();
            context.hasPendingFrame = false;
            meyer::device::logging::WriteInfo("StartCapture", "Frame capture started");
            return Succeed(&context);
        });
    }

    // 停止采集线程并清空尚未交付的帧。
    std::int32_t MeyerDeviceTransport_StopCapture(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "StopCapture", [](DeviceTransportContext& context) -> std::int32_t {
            context.facade.StopCapture();
            context.pendingFrame.Clear();
            context.hasPendingFrame = false;
            meyer::device::logging::WriteInfo("StopCapture", "Frame capture stopped");
            return Succeed(&context);
        });
    }

    // 返回 1/0 表示采集状态；负数表示句柄无效。
    std::int32_t MeyerDeviceTransport_IsCaptureActive(MeyerDeviceTransportHandle handle)
    {
        return Invoke(handle, "IsCaptureActive", [](DeviceTransportContext& context) -> std::int32_t {
            context.lastError.clear();
            return context.facade.IsCaptureActive() ? 1 : 0;
        });
    }

    // 非阻塞读取一帧。缓冲区不足时保留 pendingFrame，调用方扩容后可再次读取。
    std::int32_t MeyerDeviceTransport_GetFrame(MeyerDeviceTransportHandle handle,
                                               unsigned char* buffer,
                                               std::size_t capacity,
                                               std::size_t* frameBytes,
                                               MeyerDeviceFrameInfo* frameInfo)
    {
        if (frameBytes != nullptr)
        {
            *frameBytes = 0U;
        }
        return Invoke(handle, "GetFrame", [buffer, capacity, frameBytes, frameInfo](DeviceTransportContext& context) -> std::int32_t {
            if (frameBytes == nullptr || !HasValidHeader(frameInfo))
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "frameBytes and initialized frameInfo are required");
            }
            if (!context.facade.IsCaptureActive())
            {
                return Fail(&context, MeyerDeviceTransportResult_NotRunning,
                            "Capture is not active");
            }
            if (!context.hasPendingFrame)
            {
                if (!context.facade.GetFrame(context.pendingFrame))
                {
                    return Fail(&context, MeyerDeviceTransportResult_NotReady,
                                "No complete frame is ready");
                }
                context.hasPendingFrame = true;
            }

            *frameBytes = context.pendingFrame.pixels.size();
            FillFrameInfo(context.pendingFrame, *frameInfo);
            if (buffer == nullptr || capacity < *frameBytes)
            {
                return Fail(&context, MeyerDeviceTransportResult_BufferTooSmall,
                            "Frame buffer is smaller than the ready frame");
            }
            if (*frameBytes > 0U)
            {
                std::memcpy(buffer, &context.pendingFrame.pixels[0], *frameBytes);
            }
            context.pendingFrame.Clear();
            context.hasPendingFrame = false;
            return Succeed(&context);
        });
    }

    // 查询最近一帧的采集状态。
    std::int32_t MeyerDeviceTransport_GetCaptureStatus(MeyerDeviceTransportHandle handle,
                                                       std::int32_t* captureStatus)
    {
        return Invoke(handle, "GetCaptureStatus", [captureStatus](DeviceTransportContext& context) -> std::int32_t {
            if (captureStatus == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "captureStatus output is null");
            }
            *captureStatus = context.facade.GetCaptureStatus();
            return Succeed(&context);
        });
    }

    // 读取最近一帧解析出的三组曝光时间和增益。
    std::int32_t MeyerDeviceTransport_GetExposureState(MeyerDeviceTransportHandle handle,
                                                       std::int32_t* timeW,
                                                       std::int32_t* timeC,
                                                       std::int32_t* timeX,
                                                       std::int32_t* gainW,
                                                       std::int32_t* gainC,
                                                       std::int32_t* gainX)
    {
        return Invoke(handle, "GetExposureState", [=](DeviceTransportContext& context) -> std::int32_t {
            if (timeW == nullptr || timeC == nullptr || timeX == nullptr ||
                gainW == nullptr || gainC == nullptr || gainX == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "All exposure outputs are required");
            }
            if (!context.facade.GetExposureState(*timeW, *timeC, *timeX, *gainW, *gainC, *gainX))
            {
                return Fail(&context, MeyerDeviceTransportResult_NotReady,
                            "No frame exposure state is ready");
            }
            return Succeed(&context);
        });
    }

    // 读取最近一帧解析出的温度。
    std::int32_t MeyerDeviceTransport_GetTemperatureState(MeyerDeviceTransportHandle handle,
                                                          std::int32_t* temperature0,
                                                          std::int32_t* temperature1,
                                                          std::int32_t* temperature2,
                                                          std::int32_t* temperature3)
    {
        return Invoke(handle, "GetTemperatureState", [=](DeviceTransportContext& context) -> std::int32_t {
            if (temperature0 == nullptr || temperature1 == nullptr ||
                temperature2 == nullptr || temperature3 == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "All temperature outputs are required");
            }
            if (!context.facade.GetTemperatureState(*temperature0, *temperature1,
                                                    *temperature2, *temperature3))
            {
                return Fail(&context, MeyerDeviceTransportResult_NotReady,
                            "No frame temperature state is ready");
            }
            return Succeed(&context);
        });
    }

    // 读取每个图像平面的曝光三元组；支持先探测 required count。
    std::int32_t MeyerDeviceTransport_GetThreeGainTime(MeyerDeviceTransportHandle handle,
                                                       std::int32_t* values,
                                                       std::size_t capacity,
                                                       std::size_t* actualCount)
    {
        if (actualCount != nullptr)
        {
            *actualCount = 0U;
        }
        return Invoke(handle, "GetThreeGainTime", [values, capacity, actualCount](DeviceTransportContext& context) -> std::int32_t {
            if (actualCount == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "actualCount output is null");
            }
            std::vector<int> source;
            if (!context.facade.GetThreeGainTime(source))
            {
                return Fail(&context, MeyerDeviceTransportResult_NotReady,
                            "No exposure array is ready");
            }
            *actualCount = source.size();
            if (values == nullptr || capacity < source.size())
            {
                return Fail(&context, MeyerDeviceTransportResult_BufferTooSmall,
                            "Exposure array buffer is too small");
            }
            for (std::size_t index = 0; index < source.size(); ++index)
            {
                values[index] = source[index];
            }
            return Succeed(&context);
        });
    }

    // 读取姿态欧拉角和四元数；支持先探测 required count。
    std::int32_t MeyerDeviceTransport_GetGyroscopeData(MeyerDeviceTransportHandle handle,
                                                       float* values,
                                                       std::size_t capacity,
                                                       std::size_t* actualCount)
    {
        if (actualCount != nullptr)
        {
            *actualCount = 0U;
        }
        return Invoke(handle, "GetGyroscopeData", [values, capacity, actualCount](DeviceTransportContext& context) -> std::int32_t {
            if (actualCount == nullptr)
            {
                return Fail(&context, MeyerDeviceTransportResult_InvalidArgument,
                            "actualCount output is null");
            }
            std::vector<float> source;
            if (!context.facade.GetGyroscopeData(source))
            {
                return Fail(&context, MeyerDeviceTransportResult_NotReady,
                            "No gyroscope result is ready");
            }
            *actualCount = source.size();
            if (values == nullptr || capacity < source.size())
            {
                return Fail(&context, MeyerDeviceTransportResult_BufferTooSmall,
                            "Gyroscope result buffer is too small");
            }
            for (std::size_t index = 0; index < source.size(); ++index)
            {
                values[index] = source[index];
            }
            return Succeed(&context);
        });
    }

    // 复制当前句柄的错误文本。requiredSize 包含字符串结尾的 '\0'。
    std::int32_t MeyerDeviceTransport_GetLastError(MeyerDeviceTransportHandle handle,
                                                   char* buffer,
                                                   std::size_t capacity,
                                                   std::size_t* requiredSize)
    {
        DeviceTransportContext* context = ToContext(handle);
        if (context == nullptr)
        {
            return MeyerDeviceTransportResult_InvalidHandle;
        }
        if (requiredSize == nullptr)
        {
            return MeyerDeviceTransportResult_InvalidArgument;
        }

        std::lock_guard<std::mutex> lock(context->mutex);
        *requiredSize = context->lastError.size() + 1U;
        if (buffer == nullptr || capacity < *requiredSize)
        {
            return MeyerDeviceTransportResult_BufferTooSmall;
        }
        std::memcpy(buffer, context->lastError.c_str(), *requiredSize);
        return MeyerDeviceTransportResult_Ok;
    }

    // 返回稳定模块名，用于日志和版本清单。
    const char* MeyerDeviceTransport_GetModuleName()
    {
        return ModuleInfo::Name;
    }

    // API 版本只描述公共接口合同，不附加模块名和构建日期。
    const char* MeyerDeviceTransport_GetApiVersion()
    {
        return ModuleInfo::ApiVersion;
    }

    // DeviceCmd/MainExe 在解析业务函数前先读取整数 ABI 版本。
    std::int32_t GetMeyerModuleApiVersion()
    {
        return ModuleInfo::ApiVersionNumber;
    }

    // 根版本管理器按统一导出名读取代码版本。
    const char* GetMeyerModuleVersion()
    {
        return ModuleInfo::Version;
    }
}
