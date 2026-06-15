// =============================================================================
// 文件:    LogWriter.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 文件 I/O 使用 C stdio (fopen / fwrite / fflush / fclose) 而非
//     Windows CreateFileW / WriteFile。理由:
//       a) 代码更简单 —— 无需处理 OVERLAPPED、缓冲区管理或手动换行转换。
//       b) 性能足够 —— 日志记录不是高吞吐量的数据汇。
//       c) 可移植性 —— 如果将来需要在非 Windows 平台上编译 Logger 的
//          诊断版本进行测试，stdio 是通用的。
//   - stdio 缓冲被显式禁用 (setvbuf _IONBF)。这意味着每次 fwrite() 调用
//     直接到达操作系统，配合命名互斥量确保来自不同进程的行
//     永远不会在单行内交错。
//   - 使用 SHCreateDirectoryExW 进行递归目录创建，因为 CreateDirectory
//     只能创建最后一级组件。SHCreateDirectoryExW 在 Windows XP SP2+
//     上可用，且在 %ProgramData% 下的路径不需要管理员权限。
//   - 命名互斥量的名称是硬编码的。如果应用的多个实例需要
//     分离的日志流，它们应使用不同的日志目录 ——
//     而非不同的互斥量名称。
// =============================================================================

#include "LogWriter.h"
#include <shlobj.h>    // SHCreateDirectoryExW
#include <cstring>     // strlen
#include <sys/stat.h>  // _stati64

// ---------------------------------------------------------------------------
// 互斥量名称常量
// ---------------------------------------------------------------------------
// Global\ 前缀:  互斥量对所有用户会话可见。当
// ScanReconstructStudio.exe 可能在不同会话下运行时需要
//（例如远程桌面、服务账户）。
//
// 回退到 Local\:  如果进程没有 SeCreateGlobalPrivilege
//（在锁定严格的医院 PC 上很少见），使用 Global\ 的 CreateMutex 会失败。
// Local\ 将串行化限制在当前会话内，这对于常见的单会话场景
// 仍然是正确的。
static const wchar_t* kMutexGlobal = L"Global\\MeyerScan_Logger_Mutex";
static const wchar_t* kMutexLocal  = L"Local\\MeyerScan_Logger_Mutex";

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
LogWriter::LogWriter() {
    // 先尝试 Global 命名空间。
    m_mutex = CreateMutexW(
        nullptr,        // 默认安全描述符（可继承）
        FALSE,          // 初始不被拥有 — 第一次 WaitForSingleObject
                        // 将立即成功。
        kMutexGlobal);

    if (!m_mutex) {
        // 回退到 Local 命名空间。GetLastError() 将是
        // ERROR_ACCESS_DENIED 或 ERROR_NO_SUCH_PRIVILEGE。
        m_mutex = CreateMutexW(nullptr, FALSE, kMutexLocal);
    }

    // 如果两者都失败（极不可能 — 需要完全损坏的 kernel32），
    // m_mutex 保持 nullptr。WriteLine() 将跳过互斥量等待
    // 并无保护地写入。有无串行化的日志总比没有日志好。
}

// ---------------------------------------------------------------------------
// 析构函数
// ---------------------------------------------------------------------------
LogWriter::~LogWriter() {
    Close();
    if (m_mutex) {
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------
bool LogWriter::Open(const std::string& path) {
    // 先关闭任何之前打开的文件句柄（幂等）。
    Close();

    // ---- 确保父目录存在 ------------------------------------------------------
    // 提取路径的目录部分（最后一个反斜杠或正斜杠之前的所有内容）。
    std::string dir(path);
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
        dir = dir.substr(0, slash);

        // 转换为宽字符串以调用 Windows API。
        // 从 char* 构造 std::wstring 假设系统默认代码页，
        // 对于 ASCII 目录路径通常没问题。
        // 对于包含非 ASCII 字符的路径，调用方应传递 UTF-8，
        // 我们需要使用 MultiByteToWideChar(CP_UTF8, ...)。由于我们的
        // 日志目录始终是 ASCII（"C:/ProgramData/MeyerScan/logs"），
        // 简单转换可以正常工作。
        std::wstring wdir(dir.begin(), dir.end());

        // SHCreateDirectoryExW 创建所有中间目录。
        // 成功时返回 ERROR_SUCCESS 或 ERROR_ALREADY_EXISTS。
        // 我们忽略返回值 — 如果目录确实无法创建
        //（例如根目录权限被拒绝），下方的 fopen_s 将失败
        // 并且我们返回 false。
        SHCreateDirectoryExW(nullptr, wdir.c_str(), nullptr);
    }

    // ---- 打开文件 ------------------------------------------------------------
    // 模式 "a":  追加。如果文件不存在则创建；不截断已有文件。
    // 这对于日志轮转是正确的，因为轮转逻辑保证我们每个会话
    // 只打开每个文件名一次（当达到大小阈值时选择新文件名，
    // 因此我们永远不会追加到已经 ≥ 10 MiB 的文件）。
    errno_t err = fopen_s(&m_file, path.c_str(), "a");
    if (err != 0 || !m_file) {
        m_file = nullptr;
        return false;
    }

    // ---- 禁用 stdio 缓冲 ----------------------------------------------------
    // _IONBF = 无缓冲。每次 fwrite() 直接转换为 WriteFile() 系统调用。
    // 这对于跨进程互斥量实际串行化写入至关重要 ——
    // 如果 stdio 有缓冲，两个进程可能各自缓冲部分行，
    // 然后在不同时间刷新，导致交错输出。
    setvbuf(m_file, nullptr, _IONBF, 0);

    m_path = path;
    return true;
}

// ---------------------------------------------------------------------------
// WriteLine
// ---------------------------------------------------------------------------
void LogWriter::WriteLine(const std::string& line) {
    if (!m_file) {
        // 文件未打开（Init() 失败或从未被调用）。静默丢弃该行。
        // 我们有意不在此处调用 OutputDebugString，
        // 因为对于在日志器初始化之前调用 Write() 的模块
        //（早期启动），这是常见情况。
        return;
    }

    // ---- 跨进程串行化 --------------------------------------------------------
    // 等待最多 500 ms 获取互斥量的所有权。
    //
    // 为什么是 500 ms？
    //   - 另一个进程只应在单次 fwrite() 调用的持续时间内持有互斥量
    //     —— 对于约 200 字节的行通常 < 1 μs。
    //   - 500 ms 足够长以容忍短暂的操作系统调度抖动，
    //     而不会无限期阻塞调用线程。
    //   - 如果超时触发，另一个进程可能已挂起或在持有互斥量时崩溃。
    //     在这种情况下，丢弃一行日志是相比于阻塞 UI 线程的正确权衡。
    //
    // 如果 m_mutex 是 nullptr（Global 和 Local 创建都失败了），
    // 我们跳过等待并无保护地写入。这是一个针对病态环境的
    //"尽力而为"回退。
    if (m_mutex) {
        DWORD rc = WaitForSingleObject(m_mutex, 500);
        if (rc != WAIT_OBJECT_0) {
            // 超时 (WAIT_TIMEOUT) 或错误 (WAIT_FAILED/WAIT_ABANDONED)。
            // WAIT_ABANDONED 意味着拥有线程在未释放互斥量的情况下死亡
            // — 操作系统授予我们所有权，但受保护的资源可能处于不一致状态。
            // 我们选择丢弃该行，而非冒险写入损坏的输出。
            OutputDebugStringA(
                "[Logger] Cross-process mutex wait failed — line dropped\n");
            return;
        }
    }

    // ---- 写入日志行 ----------------------------------------------------------
    // 行是 UTF-8。我们写入原始字节外加一个换行符。
    // fwrite 返回写入的项数；我们忽略它，因为磁盘写入失败时
    // 没有有意义的恢复方式（磁盘可能已满，下一行也会失败）。
    fwrite(line.c_str(), 1, line.size(), m_file);
    fputc('\n', m_file);  // 仅换行符；不需要回车符，
                          // 因为 _IONBF 绕过 stdio 的文本模式转换。
                          // 文件在记事本中仍能正确打开
                          //（自 Windows 10 1809 起记事本支持裸 LF）。

    // ---- 释放互斥量 ----------------------------------------------------------
    if (m_mutex) {
        ReleaseMutex(m_mutex);
    }
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------
// Flush 不获取跨进程互斥量。它只保证本进程 CRT 缓冲的数据
// 被提交给操作系统。其他进程可能仍有数据在其各自的 CRT 缓冲区中，
// 但这没关系 — 每个进程独立刷新，互斥量只串行化实际的磁盘写入。
void LogWriter::Flush() {
    if (m_file) {
        fflush(m_file);
    }
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------
void LogWriter::Close() {
    if (m_file) {
        fflush(m_file);  // 关闭前尽力刷新。
        fclose(m_file);
        m_file = nullptr;
    }
    m_path.clear();
}

// ---------------------------------------------------------------------------
// FileSize
// ---------------------------------------------------------------------------
uint64_t LogWriter::FileSize() const {
    if (m_path.empty()) {
        return 0;
    }

    // _stati64:  64 位文件大小的 stat（MSVC 扩展）。
    // 这不会打开文件；只读取目录元数据，在 NTFS 上是原子的。
    struct _stati64 st;
    if (_stati64(m_path.c_str(), &st) != 0) {
        return 0;  // 文件不存在或不可访问。
    }
    return static_cast<uint64_t>(st.st_size);
}
