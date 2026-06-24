// =============================================================================
// 文件:    LogWriter.h
// 模块:    MeyerScan_Logger.dll（内部）
//
// 用途:
//   通过单个 Windows 命名互斥量串行化所有磁盘写入，
//   以便多个模块、多个进程可以安全地追加到同一个日志文件。
//
//   当前实现不长期持有 FILE* 句柄：每写一条日志都会打开文件、追加一行、
//   刷新到磁盘并关闭文件。这样后台可以移动/删除日志文件，现场收集日志
//   也不会被 Logger 长时间占用句柄。
//
// 多进程架构:
//   ┌─────────────────┐     ┌──────────────────────┐
//   │  MeyerScan.exe  │     │ ScanReconstructStudio │
//   │  （主进程）      │     │ （扫描进程）           │
//   │       │         │     │       │               │
//   │   LogWriter     │     │   LogWriter           │
//   │       │         │     │       │               │
//   │  WaitForSingle   │     │  WaitForSingle        │
//   │  Object(mutex)   │     │  Object(mutex)        │
//   │       │         │     │       │               │
//   └───────┼─────────┘     └───────┼───────────────┘
//           │                       │
//           └───────────┬───────────┘
//                       │ 由命名互斥量串行化
//                       ▼
//              MeyerScan_20260612.log
//
// 为什么使用命名互斥量而非 LockFileEx？
//   - 命名互斥量是内核对象；如果进程在持有它时崩溃，
//     操作系统会自动释放它（句柄被关闭）。不会死锁。
//   - LockFileEx 与文件句柄绑定；如果一个进程关闭其句柄
//     而另一个进程正在等待，行为是未定义的。
//   - 命名互斥量跨用户会话工作（使用 Global\ 前缀），
//     这在扫描进程以服务账户运行时很重要。
//
// 为什么使用 "Global\\" 前缀？
//   - 不使用此前缀，互斥量在会话命名空间中创建，
//     对其他会话中的进程不可见（例如 服务 vs 交互式用户）。
//   - 如果 SeCreateGlobalPrivilege 不可用（在锁定严格的
//     医院 PC 上很少见），我们回退到 "Local\\"，
//     至少在同一会话内串行化。
// =============================================================================

#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

class LogWriter {
public:
    // 构造函数:  创建（或打开）命名互斥量。
    // 析构函数:  关闭互斥量句柄。
    LogWriter();
    ~LogWriter();

    // -------------------------------------------------------------------
    // WriteLine — 向指定文件追加一行 + '\n'
    // -------------------------------------------------------------------
    // path:   绝对路径，例如 "C:\...\logs\MeyerScan_20260612_001.log"
    //         如果父目录不存在，将通过 SHCreateDirectoryExW 递归创建。
    // 以 500 ms 超时获取跨进程互斥量。
    //   - 成功时: 打开文件、写入行、追加 '\n'、刷新、关闭、释放互斥量。
    //   - 超时时: 丢弃该行并调用 OutputDebugStringA，
    //     使失败在调试器中可见。
    //
    // 500 ms 超时是故意设置得很短。如果另一个进程卡住持有互斥量
    // 超过 500 ms，日志吞吐量已经是我们最不需要担心的问题 ——
    // 另一个进程很可能已挂起/崩溃。丢弃一行日志
    // 优于无限期阻塞调用线程。
    // 返回 true 表示本条日志已经成功写入并关闭句柄。
    bool WriteLine(const std::string& path, const std::string& line);

    // -------------------------------------------------------------------
    // Lock / Unlock — 保护“选文件 + 写入”这个完整临界区
    // -------------------------------------------------------------------
    // LoggerImpl 会先 Lock，再调用 LogRotation::PathForNextWrite，
    // 最后调用 WriteLineUnlocked。这样多个进程不会同时判断同一个文件大小。
    bool Lock(uint32_t timeoutMs = 500);
    void Unlock();

    // 调用方已经持有互斥量时使用，避免重复 Lock。
    bool WriteLineUnlocked(const std::string& path, const std::string& line);

private:
    HANDLE      m_mutex = nullptr;  // 命名互斥量 "Global\MeyerScan_Logger_Mutex"

    // 确保日志文件父目录存在。
    bool EnsureParentDirectory(const std::string& path) const;
};
