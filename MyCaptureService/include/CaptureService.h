// =============================================================================
// 文件: CaptureService.h
// 模块: MeyerScan_CaptureService.dll
// 作用: 定义设备原始图像采集编排层的唯一公共 C ABI。
//
// 设计边界:
//   1. 本模块不依赖 Qt；Qt 只存在于 CaptureServiceTest 或正式 UI 模块。
//   2. 本模块动态加载 DeviceCmd、CaptureProcessing 和 AutoExposure DLL，避免
//      把下层模块的导入库和 C++ 所有权带到 UI 边界。
//   3. 调用方只复制固定布局 POD 和调用方提供的字节缓冲区，不跨 DLL 传递
//      std::vector、std::string、QObject 或需要对方释放的内存。
//   4. 一个 Service 句柄只拥有一个设备命令会话；设备命令由采集线程串行调用。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_CAPTURE_SERVICE_EXPORTS)
#  define MEYER_CAPTURE_SERVICE_API __declspec(dllexport)
#else
#  define MEYER_CAPTURE_SERVICE_API __declspec(dllimport)
#endif

static const std::uint32_t MEYER_CAPTURE_SERVICE_SCHEMA_VERSION = 2U;
static const std::int32_t MEYER_CAPTURE_SERVICE_API_VERSION = 2;
static const std::size_t MEYER_CAPTURE_SERVICE_PATH_BYTES = 1024U;

// CaptureService 自己的结果码。下层 DeviceCmd 结果会通过 lastResult/event
// 保留；这样 UI 不需要把两个 DLL 的错误码混用。
enum MeyerCaptureServiceResult : std::int32_t
{
    MeyerCaptureServiceResult_Ok = 0,
    MeyerCaptureServiceResult_InvalidHandle = -1,
    MeyerCaptureServiceResult_InvalidArgument = -2,
    MeyerCaptureServiceResult_NotConfigured = -3,
    MeyerCaptureServiceResult_ModuleLoadFailed = -4,
    MeyerCaptureServiceResult_AbiMismatch = -5,
    MeyerCaptureServiceResult_PreflightRejected = -6,
    MeyerCaptureServiceResult_NotReady = -7,
    MeyerCaptureServiceResult_AlreadyRunning = -8,
    MeyerCaptureServiceResult_NotRunning = -9,
    MeyerCaptureServiceResult_DeviceDisconnected = -10,
    MeyerCaptureServiceResult_Faulted = -11,
    MeyerCaptureServiceResult_NoData = -12,
    MeyerCaptureServiceResult_BufferTooSmall = -13,
    MeyerCaptureServiceResult_QueueFull = -14,
    MeyerCaptureServiceResult_InternalError = -15
};

// Service 生命周期状态。Closed 表示没有设备采集会话，Faulted 表示需要停止
// 并重新执行准入流程，不能继续使用旧设备快照开始采集。
enum MeyerCaptureServiceState : std::int32_t
{
    MeyerCaptureServiceState_Created = 0,
    MeyerCaptureServiceState_Configured = 1,
    MeyerCaptureServiceState_Ready = 2,
    MeyerCaptureServiceState_Running = 3,
    MeyerCaptureServiceState_Stopping = 4,
    MeyerCaptureServiceState_Faulted = 5,
    MeyerCaptureServiceState_Closed = 6
};

// 事件严重级别由 UI 映射为信息、警告和错误样式；服务不依赖具体弹窗控件。
enum MeyerCaptureServiceEventSeverity : std::int32_t
{
    MeyerCaptureServiceEventSeverity_Info = 0,
    MeyerCaptureServiceEventSeverity_Warning = 1,
    MeyerCaptureServiceEventSeverity_Error = 2
};

// 结构化事件类型。textUtf8 只补充人类可读诊断，UI 应优先根据 type 和
// resultCode 选择 tr("English source") 文案。
enum MeyerCaptureServiceEventType : std::int32_t
{
    MeyerCaptureServiceEvent_None = 0,
    MeyerCaptureServiceEvent_ModuleLoaded = 1,
    MeyerCaptureServiceEvent_PreflightStarted = 2,
    MeyerCaptureServiceEvent_PreflightReady = 3,
    MeyerCaptureServiceEvent_PreflightRejected = 4,
    MeyerCaptureServiceEvent_CaptureStarted = 5,
    MeyerCaptureServiceEvent_CaptureStopped = 6,
    MeyerCaptureServiceEvent_DeviceDisconnected = 7,
    MeyerCaptureServiceEvent_ReceiveTimeout = 8,
    MeyerCaptureServiceEvent_StreamStalled = 9,
    MeyerCaptureServiceEvent_PartialPacket = 10,
    MeyerCaptureServiceEvent_InvalidPacket = 11,
    MeyerCaptureServiceEvent_SequenceReset = 12,
    MeyerCaptureServiceEvent_ProcessingError = 13,
    MeyerCaptureServiceEvent_PostProcessDropped = 14,
    MeyerCaptureServiceEvent_GroupReady = 15,
    MeyerCaptureServiceEvent_GroupProcessed = 16,
    MeyerCaptureServiceEvent_LightCommandQueued = 17,
    MeyerCaptureServiceEvent_LightCommandSent = 18,
    MeyerCaptureServiceEvent_CommandFailed = 19,
    MeyerCaptureServiceEvent_AutoExposureSkipped = 20,
    MeyerCaptureServiceEvent_AutoExposureReserved = 21,
    MeyerCaptureServiceEvent_StateChanged = 22,
    MeyerCaptureServiceEvent_InternalError = 23,
    MeyerCaptureServiceEvent_ImagePipelineReady = 24,
    MeyerCaptureServiceEvent_ImagePipelineFeatureUnavailable = 25
};

// 与 DeviceCmd 的后端枚举保持数值兼容，但 CaptureService 公共头不要求调用方
// 包含 DeviceCmd.h；这样未来替换命令模块时 UI ABI 不必跟着暴露底层头文件。
enum MeyerCaptureServiceBackendType : std::int32_t
{
    MeyerCaptureServiceBackend_DeviceTransport = 1,
    MeyerCaptureServiceBackend_SimulatorForTest = 2
};

// 仅供自动化测试和测试界面使用的故障注入标志。位值与当前 DeviceCmd
// 模拟后端保持一致，但正式业务模块只依赖本服务头文件，不需要包含 DeviceCmd.h。
// 真实设备后端会忽略这些标志，客户运行配置不得设置 simulatedFlags。
enum MeyerCaptureServiceSimulatedFlag : std::uint32_t
{
    MeyerCaptureServiceSimulatedFlag_None = 0U,
    MeyerCaptureServiceSimulatedFlag_StreamTimeoutOnce = 1U << 22,
    MeyerCaptureServiceSimulatedFlag_StreamTimeoutAlways = 1U << 23,
    MeyerCaptureServiceSimulatedFlag_DisconnectDuringCapture = 1U << 24,
    MeyerCaptureServiceSimulatedFlag_PartialStreamPacket = 1U << 25
};

// 打开和配置 CaptureService 的参数。路径为空时，模块会从自身 DLL 所在目录
// 推导同级 DLL；因此第三方从任意 current directory 拉起程序也不会加载错库。
struct MeyerCaptureServiceConfig
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t backendType;
    std::int32_t modelHint;
    std::int32_t captureMode;
    std::int32_t allowProductionMode;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint32_t deviceIndex;
    std::uint32_t commandTimeoutMs;
    std::uint32_t streamTimeoutMs;
    std::uint32_t eventQueueCapacity;
    std::uint32_t postProcessQueueCapacity;
    char deviceCmdLibraryPathUtf8[MEYER_CAPTURE_SERVICE_PATH_BYTES];
    char deviceTransportLibraryPathUtf8[MEYER_CAPTURE_SERVICE_PATH_BYTES];
    char captureProcessingLibraryPathUtf8[MEYER_CAPTURE_SERVICE_PATH_BYTES];
    char autoExposureLibraryPathUtf8[MEYER_CAPTURE_SERVICE_PATH_BYTES];
    char captureImagePipelineLibraryPathUtf8[MEYER_CAPTURE_SERVICE_PATH_BYTES];
    char simulatedDeviceIdUtf8[32];
    std::uint32_t simulatedFlags;
    std::uint32_t reserved[8];
};

// 设备快照是 DeviceCmd 预检结果的稳定副本。reported/effective 两套字段同时
// 保留，避免生产模式的兼容默认值覆盖设备真实回包证据。
struct MeyerCaptureServiceDeviceInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t validFields;
    std::int32_t preflightStatus;
    std::int32_t lastCommandResult;
    std::int32_t deviceConnected;
    std::int32_t usb2;
    std::int32_t productionMode;
    std::int32_t deviceNumberStatus;
    std::int32_t modelCodeStatus;
    std::int32_t deviceSeries;
    std::int32_t deviceProfile;
    std::int32_t productModel;
    std::int32_t deviceModel;
    std::int32_t mainFirmwareStatus;
    std::int32_t projectionFirmwareStatus;
    std::int32_t scanHeadColorPolicy;
    std::int32_t largeHeadColorStatus;
    std::int32_t smallHeadColorStatus;
    char deviceSeriesUtf8[64];
    char deviceProfileUtf8[64];
    char reportedDeviceIdUtf8[32];
    char effectiveDeviceIdUtf8[32];
    char deviceIdStatusUtf8[64];
    char reportedModelCodeUtf8[32];
    char effectiveModelCodeUtf8[32];
    char productModelUtf8[96];
    char mainFirmwareVersionUtf8[32];
    char projectionFirmwareVersionUtf8[32];
    char detailUtf8[512];
    std::uint32_t reserved[8];
};

// 采集状态快照用于 UI 轮询，不触发 USB I/O。计数器帮助现场区分“没有数据”
// 和“数据流已经停滞”。
struct MeyerCaptureServiceStateSnapshot
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t sequence;
    std::int32_t state;
    std::int32_t captureActive;
    std::int32_t latestDataAvailable;
    std::int32_t lastResult;
    std::int32_t postQueueSize;
    std::uint64_t latestGroupSequence;
    std::uint64_t droppedPostProcessGroups;
    std::uint64_t totalPackets;
    std::uint64_t totalTimeouts;
    std::uint64_t totalPartialPackets;
    std::int32_t consecutiveTimeouts;
    std::int32_t latestLedOn;
    std::int32_t latestLongPressed;
    std::int32_t latestScanHeadType;
    std::uint32_t latestPipelineOptionsRevision;
    std::uint64_t latestPipelineUnavailableFeatures;
    std::uint32_t reserved[8];
};

// 事件使用固定文本数组，PollEvent 成功后调用方可以直接保存该结构。
struct MeyerCaptureServiceEvent
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t sequence;
    std::int32_t type;
    std::int32_t severity;
    std::int32_t resultCode;
    std::int32_t groupSequenceLow;
    std::uint64_t groupSequence;
    std::uint64_t packetCount;
    std::int32_t consecutiveTimeouts;
    char textUtf8[512];
    std::uint32_t reserved[8];
};

typedef void* MeyerCaptureServiceHandle;

extern "C"
{
    // 初始化配置、设备信息、状态和事件结构，写入安全默认值。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitConfig(
        MeyerCaptureServiceConfig* config);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitDeviceInfo(
        MeyerCaptureServiceDeviceInfo* info);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitStateSnapshot(
        MeyerCaptureServiceStateSnapshot* snapshot);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitEvent(
        MeyerCaptureServiceEvent* eventInfo);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitPipelineOptions(
        MeyerCapturePipelineOptions* options);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_InitPipelineOutputInfo(
        MeyerCapturePipelineOutputInfo* info);

    // 创建/销毁服务对象。创建阶段不加载 DLL、不访问设备。
    MEYER_CAPTURE_SERVICE_API MeyerCaptureServiceHandle MeyerCaptureService_Create();
    MEYER_CAPTURE_SERVICE_API void MeyerCaptureService_Destroy(
        MeyerCaptureServiceHandle handle);

    // 动态加载下层模块并保存配置；真实设备仍在 PrepareColorCalibration 时打开。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_Configure(
        MeyerCaptureServiceHandle handle,
        const MeyerCaptureServiceConfig* config);
    // 停止采集、关闭设备会话、销毁下层句柄，但保留 Service 对象可重新配置。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_Shutdown(
        MeyerCaptureServiceHandle handle);

    // 执行颜色校准准入：连接、USB3、机型系列、设备编号/生产模式、固件和
    // 双扫描头颜色校准状态由 DeviceCmd 统一检查。成功后保持唯一设备会话打开。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_PrepareColorCalibration(
        MeyerCaptureServiceHandle handle);
    // 启动原始 B 包采集和快速/慢速线程。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_StartCapture(
        MeyerCaptureServiceHandle handle);
    // 请求停止并等待线程退出；停止阶段会按 DeviceCmd 规则关灯并回收 USB 队列。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_StopCapture(
        MeyerCaptureServiceHandle handle);
    // 开关灯。采集中只入队，在组六图命令窗口最多发送两条；空闲时立即发送。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_RequestLight(
        MeyerCaptureServiceHandle handle,
        std::int32_t on);
    // UI 修改功能开关时提交一份完整快照；运行中的下一组六图使用新 revision。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_SetPipelineOptions(
        MeyerCaptureServiceHandle handle,
        const MeyerCapturePipelineOptions* options);

    // 非阻塞复制设备和采集状态快照。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_GetDeviceInfo(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceDeviceInfo* info);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_GetStateSnapshot(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceStateSnapshot* snapshot);
    // 非阻塞取出一条结构化事件；当前没有事件返回 NotReady。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_PollEvent(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureServiceEvent* eventInfo);

    // 复制最近一组慢处理后的单图。index 只能是 0~5；6 保留给未来扩展。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_CopyLatestPlane(
        MeyerCaptureServiceHandle handle,
        std::int32_t index,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes);
    // 复制最近一组 RGB888 彩色图，布局为 R/G/B 交错字节。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_CopyLatestRgb888(
        MeyerCaptureServiceHandle handle,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes);
    // 通用多输出接口供扫描、三维校准和颜色校准按 outputType 消费结果。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_GetLatestPipelineOutputInfo(
        MeyerCaptureServiceHandle handle,
        std::int32_t outputType,
        MeyerCapturePipelineOutputInfo* info);
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_CopyLatestPipelineOutput(
        MeyerCaptureServiceHandle handle,
        std::int32_t outputType,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* requiredBytes);
    // 复制最近慢处理结果的组状态和设备上下文。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_CopyLatestGroupInfo(
        MeyerCaptureServiceHandle handle,
        MeyerCaptureGroupInfo* groupInfo);

    // 读取最近错误文本；支持空缓冲区长度探测。
    MEYER_CAPTURE_SERVICE_API std::int32_t MeyerCaptureService_GetLastError(
        MeyerCaptureServiceHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize);
    MEYER_CAPTURE_SERVICE_API const char* MeyerCaptureService_GetModuleName();
    MEYER_CAPTURE_SERVICE_API const char* MeyerCaptureService_GetApiVersion();
    MEYER_CAPTURE_SERVICE_API std::int32_t GetMeyerModuleApiVersion();
    MEYER_CAPTURE_SERVICE_API const char* GetMeyerModuleVersion();
}
