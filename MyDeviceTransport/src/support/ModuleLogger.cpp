// =============================================================================
// 文件: ModuleLogger.cpp
// 作用: 通过 LoadLibrary/GetProcAddress 获取进程级 ILogger，避免设备模块在
//       编译期依赖 Logger.lib，同时复用全软件同一日志文件。
// =============================================================================
#include "ModuleLogger.h"
#include "ModuleInfo.h"

#include <windows.h>

#include <mutex>

namespace
{
    // 此枚举和 ILogger 前缀布局必须与 MyLogger/include/Logger.h 保持一致。
    enum class LogLevel : int
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4
    };

    // 这里只声明调用所需的稳定虚接口，不在本模块创建或销毁 Logger 对象。
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

    // 第一次写日志时加载 Logger。模块句柄保留到进程结束，避免静态析构顺序问题。
    ILogger* ResolveLogger()
    {
        std::lock_guard<std::mutex> lock(g_loggerMutex);
        if (g_logger != nullptr)
        {
            return g_logger;
        }

        if (g_loggerModule == nullptr)
        {
            // LoadLibrary 的首选搜索位置包含 EXE 同级目录，不依赖 currentPath。
            g_loggerModule = ::LoadLibraryW(L"MeyerScan_Logger.dll");
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

    // 统一补充模块名，设备号/病例号/操作员由更高层会话日志记录。
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
    namespace device
    {
        namespace logging
        {
            // 写入 Info 级别成功事件。
            void WriteInfo(const char* operation, const char* content)
            {
                Write(LogLevel::Info, operation, content);
            }

            // 写入 Error 级别失败事件。
            void WriteError(const char* operation, const char* content)
            {
                Write(LogLevel::Error, operation, content);
            }
        }
    }
}
