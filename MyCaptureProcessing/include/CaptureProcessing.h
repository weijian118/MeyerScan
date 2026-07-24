// =============================================================================
// 文件: CaptureProcessing.h
// 模块: MeyerScan_CaptureProcessing.dll
// 作用: 定义原始 B 包组图、单图解密、状态汇总和慢处理的纯 C ABI。
//
// 边界:
//   - 不使用 Qt，不打开 USB，不发送设备命令，不创建线程。
//   - 调用方负责线程和队列；本 DLL 只处理传入的一个包或一组图。
//   - 所有图像缓冲由调用方分配，不跨 DLL 传递 std::vector 所有权。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_CAPTURE_PROCESSING_EXPORTS)
#  define MEYERSCAN_CAPTURE_PROCESSING_API __declspec(dllexport)
#else
#  define MEYERSCAN_CAPTURE_PROCESSING_API __declspec(dllimport)
#endif

static const std::uint32_t MEYER_CAPTURE_PROCESSING_SCHEMA_VERSION = 1U;
static const std::int32_t MEYER_CAPTURE_PROCESSING_API_VERSION = 1;

// 负数是失败，0 是普通成功，正数是 PushPacket 的状态推进结果。
enum MeyerCaptureProcessingResult : std::int32_t
{
    MeyerCaptureProcessingResult_Ok = 0,
    MeyerCaptureProcessingResult_NeedMorePackets = 1,
    MeyerCaptureProcessingResult_ImageCompleted = 2,
    MeyerCaptureProcessingResult_GroupCompleted = 3,
    MeyerCaptureProcessingResult_SyncReset = 4,
    MeyerCaptureProcessingResult_NoCompletedGroup = 5,
    MeyerCaptureProcessingResult_InvalidHandle = -1,
    MeyerCaptureProcessingResult_InvalidArgument = -2,
    MeyerCaptureProcessingResult_InvalidProfile = -3,
    MeyerCaptureProcessingResult_InvalidPacket = -4,
    MeyerCaptureProcessingResult_InvalidHeader = -5,
    MeyerCaptureProcessingResult_InvalidState = -6,
    MeyerCaptureProcessingResult_BufferTooSmall = -7,
    MeyerCaptureProcessingResult_DecryptFailed = -8,
    MeyerCaptureProcessingResult_InternalError = -9
};

typedef void* MeyerCaptureProcessingHandle;

extern "C"
{
    // 初始化可扩展结构，不允许调用方依赖未清零栈内存。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_InitProfile(
        MeyerCaptureProfileConfig* profile);
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_InitDeviceContext(
        MeyerCaptureDeviceContext* context);
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_InitGroupInfo(
        MeyerCaptureGroupInfo* info);

    // 从集中 Profile 目录取得 MyScan 5/5H/6 默认参数。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_GetDefaultProfile(
        std::int32_t deviceProfile,
        std::int32_t captureMode,
        MeyerCaptureProfileConfig* profile);

    // 一个句柄只由一个快速采集线程使用。
    MEYERSCAN_CAPTURE_PROCESSING_API MeyerCaptureProcessingHandle MeyerCaptureProcessing_Create();
    MEYERSCAN_CAPTURE_PROCESSING_API void MeyerCaptureProcessing_Destroy(
        MeyerCaptureProcessingHandle handle);
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_Configure(
        MeyerCaptureProcessingHandle handle,
        const MeyerCaptureProfileConfig* profile,
        const MeyerCaptureDeviceContext* context);
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_Reset(
        MeyerCaptureProcessingHandle handle);

    // 推入一个完整传输 B 包。返回 GroupCompleted 时应立即调用 CopyCompletedGroup。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_PushPacket(
        MeyerCaptureProcessingHandle handle,
        const unsigned char* packet,
        std::size_t packetBytes);

    // 超时、部分包或设备拔出时废弃当前不完整组，已完成组不受影响。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_AbortIncompleteGroup(
        MeyerCaptureProcessingHandle handle);

    // 两次缓冲区合同：容量不足时 requiredBytes 返回所需大小，完成组仍保留。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_CopyCompletedGroup(
        MeyerCaptureProcessingHandle handle,
        unsigned char* groupBuffer,
        std::size_t capacity,
        std::size_t* requiredBytes,
        MeyerCaptureGroupInfo* groupInfo);

    // 慢处理是无状态函数，可在 CaptureService 后处理线程中使用。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_ProcessSlowGroup(
        const MeyerCaptureProfileConfig* profile,
        const unsigned char* decryptedGroup,
        std::size_t decryptedBytes,
        const MeyerCaptureGroupInfo* inputInfo,
        unsigned char* processedGroup,
        std::size_t capacity,
        std::size_t* requiredBytes,
        MeyerCaptureGroupInfo* outputInfo);

    // 最近错误文本只用于诊断，UI 提示应根据结构化错误码选择 tr() 文案。
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t MeyerCaptureProcessing_GetLastError(
        MeyerCaptureProcessingHandle handle,
        char* buffer,
        std::size_t capacity,
        std::size_t* requiredSize);

#if !defined(MEYER_CAPTURE_PROCESSING_HIDE_GENERIC_EXPORTS)
    MEYERSCAN_CAPTURE_PROCESSING_API std::int32_t GetMeyerModuleApiVersion();
    MEYERSCAN_CAPTURE_PROCESSING_API const char* GetMeyerModuleVersion();
#endif
}
