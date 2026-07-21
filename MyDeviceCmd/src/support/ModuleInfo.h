// =============================================================================
// 文件: ModuleInfo.h
// 作用: 集中保存模块名、代码版本和 ABI 版本，避免多处字符串不一致。
// =============================================================================
#pragma once

namespace ModuleInfo
{
    static const char* const Name = "MeyerScan_DeviceCmd";
    static const char* const Version = "MeyerScan_DeviceCmd v0.7.7 (2026-07-22)";
    static const char* const ApiVersion = "2.3.6";
    // 与 include/DeviceCmd.h 的 MEYER_DEVICE_CMD_API_VERSION 保持一致。
    static const int ApiVersionNumber = 5;
}
