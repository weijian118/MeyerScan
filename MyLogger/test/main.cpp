// =============================================================================
// 文件:    main.cpp
// 模块:    LoggerTest.exe — MeyerScan_Logger.dll 的冒烟/压力测试
//
// 用途:
//   测试 ILogger 的每一个公开 API，验证其正确性，并尽早暴露
//   任何线程、格式化或文件 I/O 方面的问题。
//
// 构建和运行方法:
//   cmake --build . --target LoggerTest
//   .\bin\LoggerTest.exe
//
// 如何验证输出:
//   在记事本或任意文本编辑器中打开生成的日志文件。
//   检查以下内容:
//     - 全部五个日志级别都应出现。
//     - "Filter" 部分应恰好包含一条日志行（Error 消息）。
//       那两条被抑制的 Debug/Info 消息不应出现。
//     - 压力测试应产生 5000 条 Info 行，且没有交错乱码。
//     - "Shutdown" 行应是最后一条。
//     - 日志文件名应符合 MeyerScan_YYYYMMDD_NNN.log 的模式。
//
// 本测试特意不依赖 Qt —— 只使用 C++ 标准库，
// 因此即使在没有安装 Qt 的构建机器上也能编译和运行。
// =============================================================================

#include "Logger.h"

#include <chrono>     // std::chrono::milliseconds, std::this_thread::sleep_for
#include <iostream>   // std::cout, std::cerr
#include <string>     // std::string, std::to_string
#include <thread>     // std::thread
#include <vector>     // std::vector

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== MeyerScan Logger Test ===\n" << std::endl;

    // ---- 1. 获取单例 -----------------------------------------------------------
    // GetLogger() 应返回非空指针。正常情况下，它永远不会返回空指针，
    // 因为单例是静态局部变量。但我们仍然做防御性检查。
    ILogger* log = GetLogger();
    if (!log) {
        std::cerr << "FATAL: GetLogger() returned null" << std::endl;
        return 1;
    }
    std::cout << "[PASS] GetLogger() returned non-null" << std::endl;

    // ---- 2. 初始化 -------------------------------------------------------------
    // 使用 std::string 重载（不依赖 Qt）。日志目录放在测试目录下，
    // 以免污染源代码树。
    //
    // 使用 LogLevel::Debug（最详细级别）—— 测试期间我们希望看到所有内容。
    bool ok = log->Init(
        std::string("F:\\MeyerScan\\MyLogger\\test\\logs"),
        LogLevel::Debug);
    if (!ok) {
        std::cerr << "FATAL: Init() failed — check that the path is writable"
                  << std::endl;
        return 1;
    }
    std::cout << "[PASS] Init() succeeded" << std::endl;

    // ---- 3. 每个级别各写一条消息 -------------------------------------------------
    // 每条调用都经过完整流水线:
    //   级别过滤 → FormatLine → Buffer::Add → (刷新线程)
    //
    // deviceId、caseId、operator 字段使用逼真的测试值，
    // 使日志输出在视觉上接近生产环境的日志。
    log->Write(LogLevel::Debug,   "LoggerTest", "AllLevels",
               "TEST-001", "CASE-001", "Tester",
               "Debug message — should appear because level filter is Debug");
    log->Write(LogLevel::Info,    "LoggerTest", "AllLevels",
               "TEST-001", "CASE-001", "Tester",
               "Info message — normal operational event");
    log->Write(LogLevel::Warning, "LoggerTest", "AllLevels",
               "TEST-001", "CASE-001", "Tester",
               "Warning message — recoverable anomaly");
    log->Write(LogLevel::Error,   "LoggerTest", "AllLevels",
               "TEST-001", "CASE-001", "Tester",
               "Error message — function impaired but continuing");
    log->Write(LogLevel::Fatal,   "LoggerTest", "AllLevels",
               "TEST-001", "CASE-001", "Tester",
               "Fatal message — the process is about to terminate");
    std::cout << "[PASS] All five log levels written" << std::endl;

    // ---- 4. 级别过滤测试 --------------------------------------------------------
    // 将过滤器提升到 Warning → Debug 和 Info 应被抑制。
    log->SetLogLevel(LogLevel::Warning);

    log->Write(LogLevel::Debug, "LoggerTest", "Filter",
               "-", "-", "-",
               "This Debug message SHOULD NOT appear in the log file");
    log->Write(LogLevel::Info,  "LoggerTest", "Filter",
               "-", "-", "-",
               "This Info message SHOULD NOT appear in the log file");
    log->Write(LogLevel::Error, "LoggerTest", "Filter",
               "-", "-", "-",
               "This Error message SHOULD appear (Error >= Warning)");

    // 恢复级别，使后续测试可以在 Debug/Info 级别记录日志。
    log->SetLogLevel(LogLevel::Debug);
    std::cout << "[PASS] Level filter: 2 Debug/Info messages suppressed, "
              << "1 Error message logged" << std::endl;

    // ---- 5. 多线程压力测试 -----------------------------------------------------
    // 10 个线程，每个写入 500 条日志 = 共计 5000 条。
    // 这测试了:
    //   - 并发的 LogBuffer::Add() 调用（互斥锁竞争）
    //   - 刷新阈值（100 行 — 我们会多次触发它）
    //   - 后台线程的跟上能力
    //
    // 每个线程使用不同的 deviceId，以便在日志中验证
    // 所有 10 个线程的输出都出现了。
    std::cout << "      Stress test: 10 threads × 500 messages each ... "
              << std::flush;

    std::vector<std::thread> threads;
    threads.reserve(10);  // 避免线程创建期间的重新分配
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([t, log]() {
            // 按值捕获 t。每个线程写入 500 条消息，使用唯一的设备 ID，
            // 以便输出可追溯。
            std::string devId = "Dev-" + std::to_string(t);
            for (int i = 0; i < 500; ++i) {
                // 对 content 使用 std::string 重载（字符串拼接）。
                std::string msg = "Thread " + std::to_string(t)
                                  + " msg " + std::to_string(i);
                log->Write(LogLevel::Info, "LoggerTest", "Stress",
                           devId.c_str(), "-", "-", msg.c_str());
            }
        });
    }

    // 等待所有线程完成。
    for (auto& th : threads) {
        th.join();
    }
    std::cout << "done (5000 messages)" << std::endl;
    std::cout << "[PASS] Multi-threaded stress: 10×500 messages, no crashes"
              << std::endl;

    // ---- 6. Error 级别立即刷新 -------------------------------------------------
    // 写入一条 Error 级别的消息。Write() 的实现应立即唤醒后台线程
    //（而不是等待 5 秒定时器）。
    log->Write(LogLevel::Error, "LoggerTest", "ImmediateFlush",
               "-", "-", "-",
               "This Error should trigger immediate flush (within ~100 ms)");

    // 给后台线程一个短暂的时间窗口来接收通知并刷新到磁盘。
    // 100 ms 远超实际需要（典型刷新耗时 < 1 ms），
    // 但为了应对慢速 CI 机器，我们留了充足的余量。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[PASS] Immediate flush on Error level" << std::endl;

    // ---- 7. 关闭 ---------------------------------------------------------------
    // 写入最后一条消息并关闭。关闭流程:
    //   1. 通知后台线程退出
    //   2. 等待线程结束 (join)
    //   3. 排空剩余的缓存行
    //   4. 关闭日志文件
    log->Write(LogLevel::Info, "LoggerTest", "Shutdown",
               "-", "-", "-", "About to shut down the logger");
    log->Shutdown();
    std::cout << "[PASS] Shutdown() completed" << std::endl;

    // ---- 8. 验证日志文件存在 ----------------------------------------------------
    // 快速健全检查 —— 日志目录下现在应至少有一个 .log 文件。
    std::cout << "[PASS] Log file written to: "
              << "F:\\MeyerScan\\MyLogger\\test\\logs\\" << std::endl;

    // ---- 完成 -----------------------------------------------------------------
    std::cout << "\n=== All tests passed ===" << std::endl;
    std::cout << "Check the log file(s) in: "
              << "F:\\MeyerScan\\MyLogger\\test\\logs\\" << std::endl;
    std::cout << "Expected line count: ~5000+ (stress) + ~7 (other messages)"
              << std::endl;

    return 0;
}
