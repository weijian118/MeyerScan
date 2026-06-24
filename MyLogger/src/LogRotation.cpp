// =============================================================================
// 文件:    LogRotation.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 轮转检查是 O(1) — 一次 stat() 调用和一次字符串比较。
//     它在 Write() 路径上运行，但只有文件大小查询和字符串拼接，
//     对当前软件的日志量来说可控。
//   - 文件名由日期和可选尾部序号确定性生成。每天优先只写一个
//     MeyerScan_YYYYMMDD.log；超过大小限制后才生成
//     MeyerScan_YYYYMMDD_001.log、_002.log 等分卷。
//   - 本类不打开文件，只做路径选择和文件大小查询。真正的打开、写入、
//     FlushFileBuffers 和关闭由 LogWriter 在跨进程互斥量保护下完成。
// =============================================================================

#include "LogRotation.h"
#include <windows.h>   // GetLocalTime, SYSTEMTIME
#include <cstdio>      // snprintf
#include <sys/stat.h>  // _stati64

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
LogRotation::LogRotation(const std::string& logDir)
    : m_logDir(logDir)
    , m_currentDate(Today())
    , m_currentIndex(0)
    , m_maxFileSize(kMaxFileSize) {

    // 规范化目录路径:  确保它以恰好一个反斜杠结尾，
    // 以便 BuildPath 可以简单地拼接。
    if (!m_logDir.empty() && m_logDir.back() != '\\' && m_logDir.back() != '/') {
        m_logDir += '\\';
    }
}

// ---------------------------------------------------------------------------
// Today — 本地日期，格式 "YYYYMMDD"
// ---------------------------------------------------------------------------
// 使用 GetLocalTime 以与 LogFormat 保持一致（相同的时钟源）。
// SYSTEMTIME → 字符串的转换很简单，不需要
// std::chrono + std::put_time 的复杂性（后者在 MSVC 上
// 由于本地化互斥锁竞争而有已知的性能问题）。
std::string LogRotation::Today() const {
    // SYSTEMTIME 是 Windows API 的本地时间结构。
    // 日志文件按本地日期分文件，便于现场人员按当天日期查找。
    SYSTEMTIME st;
    GetLocalTime(&st);
    // 16 字节足够保存 8 位日期加结束符，例如 "20260624\0"。
    char buf[16];
    // 月/日都用 2 位补零，确保文件名按字符串排序也等于按日期排序。
    snprintf(buf, sizeof(buf), "%04d%02d%02d",
             st.wYear, st.wMonth, st.wDay);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// BuildPath
// ---------------------------------------------------------------------------
// 生成:
//   index == 0: "{m_logDir}MeyerScan_{date}.log"
//   index >  0: "{m_logDir}MeyerScan_{date}_{index:03d}.log"
// 示例:
//   "C:\...\logs\MeyerScan_20260612.log"
//   "C:\...\logs\MeyerScan_20260612_001.log"
//
// 缓冲区使用 MAX_PATH (260)。我们的路径远低于此限制，因为:
//   - 日志目录通常是 "C:\ProgramData\MeyerScan\logs\"（约 35 字符）
//   - 文件名是 "MeyerScan_YYYYMMDD.log" 或
//     "MeyerScan_YYYYMMDD_NNN.log"（约 25-30 字符）
//   → 总计约 65 字符。
std::string LogRotation::BuildPath(const std::string& date, int index) const {
    // MAX_PATH 来自 Windows 传统路径长度限制。
    // 当前日志目录和文件名都很短，因此固定栈缓冲足够且容易审查。
    char buf[MAX_PATH];
    if (index <= 0) {
        // 当天第一个日志文件不带尾部序号，满足“每天默认一个文件”的直觉。
        snprintf(buf, sizeof(buf), "%sMeyerScan_%s.log",
                 m_logDir.c_str(), date.c_str());
    } else {
        // 超过大小限制后才增加尾部序号，序号 3 位补零便于自然排序。
        snprintf(buf, sizeof(buf), "%sMeyerScan_%s_%03d.log",
                 m_logDir.c_str(), date.c_str(), index);
    }
    // snprintf 会写入 '\0'；std::string 会复制直到结束符。
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// CurrentPath
// ---------------------------------------------------------------------------
std::string LogRotation::CurrentPath() const {
    return BuildPath(m_currentDate, m_currentIndex);
}

// ---------------------------------------------------------------------------
// PathForNextWrite
// ---------------------------------------------------------------------------
// 算法:
//   1. 获取今天的日期字符串。
//   2. 如果与 m_currentDate 不同 → 日期已翻转 → 重置为当天主文件。
//   3. 查看当前文件大小；如果写入 incomingBytes 后超过上限，则递增
//      m_currentIndex 并继续检查下一个分卷。
//   4. 返回最终选中的路径。
//
// 边界情况 — stat() 失败（文件被外部删除）:
//   _stati64 返回非零。我们将其视为"文件不存在或不可访问"，
//   不进行轮转。下一次写入将通过 LogWriter::Open() 的 "a" 模式
//   重新创建文件。这是有意为之 —— 此处轮转将创建一个新文件名，
//   而旧文件将成为孤儿文件。
std::string LogRotation::PathForNextWrite(uint64_t incomingBytes) {
    const std::string today = Today();

    // ---- 触发器 1: 日期已变更 ------------------------------------------------
    if (today != m_currentDate) {
        m_currentDate  = today;
        m_currentIndex = 0;     // 新一天重新从 MeyerScan_YYYYMMDD.log 开始。
    }

    // ---- 触发器 2: 文件过大 --------------------------------------------------
    // while 用于处理启动时已有日志文件已经超过上限，或者某个分卷也已满的情况。
    // VS2015 /W4 会把 while(true) 报为 C4127 常量条件。
    // for(;;) 是同样语义的无限循环，但不会触发该警告。
    for (;;) {
        const std::string path = CurrentPath();
        const uint64_t currentSize = FileSize(path);

        // 当前文件为空或写入后仍不超过限制，就继续写入当前文件。
        // incomingBytes 大于最大文件限制时也允许写入当前文件，避免单条超长日志死循环。
        if (currentSize == 0 ||
            currentSize + incomingBytes <= m_maxFileSize ||
            incomingBytes >= m_maxFileSize ||
            m_currentIndex >= 999) {
            return path;
        }

        // 当前文件写入本条后会超过限制，切到下一个分卷。
        ++m_currentIndex;
    }
}

// ---------------------------------------------------------------------------
// FileSize
// ---------------------------------------------------------------------------
uint64_t LogRotation::FileSize(const std::string& path) const {
    struct _stati64 st;
    if (_stati64(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

// ---------------------------------------------------------------------------
// SetMaxFileSize（用于测试）
// ---------------------------------------------------------------------------
void LogRotation::SetMaxFileSize(uint64_t bytes) {
    m_maxFileSize = bytes;
}
