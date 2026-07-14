#pragma once
//基于CyApi的传输实现
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
#include <windows.h>

#include "../ITransport.h"

class CCyUSBDevice;
class CCyUSBEndPoint;

namespace meyer
{
    namespace device
    {
        class CyApiTransport : public ITransport
        {
        public:
            // 初始化空设备、空端点和空异步队列。
            CyApiTransport();
            // 析构前停止传输并释放 CyAPI 设备对象。
            virtual ~CyApiTransport();

            // 返回 CyApiUsb 类型。
            virtual TransportType GetType() const override;

            // 枚举 VID/PID 和索引匹配的设备并绑定 Bulk 端点。
            virtual bool Open(const TransportConfig& config) override;
            // 中止传输、释放异步槽位并关闭设备。
            virtual void Close() override;
            // 同时检查缓存标志和 CyAPI 真实句柄。
            virtual bool IsOpen() const override;
            // 调用 CyAPI 重连并重新绑定端点。
            virtual bool Reconnect() override;

            // 通过 Bulk OUT 异步发送原始命令。
            virtual bool SendCommand(const Byte* data, std::size_t size, std::uint32_t timeoutMs) override;
            // 通过 Bulk IN 接收命令响应。
            virtual bool ReceiveCommand(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs) override;

            // 标记进入流模式；实际异步请求由 PrimeStream 提交。
            virtual bool StartStream() override;
            // 停止流并释放所有异步槽位。
            virtual void StopStream() override;
            // 等待当前槽位完成，复制结果后重新提交该槽位。
            virtual bool ReceiveStreamPacket(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs) override;

            // 查询当前设备是否以 USB 2.x 速率连接。
            bool IsUsb2() const;
            // 返回最近一次枚举到的设备数量。
            int GetDeviceCount() const;
            // 创建固定传输大小和队列深度的异步请求环。
            bool PrimeStream(std::size_t transferSize, std::size_t queueDepth);

        private:
            // 按需创建 CCyUSBDevice，避免构造阶段访问驱动。
            bool EnsureDeviceInstance();
            // 枚举并打开配置指定的设备索引。
            bool OpenMatchingDevice(const TransportConfig& config);
            // 从当前设备端点表中找到 Bulk IN 和 Bulk OUT。
            bool BindBulkEndpoints();
            // 执行一次普通 Bulk IN 异步接收。
            bool ReceiveFromBulkIn(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs);
            // 检查现有流资源是否与请求容量兼容。
            bool EnsureStreamResources(std::size_t packetCapacity);
            // 为每个槽位分配缓冲区、OVERLAPPED 事件和上下文。
            bool InitializeStreamResources(std::size_t packetCapacity);
            // 向 CyAPI 提交指定槽位的下一次异步读。
            bool QueueStreamTransfer(std::size_t slotIndex);
            // 中止端点并成对释放上下文、事件和缓冲区。
            void ReleaseStreamResources();
            // 清空连接标志和缓存端点，不释放设备对象本身。
            void ResetState();

        private:
            mutable std::mutex m_mutex;
            TransportConfig m_config;

            CCyUSBDevice* m_device;
            CCyUSBEndPoint* m_bulkInEndpoint;
            CCyUSBEndPoint* m_bulkOutEndpoint;

            bool m_isOpen;
            bool m_isUsb2;
            bool m_streaming;
            int m_deviceCount;
            std::size_t m_streamReadIndex;
            std::size_t m_streamTransferSize;
            std::size_t m_streamQueueDepth;
            std::vector<UCHAR*> m_streamBuffers;
            std::vector<UCHAR*> m_streamContexts;
            std::vector<OVERLAPPED> m_streamOverlapped;
        };
    }
}

