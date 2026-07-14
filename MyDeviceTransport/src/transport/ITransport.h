#pragma once
//底层传输抽象
#include <cstddef>
#include <vector>
#include <cstdint>

#include "../api/DeviceTypes.h"
#include "../model/TransportConfig.h"

namespace meyer
{
    namespace device
    {
        class ITransport
        {
        public:
            // 通过基类指针释放具体传输实现时调用正确析构函数。
            virtual ~ITransport() {}

            // 返回实现对应的传输类型。
            virtual TransportType GetType() const = 0;

            // 按配置打开指定设备。
            virtual bool Open(const TransportConfig& config) = 0;
            // 中止正在进行的传输并关闭设备。
            virtual void Close() = 0;
            // 查询设备句柄是否有效。
            virtual bool IsOpen() const = 0;
            // 尝试恢复同一个设备连接。
            virtual bool Reconnect() = 0;

            // 发送一段原始命令字节。
            virtual bool SendCommand(const Byte* data, std::size_t size, std::uint32_t timeoutMs) = 0;
            // 接收一包命令响应并返回实际字节数。
            virtual bool ReceiveCommand(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs) = 0;

            // 切换到底层流接收状态。
            virtual bool StartStream() = 0;
            // 停止底层流并释放相关资源。
            virtual void StopStream() = 0;
            // 接收一包原始流数据。
            virtual bool ReceiveStreamPacket(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs) = 0;

            // vector 便利重载只借用连续内存，不把容器跨 DLL 边界传递。
            bool SendCommand(const std::vector<Byte>& data, std::uint32_t timeoutMs)
            {
                if (data.empty())
                {
                    return false;
                }

                return SendCommand(&data[0], data.size(), timeoutMs);
            }
        };
    }
}
