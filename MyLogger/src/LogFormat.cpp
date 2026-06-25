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
//   - 输出采用“有值才显示”的紧凑格式。deviceId/caseId/operator 为空时
//     不再输出 [Dev:-]、[Case:-]、[Opr:-]，避免日志观感很差。
//   - 日志正文固定使用 [Content:] 标签。该标签已经写入全局文档和测试，
//     后续日志读取/分析工具也会按这个字段名解析。
//   - 尾随换行符被有意省略。LogWriter::WriteLineUnlocked()
//     会追加 CRLF，确保每次系统级写入恰好是一条完整日志条目。
// =============================================================================

#include "LogFormat.h"
#include <cstdio>    // snprintf
#include <string>    // std::string
#include <windows.h> // GetLocalTime, SYSTEMTIME

namespace LogFormat {

// ---------------------------------------------------------------------------
// LevelName
// ---------------------------------------------------------------------------
// 将 LogLevel 枚举映射为显示字符串。
// 不再补尾随空格，避免日志中出现 [INFO ] 这类视觉噪音。
// 返回的指针指向静态字符串字面量；调用方不得释放它。
const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
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
    // 空字段直接省略，只有 module/operation/content 作为日志基础字段保留。
    auto safe = [](const char* s) -> const char* {
        return (s && s[0]) ? s : "";
    };

    const char* mod  = safe(module);
    const char* op   = safe(operation);
    const char* dev  = safe(deviceId);
    const char* cid  = safe(caseId);
    const char* opn  = safe(operator_);
    const char* txt  = safe(content);

    // ---- 3. 组装日志行 --------------------------------------------------------
    // 基础格式:
    //   [时间] [级别] [Mod:模块] [Op:操作] [Dev:xxx] [Case:xxx] [Opr:xxx] [Content:内容]
    // Dev/Case/Opr/Content 字段只有非空时才出现。Mod/Op 如果调用方没有传值，
    // 当前也会省略，避免生成没有实际信息的空标签。
    std::string line;
    line.reserve(256);
    line += "[";
    line += ts;
    line += "] [";
    line += LevelName(level);
    line += "]";
    if (mod[0]) {
        line += " [Mod:";
        line += mod;
        line += "]";
    }
    if (op[0]) {
        line += " [Op:";
        line += op;
        line += "]";
    }
    if (dev[0]) {
        line += " [Dev:";
        line += dev;
        line += "]";
    }
    if (cid[0]) {
        line += " [Case:";
        line += cid;
        line += "]";
    }
    if (opn[0]) {
        line += " [Opr:";
        line += opn;
        line += "]";
    }
    if (txt[0]) {
        // 日志正文使用 Content，而不是历史版本里的 Txt。
        // 这里属于日志格式契约，不能由业务模块自行更改。
        line += " [Content:";
        line += txt;
        line += "]";
    }

    return line;
}

} // namespace LogFormat
