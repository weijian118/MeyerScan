// =============================================================================
// 文件:    LoggerImpl.h
// 模块:    MeyerScan_Logger.dll（内部 — 不对外暴露）
//
// 用途:
//   ILogger 接口的具体实现。拥有整个日志流水线:
//
//   Write() 调用
//     → 级别过滤（原子操作）
//     → LogFormat::FormatLine()
//     → LogBuffer::Add()
//     → （如果是 Error/Fatal 或达到阈值）唤醒后台线程
//
//   后台线程（每 5 秒或被唤醒时）:
//     → LogBuffer::Drain()
//     → LogRotation::CheckRotation()
//     → LogWriter::WriteLine() × N
//     → LogWriter::Flush()
//
// 单例理由:
//   进程级单例（LoggerImpl.cpp 中的静态局部变量）保证了
//   Init() 在每个进程中恰好被调用一次，并且 DLL 可以
//   安全地从静态初始化器中使用（C++11 的"魔法静态变量"
//   是线程安全的，并保证恰好构造一次）。
//
// 为什么析构函数调用 Shutdown():
//   单例在 DLL_PROCESS_DETACH 期间或静态析构时被销毁。
//   通过在 ~LoggerImpl() 中调用 Shutdown()，我们给后台线程
//   最后一次排空缓冲区的机会，在 CRT 本身被拆除之前。
//   在静态析构开始之后写入的行可能会丢失 ——
//   但这是任何日志库都固有的限制。
// =============================================================================

#pragma once
#include "Logger.h"
#include "LogBuffer.h"
#include "LogWriter.h"
#include "LogRotation.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class LoggerImpl : public ILogger {
public:
    // -------------------------------------------------------------------
    // 单例访问器（C++11 线程安全的静态局部变量）
    // -------------------------------------------------------------------
    static LoggerImpl& Instance();

    // -------------------------------------------------------------------
    // ILogger 实现 — 完整文档见 Logger.h
    // -------------------------------------------------------------------
    bool     Init(const char* logDir, LogLevel level) override;
    void     Write(LogLevel level,
                   const char* module, const char* operation,
                   const char* deviceId, const char* caseId,
                   const char* operator_, const char* content) override;
    void     SetLogLevel(LogLevel level) override;
    LogLevel GetLogLevel() const override;
    void     Flush() override;
    void     Shutdown() override;

private:
    // 只有单例可以构造和析构。
    LoggerImpl();
    ~LoggerImpl();

    // 不可复制，不可移动。
    LoggerImpl(const LoggerImpl&) = delete;
    LoggerImpl& operator=(const LoggerImpl&) = delete;

    // -------------------------------------------------------------------
    // BackgroundThread — 刷新循环
    // -------------------------------------------------------------------
    // 在专用的 std::thread 上运行。以 5 秒超时等待 m_cv。
    // 每次唤醒（信号或超时）时调用 DoFlush()。
    // 当 m_running 变为 false 时退出。
    void BackgroundThread();

    // -------------------------------------------------------------------
    // DoFlush — 排空缓冲区 → 检查轮转 → 写入 → 同步
    // -------------------------------------------------------------------
    // 从 BackgroundThread 和 Shutdown()（最终排空）中调用。
    // 如果缓冲区为空，立即返回（无磁盘 I/O）。
    void DoFlush();

    // -------------------------------------------------------------------
    // 状态
    // -------------------------------------------------------------------

    // m_initialized:  首次成功的 Init() 调用后为 true。
    // 防止文件句柄和线程的重复初始化。
    std::atomic<bool>    m_initialized{false};

    // m_level:  当前过滤级别。原子变量，以便 Write() 可以在
    // 不获取任何互斥锁的情况下读取 —— 这是整个日志流水线中
    // 最热的代码路径。
    std::atomic<LogLevel> m_level{LogLevel::Info};

    // 流水线组件，按数据流顺序排列。
    LogBuffer   m_buffer;      // 线程安全的行累积器
    LogWriter   m_writer;      // 文件 I/O + 跨进程互斥量
    LogRotation m_rotation;    // 日期/大小跟踪；在 Init() 中更新

    // 后台刷新线程。
    //   m_thread:    线程对象（从 Init 之后到 Shutdown 之前可 join）
    //   m_running:   在 Init 中设置为 true，在 Shutdown 中设置为 false
    //   m_cvMutex:   与 m_cv 配对，用于条件变量
    //   m_cv:        当缓冲区达到阈值或 Error/Fatal 时被通知
    std::thread             m_thread;
    std::atomic<bool>       m_running{false};
    std::mutex              m_cvMutex;
    std::condition_variable m_cv;
};
