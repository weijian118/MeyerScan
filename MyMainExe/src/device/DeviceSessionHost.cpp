// =============================================================================
// 文件: DeviceSessionHost.cpp
// 作用: 实现 MainExe 对 DeviceCmd/DeviceTransport 的单会话动态调用链。
// =============================================================================
#include "DeviceSessionHost.h"

#include "Logger.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace
{
    const char* const kHostLogModule = "MeyerScan_MainExe";
    const int kExpectedDeviceCmdApiVersion = 1;

    // QLibrary::resolve 返回通用函数指针；该模板集中完成签名转换，避免调用处重复。
    template<typename FunctionType>
    FunctionType ResolveFunction(QLibrary& library, const char* name)
    {
        return reinterpret_cast<FunctionType>(library.resolve(name));
    }
}

// 初始化全部成员为空值，保证 EnsureLoaded 中任一步失败后都能安全 Shutdown。
DeviceSessionHost::DeviceSessionHost()
    : m_logger(nullptr)
    , m_handle(nullptr)
    , m_hasSnapshot(false)
    , m_initOpenParams(nullptr)
    , m_initPreflight(nullptr)
    , m_create(nullptr)
    , m_destroy(nullptr)
    , m_prepare(nullptr)
    , m_close(nullptr)
    , m_isOpen(nullptr)
    , m_getLastError(nullptr)
{
    std::memset(&m_snapshot, 0, sizeof(m_snapshot));
}

// 析构统一复用 Shutdown，避免 MainWindow 异常退出时遗漏设备句柄。
DeviceSessionHost::~DeviceSessionHost()
{
    Shutdown();
}

// 只保存调用上下文，不在 MainWindow 构造阶段访问 USB 或加载设备 DLL。
void DeviceSessionHost::Initialize(const QString& applicationDir, ILogger* logger)
{
    m_applicationDir = QFileInfo(applicationDir).absoluteFilePath();
    m_logger = logger;
    WriteLog(LogLevel::Info, "DeviceHostInitialize", "Device session host initialized lazily");
}

// 建立颜色校准所需设备会话并保存返回快照。
std::int32_t DeviceSessionHost::PrepareColorCalibration(
    MeyerDeviceCalibrationPreflight* preflight)
{
    if (preflight == nullptr)
    {
        return MeyerDeviceCmdResult_InvalidArgument;
    }
    if (!EnsureLoaded())
    {
        // DLL 尚未可用时也填充一个可判断的结果，SettingsUI 可以显示稳定提示。
        std::memset(preflight, 0, sizeof(*preflight));
        preflight->structSize = sizeof(*preflight);
        preflight->schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
        preflight->status = MeyerDeviceCalibrationPreflight_InternalError;
        preflight->commandResult = MeyerDeviceCmdResult_TransportLoadFailed;
        const QByteArray detail = QString("DeviceCmd load failed: %1")
            .arg(m_deviceCmdLibrary.errorString()).toUtf8();
        std::strncpy(preflight->detailUtf8,
                     detail.constData(),
                     sizeof(preflight->detailUtf8) - 1U);
        return MeyerDeviceCmdResult_TransportLoadFailed;
    }

    if (m_initPreflight(preflight) != MeyerDeviceCmdResult_Ok)
    {
        WriteLog(LogLevel::Error,
                 "DevicePreflightInitFailed",
                 "DeviceCmd rejected the calibration preflight structure");
        return MeyerDeviceCmdResult_TransportApiMismatch;
    }

    MeyerDeviceCmdOpenParams params;
    if (m_initOpenParams(&params) != MeyerDeviceCmdResult_Ok)
    {
        WriteLog(LogLevel::Error,
                 "DeviceOpenParamsInitFailed",
                 "DeviceCmd failed to initialize open parameters");
        return MeyerDeviceCmdResult_TransportApiMismatch;
    }

    // Unknown 表示先用 Cypress 公共探测链路读取 0xCD/0xCE，再由 DeviceCmd
    // 从设备明确上报的型号标记切换真实能力目录。
    params.modelHint = MeyerDeviceModel_Unknown;
    const QString transportPath = QDir(m_applicationDir).filePath(
        "MeyerScan_DeviceTransport.dll");
    const QByteArray transportPathUtf8 = QFileInfo(transportPath).absoluteFilePath().toUtf8();
    if (transportPathUtf8.size() >= static_cast<int>(sizeof(params.transportLibraryPathUtf8)))
    {
        WriteLog(LogLevel::Error,
                 "DeviceTransportPathRejected",
                 "DeviceTransport absolute path exceeds the DeviceCmd ABI buffer");
        return MeyerDeviceCmdResult_InvalidArgument;
    }
    std::strncpy(params.transportLibraryPathUtf8,
                 transportPathUtf8.constData(),
                 sizeof(params.transportLibraryPathUtf8) - 1U);

    WriteLog(LogLevel::Info,
             "DevicePreflightStart",
             QString("Preparing color calibration through %1").arg(transportPath));
    // 设备命令可能等待 USB 超时，不能直接在按钮回调的 GUI 线程中执行。
    // 工作线程只访问 DeviceCmd C ABI；主线程运行局部 Qt 事件循环，继续处理窗口绘制。
    MeyerDeviceCalibrationPreflight workerPreflight = *preflight;
    std::atomic<bool> completed(false);
    std::int32_t result = MeyerDeviceCmdResult_InternalError;
    std::thread worker;
    try
    {
        worker = std::thread([this, &params, &workerPreflight, &result, &completed]() {
            result = m_prepare(m_handle, &params, &workerPreflight);
            completed.store(true);
        });
    }
    catch (...)
    {
        WriteLog(LogLevel::Error,
                 "DevicePreflightThreadFailed",
                 "Failed to start the device preflight worker thread");
        return MeyerDeviceCmdResult_InternalError;
    }

    QEventLoop waitLoop;
    QTimer completionTimer;
    completionTimer.setInterval(10);
    QObject::connect(&completionTimer, &QTimer::timeout, [&completed, &waitLoop]() {
        if (completed.load())
        {
            waitLoop.quit();
        }
    });
    completionTimer.start();
    waitLoop.exec();
    completionTimer.stop();
    worker.join();
    *preflight = workerPreflight;
    if (result != MeyerDeviceCmdResult_Ok)
    {
        WriteLog(LogLevel::Error,
                 "DevicePreflightCallFailed",
                 QString("DeviceCmd preflight API failed: %1").arg(ReadLastError()));
        return result;
    }

    // 预检结构拥有完整 POD 副本，MainExe 后续查询不再触发 USB 命令。
    m_snapshot = preflight->state;
    m_hasSnapshot = true;
    const QString detail = QString::fromUtf8(preflight->detailUtf8);
    WriteLog(preflight->status == MeyerDeviceCalibrationPreflight_Ready
                 ? LogLevel::Info
                 : LogLevel::Warning,
             "DevicePreflightResult",
             QString("status=%1 model=%2 usb2=%3 detail=%4")
                 .arg(preflight->status)
                 .arg(preflight->state.model)
                 .arg(preflight->state.isUsb2)
                 .arg(detail));
    return MeyerDeviceCmdResult_Ok;
}

// 关闭当前会话。DeviceCmd Close 本身幂等，因此这里不需要维护第二套连接布尔值。
void DeviceSessionHost::CloseSession()
{
    if (m_handle != nullptr && m_close != nullptr)
    {
        const std::int32_t result = m_close(m_handle);
        WriteLog(result == MeyerDeviceCmdResult_Ok ? LogLevel::Info : LogLevel::Warning,
                 "DeviceSessionClose",
                 QString("Device session close result: %1").arg(result));
    }
}

// 直接询问 DeviceCmd 的真实连接状态，避免使用可能过期的缓存快照。
bool DeviceSessionHost::IsSessionOpen() const
{
    return m_handle != nullptr && m_isOpen != nullptr && m_isOpen(m_handle) == 1;
}

// 向调用方复制快照。调用方必须自己保存副本，不能持有本类成员地址。
bool DeviceSessionHost::GetSnapshot(MeyerDeviceStateSnapshot* snapshot) const
{
    if (snapshot == nullptr || !m_hasSnapshot)
    {
        return false;
    }
    *snapshot = m_snapshot;
    return true;
}

// 按“关闭设备 -> 销毁 DeviceCmd 句柄 -> 卸载 DLL 引用”的顺序释放。
void DeviceSessionHost::Shutdown()
{
    CloseSession();
    if (m_handle != nullptr && m_destroy != nullptr)
    {
        m_destroy(m_handle);
        m_handle = nullptr;
    }
    if (m_deviceCmdLibrary.isLoaded())
    {
        m_deviceCmdLibrary.unload();
    }
    m_hasSnapshot = false;
    std::memset(&m_snapshot, 0, sizeof(m_snapshot));
}

// 完成 DeviceCmd 动态加载和 ABI 门禁。
bool DeviceSessionHost::EnsureLoaded()
{
    if (m_handle != nullptr)
    {
        return true;
    }
    if (m_applicationDir.isEmpty())
    {
        WriteLog(LogLevel::Error,
                 "DeviceCmdLoadFailed",
                 "Application directory is empty");
        return false;
    }

    const QString libraryPath = QDir(m_applicationDir).filePath("MeyerScan_DeviceCmd.dll");
    m_deviceCmdLibrary.setFileName(QFileInfo(libraryPath).absoluteFilePath());
    m_deviceCmdLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    if (!m_deviceCmdLibrary.load())
    {
        WriteLog(LogLevel::Error,
                 "DeviceCmdLoadFailed",
                 QString("Failed to load DeviceCmd: %1").arg(m_deviceCmdLibrary.errorString()));
        return false;
    }

    const GetApiVersionFunction getApiVersion =
        ResolveFunction<GetApiVersionFunction>(m_deviceCmdLibrary,
                                               "GetMeyerModuleApiVersion");
    if (getApiVersion == nullptr || getApiVersion() != kExpectedDeviceCmdApiVersion)
    {
        WriteLog(LogLevel::Error,
                 "DeviceCmdApiRejected",
                 "DeviceCmd integer ABI version is missing or incompatible");
        return false;
    }

    m_initOpenParams = ResolveFunction<InitOpenParamsFunction>(
        m_deviceCmdLibrary, "MeyerDeviceCmd_InitOpenParams");
    m_initPreflight = ResolveFunction<InitPreflightFunction>(
        m_deviceCmdLibrary, "MeyerDeviceCmd_InitCalibrationPreflight");
    m_create = ResolveFunction<CreateFunction>(m_deviceCmdLibrary, "MeyerDeviceCmd_Create");
    m_destroy = ResolveFunction<DestroyFunction>(m_deviceCmdLibrary, "MeyerDeviceCmd_Destroy");
    m_prepare = ResolveFunction<PrepareFunction>(
        m_deviceCmdLibrary, "MeyerDeviceCmd_PrepareColorCalibration");
    m_close = ResolveFunction<SimpleHandleFunction>(m_deviceCmdLibrary, "MeyerDeviceCmd_Close");
    m_isOpen = ResolveFunction<SimpleHandleFunction>(m_deviceCmdLibrary, "MeyerDeviceCmd_IsOpen");
    m_getLastError = ResolveFunction<GetLastErrorFunction>(
        m_deviceCmdLibrary, "MeyerDeviceCmd_GetLastError");

    if (m_initOpenParams == nullptr || m_initPreflight == nullptr || m_create == nullptr ||
        m_destroy == nullptr || m_prepare == nullptr || m_close == nullptr ||
        m_isOpen == nullptr || m_getLastError == nullptr)
    {
        WriteLog(LogLevel::Error,
                 "DeviceCmdExportsMissing",
                 "DeviceCmd is missing one or more calibration host exports");
        return false;
    }

    m_handle = m_create();
    if (m_handle == nullptr)
    {
        WriteLog(LogLevel::Error,
                 "DeviceCmdCreateFailed",
                 "DeviceCmd returned a null session handle");
        return false;
    }
    WriteLog(LogLevel::Info,
             "DeviceCmdLoaded",
             QString("DeviceCmd loaded from %1").arg(libraryPath));
    return true;
}

// 按 DeviceCmd 的两次缓冲区合同读取 UTF-8 错误文本。
QString DeviceSessionHost::ReadLastError() const
{
    if (m_handle == nullptr || m_getLastError == nullptr)
    {
        return QString();
    }
    std::size_t requiredSize = 0U;
    m_getLastError(m_handle, nullptr, 0U, &requiredSize);
    if (requiredSize <= 1U || requiredSize > 4096U)
    {
        return QString();
    }
    std::vector<char> buffer(requiredSize, '\0');
    if (m_getLastError(m_handle, &buffer[0], buffer.size(), &requiredSize) !=
        MeyerDeviceCmdResult_Ok)
    {
        return QString();
    }
    return QString::fromUtf8(&buffer[0]);
}

// DeviceSessionHost 属于 MainExe 编排层，因此日志模块字段固定使用 MainExe。
void DeviceSessionHost::WriteLog(LogLevel level,
                                 const char* operation,
                                 const QString& content) const
{
    if (m_logger == nullptr)
    {
        return;
    }
    const QByteArray deviceId = m_hasSnapshot
        ? QByteArray(m_snapshot.deviceIdUtf8)
        : QByteArray();
    const QByteArray contentUtf8 = content.toUtf8();
    m_logger->Write(level,
                    kHostLogModule,
                    operation == nullptr ? "DeviceSessionHost" : operation,
                    deviceId.constData(),
                    "",
                    "",
                    contentUtf8.constData());
}
