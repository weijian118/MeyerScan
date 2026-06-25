// =============================================================================
// 文件:    Logger.h
// 模块:    MeyerScan_Logger.dll (MeyerScan 口扫软件 — 日志基础设施)
//
// 用途:
//   这是 Logger 模块的唯一公开头文件。每个模块（主 EXE、所有 DLL、
//   ScanReconstructStudio.exe）都包含此头文件以写入结构化的、带时间戳的
//   日志条目。
//
// 设计决策:
//   1. Logger.dll 本体保持零 Qt 依赖。
//      DLL 本身只链接 MSVC 运行时 + kernel32 + shell32。
//      这保证了 Logger 可以在 Qt 初始化*之前*加载，
//      且非 Qt 模块（算法 DLL 等）无需引入 Qt5Core.dll（约 5 MiB）
//      即可使用它。Qt 模块可使用本头文件中的 QString 内联便捷接口，
//      但真正跨 DLL 的 ABI 仍然是 const char*。
//
//   2. 主要 ABI 使用 const char*（UTF-8）。
//      const char* 是唯一一种可以安全跨越 DLL 边界而不会触发
//      std::string ABI 问题（不同 STL 实现、debug/release 堆不匹配）
//      的类型。纯虚函数 Write() 只接受 const char*。
//
//   3. 便捷重载是内联的（编译到调用方中）。
//      - std::string  重载: 始终可用，零开销。
//      - QString      重载: 仅当 <QString> 在此头文件之前被包含时才启用。
//        两个重载都委托给 const char* 版本，因此 DLL 边界
//        永远不会传递复杂类型。
//
//   4. 每条日志同步落盘并关闭句柄。
//      Logger 内部不再使用后台缓冲线程，也不长期持有日志文件句柄。
//      每条日志都会在跨进程互斥量保护下完成“选文件 → 追加一行 →
//      FlushFileBuffers → CloseHandle”，方便现场后台移动、删除、打包日志文件。
//
//   5. 模块初始化时缓存 ILogger*，后续统一通过该变量写日志。
//      推荐模式是在模块实现类中保存成员变量 `ILogger* m_logger`。
//      Init 阶段调用一次 GetLogger() 并保存，模块生命周期内都通过
//      m_logger->Write(...) 输出日志，避免每条日志都重新 GetLogger()。
//
//   6. 宏只作为简单场景的便利入口。
//      MEYER_LOG_DEBUG / INFO / WARN / ERROR / FATAL 自动注入
//      MEYER_MODULE_NAME，并对 GetLogger() 做空检查；正式模块代码
//      优先使用缓存后的 m_logger。
//
// 线程安全:
//   所有 ILogger 方法都是线程安全的。调用方可以在任意线程中
//   调用 Write()，无需外部同步。
//
// 进程安全:
//   多个进程可以同时写入同一个日志文件。实现使用 Windows 命名互斥量
//   (Global\MeyerScan_Logger_Mutex) 来串行化磁盘写入。
//
// 使用示例:
//   ---- Qt 模块 ----
//   #include <QString>
//   #include "Logger.h"
//   GetLogger()->Init("C:/ProgramData/MeyerScan/logs/", LogLevel::Info);
//   MEYER_LOG_INFO("CreateCase", "SN-001", "C-20260612-001", "Dr.Wang",
//                  QString("Case for patient %1").arg(patientName));
//   ILogger* logger = GetLogger();
//   logger->Write(LogLevel::Info, "CaseUI", "CreateWidget", "", "", "",
//                 "case page created");
//
//   ---- 非 Qt 模块（算法 DLL，纯 C++） ----
//   #include "Logger.h"
//   GetLogger()->Init("C:/ProgramData/MeyerScan/logs/", LogLevel::Info);
//   MEYER_LOG_INFO("ProcessFrame", "SN-001", "C-001", "",
//                  "Frame " + std::to_string(frameIdx) + " processed");
//
// 依赖（仅本头文件）:
//   <cstdint>   — int32_t 等（始终可用）
//   <string>    — std::string（始终可用，C++11）
//   <QString>   — 可选，由调用方在包含本头文件之前包含
// =============================================================================

#pragma once

#include <cstdint>
#include <string>

#if defined(QT_CORE_LIB) && !defined(QSTRING_H)
#  include <QString>
#endif

// =========================================================================
// DLL 导出/导入机制
// =========================================================================
// MEYER_LOGGER_EXPORTS 仅在编译 Logger.dll 本身时定义
//（通过 CMake 的 target_compile_definitions 设置）。消费者获得 dllimport，
// 这会生成通过 IAT 的更高效间接调用。
#ifdef MEYER_LOGGER_EXPORTS
#  define MEYER_LOGGER_API __declspec(dllexport)
#else
#  define MEYER_LOGGER_API __declspec(dllimport)
#endif

// =========================================================================
// LogLevel — 日志条目的严重程度
// =========================================================================
// 值的排序使得 "if (msgLevel >= currentFilter) { write; }" 能正确工作。
// Debug=0 最详细；Fatal=4 最简略。
//
// 典型过滤器设置:
//   开发 / 调试:            Debug
//   QA / 集成测试:          Info
//   客户现场稳定版本:        Warning（抑制 Info 噪音）
//   出厂 / 锁定版本:        Error
enum class LogLevel : int {
    Debug   = 0,   // 详细；逐帧追踪、函数进入/退出等
    Info    = 1,   // 正常操作事件（创建病例、开始扫描……）
    Warning = 2,   // 非预期但可恢复的情况（重试成功……）
    Error   = 3,   // 功能受损；应通知操作员
    Fatal   = 4    // 进程即将终止或进入不可用状态
};

// =========================================================================
// ILogger — 抽象接口
// =========================================================================
// 这是消费者代码能看到的唯一类型。具体实现 (LoggerImpl) 隐藏在 DLL 内部。
//
// 所有字符串为 UTF-8 编码。空字段应传递 ""（空字符串字面量），
// 绝不要传递 nullptr。
//
// 生命周期:
//   1. GetLogger()          — 获取进程级单例指针
//   2. Init(logDir, level)  — 初始化（记录日志目录和级别）
//   3. Write(...)           — 可在任意线程中任意次调用
//   4. Shutdown()           — 关闭可写状态
//
// Shutdown() 之后，如果需要，实例可以重新初始化（例如在会话中途
// 更改日志目录）。这种情况很少见，但受支持。
class ILogger {
public:
    virtual ~ILogger() = default;

    // -----------------------------------------------------------------
    // Init — 一次性设置（幂等；可重入）
    // -----------------------------------------------------------------
    // logDir: 日志文件将被写入的目录的绝对路径。
    //         如果目录不存在，将递归创建。
    //         示例: "C:/ProgramData/MeyerScan/logs"
    // level:  低于此级别的消息将被静默丢弃。
    //         之后可通过 SetLogLevel() 修改。
    // 返回值: 第一个日志文件是否成功打开。
    //
    // Init 可被多次调用。后续调用只更新日志级别；
    // 目录和文件句柄保持不变。
    virtual bool Init(const char* logDir, LogLevel level) = 0;

    // -----------------------------------------------------------------
    // Write — 热路径（每个会话调用数万次）
    // -----------------------------------------------------------------
    // 所有参数均为以 null 结尾的 UTF-8 C 字符串。
    // 对不适用的字段使用 ""（空字符串）；绝不要传递 nullptr。
    //
    // 参数语义:
    //   module:    调用方 DLL/EXE 的名称，例如 "MeyerScan_CaseOrderService"。
    //              由 MEYER_LOG_* 宏通过 MEYER_MODULE_NAME 编译期定义
    //              自动填充。
    //   operation: 模块正在执行的操作，例如 "CreateCase"、
    //              "SendCmd"、"LoadFile"。使用 PascalCase 命名。
    //   deviceId:  当前扫描仪设备的序列号。
    //   caseId:    病例 / 订单标识符。
    //   operator_: 操作员（医生/技师）标识符。
    //   content:   自由文本日志消息，英文。
    //
    // 线程安全: 完全可重入。可以在任意线程中调用，无需外部加锁。
    //
    // 性能:
    //   - 级别过滤是原子读取（无锁）。
    //   - 格式化在调用线程内完成，不占用全局锁。
    //   - “选择日志文件 + 写入一行 + FlushFileBuffers + CloseHandle”
    //     在命名互斥量保护下同步完成，保证多模块/多进程写同一文件时
    //     不出现半行交错。
    //   - 逐条打开/关闭文件会牺牲一部分吞吐量，但换来日志完整性、
    //     后台可移动/删除/打包日志文件，以及更低的现场维护成本。
    virtual void Write(LogLevel level,
                       const char* module,
                       const char* operation,
                       const char* deviceId,
                       const char* caseId,
                       const char* operator_,
                       const char* content) = 0;

    // -----------------------------------------------------------------
    // 运行时控制
    // -----------------------------------------------------------------

    // 更新级别过滤器。对所有线程立即生效。
    // 示例: SetLogLevel(LogLevel::Warning) 会抑制 Debug 和 Info。
    virtual void     SetLogLevel(LogLevel level) = 0;

    // 返回当前级别过滤器（原子读取，无锁）。
    virtual LogLevel GetLogLevel() const = 0;

    // Flush — 兼容接口。
    // 当前 Logger 每条日志都会同步写入、刷盘并关闭句柄，因此 Flush 是幂等空操作。
    virtual void     Flush() = 0;

    // Shutdown — 关闭可写状态。
    // 在卸载 DLL 或退出进程之前调用一次。
    //
    // Shutdown() 之后，Write() 调用变为空操作，
    // 直到再次调用 Init()。
    virtual void     Shutdown() = 0;

    // 返回模块版本字符串，格式如:
    // "MeyerScan_Logger v1.1.0 (2026-06-24)"。
    // 使用 const char*，避免跨 DLL 边界传递 C++ 对象。
    virtual const char* GetModuleVersion() const = 0;

    // =================================================================
    // 便捷重载 — 内联，编译到调用方的翻译单元中。
    // 这些永远不会在 DLL 边界上传递复杂类型。
    // =================================================================

    // ---- std::string（始终可用；C++11） ------------------------------
    // 简单地将 .c_str() 指针转发给主要重载。
    bool Init(const std::string& logDir, LogLevel level) {
        return Init(logDir.c_str(), level);
    }

    void Write(LogLevel level,
               const std::string& module,
               const std::string& operation,
               const std::string& deviceId,
               const std::string& caseId,
               const std::string& operator_,
               const std::string& content) {
        Write(level,
              module.c_str(),    operation.c_str(),
              deviceId.c_str(),  caseId.c_str(),
              operator_.c_str(), content.c_str());
    }

    // ---- QString（仅在引入 Qt 时可用） -------------------------------
    // 激活条件: 在 #include "Logger.h" 之前 #include <QString>。
    // QString 重载调用 toUtf8().constData() 来生成
    // 主要接口所需的以 null 结尾的 UTF-8 缓冲区。
    //
    // 守卫宏: QSTRING_H 由 <QString> 的包含守卫定义；
    // QT_CORE_LIB 在构建任何 Qt 模块时全局定义。
    // 二者之一被设置即表示 QString 可用。
#if defined(QSTRING_H) || defined(QT_CORE_LIB)
    // 使用 QString 初始化日志目录。
    // 该重载只存在于调用方编译单元中，真正跨 DLL 的仍是 const char* 主接口。
    bool Init(const QString& logDir, LogLevel level) {
        // toUtf8() 返回 QByteArray；constData() 提供一个
        // 在临时对象生命周期内有效的以 null 结尾的指针。
        return Init(logDir.toUtf8().constData(), level);
    }

    // 使用 QString 写日志。
    // 每个 QString 在调用前转换为 UTF-8 字节，DLL 内部不持有 Qt 对象。
    void Write(LogLevel level,
               const QString& module,
               const QString& operation,
               const QString& deviceId,
               const QString& caseId,
               const QString& operator_,
               const QString& content) {
        Write(level,
              module.toUtf8().constData(),
              operation.toUtf8().constData(),
              deviceId.toUtf8().constData(),
              caseId.toUtf8().constData(),
              operator_.toUtf8().constData(),
              content.toUtf8().constData());
    }
#endif // Qt
};

// =========================================================================
// 工厂函数 — DLL 导出
// =========================================================================
// 返回进程级单例 ILogger 实例的指针。
// 永远不会返回 nullptr（单例是 DLL 内部的静态局部变量）。
//
// extern "C" 避免 C++ 名称修饰。动态加载 Logger.dll 的模块可以通过
// GetProcAddress("GetLogger") 获取该函数。
extern "C" MEYER_LOGGER_API ILogger* GetLogger();

// =========================================================================
// 便捷宏
// =========================================================================
// 这些是写入日志条目的推荐方式。它们:
//   1. 自动注入 MEYER_MODULE_NAME（通过 CMake 按模块定义）
//   2. 对 GetLogger() 做空检查 → 在 Init() 之前或 Shutdown() 之后调用也安全
//   3. 用 do { ... } while(0) 包裹 → 在 if/else 中不带花括号也安全
//
// 每个模块必须在其 CMakeLists.txt 中定义 MEYER_MODULE_NAME:
//   target_compile_definitions(MyModule PRIVATE MEYER_MODULE_NAME="MyModule")
//
// 如果模块忘记定义，宏会展开为 "Unknown"，这样日志仍然会被写入，
// 并且遗漏在日志输出中一目了然。

#ifndef MEYER_MODULE_NAME
#  define MEYER_MODULE_NAME "Unknown"
#endif

// 模式:
//   do {                                    \
//       auto* _l = GetLogger();             \  // 获取单例指针
//       if (_l)                             \  // 如果 DLL 尚未加载则为空
//           _l->Write(level,                \
//                     MEYER_MODULE_NAME,     \  // 编译期常量
//                     op, dev, caseId,      \
//                     oper, content);        \
//   } while(0)                                 // 要求宏后跟分号

#define MEYER_LOG_DEBUG(op, dev, caseId, oper, content)  \
    do { auto* _l = GetLogger(); if (_l) _l->Write(LogLevel::Debug,  MEYER_MODULE_NAME, op, dev, caseId, oper, content); } while(0)

#define MEYER_LOG_INFO(op, dev, caseId, oper, content)   \
    do { auto* _l = GetLogger(); if (_l) _l->Write(LogLevel::Info,   MEYER_MODULE_NAME, op, dev, caseId, oper, content); } while(0)

#define MEYER_LOG_WARN(op, dev, caseId, oper, content)   \
    do { auto* _l = GetLogger(); if (_l) _l->Write(LogLevel::Warning,MEYER_MODULE_NAME, op, dev, caseId, oper, content); } while(0)

#define MEYER_LOG_ERROR(op, dev, caseId, oper, content)  \
    do { auto* _l = GetLogger(); if (_l) _l->Write(LogLevel::Error,  MEYER_MODULE_NAME, op, dev, caseId, oper, content); } while(0)

#define MEYER_LOG_FATAL(op, dev, caseId, oper, content)  \
    do { auto* _l = GetLogger(); if (_l) _l->Write(LogLevel::Fatal,  MEYER_MODULE_NAME, op, dev, caseId, oper, content); } while(0)
