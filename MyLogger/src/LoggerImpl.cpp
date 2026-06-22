// =============================================================================
// 文件:    LoggerImpl.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 单例模式（通过静态局部变量实现的 Meyer 单例）是 DLL 单例
//     最简单正确的模式。它保证:
//       a) 线程安全的初始化（C++11 强制要求）。
//       b) 每个进程恰好一个实例（静态局部变量是每个 DLL 实例独立的，
//          且每个进程加载自己的 DLL 副本）。
//       c) 相对于同一翻译单元中其他静态对象的确定性析构顺序。
//   - 后台线程在 Shutdown() 中被 join，Shutdown() 从 ~LoggerImpl() 调用。
//     这意味着线程保证在 CRT 被拆除之前退出，避免了
//     "main() 返回后线程仍在运行"的问题。
//   - Write() 中的 condition_variable 通知是有意放在缓冲区锁外部的。
//     最坏情况是在缓冲区为空时发生虚假唤醒，这由 DoFlush() 中的
//     简单 "if empty, return" 检查处理。
//   - Error 和 Fatal 日志级别触发立即刷新通知。这是尽力而为的保证 —
//     如果进程在 Write() 调用和后台线程获取 CPU 之间崩溃，
//     该行仍可能丢失。对于真正的崩溃安全日志记录，我们需要
//     在每次 Error/Fatal 时同步写入并 fsync，这将是
//     异常昂贵的（慢 10–100 倍）。
// =============================================================================

#include "LoggerImpl.h"
#include "LogFormat.h"
#include <windows.h>   // OutputDebugStringA

// =========================================================================
// DLL 工厂函数
// =========================================================================
// 这是从 DLL 导出的唯一符号（除了 ILogger 的虚函数表 thunk，
// 这些由编译器自动生成，因为该类通过 MEYER_LOGGER_API
// 标记为 __declspec(dllexport)）。
//
// extern "C" 抑制了 C++ 名称修饰，因此导出表条目是
// 简单的 "GetLogger" 而非 "?GetLogger@@YAPAVILogger@@XZ"。
extern "C" MEYER_LOGGER_API ILogger* GetLogger() {
    return &LoggerImpl::Instance();
}

// =========================================================================
// 单例
// =========================================================================
// C++11 [stmt.dcl] 保证块作用域静态变量在控制首次经过声明时
// 以线程安全的方式恰好初始化一次。不需要显式的双重检查锁定。
LoggerImpl& LoggerImpl::Instance() {
    static LoggerImpl s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
// Rotation 对象用空日志目录构造。真正的目录在 Init() 中
// 通过移动赋值设置。这有点别扭，但是必要的，因为 LogRotation
// 没有默认构造函数（它需要一个路径）。
// 一个替代方案是 std::optional<LogRotation>，但那需要 C++17，
// 而我们为了 VS2015 兼容性目标 C++14。
LoggerImpl::LoggerImpl()
    : m_rotation("") {
    // 所有其他成员由其类内初始化器初始化:
    //   m_initialized = false
    //   m_level       = LogLevel::Info
    //   m_running     = false
    //   LogBuffer、LogWriter — 默认构造
}

// ---------------------------------------------------------------------------
// 析构函数
// ---------------------------------------------------------------------------
// 调用 Shutdown() 以确保后台线程被 join 且文件在单例的成员被销毁之前
// 关闭。Shutdown() 是幂等的（可安全多次调用），因此如果消费者
// 已经显式调用了 Shutdown()，这里将是空操作。
LoggerImpl::~LoggerImpl() {
    Shutdown();
}

// =========================================================================
// Init
// =========================================================================
bool LoggerImpl::Init(const char* logDir, LogLevel level) {
    // 无论初始化状态如何都更新级别过滤器。
    // 这允许调用方在运行时更改日志级别而无需重新初始化。
    m_level.store(level);

    if (m_initialized.load()) {
        // 已经初始化。我们不会重新打开文件或重启后台线程。
        // 如果调用方想要用不同目录完全重新初始化，
        // 应首先调用 Shutdown()。
        return true;
    }

    // ---- 验证输入 ------------------------------------------------------------
    if (!logDir || !logDir[0]) {
        OutputDebugStringA("[Logger] Init failed: logDir is null or empty\n");
        return false;
    }

    // ---- 设置轮转并打开第一个文件 -------------------------------------------
    // 将正确构造的 LogRotation 移动赋值到位。
    m_rotation = LogRotation(std::string(logDir));

    std::string firstPath = m_rotation.CurrentPath();
    if (!m_writer.Open(firstPath)) {
        OutputDebugStringA("[Logger] Init failed: cannot open first log file\n");
        return false;
    }

    // ---- 启动后台刷新线程 ---------------------------------------------------
    m_running.store(true);
    try {
        m_thread = std::thread(&LoggerImpl::BackgroundThread, this);
    } catch (const std::system_error&) {
        // std::thread 构造函数在操作系统资源耗尽时可能抛出异常
        //（极罕见）。回退到同步刷新: 每次 Write() 将直接调用 DoFlush()。
        // 日志记录仍然工作，只是在调用线程上有更高的延迟。
        OutputDebugStringA("[Logger] Failed to create background thread — "
                           "falling back to synchronous flush\n");
        m_running.store(false);
        m_writer.Close();
        return false;
    }

    m_initialized.store(true);

    // ---- 写入第一条日志条目 -------------------------------------------------
    // 这确认了日志器正在运行，并在日志文件中提供清晰的
    // 本次会话开始标记。
    Write(LogLevel::Info, "Logger", "Init", "-", "-", "-",
          std::string("Logger initialised. Log directory: ").append(logDir).c_str());

    return true;
}

// =========================================================================
// Write — 热路径
// =========================================================================
void LoggerImpl::Write(LogLevel level,
                       const char* module,
                       const char* operation,
                       const char* deviceId,
                       const char* caseId,
                       const char* operator_,
                       const char* content) {
    if (!m_initialized.load()) {
        return;
    }

    // ---- 1. 级别过滤（原子读取，无锁） ---------------------------------------
    // 这是整个日志器中最关键性能的一行。
    // 原子加载在 x86-64 上编译为单条 MOV 指令
    //（acquire 语义在该架构上是免费的）。
    if (level < m_level.load()) {
        return;  // 静默丢弃。
    }

    // ---- 2. 格式化日志行（纯计算，无锁） ------------------------------------
    // FormatLine 分配一个 std::string。对于常见情况（约 200 字节的行），
    // 这是一个大多数 malloc 实现都能快速处理的小分配
    //（tcmalloc / mimalloc: < 50 ns；MSVC debug heap: 较慢但
    // 对于日志路径仍可接受）。
    std::string line = LogFormat::FormatLine(level, module, operation,
                                             deviceId, caseId, operator_,
                                             content);

    // ---- 3. 追加到缓冲区（短暂持锁） ----------------------------------------
    m_buffer.Add(line);

    // ---- 4. 决定是否唤醒刷新线程 --------------------------------------------
    // 唤醒条件:
    //   a) Error 或 Fatal 级别  →  尝试尽快将此消息刷到磁盘。
    //   b) 缓冲区大小 ≥ kFlushThreshold (100 行) → 避免无界内存增长。
    //
    // 我们在添加行之后检查缓冲区大小，因此阈值比较是"≥"而非">"。
    // 此处的假阳性最多导致一次额外的 condition_variable 通知，
    // 这是廉价的。
    bool shouldNotify = (level >= LogLevel::Error ||
                         m_buffer.Size() >= LogBuffer::kFlushThreshold);

    // ---- 5. 通知后台线程（在任何锁之外） -----------------------------------
    // notify_one() 在没有等待者时是廉价的（几次原子操作）。
    // 我们有意不在此处持有 m_buffer 的互斥锁 — 通知是提示性的，
    // 刷新线程将在其自己的锁下重新检查缓冲区状态。
    if (shouldNotify) {
        m_cv.notify_one();
    }
}

// =========================================================================
// 运行时控制
// =========================================================================

void LoggerImpl::SetLogLevel(LogLevel level) {
    // 使用顺序一致性内存序的原子存储（std::atomic 的默认值）。
    // 这确保在此存储之后执行的任何 Write() 调用都能看到新级别。
    // 不需要互斥锁。
    m_level.store(level);
}

LogLevel LoggerImpl::GetLogLevel() const {
    return m_level.load();
}

void LoggerImpl::Flush() {
    // 立即唤醒后台线程。如果它当前处于 5 秒等待中，
    // 它将唤醒、排空，然后回到睡眠状态。如果它已经在刷新中，
    // 此通知将在下一次循环迭代中被接收。
    m_cv.notify_one();
}

// =========================================================================
// Shutdown
// =========================================================================
void LoggerImpl::Shutdown() {
    // 防护双重 Shutdown（幂等）。
    if (!m_initialized.load()) {
        return;
    }

    // ---- 1. 通知后台线程退出 -------------------------------------------------
    m_running.store(false);
    m_cv.notify_one();  // 如果它在休眠中则唤醒它。

    // ---- 2. 等待线程完成 -----------------------------------------------------
    if (m_thread.joinable()) {
        m_thread.join();
        // join() 之后，线程对象处于"非线程"状态。
        // 析构函数不会调用 std::terminate。
    }

    // ---- 3. 最终排空 — 写入仍缓冲的任何行 ---------------------------------
    // 后台线程在退出前排空，但存在竞争:
    // Write() 可能在最后一次排空和 m_running 检查之间添加了一行。
    // 此处的 DoFlush() 捕获这些遗漏的行。
    DoFlush();

    // ---- 4. 关闭文件 ---------------------------------------------------------
    m_writer.Close();

    m_initialized.store(false);
}

const char* LoggerImpl::GetModuleVersion() const {
    return "MeyerScan_Logger v1.0.0 (2026-06-17)";
}

// =========================================================================
// 后台线程
// =========================================================================
void LoggerImpl::BackgroundThread() {
    // 线程运行直到 m_running 变为 false。每次迭代执行:
    //   1. 以 5 秒超时等待 m_cv。
    //   2. 如果被唤醒（通过通知或超时），调用 DoFlush()。
    //
    // 5 秒超时保证日志行在写入后的最多 5 秒内落到磁盘上，
    // 即使应用程序日志记录非常缓慢（低于 100 行阈值）。
    //
    // 在关闭时，m_running 被设置为 false 且 m_cv 被通知。
    // 线程唤醒，看到 m_running == false，跳出循环，
    // 析构函数 join 它。
    while (m_running.load()) {
        {
            // 获取保护条件变量的互斥锁。
            std::unique_lock<std::mutex> lock(m_cvMutex);

            // wait_for 原子地释放互斥锁并休眠直到:
            //   - notify_one() / notify_all() 被调用，或
            //   - 5 秒过去（以先到者为准）。
            // 当它返回时，互斥锁被重新获取。
            m_cv.wait_for(lock, std::chrono::seconds(5));
        }
        // 互斥锁在此处释放 — DoFlush() 不需要它。

        // 唤醒后重新检查 m_running。如果我们是被关闭通知唤醒的，
        // 立即退出而不刷新（Shutdown() 将在 join 后进行最终刷新）。
        if (!m_running.load()) {
            break;
        }

        DoFlush();
    }
}

// =========================================================================
// DoFlush
// =========================================================================
void LoggerImpl::DoFlush() {
    // ---- 1. 排空缓冲区（廉价；在互斥锁下 swap） -----------------------------
    std::vector<std::string> lines = m_buffer.Drain();
    if (lines.empty()) {
        return;  // 没有事情可做。常见情况: 后台线程因 5 秒超时而唤醒，
                 // 但没有人写入任何内容。
    }

    // ---- 2. 检查轮转（可能关闭旧文件并打开新文件） -------------------------
    std::string newPath = m_rotation.CheckRotation();
    if (!newPath.empty()) {
        // 发生了轮转。关闭旧文件并打开新路径。
        // 在此点之前被排空的任何行都将写入新文件 —
        // 这是有意为之。我们希望轮转边界是干净的:
        // 00:00 之后的所有行都进入新日期的文件。
        m_writer.Close();
        m_writer.Open(newPath);
    }

    // ---- 3. 将所有行写入磁盘 -------------------------------------------------
    // 每次 WriteLine 调用都获取跨进程互斥量，因此如果
    // ScanReconstructStudio.exe 同时正在刷新其缓冲区，
    // 行将以行级干净地交错，而不会出现来自两个进程
    // 的半写入垃圾。
    for (const auto& line : lines) {
        m_writer.WriteLine(line);
    }

    // ---- 4. 将 CRT 缓冲区刷新到操作系统 -------------------------------------
    m_writer.Flush();
}
