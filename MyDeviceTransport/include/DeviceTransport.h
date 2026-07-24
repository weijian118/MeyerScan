// =============================================================================
// 文件: DeviceTransport.h
// 模块: MeyerScan_DeviceTransport.dll
// 模块版本: 1.3.0；公共语义 API: 1.1.0；整数 ABI: 2
//
// 作用:
//   定义设备传输模块唯一的公共 C ABI。上层通过不透明句柄完成设备连接、
//   命令收发、流数据接收和完整采集帧读取，不接触 CyAPI、线程或内部容器。
//
// ABI 约束:
//   1. 公共结构只使用固定宽度整数、固定数组和调用方管理的缓冲区。
//   2. 每个可扩展结构都带 structSize/schemaVersion/reserved。
//   3. DLL 内部分配的 C++ 对象不会跨 DLL 边界交给调用方释放。
//   4. 所有文本均为 UTF-8；当前模块仅支持 x64 Windows。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_DEVICE_TRANSPORT_EXPORTS)
#  define MEYERSCAN_DEVICE_TRANSPORT_API __declspec(dllexport)
#else
#  define MEYERSCAN_DEVICE_TRANSPORT_API __declspec(dllimport)
#endif

// 公共结构的首个版本号。后续增加字段时应追加在 reserved 之前或发布新版本。
static const std::uint32_t MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION = 2U;

// 公开参数上限既是调用约定，也是 DLL 的内存保护边界。调用方应在创建采集
// 参数时遵守这些值；DLL 仍会再次校验，不能依赖调用方自行保证合法性。
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_DIMENSION = 16384U;
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_IMAGE_COUNT = 64U;
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_PACKETS_PER_IMAGE = 65536U;
static const std::uint64_t MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE = 16ULL * 1024ULL * 1024ULL;
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH = 64U;
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_READY_FRAMES = 16U;
static const std::uint32_t MEYER_DEVICE_TRANSPORT_MAX_TIMEOUT_MS = 300000U;
static const std::uint64_t MEYER_DEVICE_TRANSPORT_MAX_FRAME_BYTES = 256ULL * 1024ULL * 1024ULL;
static const std::uint64_t MEYER_DEVICE_TRANSPORT_MAX_CAPTURE_MEMORY = 512ULL * 1024ULL * 1024ULL;

// deviceIndex 使用该值时，CyAPI 后端会按枚举顺序尝试全部设备，直到找到
// VID/PID 和 USB 速度都符合要求的设备。显式索引仍用于现场诊断指定设备。
static const std::uint32_t MEYER_DEVICE_TRANSPORT_AUTO_DEVICE_INDEX = 0xFFFFFFFFU;

// 设备传输接口的结果码。0 表示成功，负数表示失败或尚未就绪。
enum MeyerDeviceTransportResult : std::int32_t
{
    MeyerDeviceTransportResult_Ok = 0,
    MeyerDeviceTransportResult_InvalidHandle = -1,
    MeyerDeviceTransportResult_InvalidArgument = -2,
    MeyerDeviceTransportResult_UnsupportedTransport = -3,
    MeyerDeviceTransportResult_DeviceNotFound = -4,
    MeyerDeviceTransportResult_NotOpen = -5,
    MeyerDeviceTransportResult_IoFailed = -6,
    MeyerDeviceTransportResult_Timeout = -7,
    MeyerDeviceTransportResult_BufferTooSmall = -8,
    MeyerDeviceTransportResult_AlreadyRunning = -9,
    MeyerDeviceTransportResult_NotRunning = -10,
    MeyerDeviceTransportResult_NotReady = -11,
    MeyerDeviceTransportResult_InternalError = -12,
    // 连续两次流接收超时，表示设备数据流已经停滞，不再是单次调度抖动。
    MeyerDeviceTransportResult_StreamStalled = -13,
    // 采集期间底层设备句柄已失效，通常对应 USB 拔出或驱动连接丢失。
    MeyerDeviceTransportResult_DeviceDisconnected = -14
};

// 原始 B 包传输事件。该枚举只描述传输现象，不解释图像序号或扫描头等业务语义。
enum MeyerDeviceTransportStreamEvent : std::int32_t
{
    MeyerDeviceTransportStreamEvent_None = 0,
    MeyerDeviceTransportStreamEvent_PacketReceived = 1,
    MeyerDeviceTransportStreamEvent_ReceiveTimeout = 2,
    MeyerDeviceTransportStreamEvent_ConsecutiveTimeout = 3,
    MeyerDeviceTransportStreamEvent_DeviceDisconnected = 4,
    MeyerDeviceTransportStreamEvent_PartialPacket = 5,
    MeyerDeviceTransportStreamEvent_IoFailure = 6
};

// 当前正式实现的传输方式。预留其他值时不能在没有实现的情况下返回成功。
enum MeyerDeviceTransportType : std::int32_t
{
    MeyerDeviceTransportType_CyApiUsb = 1
};

// 扫描设备型号。数值沿用既有设备协议，避免改变下位机映射。
enum MeyerDeviceType : std::int32_t
{
    MeyerDeviceType_Unknown = 0,
    MeyerDeviceType_Skys1000 = 6,
    MeyerDeviceType_Three = 7
};

// 图像排列模式。Aes 仅表示历史协议模式，不代表本模块实现加解密。
enum MeyerPictureOrderMode : std::int32_t
{
    MeyerPictureOrderMode_Old = 1,
    MeyerPictureOrderMode_New = 2,
    MeyerPictureOrderMode_Aes = 3
};

// 采集用途会影响帧状态解析，但不负责上层 UI 流程。
enum MeyerCaptureScanMode : std::int32_t
{
    MeyerCaptureScanMode_Scan = 0,
    MeyerCaptureScanMode_Calibration3D = 1,
    MeyerCaptureScanMode_CalibrationColor = 2
};

// 打开设备所需参数。必须先调用 InitOpenParams，再按需修改字段。
struct MeyerDeviceTransportOpenParams
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t transportType;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint32_t deviceIndex;
    std::uint32_t commandTimeoutMs;
    std::uint32_t streamTimeoutMs;
    char host[128];
    std::uint16_t port;
    std::uint16_t reserved16;
    std::uint32_t reserved[8];
};

// 启动组帧采集所需参数。尺寸和分包值必须全部大于 0，并且不得超过上面的
// MAX_* 常量。lastPacketValidSize 为 0 时由 DLL 根据平面尺寸推导。
struct MeyerDeviceCaptureStartParams
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t packetsPerImage;
    std::uint64_t transferSize;
    std::uint32_t queueDepth;
    std::int32_t packetPayloadSize;
    std::int32_t lastPacketValidSize;
    std::uint32_t timeoutMs;
    std::uint32_t maxReadyFrames;
    std::uint32_t reserved[8];
};

// 一帧图像对应的元数据。像素内容由 GetFrame 写入调用方缓冲区。
struct MeyerDeviceFrameInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t deviceType;
    std::int32_t captureStatus;
    std::int32_t scanMode;
    std::int32_t pictureOrderMode;
    std::int32_t scanHeadType;
    std::int32_t ledOn;
    std::int32_t photoMode;
    std::int32_t timeW;
    std::int32_t timeC;
    std::int32_t timeX;
    std::int32_t gainW;
    std::int32_t gainC;
    std::int32_t gainX;
    std::int32_t temperature0;
    std::int32_t temperature1;
    std::int32_t temperature2;
    std::int32_t temperature3;
    std::uint64_t frameBytes;
    std::uint32_t reserved[8];
};

// 原始流诊断快照。调用方可以低频轮询该结构，不需要解析日志文本。
// 计数器在 StartStream/PrimeStream 开始新流时清零，StopStream 后保留末次统计供诊断。
struct MeyerDeviceTransportStreamDiagnostics
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t sequence;
    std::uint64_t totalPackets;
    std::uint64_t totalTimeouts;
    std::uint64_t totalPartialPackets;
    std::uint64_t totalIoFailures;
    std::int32_t consecutiveTimeouts;
    std::int32_t lastResult;
    std::int32_t lastEvent;
    std::int32_t streamActive;
    std::uint32_t queueDepth;
    std::uint32_t reserved32;
    std::uint64_t transferSize;
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceTransportStreamDiagnostics) == 112U,
              "MeyerDeviceTransportStreamDiagnostics ABI size changed");

// 不透明句柄隐藏内部 C++ 对象和第三方 SDK 类型。
typedef void* MeyerDeviceTransportHandle;

extern "C"
{
    // 初始化设备打开参数并写入安全默认值。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_InitOpenParams(MeyerDeviceTransportOpenParams* params);
    // 初始化采集参数；图像尺寸和分包数量仍由调用方填写。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_InitCaptureParams(MeyerDeviceCaptureStartParams* params);
    // 初始化帧元数据结构，供 GetFrame 校验版本并回填。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_InitFrameInfo(MeyerDeviceFrameInfo* frameInfo);
    // 初始化原始流诊断结构，设置结构大小、版本和空事件。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_InitStreamDiagnostics(MeyerDeviceTransportStreamDiagnostics* diagnostics);

    // 创建一个只管理单设备连接的不透明会话句柄。
    MEYERSCAN_DEVICE_TRANSPORT_API MeyerDeviceTransportHandle MeyerDeviceTransport_Create();
    // 停止采集、关闭连接并销毁会话句柄；允许传入空句柄。
    MEYERSCAN_DEVICE_TRANSPORT_API void MeyerDeviceTransport_Destroy(MeyerDeviceTransportHandle handle);

    // 按参数打开设备，并先清理同句柄上一次遗留状态。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_Open(MeyerDeviceTransportHandle handle, const MeyerDeviceTransportOpenParams* params);
    // 停止采集并关闭设备，重复调用仍返回成功。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_Close(MeyerDeviceTransportHandle handle);
    // 返回 1 表示已打开、0 表示未打开，负数表示错误。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_IsOpen(MeyerDeviceTransportHandle handle);
    // 使用最近一次打开配置重新连接设备。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_Reconnect(MeyerDeviceTransportHandle handle);

    // 发送原始命令字节，不解释命令业务语义。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SendCommand(MeyerDeviceTransportHandle handle, const unsigned char* data, std::size_t size, std::uint32_t timeoutMs);
    // 接收命令响应，receivedSize 返回本次实际收到的字节数。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_ReceiveCommand(MeyerDeviceTransportHandle handle, unsigned char* buffer, std::size_t capacity, std::size_t* receivedSize, std::uint32_t timeoutMs);

    // 进入原始数据流模式。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_StartStream(MeyerDeviceTransportHandle handle);
    // 建立固定传输长度和深度的 CyAPI 异步请求队列。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_PrimeStream(MeyerDeviceTransportHandle handle, std::size_t transferSize, std::size_t queueDepth);
    // 停止原始流并回收异步队列资源。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_StopStream(MeyerDeviceTransportHandle handle);
    // 接收一包原始流数据并返回实际字节数。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_ReceiveStreamPacket(MeyerDeviceTransportHandle handle, unsigned char* buffer, std::size_t capacity, std::size_t* receivedSize, std::uint32_t timeoutMs);
    // 复制当前原始流诊断快照；本函数只读内存，不触发 USB I/O。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetStreamDiagnostics(MeyerDeviceTransportHandle handle, MeyerDeviceTransportStreamDiagnostics* diagnostics);

    // 返回 CyAPI 枚举到的设备数量。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetDeviceCount(MeyerDeviceTransportHandle handle, std::int32_t* deviceCount);
    // 返回 1 表示 USB 2.x、0 表示 USB 3.x，负数表示错误。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetIsUsb2(MeyerDeviceTransportHandle handle, std::int32_t* isUsb2);

    // 设置后续组帧和协议解析使用的设备型号。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetDeviceType(MeyerDeviceTransportHandle handle, std::int32_t deviceType);
    // 设置图像平面排列模式。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetPictureOrderMode(MeyerDeviceTransportHandle handle, std::int32_t pictureOrderMode);
    // 设置扫描、三维校准或颜色校准用途。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetCaptureScanMode(MeyerDeviceTransportHandle handle, std::int32_t captureScanMode);
    // 启用或禁用 IMU 相对姿态计算。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetAhrsEnabled(MeyerDeviceTransportHandle handle, std::int32_t enabled);
    // 暂停或恢复向上层发布 IMU 姿态。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetImuPaused(MeyerDeviceTransportHandle handle, std::int32_t paused);
    // 请求下一份有效 IMU 样本重新建立参考零点。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_RequestImuReferenceReset(MeyerDeviceTransportHandle handle);
    // 设置尚无帧数据时使用的扫描头类型。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_SetScanHeadType(MeyerDeviceTransportHandle handle, std::int32_t scanHeadType);

    // 启动后台收包、组帧和状态解析流水线。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_StartCapture(MeyerDeviceTransportHandle handle, const MeyerDeviceCaptureStartParams* params);
    // 停止采集线程并清空尚未交付的帧。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_StopCapture(MeyerDeviceTransportHandle handle);
    // 返回 1 表示采集中、0 表示未采集，负数表示错误。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_IsCaptureActive(MeyerDeviceTransportHandle handle);
    // 非阻塞读取完整帧；当前无帧时立即返回 NotReady。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetFrame(MeyerDeviceTransportHandle handle, unsigned char* buffer, std::size_t capacity, std::size_t* frameBytes, MeyerDeviceFrameInfo* frameInfo);
    // 返回最近交付帧的四态采集状态。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetCaptureStatus(MeyerDeviceTransportHandle handle, std::int32_t* captureStatus);
    // 返回最近交付帧的三组曝光时间和增益。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetExposureState(MeyerDeviceTransportHandle handle, std::int32_t* timeW, std::int32_t* timeC, std::int32_t* timeX, std::int32_t* gainW, std::int32_t* gainC, std::int32_t* gainX);
    // 返回最近交付帧的四路温度字段。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetTemperatureState(MeyerDeviceTransportHandle handle, std::int32_t* temperature0, std::int32_t* temperature1, std::int32_t* temperature2, std::int32_t* temperature3);
    // 返回每个图像平面的曝光三元组展平数组。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetThreeGainTime(MeyerDeviceTransportHandle handle, std::int32_t* values, std::size_t capacity, std::size_t* actualCount);
    // 返回 roll/pitch/yaw 和四元数共七个 float 值。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetGyroscopeData(MeyerDeviceTransportHandle handle, float* values, std::size_t capacity, std::size_t* actualCount);

    // 读取当前句柄最近错误；支持空缓冲区的长度探测调用。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t MeyerDeviceTransport_GetLastError(MeyerDeviceTransportHandle handle, char* buffer, std::size_t capacity, std::size_t* requiredSize);

    // 返回由 DLL 静态持有的稳定模块名。
    MEYERSCAN_DEVICE_TRANSPORT_API const char* MeyerDeviceTransport_GetModuleName();
    // 返回公共 API 语义版本。
    MEYERSCAN_DEVICE_TRANSPORT_API const char* MeyerDeviceTransport_GetApiVersion();
    // 返回自研 DLL 动态加载门禁使用的整数 ABI 版本。
    MEYERSCAN_DEVICE_TRANSPORT_API std::int32_t GetMeyerModuleApiVersion();
    // 返回版本清单统一读取的代码版本。
    MEYERSCAN_DEVICE_TRANSPORT_API const char* GetMeyerModuleVersion();
}
