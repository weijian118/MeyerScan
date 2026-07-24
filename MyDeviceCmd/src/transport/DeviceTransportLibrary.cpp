// =============================================================================
// 文件: DeviceTransportLibrary.cpp
// 作用: 通过绝对路径动态加载 DeviceTransport，并把其 C ABI 适配为内部接口。
// =============================================================================
#include "DeviceTransportLibrary.h"

#include "DeviceTransportAbi.h"

#include <windows.h>

#include <cstring>
#include <sstream>
#include <vector>

namespace
{
    // 判断 Windows 绝对路径，避免为加载 DLL 引入额外的 Shell/Path 依赖。
    bool IsAbsoluteWindowsPath(const std::wstring& path)
    {
        if (path.size() >= 3U &&
            ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
            path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
        {
            return true;
        }
        return path.size() >= 2U && path[0] == L'\\' && path[1] == L'\\';
    }

    // 把 UTF-8 路径转换为 Win32 宽字符路径，不依赖 Qt QString。
    bool Utf8ToWide(const char* text, std::wstring& output)
    {
        output.clear();
        if (text == nullptr || text[0] == '\0')
        {
            return false;
        }

        const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
        if (required <= 1)
        {
            return false;
        }

        std::vector<wchar_t> buffer(static_cast<std::size_t>(required), L'\0');
        if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, &buffer[0], required) <= 0)
        {
            return false;
        }

        output.assign(&buffer[0]);
        return true;
    }

    // 统一解析函数地址，减少每个 GetProcAddress 的重复转换代码。
    template<typename FunctionType>
    FunctionType Resolve(void* module, const char* name)
    {
        return reinterpret_cast<FunctionType>(
            ::GetProcAddress(static_cast<HMODULE>(module), name));
    }
}

namespace meyer
{
    namespace devicecmd
    {
        using namespace transportabi;

        // 函数表完全留在 cpp 中，公共/私有头文件都不暴露 Windows 函数指针细节。
        struct DeviceTransportLibrary::Functions
        {
            typedef std::int32_t (*GetApiVersionFunction)();
            typedef std::int32_t (*InitOpenParamsFunction)(OpenParams*);
            typedef std::int32_t (*InitCaptureParamsFunction)(CaptureParams*);
            typedef std::int32_t (*InitFrameInfoFunction)(FrameInfo*);
            typedef std::int32_t (*InitStreamDiagnosticsFunction)(StreamDiagnostics*);
            typedef Handle (*CreateFunction)();
            typedef void (*DestroyFunction)(Handle);
            typedef std::int32_t (*OpenFunction)(Handle, const OpenParams*);
            typedef std::int32_t (*SimpleHandleFunction)(Handle);
            typedef std::int32_t (*SendCommandFunction)(Handle, const unsigned char*, std::size_t, std::uint32_t);
            typedef std::int32_t (*ReceiveCommandFunction)(Handle, unsigned char*, std::size_t, std::size_t*, std::uint32_t);
            typedef std::int32_t (*GetIntegerFunction)(Handle, std::int32_t*);
            typedef std::int32_t (*SetIntegerFunction)(Handle, std::int32_t);
            typedef std::int32_t (*StartCaptureFunction)(Handle, const CaptureParams*);
            typedef std::int32_t (*GetFrameFunction)(Handle, unsigned char*, std::size_t, std::size_t*, FrameInfo*);
            typedef std::int32_t (*PrimeStreamFunction)(Handle, std::size_t, std::size_t);
            typedef std::int32_t (*ReceiveStreamPacketFunction)(Handle, unsigned char*, std::size_t, std::size_t*, std::uint32_t);
            typedef std::int32_t (*GetStreamDiagnosticsFunction)(Handle, StreamDiagnostics*);
            typedef std::int32_t (*GetLastErrorFunction)(Handle, char*, std::size_t, std::size_t*);

            GetApiVersionFunction getApiVersion;
            InitOpenParamsFunction initOpenParams;
            InitCaptureParamsFunction initCaptureParams;
            InitFrameInfoFunction initFrameInfo;
            InitStreamDiagnosticsFunction initStreamDiagnostics;
            CreateFunction create;
            DestroyFunction destroy;
            OpenFunction open;
            SimpleHandleFunction close;
            SimpleHandleFunction isOpen;
            SimpleHandleFunction reconnect;
            SendCommandFunction sendCommand;
            ReceiveCommandFunction receiveCommand;
            GetIntegerFunction getDeviceCount;
            GetIntegerFunction getIsUsb2;
            SetIntegerFunction setDeviceType;
            SetIntegerFunction setPictureOrderMode;
            SetIntegerFunction setCaptureScanMode;
            SetIntegerFunction setAhrsEnabled;
            SetIntegerFunction setScanHeadType;
            StartCaptureFunction startCapture;
            SimpleHandleFunction stopCapture;
            SimpleHandleFunction isCaptureActive;
            GetFrameFunction getFrame;
            SimpleHandleFunction startStream;
            PrimeStreamFunction primeStream;
            SimpleHandleFunction stopStream;
            ReceiveStreamPacketFunction receiveStreamPacket;
            GetStreamDiagnosticsFunction getStreamDiagnostics;
            GetLastErrorFunction getLastError;

            // 值初始化把全部函数指针清零，便于统一检查缺失导出。
            Functions()
                : getApiVersion(nullptr), initOpenParams(nullptr), initCaptureParams(nullptr),
                  initFrameInfo(nullptr), initStreamDiagnostics(nullptr), create(nullptr), destroy(nullptr), open(nullptr),
                  close(nullptr), isOpen(nullptr), reconnect(nullptr), sendCommand(nullptr),
                  receiveCommand(nullptr), getDeviceCount(nullptr), getIsUsb2(nullptr),
                  setDeviceType(nullptr), setPictureOrderMode(nullptr), setCaptureScanMode(nullptr),
                  setAhrsEnabled(nullptr), setScanHeadType(nullptr), startCapture(nullptr),
                  stopCapture(nullptr), isCaptureActive(nullptr), getFrame(nullptr),
                  startStream(nullptr), primeStream(nullptr), stopStream(nullptr),
                  receiveStreamPacket(nullptr), getStreamDiagnostics(nullptr),
                  getLastError(nullptr)
            {
            }
        };

        // 只创建空函数表，实际 LoadLibrary 延迟到宿主调用 Open 时执行。
        DeviceTransportLibrary::DeviceTransportLibrary()
            : m_module(nullptr)
            , m_handle(nullptr)
            , m_functions(new Functions())
            , m_rawCaptureActive(false)
        {
        }

        // 析构顺序必须是关闭/销毁会话后再 FreeLibrary，否则函数指针会悬空。
        DeviceTransportLibrary::~DeviceTransportLibrary()
        {
            Unload();
        }

        // 加载后端并把 DeviceCmd 打开参数转换为 DeviceTransport API v1 结构。
        // 把 DeviceCmd 的公共参数映射到私有 ABI，并在完成导出校验后打开设备。
        std::int32_t DeviceTransportLibrary::Open(const MeyerDeviceCmdOpenParams& params)
        {
            const std::int32_t loadResult = Load(params.transportLibraryPathUtf8);
            if (loadResult != MeyerDeviceCmdResult_Ok)
            {
                return loadResult;
            }

            OpenParams transportParams = {};
            if (m_functions->initOpenParams(&transportParams) != Ok)
            {
                m_lastError = "DeviceTransport failed to initialize open parameters";
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            // transportType=1 表示当前正式实现的 CyAPI USB。
            transportParams.transportType = 1;
            transportParams.vendorId = params.vendorId;
            transportParams.productId = params.productId;
            transportParams.deviceIndex = params.deviceIndex;
            transportParams.commandTimeoutMs = params.commandTimeoutMs;
            transportParams.streamTimeoutMs = params.streamTimeoutMs;

            const std::int32_t result = m_functions->open(m_handle, &transportParams);
            return MapResult(result, "Open");
        }

        // 关闭连接但保留 DLL 和句柄，允许同一对象后续 Reconnect/Open。
        // 关闭底层句柄但不卸载 DLL，析构或下一次 Load 再负责最终卸载。
        void DeviceTransportLibrary::Close()
        {
            if (m_handle != nullptr && m_functions && m_functions->close != nullptr)
            {
                m_functions->close(m_handle);
            }
            m_rawCaptureActive = false;
        }

        // 通过动态函数表读取真实连接状态，而不是依赖本地布尔缓存。
        bool DeviceTransportLibrary::IsOpen() const
        {
            return m_handle != nullptr && m_functions && m_functions->isOpen != nullptr &&
                   m_functions->isOpen(m_handle) == 1;
        }

        // 复用底层 Transport 的重连逻辑，错误统一映射到 DeviceCmd 结果域。
        std::int32_t DeviceTransportLibrary::Reconnect()
        {
            if (m_handle == nullptr)
            {
                m_lastError = "DeviceTransport session has not been created";
                return MeyerDeviceCmdResult_NotOpen;
            }
            return MapResult(m_functions->reconnect(m_handle), "Reconnect");
        }

        // 发送前检查编码帧非空，避免把空指针传给跨 DLL 函数。
        std::int32_t DeviceTransportLibrary::SendCommand(const std::vector<std::uint8_t>& frame,
                                                         std::uint32_t timeoutMs)
        {
            if (frame.empty())
            {
                m_lastError = "Encoded command frame is empty";
                return MeyerDeviceCmdResult_InvalidArgument;
            }
            return MapResult(m_functions->sendCommand(m_handle, &frame[0], frame.size(), timeoutMs),
                             "SendCommand");
        }

        // 先分配调用方指定容量，再根据底层返回的实际长度裁剪 vector。
        std::int32_t DeviceTransportLibrary::ReceiveCommand(std::vector<std::uint8_t>& frame,
                                                            std::size_t capacity,
                                                            std::uint32_t timeoutMs)
        {
            frame.assign(capacity, 0U);
            std::size_t receivedSize = 0U;
            const std::int32_t result = m_functions->receiveCommand(
                m_handle, &frame[0], frame.size(), &receivedSize, timeoutMs);
            if (result != Ok)
            {
                frame.clear();
                return MapResult(result, "ReceiveCommand");
            }

            frame.resize(receivedSize);
            return MeyerDeviceCmdResult_Ok;
        }

        // 查询 Transport 枚举结果，DeviceCmd 只负责转发数值和错误。
        std::int32_t DeviceTransportLibrary::GetDeviceCount(std::int32_t& deviceCount)
        {
            return MapResult(m_functions->getDeviceCount(m_handle, &deviceCount), "GetDeviceCount");
        }

        // 查询 USB 速度标志，供状态快照显示和诊断使用。
        std::int32_t DeviceTransportLibrary::GetIsUsb2(std::int32_t& isUsb2)
        {
            return MapResult(m_functions->getIsUsb2(m_handle, &isUsb2), "GetIsUsb2");
        }

        // 依次设置解析模式和采集参数，再启动 DeviceTransport 后台组帧流水线。
        std::int32_t DeviceTransportLibrary::StartCapture(const MeyerDeviceCmdCaptureParams& params,
                                                          std::int32_t transportDecoderType)
        {
            std::int32_t result = m_functions->setDeviceType(m_handle, transportDecoderType);
            if (result == Ok)
            {
                result = m_functions->setPictureOrderMode(m_handle, params.pictureOrderMode);
            }

            // DeviceCmd 公开值为 Idle=0/Scan=1/3D=2/Color=3，Transport 为 Scan=0/3D=1/Color=2。
            const std::int32_t transportScanMode = params.workMode - 1;
            if (result == Ok)
            {
                result = m_functions->setCaptureScanMode(m_handle, transportScanMode);
            }
            if (result == Ok)
            {
                result = m_functions->setAhrsEnabled(m_handle, params.ahrsEnabled != 0 ? 1 : 0);
            }
            if (result == Ok)
            {
                result = m_functions->setScanHeadType(m_handle, params.scanHeadType);
            }
            if (result != Ok)
            {
                return MapResult(result, "ConfigureCapture");
            }

            CaptureParams transportParams = {};
            if (m_functions->initCaptureParams(&transportParams) != Ok)
            {
                m_lastError = "DeviceTransport failed to initialize capture parameters";
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            transportParams.width = params.width;
            transportParams.height = params.height;
            transportParams.imageCount = params.imageCount;
            transportParams.packetsPerImage = params.packetsPerImage;
            transportParams.transferSize = params.transferSize;
            transportParams.queueDepth = params.queueDepth;
            transportParams.packetPayloadSize = params.packetPayloadSize;
            transportParams.lastPacketValidSize = params.lastPacketValidSize;
            transportParams.timeoutMs = params.timeoutMs;
            transportParams.maxReadyFrames = params.maxReadyFrames;
            return MapResult(m_functions->startCapture(m_handle, &transportParams), "StartCapture");
        }

        // 将停止采集请求转发给 Transport；具体命令 0x0B 由上层 DeviceCmd 发送。
        std::int32_t DeviceTransportLibrary::StopCapture()
        {
            return MapResult(m_functions->stopCapture(m_handle), "StopCapture");
        }

        // 返回底层异步流状态，用于保护命令响应和采集释放顺序。
        bool DeviceTransportLibrary::IsCaptureActive() const
        {
            return m_rawCaptureActive ||
                   (m_handle != nullptr && m_functions->isCaptureActive(m_handle) == 1);
        }

        // 将 DeviceTransport 帧元数据复制成 DeviceCmd 自己的稳定公共结构。
        // 非阻塞读取完整帧；DeviceTransport 的 NotReady 原样映射给上层。
        std::int32_t DeviceTransportLibrary::GetFrame(unsigned char* buffer,
                                                      std::size_t capacity,
                                                      std::size_t& frameBytes,
                                                      MeyerDeviceCmdFrameInfo& frameInfo)
        {
            FrameInfo transportInfo = {};
            if (m_functions->initFrameInfo(&transportInfo) != Ok)
            {
                m_lastError = "DeviceTransport failed to initialize frame information";
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            const std::int32_t result = m_functions->getFrame(
                m_handle, buffer, capacity, &frameBytes, &transportInfo);
            if (result != Ok)
            {
                return MapResult(result, "GetFrame");
            }

            frameInfo.width = transportInfo.width;
            frameInfo.height = transportInfo.height;
            frameInfo.imageCount = transportInfo.imageCount;
            frameInfo.captureStatus = transportInfo.captureStatus;
            frameInfo.workMode = transportInfo.scanMode + 1;
            frameInfo.pictureOrderMode = transportInfo.pictureOrderMode;
            frameInfo.scanHeadType = transportInfo.scanHeadType;
            frameInfo.ledOn = transportInfo.ledOn;
            frameInfo.photoMode = transportInfo.photoMode;
            frameInfo.temperature0 = transportInfo.temperature0;
            frameInfo.temperature1 = transportInfo.temperature1;
            frameInfo.temperature2 = transportInfo.temperature2;
            frameInfo.temperature3 = transportInfo.temperature3;
            frameInfo.frameBytes = transportInfo.frameBytes;
            return MeyerDeviceCmdResult_Ok;
        }

        // 原始采集只建立 CyAPI 异步请求环；图像头、序号和解密由 CaptureProcessing 完成。
        std::int32_t DeviceTransportLibrary::StartRawCapture(
            const MeyerDeviceCmdCaptureParams& params)
        {
            if (m_rawCaptureActive)
            {
                m_lastError = "Raw capture stream is already active";
                return MeyerDeviceCmdResult_Busy;
            }

            std::int32_t result = m_functions->startStream(m_handle);
            if (result == Ok)
            {
                result = m_functions->primeStream(
                    m_handle,
                    static_cast<std::size_t>(params.transferSize),
                    static_cast<std::size_t>(params.queueDepth));
            }
            if (result != Ok)
            {
                // Prime 失败时也要回收 StartStream 已创建的临时状态。
                m_functions->stopStream(m_handle);
                return MapResult(result, "StartRawCapture");
            }

            m_rawCaptureActive = true;
            return MeyerDeviceCmdResult_Ok;
        }

        // 停流操作幂等，便于异常路径和正常退出共用同一清理函数。
        std::int32_t DeviceTransportLibrary::StopRawCapture()
        {
            if (!m_rawCaptureActive)
            {
                return MeyerDeviceCmdResult_Ok;
            }

            const std::int32_t result = m_functions->stopStream(m_handle);
            m_rawCaptureActive = false;
            return MapResult(result, "StopRawCapture");
        }

        // 该值只描述本适配器启动的原始流，不读取旧组帧引擎状态。
        bool DeviceTransportLibrary::IsRawCaptureActive() const
        {
            return m_rawCaptureActive;
        }

        // 直接把调用方缓冲区交给 Transport，避免在 DeviceCmd 内部再复制一次 16 KiB。
        std::int32_t DeviceTransportLibrary::ReceiveRawCapturePacket(
            unsigned char* buffer,
            std::size_t capacity,
            std::size_t& receivedSize,
            std::uint32_t timeoutMs)
        {
            receivedSize = 0U;
            if (!m_rawCaptureActive)
            {
                m_lastError = "Raw capture stream is not active";
                return MeyerDeviceCmdResult_NotReady;
            }

            const std::int32_t result = m_functions->receiveStreamPacket(
                m_handle, buffer, capacity, &receivedSize, timeoutMs);
            return result == Ok ? MeyerDeviceCmdResult_Ok : MapResult(result, "ReceiveRawCapturePacket");
        }

        // 把 Transport schema 转换为 DeviceCmd schema，不向上层暴露私有 ABI 镜像。
        std::int32_t DeviceTransportLibrary::GetStreamDiagnostics(
            MeyerDeviceCmdStreamDiagnostics& diagnostics)
        {
            StreamDiagnostics transportDiagnostics = {};
            if (m_functions->initStreamDiagnostics(&transportDiagnostics) != Ok)
            {
                m_lastError = "DeviceTransport failed to initialize stream diagnostics";
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            const std::int32_t result =
                m_functions->getStreamDiagnostics(m_handle, &transportDiagnostics);
            if (result != Ok)
            {
                return MapResult(result, "GetStreamDiagnostics");
            }

            diagnostics.sequence = transportDiagnostics.sequence;
            diagnostics.totalPackets = transportDiagnostics.totalPackets;
            diagnostics.totalTimeouts = transportDiagnostics.totalTimeouts;
            diagnostics.totalPartialPackets = transportDiagnostics.totalPartialPackets;
            diagnostics.totalIoFailures = transportDiagnostics.totalIoFailures;
            diagnostics.consecutiveTimeouts = transportDiagnostics.consecutiveTimeouts;
            diagnostics.lastResult = transportDiagnostics.lastResult;
            diagnostics.lastEvent = transportDiagnostics.lastEvent;
            diagnostics.streamActive = transportDiagnostics.streamActive;
            diagnostics.queueDepth = transportDiagnostics.queueDepth;
            diagnostics.transferSize = transportDiagnostics.transferSize;
            return MeyerDeviceCmdResult_Ok;
        }

        // 返回适配器保存的最近错误，字符串引用只在下一次操作前有效。
        const std::string& DeviceTransportLibrary::LastError() const
        {
            return m_lastError;
        }

        // 动态加载只接受绝对 UTF-8 路径，并在创建句柄前完成全部 ABI 门禁。
        // 将 UTF-8 绝对路径转为宽字符，加载 DLL、检查 ABI 并解析全部函数地址。
        std::int32_t DeviceTransportLibrary::Load(const char* libraryPathUtf8)
        {
            if (m_module != nullptr && m_handle != nullptr)
            {
                return MeyerDeviceCmdResult_Ok;
            }

            std::wstring libraryPath;
            if (!Utf8ToWide(libraryPathUtf8, libraryPath) || !IsAbsoluteWindowsPath(libraryPath))
            {
                m_lastError = "DeviceTransport DLL path must be an absolute UTF-8 path";
                return MeyerDeviceCmdResult_InvalidArgument;
            }

            m_module = ::LoadLibraryW(libraryPath.c_str());
            if (m_module == nullptr)
            {
                // LoadLibraryW 失败时立即读取 GetLastError；后续任何字符串转换或
                // 日志调用都可能覆盖线程错误码。错误码能区分缺少依赖、位数不匹配、
                // 文件损坏和路径错误，避免上层误报成“设备未连接”。
                const DWORD win32Error = ::GetLastError();
                std::ostringstream error;
                error << "Failed to load MeyerScan_DeviceTransport.dll from the explicit path"
                      << "; Win32 error=" << static_cast<unsigned long>(win32Error);
                m_lastError = error.str();
                return MeyerDeviceCmdResult_TransportLoadFailed;
            }

            Functions& f = *m_functions;
            f.getApiVersion = Resolve<Functions::GetApiVersionFunction>(m_module, "GetMeyerModuleApiVersion");
            if (f.getApiVersion == nullptr || f.getApiVersion() != kApiVersion)
            {
                m_lastError = "DeviceTransport API version is missing or incompatible";
                Unload();
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            f.initOpenParams = Resolve<Functions::InitOpenParamsFunction>(m_module, "MeyerDeviceTransport_InitOpenParams");
            f.initCaptureParams = Resolve<Functions::InitCaptureParamsFunction>(m_module, "MeyerDeviceTransport_InitCaptureParams");
            f.initFrameInfo = Resolve<Functions::InitFrameInfoFunction>(m_module, "MeyerDeviceTransport_InitFrameInfo");
            f.initStreamDiagnostics = Resolve<Functions::InitStreamDiagnosticsFunction>(m_module, "MeyerDeviceTransport_InitStreamDiagnostics");
            f.create = Resolve<Functions::CreateFunction>(m_module, "MeyerDeviceTransport_Create");
            f.destroy = Resolve<Functions::DestroyFunction>(m_module, "MeyerDeviceTransport_Destroy");
            f.open = Resolve<Functions::OpenFunction>(m_module, "MeyerDeviceTransport_Open");
            f.close = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_Close");
            f.isOpen = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_IsOpen");
            f.reconnect = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_Reconnect");
            f.sendCommand = Resolve<Functions::SendCommandFunction>(m_module, "MeyerDeviceTransport_SendCommand");
            f.receiveCommand = Resolve<Functions::ReceiveCommandFunction>(m_module, "MeyerDeviceTransport_ReceiveCommand");
            f.getDeviceCount = Resolve<Functions::GetIntegerFunction>(m_module, "MeyerDeviceTransport_GetDeviceCount");
            f.getIsUsb2 = Resolve<Functions::GetIntegerFunction>(m_module, "MeyerDeviceTransport_GetIsUsb2");
            f.setDeviceType = Resolve<Functions::SetIntegerFunction>(m_module, "MeyerDeviceTransport_SetDeviceType");
            f.setPictureOrderMode = Resolve<Functions::SetIntegerFunction>(m_module, "MeyerDeviceTransport_SetPictureOrderMode");
            f.setCaptureScanMode = Resolve<Functions::SetIntegerFunction>(m_module, "MeyerDeviceTransport_SetCaptureScanMode");
            f.setAhrsEnabled = Resolve<Functions::SetIntegerFunction>(m_module, "MeyerDeviceTransport_SetAhrsEnabled");
            f.setScanHeadType = Resolve<Functions::SetIntegerFunction>(m_module, "MeyerDeviceTransport_SetScanHeadType");
            f.startCapture = Resolve<Functions::StartCaptureFunction>(m_module, "MeyerDeviceTransport_StartCapture");
            f.stopCapture = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_StopCapture");
            f.isCaptureActive = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_IsCaptureActive");
            f.getFrame = Resolve<Functions::GetFrameFunction>(m_module, "MeyerDeviceTransport_GetFrame");
            f.startStream = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_StartStream");
            f.primeStream = Resolve<Functions::PrimeStreamFunction>(m_module, "MeyerDeviceTransport_PrimeStream");
            f.stopStream = Resolve<Functions::SimpleHandleFunction>(m_module, "MeyerDeviceTransport_StopStream");
            f.receiveStreamPacket = Resolve<Functions::ReceiveStreamPacketFunction>(m_module, "MeyerDeviceTransport_ReceiveStreamPacket");
            f.getStreamDiagnostics = Resolve<Functions::GetStreamDiagnosticsFunction>(m_module, "MeyerDeviceTransport_GetStreamDiagnostics");
            f.getLastError = Resolve<Functions::GetLastErrorFunction>(m_module, "MeyerDeviceTransport_GetLastError");

            // 任一必需导出缺失都拒绝继续，不能在后续业务路径中调用空函数指针。
            if (!f.initOpenParams || !f.initCaptureParams || !f.initFrameInfo ||
                !f.initStreamDiagnostics || !f.create ||
                !f.destroy || !f.open || !f.close || !f.isOpen || !f.reconnect ||
                !f.sendCommand || !f.receiveCommand || !f.getDeviceCount || !f.getIsUsb2 ||
                !f.setDeviceType || !f.setPictureOrderMode || !f.setCaptureScanMode ||
                !f.setAhrsEnabled || !f.setScanHeadType || !f.startCapture || !f.stopCapture ||
                !f.isCaptureActive || !f.getFrame || !f.startStream || !f.primeStream ||
                !f.stopStream || !f.receiveStreamPacket || !f.getStreamDiagnostics ||
                !f.getLastError)
            {
                m_lastError = "DeviceTransport is missing one or more required API v2 exports";
                Unload();
                return MeyerDeviceCmdResult_TransportApiMismatch;
            }

            m_handle = f.create();
            if (m_handle == nullptr)
            {
                m_lastError = "DeviceTransport failed to create a session handle";
                Unload();
                return MeyerDeviceCmdResult_InternalError;
            }

            return MeyerDeviceCmdResult_Ok;
        }

        // 映射错误并尽量读取底层详细原因，调用方最终只看到 DeviceCmd 结果域。
        // 将 Transport 专属结果码转换为 DeviceCmd 公共结果码，并复制底层错误。
        std::int32_t DeviceTransportLibrary::MapResult(std::int32_t transportResult,
                                                       const char* operation)
        {
            if (transportResult == Ok)
            {
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            std::size_t requiredSize = 0U;
            if (m_handle != nullptr && m_functions->getLastError != nullptr)
            {
                m_functions->getLastError(m_handle, nullptr, 0U, &requiredSize);
                if (requiredSize > 1U && requiredSize < 4096U)
                {
                    std::vector<char> message(requiredSize, '\0');
                    m_functions->getLastError(m_handle, &message[0], message.size(), &requiredSize);
                    m_lastError.assign(&message[0]);
                }
            }
            if (m_lastError.empty())
            {
                m_lastError = operation == nullptr ? "DeviceTransport operation failed" : operation;
            }

            switch (transportResult)
            {
            case DeviceNotFound: return MeyerDeviceCmdResult_DeviceNotFound;
            case NotOpen: return MeyerDeviceCmdResult_NotOpen;
            case Timeout: return MeyerDeviceCmdResult_Timeout;
            case BufferTooSmall: return MeyerDeviceCmdResult_BufferTooSmall;
            case AlreadyRunning: return MeyerDeviceCmdResult_Busy;
            case NotReady: return MeyerDeviceCmdResult_NotReady;
            case InvalidArgument: return MeyerDeviceCmdResult_InvalidArgument;
            case IoFailed: return MeyerDeviceCmdResult_IoFailed;
            case StreamStalled: return MeyerDeviceCmdResult_StreamStalled;
            case DeviceDisconnected: return MeyerDeviceCmdResult_DeviceDisconnected;
            default: return MeyerDeviceCmdResult_InternalError;
            }
        }

        // 释放顺序与 Load 相反；即使只完成了一半加载也可以安全调用。
        // 先销毁函数表中的会话句柄，再释放模块句柄并重置函数表。
        void DeviceTransportLibrary::Unload()
        {
            if (m_handle != nullptr && m_functions && m_functions->destroy != nullptr)
            {
                m_functions->destroy(m_handle);
                m_handle = nullptr;
            }
            m_rawCaptureActive = false;
            if (m_module != nullptr)
            {
                ::FreeLibrary(static_cast<HMODULE>(m_module));
                m_module = nullptr;
            }

            // 重建空函数表，避免下次 Load 误用上一次 DLL 的地址。
            m_functions.reset(new Functions());
        }
    }
}
