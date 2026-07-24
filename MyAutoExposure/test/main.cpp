// =============================================================================
// 文件: test/main.cpp
// 作用: 验证自动曝光模块的 ABI、上下文保存和“未实现”返回合同。
// =============================================================================
#include "AutoExposure.h"

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
}

int main(int argc, char* argv[])
{
    if (argc < 2 || std::string(argv[1]) != "--smoke")
    {
        std::cout << "AutoExposureTest supports only --smoke.\n";
        return 0;
    }

    MeyerAutoExposureOutput output;
    MeyerCaptureDeviceContext context;
    MeyerCaptureGroupInfo group;
    // AutoExposureTest 只链接自动曝光 DLL，因此不能调用 CaptureProcessing 的
    // Init 函数。这里先把共享 POD 全部清零，再手工写入结构头，等价于公共
    // 初始化函数的最小合同，也能验证 AutoExposure 没有隐藏的模块依赖。
    std::memset(&context, 0, sizeof(context));
    std::memset(&group, 0, sizeof(group));
    MeyerAutoExposure_InitOutput(&output);
    MeyerCaptureProfileConfig defaultProfile;
    std::memset(&defaultProfile, 0, sizeof(defaultProfile));
    // 测试不依赖 CaptureProcessing DLL，手工填写最小合法上下文。
    defaultProfile.structSize = sizeof(defaultProfile);
    defaultProfile.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    defaultProfile.deviceProfile = MeyerCaptureDeviceProfile_MyScan5;
    defaultProfile.width = 1024;
    defaultProfile.height = 455;
    defaultProfile.imageCount = 6;
    defaultProfile.captureMode = MeyerCaptureMode_ColorCalibration;
    group.structSize = sizeof(group);
    group.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    context.structSize = sizeof(context);
    context.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    context.deviceProfile = MeyerCaptureDeviceProfile_MyScan5;
    context.captureMode = MeyerCaptureMode_ColorCalibration;

    MeyerAutoExposureHandle handle = MeyerAutoExposure_Create();
    if (!Check(handle != nullptr, "automatic exposure session creates"))
    {
        return 1;
    }
    if (!Check(MeyerAutoExposure_Configure(handle, &context) == MeyerAutoExposureResult_Ok,
               "automatic exposure session stores device context"))
    {
        MeyerAutoExposure_Destroy(handle);
        return 2;
    }

    std::vector<unsigned char> dummy(1024U, 0U);
    const std::int32_t result = MeyerAutoExposure_Calculate(
        handle, &defaultProfile, &group, &dummy[0], dummy.size(), &output);
    const bool valid = Check(result == MeyerAutoExposureResult_NotImplemented,
                             "automatic exposure explicitly reports not implemented") &&
                       Check(output.valid == 0 && output.commandPayload[0] == 0U,
                             "reserved output cannot be mistaken for a valid command");
    MeyerAutoExposure_Destroy(handle);
    std::cout << "AutoExposureTest " << (valid ? "passed" : "failed") << ".\n";
    return valid ? 0 : 3;
}
