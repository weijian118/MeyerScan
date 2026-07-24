// =============================================================================
// 文件: DeviceCmdLibrary.cpp
// 作用: 实现 DeviceCmd DLL 的显式动态加载、ABI 检查和调用转发。
// =============================================================================
#include "DeviceCmdLibrary.h"

#include "../support/PathUtils.h"

#include <windows.h>

#include <cstring>
#include <sstream>

namespace meyer
{
    namespace captureservice
    {
        struct DeviceCmdLibrary::Functions
        {
            typedef std::int32_t (*GetAbiVersionFunction)();
            typedef std::int32_t (*InitOpenParamsFunction)(MeyerDeviceCmdOpenParams*);
            typedef std::int32_t (*InitCaptureParamsFunction)(std::int32_t, MeyerDeviceCmdCaptureParams*);
            typedef std::int32_t (*InitPreflightFunction)(MeyerDeviceCalibrationPreflight*);
            typedef std::int32_t (*InitStateFunction)(MeyerDeviceStateSnapshot*);
            typedef std::int32_t (*InitDiagnosticsFunction)(MeyerDeviceCmdStreamDiagnostics*);
            typedef MeyerDeviceCmdHandle (*CreateFunction)();
            typedef void (*DestroyFunction)(MeyerDeviceCmdHandle);
            typedef std::int32_t (*OpenFunction)(MeyerDeviceCmdHandle, const MeyerDeviceCmdOpenParams*);
            typedef std::int32_t (*CloseFunction)(MeyerDeviceCmdHandle);
            typedef std::int32_t (*PrepareFunction)(MeyerDeviceCmdHandle, const MeyerDeviceCmdOpenParams*, MeyerDeviceCalibrationPreflight*);
            typedef std::int32_t (*StateFunction)(MeyerDeviceCmdHandle, MeyerDeviceStateSnapshot*);
            typedef std::int32_t (*LightweightFunction)(MeyerDeviceCmdHandle);
            typedef std::int32_t (*StartRawFunction)(MeyerDeviceCmdHandle, const MeyerDeviceCmdCaptureParams*);
            typedef std::int32_t (*StopRawFunction)(MeyerDeviceCmdHandle, std::int32_t);
            typedef std::int32_t (*ReceiveRawFunction)(MeyerDeviceCmdHandle, unsigned char*, std::size_t, std::size_t*, std::uint32_t);
            typedef std::int32_t (*DiagnosticsFunction)(MeyerDeviceCmdHandle, MeyerDeviceCmdStreamDiagnostics*);
            typedef std::int32_t (*LightFunction)(MeyerDeviceCmdHandle, std::int32_t);
            typedef std::int32_t (*ExposureFunction)(MeyerDeviceCmdHandle, const MeyerDeviceCmdExposureParameters*);
            typedef std::int32_t (*LastErrorFunction)(MeyerDeviceCmdHandle, char*, std::size_t, std::size_t*);

            GetAbiVersionFunction getAbiVersion;
            InitOpenParamsFunction initOpenParams;
            InitCaptureParamsFunction initCaptureParams;
            InitPreflightFunction initPreflight;
            InitStateFunction initState;
            InitDiagnosticsFunction initDiagnostics;
            CreateFunction create;
            DestroyFunction destroy;
            OpenFunction open;
            CloseFunction close;
            PrepareFunction prepare;
            StateFunction getState;
            LightweightFunction lightweight;
            StartRawFunction startRaw;
            StopRawFunction stopRaw;
            ReceiveRawFunction receiveRaw;
            DiagnosticsFunction diagnostics;
            LightFunction setLight;
            ExposureFunction setExposure;
            LastErrorFunction lastError;

            Functions()
                : getAbiVersion(nullptr), initOpenParams(nullptr), initCaptureParams(nullptr),
                  initPreflight(nullptr), initState(nullptr), initDiagnostics(nullptr),
                  create(nullptr), destroy(nullptr), open(nullptr), close(nullptr),
                  prepare(nullptr), getState(nullptr), lightweight(nullptr),
                  startRaw(nullptr), stopRaw(nullptr), receiveRaw(nullptr),
                  diagnostics(nullptr), setLight(nullptr), setExposure(nullptr),
                  lastError(nullptr)
            {
            }
        };

        namespace
        {
            // 用模板把 GetProcAddress 的强制转换集中在一个位置，避免调用点重复。
            template<typename T>
            T Resolve(HMODULE module, const char* name)
            {
                return reinterpret_cast<T>(::GetProcAddress(module, name));
            }
        }

        DeviceCmdLibrary::DeviceCmdLibrary()
            : m_module(nullptr), m_handle(nullptr), m_functions(new Functions())
        {
        }

        DeviceCmdLibrary::~DeviceCmdLibrary()
        {
            Unload();
            delete m_functions;
            m_functions = nullptr;
        }

        // DLL 加载成功后立即校验整数 ABI，再解析实际使用的全部导出函数。
        std::int32_t DeviceCmdLibrary::Load(const std::string& pathUtf8)
        {
            if (m_module != nullptr && m_handle != nullptr)
            {
                return MeyerDeviceCmdResult_Ok;
            }

            std::wstring path;
            if (!Utf8ToWide(pathUtf8.c_str(), path) || !IsAbsoluteWindowsPath(path))
            {
                m_lastError = "DeviceCmd DLL path must be an absolute UTF-8 path";
                return MeyerDeviceCmdResult_InvalidArgument;
            }

            HMODULE module = ::LoadLibraryW(path.c_str());
            if (module == nullptr)
            {
                std::ostringstream message;
                message << "Failed to load DeviceCmd DLL; Win32 error="
                        << static_cast<unsigned long>(::GetLastError());
                m_lastError = message.str();
                return MeyerDeviceCmdResult_TransportLoadFailed;
            }
            m_module = module;
            *m_functions = Functions();
            m_functions->getAbiVersion = Resolve<Functions::GetAbiVersionFunction>(module, "GetMeyerModuleApiVersion");
            if (m_functions->getAbiVersion == nullptr ||
                m_functions->getAbiVersion() != MEYER_DEVICE_CMD_API_VERSION)
            {
                m_lastError = "DeviceCmd ABI version is missing or incompatible";
                Unload();
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            m_functions->initOpenParams = Resolve<Functions::InitOpenParamsFunction>(module, "MeyerDeviceCmd_InitOpenParams");
            m_functions->initCaptureParams = Resolve<Functions::InitCaptureParamsFunction>(module, "MeyerDeviceCmd_InitCaptureParamsForModel");
            m_functions->initPreflight = Resolve<Functions::InitPreflightFunction>(module, "MeyerDeviceCmd_InitCalibrationPreflight");
            m_functions->initState = Resolve<Functions::InitStateFunction>(module, "MeyerDeviceCmd_InitStateSnapshot");
            m_functions->initDiagnostics = Resolve<Functions::InitDiagnosticsFunction>(module, "MeyerDeviceCmd_InitStreamDiagnostics");
            m_functions->create = Resolve<Functions::CreateFunction>(module, "MeyerDeviceCmd_Create");
            m_functions->destroy = Resolve<Functions::DestroyFunction>(module, "MeyerDeviceCmd_Destroy");
            m_functions->open = Resolve<Functions::OpenFunction>(module, "MeyerDeviceCmd_Open");
            m_functions->close = Resolve<Functions::CloseFunction>(module, "MeyerDeviceCmd_Close");
            m_functions->prepare = Resolve<Functions::PrepareFunction>(module, "MeyerDeviceCmd_PrepareColorCalibration");
            m_functions->getState = Resolve<Functions::StateFunction>(module, "MeyerDeviceCmd_GetStateSnapshot");
            m_functions->lightweight = Resolve<Functions::LightweightFunction>(module, "MeyerDeviceCmd_IsDeviceConnectedLightweight");
            m_functions->startRaw = Resolve<Functions::StartRawFunction>(module, "MeyerDeviceCmd_StartRawCapture");
            m_functions->stopRaw = Resolve<Functions::StopRawFunction>(module, "MeyerDeviceCmd_StopRawCapture");
            m_functions->receiveRaw = Resolve<Functions::ReceiveRawFunction>(module, "MeyerDeviceCmd_ReceiveRawCapturePacket");
            m_functions->diagnostics = Resolve<Functions::DiagnosticsFunction>(module, "MeyerDeviceCmd_GetStreamDiagnostics");
            m_functions->setLight = Resolve<Functions::LightFunction>(module, "MeyerDeviceCmd_SetLight");
            m_functions->setExposure = Resolve<Functions::ExposureFunction>(module, "MeyerDeviceCmd_SetExposureParameters");
            m_functions->lastError = Resolve<Functions::LastErrorFunction>(module, "MeyerDeviceCmd_GetLastError");

            if (m_functions->initOpenParams == nullptr ||
                m_functions->initCaptureParams == nullptr ||
                m_functions->initPreflight == nullptr ||
                m_functions->initState == nullptr ||
                m_functions->initDiagnostics == nullptr ||
                m_functions->create == nullptr || m_functions->destroy == nullptr ||
                m_functions->open == nullptr || m_functions->close == nullptr ||
                m_functions->prepare == nullptr || m_functions->getState == nullptr ||
                m_functions->lightweight == nullptr || m_functions->startRaw == nullptr ||
                m_functions->stopRaw == nullptr || m_functions->receiveRaw == nullptr ||
                m_functions->diagnostics == nullptr || m_functions->setLight == nullptr ||
                m_functions->setExposure == nullptr ||
                m_functions->lastError == nullptr)
            {
                m_lastError = "DeviceCmd is missing one or more required API exports";
                Unload();
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            m_handle = m_functions->create();
            if (m_handle == nullptr)
            {
                m_lastError = "DeviceCmd failed to create a session handle";
                Unload();
                return MeyerDeviceCmdResult_InternalError;
            }
            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        std::int32_t DeviceCmdLibrary::Open(const MeyerDeviceCmdOpenParams& params)
        {
            if (m_handle == nullptr || m_functions->open == nullptr)
            {
                m_lastError = "DeviceCmd is not loaded";
                return MeyerDeviceCmdResult_NotOpen;
            }
            const std::int32_t result = m_functions->open(m_handle, &params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        // 不在 CaptureService 中复制 DeviceCmd 的默认值，避免两个模块的
        // 超时、VID/PID 和 schema 规则逐渐分叉。
        std::int32_t DeviceCmdLibrary::InitOpenParams(
            MeyerDeviceCmdOpenParams& params)
        {
            return m_functions->initOpenParams(&params);
        }

        std::int32_t DeviceCmdLibrary::InitCalibrationPreflight(
            MeyerDeviceCalibrationPreflight& preflight)
        {
            return m_functions->initPreflight(&preflight);
        }

        std::int32_t DeviceCmdLibrary::InitStateSnapshot(
            MeyerDeviceStateSnapshot& snapshot)
        {
            return m_functions->initState(&snapshot);
        }

        std::int32_t DeviceCmdLibrary::InitStreamDiagnostics(
            MeyerDeviceCmdStreamDiagnostics& diagnostics)
        {
            return m_functions->initDiagnostics(&diagnostics);
        }

        std::int32_t DeviceCmdLibrary::PrepareColorCalibration(
            const MeyerDeviceCmdOpenParams& params,
            MeyerDeviceCalibrationPreflight& preflight)
        {
            if (m_handle == nullptr)
            {
                m_lastError = "DeviceCmd is not loaded";
                return MeyerDeviceCmdResult_NotOpen;
            }
            const std::int32_t result = m_functions->prepare(m_handle, &params, &preflight);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::Close()
        {
            if (m_handle == nullptr || m_functions->close == nullptr)
            {
                return MeyerDeviceCmdResult_Ok;
            }
            const std::int32_t result = m_functions->close(m_handle);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::GetStateSnapshot(MeyerDeviceStateSnapshot& snapshot)
        {
            return m_functions->getState(m_handle, &snapshot);
        }

        std::int32_t DeviceCmdLibrary::IsDeviceConnectedLightweight()
        {
            return m_functions->lightweight(m_handle);
        }

        std::int32_t DeviceCmdLibrary::InitCaptureParamsForModel(
            std::int32_t model, MeyerDeviceCmdCaptureParams& params)
        {
            const std::int32_t result = m_functions->initCaptureParams(model, &params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::StartRawCapture(
            const MeyerDeviceCmdCaptureParams& params)
        {
            const std::int32_t result = m_functions->startRaw(m_handle, &params);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::StopRawCapture(bool turnLightOff)
        {
            const std::int32_t result = m_functions->stopRaw(m_handle, turnLightOff ? 1 : 0);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::ReceiveRawCapturePacket(
            unsigned char* buffer, std::size_t capacity,
            std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            receivedSize = 0U;
            const std::int32_t result = m_functions->receiveRaw(
                m_handle, buffer, capacity, &receivedSize, timeoutMs);
            if (result != MeyerDeviceCmdResult_Ok &&
                result != MeyerDeviceCmdResult_Timeout)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::GetStreamDiagnostics(
            MeyerDeviceCmdStreamDiagnostics& diagnostics)
        {
            const std::int32_t result = m_functions->diagnostics(m_handle, &diagnostics);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::SetLight(bool on)
        {
            const std::int32_t result = m_functions->setLight(m_handle, on ? 1 : 0);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        std::int32_t DeviceCmdLibrary::SetExposureParameters(
            const MeyerDeviceCmdExposureParameters& parameters)
        {
            const std::int32_t result = m_functions->setExposure(m_handle, &parameters);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                ReadLastError();
            }
            return result;
        }

        const std::string& DeviceCmdLibrary::LastError() const
        {
            return m_lastError;
        }

        bool DeviceCmdLibrary::IsLoaded() const
        {
            return m_module != nullptr && m_handle != nullptr;
        }

        bool DeviceCmdLibrary::IsOpen() const
        {
            return IsLoaded() && m_functions->lightweight(m_handle) == 1;
        }

        void DeviceCmdLibrary::ReadLastError()
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
                MeyerDeviceCmdResult_Ok)
            {
                m_lastError.assign(buffer.c_str());
            }
        }

        void DeviceCmdLibrary::Unload()
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
