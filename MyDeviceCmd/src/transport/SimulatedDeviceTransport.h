// =============================================================================
// 文件: SimulatedDeviceTransport.h
// 作用: 声明无硬件 smoke 专用模拟后端，不连接 CyAPI 或真实设备。
// =============================================================================
#pragma once

#include "IRawDeviceTransport.h"

namespace meyer
{
    namespace devicecmd
    {
        class SimulatedDeviceTransport : public IRawDeviceTransport
        {
        public:
            // 初始化默认设备编号、协议响应和确定性采集帧。
            SimulatedDeviceTransport();
            // 释放模拟帧和待响应数据。
            ~SimulatedDeviceTransport() override;

            // 打开模拟会话，不连接真实 USB。
            std::int32_t Open(const MeyerDeviceCmdOpenParams& params) override;
            // 清理模拟连接、响应队列和采集帧。
            void Close() override;
            // 返回模拟会话是否打开。
            bool IsOpen() const override;
            // 恢复模拟会话状态。
            std::int32_t Reconnect() override;
            // 解析 DeviceCmd 编码的 A 类请求并更新模拟设备状态。
            std::int32_t SendCommand(const std::vector<std::uint8_t>& frame,
                                     std::uint32_t timeoutMs) override;
            // 取出最近一次查询命令生成的 A 类响应。
            std::int32_t ReceiveCommand(std::vector<std::uint8_t>& frame,
                                        std::size_t capacity,
                                        std::uint32_t timeoutMs) override;
            // 模拟始终返回一个已枚举设备。
            std::int32_t GetDeviceCount(std::int32_t& deviceCount) override;
            // 返回确定性的 USB 速度标志。
            std::int32_t GetIsUsb2(std::int32_t& isUsb2) override;
            // 创建确定性 B 类帧并标记采集已启动。
            std::int32_t StartCapture(const MeyerDeviceCmdCaptureParams& params,
                                      std::int32_t transportDecoderType) override;
            // 清理模拟采集帧并标记采集停止。
            std::int32_t StopCapture() override;
            // 返回模拟采集状态。
            bool IsCaptureActive() const override;
            // 将一帧确定性数据复制到调用方缓冲区。
            std::int32_t GetFrame(unsigned char* buffer,
                                  std::size_t capacity,
                                  std::size_t& frameBytes,
                                  MeyerDeviceCmdFrameInfo& frameInfo) override;
            // 返回模拟后端最近错误文本。
            const std::string& LastError() const override;

        private:
            // 根据收到的请求命令生成协议正确的模拟响应。
            std::int32_t QueueResponse(std::uint8_t requestCode,
                                       const std::vector<std::uint8_t>* requestPayload = nullptr);
            // 把 13 位设备编号转换为协议中的逐位数值字节。
            void BuildMachineCodePayload(std::vector<std::uint8_t>& payload) const;
            // 根据当前模拟设备状态生成固定长度的协议响应数据。
            void BuildDefaultPayloads();
            // 读取协议 payload 中的大端 16 位字段。
            static std::uint16_t ReadBigEndian16(const std::vector<std::uint8_t>& payload,
                                                  std::size_t offset);

            bool m_open;
            bool m_captureActive;
            bool m_frameReady;
            bool m_lightOn;
            bool m_isUsb2;
            bool m_omitModelMarker;
            std::int32_t m_model;
            std::string m_deviceId;
            std::string m_lastError;
            std::vector<std::uint8_t> m_pendingResponse;
            std::vector<std::uint8_t> m_frame;
            std::vector<std::uint8_t> m_cameraParameters;
            std::vector<std::uint8_t> m_colorMatrix;
            std::vector<std::uint8_t> m_temperature;
            std::vector<std::uint8_t> m_camera1Calibration;
            std::vector<std::uint8_t> m_camera2Calibration;
            std::vector<std::uint8_t> m_colorCalibration;
            std::vector<std::uint8_t> m_deviceInfo;
            std::vector<std::uint8_t> m_exposureParameters;
            std::uint8_t m_frameRate;
            MeyerDeviceCmdFrameInfo m_frameInfo;
        };
    }
}
