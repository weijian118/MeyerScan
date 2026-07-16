// =============================================================================
// 文件: ModuleLogger.cpp
// 作用: 从 DeviceCmd DLL 自身目录动态加载 MeyerScan_Logger.dll，复用进程日志。
// =============================================================================
#include "ModuleLogger.h"

#include "ModuleInfo.h"

#include <windows.h>

#include <mutex>
#include <string>
#include <vector>

namespace
{
    enum class LogLevel : int
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4
    };

    // 该最小虚接口前缀必须与 MyLogger/include/Logger.h 保持一致。
    class ILogger
    {
    public:
        virtual ~ILogger() {}
        virtual bool Init(const char* logDir, LogLevel level) = 0;
        virtual void Write(LogLevel level,
                           const char* module,
                           const char* operation,
                           const char* deviceId,
                           const char* caseId,
                           const char* operatorId,
                           const char* content) = 0;
        virtual void SetLogLevel(LogLevel level) = 0;
        virtual LogLevel GetLogLevel() const = 0;
        virtual void Flush() = 0;
        virtual void Shutdown() = 0;
        virtual const char* GetModuleVersion() const = 0;
    };

    typedef ILogger* (*GetLoggerFunction)();

    std::mutex g_loggerMutex;
    HMODULE g_loggerModule = nullptr;
    ILogger* g_logger = nullptr;

    // 获取当前 DeviceCmd DLL 文件夹，不能依赖进程 current directory。
    bool GetOwnDirectory(std::wstring& directory)
    {
        HMODULE ownModule = nullptr;
        const BOOL found = ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetOwnDirectory),
            &ownModule);
        if (!found || ownModule == nullptr)
        {
            return false;
        }

        std::vector<wchar_t> path(32768U, L'\0');
        const DWORD length = ::GetModuleFileNameW(ownModule, &path[0], static_cast<DWORD>(path.size()));
        if (length == 0U || length >= path.size())
        {
            return false;
        }

        directory.assign(&path[0], length);
        const std::wstring::size_type slash = directory.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            directory.clear();
            return false;
        }
        directory.resize(slash);
        return true;
    }

    // 第一次写日志时解析全局 Logger；失败只影响日志，不中断设备流程。
    ILogger* ResolveLogger()
    {
        std::lock_guard<std::mutex> lock(g_loggerMutex);
        if (g_logger != nullptr)
        {
            return g_logger;
        }

        if (g_loggerModule == nullptr)
        {
            std::wstring moduleDirectory;
            if (!GetOwnDirectory(moduleDirectory))
            {
                return nullptr;
            }
            const std::wstring loggerPath = moduleDirectory + L"\\MeyerScan_Logger.dll";
            g_loggerModule = ::LoadLibraryW(loggerPath.c_str());
            if (g_loggerModule == nullptr)
            {
                return nullptr;
            }
        }

        GetLoggerFunction getLogger = reinterpret_cast<GetLoggerFunction>(
            ::GetProcAddress(g_loggerModule, "GetLogger"));
        if (getLogger == nullptr)
        {
            return nullptr;
        }
        g_logger = getLogger();
        return g_logger;
    }

    // 模块名由 ModuleInfo 自动补充，调用点只提供操作 key 和内容。
    void Write(LogLevel level, const char* operation, const char* content)
    {
        ILogger* logger = ResolveLogger();
        if (logger == nullptr)
        {
            return;
        }
        logger->Write(level,
                      ModuleInfo::Name,
                      operation == nullptr ? "" : operation,
                      "",
                      "",
                      "",
                      content == nullptr ? "" : content);
    }
}

namespace meyer
{
    namespace devicecmd
    {
        namespace logging
        {
            void WriteInfo(const char* operation, const char* content)
            {
                // 通过统一内部函数补充模块名，并把空指针转换为空字符串。
                Write(LogLevel::Info, operation, content);
            }

            void WriteWarning(const char* operation, const char* content)
            {
                // 警告和普通信息共用同一动态 Logger，避免模块各自维护文件句柄。
                Write(LogLevel::Warning, operation, content);
            }

            void WriteError(const char* operation, const char* content)
            {
                // 错误日志只提供诊断能力，不改变调用方正在处理的返回值。
                Write(LogLevel::Error, operation, content);
            }
        }
    }
}
