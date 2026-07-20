// =============================================================================
// 文件: DeviceCommandService.h
// 作用: 声明设备命令、状态缓存和采集编排的内部门面。
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

            // 默认状态表示尚未发出请求，便于失败分支准确判断停在哪一步。
            CommandExchangeDiagnostics()
                : requestSent(false), responseReceived(false),
                  parseStatus(protocol::CommandParseStatus::NotRun)
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

            // 为颜色校准建立空闲会话，依次检查连接/USB、机器码和设备型号。
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
            // 读取电池响应并更新状态快照。
            std::int32_t RefreshBattery();
            // 读取设备授权信息并更新状态快照。
            std::int32_t RefreshDeviceSecurityInfo();
            // 读取并严格验证 0xD4/0xD9 设备编号；校验失败可识别为生产模式。
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
                                           std::uint32_t timeoutMs = 0U);
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
        };
    }
}
