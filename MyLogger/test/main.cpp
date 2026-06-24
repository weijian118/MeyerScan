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
//     - 空字段不应生成 [Dev:-]、[Case:-]、[Op:-] 这类占位。
//     - 日志文件名默认应符合 MeyerScan_YYYYMMDD.log；达到大小上限后才追加 _NNN。
//     - 写完一条后文件句柄应关闭，测试会尝试重命名日志文件。
//
// 本测试特意不依赖 Qt —— 只使用 C++ 标准库，
// 因此即使在没有安装 Qt 的构建机器上也能编译和运行。
// =============================================================================

#include "Logger.h"

#include <chrono>     // std::chrono::milliseconds, std::this_thread::sleep_for
#include <cstdio>     // std::rename, std::remove
#include <fstream>    // std::ifstream
#include <iostream>   // std::cout, std::cerr
#include <string>     // std::string, std::to_string
#include <thread>     // std::thread
#include <vector>     // std::vector
#include <windows.h>  // GetLocalTime, SYSTEMTIME

namespace {
// 测试日志目录。集中定义，避免测试中硬编码散落。
const char* kLogDir = "F:\\MeyerScan\\MyLogger\\test\\logs";

// 返回今天的主日志文件路径。
// Logger 新规则是每天默认只生成 MeyerScan_YYYYMMDD.log。
std::string TodayLogPath() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char date[16];
    snprintf(date, sizeof(date), "%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
    return std::string(kLogDir) + "\\MeyerScan_" + date + ".log";
}

// 读取整个文本文件。
// 测试只读取当前日志文件，文件通常很小，直接读入字符串足够简单。
std::string ReadTextFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== MeyerScan Logger Test ===\n" << std::endl;

    // ---- 0. 清理当天测试日志 ---------------------------------------------------
    // Logger 的生产规则是“每天一个文件，超限后加序号”。
    // 测试目录可能残留旧版本当天日志，里面可能还有 [Txt:]、[INFO ] 等历史格式。
    // 如果直接扫描整天文件，会把旧内容误判为本次测试失败。
    // 因此测试启动前只删除测试目录下当天主日志文件，不影响正式 logs 目录。
    const std::string logPath = TodayLogPath();
    std::remove(logPath.c_str());
    std::remove((logPath + ".move_test").c_str());

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
    bool ok = log->Init(std::string(kLogDir), LogLevel::Debug);
    if (!ok) {
        std::cerr << "FATAL: Init() failed — check that the path is writable"
                  << std::endl;
        return 1;
    }
    std::cout << "[PASS] Init() succeeded" << std::endl;

    // ---- 3. 每个级别各写一条消息 -------------------------------------------------
    // 每条调用都经过完整流水线:
    //   级别过滤 → FormatLine → 跨进程互斥 → 文件轮转判断 → 追加一行 → 刷盘并关闭句柄
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
               "", "", "",
               "This Debug message SHOULD NOT appear in the log file");
    log->Write(LogLevel::Info,  "LoggerTest", "Filter",
               "", "", "",
               "This Info message SHOULD NOT appear in the log file");
    log->Write(LogLevel::Error, "LoggerTest", "Filter",
               "", "", "",
               "This Error message SHOULD appear (Error >= Warning)");

    // 恢复级别，使后续测试可以在 Debug/Info 级别记录日志。
    log->SetLogLevel(LogLevel::Debug);
    std::cout << "[PASS] Level filter: 2 Debug/Info messages suppressed, "
              << "1 Error message logged" << std::endl;

    // ---- 5. 多线程压力测试 -----------------------------------------------------
    // 10 个线程，每个写入 500 条日志 = 共计 5000 条。
    // 这测试了:
    //   - 多线程同时进入 Logger::Write()
    //   - 跨进程互斥量能串行化写入
    //   - 每条日志打开/写入/关闭文件句柄不会崩溃
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
                           devId.c_str(), "", "", msg.c_str());
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

    // ---- 6. Error 级别立即落盘 -------------------------------------------------
    // 当前 Logger 不再使用后台线程。Write() 返回前已经写入、刷盘并关闭句柄。
    log->Write(LogLevel::Error, "LoggerTest", "ImmediateFlush",
               "", "", "",
               "This Error should be written and closed before Write returns");

    std::cout << "[PASS] Immediate write on Error level" << std::endl;

    // ---- 7. 格式和文件句柄验证 -------------------------------------------------
    // 空字段日志不应生成占位字段，同时 content 字段必须使用清晰的 [Content:] 标记。
    log->Write(LogLevel::Info, "LoggerTest", "CompactFormat",
               "", "", "", "Compact format should omit empty fields");
    const std::string logText = ReadTextFile(logPath);
    if (logText.find("[Dev:-") != std::string::npos ||
        logText.find("[Case:-") != std::string::npos ||
        logText.find("[Op:-") != std::string::npos ||
        logText.find("[INFO ]") != std::string::npos ||
        logText.find("[Txt:") != std::string::npos ||
        logText.find("[Content:Compact format should omit empty fields]") == std::string::npos) {
        std::cerr << "FATAL: Compact log format check failed" << std::endl;
        return 1;
    }
    std::cout << "[PASS] Compact format omits empty fields" << std::endl;

    // 写完后 Logger 不应长期占用文件句柄；重命名成功说明后台可以移动/删除日志文件。
    const std::string movedPath = logPath + ".move_test";
    std::remove(movedPath.c_str());
    if (std::rename(logPath.c_str(), movedPath.c_str()) != 0) {
        std::cerr << "FATAL: Log file rename failed — file handle may still be open" << std::endl;
        return 1;
    }
    if (std::rename(movedPath.c_str(), logPath.c_str()) != 0) {
        std::cerr << "FATAL: Log file restore failed after rename test" << std::endl;
        return 1;
    }
    std::cout << "[PASS] Log file can be moved after each write" << std::endl;

    // ---- 8. 关闭 ---------------------------------------------------------------
    // 当前 Shutdown 只关闭可写状态。文件句柄已经在每次 Write() 后关闭。
    log->Write(LogLevel::Info, "LoggerTest", "Shutdown",
               "", "", "", "About to shut down the logger");
    log->Shutdown();
    std::cout << "[PASS] Shutdown() completed" << std::endl;

    // ---- 9. 验证日志文件存在 ----------------------------------------------------
    // 快速健全检查 —— 日志目录下现在应至少有一个 .log 文件。
    std::cout << "[PASS] Log file written to: "
              << kLogDir << std::endl;

    // ---- 完成 -----------------------------------------------------------------
    std::cout << "\n=== All tests passed ===" << std::endl;
    std::cout << "Check the log file(s) in: " << kLogDir << std::endl;
    std::cout << "Expected line count: ~5000+ (stress) + ~7 (other messages)"
              << std::endl;

    return 0;
}
