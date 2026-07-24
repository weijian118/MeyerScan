// =============================================================================
// 文件: test/main.cpp
// 作用: 无硬件验证图像流水线的 RGB、重建副本、功能门禁和两次缓冲区合同。
// =============================================================================
#include "CaptureImagePipeline.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (condition)
        {
            std::cout << "[PASS] " << message << "\n";
            return true;
        }
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    // 测试使用很小的 8x8 六图，验证通道映射而不分配真实 1024x455 大缓冲。
    void InitializeContract(MeyerCaptureProfileConfig& profile,
                            MeyerCaptureDeviceContext& context,
                            MeyerCaptureGroupInfo& group,
                            MeyerCapturePipelineOptions& options)
    {
        std::memset(&profile, 0, sizeof(profile));
        profile.structSize = sizeof(profile);
        profile.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        profile.deviceProfile = MeyerCaptureDeviceProfile_MyScan5;
        profile.captureMode = MeyerCaptureMode_ColorCalibration;
        profile.width = 8;
        profile.height = 8;
        profile.imageCount = 6;
        profile.headerBytes = 4U;

        std::memset(&context, 0, sizeof(context));
        context.structSize = sizeof(context);
        context.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        context.deviceSeries = 5;
        context.deviceProfile = MeyerCaptureDeviceProfile_MyScan5;
        context.captureMode = MeyerCaptureMode_ColorCalibration;
        std::strncpy(context.deviceSeriesUtf8, "mOS MyScan 5",
                     sizeof(context.deviceSeriesUtf8) - 1U);

        std::memset(&group, 0, sizeof(group));
        group.structSize = sizeof(group);
        group.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
        group.groupSequence = 7U;
        group.width = profile.width;
        group.height = profile.height;
        group.imageCount = profile.imageCount;
        group.slowProcessed = 1;
        group.device = context;

        MeyerCaptureImagePipeline_InitOptions(&options);
        options.captureMode = MeyerCaptureMode_ColorCalibration;
        options.optionsRevision = 3U;
    }
}

int main(int argc, char* argv[])
{
    if (argc <= 1 || std::string(argv[1]) != "--smoke")
    {
        std::cout << "CaptureImagePipelineTest supports only --smoke.\n";
        return 0;
    }

    MeyerCaptureProfileConfig profile = {};
    MeyerCaptureDeviceContext context = {};
    MeyerCaptureGroupInfo group = {};
    MeyerCapturePipelineOptions options = {};
    InitializeContract(profile, context, group, options);

    MeyerCaptureImagePipelineHandle handle = MeyerCaptureImagePipeline_Create();
    if (!Check(handle != nullptr, "pipeline session creates") ||
        !Check(MeyerCaptureImagePipeline_Configure(handle, &profile, &context) ==
                   MeyerCaptureImagePipelineResult_Ok,
               "pipeline session configures with device context"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 1;
    }

    const std::size_t planeBytes = 64U;
    std::vector<unsigned char> normalized(planeBytes * 6U, 0U);
    // 标准化顺序为 黑/R/G/B/激光G/激光B，像素 10 用于检查 RGB 通道。
    normalized[planeBytes + 10U] = 91U;
    normalized[planeBytes * 2U + 10U] = 112U;
    normalized[planeBytes * 3U + 10U] = 133U;
    if (!Check(MeyerCaptureImagePipeline_ProcessGroup(
                   handle, &normalized[0], normalized.size(), &group, &options) ==
                   MeyerCaptureImagePipelineResult_Ok,
               "base pipeline produces outputs without optional algorithms"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 2;
    }

    MeyerCapturePipelineOutputInfo info = {};
    MeyerCaptureImagePipeline_InitOutputInfo(&info);
    if (!Check(MeyerCaptureImagePipeline_GetOutputInfo(
                   handle, MeyerCapturePipelineOutput_DisplayRgb888, &info) ==
                   MeyerCaptureImagePipelineResult_Ok,
               "RGB output metadata is available") ||
        !Check(info.byteSize == planeBytes * 3U && info.channels == 3 &&
                   info.groupSequence == 7U && info.optionsRevision == 3U,
               "RGB output metadata keeps size, group and options revision"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 3;
    }

    std::size_t required = 0U;
    if (!Check(MeyerCaptureImagePipeline_CopyOutput(
                   handle, MeyerCapturePipelineOutput_DisplayRgb888,
                   nullptr, 0U, &required) ==
                   MeyerCaptureImagePipelineResult_BufferTooSmall,
               "RGB output supports size probing"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 4;
    }
    std::vector<unsigned char> rgb(required, 0U);
    if (!Check(MeyerCaptureImagePipeline_CopyOutput(
                   handle, MeyerCapturePipelineOutput_DisplayRgb888,
                   &rgb[0], rgb.size(), &required) ==
                   MeyerCaptureImagePipelineResult_Ok,
               "RGB output copies to caller memory") ||
        !Check(rgb[10U * 3U] == 91U && rgb[10U * 3U + 1U] == 112U &&
                   rgb[10U * 3U + 2U] == 133U,
               "RGB output maps normalized R/G/B planes") ||
        !Check(rgb[0] == 0U && rgb[1] == 0U && rgb[2] == 0U,
               "protocol header pixels are hidden from display RGB"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 5;
    }

    required = 0U;
    MeyerCaptureImagePipeline_CopyOutput(
        handle, MeyerCapturePipelineOutput_ReconstructionPlanes,
        nullptr, 0U, &required);
    std::vector<unsigned char> reconstruction(required, 0U);
    if (!Check(MeyerCaptureImagePipeline_CopyOutput(
                   handle, MeyerCapturePipelineOutput_ReconstructionPlanes,
                   &reconstruction[0], reconstruction.size(), &required) ==
                   MeyerCaptureImagePipelineResult_Ok,
               "reconstruction six-plane output copies") ||
        !Check(reconstruction == normalized,
               "base reconstruction output is an exact normalized deep copy"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 6;
    }

    // 高级算法尚未接入时，required 功能必须失败且清除旧输出，避免 UI
    // 误用上一组成功结果；这里只验证接口占位，不伪造颜色校准图。
    options.enabledFeatures = MeyerCapturePipelineFeature_ColorCalibration;
    options.requiredFeatures = MeyerCapturePipelineFeature_ColorCalibration;
    if (!Check(MeyerCaptureImagePipeline_ProcessGroup(
                   handle, &normalized[0], normalized.size(), &group, &options) ==
                   MeyerCaptureImagePipelineResult_FeatureUnavailable,
               "required unavailable algorithm fails explicitly"))
    {
        MeyerCaptureImagePipeline_Destroy(handle);
        return 7;
    }
    MeyerCaptureImagePipeline_InitOutputInfo(&info);
    Check(MeyerCaptureImagePipeline_GetOutputInfo(
              handle, MeyerCapturePipelineOutput_DisplayRgb888, &info) ==
              MeyerCaptureImagePipelineResult_NoData,
          "failed processing does not expose stale output");

    MeyerCaptureImagePipeline_Destroy(handle);
    std::cout << "CaptureImagePipelineTest passed.\n";
    return 0;
}
