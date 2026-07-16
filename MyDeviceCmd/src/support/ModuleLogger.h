// =============================================================================
// 文件: ModuleLogger.h
// 作用: 声明设备命令模块的可选进程级日志适配器。
// =============================================================================
#pragma once

namespace meyer
{
    namespace devicecmd
    {
        namespace logging
        {
            // 记录普通信息；日志模块不可用时由实现静默忽略，不阻断设备流程。
            void WriteInfo(const char* operation, const char* content);
            // 记录可恢复警告；参数使用 UTF-8/ASCII 常量，不转移调用方内存所有权。
            void WriteWarning(const char* operation, const char* content);
            // 记录错误信息；错误日志失败不能覆盖原始设备错误码。
            void WriteError(const char* operation, const char* content);
        }
    }
}
