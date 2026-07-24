// =============================================================================
// 文件: IRawDeviceTransport.h
// 作用: 隔离 DeviceCmd 业务逻辑与真实 DeviceTransport DLL/测试模拟后端。
// =============================================================================
#pragma once

#include "../../include/DeviceCmd.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace meyer
{
    namespace devicecmd
    {
        // 该接口只在 MeyerScan_DeviceCmd.dll 内部使用，不跨 DLL 边界。
        class IRawDeviceTransport
        {
        public:
            virtual ~IRawDeviceTransport() {}

            // 按公共打开参数建立底层设备连接。
            virtual std::int32_t Open(const MeyerDeviceCmdOpenParams& params) = 0;
            // 停止采集并关闭底层连接。
            virtual void Close() = 0;
            // 查询真实连接状态。
            virtual bool IsOpen() const = 0;
            // 恢复最近一次连接。
            virtual std::int32_t Reconnect() = 0;

            // 发送一条已经编码完成的 A 类命令帧。
            virtual std::int32_t SendCommand(const std::vector<std::uint8_t>& frame,
                                             std::uint32_t timeoutMs) = 0;
            // 接收一条 A 类响应帧；返回值只描述传输，不解释协议字段。
            virtual std::int32_t ReceiveCommand(std::vector<std::uint8_t>& frame,
                                                std::size_t capacity,
                                                std::uint32_t timeoutMs) = 0;

            // 返回枚举到的设备数量和 USB 速度类型。
            virtual std::int32_t GetDeviceCount(std::int32_t& deviceCount) = 0;
            virtual std::int32_t GetIsUsb2(std::int32_t& isUsb2) = 0;

            // 配置并启动 B 类图像流接收。
            virtual std::int32_t StartCapture(const MeyerDeviceCmdCaptureParams& params,
                                              std::int32_t transportDecoderType) = 0;
            // 停止 B 类流并清空待交付帧。
            virtual std::int32_t StopCapture() = 0;
            // 查询底层采集线程是否仍在运行。
            virtual bool IsCaptureActive() const = 0;
            // 非阻塞读取一帧完整数据。
            virtual std::int32_t GetFrame(unsigned char* buffer,
                                          std::size_t capacity,
                                          std::size_t& frameBytes,
                                          MeyerDeviceCmdFrameInfo& frameInfo) = 0;

            // 新采集链路只启动底层原始流和预提交队列，不调用 Transport 组帧。
            virtual std::int32_t StartRawCapture(const MeyerDeviceCmdCaptureParams& params) = 0;
            // 停止原始流并回收 CyAPI 异步槽位。
            virtual std::int32_t StopRawCapture() = 0;
            // 查询原始流是否已启动。
            virtual bool IsRawCaptureActive() const = 0;
            // 读取一个原始 B 包，调用方拥有缓冲区。
            virtual std::int32_t ReceiveRawCapturePacket(unsigned char* buffer,
                                                         std::size_t capacity,
                                                         std::size_t& receivedSize,
                                                         std::uint32_t timeoutMs) = 0;
            // 复制底层原始流诊断统计。
            virtual std::int32_t GetStreamDiagnostics(
                MeyerDeviceCmdStreamDiagnostics& diagnostics) = 0;

            // 返回当前后端最近错误文本，供 DeviceCmd 组合诊断信息。
            virtual const std::string& LastError() const = 0;
        };
    }
}
