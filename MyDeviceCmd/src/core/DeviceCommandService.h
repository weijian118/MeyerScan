// =============================================================================
// 文件: DeviceCommandService.h
// 作用: 声明设备命令、状态缓存和采集编排的内部门面。
//
// 整体设计:
//   - 公共 C ABI 在 DeviceCmdApi.cpp 把 MeyerDeviceCmdHandle 还原为内部上下文，
//     并通过每句柄 mutex 串行所有调用；DeviceCommandService 不暴露给其它 DLL。
//   - ExecuteCommand 是所有 A 类命令的唯一交换入口，统一做能力检查、组帧、发送、
//     接收、校验和响应码核对。ReadMachineCode 等语义函数只解释 payload。
//   - PrepareColorCalibration 是只读预检编排，不执行写 Flash、校准计算或 UI 操作；
//     失败时复制已取得的证据后关闭会话，Ready 时由 DeviceSessionHost 继续持有。
//   - reported 字段只保存设备真实回包，effective 字段才允许保存兼容默认值；
//     两者及来源必须一起传递，禁止用默认值覆盖真实字段。
//   - m_state 是当前句柄内存快照，validFields 区分“真实值为 0”和“尚未读取”；
//     调用方只能通过 GetStateSnapshot 获得副本，不能持有成员地址。
// =============================================================================
#pragma once

#include "../../include/DeviceCmd.h"
#include "../model/DeviceModelCatalog.h"
#include "../protocol/DeviceCommandCodec.h"
#include "../transport/IRawDeviceTransport.h"

#include <memory>
#include <string>
#include <vector>

namespace meyer
{
    namespace devicecmd
    {
        // 一次命令交换的内部诊断。原始回包只在 DLL 内短期保存，不进入公共结构。
        struct CommandExchangeDiagnostics
        {
            bool requestSent;
            bool responseReceived;
            protocol::CommandParseStatus parseStatus;
            std::vector<std::uint8_t> rawResponse;
            std::int32_t result;
            std::uint64_t preSendWaitUs;
            std::uint64_t profileSettleWaitUs;
            std::uint64_t sendUs;
            std::uint64_t postSendWaitUs;
            std::uint64_t receiveUs;
            std::uint64_t frameParseUs;
            std::uint64_t exchangeTotalUs;

            // 默认状态表示尚未发出请求，便于失败分支准确判断停在哪一步。
            CommandExchangeDiagnostics()
                : requestSent(false), responseReceived(false),
                  parseStatus(protocol::CommandParseStatus::NotRun),
                  result(MeyerDeviceCmdResult_Ok), preSendWaitUs(0U),
                  profileSettleWaitUs(0U), sendUs(0U),
                  postSendWaitUs(0U), receiveUs(0U), frameParseUs(0U), exchangeTotalUs(0U)
            {
            }
        };

        class DeviceCommandService
        {
        public:
            // 构造空设备会话，初始状态为未连接且不持有底层 Transport。
            DeviceCommandService();
            // 析构时停止采集、关闭设备并释放动态加载的 Transport DLL。
            ~DeviceCommandService();

            // 根据型号和后端类型创建唯一设备会话并打开设备。
            std::int32_t Open(const MeyerDeviceCmdOpenParams& params);
            // 幂等关闭设备并清理采集状态。
            std::int32_t Close();
            // 查询当前 Transport 是否已经打开。
            bool IsOpen() const;
            // 使用最近一次打开参数重新连接设备。
            std::int32_t Reconnect();

            // 为颜色校准建立空闲会话，依次检查连接/USB、机器码、设备型号和
            // 必需的下位机版本；成功后保留唯一会话供颜色校准继续使用。
            std::int32_t PrepareColorCalibration(const MeyerDeviceCmdOpenParams& params,
                                                 MeyerDeviceCalibrationPreflight& preflight);

            // 顺序读取机器码、主板版本、电池和授权信息等基础状态。
            std::int32_t RefreshBasicState();
            // 复制最近一次状态快照，不触发新的 USB 请求。
            std::int32_t GetStateSnapshot(MeyerDeviceStateSnapshot& snapshot) const;
            // 发送普通开灯或关灯命令，并更新最近请求状态。
            std::int32_t SetLight(bool on);
            // 发送强制开灯模式命令。
            std::int32_t SetForceLight(bool enabled);
            // 发送控制器软件复位命令。
            std::int32_t ResetController();
            // 发送 0xD4 并解析 0xD9 机器码，同时更新状态快照。
            std::int32_t ReadMachineCode(MeyerDeviceCmdMachineCode& machineCode);
            // 将 13 位十进制机器码转换后固化到设备。
            std::int32_t StoreMachineCode(const char* machineCodeUtf8);
            // 读取 16 字节相机参数并转换为公共结构。
            std::int32_t ReadCameraParameters(MeyerDeviceCmdCameraParameters& parameters);
            // 将相机参数结构转换为 16 字节协议数据并固化。
            std::int32_t StoreCameraParameters(const MeyerDeviceCmdCameraParameters& parameters);
            // 在线设置相机 1/2 开窗位置，不写入 Flash。
            std::int32_t SetCameraWindowPosition(const MeyerDeviceCmdCameraWindowPosition& position);
            // 读取 416 字节颜色校正矩阵。
            std::int32_t ReadColorMatrix(MeyerDeviceCmdColorMatrix& matrix);
            // 固化 416 字节颜色校正矩阵。
            std::int32_t StoreColorMatrix(const MeyerDeviceCmdColorMatrix& matrix);
            // 读取镜头、基板和扫描头原始温度电压。
            std::int32_t ReadTemperature(MeyerDeviceCmdTemperature& temperature);
            // 设置协议允许的 18/20/22/25 帧率。
            std::int32_t SetFrameRate(std::int32_t framesPerSecond);
            // 发送固件擦除命令并解析一条擦除进度响应。
            std::int32_t EraseFirmware(MeyerDeviceCmdFirmwareEraseProgress& progress, std::uint32_t timeoutMs);
            // 发送一个 256 字节固件分包并核对设备应答。
            std::int32_t WriteFirmwarePacket(const MeyerDeviceCmdFirmwareWritePacket& packet,
                                             MeyerDeviceCmdFirmwareWriteProgress& progress,
                                             std::uint32_t timeoutMs);
            // 读取相机 1 标定参数。
            std::int32_t ReadCamera1Calibration(MeyerDeviceCmdCameraCalibration& calibration);
            // 固化相机 1 标定参数。
            std::int32_t StoreCamera1Calibration(const MeyerDeviceCmdCameraCalibration& calibration);
            // 读取相机 2 标定参数。
            std::int32_t ReadCamera2Calibration(MeyerDeviceCmdCameraCalibration& calibration);
            // 固化相机 2 标定参数。
            std::int32_t StoreCamera2Calibration(const MeyerDeviceCmdCameraCalibration& calibration);
            // 读取 72 字节颜色标定参数。
            std::int32_t ReadColorCalibration(MeyerDeviceCmdColorCalibration& calibration);
            // 固化 72 字节颜色标定参数。
            std::int32_t StoreColorCalibration(const MeyerDeviceCmdColorCalibration& calibration);
            // 读取加密标志、设备编号和期限原始数据。
            std::int32_t ReadDeviceInfo(MeyerDeviceCmdDeviceInfo& info);
            // 固化加密标志、设备编号和期限原始数据。
            std::int32_t StoreDeviceInfo(const MeyerDeviceCmdDeviceInfo& info);
            // 在线设置两路相机白图/条纹曝光参数。
            std::int32_t SetExposureParameters(const MeyerDeviceCmdExposureParameters& parameters);
            // 读取两路相机白图/条纹曝光参数。
            std::int32_t ReadExposureParameters(MeyerDeviceCmdExposureParameters& parameters);
            // 发送尚未封装语义接口的 A 类命令，同时复用统一帧校验和错误边界。
            std::int32_t ExecuteRawCommand(std::uint8_t commandCode,
                                           const unsigned char* payload,
                                           std::size_t payloadSize,
                                           std::int32_t expectedResponseCode,
                                           MeyerDeviceCmdRawResponse* response,
                                           std::uint32_t timeoutMs);

            // 先建立异步接收队列，再发送 0x0A 开始传图。
            std::int32_t StartCapture(const MeyerDeviceCmdCaptureParams& params);
            // 按型号顺序发送停止/关灯命令并释放采集资源。
            std::int32_t StopCapture(bool turnLightOff);
            // 非阻塞取得一帧完整图像和帧元数据。
            std::int32_t GetFrame(unsigned char* buffer,
                                  std::size_t capacity,
                                  std::size_t& frameBytes,
                                  MeyerDeviceCmdFrameInfo& frameInfo);
            // 只建立原始 B 包流并发送 0x0A，不使用 Transport 的旧组帧器。
            std::int32_t StartRawCapture(const MeyerDeviceCmdCaptureParams& params);
            // 发送停图/关灯并回收原始流。
            std::int32_t StopRawCapture(bool turnLightOff);
            // 阻塞读取一个原始 B 包。
            std::int32_t ReceiveRawCapturePacket(unsigned char* buffer,
                                                 std::size_t capacity,
                                                 std::size_t& receivedSize,
                                                 std::uint32_t timeoutMs);
            // 复制底层原始流诊断快照。
            std::int32_t GetStreamDiagnostics(MeyerDeviceCmdStreamDiagnostics& diagnostics);
            // 仅查看底层句柄，不使用 A 类命令。
            bool IsDeviceConnectedLightweight() const;

            // 返回最近一次失败操作的诊断文本，不执行新的设备操作。
            const std::string& LastError() const;

        private:
            // 编码、发送、接收并校验一条 A 类命令；所有语义命令共用此入口。
            std::int32_t ExecuteCommand(std::uint8_t commandCode,
                                        const unsigned char* payload,
                                        std::size_t payloadSize,
                                        std::int32_t expectedResponseCode,
                                        protocol::CommandFrame* response,
                                        std::uint32_t timeoutMs,
                                        CommandExchangeDiagnostics* diagnostics = nullptr);
            // 读取主板版本响应并更新状态快照。
            std::int32_t RefreshFirmwareVersion();
            // 按已识别的机型读取主控板版本；mOS MyScan 还读取投图板版本。
            std::int32_t RefreshFirmwareVersions();
            // 读取 mOS MyScan 独有的 0x12/0x13 投图板版本响应。
            std::int32_t RefreshProjectionBoardFirmwareVersion();
            // 解析四字节版本 payload，并写入状态快照和对应有效位。
            std::int32_t ReadFirmwareVersionPayload(std::uint8_t requestCode,
                                                    std::uint8_t responseCode,
                                                    char* destination,
                                                    std::size_t destinationCapacity,
                                                    std::uint64_t stateField,
                                                    const char* operation);
            // 将状态快照中的版本值和读取状态复制到预检 POD，供宿主/UI继续传递。
            void FillFirmwareVersionSnapshot(MeyerDeviceFirmwareVersionSnapshot& snapshot,
                                              std::int32_t lastResult) const;
            // 根据主控板版本判断 MyScan 5/6 是否支持双扫描头颜色校准。
            // 规则和扫描头状态读取集中在 DeviceCmd，UI 不重复解析版本文本。
            std::int32_t CheckColorCalibrationFirmwareCompatibility(
                MeyerDeviceScanHeadColorCalibrationSnapshot& snapshot) const;
            // 读取大扫描头 A3/A4 和小扫描头 B9/BA 的颜色参数存在性。
            // 校验和失败返回业务成功并记录 NotCalibrated，其它异常返回失败。
            std::int32_t ReadScanHeadColorCalibrationSnapshot(
                MeyerDeviceScanHeadColorCalibrationSnapshot& snapshot);
            // 执行一条扫描头颜色参数读取命令，并把协议诊断归一成公共状态。
            std::int32_t ReadOneScanHeadColorCalibration(
                std::uint8_t requestCode,
                std::uint8_t responseCode,
                std::int32_t& status,
                std::int32_t& commandResult);
            // 读取电池响应并更新状态快照。
            std::int32_t RefreshBattery();
            // 读取设备授权信息并更新状态快照。
            std::int32_t RefreshDeviceSecurityInfo();
            // 读取并严格验证 0xD4/0xD9 设备编号；长度 0xFFFF 和求和校验失败
            // 均进入生产模式，同时保留不同的 deviceNumberStatus。
            std::int32_t ReadDeviceNumberForDetection(MeyerDeviceCmdMachineCode& machineCode,
                                                      MeyerDeviceDetectionRecord& record);
            // 生产模式下通过 0xC2/0xC7 命令能力探测 MyScan 与 MyScan5/6 系列。
            std::int32_t ProbeProductionSeries(MeyerDeviceDetectionRecord& record);
            // 读取 0xCD/0xCE 型号代码，并按旧固件/未初始化规则填充兼容值。
            std::int32_t ReadModelCodeForDetection(MeyerDeviceCmdDeviceInfo& info,
                                                   MeyerDeviceDetectionRecord& record);
            // 执行带 0xFF/0x00 状态回复的固化命令。
            std::int32_t ExecuteStatusCommand(std::uint8_t commandCode,
                                               const unsigned char* payload,
                                               std::size_t payloadSize,
                                               std::uint8_t expectedResponseCode,
                                               std::uint32_t timeoutMs,
                                               const char* operation);
            std::int32_t ReadFixedPayload(std::uint8_t commandCode,
                                           std::uint8_t expectedResponseCode,
                                           std::size_t expectedPayloadSize,
                                           std::vector<std::uint8_t>& payload,
                                           const char* operation,
                                           std::uint32_t timeoutMs = 0U,
                                           CommandExchangeDiagnostics* diagnostics = nullptr);
            // 校验图像尺寸、分包几何和内存预算，阻止非法参数进入 Transport。
            std::int32_t ValidateCaptureParams(const MeyerDeviceCmdCaptureParams& params);
            // 按型号配置发送停止传图和可选关灯命令。
            std::int32_t SendStopAndOptionalLightOff(bool turnLightOff);
            // 保存错误码对应的文本，并写入错误日志。
            std::int32_t SetError(std::int32_t result, const std::string& message);
            // 推进状态快照序号，使读取方能识别状态更新。
            void AdvanceState();
            // 切换型号时清空动态字段并写入型号静态信息。
            void ResetStateForModel(const DeviceModelProfile& profile);
            // 型号从设备信息的明确标记识别后，切换能力目录并保留已读取动态状态。
            bool ApplyDetectedModel(std::int32_t model, std::int32_t source);

            std::unique_ptr<IRawDeviceTransport> m_transport;
            const DeviceModelProfile* m_profile;
            MeyerDeviceCmdOpenParams m_lastOpenParams;
            MeyerDeviceStateSnapshot m_state;
            std::string m_lastError;
            // 为 true 表示上一条请求尚未收到普通合法帧或业务可识别终态回包。
            // 下一条真实设备命令发送前需等待 20 ms，为上一条请求留足响应窗口；
            // 若上一条请求已得到可识别响应，该值会立即清零，下一条命令无需等待 20 ms。
            bool m_waitBeforeNextCommand;
        };
    }
}
