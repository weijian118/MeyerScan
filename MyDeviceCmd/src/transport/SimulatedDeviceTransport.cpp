// =============================================================================
// 文件: SimulatedDeviceTransport.cpp
// 作用: 用确定性数据模拟协议响应和一帧采集数据，验证完整调用链。
// =============================================================================
#include "SimulatedDeviceTransport.h"

#include "../protocol/DeviceCommandCodec.h"
#include "../protocol/DeviceProtocolDefs.h"

#include <algorithm>
#include <cstring>

namespace meyer
{
    namespace devicecmd
    {
        // 构造模拟后端时先准备所有固定长度响应，后续读命令只复制状态数据。
        SimulatedDeviceTransport::SimulatedDeviceTransport()
            : m_open(false), m_captureActive(false), m_frameReady(false), m_lightOn(false),
              m_deviceId("6200005301203"), m_frameRate(0x14U)
        {
            std::memset(&m_frameInfo, 0, sizeof(m_frameInfo));
            BuildDefaultPayloads();
        }

        // 模拟后端也遵循真实后端的释放入口，确保测试不会遗留采集状态。
        SimulatedDeviceTransport::~SimulatedDeviceTransport()
        {
            Close();
        }

        // 模拟后端必须由调用方显式选择；它不会枚举或打开任何真实 USB 设备。
        std::int32_t SimulatedDeviceTransport::Open(const MeyerDeviceCmdOpenParams& params)
        {
            if (params.simulatedDeviceIdUtf8[0] != '\0')
            {
                m_deviceId.assign(params.simulatedDeviceIdUtf8);
            }
            m_open = true;
            m_lastError.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        void SimulatedDeviceTransport::Close()
        {
            m_open = false;
            m_captureActive = false;
            m_frameReady = false;
            m_pendingResponse.clear();
            m_frame.clear();
        }

        // 模拟连接状态是本地布尔值，不需要访问硬件。
        bool SimulatedDeviceTransport::IsOpen() const
        {
            return m_open;
        }

        // 模拟重连直接恢复打开状态，用于测试宿主的重连分支。
        std::int32_t SimulatedDeviceTransport::Reconnect()
        {
            m_open = true;
            return MeyerDeviceCmdResult_Ok;
        }

        // 解析真实 A 类请求，使 smoke 同时覆盖 DeviceCmd 的发送编码结果。
        std::int32_t SimulatedDeviceTransport::SendCommand(const std::vector<std::uint8_t>& frame,
                                                           std::uint32_t)
        {
            if (!m_open)
            {
                m_lastError = "Simulated device is not open";
                return MeyerDeviceCmdResult_NotOpen;
            }

            protocol::CommandFrame request;
            if (!protocol::DeviceCommandCodec::Parse(&frame[0], frame.size(), request, m_lastError))
            {
                return MeyerDeviceCmdResult_ProtocolError;
            }

            // 无响应控制命令直接更新模拟状态；查询命令进入响应队列。
            if (request.commandCode == protocol::SetLight && request.payload.size() == 1U)
            {
                m_lightOn = request.payload[0] != 0U;
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::StartImageTransfer)
            {
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::StopImageTransfer)
            {
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::ForceLight)
            {
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::ResetController)
            {
                // 模拟后端只记录命令已经被设备接收，不真的销毁当前模拟会话。
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::SetCameraWindowPosition && request.payload.size() == 8U)
            {
                std::copy(request.payload.begin(), request.payload.end(), m_cameraParameters.begin());
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::SetFrameRate && request.payload.size() == 1U)
            {
                m_frameRate = request.payload[0];
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::SetExposure && request.payload.size() == 16U)
            {
                m_exposureParameters.assign(request.payload.begin(), request.payload.end());
                m_exposureParameters.push_back(0U);
                return MeyerDeviceCmdResult_Ok;
            }
            if (request.commandCode == protocol::StoreMachineCode && request.payload.size() == 13U)
            {
                m_deviceId.clear();
                for (std::size_t index = 0U; index < request.payload.size(); ++index)
                {
                    if (request.payload[index] > 9U)
                    {
                        m_lastError = "Simulated machine code contains a non-decimal digit";
                        return MeyerDeviceCmdResult_ProtocolError;
                    }
                    m_deviceId.push_back(static_cast<char>('0' + request.payload[index]));
                }
                std::copy(request.payload.begin(), request.payload.end(), m_deviceInfo.begin() + 2U);
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreCameraParameters && request.payload.size() == 16U)
            {
                m_cameraParameters = request.payload;
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreColorMatrix && request.payload.size() == 416U)
            {
                m_colorMatrix = request.payload;
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreCamera1Calibration && request.payload.size() == 382U)
            {
                m_camera1Calibration = request.payload;
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreCamera2Calibration && request.payload.size() == 382U)
            {
                m_camera2Calibration = request.payload;
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreColorCalibration && request.payload.size() == 72U)
            {
                m_colorCalibration = request.payload;
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::StoreDeviceInfo && request.payload.size() == 382U)
            {
                m_deviceInfo = request.payload;
                m_deviceId.clear();
                for (std::size_t index = 2U; index < 15U; ++index)
                {
                    m_deviceId.push_back(static_cast<char>('0' + (request.payload[index] % 10U)));
                }
                return QueueResponse(request.commandCode, &request.payload);
            }
            if (request.commandCode == protocol::EraseFirmware && request.payload.empty())
            {
                return QueueResponse(request.commandCode);
            }
            if (request.commandCode == protocol::WriteFirmware && request.payload.size() == 262U)
            {
                return QueueResponse(request.commandCode, &request.payload);
            }

            return QueueResponse(request.commandCode, &request.payload);
        }

        // 只有查询命令会产生待处理响应；没有响应时用 Timeout 模拟真实等待结果。
        std::int32_t SimulatedDeviceTransport::ReceiveCommand(std::vector<std::uint8_t>& frame,
                                                              std::size_t capacity,
                                                              std::uint32_t)
        {
            if (m_pendingResponse.empty())
            {
                m_lastError = "Simulator has no pending command response";
                return MeyerDeviceCmdResult_Timeout;
            }
            if (capacity < m_pendingResponse.size())
            {
                m_lastError = "Command response buffer is too small";
                return MeyerDeviceCmdResult_BufferTooSmall;
            }

            frame = m_pendingResponse;
            m_pendingResponse.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        // 模拟环境固定返回一个设备，避免测试依赖主机 USB 枚举状态。
        std::int32_t SimulatedDeviceTransport::GetDeviceCount(std::int32_t& deviceCount)
        {
            deviceCount = 1;
            return MeyerDeviceCmdResult_Ok;
        }

        // 使用 USB3 标志模拟协议文档中的高速传输场景。
        std::int32_t SimulatedDeviceTransport::GetIsUsb2(std::int32_t& isUsb2)
        {
            isUsb2 = 0;
            return MeyerDeviceCmdResult_Ok;
        }

        // 生成一帧小型或标准尺寸的确定性数据，不模拟真实成像质量。
        std::int32_t SimulatedDeviceTransport::StartCapture(const MeyerDeviceCmdCaptureParams& params,
                                                            std::int32_t)
        {
            if (!m_open)
            {
                return MeyerDeviceCmdResult_NotOpen;
            }
            if (m_captureActive)
            {
                return MeyerDeviceCmdResult_Busy;
            }

            const std::uint64_t byteCount =
                static_cast<std::uint64_t>(params.width) *
                static_cast<std::uint64_t>(params.height) *
                static_cast<std::uint64_t>(params.imageCount);
            if (byteCount == 0U || byteCount > 256ULL * 1024ULL * 1024ULL)
            {
                m_lastError = "Simulated capture size is outside the supported range";
                return MeyerDeviceCmdResult_InvalidArgument;
            }

            m_frame.resize(static_cast<std::size_t>(byteCount), 0U);
            for (std::size_t index = 0U; index < m_frame.size(); ++index)
            {
                // 使用递增低 8 位模式，测试可验证数据确实被复制而不是全零占位。
                m_frame[index] = static_cast<std::uint8_t>(index & 0xFFU);
            }

            std::memset(&m_frameInfo, 0, sizeof(m_frameInfo));
            m_frameInfo.structSize = sizeof(MeyerDeviceCmdFrameInfo);
            m_frameInfo.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            m_frameInfo.width = params.width;
            m_frameInfo.height = params.height;
            m_frameInfo.imageCount = params.imageCount;
            m_frameInfo.captureStatus = 1;
            m_frameInfo.workMode = params.workMode;
            m_frameInfo.pictureOrderMode = params.pictureOrderMode;
            m_frameInfo.scanHeadType = params.scanHeadType;
            m_frameInfo.ledOn = m_lightOn ? 1 : 0;
            m_frameInfo.temperature0 = 31;
            m_frameInfo.temperature1 = 32;
            m_frameInfo.temperature2 = 33;
            m_frameInfo.temperature3 = 34;
            m_frameInfo.frameBytes = byteCount;
            m_captureActive = true;
            m_frameReady = true;
            return MeyerDeviceCmdResult_Ok;
        }

        // 停止操作清空未交付帧，模拟真实 Transport 的资源回收语义。
        std::int32_t SimulatedDeviceTransport::StopCapture()
        {
            m_captureActive = false;
            m_frameReady = false;
            m_frame.clear();
            return MeyerDeviceCmdResult_Ok;
        }

        // 返回本地采集状态，测试可据此验证命令互斥逻辑。
        bool SimulatedDeviceTransport::IsCaptureActive() const
        {
            return m_captureActive;
        }

        std::int32_t SimulatedDeviceTransport::GetFrame(unsigned char* buffer,
                                                        std::size_t capacity,
                                                        std::size_t& frameBytes,
                                                        MeyerDeviceCmdFrameInfo& frameInfo)
        {
            frameBytes = m_frame.size();
            if (!m_captureActive || !m_frameReady)
            {
                return MeyerDeviceCmdResult_NotReady;
            }
            if (buffer == nullptr || capacity < m_frame.size())
            {
                return MeyerDeviceCmdResult_BufferTooSmall;
            }

            // 使用显式下标复制，避免 VS2015 Checked Iterators 对跨容器/裸指针
            // 的通用 std::copy 给出误导性的安全警告，同时保持边界已校验。
            for (std::size_t index = 0U; index < m_frame.size(); ++index)
            {
                buffer[index] = m_frame[index];
            }
            frameInfo = m_frameInfo;
            m_frameReady = false;
            return MeyerDeviceCmdResult_Ok;
        }

        // 返回模拟后端保存的诊断文本，不创建临时字符串。
        const std::string& SimulatedDeviceTransport::LastError() const
        {
            return m_lastError;
        }

        // 为协议中所有需要响应的 A 类命令生成确定性回复；未知命令作为无响应
        // 发送成功处理，便于原始命令调试接口在模拟环境中保持向前兼容。
        std::int32_t SimulatedDeviceTransport::QueueResponse(
            std::uint8_t requestCode,
            const std::vector<std::uint8_t>* requestPayload)
        {
            std::uint8_t responseCode = 0U;
            std::vector<std::uint8_t> payload;
            switch (requestCode)
            {
            case protocol::ReadMachineCode:
                responseCode = protocol::UploadMachineCode;
                BuildMachineCodePayload(payload);
                break;
            case protocol::ReadMainBoardVersion:
                responseCode = protocol::UploadMainBoardVersion;
                payload.push_back(1U);
                payload.push_back(2U);
                payload.push_back(0x03U);
                payload.push_back(0xE9U);
                break;
            case protocol::ReadBattery:
                responseCode = protocol::UploadBattery;
                payload.push_back(0xFFU);
                payload.push_back(80U);
                payload.push_back(90U);
                break;
            case protocol::ReadDeviceInfo:
                responseCode = protocol::UploadDeviceInfo;
                payload = m_deviceInfo;
                {
                    std::vector<std::uint8_t> machineCode;
                    BuildMachineCodePayload(machineCode);
                    std::copy(machineCode.begin(), machineCode.end(), payload.begin() + 2U);
                }
                break;
            case protocol::StoreMachineCode:
                responseCode = protocol::MachineCodeStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadCameraParameters:
                responseCode = protocol::UploadCameraParameters;
                payload = m_cameraParameters;
                break;
            case protocol::StoreCameraParameters:
                responseCode = protocol::CameraParametersStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadColorMatrix:
                responseCode = protocol::UploadColorMatrix;
                payload = m_colorMatrix;
                break;
            case protocol::StoreColorMatrix:
                responseCode = protocol::ColorMatrixStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadTemperature:
                responseCode = protocol::UploadTemperature;
                payload = m_temperature;
                break;
            case protocol::EraseFirmware:
                responseCode = protocol::EraseFirmwareProgress;
                payload.push_back(0x00U);
                payload.push_back(0x10U);
                payload.push_back(0x00U);
                payload.push_back(0x10U);
                break;
            case protocol::WriteFirmware:
                responseCode = protocol::WriteFirmwareProgress;
                if (requestPayload == nullptr || requestPayload->size() != 262U)
                {
                    m_lastError = "Simulated firmware packet has an invalid length";
                    return MeyerDeviceCmdResult_InvalidArgument;
                }
                payload.assign(requestPayload->begin(), requestPayload->begin() + 6U);
                break;
            case protocol::ReadCamera1Calibration:
                responseCode = protocol::UploadCamera1Calibration;
                payload = m_camera1Calibration;
                break;
            case protocol::StoreCamera1Calibration:
                responseCode = protocol::Camera1CalibrationStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadCamera2Calibration:
                responseCode = protocol::UploadCamera2Calibration;
                payload = m_camera2Calibration;
                break;
            case protocol::StoreCamera2Calibration:
                responseCode = protocol::Camera2CalibrationStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadColorCalibration:
                responseCode = protocol::UploadColorCalibration;
                payload = m_colorCalibration;
                break;
            case protocol::StoreColorCalibration:
                responseCode = protocol::ColorCalibrationStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::StoreDeviceInfo:
                responseCode = protocol::DeviceInfoStoreReply;
                payload.push_back(0xFFU);
                break;
            case protocol::ReadExposure:
                responseCode = protocol::UploadExposure;
                payload = m_exposureParameters;
                break;
            default:
                m_pendingResponse.clear();
                return MeyerDeviceCmdResult_Ok;
            }

            std::string codecError;
            if (!protocol::DeviceCommandCodec::Build(responseCode,
                                                      payload.empty() ? nullptr : &payload[0],
                                                      payload.size(),
                                                      0U,
                                                      m_pendingResponse,
                                                      codecError))
            {
                m_lastError = codecError;
                return MeyerDeviceCmdResult_ProtocolError;
            }
            return MeyerDeviceCmdResult_Ok;
        }

        // 初始化协议扩展命令的确定性默认数据，让读写 smoke 可以验证完整长度。
        void SimulatedDeviceTransport::BuildDefaultPayloads()
        {
            m_cameraParameters.assign(16U, 0U);
            m_cameraParameters[0] = 0x00U;
            m_cameraParameters[1] = 0xF8U;
            m_cameraParameters[2] = 0x00U;
            m_cameraParameters[3] = 0x6CU;
            m_cameraParameters[4] = 0x00U;
            m_cameraParameters[5] = 0xF8U;
            m_cameraParameters[6] = 0x00U;
            m_cameraParameters[7] = 0x6CU;
            m_cameraParameters[8] = 0x36U;
            m_cameraParameters[9] = 0x11U;
            m_cameraParameters[10] = 0x36U;
            m_cameraParameters[11] = 0x11U;
            m_cameraParameters[12] = 0x68U;
            m_cameraParameters[13] = 0x73U;
            m_cameraParameters[14] = 0x10U;
            m_cameraParameters[15] = 0x04U;

            m_colorMatrix.resize(416U);
            for (std::size_t index = 0U; index < m_colorMatrix.size(); ++index)
            {
                m_colorMatrix[index] = static_cast<std::uint8_t>(index & 0xFFU);
            }

            m_temperature = { 0x01U, 0x2CU, 0x01U, 0x40U, 0x01U, 0x54U, 0x00U };

            m_camera1Calibration.resize(382U);
            m_camera2Calibration.resize(382U);
            for (std::size_t index = 0U; index < 382U; ++index)
            {
                m_camera1Calibration[index] = static_cast<std::uint8_t>((index + 1U) & 0xFFU);
                m_camera2Calibration[index] = static_cast<std::uint8_t>((index + 2U) & 0xFFU);
            }

            m_colorCalibration.resize(72U);
            for (std::size_t index = 0U; index < m_colorCalibration.size(); ++index)
            {
                m_colorCalibration[index] = static_cast<std::uint8_t>(0x80U + index);
            }

            m_deviceInfo.assign(382U, 0U);
            m_deviceInfo[0] = 1U;
            m_deviceInfo[1] = 2U;
            std::vector<std::uint8_t> machineCode;
            BuildMachineCodePayload(machineCode);
            std::copy(machineCode.begin(), machineCode.end(), m_deviceInfo.begin() + 2U);
            std::fill(m_deviceInfo.begin() + 15U, m_deviceInfo.begin() + 45U, 0x78U);

            m_exposureParameters.assign(17U, 0x11U);
            m_exposureParameters[16] = 0U;
        }

        // 读取模拟协议 payload 中的一个大端 16 位字段，供后续扩展测试复用。
        // 按协议大端顺序读取模拟 payload；越界时返回 0 保护测试后端。
        std::uint16_t SimulatedDeviceTransport::ReadBigEndian16(
            const std::vector<std::uint8_t>& payload,
            std::size_t offset)
        {
            if (offset + 1U >= payload.size())
            {
                return 0U;
            }
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(payload[offset]) << 8U) |
                static_cast<std::uint16_t>(payload[offset + 1U]));
        }

        // 协议示例把机器码每一位存成 0~9 数值，而不是 ASCII '0'~'9'。
        void SimulatedDeviceTransport::BuildMachineCodePayload(std::vector<std::uint8_t>& payload) const
        {
            payload.assign(13U, 0U);
            const std::size_t count = (std::min)(payload.size(), m_deviceId.size());
            for (std::size_t index = 0U; index < count; ++index)
            {
                const char digit = m_deviceId[index];
                payload[index] = digit >= '0' && digit <= '9'
                    ? static_cast<std::uint8_t>(digit - '0')
                    : 0U;
            }
        }
    }
}
