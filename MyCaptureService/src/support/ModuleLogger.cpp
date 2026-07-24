// =============================================================================
// 文件: ModuleLogger.cpp
// 作用: 从 CaptureService 自身目录动态加载 MeyerScan_Logger.dll。
// =============================================================================
#include "ModuleLogger.h"

#include "ModuleInfo.h"
#include "PathUtils.h"

#include <windows.h>

#include <mutex>

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

    // 只声明 Logger 公共虚函数前缀；具体实现仍由 Logger DLL 管理。
    class ILogger
    {
    public:
        virtual ~ILogger() {}
        virtual bool Init(const char* logDir, LogLevel level) = 0;
        virtual void Write(LogLevel level, const char* module, const char* operation,
                           const char* deviceId, const char* caseId,
                           const char* operatorId, const char* content) = 0;
        virtual void SetLogLevel(LogLevel level) = 0;
        virtual LogLevel GetLogLevel() const = 0;
        virtual void Flush() = 0;
        virtual void Shutdown() = 0;
        virtual const char* GetModuleVersion() const = 0;
    };

    typedef ILogger* (*GetLoggerFunction)();
    std::mutex g_mutex;
    HMODULE g_module = nullptr;
    ILogger* g_logger = nullptr;

    // 首次记录日志时加载同级 Logger，避免在 DLL 加载阶段执行复杂初始化。
    ILogger* ResolveLogger()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_logger != nullptr)
        {
            return g_logger;
        }
        const std::string path = meyer::captureservice::SiblingModulePathUtf8(
            "MeyerScan_Logger.dll");
        std::wstring widePath;
        if (path.empty() || !meyer::captureservice::Utf8ToWide(path.c_str(), widePath))
        {
            return nullptr;
        }
        g_module = ::LoadLibraryW(widePath.c_str());
        if (g_module == nullptr)
        {
            return nullptr;
        }
        GetLoggerFunction getLogger = reinterpret_cast<GetLoggerFunction>(
            ::GetProcAddress(g_module, "GetLogger"));
        if (getLogger == nullptr)
        {
            return nullptr;
        }
        g_logger = getLogger();
        return g_logger;
    }

    // 统一补充模块名，调用方只需要提供操作名和英文诊断文本。
    void Write(LogLevel level, const char* operation, const char* content)
    {
        ILogger* logger = ResolveLogger();
        if (logger == nullptr)
        {
            return;
        }
        logger->Write(level, ModuleInfo::Name,
                      operation == nullptr ? "" : operation,
                      "", "", "", content == nullptr ? "" : content);
    }
}

namespace meyer
{
    namespace captureservice
    {
        void WriteInfo(const char* operation, const char* content)
        {
            Write(LogLevel::Info, operation, content);
        }

        void WriteWarning(const char* operation, const char* content)
        {
            Write(LogLevel::Warning, operation, content);
        }

        void WriteError(const char* operation, const char* content)
        {
            Write(LogLevel::Error, operation, content);
        }
    }
}
