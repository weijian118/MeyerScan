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
    // 使用公共头中的整数 ABI 常量，避免 DeviceCmd 升级后宿主仍接受旧 DLL。
    const int kExpectedDeviceCmdApiVersion = MEYER_DEVICE_CMD_API_VERSION;

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
    , m_hasLastPreflight(false)
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
    std::memset(&m_lastPreflight, 0, sizeof(m_lastPreflight));
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

// 颜色校准允许生产调试设备使用带来源标记的 effective 兼容身份。
// 后续三维校准接入同一宿主时也应复用该宽松身份策略，但连接、USB3、系列、
// 型号和证据冲突等其它预检仍然必须通过。
std::int32_t DeviceSessionHost::PrepareColorCalibration(
    MeyerDeviceCalibrationPreflight* preflight)
{
    return PrepareDeviceSession(preflight,
                                AllowProductionCompatibilityIdentity,
                                "ColorCalibrationPreflight");
}

// 为创建/练习工作台建立设备会话。
// 只有 MainExe 的练习入口可以把 allowProductionIdentity 设为 true；底层 UI
// 不接触该开关，防止普通创建流程绕过真实设备编号限制。
std::int32_t DeviceSessionHost::PrepareWorkspaceSession(
    bool allowProductionIdentity,
    MeyerDeviceCalibrationPreflight* preflight)
{
    return PrepareDeviceSession(
        preflight,
        allowProductionIdentity
            ? AllowProductionCompatibilityIdentity
            : RequireProgrammedDeviceNumber,
        allowProductionIdentity
            ? "PracticeWorkspacePreflight"
            : "OrderWorkspacePreflight");
}

// 执行通用设备预检并应用场景级身份策略。
// DeviceCmd 的导出函数名称仍保留 PrepareColorCalibration，是为了兼容现有 ABI；
// 实际执行的是连接、USB3、D9、C7、CE 和产品身份识别这一组只读公共准备步骤。
std::int32_t DeviceSessionHost::PrepareDeviceSession(
    MeyerDeviceCalibrationPreflight* preflight,
    DeviceIdentityAdmissionPolicy policy,
    const char* operationName)
{
    if (preflight == nullptr)
    {
        return MeyerDeviceCmdResult_InvalidArgument;
    }

    // 同一工作台内从 Process 返回 Scan 时复用已打开会话和不可变检测快照，
    // 避免重复枚举 USB、重复发送 D9/C7/CE 命令。
    if (IsSessionOpen() && m_hasLastPreflight)
    {
        *preflight = m_lastPreflight;
        ApplyIdentityAdmissionPolicy(preflight, policy);
        m_lastPreflight = *preflight;
        WriteLog(preflight->status == MeyerDeviceCalibrationPreflight_Ready
                     ? LogLevel::Info
                     : LogLevel::Warning,
                 operationName,
                 QString("Reused open device session, status=%1 production=%2")
                     .arg(preflight->status)
                     .arg(preflight->detectionRecord.isProductionMode));
        return MeyerDeviceCmdResult_Ok;
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
        // DLL 加载失败也是一次有效预检结果，保存后可供诊断页读取。
        m_lastPreflight = *preflight;
        m_hasLastPreflight = true;
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

    // Unknown 表示先用 Cypress 公共探测链路读取 0xD4/0xD9 机器码，再读取
    // 0xCD/0xCE 机型信息并由 DeviceCmd 切换真实能力目录。
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
             operationName,
             QString("Preparing device session through %1").arg(transportPath));
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
        // 线程尚未启动时保存已初始化的 NotRun 结构，避免保留上一次设备结果。
        m_lastPreflight = *preflight;
        m_hasLastPreflight = true;
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
    // 底层检测成功后才应用工作流准入策略。这样 reported/effective 身份先完整
    // 生成，严格模式的拦截也不会丢失生产现场所需诊断信息。
    if (result == MeyerDeviceCmdResult_Ok)
    {
        ApplyIdentityAdmissionPolicy(preflight, policy);
    }
    // 即使 DeviceCmd API 返回错误，也保存它已经写入的诊断结构，便于现场查看
    // 最后失败步骤；业务层仍按 result 决定是否继续。
    m_lastPreflight = *preflight;
    m_hasLastPreflight = true;
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
             operationName,
             QString("status=%1 detection=%2 profile=%3 usb2=%4 reportedNumber=%5 "
                     "effectiveNumber=%6 reportedModelCode=%7 effectiveModelCode=%8 "
                     "product=%9 identityStatus=%10 production=%11 compatibility=%12 "
                     "detail=%13")
                  .arg(preflight->status)
                  .arg(preflight->detectionRecord.detectionStatus)
                  .arg(preflight->state.model)
                  .arg(preflight->state.isUsb2)
                  .arg(QString::fromUtf8(
                      preflight->detectionRecord.reportedDeviceNumberUtf8))
                  .arg(QString::fromUtf8(
                      preflight->detectionRecord.effectiveDeviceNumberUtf8))
                  .arg(QString::fromUtf8(
                      preflight->detectionRecord.reportedModelCodeUtf8))
                  .arg(QString::fromUtf8(
                      preflight->detectionRecord.effectiveModelCodeUtf8))
                  .arg(QString::fromUtf8(preflight->productIdentity.productNameUtf8))
                  .arg(preflight->productIdentity.identificationStatus)
                  .arg(preflight->detectionRecord.isProductionMode)
                  .arg(preflight->detectionRecord.usedCompatibilityDefaults)
                  .arg(detail));
    return MeyerDeviceCmdResult_Ok;
}

// 应用“允许兼容身份”或“必须有真实编号”的场景级准入规则。
void DeviceSessionHost::ApplyIdentityAdmissionPolicy(
    MeyerDeviceCalibrationPreflight* preflight,
    DeviceIdentityAdmissionPolicy policy)
{
    if (preflight == nullptr ||
        preflight->status != MeyerDeviceCalibrationPreflight_Ready)
    {
        return;
    }

    const bool productionDevice =
        preflight->detectionRecord.isProductionMode != 0;
    const bool strictIdentityRequired =
        policy == RequireProgrammedDeviceNumber;
    if (!productionDevice || !strictIdentityRequired)
    {
        return;
    }

    // 只改业务准入状态，不清空 reported/effective、产品身份和命令步骤。
    // 现场日志仍能看到设备真实未写号状态以及练习模式将使用的默认身份。
    preflight->status =
        MeyerDeviceCalibrationPreflight_ProductionDeviceNumberRequired;
    const char* const detail =
        "The device number is not programmed; production compatibility identity is allowed only in practice mode";
    std::memset(preflight->detailUtf8, 0, sizeof(preflight->detailUtf8));
    std::strncpy(preflight->detailUtf8,
                 detail,
                 sizeof(preflight->detailUtf8) - 1U);

    // 创建流程被拦截后立即释放 USB；练习和校准的 Ready 分支继续持有会话。
    CloseSession();
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

// 复制完整预检记录。该接口只访问内存，不会再次发送 D4、C2 或 CD 命令。
bool DeviceSessionHost::GetLastPreflight(MeyerDeviceCalibrationPreflight* preflight) const
{
    if (preflight == nullptr || !m_hasLastPreflight)
    {
        return false;
    }
    *preflight = m_lastPreflight;
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
    m_hasLastPreflight = false;
    std::memset(&m_lastPreflight, 0, sizeof(m_lastPreflight));
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
