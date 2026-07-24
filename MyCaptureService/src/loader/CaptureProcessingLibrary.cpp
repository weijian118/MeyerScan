// =============================================================================
// 文件: CaptureProcessingLibrary.cpp
// 作用: 实现组帧、解密和慢处理 DLL 的动态调用。
// =============================================================================
#include "CaptureProcessingLibrary.h"

#include "../support/PathUtils.h"

#include <windows.h>

#include <sstream>

namespace meyer
{
    namespace captureservice
    {
        struct CaptureProcessingLibrary::Functions
        {
            typedef std::int32_t (*AbiFunction)();
            typedef MeyerCaptureProcessingHandle (*CreateFunction)();
            typedef void (*DestroyFunction)(MeyerCaptureProcessingHandle);
            typedef std::int32_t (*InitProfileFunction)(MeyerCaptureProfileConfig*);
            typedef std::int32_t (*InitContextFunction)(MeyerCaptureDeviceContext*);
            typedef std::int32_t (*InitGroupFunction)(MeyerCaptureGroupInfo*);
            typedef std::int32_t (*DefaultProfileFunction)(std::int32_t, std::int32_t, MeyerCaptureProfileConfig*);
            typedef std::int32_t (*ConfigureFunction)(MeyerCaptureProcessingHandle, const MeyerCaptureProfileConfig*, const MeyerCaptureDeviceContext*);
            typedef std::int32_t (*ResetFunction)(MeyerCaptureProcessingHandle);
            typedef std::int32_t (*PushFunction)(MeyerCaptureProcessingHandle, const unsigned char*, std::size_t);
            typedef std::int32_t (*AbortFunction)(MeyerCaptureProcessingHandle);
            typedef std::int32_t (*CopyFunction)(MeyerCaptureProcessingHandle, unsigned char*, std::size_t, std::size_t*, MeyerCaptureGroupInfo*);
            typedef std::int32_t (*SlowFunction)(const MeyerCaptureProfileConfig*, const unsigned char*, std::size_t, const MeyerCaptureGroupInfo*, unsigned char*, std::size_t, std::size_t*, MeyerCaptureGroupInfo*);
            typedef std::int32_t (*LastErrorFunction)(MeyerCaptureProcessingHandle, char*, std::size_t, std::size_t*);

            AbiFunction abi;
            CreateFunction create;
            DestroyFunction destroy;
            InitProfileFunction initProfile;
            InitContextFunction initContext;
            InitGroupFunction initGroup;
            DefaultProfileFunction defaultProfile;
            ConfigureFunction configure;
            ResetFunction reset;
            PushFunction push;
            AbortFunction abort;
            CopyFunction copy;
            SlowFunction slow;
            LastErrorFunction lastError;

            Functions()
                : abi(nullptr), create(nullptr), destroy(nullptr), initProfile(nullptr),
                  initContext(nullptr), initGroup(nullptr), defaultProfile(nullptr),
                  configure(nullptr), reset(nullptr), push(nullptr), abort(nullptr),
                  copy(nullptr), slow(nullptr), lastError(nullptr)
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

        CaptureProcessingLibrary::CaptureProcessingLibrary()
            : m_module(nullptr), m_handle(nullptr), m_functions(new Functions())
        {
        }

        CaptureProcessingLibrary::~CaptureProcessingLibrary()
        {
            Unload();
            delete m_functions;
            m_functions = nullptr;
        }

        std::int32_t CaptureProcessingLibrary::Load(const std::string& pathUtf8)
        {
            if (m_module != nullptr && m_handle != nullptr)
            {
                return MeyerCaptureProcessingResult_Ok;
            }
            std::wstring path;
            if (!Utf8ToWide(pathUtf8.c_str(), path) || !IsAbsoluteWindowsPath(path))
            {
                m_lastError = "CaptureProcessing DLL path must be an absolute UTF-8 path";
                return MeyerCaptureProcessingResult_InvalidArgument;
            }
            HMODULE module = ::LoadLibraryW(path.c_str());
            if (module == nullptr)
            {
                std::ostringstream message;
                message << "Failed to load CaptureProcessing DLL; Win32 error="
                        << static_cast<unsigned long>(::GetLastError());
                m_lastError = message.str();
                return MeyerCaptureProcessingResult_InternalError;
            }
            m_module = module;
            *m_functions = Functions();
            m_functions->abi = Resolve<Functions::AbiFunction>(module, "GetMeyerModuleApiVersion");
            if (m_functions->abi == nullptr ||
                m_functions->abi() != MEYER_CAPTURE_PROCESSING_API_VERSION)
            {
                m_lastError = "CaptureProcessing ABI version is missing or incompatible";
                Unload();
                return MeyerCaptureProcessingResult_InternalError;
            }
            m_functions->create = Resolve<Functions::CreateFunction>(module, "MeyerCaptureProcessing_Create");
            m_functions->destroy = Resolve<Functions::DestroyFunction>(module, "MeyerCaptureProcessing_Destroy");
            m_functions->initProfile = Resolve<Functions::InitProfileFunction>(module, "MeyerCaptureProcessing_InitProfile");
            m_functions->initContext = Resolve<Functions::InitContextFunction>(module, "MeyerCaptureProcessing_InitDeviceContext");
            m_functions->initGroup = Resolve<Functions::InitGroupFunction>(module, "MeyerCaptureProcessing_InitGroupInfo");
            m_functions->defaultProfile = Resolve<Functions::DefaultProfileFunction>(module, "MeyerCaptureProcessing_GetDefaultProfile");
            m_functions->configure = Resolve<Functions::ConfigureFunction>(module, "MeyerCaptureProcessing_Configure");
            m_functions->reset = Resolve<Functions::ResetFunction>(module, "MeyerCaptureProcessing_Reset");
            m_functions->push = Resolve<Functions::PushFunction>(module, "MeyerCaptureProcessing_PushPacket");
            m_functions->abort = Resolve<Functions::AbortFunction>(module, "MeyerCaptureProcessing_AbortIncompleteGroup");
            m_functions->copy = Resolve<Functions::CopyFunction>(module, "MeyerCaptureProcessing_CopyCompletedGroup");
            m_functions->slow = Resolve<Functions::SlowFunction>(module, "MeyerCaptureProcessing_ProcessSlowGroup");
            m_functions->lastError = Resolve<Functions::LastErrorFunction>(module, "MeyerCaptureProcessing_GetLastError");
            if (m_functions->create == nullptr || m_functions->destroy == nullptr ||
                m_functions->initProfile == nullptr || m_functions->initContext == nullptr ||
                m_functions->initGroup == nullptr || m_functions->defaultProfile == nullptr ||
                m_functions->configure == nullptr || m_functions->reset == nullptr ||
                m_functions->push == nullptr || m_functions->abort == nullptr ||
                m_functions->copy == nullptr || m_functions->slow == nullptr ||
                m_functions->lastError == nullptr)
            {
                m_lastError = "CaptureProcessing is missing one or more required API exports";
                Unload();
                return MeyerCaptureProcessingResult_InternalError;
            }
            m_handle = m_functions->create();
            if (m_handle == nullptr)
            {
                m_lastError = "CaptureProcessing failed to create a session handle";
                Unload();
                return MeyerCaptureProcessingResult_InternalError;
            }
            m_lastError.clear();
            return MeyerCaptureProcessingResult_Ok;
        }

        std::int32_t CaptureProcessingLibrary::CreateSession()
        {
            if (m_handle != nullptr)
            {
                return MeyerCaptureProcessingResult_Ok;
            }
            m_lastError = "CaptureProcessing session is not loaded";
            return MeyerCaptureProcessingResult_InvalidState;
        }

        void CaptureProcessingLibrary::DestroySession()
        {
            if (m_handle != nullptr && m_functions->destroy != nullptr)
            {
                m_functions->destroy(m_handle);
                m_handle = nullptr;
            }
        }

        std::int32_t CaptureProcessingLibrary::GetDefaultProfile(
            std::int32_t deviceProfile, std::int32_t captureMode,
            MeyerCaptureProfileConfig& profile)
        {
            m_functions->initProfile(&profile);
            return m_functions->defaultProfile(deviceProfile, captureMode, &profile);
        }

        std::int32_t CaptureProcessingLibrary::InitDeviceContext(
            MeyerCaptureDeviceContext& context)
        {
            return m_functions->initContext(&context);
        }

        std::int32_t CaptureProcessingLibrary::InitGroupInfo(
            MeyerCaptureGroupInfo& info)
        {
            return m_functions->initGroup(&info);
        }

        std::int32_t CaptureProcessingLibrary::Configure(
            const MeyerCaptureProfileConfig& profile,
            const MeyerCaptureDeviceContext& context)
        {
            const std::int32_t result = m_functions->configure(m_handle, &profile, &context);
            if (result != MeyerCaptureProcessingResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureProcessingLibrary::Reset()
        {
            const std::int32_t result = m_functions->reset(m_handle);
            if (result != MeyerCaptureProcessingResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureProcessingLibrary::PushPacket(
            const unsigned char* packet, std::size_t bytes)
        {
            const std::int32_t result = m_functions->push(m_handle, packet, bytes);
            if (result < 0)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureProcessingLibrary::AbortIncompleteGroup()
        {
            return m_functions->abort(m_handle);
        }

        std::int32_t CaptureProcessingLibrary::CopyCompletedGroup(
            unsigned char* buffer, std::size_t capacity, std::size_t& required,
            MeyerCaptureGroupInfo& info)
        {
            required = 0U;
            const std::int32_t result = m_functions->copy(
                m_handle, buffer, capacity, &required, &info);
            if (result < 0 && result != MeyerCaptureProcessingResult_NoCompletedGroup)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureProcessingLibrary::ProcessSlowGroup(
            const MeyerCaptureProfileConfig& profile,
            const unsigned char* input, std::size_t inputBytes,
            const MeyerCaptureGroupInfo& inputInfo,
            unsigned char* output, std::size_t capacity, std::size_t& required,
            MeyerCaptureGroupInfo& outputInfo)
        {
            required = 0U;
            const std::int32_t result = m_functions->slow(
                &profile, input, inputBytes, &inputInfo, output, capacity,
                &required, &outputInfo);
            if (result != MeyerCaptureProcessingResult_Ok)
            {
                m_lastError = "CaptureProcessing slow group processing failed";
            }
            return result;
        }

        const std::string& CaptureProcessingLibrary::LastError() const
        {
            return m_lastError;
        }

        void CaptureProcessingLibrary::ReadLastError()
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
                MeyerCaptureProcessingResult_Ok)
            {
                m_lastError.assign(buffer.c_str());
            }
        }

        void CaptureProcessingLibrary::Unload()
        {
            DestroySession();
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
