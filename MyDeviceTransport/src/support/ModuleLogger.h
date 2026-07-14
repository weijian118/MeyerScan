// =============================================================================
// 文件: ModuleLogger.h
// 作用: 声明设备传输模块的可选日志适配器。
// =============================================================================
#pragma once

namespace meyer
{
    namespace device
    {
        namespace logging
        {
            // 输出关键成功事件。Logger 未初始化或 DLL 不存在时安全跳过。
            void WriteInfo(const char* operation, const char* content);

            // 输出影响设备功能的错误事件。
            void WriteError(const char* operation, const char* content);
        }
    }
}
