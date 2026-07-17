// =============================================================================
// 文件: DeviceCmd.h
// 模块: MeyerScan_DeviceCmd.dll
// 版本: 0.3.0
//
// 作用:
//   定义设备命令模块唯一的公共 C ABI。调用方通过不透明句柄完成设备基础信息
//   查询、状态快照读取、灯光控制、采集启停和完整帧读取，不直接接触 CyAPI、
//   MyDeviceTransport 内部对象或具体机型的命令字节。
//
// ABI 约束:
//   1. 公共结构只使用固定宽度整数、固定数组和调用方管理的缓冲区。
//   2. 可扩展结构均带 structSize/schemaVersion/reserved。
//   3. 所有文本均为 UTF-8；二进制期限码使用十六进制文本返回。
//   4. DLL 内部分配的 C++ 对象不跨 DLL 边界交给调用方释放。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_DEVICE_CMD_EXPORTS)
#  define MEYERSCAN_DEVICE_CMD_API __declspec(dllexport)
#else
#  define MEYERSCAN_DEVICE_CMD_API __declspec(dllimport)
#endif

// 当前公共结构版本。新增字段时只能在 reserved 前追加，不能改变已有字段含义。
static const std::uint32_t MEYER_DEVICE_CMD_SCHEMA_VERSION = 1U;

// MainExe 动态加载 DLL 前校验的整数 ABI 版本。
static const std::int32_t MEYER_DEVICE_CMD_API_VERSION = 1;

// 协议当前最大命令数据为 416 字节；保留 1024 字节可覆盖后续常规扩展，
// 同时避免把不受控的大块内存放入公共结构。
static const std::size_t MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES = 1024U;

// 协议中各类固定长度数据的大小。调用方使用这些常量分配缓冲区，避免在
// SettingsUI、CalibrationUI 和扫描模块中重复书写容易出错的魔数。
static const std::size_t MEYER_DEVICE_CMD_MACHINE_CODE_BYTES = 13U;
static const std::size_t MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES = 30U;
static const std::size_t MEYER_DEVICE_CMD_CAMERA_PARAMETERS_BYTES = 16U;
static const std::size_t MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES = 416U;
static const std::size_t MEYER_DEVICE_CMD_CALIBRATION_BYTES = 382U;
static const std::size_t MEYER_DEVICE_CMD_COLOR_CALIBRATION_BYTES = 72U;
static const std::size_t MEYER_DEVICE_CMD_EXPOSURE_SET_BYTES = 16U;
static const std::size_t MEYER_DEVICE_CMD_EXPOSURE_READ_BYTES = 17U;
static const std::size_t MEYER_DEVICE_CMD_FIRMWARE_PACKET_BYTES = 256U;

// -1 表示命令没有 A 类响应，ExecuteRawCommand 只负责发送。
static const std::int32_t MEYER_DEVICE_CMD_NO_RESPONSE = -1;

// 设备命令层结果码。0 表示成功，负数表示失败或当前尚未就绪。
enum MeyerDeviceCmdResult : std::int32_t
{
    MeyerDeviceCmdResult_Ok = 0,
    MeyerDeviceCmdResult_InvalidHandle = -1,
    MeyerDeviceCmdResult_InvalidArgument = -2,
    MeyerDeviceCmdResult_UnsupportedModel = -3,
    MeyerDeviceCmdResult_TransportLoadFailed = -4,
    MeyerDeviceCmdResult_TransportApiMismatch = -5,
    MeyerDeviceCmdResult_DeviceNotFound = -6,
    MeyerDeviceCmdResult_NotOpen = -7,
    MeyerDeviceCmdResult_Busy = -8,
    MeyerDeviceCmdResult_Timeout = -9,
    MeyerDeviceCmdResult_IoFailed = -10,
    MeyerDeviceCmdResult_ProtocolError = -11,
    MeyerDeviceCmdResult_BufferTooSmall = -12,
    MeyerDeviceCmdResult_NotReady = -13,
    MeyerDeviceCmdResult_InternalError = -14,
    MeyerDeviceCmdResult_DeviceRejected = -15
};

// 2025-08-08 协议中定义的 A 类命令码。公共层公开这些值，便于协议调试工具
// 记录原始命令；业务模块优先使用下方的语义函数，不直接拼接命令帧。
enum MeyerDeviceCommandCode : std::uint8_t
{
    // 采集、复位、机器码和灯光基础控制命令。
    MeyerDeviceCommand_StartImageTransfer = 0x0A,
    MeyerDeviceCommand_StopImageTransfer = 0x0B,
    MeyerDeviceCommand_ResetController = 0xFF,
    MeyerDeviceCommand_StoreMachineCode = 0x0D,
    MeyerDeviceCommand_MachineCodeStoreReply = 0x1D,
    MeyerDeviceCommand_ReadMachineCode = 0xD4,
    MeyerDeviceCommand_UploadMachineCode = 0xD9,
    MeyerDeviceCommand_SetLight = 0x0E,
    MeyerDeviceCommand_ForceLight = 0x0C,
    MeyerDeviceCommand_ReadMainBoardVersion = 0x14,
    MeyerDeviceCommand_UploadMainBoardVersion = 0x15,
    MeyerDeviceCommand_ReadBattery = 0x1A,
    MeyerDeviceCommand_UploadBattery = 0x1C,
    // 相机参数、颜色矩阵、温度、帧率和曝光命令。
    MeyerDeviceCommand_ReadCameraParameters = 0xA0,
    MeyerDeviceCommand_UploadCameraParameters = 0xA1,
    MeyerDeviceCommand_StoreCameraParameters = 0xA8,
    MeyerDeviceCommand_CameraParametersStoreReply = 0xA9,
    MeyerDeviceCommand_ReadColorMatrix = 0xA3,
    MeyerDeviceCommand_UploadColorMatrix = 0xA4,
    MeyerDeviceCommand_StoreColorMatrix = 0xA7,
    MeyerDeviceCommand_ColorMatrixStoreReply = 0xAE,
    MeyerDeviceCommand_SetCameraWindowPosition = 0xA5,
    MeyerDeviceCommand_ReadTemperature = 0xAA,
    MeyerDeviceCommand_UploadTemperature = 0xAB,
    MeyerDeviceCommand_SetFrameRate = 0xAD,
    MeyerDeviceCommand_EraseFirmware = 0xB6,
    MeyerDeviceCommand_EraseFirmwareProgress = 0xB7,
    MeyerDeviceCommand_WriteFirmware = 0xB4,
    MeyerDeviceCommand_WriteFirmwareProgress = 0xB5,
    MeyerDeviceCommand_StoreCamera1Calibration = 0xC3,
    MeyerDeviceCommand_Camera1CalibrationStoreReply = 0xC5,
    MeyerDeviceCommand_ReadCamera1Calibration = 0xC2,
    MeyerDeviceCommand_UploadCamera1Calibration = 0xC7,
    MeyerDeviceCommand_StoreCamera2Calibration = 0xD0,
    MeyerDeviceCommand_Camera2CalibrationStoreReply = 0xD5,
    MeyerDeviceCommand_ReadCamera2Calibration = 0xD2,
    MeyerDeviceCommand_UploadCamera2Calibration = 0xD7,
    MeyerDeviceCommand_StoreColorCalibration = 0xD1,
    MeyerDeviceCommand_ColorCalibrationStoreReply = 0xD6,
    MeyerDeviceCommand_ReadColorCalibration = 0xD3,
    MeyerDeviceCommand_UploadColorCalibration = 0xD8,
    MeyerDeviceCommand_StoreDeviceInfo = 0xC9,
    MeyerDeviceCommand_DeviceInfoStoreReply = 0xCB,
    MeyerDeviceCommand_ReadDeviceInfo = 0xCD,
    MeyerDeviceCommand_UploadDeviceInfo = 0xCE,
    MeyerDeviceCommand_SetExposure = 0xDB,
    MeyerDeviceCommand_ReadExposure = 0xDC,
    MeyerDeviceCommand_UploadExposure = 0xDE,
    // 固件、两路相机标定、颜色标定和设备授权命令使用上方同一枚举，
    // 具体请求/响应对应关系见 ProtocolCommandCoverage.md。
};

// 正式运行使用 DeviceTransport；SimulatorForTest 只能用于无硬件自动化测试。
enum MeyerDeviceCmdBackendType : std::int32_t
{
    MeyerDeviceCmdBackend_DeviceTransport = 1,
    MeyerDeviceCmdBackend_SimulatorForTest = 2
};

// 产品型号使用稳定业务枚举，不复用 DeviceTransport 的历史图像解码类型。
enum MeyerDeviceModel : std::int32_t
{
    MeyerDeviceModel_Unknown = 0,
    MeyerDeviceModel_MyScan3 = 3,
    MeyerDeviceModel_MyScan5 = 5,
    MeyerDeviceModel_MyScan5H = 50,
    MeyerDeviceModel_MyScan6 = 6,
    MeyerDeviceModel_MyScan6Wireless = 60
};

// 协议族用于把“产品型号”和“命令格式版本”分开，后续新增机型时不改调用方流程。
enum MeyerDeviceProtocolFamily : std::int32_t
{
    MeyerDeviceProtocolFamily_Unknown = 0,
    MeyerDeviceProtocolFamily_LegacySimilar = 1,
    MeyerDeviceProtocolFamily_Wireless20250808 = 2
};

// 型号来源用于区分宿主选择的协议配置和设备自身已验证的身份。
enum MeyerDeviceModelSource : std::int32_t
{
    MeyerDeviceModelSource_Unknown = 0,
    MeyerDeviceModelSource_HostHint = 1,
    MeyerDeviceModelSource_AutoDetected = 2,
    MeyerDeviceModelSource_DeviceReported = 3
};

// 设备连接状态只描述当前进程内会话，不等同于设备业务可用性。
enum MeyerDeviceConnectionState : std::int32_t
{
    MeyerDeviceConnectionState_Closed = 0,
    MeyerDeviceConnectionState_Open = 1,
    MeyerDeviceConnectionState_Faulted = 2
};

// 采集用途决定底层帧状态解析方式；UI 页面名称不进入设备层。
enum MeyerDeviceWorkMode : std::int32_t
{
    MeyerDeviceWorkMode_Idle = 0,
    MeyerDeviceWorkMode_Scan = 1,
    MeyerDeviceWorkMode_Calibration3D = 2,
    MeyerDeviceWorkMode_CalibrationColor = 3
};

// 能力位由机型目录返回。未声明能力时，上层不得盲目下发对应命令。
enum MeyerDeviceCapability : std::uint64_t
{
    MeyerDeviceCapability_MachineCode = 1ULL << 0,
    MeyerDeviceCapability_FirmwareVersion = 1ULL << 1,
    MeyerDeviceCapability_Battery = 1ULL << 2,
    MeyerDeviceCapability_Light = 1ULL << 3,
    MeyerDeviceCapability_ForceLight = 1ULL << 4,
    MeyerDeviceCapability_Capture = 1ULL << 5,
    MeyerDeviceCapability_DeviceSecurityInfo = 1ULL << 6,
    MeyerDeviceCapability_CameraParameters = 1ULL << 7,
    MeyerDeviceCapability_Temperature = 1ULL << 8,
    MeyerDeviceCapability_Exposure = 1ULL << 9,
    MeyerDeviceCapability_CalibrationData = 1ULL << 10,
    MeyerDeviceCapability_FirmwareUpdate = 1ULL << 11
};

// 状态有效位区分“值为 0”和“尚未成功读取”，避免 UI 把未知状态显示成真实状态。
enum MeyerDeviceStateField : std::uint64_t
{
    MeyerDeviceStateField_Connection = 1ULL << 0,
    MeyerDeviceStateField_Model = 1ULL << 1,
    MeyerDeviceStateField_UsbSpeed = 1ULL << 2,
    MeyerDeviceStateField_MachineCode = 1ULL << 3,
    MeyerDeviceStateField_FirmwareVersion = 1ULL << 4,
    MeyerDeviceStateField_Battery = 1ULL << 5,
    MeyerDeviceStateField_DeviceSecurityInfo = 1ULL << 6,
    MeyerDeviceStateField_LightRequested = 1ULL << 7,
    MeyerDeviceStateField_Capture = 1ULL << 8,
    MeyerDeviceStateField_FrameTelemetry = 1ULL << 9
};

// 颜色校准入口预检的业务状态。函数调用本身返回 DeviceCmdResult，调用成功后
// 再读取本枚举，区分“设备未连接”“USB2”和“型号未识别”等正常拦截原因。
enum MeyerDeviceCalibrationPreflightStatus : std::int32_t
{
    MeyerDeviceCalibrationPreflight_NotRun = 0,
    MeyerDeviceCalibrationPreflight_Ready = 1,
    MeyerDeviceCalibrationPreflight_WorkspaceOwnsDevice = 2,
    MeyerDeviceCalibrationPreflight_DeviceNotConnected = 3,
    MeyerDeviceCalibrationPreflight_Usb2Connected = 4,
    MeyerDeviceCalibrationPreflight_WirelessProbeUnsupported = 5,
    MeyerDeviceCalibrationPreflight_DeviceInfoReadFailed = 6,
    MeyerDeviceCalibrationPreflight_ModelUnknown = 7,
    MeyerDeviceCalibrationPreflight_InternalError = 8
};

// 模拟后端专用标志，只允许测试程序使用。正式 DeviceTransport 后端忽略这些值，
// 因此测试能够稳定覆盖未连接、USB2 和没有型号标记等分支。
enum MeyerDeviceCmdSimulatedFlag : std::uint32_t
{
    MeyerDeviceCmdSimulatedFlag_None = 0U,
    MeyerDeviceCmdSimulatedFlag_DeviceNotConnected = 1U << 0,
    MeyerDeviceCmdSimulatedFlag_Usb2Connected = 1U << 1,
    MeyerDeviceCmdSimulatedFlag_OmitModelMarker = 1U << 2
};

// 打开设备会话所需参数。正式后端必须提供 DeviceTransport DLL 的绝对路径，
// 以免第三方软件从其它 current directory 拉起 MeyerScan.exe 时加载错文件。
struct MeyerDeviceCmdOpenParams
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t backendType;
    std::int32_t modelHint;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint32_t deviceIndex;
    std::uint32_t commandTimeoutMs;
    std::uint32_t streamTimeoutMs;
    char transportLibraryPathUtf8[1024];
    char simulatedDeviceIdUtf8[32];
    // simulatedFlags 只供 SimulatorForTest 构造确定性分支，正式后端必须保持 0。
    std::uint32_t simulatedFlags;
    std::uint32_t reserved[7];
};

// 型号描述由模块内部目录统一维护。protocolVerified=1 只表示当前命令协议已按
// 用户提供的正式文档核对，不代表真实硬件长时间联调已经完成。
struct MeyerDeviceModelDescriptor
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t model;
    std::int32_t protocolFamily;
    std::int32_t protocolVerified;
    std::int32_t transportDecoderType;
    std::uint64_t capabilities;
    char modelNameUtf8[32];
    char protocolNameUtf8[64];
    std::uint32_t reserved[8];
};

// 采集参数通常先由 InitCaptureParamsForModel 写入机型默认值，再由扫描编排层
// 按实际扫描头或协议版本覆盖。普通 UI 不应直接维护这些底层分包字段。
struct MeyerDeviceCmdCaptureParams
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t workMode;
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
    std::int32_t pictureOrderMode;
    std::int32_t scanHeadType;
    std::int32_t ahrsEnabled;
    std::uint32_t reserved[8];
};

// 状态快照是只读副本。调用方先检查 validFields，再解释对应字段。
struct MeyerDeviceStateSnapshot
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t sequence;
    std::uint64_t validFields;
    std::uint64_t capabilities;
    std::int32_t connectionState;
    std::int32_t model;
    std::int32_t protocolFamily;
    std::int32_t modelSource;
    std::int32_t deviceCount;
    std::int32_t isUsb2;
    std::int32_t encrypted;
    std::int32_t encryptionType;
    std::int32_t batteryConnected;
    std::int32_t batteryLevel;
    std::int32_t batteryHealth;
    std::int32_t lightRequestedOn;
    std::int32_t captureActive;
    std::int32_t workMode;
    std::int32_t captureStatus;
    std::int32_t scanHeadType;
    std::int32_t temperature0;
    std::int32_t temperature1;
    std::int32_t temperature2;
    std::int32_t temperature3;
    char modelNameUtf8[32];
    char deviceIdUtf8[32];
    char firmwareVersionUtf8[32];
    char expirationCodeHex[64];
    std::uint32_t reserved[8];
};

// 一帧图像的元数据。像素内容由 GetFrame 写入调用方缓冲区。
struct MeyerDeviceCmdFrameInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t captureStatus;
    std::int32_t workMode;
    std::int32_t pictureOrderMode;
    std::int32_t scanHeadType;
    std::int32_t ledOn;
    std::int32_t photoMode;
    std::int32_t temperature0;
    std::int32_t temperature1;
    std::int32_t temperature2;
    std::int32_t temperature3;
    std::uint64_t frameBytes;
    std::uint32_t reserved[8];
};

// 0xA0/0xA1/0xA8 使用的 16 字节相机参数。坐标为协议定义的无符号大端数值，
// 曝光、增益和扫描头偏移量保留协议原始 1 字节编码，业务层再按需要换算。
struct MeyerDeviceCmdCameraParameters
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t camera1WindowX;
    std::uint16_t camera1WindowY;
    std::uint16_t camera2WindowX;
    std::uint16_t camera2WindowY;
    std::uint8_t camera1Exposure;
    std::uint8_t camera1DigitalGain;
    std::uint8_t camera2Exposure;
    std::uint8_t camera2DigitalGain;
    std::uint8_t smallHeadOffsetX;
    std::uint8_t smallHeadOffsetY;
    std::uint8_t largeHeadOffsetX;
    std::uint8_t largeHeadOffsetY;
    std::uint32_t reserved[8];
};

// 0xA5 使用的在线设置开窗位置结构。该命令只在线生效，不写入 Flash。
struct MeyerDeviceCmdCameraWindowPosition
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t camera1X;
    std::uint16_t camera1Y;
    std::uint16_t camera2X;
    std::uint16_t camera2Y;
    std::uint32_t reserved[8];
};

// 0xA3/0xA4/0xA7 使用的 416 字节颜色校正矩阵原始数据。
// 前 208 字节是相机 1 的 G 参数，后 208 字节是相机 1 的 B 参数。
struct MeyerDeviceCmdColorMatrix
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    unsigned char data[MEYER_DEVICE_CMD_COLOR_MATRIX_BYTES];
    std::uint32_t reserved[8];
};

// 0xAA/0xAB 使用的温度电压原始值。协议返回的是 mV，不是摄氏温度。
struct MeyerDeviceCmdTemperature
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t lensMillivolts;
    std::uint16_t boardMillivolts;
    std::uint16_t scanHeadMillivolts;
    std::uint8_t reservedByte;
    std::uint32_t reserved[8];
};

// 0xB7 擦除进度响应。两个字段均按协议使用大端无符号数。
struct MeyerDeviceCmdFirmwareEraseProgress
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t totalSectors;
    std::uint16_t erasedSectors;
    std::uint32_t reserved[8];
};

// 0xB4 烧写请求的 262 字节协议数据由公共结构表达，避免调用方自行拼包。
struct MeyerDeviceCmdFirmwareWritePacket
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t totalPackets;
    std::uint16_t packetIndex;
    std::uint16_t actualDataSize;
    unsigned char data[MEYER_DEVICE_CMD_FIRMWARE_PACKET_BYTES];
    std::uint32_t reserved[8];
};

// 0xB5 烧写进度响应。
struct MeyerDeviceCmdFirmwareWriteProgress
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint16_t totalPackets;
    std::uint16_t packetIndex;
    std::uint16_t actualDataSize;
    std::uint32_t reserved[8];
};

// 0xC2/0xC3/0xC7 和 0xD2/0xD0/0xD7 使用的 382 字节相机标定参数。
struct MeyerDeviceCmdCameraCalibration
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    unsigned char cameraData[187];
    unsigned char projectorData[187];
    unsigned char timestamp[7];
    unsigned char padding;
    std::uint32_t reserved[8];
};

// 0xD1/0xD3/0xD8 使用的 72 字节颜色标定参数原始数据。
struct MeyerDeviceCmdColorCalibration
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    unsigned char data[MEYER_DEVICE_CMD_COLOR_CALIBRATION_BYTES];
    std::uint32_t reserved[8];
};

// 0xC9/0xCD/0xCE 使用的设备授权信息。期限码保持原始 30 字节，不能在设备层
// 擅自解释有效期；后续由授权/加解密模块完成业务校验。
struct MeyerDeviceCmdDeviceInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint8_t encrypted;
    std::uint8_t encryptionType;
    char deviceIdUtf8[32];
    unsigned char expirationCode[MEYER_DEVICE_CMD_EXPIRATION_CODE_BYTES];
    unsigned char reservedData[337];
    std::uint32_t reserved[8];
};

// 校准预检结果同时保存设备运行状态和 0xCD/0xCE 设备信息副本。
// 多个 UI/业务模块只复制该 POD，不共享 DeviceCmd 内部对象或 USB 句柄。
struct MeyerDeviceCalibrationPreflight
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t status;
    std::int32_t commandResult;
    MeyerDeviceStateSnapshot state;
    MeyerDeviceCmdDeviceInfo deviceInfo;
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

// 0xDB/0xDC/0xDE 使用的曝光参数。前 16 个字段对应设置命令，读取响应额外
// 带一个协议预留字节，模块会自动忽略该字节。
struct MeyerDeviceCmdExposureParameters
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint8_t camera1WhiteExposure;
    std::uint8_t camera1WhiteLightTime;
    std::uint8_t camera1WhiteAnalogGain;
    std::uint8_t camera1WhiteDigitalGain;
    std::uint8_t camera1StripeExposure;
    std::uint8_t camera1StripeLightTime;
    std::uint8_t camera1StripeAnalogGain;
    std::uint8_t camera1StripeDigitalGain;
    std::uint8_t camera2WhiteExposure;
    std::uint8_t camera2WhiteLightTime;
    std::uint8_t camera2WhiteAnalogGain;
    std::uint8_t camera2WhiteDigitalGain;
    std::uint8_t camera2StripeExposure;
    std::uint8_t camera2StripeLightTime;
    std::uint8_t camera2StripeAnalogGain;
    std::uint8_t camera2StripeDigitalGain;
    std::uint32_t reserved[8];
};

// 通用扩展命令响应。后续增加具体命令时优先新增语义接口，只有协议调试、
// 尚未封装的低频命令才直接使用本结构。
struct MeyerDeviceCmdRawResponse
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t commandCode;
    std::uint32_t payloadSize;
    unsigned char payload[MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES];
    std::uint32_t reserved[8];
};

// 不透明句柄把线程同步、状态缓存、Transport DLL 句柄和 C++ 容器留在模块内部。
typedef void* MeyerDeviceCmdHandle;

extern "C"
{
    // 初始化打开参数并写入安全默认值。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitOpenParams(MeyerDeviceCmdOpenParams* params);
    // 查询型号目录并生成默认采集参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCaptureParamsForModel(std::int32_t model, MeyerDeviceCmdCaptureParams* params);
    // 初始化状态快照结构，供 GetStateSnapshot 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitStateSnapshot(MeyerDeviceStateSnapshot* snapshot);
    // 初始化帧信息结构，供 GetFrame 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFrameInfo(MeyerDeviceCmdFrameInfo* frameInfo);
    // 初始化通用命令响应结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitRawResponse(MeyerDeviceCmdRawResponse* response);
    // 初始化型号描述结构，供 GetModelDescriptor 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitModelDescriptor(MeyerDeviceModelDescriptor* descriptor);
    // 初始化相机参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraParameters(MeyerDeviceCmdCameraParameters* parameters);
    // 初始化相机开窗位置结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraWindowPosition(MeyerDeviceCmdCameraWindowPosition* position);
    // 初始化颜色校正矩阵结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitColorMatrix(MeyerDeviceCmdColorMatrix* matrix);
    // 初始化温度原始电压结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitTemperature(MeyerDeviceCmdTemperature* temperature);
    // 初始化固件擦除进度结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareEraseProgress(MeyerDeviceCmdFirmwareEraseProgress* progress);
    // 初始化固件分包烧写请求结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareWritePacket(MeyerDeviceCmdFirmwareWritePacket* packet);
    // 初始化固件分包烧写进度结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareWriteProgress(MeyerDeviceCmdFirmwareWriteProgress* progress);
    // 初始化相机标定参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCameraCalibration(MeyerDeviceCmdCameraCalibration* calibration);
    // 初始化颜色标定参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitColorCalibration(MeyerDeviceCmdColorCalibration* calibration);
    // 初始化设备授权信息结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitDeviceInfo(MeyerDeviceCmdDeviceInfo* info);
    // 初始化颜色校准设备预检结果。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCalibrationPreflight(MeyerDeviceCalibrationPreflight* preflight);
    // 初始化曝光参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitExposureParameters(MeyerDeviceCmdExposureParameters* parameters);
    // 读取指定产品型号的协议族和能力目录。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetModelDescriptor(std::int32_t model, MeyerDeviceModelDescriptor* descriptor);

    // 创建单设备命令会话。生产进程应只由 MainExe 的设备会话宿主持有一个句柄。
    MEYERSCAN_DEVICE_CMD_API MeyerDeviceCmdHandle MeyerDeviceCmd_Create();
    // 停止采集、关闭设备并销毁会话；允许传入空句柄。
    MEYERSCAN_DEVICE_CMD_API void MeyerDeviceCmd_Destroy(MeyerDeviceCmdHandle handle);

    // 加载 Transport 后端并打开指定设备。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Open(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdOpenParams* params);
    // 停止采集并关闭设备，重复调用仍安全。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Close(MeyerDeviceCmdHandle handle);
    // 返回 1 表示已打开、0 表示未打开，负数表示错误。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_IsOpen(MeyerDeviceCmdHandle handle);
    // 使用最近一次打开参数恢复连接。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_Reconnect(MeyerDeviceCmdHandle handle);

    // 打开唯一设备会话、检查 USB 速率、读取 0xCD/0xCE 设备信息并识别型号。
    // Ready 时保持会话打开供颜色校准继续使用；其它状态会主动关闭会话。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_PrepareColorCalibration(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdOpenParams* params,
        MeyerDeviceCalibrationPreflight* preflight);

    // 串行读取机器码、主板版本、电池和设备期限原始信息；采集中返回 Busy。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_RefreshBasicState(MeyerDeviceCmdHandle handle);
    // 非阻塞复制最近状态快照，不执行 USB I/O。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetStateSnapshot(MeyerDeviceCmdHandle handle, MeyerDeviceStateSnapshot* snapshot);
    // 设置普通开关灯命令；on 为 0 表示关灯，非 0 表示开灯。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetLight(MeyerDeviceCmdHandle handle, std::int32_t on);
    // 设置强制开灯策略；enabled 为 0 表示关闭，非 0 表示启用。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetForceLight(MeyerDeviceCmdHandle handle, std::int32_t enabled);

    // 下发 0xFF 使设备控制器软件复位。该命令无响应，调用方随后应关闭并重连会话。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ResetController(MeyerDeviceCmdHandle handle);
    // 把 13 位十进制机器码存入设备 Flash，并检查 0x1D 状态回复。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreMachineCode(MeyerDeviceCmdHandle handle, const char* machineCodeUtf8);

    // 读取或固化 16 字节相机参数；固化操作会检查 0xA9 状态回复。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCameraParameters(MeyerDeviceCmdHandle handle, MeyerDeviceCmdCameraParameters* parameters);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCameraParameters(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdCameraParameters* parameters);
    // 在线设置相机 1/2 开窗位置，不写入设备 Flash。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetCameraWindowPosition(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdCameraWindowPosition* position);

    // 读取或固化 416 字节颜色校正矩阵；固化操作会检查 0xAE 状态回复。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadColorMatrix(MeyerDeviceCmdHandle handle, MeyerDeviceCmdColorMatrix* matrix);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreColorMatrix(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdColorMatrix* matrix);
    // 读取三个温度通道的原始毫伏值，不在设备层换算摄氏温度。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadTemperature(MeyerDeviceCmdHandle handle, MeyerDeviceCmdTemperature* temperature);
    // 设置设备帧率，只接受协议规定的 18、20、22 或 25。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetFrameRate(MeyerDeviceCmdHandle handle, std::int32_t framesPerSecond);

    // 擦除主板固件并读取一条 0xB7 进度响应。完整升级编排仍应由更新宿主控制。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_EraseFirmware(MeyerDeviceCmdHandle handle, MeyerDeviceCmdFirmwareEraseProgress* progress, std::uint32_t timeoutMs);
    // 烧写一个最多 256 字节的固件分包，并核对 0xB5 应答中的包序信息。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_WriteFirmwarePacket(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdFirmwareWritePacket* packet, MeyerDeviceCmdFirmwareWriteProgress* progress, std::uint32_t timeoutMs);

    // 读取或固化相机 1（CGB5/绿灯）标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCamera1Calibration(MeyerDeviceCmdHandle handle, MeyerDeviceCmdCameraCalibration* calibration);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCamera1Calibration(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdCameraCalibration* calibration);
    // 读取或固化相机 2（CGB4/蓝灯）标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadCamera2Calibration(MeyerDeviceCmdHandle handle, MeyerDeviceCmdCameraCalibration* calibration);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreCamera2Calibration(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdCameraCalibration* calibration);
    // 读取或固化 72 字节颜色标定参数。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadColorCalibration(MeyerDeviceCmdHandle handle, MeyerDeviceCmdColorCalibration* calibration);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreColorCalibration(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdColorCalibration* calibration);

    // 读取或固化设备加密、机器码和期限原始数据。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadDeviceInfo(MeyerDeviceCmdHandle handle, MeyerDeviceCmdDeviceInfo* info);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StoreDeviceInfo(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdDeviceInfo* info);
    // 在线设置或读取两路相机白图/条纹曝光参数；设置命令不写入 Flash。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetExposureParameters(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdExposureParameters* parameters);
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadExposureParameters(MeyerDeviceCmdHandle handle, MeyerDeviceCmdExposureParameters* parameters);

    // 执行尚未封装成语义函数的 A 类命令。expectedResponseCode=-1 表示无响应。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ExecuteRawCommand(MeyerDeviceCmdHandle handle,
                                                                           std::uint8_t commandCode,
                                                                           const unsigned char* payload,
                                                                           std::size_t payloadSize,
                                                                           std::int32_t expectedResponseCode,
                                                                           MeyerDeviceCmdRawResponse* response,
                                                                           std::uint32_t timeoutMs);

    // 先建立底层异步接收队列，再下发 0x0A 开始传图命令。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StartCapture(MeyerDeviceCmdHandle handle, const MeyerDeviceCmdCaptureParams* params);
    // 按机型顺序下发停图/关灯命令并释放底层采集资源；turnLightOff 控制是否关灯。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StopCapture(MeyerDeviceCmdHandle handle, std::int32_t turnLightOff);
    // 非阻塞读取一帧；无完整帧时返回 NotReady。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetFrame(MeyerDeviceCmdHandle handle,
                                                                  unsigned char* buffer,
                                                                  std::size_t capacity,
                                                                  std::size_t* frameBytes,
                                                                  MeyerDeviceCmdFrameInfo* frameInfo);

    // 读取当前句柄最近错误；支持空缓冲区的长度探测调用。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetLastError(MeyerDeviceCmdHandle handle, char* buffer, std::size_t capacity, std::size_t* requiredSize);
    // 返回稳定模块名。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetModuleName();
    // 返回语义化公共 API 版本字符串。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetApiVersion();
    // 返回所有自研 DLL 统一使用的整数 ABI 版本。
    MEYERSCAN_DEVICE_CMD_API std::int32_t GetMeyerModuleApiVersion();
    // 返回版本清单读取的代码版本。
    MEYERSCAN_DEVICE_CMD_API const char* GetMeyerModuleVersion();
}
