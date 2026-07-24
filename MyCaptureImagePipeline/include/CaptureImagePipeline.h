// =============================================================================
// 文件: CaptureImagePipeline.h
// 模块: MeyerScan_CaptureImagePipeline.dll
// 作用: 定义标准化六图之后、UI/重建算法之前的场景级图像处理流水线 C ABI。
//
// 边界说明:
//   1. 输入必须是 CaptureProcessing 已完成排序、镜像和减黑图的标准化六图。
//   2. 本模块根据场景、设备上下文和 UI 功能开关生成多个具名输出。
//   3. 后续颜色校准、AI 软组织、除色和粗条纹算法由本模块动态适配，
//      CaptureService 和 UI 不直接加载这些算法 DLL。
//   4. 第一版只实现基础 RGB888 和重建六图副本；未接入算法的功能会返回
//      明确不可用状态，不会生成伪造结果。
// =============================================================================
#pragma once

#include "MeyerCaptureTypes.h"

#include <cstddef>
#include <cstdint>

#if defined(MEYERSCAN_CAPTURE_IMAGE_PIPELINE_EXPORTS)
#  define MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API __declspec(dllexport)
#else
#  define MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API __declspec(dllimport)
#endif

static const std::uint32_t MEYER_CAPTURE_IMAGE_PIPELINE_SCHEMA_VERSION = 1U;
static const std::int32_t MEYER_CAPTURE_IMAGE_PIPELINE_API_VERSION = 1;

enum MeyerCaptureImagePipelineResult : std::int32_t
{
    MeyerCaptureImagePipelineResult_Ok = 0,
    MeyerCaptureImagePipelineResult_InvalidHandle = -1,
    MeyerCaptureImagePipelineResult_InvalidArgument = -2,
    MeyerCaptureImagePipelineResult_NotConfigured = -3,
    MeyerCaptureImagePipelineResult_FeatureUnavailable = -4,
    MeyerCaptureImagePipelineResult_NoData = -5,
    MeyerCaptureImagePipelineResult_BufferTooSmall = -6,
    MeyerCaptureImagePipelineResult_InternalError = -7
};

typedef void* MeyerCaptureImagePipelineHandle;

extern "C"
{
    // 初始化公共选项和输出描述，调用方不能依赖未清零的栈内存。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_InitOptions(MeyerCapturePipelineOptions* options);
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_InitOutputInfo(MeyerCapturePipelineOutputInfo* info);

    // 一个句柄对应一个采集会话，可在内部持有后续算法状态和历史参数。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API MeyerCaptureImagePipelineHandle
        MeyerCaptureImagePipeline_Create();
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API void MeyerCaptureImagePipeline_Destroy(
        MeyerCaptureImagePipelineHandle handle);

    // Profile 和设备上下文在准入完成后配置；换机或换场景必须重新配置。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_Configure(
            MeyerCaptureImagePipelineHandle handle,
            const MeyerCaptureProfileConfig* profile,
            const MeyerCaptureDeviceContext* context);

    // 处理一组标准化六图。函数返回后可按 outputType 查询并复制多个输出。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_ProcessGroup(
            MeyerCaptureImagePipelineHandle handle,
            const unsigned char* normalizedGroup,
            std::size_t normalizedBytes,
            const MeyerCaptureGroupInfo* groupInfo,
            const MeyerCapturePipelineOptions* options);

    // 查询输出元数据。当前输出不存在时返回 NoData，同时 info.available=0。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_GetOutputInfo(
            MeyerCaptureImagePipelineHandle handle,
            std::int32_t outputType,
            MeyerCapturePipelineOutputInfo* info);

    // 使用两次缓冲区合同复制指定输出；DLL 不把内部 vector 指针交给调用方。
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_CopyOutput(
            MeyerCaptureImagePipelineHandle handle,
            std::int32_t outputType,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t* requiredBytes);

    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t
        MeyerCaptureImagePipeline_GetLastError(
            MeyerCaptureImagePipelineHandle handle,
            char* buffer,
            std::size_t capacity,
            std::size_t* requiredSize);

#if !defined(MEYER_CAPTURE_IMAGE_PIPELINE_HIDE_GENERIC_EXPORTS)
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API std::int32_t GetMeyerModuleApiVersion();
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_API const char* GetMeyerModuleVersion();
#endif
}
