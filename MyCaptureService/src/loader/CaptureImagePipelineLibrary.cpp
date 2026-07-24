// =============================================================================
// 文件: CaptureImagePipelineLibrary.cpp
// 作用: 实现场景级图像流水线 DLL 的显式加载、ABI 门禁和调用转发。
// =============================================================================
#include "CaptureImagePipelineLibrary.h"

#include "../support/PathUtils.h"

#include <windows.h>

#include <sstream>

namespace meyer
{
    namespace captureservice
    {
        struct CaptureImagePipelineLibrary::Functions
        {
            typedef std::int32_t (*AbiFunction)();
            typedef std::int32_t (*InitOptionsFunction)(MeyerCapturePipelineOptions*);
            typedef std::int32_t (*InitInfoFunction)(MeyerCapturePipelineOutputInfo*);
            typedef MeyerCaptureImagePipelineHandle (*CreateFunction)();
            typedef void (*DestroyFunction)(MeyerCaptureImagePipelineHandle);
            typedef std::int32_t (*ConfigureFunction)(
                MeyerCaptureImagePipelineHandle,
                const MeyerCaptureProfileConfig*,
                const MeyerCaptureDeviceContext*);
            typedef std::int32_t (*ProcessFunction)(
                MeyerCaptureImagePipelineHandle,
                const unsigned char*, std::size_t,
                const MeyerCaptureGroupInfo*,
                const MeyerCapturePipelineOptions*);
            typedef std::int32_t (*InfoFunction)(
                MeyerCaptureImagePipelineHandle,
                std::int32_t, MeyerCapturePipelineOutputInfo*);
            typedef std::int32_t (*CopyFunction)(
                MeyerCaptureImagePipelineHandle,
                std::int32_t, unsigned char*, std::size_t, std::size_t*);
            typedef std::int32_t (*LastErrorFunction)(
                MeyerCaptureImagePipelineHandle, char*, std::size_t, std::size_t*);

            AbiFunction abi;
            InitOptionsFunction initOptions;
            InitInfoFunction initInfo;
            CreateFunction create;
            DestroyFunction destroy;
            ConfigureFunction configure;
            ProcessFunction process;
            InfoFunction info;
            CopyFunction copy;
            LastErrorFunction lastError;

            Functions()
                : abi(nullptr), initOptions(nullptr), initInfo(nullptr),
                  create(nullptr), destroy(nullptr), configure(nullptr),
                  process(nullptr), info(nullptr), copy(nullptr), lastError(nullptr)
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

        CaptureImagePipelineLibrary::CaptureImagePipelineLibrary()
            : m_module(nullptr), m_handle(nullptr), m_functions(new Functions())
        {
        }

        CaptureImagePipelineLibrary::~CaptureImagePipelineLibrary()
        {
            Unload();
            delete m_functions;
            m_functions = nullptr;
        }

        std::int32_t CaptureImagePipelineLibrary::Load(
            const std::string& pathUtf8)
        {
            if (m_module != nullptr && m_handle != nullptr)
            {
                return MeyerCaptureImagePipelineResult_Ok;
            }
            std::wstring path;
            if (!Utf8ToWide(pathUtf8.c_str(), path) || !IsAbsoluteWindowsPath(path))
            {
                m_lastError = "CaptureImagePipeline DLL path must be absolute UTF-8";
                return MeyerCaptureImagePipelineResult_InvalidArgument;
            }

            HMODULE module = ::LoadLibraryW(path.c_str());
            if (module == nullptr)
            {
                std::ostringstream text;
                text << "Failed to load CaptureImagePipeline DLL; Win32 error="
                     << static_cast<unsigned long>(::GetLastError());
                m_lastError = text.str();
                return MeyerCaptureImagePipelineResult_InternalError;
            }
            m_module = module;
            *m_functions = Functions();
            m_functions->abi = Resolve<Functions::AbiFunction>(
                module, "GetMeyerModuleApiVersion");
            m_functions->initOptions = Resolve<Functions::InitOptionsFunction>(
                module, "MeyerCaptureImagePipeline_InitOptions");
            m_functions->initInfo = Resolve<Functions::InitInfoFunction>(
                module, "MeyerCaptureImagePipeline_InitOutputInfo");
            m_functions->create = Resolve<Functions::CreateFunction>(
                module, "MeyerCaptureImagePipeline_Create");
            m_functions->destroy = Resolve<Functions::DestroyFunction>(
                module, "MeyerCaptureImagePipeline_Destroy");
            m_functions->configure = Resolve<Functions::ConfigureFunction>(
                module, "MeyerCaptureImagePipeline_Configure");
            m_functions->process = Resolve<Functions::ProcessFunction>(
                module, "MeyerCaptureImagePipeline_ProcessGroup");
            m_functions->info = Resolve<Functions::InfoFunction>(
                module, "MeyerCaptureImagePipeline_GetOutputInfo");
            m_functions->copy = Resolve<Functions::CopyFunction>(
                module, "MeyerCaptureImagePipeline_CopyOutput");
            m_functions->lastError = Resolve<Functions::LastErrorFunction>(
                module, "MeyerCaptureImagePipeline_GetLastError");
            if (m_functions->abi == nullptr ||
                m_functions->abi() != MEYER_CAPTURE_IMAGE_PIPELINE_API_VERSION ||
                m_functions->initOptions == nullptr || m_functions->initInfo == nullptr ||
                m_functions->create == nullptr || m_functions->destroy == nullptr ||
                m_functions->configure == nullptr || m_functions->process == nullptr ||
                m_functions->info == nullptr || m_functions->copy == nullptr ||
                m_functions->lastError == nullptr)
            {
                m_lastError = "CaptureImagePipeline API is missing or incompatible";
                Unload();
                return MeyerCaptureImagePipelineResult_InternalError;
            }
            m_handle = m_functions->create();
            if (m_handle == nullptr)
            {
                m_lastError = "CaptureImagePipeline failed to create a session";
                Unload();
                return MeyerCaptureImagePipelineResult_InternalError;
            }
            m_lastError.clear();
            return MeyerCaptureImagePipelineResult_Ok;
        }

        std::int32_t CaptureImagePipelineLibrary::InitOptions(
            MeyerCapturePipelineOptions& options)
        {
            return m_functions->initOptions(&options);
        }

        std::int32_t CaptureImagePipelineLibrary::InitOutputInfo(
            MeyerCapturePipelineOutputInfo& info)
        {
            return m_functions->initInfo(&info);
        }

        std::int32_t CaptureImagePipelineLibrary::Configure(
            const MeyerCaptureProfileConfig& profile,
            const MeyerCaptureDeviceContext& context)
        {
            const std::int32_t result = m_functions->configure(
                m_handle, &profile, &context);
            if (result != MeyerCaptureImagePipelineResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureImagePipelineLibrary::ProcessGroup(
            const unsigned char* normalizedGroup,
            std::size_t normalizedBytes,
            const MeyerCaptureGroupInfo& groupInfo,
            const MeyerCapturePipelineOptions& options)
        {
            const std::int32_t result = m_functions->process(
                m_handle, normalizedGroup, normalizedBytes, &groupInfo, &options);
            if (result != MeyerCaptureImagePipelineResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureImagePipelineLibrary::GetOutputInfo(
            std::int32_t outputType,
            MeyerCapturePipelineOutputInfo& info)
        {
            const std::int32_t result = m_functions->info(
                m_handle, outputType, &info);
            if (result != MeyerCaptureImagePipelineResult_Ok &&
                result != MeyerCaptureImagePipelineResult_NoData)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t CaptureImagePipelineLibrary::CopyOutput(
            std::int32_t outputType,
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& requiredBytes)
        {
            requiredBytes = 0U;
            const std::int32_t result = m_functions->copy(
                m_handle, outputType, buffer, capacity, &requiredBytes);
            if (result != MeyerCaptureImagePipelineResult_Ok &&
                result != MeyerCaptureImagePipelineResult_BufferTooSmall &&
                result != MeyerCaptureImagePipelineResult_NoData)
            {
                ReadLastError();
            }
            return result;
        }

        const std::string& CaptureImagePipelineLibrary::LastError() const
        {
            return m_lastError;
        }

        void CaptureImagePipelineLibrary::ReadLastError()
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
            if (m_functions->lastError(
                    m_handle, &buffer[0], buffer.size(), &required) ==
                MeyerCaptureImagePipelineResult_Ok)
            {
                m_lastError.assign(buffer.c_str());
            }
        }

        void CaptureImagePipelineLibrary::Unload()
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
