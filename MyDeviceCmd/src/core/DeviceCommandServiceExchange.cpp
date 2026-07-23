#include "DeviceCommandServiceInternal.h"

namespace meyer
{
    namespace devicecmd
    {
        const std::string& DeviceCommandService::LastError() const
        {
            return m_lastError;
        }

        // 所有 A 类命令都从这里经过，保证编码、日志、响应校验和超时行为一致。
        std::int32_t DeviceCommandService::ExecuteCommand(std::uint8_t commandCode,
                                                          const unsigned char* payload,
                                                          std::size_t payloadSize,
                                                          std::int32_t expectedResponseCode,
                                                          protocol::CommandFrame* response,
                                                          std::uint32_t timeoutMs,
                                                          CommandExchangeDiagnostics* diagnostics)
        {
            const std::chrono::steady_clock::time_point exchangeStartedAt =
                std::chrono::steady_clock::now();
            // 每次调用先重置诊断，调用方不会误读上一次命令留下的回包状态。
            if (diagnostics != nullptr)
            {
                *diagnostics = CommandExchangeDiagnostics();
            }
            if (!IsOpen())
            {
                return SetError(MeyerDeviceCmdResult_NotOpen, "Device is not open");
            }
            const std::uint64_t requiredCapability = RequiredCapabilityForCommand(commandCode);
            if (requiredCapability != 0U &&
                (m_profile == nullptr || (m_profile->capabilities & requiredCapability) == 0U))
            {
                return SetError(MeyerDeviceCmdResult_UnsupportedModel,
                                "Current model does not declare capability for this command");
            }
            if (m_transport->IsCaptureActive() && expectedResponseCode != MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                return SetError(MeyerDeviceCmdResult_Busy,
                                "Commands with responses are not allowed while capture is active");
            }
            if (expectedResponseCode < MEYER_DEVICE_CMD_NO_RESPONSE || expectedResponseCode > 0xFF)
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument,
                                "Expected response code must be -1 or one byte");
            }

            std::vector<std::uint8_t> encoded;
            std::string codecError;
            if (!protocol::DeviceCommandCodec::Build(commandCode,
                                                      payload,
                                                      payloadSize,
                                                      protocol::kHostTrailerZeroCount,
                                                      encoded,
                                                      codecError))
            {
                return SetError(MeyerDeviceCmdResult_InvalidArgument, codecError);
            }

            const std::uint32_t effectiveTimeout = timeoutMs == 0U
                ? m_lastOpenParams.commandTimeoutMs
                : timeoutMs;
            // 只有上一条请求没有完成“收到、校验并解析期望回包”的完整交换时，
            // 才在本次发送前等待 20 ms。若上一条回包已处理完成，设备已经响应，
            // 无需再人为补足固定间隔，本次命令可立即发送。
            if (m_waitBeforeNextCommand)
            {
                if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport)
                {
                    const std::chrono::steady_clock::time_point waitStartedAt =
                        std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(kUnresolvedResponseWaitMs);
                    if (diagnostics != nullptr)
                    {
                        diagnostics->preSendWaitUs = ElapsedMicroseconds(waitStartedAt);
                    }
                }

                // 等待结束后，上一条请求的兜底响应窗口已经结束。随后仅由本次
                // SendCommand 的成功结果决定是否要求下一条命令再次等待。
                m_waitBeforeNextCommand = false;
            }

            // 实机确认旧 mOS MyScan 在主控板 0x15 已返回后，仍不能立即处理
            // 紧随其后的投图板 0x12。该等待属于机型硬件板间切换时序，集中
            // 保存在 Profile，不得重新扩大为所有命令之间的固定间隔。
            if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport &&
                commandCode == protocol::ReadProjectionBoardVersion &&
                m_profile != nullptr &&
                m_profile->projectionBoardSwitchDelayMs > 0U)
            {
                const std::chrono::steady_clock::time_point settleStartedAt =
                    std::chrono::steady_clock::now();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(m_profile->projectionBoardSwitchDelayMs));
                if (diagnostics != nullptr)
                {
                    diagnostics->profileSettleWaitUs = ElapsedMicroseconds(settleStartedAt);
                }
            }

            const std::chrono::steady_clock::time_point sendStartedAt =
                std::chrono::steady_clock::now();
            std::int32_t result = m_transport->SendCommand(encoded, effectiveTimeout);
            if (diagnostics != nullptr)
            {
                diagnostics->sendUs = ElapsedMicroseconds(sendStartedAt);
            }
            if (result != MeyerDeviceCmdResult_Ok)
            {
                FinishCommandDiagnostics(diagnostics, result, exchangeStartedAt);
                return SetError(result, m_transport->LastError());
            }
            if (diagnostics != nullptr)
            {
                diagnostics->requestSent = true;
            }
            // 请求一旦成功发出，在收到并解析期望回包前，都必须认为设备仍可能响应。
                // 无响应命令也保持该状态，使下一条命令发送前补足 20 ms 响应窗口。
            m_waitBeforeNextCommand = true;

            std::ostringstream sentMessage;
            sentMessage << "Command 0x" << std::hex << std::uppercase
                        << static_cast<unsigned int>(commandCode) << " sent";
            logging::WriteInfo("CommandSent", sentMessage.str().c_str());

            if (expectedResponseCode == MEYER_DEVICE_CMD_NO_RESPONSE)
            {
                FinishCommandDiagnostics(diagnostics, MeyerDeviceCmdResult_Ok, exchangeStartedAt);
                m_lastError.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            // D4/D9 和 CD/CE 按实机要求在当前命令发送后等待 50 ms，再提交 Bulk IN。
            // 这是当前请求的接收时序，不是两条命令之间的固定间隔。版本命令发送后
            // 直接提交阻塞式 Bulk IN，由 ReceiveCommand 等待设备回包。
            if (m_lastOpenParams.backendType == MeyerDeviceCmdBackend_DeviceTransport)
            {
                if (commandCode == protocol::ReadMachineCode ||
                    commandCode == protocol::ReadDeviceInfo)
                {
                    const std::chrono::steady_clock::time_point waitStartedAt =
                        std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(kDeviceInformationResponseDelayMs);
                    if (diagnostics != nullptr)
                    {
                        diagnostics->postSendWaitUs = ElapsedMicroseconds(waitStartedAt);
                    }
                }
            }

            std::vector<std::uint8_t> received;
            const std::chrono::steady_clock::time_point receiveStartedAt =
                std::chrono::steady_clock::now();
            result = m_transport->ReceiveCommand(received,
                                                 MEYER_DEVICE_CMD_MAX_RAW_RESPONSE_BYTES + 10U,
                                                 effectiveTimeout);
            if (diagnostics != nullptr)
            {
                diagnostics->receiveUs = ElapsedMicroseconds(receiveStartedAt);
            }
            if (result != MeyerDeviceCmdResult_Ok)
            {
                FinishCommandDiagnostics(diagnostics, result, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    // 身份探测会把“无回包”解释为旧固件/命令能力证据，因此先按
                    // warning 保存诊断，最终是否失败由上层检测流程决定。
                    m_lastError = m_transport->LastError();
                    logging::WriteWarning("CommandReceiveDiagnostic", m_lastError.c_str());
                    return result;
                }
                return SetError(result, m_transport->LastError());
            }
            if (diagnostics != nullptr)
            {
                diagnostics->responseReceived = true;
                diagnostics->rawResponse = received;
            }

            protocol::CommandFrame decoded;
            const std::chrono::steady_clock::time_point parseStartedAt =
                std::chrono::steady_clock::now();
            const protocol::CommandParseStatus parseStatus =
                protocol::DeviceCommandCodec::ParseDetailed(
                    received.empty() ? nullptr : &received[0],
                    received.size(),
                    decoded,
                    codecError);
            if (diagnostics != nullptr)
            {
                diagnostics->frameParseUs = ElapsedMicroseconds(parseStartedAt);
            }
            if (diagnostics != nullptr)
            {
                diagnostics->parseStatus = parseStatus;
            }
            if (parseStatus != protocol::CommandParseStatus::Ok)
            {
                // 0xFFFF 未初始化和求和校验失败虽然不能生成普通 CommandFrame，
                // 但身份识别流程能够把它们解析成明确的设备终态。只要帧头和
                // 命令码表明它确实是当前请求的期望回包，就说明设备已经响应，
                // 下一条命令无需再为本请求保留 20 ms 响应窗口。
                const bool isRecognizedTerminalResponse =
                    received.size() >= 5U &&
                    received[0] == protocol::kHeader0 &&
                    received[1] == protocol::kHeader1 &&
                    received[2] == static_cast<std::uint8_t>(expectedResponseCode) &&
                    (parseStatus == protocol::CommandParseStatus::UninitializedLength ||
                     parseStatus == protocol::CommandParseStatus::ChecksumMismatch);
                if (isRecognizedTerminalResponse)
                {
                    m_waitBeforeNextCommand = false;
                }

                FinishCommandDiagnostics(
                    diagnostics, MeyerDeviceCmdResult_ProtocolError, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    m_lastError = codecError;
                    logging::WriteWarning("CommandParseDiagnostic", m_lastError.c_str());
                    return MeyerDeviceCmdResult_ProtocolError;
                }
                return SetError(MeyerDeviceCmdResult_ProtocolError, codecError);
            }
            if (decoded.commandCode != static_cast<std::uint8_t>(expectedResponseCode))
            {
                FinishCommandDiagnostics(
                    diagnostics, MeyerDeviceCmdResult_ProtocolError, exchangeStartedAt);
                if (diagnostics != nullptr)
                {
                    diagnostics->parseStatus = protocol::CommandParseStatus::UnexpectedCommand;
                    m_lastError = "Device returned an unexpected command response code";
                    logging::WriteWarning("CommandParseDiagnostic", m_lastError.c_str());
                    return MeyerDeviceCmdResult_ProtocolError;
                }
                return SetError(MeyerDeviceCmdResult_ProtocolError,
                                "Device returned an unexpected command response code");
            }
            if (response != nullptr)
            {
                *response = decoded;
            }

            // 已经收到、校验并解析出期望命令码，上一条请求的响应窗口在此结束。
            // 下一条命令可以立即发送，无需再等待 20 ms。
            m_waitBeforeNextCommand = false;
            FinishCommandDiagnostics(diagnostics, MeyerDeviceCmdResult_Ok, exchangeStartedAt);
            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

    }
}
