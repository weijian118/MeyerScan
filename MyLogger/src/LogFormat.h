// =============================================================================
// 文件:    LogFormat.h
// 模块:    MeyerScan_Logger.dll（内部 — 不对外暴露）
//
// 用途:
//   将原始日志字段转换为一条紧凑格式的 UTF-8 行，准备写入磁盘。
//   所有格式决策（时间戳样式、字段输出顺序、空字段是否省略）
//   都集中在这个翻译单元中，这样可以在一个地方更改日志格式，
//   而无需触及任何调用方。
//
// 格式规范:
//   [2026-06-24 08:42:14.893] [INFO] [Mod:MeyerScan_Database] [Op:SetDatabaseType] [Content:Database type switched]
//   [2026-06-24 08:42:15.100] [INFO] [Mod:CaseUI] [Op:OpenOrder] [Case:C001] [Content:order opened]
//
//   固定字段: 时间戳、级别、模块、操作、内容。
//   可选字段: [Dev:xxx]、[Case:xxx]、[Opr:xxx]，只有传入非空字符串时才输出。
//
//   - 时间戳使用 Windows SYSTEMTIME 结构体（本地时间，非 UTC）。
//     这是有意为之 —— 操作员和现场工程师在生成日志的机器上阅读这些日志。
//   - 调用方不得包含尾随换行符；LogWriter::WriteLine()
//     在行尾追加 '\n'，使每次系统级写入恰好是一条完整的日志条目。
//   - 空字段直接省略，不输出 "-" 占位。这样日志在普通文本编辑器中
//     更清爽，也避免出现大量无意义的 [Dev:-] / [Case:-] / [Op:-]。
//   - 如果 content 字段极长，行将被截断到 2047 字节。
//     这是安全限制而非功能特性 —— 过长的日志消息
//     应由调用方自行拆分。
// =============================================================================

#pragma once
#include "Logger.h"   // LogLevel
#include <string>

namespace LogFormat {

// ---------------------------------------------------------------------------
// FormatLine — 生成一条完整的日志行（不含尾随 '\n'）
// ---------------------------------------------------------------------------
// 所有字符串参数均为以 null 结尾的 UTF-8。nullptr 被视为等同于
// ""（空字符串）。
//
// 线程安全:  纯函数。无共享状态。可从任意线程调用，无需同步。
//
// 内存:      按值返回 std::string（通常 ≤ 300 字节）。
//            移动语义使这对调用方来说成本很低。
std::string FormatLine(LogLevel level,
                       const char* module,
                       const char* operation,
                       const char* deviceId,
                       const char* caseId,
                       const char* operator_,
                       const char* content);

// ---------------------------------------------------------------------------
// LevelName — 人类可读的级别字符串（不做固定宽度补空格）
// ---------------------------------------------------------------------------
// 返回: "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
// 返回的指针指向静态常量；绝不要释放它。
const char* LevelName(LogLevel level);

} // namespace LogFormat
