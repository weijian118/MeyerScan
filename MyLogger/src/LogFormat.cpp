// =============================================================================
// 文件:    LogFormat.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 时间戳由 Windows GetLocalTime() API 生成。这提供毫秒精度
//     (wMilliseconds 字段) 和本地时区表示 —— 日志在写入它的机器上阅读。
//   - 所有格式化使用 snprintf。它:
//       a) 是 C99 标准（自 VS2015 起通过 _snprintf 在 MSVC 中可用，
//          但 UCRT 提供了符合标准的 snprintf）。
//       b) 对日志路径来说足够快（通常每次调用 < 1 μs）。
//       c) 容易审计缓冲区溢出（显式缓冲区大小）。
//   - 输出缓冲区是 2048 字节的固定大小栈数组。这对任何合理的日志行来说
//     都足够大（典型行 120–300 字节），同时防止无界的栈增长。
//     如果行超过 2047 字节，会被静默截断。需要长消息的调用方
//     应将其拆分为多次 Write() 调用。
//   - 尾随换行符被有意省略。LogWriter::WriteLine()
//     在获取跨进程互斥量后追加 '\n'，确保每次系统级写入
//     恰好是一条完整的日志条目。
// =============================================================================

#include "LogFormat.h"
#include <cstdio>    // snprintf
#include <windows.h> // GetLocalTime, SYSTEMTIME

namespace LogFormat {

// ---------------------------------------------------------------------------
// LevelName
// ---------------------------------------------------------------------------
// 将 LogLevel 枚举映射为 5 字符、左对齐的显示字符串。
// 字符串用尾随空格填充，使所有级别名称在日志文件中恰好占据 5 列。
// 这使得用固定宽度模式 grep 特定级别变得容易:
//   grep "\[ERROR\]"  → 仅匹配 Error
//   grep "\[INFO \]"  → 仅匹配 Info（包含空格）
//
// 返回的指针指向静态字符串字面量；调用方不得释放它。
const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
    case LogLevel::Fatal:   return "FATAL";
    }
    // 如果到达此处，说明枚举被损坏或从无效整数强制转换而来。
    // 输出视觉上明显的内容，以便日志文件阅读者能发现异常。
    return "?????";
}

// ---------------------------------------------------------------------------
// FormatLine
// ---------------------------------------------------------------------------
std::string FormatLine(LogLevel level,
                       const char* module,
                       const char* operation,
                       const char* deviceId,
                       const char* caseId,
                       const char* operator_,
                       const char* content) {

    // ---- 1. 时间戳 ------------------------------------------------------------
    // GetLocalTime 以本地时区返回当前系统时间，
    // wMilliseconds 被填充（0–999）。这是有文档记录且可靠的，
    // 适用于 Windows 2000 及更高版本。
    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
    // ts 现在恰好是 23 个字符: "YYYY-MM-DD HH:MM:SS.mmm"
    // 保证以 null 结尾，因为当缓冲区大小 > 0 时 snprintf 总是写入 '\0'
    //（即使发生截断）。

    // ---- 2. 清理空/null 字段 --------------------------------------------------
    // 将 nullptr 和空字符串替换为单个短横线。这保持列在视觉上对齐，
    // 并避免 "(null)" 或空白字段使解析变得更困难。
    //
    // lambda 不捕获任何内容，可被简单内联。
    auto safe = [](const char* s, const char* fallback) -> const char* {
        // s == nullptr   → 使用 fallback
        // s[0] == '\0'   → 字符串为空 → 使用 fallback
        return (s && s[0]) ? s : fallback;
    };

    const char* mod  = safe(module,    "-");
    const char* op   = safe(operation, "-");
    const char* dev  = safe(deviceId,  "-");
    const char* cid  = safe(caseId,    "-");
    const char* opn  = safe(operator_, "-");
    const char* txt  = safe(content,   "");  // content 可以为空字符串

    // ---- 3. 组装日志行 --------------------------------------------------------
    // 格式说明符:
    //   %-5s   → 级别名称，在 5 字符字段中左对齐
    //   %-31s  → 模块/操作，在 31 字符字段中左对齐
    //   %-8s   → deviceId，在 8 字符字段中左对齐
    //   %-16s  → caseId/操作员，在 16 字符字段中左对齐
    //
    // content 之前的固定开销总计: ~108 字符。
    // content 从约第 109 个字符位置开始，为消息留下约 1939 字节。
    //
    // 如果消息超过约 1939 字节，snprintf 会截断并返回
    // ≥ sizeof(line) 的值。我们通过将 n 限制在 sizeof(line)-1 来处理。
    char line[2048];
    int n = snprintf(line, sizeof(line),
                     "[%s] [%-5s] [%-31s] [%-31s] [Dev:%-8s] [Case:%-16s] [Op:%-16s] %s",
                     ts,               // 23 字符: 时间戳
                     LevelName(level), // 5 字符: 级别
                     mod,              // ≤31 字符: 模块名称
                     op,               // ≤31 字符: 操作名称
                     dev,              // ≤8 字符: 设备序列号
                     cid,              // ≤16 字符: 病例 ID
                     opn,              // ≤16 字符: 操作员
                     txt);             // 自由文本

    // snprintf 返回在缓冲区足够大时本应写入的字符数
    //（C99 行为）。如果 n < 0，发生了编码错误
    //（对于 ASCII/UTF-8 不应发生）。
    // 将 n 限制在可用范围内。
    if (n < 0) {
        n = 0;
    }
    if (n >= static_cast<int>(sizeof(line))) {
        n = static_cast<int>(sizeof(line)) - 1;
    }

    // 从缓冲区构造返回字符串。std::string(char*, size)
    // 恰好复制 n 个字节 — 无需调用 strlen()。
    return std::string(line, static_cast<size_t>(n));
}

} // namespace LogFormat
