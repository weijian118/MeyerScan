// =============================================================================
// 文件: CaptureServiceTestController.cpp
// 作用: 实现测试界面对 CaptureService 的稳定调用顺序和两次缓冲区合同。
// =============================================================================
#include "CaptureServiceTestController.h"

#include <cstring>

CaptureServiceTestController::CaptureServiceTestController()
    : m_handle(MeyerCaptureService_Create())
{
}

CaptureServiceTestController::~CaptureServiceTestController()
{
    if (m_handle != nullptr)
    {
        MeyerCaptureService_StopCapture(m_handle);
        MeyerCaptureService_Shutdown(m_handle);
        MeyerCaptureService_Destroy(m_handle);
        m_handle = nullptr;
    }
}

std::int32_t CaptureServiceTestController::Configure(
    bool simulator, std::uint32_t simulatedFlags)
{
    if (m_handle == nullptr)
    {
        return MeyerCaptureServiceResult_InvalidHandle;
    }
    MeyerCaptureServiceConfig config = {};
    MeyerCaptureService_InitConfig(&config);
    config.backendType = simulator
        ? MeyerCaptureServiceBackend_SimulatorForTest
        : MeyerCaptureServiceBackend_DeviceTransport;
    config.modelHint = 0;
    config.captureMode = MeyerCaptureMode_ColorCalibration;
    config.allowProductionMode = 1;
    config.simulatedFlags = simulatedFlags;
    if (simulator)
    {
        // 13 位编号和 0xCE 型号代码由 DeviceCmd 模拟器共同形成 MyScan 5 身份。
        std::strncpy(config.simulatedDeviceIdUtf8,
                     "6200005301203",
                     sizeof(config.simulatedDeviceIdUtf8) - 1U);
    }
    return MeyerCaptureService_Configure(m_handle, &config);
}

std::int32_t CaptureServiceTestController::PrepareColorCalibration()
{
    return m_handle == nullptr
        ? MeyerCaptureServiceResult_InvalidHandle
        : MeyerCaptureService_PrepareColorCalibration(m_handle);
}

std::int32_t CaptureServiceTestController::StartCapture()
{
    return m_handle == nullptr
        ? MeyerCaptureServiceResult_InvalidHandle
        : MeyerCaptureService_StartCapture(m_handle);
}

std::int32_t CaptureServiceTestController::StopCapture()
{
    return m_handle == nullptr
        ? MeyerCaptureServiceResult_InvalidHandle
        : MeyerCaptureService_StopCapture(m_handle);
}

std::int32_t CaptureServiceTestController::RequestLight(bool on)
{
    return m_handle == nullptr
        ? MeyerCaptureServiceResult_InvalidHandle
        : MeyerCaptureService_RequestLight(m_handle, on ? 1 : 0);
}

bool CaptureServiceTestController::ReadDeviceInfo(
    MeyerCaptureServiceDeviceInfo& info) const
{
    MeyerCaptureService_InitDeviceInfo(&info);
    return m_handle != nullptr &&
           MeyerCaptureService_GetDeviceInfo(m_handle, &info) ==
               MeyerCaptureServiceResult_Ok;
}

bool CaptureServiceTestController::ReadState(
    MeyerCaptureServiceStateSnapshot& state) const
{
    MeyerCaptureService_InitStateSnapshot(&state);
    return m_handle != nullptr &&
           MeyerCaptureService_GetStateSnapshot(m_handle, &state) ==
               MeyerCaptureServiceResult_Ok;
}

bool CaptureServiceTestController::ReadLatestGroupInfo(
    MeyerCaptureGroupInfo& info) const
{
    std::memset(&info, 0, sizeof(info));
    info.structSize = sizeof(info);
    info.schemaVersion = MEYER_CAPTURE_TYPES_SCHEMA_VERSION;
    return m_handle != nullptr &&
           MeyerCaptureService_CopyLatestGroupInfo(m_handle, &info) ==
               MeyerCaptureServiceResult_Ok;
}

bool CaptureServiceTestController::PollEvent(
    MeyerCaptureServiceEvent& eventInfo)
{
    MeyerCaptureService_InitEvent(&eventInfo);
    return m_handle != nullptr &&
           MeyerCaptureService_PollEvent(m_handle, &eventInfo) ==
               MeyerCaptureServiceResult_Ok;
}

std::int32_t CaptureServiceTestController::CopyPlane(
    std::int32_t index, std::vector<unsigned char>& bytes) const
{
    bytes.clear();
    if (m_handle == nullptr)
    {
        return MeyerCaptureServiceResult_InvalidHandle;
    }
    std::size_t required = 0U;
    std::int32_t result = MeyerCaptureService_CopyLatestPlane(
        m_handle, index, nullptr, 0U, &required);
    if (result != MeyerCaptureServiceResult_BufferTooSmall || required == 0U)
    {
        return result;
    }
    bytes.resize(required);
    result = MeyerCaptureService_CopyLatestPlane(
        m_handle, index, &bytes[0], bytes.size(), &required);
    if (result != MeyerCaptureServiceResult_Ok)
    {
        bytes.clear();
    }
    return result;
}

std::int32_t CaptureServiceTestController::CopyRgb888(
    std::vector<unsigned char>& bytes) const
{
    bytes.clear();
    if (m_handle == nullptr)
    {
        return MeyerCaptureServiceResult_InvalidHandle;
    }
    std::size_t required = 0U;
    std::int32_t result = MeyerCaptureService_CopyLatestRgb888(
        m_handle, nullptr, 0U, &required);
    if (result != MeyerCaptureServiceResult_BufferTooSmall || required == 0U)
    {
        return result;
    }
    bytes.resize(required);
    result = MeyerCaptureService_CopyLatestRgb888(
        m_handle, &bytes[0], bytes.size(), &required);
    if (result != MeyerCaptureServiceResult_Ok)
    {
        bytes.clear();
    }
    return result;
}

bool CaptureServiceTestController::ReadPipelineOutputInfo(
    std::int32_t outputType,
    MeyerCapturePipelineOutputInfo& info) const
{
    MeyerCaptureService_InitPipelineOutputInfo(&info);
    return m_handle != nullptr &&
           MeyerCaptureService_GetLatestPipelineOutputInfo(
               m_handle, outputType, &info) == MeyerCaptureServiceResult_Ok;
}

std::int32_t CaptureServiceTestController::CopyPipelineOutput(
    std::int32_t outputType,
    std::vector<unsigned char>& bytes) const
{
    bytes.clear();
    if (m_handle == nullptr)
    {
        return MeyerCaptureServiceResult_InvalidHandle;
    }
    std::size_t required = 0U;
    std::int32_t result = MeyerCaptureService_CopyLatestPipelineOutput(
        m_handle, outputType, nullptr, 0U, &required);
    if (result != MeyerCaptureServiceResult_BufferTooSmall || required == 0U)
    {
        return result;
    }
    bytes.resize(required);
    result = MeyerCaptureService_CopyLatestPipelineOutput(
        m_handle, outputType, &bytes[0], bytes.size(), &required);
    if (result != MeyerCaptureServiceResult_Ok)
    {
        bytes.clear();
    }
    return result;
}

std::string CaptureServiceTestController::LastError() const
{
    if (m_handle == nullptr)
    {
        return "CaptureService handle is null";
    }
    std::size_t required = 0U;
    MeyerCaptureService_GetLastError(m_handle, nullptr, 0U, &required);
    if (required == 0U || required > 8192U)
    {
        return std::string();
    }
    std::vector<char> buffer(required, '\0');
    if (MeyerCaptureService_GetLastError(
            m_handle, &buffer[0], buffer.size(), &required) !=
        MeyerCaptureServiceResult_Ok)
    {
        return std::string();
    }
    return std::string(&buffer[0]);
}
