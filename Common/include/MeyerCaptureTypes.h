// =============================================================================
// 文件: MeyerCaptureTypes.h
// 作用: 定义数据采集、慢处理和 UI 之间共用的固定布局 POD。
//
// 设计约束:
//   1. 本文件不依赖 Qt、CyAPI、STL 容器或任何内存所有权。
//   2. DLL 边界只复制这些结构和调用方分配的字节缓冲区。
//   3. 新字段只能追加或消耗 reserved，不能改变已发布字段含义。
// =============================================================================
#pragma once

#include <cstdint>
#include <type_traits>

static const std::uint32_t MEYER_CAPTURE_TYPES_SCHEMA_VERSION = 1U;
static const std::uint32_t MEYER_CAPTURE_MAX_IMAGE_COUNT = 8U;
static const std::uint32_t MEYER_CAPTURE_TEXT_SHORT_BYTES = 32U;
static const std::uint32_t MEYER_CAPTURE_TEXT_MEDIUM_BYTES = 64U;
static const std::uint32_t MEYER_CAPTURE_PIPELINE_MAX_OUTPUT_COUNT = 8U;

// 协议/采集 Profile 与销售产品型号分开。MyScan 5H 当前参数相同，仍保留独立值。
enum MeyerCaptureDeviceProfile : std::int32_t
{
    MeyerCaptureDeviceProfile_Unknown = 0,
    MeyerCaptureDeviceProfile_MyScan5 = 5,
    MeyerCaptureDeviceProfile_MyScan5H = 50,
    MeyerCaptureDeviceProfile_MyScan6 = 6
};

// 采集用途影响准入和后续曝光策略，但不改变原始字节的所有权。
enum MeyerCaptureMode : std::int32_t
{
    MeyerCaptureMode_Unknown = 0,
    MeyerCaptureMode_ColorCalibration = 1,
    MeyerCaptureMode_Calibration3D = 2,
    MeyerCaptureMode_PracticeScan = 3,
    MeyerCaptureMode_OrderScan = 4
};

// 扫描头值沿用下位机数据头编码。
enum MeyerCaptureScanHeadType : std::int32_t
{
    MeyerCaptureScanHead_Unknown = 0,
    MeyerCaptureScanHead_Large = 1,
    MeyerCaptureScanHead_Small = 2,
    MeyerCaptureScanHead_NotInserted = 3
};

// 图像解密策略是 Profile 的一部分，不应在 UI 中根据型号写 if/else。
enum MeyerCaptureEncryptionMode : std::int32_t
{
    MeyerCaptureEncryption_None = 0,
    // 前 40 字节保留，后续每字节执行历史逆 S-Box 替换。
    MeyerCaptureEncryption_LegacyInverseSubstitution40 = 1
};

// 场景级图像再处理功能位。功能位只描述调用方意图；对应算法 DLL 未加载时
// Pipeline 必须返回明确的 FeatureUnavailable，不能静默跳过后伪装成功。
static const std::uint64_t MeyerCapturePipelineFeature_ColorCalibration = 1ULL << 0;
static const std::uint64_t MeyerCapturePipelineFeature_AiSoftTissue = 1ULL << 1;
static const std::uint64_t MeyerCapturePipelineFeature_ColorRemoval = 1ULL << 2;
static const std::uint64_t MeyerCapturePipelineFeature_CoarseStripe = 1ULL << 3;

// Pipeline 允许同时产生多路输出。前两个输出在第一版可用，后续输出只有在
// 对应算法真正接入并通过版本门禁后才会标记 available。
enum MeyerCapturePipelineOutputType : std::int32_t
{
    MeyerCapturePipelineOutput_Unknown = 0,
    MeyerCapturePipelineOutput_DisplayRgb888 = 1,
    MeyerCapturePipelineOutput_ReconstructionPlanes = 2,
    MeyerCapturePipelineOutput_ColorCalibratedRgb888 = 3,
    MeyerCapturePipelineOutput_SoftTissueMask = 4,
    MeyerCapturePipelineOutput_ColorRemovedRgb888 = 5,
    MeyerCapturePipelineOutput_CoarseStripe = 6
};

enum MeyerCapturePipelineDataFormat : std::int32_t
{
    MeyerCapturePipelineDataFormat_Unknown = 0,
    MeyerCapturePipelineDataFormat_Gray8Planes = 1,
    MeyerCapturePipelineDataFormat_Rgb888 = 2,
    MeyerCapturePipelineDataFormat_Mask8 = 3
};

// 机型采集参数。logicalImageBytes 由 width*height 得到，不与 wire 填充混用。
struct MeyerCaptureProfileConfig
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t deviceProfile;
    std::int32_t captureMode;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t packetsPerImage;
    std::uint32_t packetBytes;
    std::uint32_t lastPacketValidBytes;
    std::uint32_t headerBytes;
    std::uint32_t queueDepth;
    std::uint32_t receiveTimeoutMs;
    std::uint32_t postProcessQueueCapacity;
    std::int32_t frameRate;
    std::int32_t encryptionMode;
    char profileNameUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char profileVersionUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    std::uint32_t reserved[8];
};

// 设备身份上下文随每组图复制，保证异步后处理不会读到后来换机后的快照。
struct MeyerCaptureDeviceContext
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t validFields;
    std::int32_t deviceSeries;
    std::int32_t deviceProfile;
    std::int32_t deviceIdStatus;
    std::int32_t deviceModel;
    std::int32_t productModel;
    std::int32_t productionMode;
    std::int32_t captureMode;
    std::int32_t reserved32;
    char deviceSeriesUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char deviceProfileUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char deviceIdUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char modelCodeUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char deviceModelUtf8[MEYER_CAPTURE_TEXT_MEDIUM_BYTES];
    char firmwareVersionUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    char captureSessionIdUtf8[MEYER_CAPTURE_TEXT_MEDIUM_BYTES];
    std::uint32_t reserved[8];
};

// 保留每张单图的原始状态，整组汇总不覆盖这些证据。
struct MeyerCapturePlaneState
{
    std::int32_t imageIndex;
    std::int32_t sourceImageIndex;
    std::int32_t ledRaw;
    std::int32_t longPressRaw;
    std::int32_t scanHeadRaw;
    std::int32_t valid;
    std::uint32_t reserved[2];
};

// 一组已解密或已慢处理图像的元数据。字节内容仍由调用方缓冲区承载。
struct MeyerCaptureGroupInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint64_t groupSequence;
    std::uint64_t groupBytes;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t ledOn;
    std::int32_t longPressed;
    std::int32_t scanHeadType;
    std::int32_t slowProcessed;
    std::int32_t stateRuleVersion;
    MeyerCaptureDeviceContext device;
    MeyerCapturePlaneState planes[MEYER_CAPTURE_MAX_IMAGE_COUNT];
    std::uint32_t reserved[8];
};

// UI 或业务场景传入的处理开关快照。optionsRevision 由上层每次修改开关时
// 递增，一组六图只使用开始处理时复制到本地的同一份快照。
struct MeyerCapturePipelineOptions
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t captureMode;
    std::int32_t reserved32;
    std::uint64_t enabledFeatures;
    std::uint64_t requiredFeatures;
    std::uint32_t optionsRevision;
    std::uint32_t reservedRevision;
    char colorCalibrationProfileUtf8[MEYER_CAPTURE_TEXT_MEDIUM_BYTES];
    std::uint32_t reserved[8];
};

// 每种输出都带独立描述，调用方先查询描述再按 byteSize 分配缓冲区。
struct MeyerCapturePipelineOutputInfo
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t outputType;
    std::int32_t available;
    std::int32_t width;
    std::int32_t height;
    std::int32_t imageCount;
    std::int32_t channels;
    std::int32_t dataFormat;
    std::int32_t captureMode;
    std::uint64_t byteSize;
    std::uint64_t groupSequence;
    std::uint64_t appliedFeatures;
    std::uint64_t unavailableFeatures;
    std::uint32_t optionsRevision;
    char producerVersionUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    std::uint32_t reserved[8];
};

static_assert(std::is_standard_layout<MeyerCaptureProfileConfig>::value,
              "MeyerCaptureProfileConfig must remain a standard-layout POD");
static_assert(std::is_standard_layout<MeyerCaptureDeviceContext>::value,
              "MeyerCaptureDeviceContext must remain a standard-layout POD");
static_assert(std::is_standard_layout<MeyerCaptureGroupInfo>::value,
              "MeyerCaptureGroupInfo must remain a standard-layout POD");
static_assert(std::is_standard_layout<MeyerCapturePipelineOptions>::value,
              "MeyerCapturePipelineOptions must remain a standard-layout POD");
static_assert(std::is_standard_layout<MeyerCapturePipelineOutputInfo>::value,
              "MeyerCapturePipelineOutputInfo must remain a standard-layout POD");
