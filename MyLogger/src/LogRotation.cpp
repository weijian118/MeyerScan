// =============================================================================
// 文件:    LogRotation.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 轮转检查是 O(1) — 一次 stat() 调用和一次字符串比较。
//     它在后台线程上运行（不在热的 Write() 路径上），所以即使
//     开销更大也不会影响应用延迟。
//   - 文件名由日期+索引确定性生成，使得从日志中的时间戳
//     定位特定日志文件变得很简单。
//   - 当索引溢出（999 → 001）时，当天最早的文件会被覆盖，
//     因为写入者以 "a"（追加）模式打开。要防止这种情况，
//     我们需要不同的策略（例如失败或先删除再创建）。
//     999 × 10 MiB = ~10 GiB/天 的上限对于每天最多产生
//     几百 MiB 日志的医疗设备来说是足够的。
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
    , m_currentIndex(1) {

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
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d%02d%02d",
             st.wYear, st.wMonth, st.wDay);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// BuildPath
// ---------------------------------------------------------------------------
// 生成: "{m_logDir}MeyerScan_{date}_{index:03d}.log"
// 示例:  "C:\...\logs\MeyerScan_20260612_001.log"
//
// 缓冲区使用 MAX_PATH (260)。我们的路径远低于此限制，因为:
//   - 日志目录通常是 "C:\ProgramData\MeyerScan\logs\"（约 35 字符）
//   - 文件名是 "MeyerScan_YYYYMMDD_NNN.log"（约 30 字符）
//   → 总计约 65 字符。
std::string LogRotation::BuildPath(const std::string& date, int index) const {
    char buf[MAX_PATH];
    snprintf(buf, sizeof(buf), "%sMeyerScan_%s_%03d.log",
             m_logDir.c_str(), date.c_str(), index);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// CurrentPath
// ---------------------------------------------------------------------------
std::string LogRotation::CurrentPath() const {
    return BuildPath(m_currentDate, m_currentIndex);
}

// ---------------------------------------------------------------------------
// CheckRotation
// ---------------------------------------------------------------------------
// 算法:
//   1. 获取今天的日期字符串。
//   2. 如果与 m_currentDate 不同 → 日期已翻转 → 重置索引为 1
//      并返回新路径。
//   3. 否则，_stati64 当前文件。如果大小 ≥ m_maxFileSize → 递增
//      索引（在 1000 处回绕）并返回新路径。
//   4. 否则，返回空字符串（不轮转）。
//
// 边界情况 — stat() 失败（文件被外部删除）:
//   _stati64 返回非零。我们将其视为"文件不存在或不可访问"，
//   不进行轮转。下一次写入将通过 LogWriter::Open() 的 "a" 模式
//   重新创建文件。这是有意为之 —— 此处轮转将创建一个新文件名，
//   而旧文件将成为孤儿文件。
std::string LogRotation::CheckRotation() {
    const std::string today = Today();
    bool needRotation = false;

    // ---- 触发器 1: 日期已变更 ------------------------------------------------
    if (today != m_currentDate) {
        m_currentDate  = today;
        m_currentIndex = 1;     // 重置为新一天的第一个文件。
        needRotation   = true;
    }

    // ---- 触发器 2: 文件过大 --------------------------------------------------
    // 仅在日期尚未触发轮转时检查文件大小
    //（新日期的文件尚不存在，stat 无论如何都会失败）。
    if (!needRotation) {
        std::string path = CurrentPath();
        struct _stati64 st;
        if (_stati64(path.c_str(), &st) == 0) {
            if (static_cast<uint64_t>(st.st_size) >= m_maxFileSize) {
                ++m_currentIndex;
                // 在 1000 处回绕到 001（当天最早的文件
                // 被静默覆盖）。
                if (m_currentIndex > 999) {
                    m_currentIndex = 1;
                }
                needRotation = true;
            }
        }
        // 如果 _stati64 失败（文件被删除、权限错误……），
        // 我们有意不做任何事。下一次写入发生时，
        // 文件将被原地重新创建。
    }

    // 如果发生了轮转则返回新路径，否则返回空字符串。
    return needRotation ? CurrentPath() : std::string();
}

// ---------------------------------------------------------------------------
// SetMaxFileSize（用于测试）
// ---------------------------------------------------------------------------
void LogRotation::SetMaxFileSize(uint64_t bytes) {
    m_maxFileSize = bytes;
}
