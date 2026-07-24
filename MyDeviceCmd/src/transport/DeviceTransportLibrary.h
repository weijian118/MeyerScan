// =============================================================================
// 文件: DeviceTransportLibrary.h
// 作用: 声明 MeyerScan_DeviceTransport.dll 的运行时动态绑定适配器。
// =============================================================================
#pragma once

#include "IRawDeviceTransport.h"

#include <memory>

namespace meyer
{
    namespace devicecmd
    {
        class DeviceTransportLibrary : public IRawDeviceTransport
        {
        public:
            // 构造空动态绑定对象，不立即加载 DLL。
            DeviceTransportLibrary();
            // 先销毁 Transport 会话，再卸载 DLL。
            ~DeviceTransportLibrary() override;

            // 按绝对路径加载 DeviceTransport 并打开底层设备。
            std::int32_t Open(const MeyerDeviceCmdOpenParams& params) override;
            // 关闭底层设备但保留动态库对象，便于后续重连。
            void Close() override;
            // 查询底层会话是否打开。
            bool IsOpen() const override;
            // 调用 DeviceTransport 的重连接口。
            std::int32_t Reconnect() override;
            // 转发已经编码完成的 A 类命令帧。
            std::int32_t SendCommand(const std::vector<std::uint8_t>& frame,
                                     std::uint32_t timeoutMs) override;
            // 接收一条 A 类响应帧并裁剪到实际长度。
            std::int32_t ReceiveCommand(std::vector<std::uint8_t>& frame,
                                        std::size_t capacity,
                                        std::uint32_t timeoutMs) override;
            // 获取底层枚举到的设备数量。
            std::int32_t GetDeviceCount(std::int32_t& deviceCount) override;
            // 获取底层 USB2/USB3 状态。
            std::int32_t GetIsUsb2(std::int32_t& isUsb2) override;
            // 配置传输解析模式并启动 B 类采集流。
            std::int32_t StartCapture(const MeyerDeviceCmdCaptureParams& params,
                                      std::int32_t transportDecoderType) override;
            // 停止底层采集流。
            std::int32_t StopCapture() override;
            // 查询底层采集线程是否仍在运行。
            bool IsCaptureActive() const override;
            // 从底层非阻塞读取完整图像帧。
            std::int32_t GetFrame(unsigned char* buffer,
                                  std::size_t capacity,
                                  std::size_t& frameBytes,
                                  MeyerDeviceCmdFrameInfo& frameInfo) override;
            // 启动不带组帧的原始 B 包流。
            std::int32_t StartRawCapture(const MeyerDeviceCmdCaptureParams& params) override;
            // 停止原始 B 包流。
            std::int32_t StopRawCapture() override;
            // 查询本适配器是否已启动原始流。
            bool IsRawCaptureActive() const override;
            // 转发一次原始包阻塞接收。
            std::int32_t ReceiveRawCapturePacket(unsigned char* buffer,
                                                 std::size_t capacity,
                                                 std::size_t& receivedSize,
                                                 std::uint32_t timeoutMs) override;
            // 复制 Transport 原始流诊断快照。
            std::int32_t GetStreamDiagnostics(
                MeyerDeviceCmdStreamDiagnostics& diagnostics) override;
            // 返回动态加载和底层设备操作的最近错误文本。
            const std::string& LastError() const override;

        private:
            struct Functions;

            // 加载绝对路径 DLL、校验 API 版本并解析全部必需函数。
            std::int32_t Load(const char* libraryPathUtf8);
            // 将 DeviceTransport 结果映射为 DeviceCmd 结果，并读取底层错误。
            std::int32_t MapResult(std::int32_t transportResult, const char* operation);
            // 在卸载 DLL 前销毁底层不透明句柄。
            void Unload();

            void* m_module;
            void* m_handle;
            std::unique_ptr<Functions> m_functions;
            mutable std::string m_lastError;
            bool m_rawCaptureActive;
        };
    }
}
