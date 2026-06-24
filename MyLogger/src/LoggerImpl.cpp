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
//   - 当前实现不再使用后台缓冲线程。每条日志在 Write() 内完成
//     选文件、追加、FlushFileBuffers 和 CloseHandle。
//   - 这样做会牺牲一部分吞吐量，但日志量对当前口扫软件不是瓶颈；
//     换来的是更强的落盘确定性，以及后台可以移动/删除日志文件。
// =============================================================================

#include "LoggerImpl.h"
#include "LogFormat.h"
#include <windows.h>   // OutputDebugStringA

namespace {
namespace ModuleInfo {
// 模块名用于 Logger 自身初始化日志，必须与工程中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_Logger";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_Logger v1.1.0 (2026-06-24)";
}
}

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

LoggerImpl::LoggerImpl()
    : m_rotation("") {
    // 所有其他成员由其类内初始化器初始化:
    //   m_initialized = false
    //   m_level       = LogLevel::Info
    //   LogWriter     — 默认构造，创建跨进程互斥量
}

// ---------------------------------------------------------------------------
// 析构函数
// ---------------------------------------------------------------------------
// 调用 Shutdown() 以清理初始化状态。当前 Logger 不长期持有文件句柄，
// 因此析构阶段没有后台线程或文件句柄需要等待。
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

    // ---- 设置轮转状态 --------------------------------------------------------
    // 将正确构造的 LogRotation 移动赋值到位。文件不会在 Init 中长期打开，
    // 每条日志写入时才打开并立即关闭。
    m_rotation = LogRotation(std::string(logDir));

    // ---- 写入第一条日志条目 -------------------------------------------------
    // 这确认了日志器正在运行，并在日志文件中提供清晰的
    // 本次会话开始标记。
    const std::string initContent = std::string("Logger initialized. Log directory: ").append(logDir);
    const std::string initLine = LogFormat::FormatLine(LogLevel::Info,
                                                       ModuleInfo::Name,
                                                       "Init",
                                                       "",
                                                       "",
                                                       "",
                                                       initContent.c_str());
    if (!m_writer.Lock()) {
        OutputDebugStringA("[Logger] Init failed: cannot lock log writer\n");
        return false;
    }
    const std::string initPath = m_rotation.PathForNextWrite(
        static_cast<uint64_t>(initLine.size() + 2));
    const bool initWritten = m_writer.WriteLineUnlocked(initPath, initLine);
    m_writer.Unlock();
    if (!initWritten) {
        OutputDebugStringA("[Logger] Init failed: cannot write first log line\n");
        return false;
    }

    m_initialized.store(true);

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
    // FormatLine 会省略空 deviceId/caseId/operator 字段，降低日志视觉噪音。
    std::string line = LogFormat::FormatLine(level, module, operation,
                                             deviceId, caseId, operator_,
                                             content);

    // ---- 3. 跨进程临界区：选文件 + 写入 --------------------------------------
    // 轮转判断必须和写入处于同一把跨进程锁里，否则两个进程可能同时判断
    // 当前文件未超限并一起写穿大小上限。
    if (!m_writer.Lock()) {
        return;
    }

    // line.size() + 2 包含 CRLF。日志文件大小以实际落盘字节数判断。
    const std::string path = m_rotation.PathForNextWrite(
        static_cast<uint64_t>(line.size() + 2));
    m_writer.WriteLineUnlocked(path, line);
    m_writer.Unlock();
}

// =========================================================================
// 运行时控制
// =========================================================================

// 修改日志级别过滤器。
// 这是热路径相关配置，使用原子变量避免给所有 Write() 调用增加锁开销。
void LoggerImpl::SetLogLevel(LogLevel level) {
    // 使用顺序一致性内存序的原子存储（std::atomic 的默认值）。
    // 这确保在此存储之后执行的任何 Write() 调用都能看到新级别。
    // 不需要互斥锁。
    m_level.store(level);
}

// 获取当前日志级别过滤器。
// 原子读取无锁，适合调试面板或测试代码随时查询。
LogLevel LoggerImpl::GetLogLevel() const {
    return m_level.load();
}

// 请求尽快刷新日志。
// 当前 Logger 每条日志都同步写入、刷盘并关闭句柄，因此 Flush 是幂等空操作。
void LoggerImpl::Flush() {
    // 保留接口是为了兼容现有模块；调用 Flush 不再触发额外磁盘动作。
}

// =========================================================================
// Shutdown
// =========================================================================
// 关闭日志模块。
// 该函数是幂等的：重复调用不会重复 join 线程或关闭文件，适合模块退出阶段防御性调用。
void LoggerImpl::Shutdown() {
    // 防护双重 Shutdown（幂等）。
    if (!m_initialized.load()) {
        return;
    }

    // 当前实现没有后台线程和长期文件句柄，Shutdown 只关闭可写状态。
    // 之后 Write() 会变成空操作，直到下一次 Init()。
    m_initialized.store(false);
}

// 返回日志模块版本字符串。
// 使用字符串字面量，避免跨 DLL 边界返回 std::string。
const char* LoggerImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}
