// =============================================================================
// 文件: CaptureServiceContext.cpp
// 作用: 实现 CaptureService 的配置、设备准入和生命周期管理。
// =============================================================================
#include "CaptureServiceContext.h"

#include "../support/ModuleLogger.h"
#include "../support/PathUtils.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace
{
    // 固定数组复制必须先清零并保留结尾，避免外部 DLL 返回满数组时越界读取。
    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const char* source)
    {
        std::memset(destination, 0, Capacity);
        // C++ 不允许声明长度为 0 的原生数组，因此 Capacity 在本模板中必定
        // 大于 0。去掉恒真分支既保持结尾保护，也避免 VS2015 的 C4127 警告。
        if (source != nullptr)
        {
            std::strncpy(destination, source, Capacity - 1U);
        }
    }

    // 根据 D4/D9 的结构化状态生成可用于测试界面的短文本。
    const char* DeviceNumberStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceNumberRead_Valid: return "Programmed";
        case MeyerDeviceNumberRead_ResponseMissing: return "Response missing";
        case MeyerDeviceNumberRead_FrameInvalid: return "Frame invalid";
        case MeyerDeviceNumberRead_ChecksumIndicatesUnprogrammed:
            return "Unprogrammed: checksum marker";
        case MeyerDeviceNumberRead_UninitializedLength:
            return "Unprogrammed: length is 0xFFFF";
        case MeyerDeviceNumberRead_ValueInvalid: return "Value invalid";
        default: return "Not checked";
        }
    }

}

namespace meyer
{
    namespace captureservice
    {
        CaptureServiceContext::CaptureServiceContext()
            : m_stopRequested(false), m_postStopRequested(false),
              m_configured(false), m_preflightReady(false),
              m_captureActive(false), m_autoExposureReservedReported(false),
              m_eventSequence(0U), m_lightRequestSequence(0U)
        {
            // 所有跨 DLL POD 在首次使用前必须清零并写入结构头。
            std::memset(&m_config, 0, sizeof(m_config));
            std::memset(&m_deviceInfo, 0, sizeof(m_deviceInfo));
            std::memset(&m_state, 0, sizeof(m_state));
            std::memset(&m_openParams, 0, sizeof(m_openParams));
            std::memset(&m_preflight, 0, sizeof(m_preflight));
            std::memset(&m_profile, 0, sizeof(m_profile));
            std::memset(&m_captureDeviceContext, 0, sizeof(m_captureDeviceContext));
            std::memset(&m_latestGroupInfo, 0, sizeof(m_latestGroupInfo));
            std::memset(&m_pipelineOptions, 0, sizeof(m_pipelineOptions));
            std::memset(&m_latestDisplayOutputInfo, 0,
                        sizeof(m_latestDisplayOutputInfo));
            std::memset(&m_latestReconstructionOutputInfo, 0,
                        sizeof(m_latestReconstructionOutputInfo));
            m_deviceInfo.structSize = sizeof(m_deviceInfo);
            m_deviceInfo.schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
            m_state.structSize = sizeof(m_state);
            m_state.schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
            m_state.state = MeyerCaptureServiceState_Created;
            m_state.lastResult = MeyerCaptureServiceResult_Ok;
        }

        CaptureServiceContext::~CaptureServiceContext()
        {
            Shutdown();
        }

        // Configure 只加载模块和形成打开参数，不提前触发 USB 枚举或设备命令。
        std::int32_t CaptureServiceContext::Configure(
            const MeyerCaptureServiceConfig& config)
        {
            if (config.structSize < sizeof(config) ||
                config.schemaVersion != MEYER_CAPTURE_SERVICE_SCHEMA_VERSION ||
                (config.backendType != MeyerCaptureServiceBackend_DeviceTransport &&
                 config.backendType != MeyerCaptureServiceBackend_SimulatorForTest) ||
                config.captureMode < MeyerCaptureMode_ColorCalibration ||
                config.captureMode > MeyerCaptureMode_OrderScan)
            {
                return MeyerCaptureServiceResult_InvalidArgument;
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_captureActive || m_captureThread.joinable() || m_postThread.joinable())
                {
                    return MeyerCaptureServiceResult_AlreadyRunning;
                }
                if (m_configured)
                {
                    return MeyerCaptureServiceResult_InvalidArgument;
                }
                m_config = config;
                if (m_config.eventQueueCapacity == 0U)
                {
                    m_config.eventQueueCapacity = 256U;
                }
                if (m_config.postProcessQueueCapacity == 0U)
                {
                    m_config.postProcessQueueCapacity = 3U;
                }
            }

            const std::string deviceCmdPath = config.deviceCmdLibraryPathUtf8[0] == '\0'
                ? SiblingModulePathUtf8("MeyerScan_DeviceCmd.dll")
                : std::string(config.deviceCmdLibraryPathUtf8);
            const std::string processingPath =
                config.captureProcessingLibraryPathUtf8[0] == '\0'
                ? SiblingModulePathUtf8("MeyerScan_CaptureProcessing.dll")
                : std::string(config.captureProcessingLibraryPathUtf8);
            const std::string exposurePath = config.autoExposureLibraryPathUtf8[0] == '\0'
                ? SiblingModulePathUtf8("MeyerScan_AutoExposure.dll")
                : std::string(config.autoExposureLibraryPathUtf8);
            const std::string imagePipelinePath =
                config.captureImagePipelineLibraryPathUtf8[0] == '\0'
                ? SiblingModulePathUtf8("MeyerScan_CaptureImagePipeline.dll")
                : std::string(config.captureImagePipelineLibraryPathUtf8);

            std::int32_t result = m_deviceCmd.Load(deviceCmdPath);
            if (result != MeyerDeviceCmdResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_ModuleLoadFailed,
                         MeyerCaptureServiceEvent_InternalError,
                         m_deviceCmd.LastError());
                return MeyerCaptureServiceResult_ModuleLoadFailed;
            }
            PublishEvent(MeyerCaptureServiceEvent_ModuleLoaded,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "MeyerScan_DeviceCmd.dll loaded and ABI checked");

            result = m_processing.Load(processingPath);
            if (result != MeyerCaptureProcessingResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_ModuleLoadFailed,
                         MeyerCaptureServiceEvent_InternalError,
                         m_processing.LastError());
                return MeyerCaptureServiceResult_ModuleLoadFailed;
            }
            PublishEvent(MeyerCaptureServiceEvent_ModuleLoaded,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "MeyerScan_CaptureProcessing.dll loaded and ABI checked");

            result = m_imagePipeline.Load(imagePipelinePath);
            if (result != MeyerCaptureImagePipelineResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_ModuleLoadFailed,
                         MeyerCaptureServiceEvent_InternalError,
                         m_imagePipeline.LastError());
                return MeyerCaptureServiceResult_ModuleLoadFailed;
            }
            PublishEvent(MeyerCaptureServiceEvent_ModuleLoaded,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "MeyerScan_CaptureImagePipeline.dll loaded and ABI checked");

            result = m_autoExposure.Load(exposurePath);
            if (result != MeyerAutoExposureResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_ModuleLoadFailed,
                         MeyerCaptureServiceEvent_InternalError,
                         m_autoExposure.LastError());
                return MeyerCaptureServiceResult_ModuleLoadFailed;
            }
            PublishEvent(MeyerCaptureServiceEvent_ModuleLoaded,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "MeyerScan_AutoExposure.dll placeholder loaded and ABI checked");

            if (m_imagePipeline.InitOptions(m_pipelineOptions) !=
                    MeyerCaptureImagePipelineResult_Ok ||
                m_imagePipeline.InitOutputInfo(m_latestDisplayOutputInfo) !=
                    MeyerCaptureImagePipelineResult_Ok ||
                m_imagePipeline.InitOutputInfo(m_latestReconstructionOutputInfo) !=
                    MeyerCaptureImagePipelineResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_AbiMismatch,
                         MeyerCaptureServiceEvent_InternalError,
                         "CaptureImagePipeline failed to initialize public structures");
                return MeyerCaptureServiceResult_AbiMismatch;
            }
            m_pipelineOptions.captureMode = config.captureMode;

            if (m_deviceCmd.InitOpenParams(m_openParams) != MeyerDeviceCmdResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_AbiMismatch,
                         MeyerCaptureServiceEvent_InternalError,
                         "DeviceCmd failed to initialize open parameters");
                return MeyerCaptureServiceResult_AbiMismatch;
            }
            m_openParams.backendType = config.backendType;
            m_openParams.modelHint = config.modelHint;
            m_openParams.vendorId = config.vendorId;
            m_openParams.productId = config.productId;
            m_openParams.deviceIndex = config.deviceIndex;
            if (config.commandTimeoutMs > 0U)
            {
                m_openParams.commandTimeoutMs = config.commandTimeoutMs;
            }
            if (config.streamTimeoutMs > 0U)
            {
                m_openParams.streamTimeoutMs = config.streamTimeoutMs;
            }
            m_openParams.simulatedFlags = config.simulatedFlags;
            CopyText(m_openParams.simulatedDeviceIdUtf8,
                     config.simulatedDeviceIdUtf8);

            const std::string transportPath =
                config.deviceTransportLibraryPathUtf8[0] == '\0'
                ? SiblingModulePathUtf8("MeyerScan_DeviceTransport.dll")
                : std::string(config.deviceTransportLibraryPathUtf8);
            CopyText(m_openParams.transportLibraryPathUtf8,
                     transportPath.c_str());

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_configured = true;
                m_preflightReady = false;
                m_lastError.clear();
            }
            SetState(MeyerCaptureServiceState_Configured,
                     MeyerCaptureServiceResult_Ok);
            WriteInfo("Configure", "Capture service dependencies loaded and configured");
            return MeyerCaptureServiceResult_Ok;
        }

        // Shutdown 可以重复调用。线程先退出，随后关闭设备，最后清理内存队列。
        std::int32_t CaptureServiceContext::Shutdown()
        {
            StopCapture();
            m_deviceCmd.Close();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_configured = false;
                m_preflightReady = false;
                m_captureActive = false;
                m_events.clear();
                m_postQueue.clear();
                m_lightRequests.clear();
                m_latestProcessedGroup.clear();
                m_latestRgb.clear();
                m_lastError.clear();
                m_state.captureActive = 0;
                m_state.latestDataAvailable = 0;
                m_state.state = MeyerCaptureServiceState_Closed;
                m_state.lastResult = MeyerCaptureServiceResult_Ok;
                ++m_state.sequence;
            }
            return MeyerCaptureServiceResult_Ok;
        }

        // 颜色校准准入成功后，DeviceCmd 会保留同一个打开会话供后续 StartCapture 使用。
        std::int32_t CaptureServiceContext::PrepareColorCalibration()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_configured)
                {
                    return MeyerCaptureServiceResult_NotConfigured;
                }
                if (m_captureActive)
                {
                    return MeyerCaptureServiceResult_AlreadyRunning;
                }
                if (m_config.captureMode != MeyerCaptureMode_ColorCalibration)
                {
                    m_lastError = "This preflight entry only supports color calibration";
                    return MeyerCaptureServiceResult_InvalidArgument;
                }
            }

            PublishEvent(MeyerCaptureServiceEvent_PreflightStarted,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "Color calibration device preflight started");
            WriteInfo("PrepareColorCalibration", "Color calibration preflight started");

            if (m_deviceCmd.InitCalibrationPreflight(m_preflight) !=
                MeyerDeviceCmdResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_AbiMismatch,
                         MeyerCaptureServiceEvent_InternalError,
                         "DeviceCmd failed to initialize preflight structure");
                return MeyerCaptureServiceResult_AbiMismatch;
            }
            const std::int32_t commandResult =
                m_deviceCmd.PrepareColorCalibration(m_openParams, m_preflight);
            FillDeviceInfo(m_preflight);

            if (commandResult != MeyerDeviceCmdResult_Ok)
            {
                const std::string detail = m_deviceCmd.LastError().empty()
                    ? "DeviceCmd color calibration preflight call failed"
                    : m_deviceCmd.LastError();
                SetFault(MeyerCaptureServiceResult_PreflightRejected,
                         MeyerCaptureServiceEvent_PreflightRejected, detail);
                return MeyerCaptureServiceResult_PreflightRejected;
            }
            if (m_preflight.status != MeyerDeviceCalibrationPreflight_Ready)
            {
                const std::string detail = m_preflight.detailUtf8[0] == '\0'
                    ? "Color calibration preflight was rejected"
                    : std::string(m_preflight.detailUtf8);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_preflightReady = false;
                    m_lastError = detail;
                }
                SetState(MeyerCaptureServiceState_Configured,
                         MeyerCaptureServiceResult_PreflightRejected);
                PublishEvent(MeyerCaptureServiceEvent_PreflightRejected,
                             MeyerCaptureServiceEventSeverity_Warning,
                             m_preflight.status, 0U, detail);
                WriteWarning("PrepareColorCalibration", detail.c_str());
                return MeyerCaptureServiceResult_PreflightRejected;
            }
            if (m_preflight.detectionRecord.isProductionMode != 0 &&
                m_config.allowProductionMode == 0)
            {
                m_deviceCmd.Close();
                const std::string detail =
                    "Production device mode is disabled for this capture workflow";
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_preflightReady = false;
                    m_lastError = detail;
                }
                SetState(MeyerCaptureServiceState_Configured,
                         MeyerCaptureServiceResult_PreflightRejected);
                PublishEvent(MeyerCaptureServiceEvent_PreflightRejected,
                             MeyerCaptureServiceEventSeverity_Warning,
                             MeyerCaptureServiceResult_PreflightRejected,
                             0U, detail);
                return MeyerCaptureServiceResult_PreflightRejected;
            }

            FillCaptureDeviceContext(m_preflight);
            const std::int32_t profileResult = m_processing.GetDefaultProfile(
                m_captureDeviceContext.deviceProfile,
                m_config.captureMode,
                m_profile);
            if (profileResult != MeyerCaptureProcessingResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_ProcessingError,
                         "No capture processing profile matches the detected device");
                return MeyerCaptureServiceResult_InternalError;
            }
            // 配置中的队列容量覆盖 Profile 默认值，但每个机型其它采集参数保持独立。
            m_profile.postProcessQueueCapacity = m_config.postProcessQueueCapacity;
            if (m_processing.Configure(m_profile, m_captureDeviceContext) !=
                MeyerCaptureProcessingResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_ProcessingError,
                         m_processing.LastError());
                return MeyerCaptureServiceResult_InternalError;
            }
            if (m_autoExposure.Configure(m_captureDeviceContext) !=
                MeyerAutoExposureResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_InternalError,
                         m_autoExposure.LastError());
                return MeyerCaptureServiceResult_InternalError;
            }
            if (m_imagePipeline.Configure(m_profile, m_captureDeviceContext) !=
                MeyerCaptureImagePipelineResult_Ok)
            {
                SetFault(MeyerCaptureServiceResult_InternalError,
                         MeyerCaptureServiceEvent_ProcessingError,
                         m_imagePipeline.LastError());
                return MeyerCaptureServiceResult_InternalError;
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_preflightReady = true;
                m_lastError.clear();
            }
            SetState(MeyerCaptureServiceState_Ready,
                     MeyerCaptureServiceResult_Ok);
            PublishEvent(MeyerCaptureServiceEvent_PreflightReady,
                         MeyerCaptureServiceEventSeverity_Info,
                         MeyerCaptureServiceResult_Ok, 0U,
                         "Color calibration device preflight passed");
            WriteInfo("PrepareColorCalibration", "Color calibration preflight passed");
            return MeyerCaptureServiceResult_Ok;
        }

        // 服务设备快照只复制必要字段，原始大块协议数据继续留在 DeviceCmd 内部。
        void CaptureServiceContext::FillDeviceInfo(
            const MeyerDeviceCalibrationPreflight& preflight)
        {
            MeyerCaptureServiceDeviceInfo info = {};
            info.structSize = sizeof(info);
            info.schemaVersion = MEYER_CAPTURE_SERVICE_SCHEMA_VERSION;
            info.preflightStatus = preflight.status;
            info.lastCommandResult = preflight.commandResult;
            info.deviceConnected =
                preflight.state.connectionState == MeyerDeviceConnectionState_Open ? 1 : 0;
            info.usb2 = preflight.state.isUsb2;
            info.productionMode = preflight.detectionRecord.isProductionMode;
            info.deviceNumberStatus = preflight.detectionRecord.deviceNumberStatus;
            info.modelCodeStatus = preflight.detectionRecord.modelCodeStatus;
            info.deviceSeries = preflight.productIdentity.productFamily;
            info.deviceProfile = preflight.productIdentity.protocolProfile;
            info.productModel = preflight.productIdentity.productModel;
            info.deviceModel = preflight.state.model;
            info.mainFirmwareStatus = preflight.firmwareVersions.mainBoardStatus;
            info.projectionFirmwareStatus = preflight.firmwareVersions.projectionBoardStatus;
            info.scanHeadColorPolicy = preflight.scanHeadColorCalibration.policy;
            info.largeHeadColorStatus = preflight.scanHeadColorCalibration.largeHeadStatus;
            info.smallHeadColorStatus = preflight.scanHeadColorCalibration.smallHeadStatus;
            CopyText(info.deviceSeriesUtf8, preflight.productIdentity.seriesNameUtf8);
            CopyText(info.deviceProfileUtf8, preflight.state.modelNameUtf8);
            CopyText(info.reportedDeviceIdUtf8,
                     preflight.detectionRecord.reportedDeviceNumberUtf8);
            CopyText(info.effectiveDeviceIdUtf8,
                     preflight.detectionRecord.effectiveDeviceNumberUtf8);
            CopyText(info.deviceIdStatusUtf8,
                     DeviceNumberStatusText(info.deviceNumberStatus));
            CopyText(info.reportedModelCodeUtf8,
                     preflight.detectionRecord.reportedModelCodeUtf8);
            CopyText(info.effectiveModelCodeUtf8,
                     preflight.detectionRecord.effectiveModelCodeUtf8);
            CopyText(info.productModelUtf8, preflight.productIdentity.productNameUtf8);
            CopyText(info.mainFirmwareVersionUtf8,
                     preflight.firmwareVersions.mainBoardVersionUtf8);
            CopyText(info.projectionFirmwareVersionUtf8,
                     preflight.firmwareVersions.projectionBoardVersionUtf8);
            CopyText(info.detailUtf8, preflight.detailUtf8);
            info.validFields = ~static_cast<std::uint64_t>(0U);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_deviceInfo = info;
        }

        // 每组图都会复制该上下文，异步后处理不会读到后来换机后的可变状态。
        void CaptureServiceContext::FillCaptureDeviceContext(
            const MeyerDeviceCalibrationPreflight& preflight)
        {
            m_processing.InitDeviceContext(m_captureDeviceContext);
            m_captureDeviceContext.validFields = ~static_cast<std::uint64_t>(0U);
            m_captureDeviceContext.deviceSeries =
                preflight.productIdentity.productFamily;
            m_captureDeviceContext.deviceProfile =
                preflight.productIdentity.protocolProfile;
            m_captureDeviceContext.deviceIdStatus =
                preflight.detectionRecord.deviceNumberStatus;
            m_captureDeviceContext.deviceModel = preflight.state.model;
            m_captureDeviceContext.productModel =
                preflight.productIdentity.productModel;
            m_captureDeviceContext.productionMode =
                preflight.detectionRecord.isProductionMode;
            m_captureDeviceContext.captureMode = m_config.captureMode;
            CopyText(m_captureDeviceContext.deviceSeriesUtf8,
                     preflight.productIdentity.seriesNameUtf8);
            CopyText(m_captureDeviceContext.deviceProfileUtf8,
                     preflight.state.modelNameUtf8);
            CopyText(m_captureDeviceContext.deviceIdUtf8,
                     preflight.detectionRecord.reportedDeviceNumberUtf8);
            CopyText(m_captureDeviceContext.modelCodeUtf8,
                     preflight.detectionRecord.effectiveModelCodeUtf8);
            CopyText(m_captureDeviceContext.deviceModelUtf8,
                     preflight.productIdentity.productNameUtf8);
            CopyText(m_captureDeviceContext.firmwareVersionUtf8,
                     preflight.firmwareVersions.mainBoardVersionUtf8);

            // 会话 ID 使用单调时钟刻度，不依赖系统时间回拨，也不包含患者信息。
            const std::uint64_t tick = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            std::snprintf(m_captureDeviceContext.captureSessionIdUtf8,
                          sizeof(m_captureDeviceContext.captureSessionIdUtf8),
                          "capture-%llu",
                          static_cast<unsigned long long>(tick));
        }

    }
}
