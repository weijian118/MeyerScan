// =============================================================================
// 文件: DeviceCmd.h
// 模块: MeyerScan_DeviceCmd.dll
// 模块版本: 1.0.0；公共语义 API: 3.0.0；schema: 7；整数 ABI: 8
//
// 作用:
//   定义设备命令模块唯一的公共 C ABI。调用方通过不透明句柄完成设备基础信息
//   查询、状态快照读取、灯光控制、采集启停和完整帧读取，不直接接触 CyAPI、
//   MyDeviceTransport 内部对象或具体机型的命令字节。
//
// 设备识别与版本读取流程:
//   1. DeviceTransport 只完成 Cypress 枚举、USB2/USB3 判断和原始字节收发。
//   2. DeviceCmd 发送 D4/D9 读取真实设备编号；D9 长度 0xFFFF 和求和
//      校验失败都表示生产未写号，但在状态字段中分别记录。
//   3. 生产状态使用 C2/C7 探测系列候选，随后所有设备都读取 CD/CE 型号代码。
//   4. DeviceProductCatalog 综合编号前缀、完整型号代码和命令证据，输出产品系列、
//      具体产品和协议 Profile；UI 不得复制这套映射。
//   5. 型号确认后读取 0x14/0x15 主控板版本；只有 mOS MyScan 再读取
//      0x12/0x13 投图板版本。其它系列不发送投图板命令。
//   6. MyScan 5/6 在主控板版本满足 1.3.x 后读取 A3/A4 大扫描头和 B9/BA 小扫描头
//      状态；期望回包求和失败表示未校准，通信异常阻止进入颜色校准。
//   7. 完整结果写入 MeyerDeviceCalibrationPreflight，MainExe 只复制 POD 快照，
//      Settings/Calibration/Scan 不持有 DeviceCmd 句柄或再次查询同一信息。
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
static const std::uint32_t MEYER_DEVICE_CMD_SCHEMA_VERSION = 7U;

// MainExe 动态加载 DLL 前校验的整数 ABI 版本。
static const std::int32_t MEYER_DEVICE_CMD_API_VERSION = 8;

// 协议当前最大命令数据为 416 字节；保留 1024 字节可覆盖后续常规扩展，
// 同时避免把不受控的大块内存放入公共结构。
static const std::size_t MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES = 1024U;

// 协议中各类固定长度数据的大小。调用方使用这些常量分配缓冲区，避免在
// SettingsUI、CalibrationUI 和扫描模块中重复书写容易出错的魔数。
static const std::size_t MEYER_DEVICE_CMD_MACHINE_CODE_BYTES = 13U;
static const std::size_t MEYER_DEVICE_CMD_MODEL_PREFIX_BYTES = 8U;
// 8 个无符号字节逐个转十进制后最长为 24 个字符，额外保留字符串结尾。
static const std::size_t MEYER_DEVICE_CMD_MODEL_CODE_UTF8_BYTES = 25U;
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
    MeyerDeviceCmdResult_DeviceRejected = -15,
    // 底层原始 B 包连续两次超时，采集服务应进入 Faulted 并通知 UI。
    MeyerDeviceCmdResult_StreamStalled = -16,
    // 采集期间检测到设备句柄已失效，不能继续使用旧设备快照发命令。
    MeyerDeviceCmdResult_DeviceDisconnected = -17
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
    // mOS MyScan 的独立投图板版本读取命令。
    MeyerDeviceCommand_ReadProjectionBoardVersion = 0x12,
    MeyerDeviceCommand_UploadProjectionBoardVersion = 0x13,
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
    // MyScan 5/6 小扫描头颜色参数读取请求和回包。
    MeyerDeviceCommand_ReadSmallScanHeadColorMatrix = 0xB9,
    MeyerDeviceCommand_UploadSmallScanHeadColorMatrix = 0xBA,
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

// 该枚举描述协议/硬件能力 Profile，不再承担销售产品型号含义。
// 名称和值暂时保持兼容，避免已有采集参数和 Transport 解码分支同时发生变化。
enum MeyerDeviceModel : std::int32_t
{
    MeyerDeviceModel_Unknown = 0,
    MeyerDeviceModel_MyScan3 = 3,
    MeyerDeviceModel_MyScan5 = 5,
    MeyerDeviceModel_MyScan5H = 50,
    MeyerDeviceModel_MyScan6 = 6,
    MeyerDeviceModel_MyScan6Wireless = 60
};

// 产品系列是面向业务和界面的一级分类。一个系列可以包含多个具体产品型号，
// 不能仅凭系列名称选择贴牌、海外或医院版等产品分支。
enum MeyerDeviceProductFamily : std::int32_t
{
    MeyerDeviceProductFamily_Unknown = 0,
    MeyerDeviceProductFamily_MyScan = 1,
    MeyerDeviceProductFamily_MyScan5 = 5,
    MeyerDeviceProductFamily_MyScan6 = 6
};

// 具体产品型号使用稳定整数 ID。型号代码待定的产品也先占用稳定 ID，后续只需
// 在产品目录补充型号代码，不应修改已经发布的枚举值。
enum MeyerDeviceProductModel : std::int32_t
{
    MeyerDeviceProductModel_Unknown = 0,
    MeyerDeviceProductModel_MyScan_SY_KS1000_P1 = 1001,
    MeyerDeviceProductModel_MyScan_SY_KS1000_P2 = 1002,
    MeyerDeviceProductModel_MyScan_SY_KS1000_P3 = 1003,
    MeyerDeviceProductModel_MyScan_InternationalStandard = 1010,
    MeyerDeviceProductModel_MyScan_InternationalPrivateLabel = 1011,
    MeyerDeviceProductModel_MyScan5_DomesticStandard = 5001,
    MeyerDeviceProductModel_MyScan5_InternationalStandard = 5002,
    MeyerDeviceProductModel_MyScan5_DomesticCaries = 5003,
    MeyerDeviceProductModel_MyScan5H_PublicHospital = 5051,
    MeyerDeviceProductModel_MyScan6_DomesticWired = 6001,
    MeyerDeviceProductModel_MyScan6_DomesticWireless = 6002
};

// 识别状态明确区分“只知道系列”和“已经知道具体产品”，避免上层把候选值
// 当作精确结果。设备编号未写入是生产阶段状态，不等同于设备通信失败。
enum MeyerDeviceProductIdentificationStatus : std::int32_t
{
    MeyerDeviceProductIdentification_Unknown = 0,
    MeyerDeviceProductIdentification_SeriesOnly = 1,
    MeyerDeviceProductIdentification_ExactProduct = 2,
    MeyerDeviceProductIdentification_DeviceNumberUnprogrammed = 3,
    MeyerDeviceProductIdentification_Conflict = 4,
    // 使用旧固件/生产模式兼容默认值识别，不能伪装成设备精确上报。
    MeyerDeviceProductIdentification_CompatibilityInferred = 5
};

// 识别证据使用位标记保存来源。后续加入固件命令时可以继续组合证据，
// 无需改变现有识别状态或让 UI 解析原始命令回包。
enum MeyerDeviceProductEvidence : std::uint64_t
{
    MeyerDeviceProductEvidence_None = 0ULL,
    MeyerDeviceProductEvidence_ConnectionType = 1ULL << 0,
    MeyerDeviceProductEvidence_DeviceNumberPrefix = 1ULL << 1,
    MeyerDeviceProductEvidence_ModelCode = 1ULL << 2,
    MeyerDeviceProductEvidence_FirmwareVersion = 1ULL << 3,
    MeyerDeviceProductEvidence_CommandCapability = 1ULL << 4,
    MeyerDeviceProductEvidence_CompatibilityDefault = 1ULL << 5,
    MeyerDeviceProductEvidence_CalibrationCommandProbe = 1ULL << 6
};

// reported 表示设备真实回包，CompatibilityDefault 表示旧流程为继续生产/兼容
// 选择的默认值。两类来源必须分别记录，禁止默认值覆盖真实字段。
enum MeyerDeviceIdentityValueSource : std::int32_t
{
    MeyerDeviceIdentityValueSource_Unknown = 0,
    MeyerDeviceIdentityValueSource_DeviceReported = 1,
    MeyerDeviceIdentityValueSource_CompatibilityDefault = 2
};

// 0xD4/0xD9 设备编号步骤状态。
enum MeyerDeviceNumberReadStatus : std::int32_t
{
    MeyerDeviceNumberRead_NotRun = 0,
    MeyerDeviceNumberRead_Valid = 1,
    MeyerDeviceNumberRead_ResponseMissing = 2,
    MeyerDeviceNumberRead_FrameInvalid = 3,
    MeyerDeviceNumberRead_ChecksumIndicatesUnprogrammed = 4,
    MeyerDeviceNumberRead_ValueInvalid = 5,
    // 设备明确返回 0xD9，但 payload 长度字段为 0xFFFF。该状态表示
    // 设备编号参数未初始化，与求和校验失败的旧生产回包分开记录。
    MeyerDeviceNumberRead_UninitializedLength = 6
};

// 0xCD/0xCE 型号代码步骤状态。
enum MeyerDeviceModelCodeReadStatus : std::int32_t
{
    MeyerDeviceModelCodeRead_NotRun = 0,
    MeyerDeviceModelCodeRead_Valid = 1,
    MeyerDeviceModelCodeRead_FirmwareTooOld = 2,
    MeyerDeviceModelCodeRead_FrameInvalid = 3,
    MeyerDeviceModelCodeRead_ChecksumInvalid = 4,
    MeyerDeviceModelCodeRead_Uninitialized = 5,
    MeyerDeviceModelCodeRead_ValueInvalid = 6
};

// 设备编号未写入时，用 0xC2/0xC7 命令能力探测产品系列候选。
enum MeyerDeviceSeriesProbeStatus : std::int32_t
{
    MeyerDeviceSeriesProbe_NotRun = 0,
    MeyerDeviceSeriesProbe_NotRequired = 1,
    MeyerDeviceSeriesProbe_MyScan = 2,
    MeyerDeviceSeriesProbe_MyScan5Or6 = 3,
    MeyerDeviceSeriesProbe_ResponseAbnormal = 4
};

// 最终检测状态既描述是否可用，也保留是否依赖兼容默认值。
enum MeyerDeviceDetectionStatus : std::int32_t
{
    MeyerDeviceDetection_NotRun = 0,
    MeyerDeviceDetection_Exact = 1,
    MeyerDeviceDetection_CompatibilityInferred = 2,
    MeyerDeviceDetection_ProductionExactModel = 3,
    MeyerDeviceDetection_ProductionInferred = 4,
    MeyerDeviceDetection_Failed = 5,
    MeyerDeviceDetection_Conflict = 6
};

// 0xCE 在旧有线设备和无线设备中含义不同。记录实际解析布局可帮助日志和
// 实机联调确认走了哪个协议分支，禁止再次把两种结构强行按同一偏移解释。
enum MeyerDeviceInfoLayout : std::uint8_t
{
    MeyerDeviceInfoLayout_Unknown = 0,
    MeyerDeviceInfoLayout_LegacyWiredModelCode = 1,
    MeyerDeviceInfoLayout_WirelessSecurityInfo = 2
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
    MeyerDeviceCapability_FirmwareUpdate = 1ULL << 11,
    MeyerDeviceCapability_ProjectionBoardFirmwareVersion = 1ULL << 12
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
    MeyerDeviceStateField_FrameTelemetry = 1ULL << 9,
    MeyerDeviceStateField_ModelCode = 1ULL << 10,
    MeyerDeviceStateField_ProjectionBoardFirmwareVersion = 1ULL << 11
};

// 进入颜色校准前使用的扫描头策略。旧 mOS MyScan 的小扫描头共用大扫描头
// 参数，因此只需要针对大扫描头执行一次校准；MyScan 5/6 必须分开记录。
enum MeyerDeviceScanHeadColorCalibrationPolicy : std::int32_t
{
    MeyerDeviceScanHeadColorCalibrationPolicy_NotRun = 0,
    MeyerDeviceScanHeadColorCalibrationPolicy_LargeOnlyShared = 1,
    MeyerDeviceScanHeadColorCalibrationPolicy_LargeAndSmall = 2
};

// 单个扫描头颜色参数的读取状态。ChecksumInvalid 是设备明确返回期望响应但
// 参数区求和失败，业务含义是“未校准”；其它失败状态表示通信/帧解析失败。
enum MeyerDeviceScanHeadColorCalibrationStatus : std::int32_t
{
    MeyerDeviceScanHeadColorCalibration_NotChecked = 0,
    MeyerDeviceScanHeadColorCalibration_Calibrated = 1,
    MeyerDeviceScanHeadColorCalibration_NotCalibrated = 2,
    MeyerDeviceScanHeadColorCalibration_NotRequired = 3,
    MeyerDeviceScanHeadColorCalibration_ResponseMissing = 4,
    MeyerDeviceScanHeadColorCalibration_FrameInvalid = 5,
    MeyerDeviceScanHeadColorCalibration_PayloadInvalid = 6
};

// 主控板版本对双扫描头颜色校准的兼容结果。版本无法按 x.y.z 解析时必须
// 使用 ParseFailed，不能把未知版本当作支持版本放行。
enum MeyerDeviceColorCalibrationFirmwareCompatibility : std::int32_t
{
    MeyerDeviceColorCalibrationFirmware_NotChecked = 0,
    MeyerDeviceColorCalibrationFirmware_Supported = 1,
    MeyerDeviceColorCalibrationFirmware_Unsupported = 2,
    MeyerDeviceColorCalibrationFirmware_ParseFailed = 3,
    MeyerDeviceColorCalibrationFirmware_NotRequired = 4
};

// 双扫描头颜色校准预检快照。detailUtf8 保存最近一次失败或特殊状态的说明；
// large/smallCommandResult 保留 DeviceCmd API 结果，便于现场区分超时和坏帧。
struct MeyerDeviceScanHeadColorCalibrationSnapshot
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t policy;
    std::int32_t firmwareCompatibility;
    std::int32_t largeHeadStatus;
    std::int32_t smallHeadStatus;
    std::int32_t largeHeadCommandResult;
    std::int32_t smallHeadCommandResult;
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceScanHeadColorCalibrationSnapshot) == 320U,
              "MeyerDeviceScanHeadColorCalibrationSnapshot ABI size changed");

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
    MeyerDeviceCalibrationPreflight_InternalError = 8,
    // 为保持既有状态数值稳定，新状态追加在末尾，不插入已有枚举中间。
    MeyerDeviceCalibrationPreflight_MachineCodeReadFailed = 9,
    // 设备编号前缀与 0xCE 型号代码指向不同系列时必须阻止继续使用。
    MeyerDeviceCalibrationPreflight_ProductIdentityConflict = 10,
    MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal = 11,
    MeyerDeviceCalibrationPreflight_DeviceNumberInvalid = 12,
    MeyerDeviceCalibrationPreflight_DeviceModelCodeInvalid = 13,
    // 设备已识别为生产状态但尚未写入真实编号。该状态由创建工作流宿主设置；
    // 练习、颜色校准和三维校准可以继续使用带来源的 effective 兼容身份。
    MeyerDeviceCalibrationPreflight_ProductionDeviceNumberRequired = 14,
    // 设备身份已经识别，但颜色校准所需的版本命令读取失败。
    MeyerDeviceCalibrationPreflight_FirmwareVersionReadFailed = 15,
    // MyScan 5/6 主控板版本低于 1.3 或版本文本无法可靠解析。
    MeyerDeviceCalibrationPreflight_ColorCalibrationFirmwareUnsupported = 16,
    // A3/A4 或 B9/BA 没有收到可解释的完整回包。
    MeyerDeviceCalibrationPreflight_ScanHeadColorCalibrationReadFailed = 17,
    // 重构后的软件只支持 mOS MyScan 5/6。旧 mOS MyScan 的协议识别代码
    // 继续保留用于输出准确诊断，但识别成功后必须通过该状态统一阻止进入业务。
    MeyerDeviceCalibrationPreflight_ProductFamilyUnsupported = 18
};

// 模拟后端专用标志，只允许测试程序使用。正式 DeviceTransport 后端忽略这些值，
// 因此测试能够稳定覆盖未连接、USB2 和没有型号标记等分支。
enum MeyerDeviceCmdSimulatedFlag : std::uint32_t
{
    MeyerDeviceCmdSimulatedFlag_None = 0U,
    MeyerDeviceCmdSimulatedFlag_DeviceNotConnected = 1U << 0,
    MeyerDeviceCmdSimulatedFlag_Usb2Connected = 1U << 1,
    MeyerDeviceCmdSimulatedFlag_OmitModelMarker = 1U << 2,
    MeyerDeviceCmdSimulatedFlag_MachineCodeReadFailure = 1U << 3,
    MeyerDeviceCmdSimulatedFlag_DeviceNumberChecksumFailure = 1U << 4,
    MeyerDeviceCmdSimulatedFlag_ModelCodeReadFailure = 1U << 5,
    MeyerDeviceCmdSimulatedFlag_ModelCodeUninitialized = 1U << 6,
    MeyerDeviceCmdSimulatedFlag_ModelCodeChecksumFailure = 1U << 7,
    MeyerDeviceCmdSimulatedFlag_Camera1ProbeUnsupported = 1U << 8,
    MeyerDeviceCmdSimulatedFlag_InvalidDeviceNumber = 1U << 9,
    MeyerDeviceCmdSimulatedFlag_InvalidModelCode = 1U << 10,
    // 以下三个标志生成“已经收到、但不是校验错误”的坏包，用于验证回包异常分支。
    MeyerDeviceCmdSimulatedFlag_DeviceNumberFrameInvalid = 1U << 11,
    MeyerDeviceCmdSimulatedFlag_ModelCodeFrameInvalid = 1U << 12,
    MeyerDeviceCmdSimulatedFlag_Camera1ProbeFrameInvalid = 1U << 13,
    // 版本读取失败标志用于验证主控板和投图板的独立错误记录。
    MeyerDeviceCmdSimulatedFlag_MainBoardVersionReadFailure = 1U << 14,
    MeyerDeviceCmdSimulatedFlag_ProjectionBoardVersionReadFailure = 1U << 15,
    // 生成长度字段为 0xFFFF 的 0xD9 回包，验证新生产模式分支。
    MeyerDeviceCmdSimulatedFlag_DeviceNumberUninitialized = 1U << 16,
    // 以下标志专门覆盖颜色校准版本门禁和大小扫描头参数状态。
    MeyerDeviceCmdSimulatedFlag_UnsupportedColorCalibrationFirmware = 1U << 17,
    MeyerDeviceCmdSimulatedFlag_LargeHeadColorChecksumFailure = 1U << 18,
    MeyerDeviceCmdSimulatedFlag_SmallHeadColorChecksumFailure = 1U << 19,
    MeyerDeviceCmdSimulatedFlag_LargeHeadColorReadFailure = 1U << 20,
    MeyerDeviceCmdSimulatedFlag_SmallHeadColorReadFailure = 1U << 21,
    // 以下标志只用于 CaptureService 无硬件异常回归。
    MeyerDeviceCmdSimulatedFlag_StreamTimeoutOnce = 1U << 22,
    MeyerDeviceCmdSimulatedFlag_StreamTimeoutAlways = 1U << 23,
    MeyerDeviceCmdSimulatedFlag_DisconnectDuringCapture = 1U << 24,
    MeyerDeviceCmdSimulatedFlag_PartialStreamPacket = 1U << 25
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

// 设备产品识别结果是可跨 DLL 复制的只读 POD。协议能力 Profile、产品系列和
// 具体产品型号分别保存，上层无需保留 DeviceCmd 内部目录指针或 C++ 字符串。
struct MeyerDeviceProductIdentity
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t productFamily;
    std::int32_t productModel;
    std::int32_t identificationStatus;
    std::int32_t protocolProfile;
    std::uint64_t evidence;
    char deviceNumberPrefixUtf8[16];
    char modelCodeUtf8[16];
    char seriesNameUtf8[32];
    char productNameUtf8[96];
    char detailUtf8[128];
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceProductIdentity) == 352U,
              "MeyerDeviceProductIdentity ABI size changed");

// 完整设备检测记录同时保存真实回包值与最终兼容值。该结构可写入日志、运行时
// 快照并传给 UI，但不包含原始 USB 缓冲区或 DLL 内部指针。
struct MeyerDeviceDetectionRecord
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t detectionStatus;
    std::int32_t deviceNumberStatus;
    std::int32_t modelCodeStatus;
    std::int32_t seriesProbeStatus;
    std::int32_t isProductionMode;
    std::int32_t usedCompatibilityDefaults;
    std::int32_t deviceNumberSource;
    std::int32_t modelCodeSource;
    char reportedDeviceNumberUtf8[32];
    char effectiveDeviceNumberUtf8[32];
    char reportedModelCodeUtf8[16];
    char effectiveModelCodeUtf8[16];
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceDetectionRecord) == 424U,
              "MeyerDeviceDetectionRecord ABI size changed");

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

// DeviceCmd 向上层复制的原始流诊断快照。它与 Transport 数值语义一致，
// 但使用 DeviceCmd 自身的 schema，上层不需要同时包含 DeviceTransport 头文件。
struct MeyerDeviceCmdStreamDiagnostics
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

static_assert(sizeof(MeyerDeviceCmdStreamDiagnostics) == 112U,
              "MeyerDeviceCmdStreamDiagnostics ABI size changed");

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
    // mOS MyScan 的独立投图板版本；其它机型没有该硬件时保持为空。
    char projectionBoardFirmwareVersionUtf8[32];
    char expirationCodeHex[64];
    // 旧软件把 0xCE payload 前 8 字节逐字节转十进制后拼成机型标识。
    // 该文本保留原始含义；model 字段只在标识能可靠映射时才写具体枚举。
    char modelCodeUtf8[MEYER_DEVICE_CMD_MODEL_CODE_UTF8_BYTES];
    std::uint8_t reservedBytes[7];
};

// 0xD4/0xD9 使用的 13 位设备编号。MachineCode 是既有协议/API 历史命名；
// rawDigits 保留逐位数值，machineCodeUtf8 保存规范化设备编号字符串。
struct MeyerDeviceCmdMachineCode
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint8_t rawDigits[MEYER_DEVICE_CMD_MACHINE_CODE_BYTES];
    char machineCodeUtf8[32];
    std::uint32_t reserved[8];
};

// 公共 ABI 尺寸必须稳定。新增字段应优先消耗 reserved，并同步升级 schema/整数 ABI。
static_assert(sizeof(MeyerDeviceStateSnapshot) == 336U,
              "MeyerDeviceStateSnapshot ABI size changed");
static_assert(sizeof(MeyerDeviceCmdMachineCode) == 88U,
              "MeyerDeviceCmdMachineCode ABI size changed");

// 下位机版本读取结果。版本号按协议的三个字段规范化为
// "主版本.次版本.修订号"；读取失败时保留状态和诊断文本，不能把空字符串当作版本。
enum MeyerDeviceFirmwareVersionStatus : std::int32_t
{
    MeyerDeviceFirmwareVersion_NotRun = 0,
    MeyerDeviceFirmwareVersion_Valid = 1,
    MeyerDeviceFirmwareVersion_NotRequired = 2,
    MeyerDeviceFirmwareVersion_ResponseMissing = 3,
    MeyerDeviceFirmwareVersion_FrameInvalid = 4,
    MeyerDeviceFirmwareVersion_PayloadInvalid = 5
};

struct MeyerDeviceFirmwareVersionSnapshot
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t mainBoardStatus;
    std::int32_t projectionBoardStatus;
    char mainBoardVersionUtf8[32];
    char projectionBoardVersionUtf8[32];
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceFirmwareVersionSnapshot) == 368U,
              "MeyerDeviceFirmwareVersionSnapshot ABI size changed");

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
    // detectedModel 只保存 DeviceCmd 可以可靠映射的结果；无法映射时为 Unknown。
    std::int32_t detectedModel;
    char modelCodeUtf8[MEYER_DEVICE_CMD_MODEL_CODE_UTF8_BYTES];
    std::uint8_t responseLayout;
    std::uint8_t reservedBytes[2];
};

// 本结构替换了原 32 字节 reserved，尺寸仍必须保持 444 字节。
static_assert(sizeof(MeyerDeviceCmdDeviceInfo) == 444U,
              "MeyerDeviceCmdDeviceInfo ABI size changed");

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
    MeyerDeviceProductIdentity productIdentity;
    MeyerDeviceDetectionRecord detectionRecord;
    MeyerDeviceFirmwareVersionSnapshot firmwareVersions;
    MeyerDeviceScanHeadColorCalibrationSnapshot scanHeadColorCalibration;
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(MeyerDeviceCalibrationPreflight) == 2552U,
              "MeyerDeviceCalibrationPreflight ABI size changed");

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
    // 初始化原始流诊断快照结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitStreamDiagnostics(MeyerDeviceCmdStreamDiagnostics* diagnostics);
    // 初始化状态快照结构，供 GetStateSnapshot 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitStateSnapshot(MeyerDeviceStateSnapshot* snapshot);
    // 初始化帧信息结构，供 GetFrame 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFrameInfo(MeyerDeviceCmdFrameInfo* frameInfo);
    // 初始化通用命令响应结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitRawResponse(MeyerDeviceCmdRawResponse* response);
    // 初始化型号描述结构，供 GetModelDescriptor 校验和回填。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitModelDescriptor(MeyerDeviceModelDescriptor* descriptor);
    // 初始化产品识别结果结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitProductIdentity(MeyerDeviceProductIdentity* identity);
    // 初始化设备型号检测记录结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitDetectionRecord(MeyerDeviceDetectionRecord* record);
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
    // 初始化机器码结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitMachineCode(MeyerDeviceCmdMachineCode* machineCode);
    // 初始化颜色校准设备预检结果。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitCalibrationPreflight(MeyerDeviceCalibrationPreflight* preflight);
    // 初始化主控板/投图板版本结果结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitFirmwareVersionSnapshot(
        MeyerDeviceFirmwareVersionSnapshot* versions);
    // 初始化曝光参数结构。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_InitExposureParameters(MeyerDeviceCmdExposureParameters* parameters);
    // 读取指定协议/硬件 Profile 的协议族和能力目录。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetModelDescriptor(std::int32_t model, MeyerDeviceModelDescriptor* descriptor);
    // 根据 0xD9 设备编号和 0xCE 型号代码执行纯目录识别，不访问 USB 设备。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_IdentifyProduct(
        const char* deviceNumberUtf8,
        const char* modelCodeUtf8,
        std::uint64_t baseEvidence,
        MeyerDeviceProductIdentity* identity);

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
    // 只查看底层句柄和 CyAPI 连接状态，不发送设备命令，供每组图开始前轻量检查。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_IsDeviceConnectedLightweight(MeyerDeviceCmdHandle handle);

    // 打开唯一设备会话、检查 USB 速率、读取 0xCD/0xCE 设备信息并识别型号。
    // Ready 时保持会话打开供颜色校准继续使用；其它状态会主动关闭会话。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_PrepareColorCalibration(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdOpenParams* params,
        MeyerDeviceCalibrationPreflight* preflight);

    // 串行读取机器码、主控板版本、必要时投图板版本、电池和设备期限原始信息；
    // 采集中返回 Busy。版本结果从 MeyerDeviceStateSnapshot 的对应字段读取。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_RefreshBasicState(MeyerDeviceCmdHandle handle);
    // 非阻塞复制最近状态快照，不执行 USB I/O。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetStateSnapshot(MeyerDeviceCmdHandle handle, MeyerDeviceStateSnapshot* snapshot);
    // 设置普通开关灯命令；on 为 0 表示关灯，非 0 表示开灯。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetLight(MeyerDeviceCmdHandle handle, std::int32_t on);
    // 设置强制开灯策略；enabled 为 0 表示关闭，非 0 表示启用。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_SetForceLight(MeyerDeviceCmdHandle handle, std::int32_t enabled);

    // 下发 0xFF 使设备控制器软件复位。该命令无响应，调用方随后应关闭并重连会话。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ResetController(MeyerDeviceCmdHandle handle);
    // 发送 0xD4 并解析 0xD9 的 13 位机器码，同时更新状态快照。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReadMachineCode(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdMachineCode* machineCode);
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

    // 新采集架构使用的原始 B 包接口。StartRawCapture 只建立 queueDepth 异步请求环
    // 并发送 0x0A，不在 DeviceTransport 内部组帧或后处理。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StartRawCapture(
        MeyerDeviceCmdHandle handle,
        const MeyerDeviceCmdCaptureParams* params);
    // 发送 0x0B、可选关灯并回收原始流请求环；重复调用保持幂等。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_StopRawCapture(
        MeyerDeviceCmdHandle handle,
        std::int32_t turnLightOff);
    // 阻塞等待一个原始 B 包，只传递字节和实际长度，不解析图像头。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_ReceiveRawCapturePacket(
        MeyerDeviceCmdHandle handle,
        unsigned char* buffer,
        std::size_t capacity,
        std::size_t* receivedSize,
        std::uint32_t timeoutMs);
    // 读取 Transport 累计的包数、超时、部分包和连接丢失诊断。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetStreamDiagnostics(
        MeyerDeviceCmdHandle handle,
        MeyerDeviceCmdStreamDiagnostics* diagnostics);

    // 读取当前句柄最近错误；支持空缓冲区的长度探测调用。
    MEYERSCAN_DEVICE_CMD_API std::int32_t MeyerDeviceCmd_GetLastError(MeyerDeviceCmdHandle handle, char* buffer, std::size_t capacity, std::size_t* requiredSize);
    // 返回稳定模块名。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetModuleName();
    // 返回语义化公共 API 版本字符串。
    MEYERSCAN_DEVICE_CMD_API const char* MeyerDeviceCmd_GetApiVersion();
    // 动态加载器只需要通过 GetProcAddress 解析通用版本导出；隐藏声明可以
    // 避免一个编译单元同时包含多个模块头时发生同名 C 导出声明冲突。
#if !defined(MEYER_DEVICE_CMD_HIDE_GENERIC_EXPORTS)
    // 返回所有自研 DLL 统一使用的整数 ABI 版本。
    MEYERSCAN_DEVICE_CMD_API std::int32_t GetMeyerModuleApiVersion();
    // 返回版本清单读取的代码版本。
    MEYERSCAN_DEVICE_CMD_API const char* GetMeyerModuleVersion();
#endif
}
