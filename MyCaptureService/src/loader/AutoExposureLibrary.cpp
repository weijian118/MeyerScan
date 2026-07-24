// =============================================================================
// 文件: AutoExposureLibrary.cpp
// 作用: 实现自动曝光占位接口的动态加载和调用。
// =============================================================================
#include "AutoExposureLibrary.h"

#include "../support/PathUtils.h"

#include <windows.h>

#include <sstream>

namespace meyer
{
    namespace captureservice
    {
        struct AutoExposureLibrary::Functions
        {
            typedef std::int32_t (*AbiFunction)();
            typedef MeyerAutoExposureHandle (*CreateFunction)();
            typedef void (*DestroyFunction)(MeyerAutoExposureHandle);
            typedef std::int32_t (*InitOutputFunction)(MeyerAutoExposureOutput*);
            typedef std::int32_t (*ConfigureFunction)(MeyerAutoExposureHandle, const MeyerCaptureDeviceContext*);
            typedef std::int32_t (*CalculateFunction)(MeyerAutoExposureHandle, const MeyerCaptureProfileConfig*, const MeyerCaptureGroupInfo*, const unsigned char*, std::size_t, MeyerAutoExposureOutput*);
            typedef std::int32_t (*LastErrorFunction)(MeyerAutoExposureHandle, char*, std::size_t, std::size_t*);
            AbiFunction abi;
            CreateFunction create;
            DestroyFunction destroy;
            InitOutputFunction initOutput;
            ConfigureFunction configure;
            CalculateFunction calculate;
            LastErrorFunction lastError;
            Functions()
                : abi(nullptr), create(nullptr), destroy(nullptr), initOutput(nullptr),
                  configure(nullptr), calculate(nullptr), lastError(nullptr)
            {
            }
        };

        namespace
        {
            template<typename T>
            T Resolve(HMODULE module, const char* name)
            {
                return reinterpret_cast<T>(::GetProcAddress(module, name));
            }
        }

        AutoExposureLibrary::AutoExposureLibrary()
            : m_module(nullptr), m_handle(nullptr), m_functions(new Functions())
        {
        }

        AutoExposureLibrary::~AutoExposureLibrary()
        {
            Unload();
            delete m_functions;
            m_functions = nullptr;
        }

        std::int32_t AutoExposureLibrary::Load(const std::string& pathUtf8)
        {
            if (m_module != nullptr && m_handle != nullptr)
            {
                return MeyerAutoExposureResult_Ok;
            }
            std::wstring path;
            if (!Utf8ToWide(pathUtf8.c_str(), path) || !IsAbsoluteWindowsPath(path))
            {
                m_lastError = "AutoExposure DLL path must be an absolute UTF-8 path";
                return MeyerAutoExposureResult_InvalidArgument;
            }
            HMODULE module = ::LoadLibraryW(path.c_str());
            if (module == nullptr)
            {
                std::ostringstream message;
                message << "Failed to load AutoExposure DLL; Win32 error="
                        << static_cast<unsigned long>(::GetLastError());
                m_lastError = message.str();
                return MeyerAutoExposureResult_InternalError;
            }
            m_module = module;
            *m_functions = Functions();
            m_functions->abi = Resolve<Functions::AbiFunction>(module, "GetMeyerModuleApiVersion");
            m_functions->create = Resolve<Functions::CreateFunction>(module, "MeyerAutoExposure_Create");
            m_functions->destroy = Resolve<Functions::DestroyFunction>(module, "MeyerAutoExposure_Destroy");
            m_functions->initOutput = Resolve<Functions::InitOutputFunction>(module, "MeyerAutoExposure_InitOutput");
            m_functions->configure = Resolve<Functions::ConfigureFunction>(module, "MeyerAutoExposure_Configure");
            m_functions->calculate = Resolve<Functions::CalculateFunction>(module, "MeyerAutoExposure_Calculate");
            m_functions->lastError = Resolve<Functions::LastErrorFunction>(module, "MeyerAutoExposure_GetLastError");
            if (m_functions->abi == nullptr || m_functions->abi() != 1 ||
                m_functions->create == nullptr || m_functions->destroy == nullptr ||
                m_functions->initOutput == nullptr || m_functions->configure == nullptr ||
                m_functions->calculate == nullptr || m_functions->lastError == nullptr)
            {
                m_lastError = "AutoExposure API is missing or incompatible";
                Unload();
                return MeyerAutoExposureResult_InternalError;
            }
            m_handle = m_functions->create();
            if (m_handle == nullptr)
            {
                m_lastError = "AutoExposure failed to create a session";
                Unload();
                return MeyerAutoExposureResult_InternalError;
            }
            m_lastError.clear();
            return MeyerAutoExposureResult_Ok;
        }

        std::int32_t AutoExposureLibrary::Configure(
            const MeyerCaptureDeviceContext& context)
        {
            if (m_handle == nullptr)
            {
                return MeyerAutoExposureResult_InvalidHandle;
            }
            const std::int32_t result = m_functions->configure(m_handle, &context);
            if (result != MeyerAutoExposureResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t AutoExposureLibrary::Calculate(
            const MeyerCaptureProfileConfig& profile,
            const MeyerCaptureGroupInfo& groupInfo,
            const unsigned char* decrypted,
            std::size_t bytes,
            MeyerAutoExposureOutput& output)
        {
            m_functions->initOutput(&output);
            const std::int32_t result = m_functions->calculate(
                m_handle, &profile, &groupInfo, decrypted, bytes, &output);
            if (result != MeyerAutoExposureResult_Ok &&
                result != MeyerAutoExposureResult_NotImplemented)
            {
                ReadLastError();
            }
            return result;
        }

        const std::string& AutoExposureLibrary::LastError() const
        {
            return m_lastError;
        }

        bool AutoExposureLibrary::IsLoaded() const
        {
            return m_module != nullptr && m_handle != nullptr;
        }

        void AutoExposureLibrary::ReadLastError()
        {
            if (m_handle == nullptr || m_functions->lastError == nullptr)
            {
                return;
            }
            std::size_t required = 0U;
            m_functions->lastError(m_handle, nullptr, 0U, &required);
            if (required == 0U || required > 8192U)
            {
                return;
            }
            std::string buffer(required, '\0');
            if (m_functions->lastError(m_handle, &buffer[0], buffer.size(), &required) ==
                MeyerAutoExposureResult_Ok)
            {
                m_lastError.assign(buffer.c_str());
            }
        }

        void AutoExposureLibrary::Unload()
        {
            if (m_handle != nullptr && m_functions != nullptr &&
                m_functions->destroy != nullptr)
            {
                m_functions->destroy(m_handle);
                m_handle = nullptr;
            }
            if (m_module != nullptr)
            {
                ::FreeLibrary(static_cast<HMODULE>(m_module));
                m_module = nullptr;
            }
            if (m_functions != nullptr)
            {
                *m_functions = Functions();
            }
        }
    }
}
