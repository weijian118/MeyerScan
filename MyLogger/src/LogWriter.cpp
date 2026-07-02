// =============================================================================
// 文件:    LogWriter.cpp
// 模块:    MeyerScan_Logger.dll（内部）
//
// 实现说明:
//   - 文件 I/O 使用 Win32 CreateFile/WriteFile/FlushFileBuffers/CloseHandle。
//     每写一条日志都打开并关闭句柄，便于后台删除、移动或打包日志文件。
//   - 早期版本使用 C stdio (fopen / fwrite / fflush / fclose) 并长期持有句柄。
//     这对吞吐量友好，但不满足现场维护“写完即可移动/删除日志文件”的要求。
//   - 曾考虑继续使用 C stdio:
//     Windows CreateFileW / WriteFile。理由:
//       a) 代码更简单 —— 无需处理 OVERLAPPED、缓冲区管理或手动换行转换。
//       b) 性能足够 —— 日志记录不是高吞吐量的数据汇。
//       c) 可移植性 —— 如果将来需要在非 Windows 平台上编译 Logger 的
//          诊断版本进行测试，stdio 是通用的。
//   - 使用 SHCreateDirectoryExW 进行递归目录创建，因为 CreateDirectory
//     只能创建最后一级组件。SHCreateDirectoryExW 在 Windows XP SP2+
//     上可用，且在 %ProgramData% 下的路径不需要管理员权限。
//   - 路径从公共接口传入时约定为 UTF-8。写文件前统一转成 UTF-16，
//     再调用 Win32 宽字符 API，避免安装目录包含中文时日志无法创建。
//   - 命名互斥量的名称是硬编码的。如果应用的多个实例需要
//     分离的日志流，它们应使用不同的日志目录 ——
//     而非不同的互斥量名称。
// =============================================================================

#include "LogWriter.h"
#include <shlobj.h>    // SHCreateDirectoryExW

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
// Utf8ToWide
// ---------------------------------------------------------------------------
// Logger 公共 ABI 使用 UTF-8 const char*，但 Windows 文件 API 的可靠入口是
// UTF-16 宽字符版本。这里把日志路径从 UTF-8 转为 UTF-16。
//
// 如果调用方传入的不是合法 UTF-8（例如历史 ANSI 路径），函数会退回到
// 简单字节扩展，至少保证纯 ASCII 路径仍能工作。
static std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }

    // 第一次调用只询问需要多少 wchar_t。返回值包含结尾的 L'\0'。
    int count = MultiByteToWideChar(CP_UTF8,
                                    MB_ERR_INVALID_CHARS,
                                    text.c_str(),
                                    -1,
                                    nullptr,
                                    0);
    if (count <= 1) {
        // 非法 UTF-8 或空结果时保留历史 ASCII/ANSI 行为，避免直接失败。
        return std::wstring(text.begin(), text.end());
    }

    // 先按 count 分配，给 MultiByteToWideChar 写入结尾 L'\0' 留足空间。
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        text.c_str(),
                        -1,
                        &wide[0],
                        count);
    // std::wstring 作为路径使用时不需要保留结尾 L'\0'。
    wide.resize(static_cast<size_t>(count - 1));
    return wide;
}

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
    if (m_mutex) {
        // 命名互斥量是 Windows 内核对象，必须 CloseHandle 释放当前进程句柄。
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }
}

// ---------------------------------------------------------------------------
// EnsureParentDirectory
// ---------------------------------------------------------------------------
bool LogWriter::EnsureParentDirectory(const std::string& path) const {
    std::string dir(path);
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
        dir = dir.substr(0, slash);

        // 路径约定为 UTF-8，转换成 UTF-16 后使用 Windows 宽字符 API。
        // 这让安装目录包含中文时仍能创建 logs 目录。
        const std::wstring wdir = Utf8ToWide(dir);

        // SHCreateDirectoryExW 创建所有中间目录。
        // 成功时返回 ERROR_SUCCESS 或 ERROR_ALREADY_EXISTS。
        // 我们忽略返回值 — 如果目录确实无法创建
        //（例如根目录权限被拒绝），下方的 fopen_s 将失败
        // 并且我们返回 false。
        const int rc = SHCreateDirectoryExW(nullptr, wdir.c_str(), nullptr);
        return rc == ERROR_SUCCESS ||
               rc == ERROR_ALREADY_EXISTS ||
               rc == ERROR_FILE_EXISTS;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Lock
// ---------------------------------------------------------------------------
bool LogWriter::Lock(uint32_t timeoutMs) {
    if (!m_mutex) {
        // 极端环境下互斥量创建失败，仍允许写日志。
        // 这种情况下无法跨进程序列化，但有日志比完全无日志更有价值。
        return true;
    }

    const DWORD rc = WaitForSingleObject(m_mutex, timeoutMs);
    if (rc == WAIT_OBJECT_0 || rc == WAIT_ABANDONED) {
        // WAIT_ABANDONED 表示上一个持有互斥量的进程异常退出。
        // 日志文件本身是追加文本，继续写入比丢弃全部日志更合理。
        return true;
    }

    OutputDebugStringA("[Logger] Cross-process mutex wait failed — line dropped\n");
    return false;
}

// ---------------------------------------------------------------------------
// Unlock
// ---------------------------------------------------------------------------
void LogWriter::Unlock() {
    if (m_mutex) {
        ReleaseMutex(m_mutex);
    }
}

// ---------------------------------------------------------------------------
// WriteLine
// ---------------------------------------------------------------------------
bool LogWriter::WriteLine(const std::string& path, const std::string& line) {
    // 互斥量覆盖“选择好文件路径后的实际写入”阶段。
    // 文件路径由 LogRotation 选择；这里保证同一时刻只有一个进程往目标文件追加一行。
    if (!Lock()) {
        return false;
    }
    // WriteLineUnlocked 假设调用方已经拿到锁，避免锁逻辑和文件 I/O 互相嵌套过深。
    const bool ok = WriteLineUnlocked(path, line);
    Unlock();
    return ok;
}

// ---------------------------------------------------------------------------
// WriteLineUnlocked
// ---------------------------------------------------------------------------
bool LogWriter::WriteLineUnlocked(const std::string& path, const std::string& line) {
    if (!EnsureParentDirectory(path)) {
        OutputDebugStringA("[Logger] Failed to create log directory\n");
        return false;
    }

    // 路径从 UTF-8 转为 UTF-16 后调用 CreateFileW。
    // 这样 MeyerScan 安装在中文目录、用户名目录或 OEM 自定义目录时，
    // 日志仍能正确写入。
    const std::wstring widePath = Utf8ToWide(path);
    HANDLE file = CreateFileW(widePath.c_str(),
                              // FILE_APPEND_DATA 让 Windows 把每次写入追加到文件尾部，
                              // 避免多个进程先 Seek 再 Write 产生竞态。
                              FILE_APPEND_DATA,
                              // 允许读、写、删除共享，是为了售后工具可以在日志继续写入时打包/移动旧文件。
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("[Logger] Failed to open log file\n");
        return false;
    }

    DWORD written = 0;
    bool ok = true;

    // 行内容是 UTF-8 原始字节。先写正文，再写 Windows 友好的 CRLF。
    if (!line.empty()) {
        // WriteFile 的 written 是 DWORD；日志单行不会超过 4GB，转换是安全的。
        ok = WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) &&
             // 检查 written 可以发现磁盘满、网络盘异常等“部分写入”问题。
             written == line.size();
    }

    if (ok) {
        static const char kNewLine[] = "\r\n";
        ok = WriteFile(file, kNewLine, 2, &written, nullptr) && written == 2;
    }

    // FlushFileBuffers 比 fflush 更强，要求操作系统尽量把本文件句柄的数据刷到底层设备。
    // 这会降低性能，但满足“每条日志尽量安全、完整落盘”的要求。
    if (ok) {
        ok = FlushFileBuffers(file) != FALSE;
    }

    CloseHandle(file);

    if (!ok) {
        OutputDebugStringA("[Logger] Failed to write log line\n");
    }
    return ok;
}
