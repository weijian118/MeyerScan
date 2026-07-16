// =============================================================================
// 文件: ModuleInfo.h
// 作用: 集中保存模块名、代码版本和 ABI 版本，避免多处字符串不一致。
// =============================================================================
#pragma once

namespace ModuleInfo
{
    static const char* const Name = "MeyerScan_DeviceCmd";
    static const char* const Version = "MeyerScan_DeviceCmd v0.2.0 (2026-07-16)";
    static const char* const ApiVersion = "1.1.0";
    static const int ApiVersionNumber = 1;
}
