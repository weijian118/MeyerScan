#pragma once

// 模块名、代码版本和 API 版本集中在此处，避免日志、版本清单和公共接口
// 分别维护字符串后发生不一致。Version 的语义版本必须与 Version.rc 一致。
namespace ModuleInfo
{
    static const char* const Name = "MeyerScan_DeviceTransport";
    static const char* const Version = "MeyerScan_DeviceTransport v1.3.0 (2026-07-24)";
    static const char* const ApiVersion = "1.1.0";
    static const int ApiVersionNumber = 2;
}
