// =============================================================================
// 文件: ModuleLogger.h
// 作用: 声明 CaptureService 的可选进程级日志适配器。
// =============================================================================
#pragma once

namespace meyer
{
    namespace captureservice
    {
        // 日志 DLL 不可用时这些函数静默返回，不能阻断采集链路。
        void WriteInfo(const char* operation, const char* content);
        void WriteWarning(const char* operation, const char* content);
        void WriteError(const char* operation, const char* content);
    }
}
