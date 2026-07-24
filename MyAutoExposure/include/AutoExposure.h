// =============================================================================
// 文件: AutoExposure.h
// 模块: MeyerScan_AutoExposure.dll
// 作用: 为自动曝光算法保留稳定的纯 C ABI，当前不实现算法。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_AUTO_EXPOSURE_EXPORTS)
#  define MEYERSCAN_AUTO_EXPOSURE_API __declspec(dllexport)
#else
#  define MEYERSCAN_AUTO_EXPOSURE_API __declspec(dllimport)
#endif

enum MeyerAutoExposureResult : std::int32_t
{
    MeyerAutoExposureResult_Ok = 0,
    MeyerAutoExposureResult_NotImplemented = 1,
    MeyerAutoExposureResult_InvalidHandle = -1,
    MeyerAutoExposureResult_InvalidArgument = -2,
    MeyerAutoExposureResult_InvalidProfile = -3,
    MeyerAutoExposureResult_InternalError = -4
};

// 输出结构先固定为设备命令需要的 16 字节，并保留算法版本和有效标志。
struct MeyerAutoExposureOutput
{
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t valid;
    std::int32_t diagnosticCode;
    char algorithmVersionUtf8[MEYER_CAPTURE_TEXT_SHORT_BYTES];
    std::uint8_t commandPayload[16];
    std::uint32_t reserved[8];
};

typedef void* MeyerAutoExposureHandle;

extern "C"
{
    MEYERSCAN_AUTO_EXPOSURE_API std::int32_t MeyerAutoExposure_InitOutput(
        MeyerAutoExposureOutput* output);
    MEYERSCAN_AUTO_EXPOSURE_API MeyerAutoExposureHandle MeyerAutoExposure_Create();
    MEYERSCAN_AUTO_EXPOSURE_API void MeyerAutoExposure_Destroy(MeyerAutoExposureHandle handle);
    // 保存会话级设备上下文，为未来的历史曝光参数提供位置。
    MEYERSCAN_AUTO_EXPOSURE_API std::int32_t MeyerAutoExposure_Configure(
        MeyerAutoExposureHandle handle,
        const MeyerCaptureDeviceContext* context);
    // 当前只返回 NotImplemented，绝不伪造有效曝光命令。
    MEYERSCAN_AUTO_EXPOSURE_API std::int32_t MeyerAutoExposure_Calculate(
        MeyerAutoExposureHandle handle,
        const MeyerCaptureProfileConfig* profile,
        const MeyerCaptureGroupInfo* groupInfo,
        const unsigned char* decryptedGroup,
        std::size_t decryptedBytes,
        MeyerAutoExposureOutput* output);
    MEYERSCAN_AUTO_EXPOSURE_API std::int32_t MeyerAutoExposure_GetLastError(
        MeyerAutoExposureHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize);
#if !defined(MEYER_AUTO_EXPOSURE_HIDE_GENERIC_EXPORTS)
    MEYERSCAN_AUTO_EXPOSURE_API std::int32_t GetMeyerModuleApiVersion();
    MEYERSCAN_AUTO_EXPOSURE_API const char* GetMeyerModuleVersion();
#endif
}
