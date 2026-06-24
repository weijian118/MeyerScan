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
//     → 获取跨进程互斥量
//     → LogRotation::PathForNextWrite()
//     → LogWriter::WriteLineUnlocked()
//     → FlushFileBuffers + CloseHandle
//
// 单例理由:
//   进程级单例（LoggerImpl.cpp 中的静态局部变量）保证了
//   Init() 在每个进程中恰好被调用一次，并且 DLL 可以
//   安全地从静态初始化器中使用（C++11 的"魔法静态变量"
//   是线程安全的，并保证恰好构造一次）。
//
// 为什么同步逐条写入:
//   当前产品更看重日志完整性、现场可维护性和后台可移动/删除日志文件，
//   因此 Logger 不长期持有文件句柄，也不使用后台缓冲线程。
// =============================================================================

#pragma once
#include "Logger.h"
#include "LogWriter.h"
#include "LogRotation.h"
#include <atomic>

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
    const char* GetModuleVersion() const override;

private:
    // 只有单例可以构造和析构。
    LoggerImpl();
    ~LoggerImpl();

    // 不可复制，不可移动。
    LoggerImpl(const LoggerImpl&) = delete;
    LoggerImpl& operator=(const LoggerImpl&) = delete;

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
    LogWriter   m_writer;      // 跨进程互斥量 + 逐条打开/写入/关闭
    LogRotation m_rotation;    // 日期/大小跟踪；在 Init() 中更新
};
