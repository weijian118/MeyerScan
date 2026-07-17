#include "../../stdafx.h"

// Cypress CyAPI 传输实现。所有公开成员先取得 m_mutex，保护设备和异步槽位。
#include "CyApiTransport.h"

#include "CyAPI.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <limits>
#include <windows.h>

namespace
{
    const std::size_t kDefaultCommandBufferSize = 512;
    const std::size_t kDefaultStreamQueueDepth = 8;
    const std::size_t kMaximumStreamQueueDepth = 64;
    const std::size_t kMaximumTransferSize = 16U * 1024U * 1024U;
}

namespace meyer
{
    namespace device
    {
        // 初始化为空连接；CyAPI 对象在第一次 Open 时按需创建。
        CyApiTransport::CyApiTransport()
            : m_device(nullptr)
            , m_bulkInEndpoint(nullptr)
            , m_bulkOutEndpoint(nullptr)
            , m_isOpen(false)
            , m_isUsb2(false)
            , m_streaming(false)
            , m_deviceCount(0)
            , m_streamReadIndex(0)
            , m_streamTransferSize(0)
            , m_streamQueueDepth(0)
        {
        }

        // 先关闭连接和异步请求，再释放 CyAPI 设备对象。
        CyApiTransport::~CyApiTransport()
        {
            Close();

            if (m_device != nullptr)
            {
                delete m_device;
                m_device = nullptr;
            }
        }

        // 返回工厂注册的传输类型。
        TransportType CyApiTransport::GetType() const
        {
            return TransportType::CyApiUsb;
        }

        // 枚举指定索引和 VID/PID 设备，并绑定一入一出两个 Bulk 端点。
        bool CyApiTransport::Open(const TransportConfig& config)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Open 可重复调用；先关闭旧设备，避免缓存端点继续指向旧连接。
            if (m_device != nullptr)
            {
                m_device->Close();
            }
            m_config = config;
            ReleaseStreamResources();
            ResetState();

            if (!EnsureDeviceInstance())
            {
                return false;
            }

            if (!OpenMatchingDevice(config))
            {
                ResetState();
                return false;
            }

            if (!BindBulkEndpoints())
            {
                if (m_device != nullptr)
                {
                    m_device->Close();
                }
                ReleaseStreamResources();
                ResetState();
                return false;
            }

            m_isOpen = true;
            return true;
        }

        // 中止异步传输、关闭设备并清空所有缓存状态。
        void CyApiTransport::Close()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_streaming = false;
            ReleaseStreamResources();

            if (m_device != nullptr)
            {
                m_device->Close();
            }

            ResetState();
        }

        // 同时检查缓存标志和 CyAPI 实际设备句柄。
        bool CyApiTransport::IsOpen() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // 同时检查缓存状态和 CyAPI 真实句柄，防止设备拔出后仍报告已打开。
            return m_isOpen && m_device != nullptr && m_device->IsOpen();
        }

        // 使用 CyAPI ReConnect 恢复句柄，并重新扫描端点指针。
        bool CyApiTransport::Reconnect()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_device == nullptr)
            {
                return false;
            }

            if (!m_device->ReConnect())
            {
                return false;
            }

            if (!BindBulkEndpoints())
            {
                ReleaseStreamResources();
                ResetState();
                return false;
            }

            m_isOpen = true;
            return true;
        }

        // 通过 Bulk OUT 异步发送一条命令；无论成功或超时都完成上下文回收。
        bool CyApiTransport::SendCommand(const Byte* data, std::size_t size, std::uint32_t timeoutMs)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_isOpen || m_device == nullptr || m_bulkOutEndpoint == nullptr || data == nullptr || size == 0)
            {
                return false;
            }

            if (size > static_cast<std::size_t>(LONG_MAX))
            {
                return false;
            }

            // CyAPI 接口要求可写 UCHAR*，因此复制调用方只读缓冲区。
            std::vector<UCHAR> transferBuffer(data, data + size);
            LONG transferLength = static_cast<LONG>(size);

            OVERLAPPED overlapped;
            ::ZeroMemory(&overlapped, sizeof(OVERLAPPED));
            overlapped.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
            if (overlapped.hEvent == NULL)
            {
                return false;
            }

            bool success = false;
            m_bulkOutEndpoint->SetXferSize(static_cast<ULONG>(transferLength));
            UCHAR* context = m_bulkOutEndpoint->BeginDataXfer(&transferBuffer[0], transferLength, &overlapped);
            if (context == nullptr)
            {
                ::CloseHandle(overlapped.hEvent);
                return false;
            }

            const bool completedBeforeTimeout = m_bulkOutEndpoint->WaitForXfer(&overlapped, timeoutMs);
            if (!completedBeforeTimeout)
            {
                m_bulkOutEndpoint->Abort();
                if (m_bulkOutEndpoint->LastError == ERROR_IO_PENDING)
                {
                    ::WaitForSingleObject(overlapped.hEvent, timeoutMs);
                }
            }

            // 即使超时并 Abort，也必须 FinishDataXfer 一次，让 CyAPI 回收异步上下文。
            const bool finishSucceeded =
                m_bulkOutEndpoint->FinishDataXfer(&transferBuffer[0], transferLength, &overlapped, context);
            success = completedBeforeTimeout && finishSucceeded;

            ::CloseHandle(overlapped.hEvent);
            return success;
        }

        // 命令响应和流都来自 Bulk IN，此函数复用单次异步接收实现。
        bool CyApiTransport::ReceiveCommand(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return ReceiveFromBulkIn(buffer, capacity, receivedSize, timeoutMs);
        }

        // 标记流已启动；异步队列由 PrimeStream 单独创建。
        bool CyApiTransport::StartStream()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_isOpen || m_bulkInEndpoint == nullptr)
            {
                return false;
            }

            ReleaseStreamResources();
            m_streaming = true;
            return true;
        }

        // 停止流并释放全部事件、缓冲区和 CyAPI 上下文。
        void CyApiTransport::StopStream()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_streaming = false;
            ReleaseStreamResources();
        }

        // 等待当前环形槽位，完成后复制数据并立即把该槽位重新提交到队尾。
        bool CyApiTransport::ReceiveStreamPacket(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_streaming)
            {
                receivedSize = 0;
                return false;
            }

            if (buffer == nullptr || capacity == 0U || m_bulkInEndpoint == nullptr)
            {
                receivedSize = 0;
                return false;
            }

            if (!EnsureStreamResources(capacity))
            {
                receivedSize = 0;
                return false;
            }

            const std::size_t slotIndex = m_streamReadIndex;
            if (slotIndex >= m_streamContexts.size() || m_streamContexts[slotIndex] == nullptr)
            {
                receivedSize = 0;
                ReleaseStreamResources();
                return false;
            }
            LONG transferLength = static_cast<LONG>(m_streamTransferSize);

            bool waitOk = m_bulkInEndpoint->WaitForXfer(&m_streamOverlapped[slotIndex], timeoutMs);
            if (!waitOk)
            {
                m_bulkInEndpoint->Abort();
                if (m_bulkInEndpoint->LastError == ERROR_IO_PENDING)
                {
                    ::WaitForSingleObject(m_streamOverlapped[slotIndex].hEvent, timeoutMs);
                }
            }

            // FinishDataXfer 会把 transferLength 改为实际字节数。
            const bool success = m_bulkInEndpoint->FinishDataXfer(
                m_streamBuffers[slotIndex],
                transferLength,
                &m_streamOverlapped[slotIndex],
                m_streamContexts[slotIndex]);

            m_streamContexts[slotIndex] = nullptr;
            if (!success || transferLength <= 0)
            {
                ReleaseStreamResources();
                receivedSize = 0;
                return false;
            }

            receivedSize = static_cast<std::size_t>(transferLength);
            if (receivedSize > capacity)
            {
                receivedSize = capacity;
            }
            memcpy(buffer, m_streamBuffers[slotIndex], receivedSize);

            if (!QueueStreamTransfer(slotIndex))
            {
                ReleaseStreamResources();
                receivedSize = 0;
                return false;
            }

            m_streamReadIndex = (slotIndex + 1) % m_streamBuffers.size();
            return true;
        }

        // 返回 OpenMatchingDevice 根据 USB 描述符判断的速度类型。
        bool CyApiTransport::IsUsb2() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_isUsb2;
        }

        // 返回最近一次 Open 枚举到的 CyAPI 设备总数。
        int CyApiTransport::GetDeviceCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_deviceCount;
        }

        // 校验资源上限后预提交固定数量的异步 Bulk IN 请求。
        bool CyApiTransport::PrimeStream(std::size_t transferSize, std::size_t queueDepth)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_streaming)
            {
                return false;
            }

            if (transferSize == 0 || transferSize > kMaximumTransferSize ||
                transferSize > static_cast<std::size_t>(LONG_MAX) ||
                queueDepth > kMaximumStreamQueueDepth)
            {
                return false;
            }

            m_streamQueueDepth = (queueDepth == 0) ? kDefaultStreamQueueDepth : queueDepth;
            if (!m_streamBuffers.empty())
            {
                ReleaseStreamResources();
            }

            return InitializeStreamResources(transferSize);
        }

        // 延迟创建 CCyUSBDevice，避免仅做 ABI smoke 时访问硬件 SDK。
        bool CyApiTransport::EnsureDeviceInstance()
        {
            if (m_device == nullptr)
            {
                m_device = new CCyUSBDevice(NULL);
            }

            return m_device != nullptr;
        }

        // 自动遍历或打开指定索引，并核对 VID/PID 和 USB 速度描述符。
        bool CyApiTransport::OpenMatchingDevice(const TransportConfig& config)
        {
            if (m_device == nullptr)
            {
                return false;
            }

            const int deviceCount = m_device->DeviceCount();
            m_deviceCount = deviceCount;
            if (deviceCount <= 0)
            {
                return false;
            }

            // UINT32_MAX 是公共 ABI 约定的“自动选择”。不能先转成 long 再判断，
            // 因为 Windows x64 的 long 仍是 32 位，UINT32_MAX 会变成 -1。
            const bool autoSelect =
                config.deviceIndex == std::numeric_limits<std::uint32_t>::max();
            if (!autoSelect && config.deviceIndex >= static_cast<std::uint32_t>(deviceCount))
            {
                return false;
            }
            for (int index = 0; index < deviceCount; ++index)
            {
                if (!autoSelect &&
                    static_cast<std::uint32_t>(index) != config.deviceIndex)
                {
                    continue;
                }

                // CyAPI 的 Open 参数是 UCHAR，枚举索引在 DeviceCount 范围内后再显式转换。
                if (!m_device->Open(static_cast<UCHAR>(index)))
                {
                    continue;
                }

                if (!m_device->IsOpen())
                {
                    continue;
                }

                const int vendorId = m_device->VendorID;
                const int productId = m_device->ProductID;
                if (vendorId != config.vendorId || productId != config.productId)
                {
                    m_device->Close();
                    continue;
                }

                const int highSpeed = m_device->bHighSpeed;
                const int bcdUsb = m_device->BcdUSB;
                const int superSpeed = m_device->bSuperSpeed;

                // 旧软件把 bHighSpeed + USB 2.x BCD 判断为 USB2。这里使用
                // [0x0200, 0x0300) 范围，同时兼容截图中的 0x0200/0x0210。
                if ((highSpeed == 1) && (bcdUsb >= 512) && (bcdUsb < 768))
                {
                    m_isUsb2 = true;
                    return true;
                }

                // bSuperSpeed + USB 3.x BCD 才能判为 USB3；未知速度必须关闭
                // 当前句柄继续枚举，不能把默认 false 当成 USB3。
                if ((superSpeed == 1) && (bcdUsb >= 768))
                {
                    m_isUsb2 = false;
                    return true;
                }

                m_device->Close();
            }

            return false;
        }

        // 遍历当前接口端点，分别保存 Bulk IN 和 Bulk OUT 指针。
        bool CyApiTransport::BindBulkEndpoints()
        {
            if (m_device == nullptr || !m_device->IsOpen())
            {
                return false;
            }

            m_bulkInEndpoint = nullptr;
            m_bulkOutEndpoint = nullptr;

            const int endpointCount = m_device->EndPointCount();
            for (int index = 1; index < endpointCount; ++index)
            {
                CCyUSBEndPoint* endpoint = m_device->EndPoints[index];
                if (endpoint == nullptr)
                {
                    continue;
                }

                const bool isIn = ((endpoint->Address & 0x80) == 0x80);
                const bool isBulk = (endpoint->Attributes == 2);
                if (!isBulk)
                {
                    continue;
                }

                if (isIn)
                {
                    m_bulkInEndpoint = endpoint;
                }
                else
                {
                    m_bulkOutEndpoint = endpoint;
                }
            }

            return m_bulkInEndpoint != nullptr && m_bulkOutEndpoint != nullptr;
        }

        // 使用一个临时事件完成单次 Bulk IN 接收，适用于低频命令响应。
        bool CyApiTransport::ReceiveFromBulkIn(Byte* buffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs)
        {
            receivedSize = 0;

            if (!m_isOpen || m_device == nullptr || m_bulkInEndpoint == nullptr || buffer == nullptr || capacity == 0)
            {
                return false;
            }

            if (capacity > static_cast<std::size_t>(LONG_MAX))
            {
                return false;
            }

            LONG transferLength = static_cast<LONG>(capacity);
            std::vector<UCHAR> transferBuffer(capacity, static_cast<UCHAR>(0));

            OVERLAPPED overlapped;
            ::ZeroMemory(&overlapped, sizeof(OVERLAPPED));
            overlapped.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
            if (overlapped.hEvent == NULL)
            {
                return false;
            }

            bool success = false;
            m_bulkInEndpoint->SetXferSize(static_cast<ULONG>(transferLength));
            UCHAR* context = m_bulkInEndpoint->BeginDataXfer(&transferBuffer[0], transferLength, &overlapped);
            if (context == nullptr)
            {
                ::CloseHandle(overlapped.hEvent);
                return false;
            }

            const bool completedBeforeTimeout = m_bulkInEndpoint->WaitForXfer(&overlapped, timeoutMs);
            if (!completedBeforeTimeout)
            {
                m_bulkInEndpoint->Abort();
                if (m_bulkInEndpoint->LastError == ERROR_IO_PENDING)
                {
                    ::WaitForSingleObject(overlapped.hEvent, timeoutMs);
                }
            }

            const bool finishSucceeded =
                m_bulkInEndpoint->FinishDataXfer(&transferBuffer[0], transferLength, &overlapped, context);
            success = completedBeforeTimeout && finishSucceeded;
            if (success && transferLength > 0 &&
                static_cast<std::size_t>(transferLength) <= capacity)
            {
                receivedSize = static_cast<std::size_t>(transferLength);
                ::memcpy(buffer, &transferBuffer[0], receivedSize);
            }

            ::CloseHandle(overlapped.hEvent);
            return success;
        }

        // 首次接收或包长度变化时创建/重建异步流资源。
        bool CyApiTransport::EnsureStreamResources(std::size_t packetCapacity)
        {
            if (packetCapacity == 0 || packetCapacity > kMaximumTransferSize ||
                packetCapacity > static_cast<std::size_t>(LONG_MAX))
            {
                return false;
            }

            if (m_streamBuffers.empty())
            {
                if (m_streamQueueDepth == 0)
                {
                    m_streamQueueDepth = kDefaultStreamQueueDepth;
                }
                return InitializeStreamResources(packetCapacity);
            }

            if (m_streamTransferSize != packetCapacity)
            {
                ReleaseStreamResources();
                if (m_streamQueueDepth == 0)
                {
                    m_streamQueueDepth = kDefaultStreamQueueDepth;
                }
                return InitializeStreamResources(packetCapacity);
            }

            return true;
        }

        // 为每个队列槽位分配缓冲区、手动复位事件并提交第一次读取。
        bool CyApiTransport::InitializeStreamResources(std::size_t packetCapacity)
        {
            if (!m_isOpen || m_bulkInEndpoint == nullptr)
            {
                return false;
            }

            ReleaseStreamResources();

            m_streamTransferSize = packetCapacity;
            m_streamReadIndex = 0;
            if (m_streamQueueDepth == 0)
            {
                m_streamQueueDepth = kDefaultStreamQueueDepth;
            }
            if (m_streamQueueDepth > kMaximumStreamQueueDepth)
            {
                return false;
            }

            m_streamBuffers.assign(m_streamQueueDepth, nullptr);
            m_streamContexts.assign(m_streamQueueDepth, nullptr);
            m_streamOverlapped.assign(m_streamQueueDepth, OVERLAPPED());

            m_bulkInEndpoint->SetXferSize(static_cast<ULONG>(m_streamTransferSize));

            for (std::size_t slotIndex = 0; slotIndex < m_streamQueueDepth; ++slotIndex)
            {
                m_streamBuffers[slotIndex] = new UCHAR[m_streamTransferSize];
                ZeroMemory(m_streamBuffers[slotIndex], static_cast<SIZE_T>(m_streamTransferSize));

                ZeroMemory(&m_streamOverlapped[slotIndex], sizeof(OVERLAPPED));
                m_streamOverlapped[slotIndex].hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
                if (m_streamOverlapped[slotIndex].hEvent == NULL)
                {
                    ReleaseStreamResources();
                    return false;
                }

                if (!QueueStreamTransfer(slotIndex))
                {
                    ReleaseStreamResources();
                    return false;
                }
            }

            return true;
        }

        // 将一个已经消费的槽位重新提交给 CyAPI。
        bool CyApiTransport::QueueStreamTransfer(std::size_t slotIndex)
        {
            if (slotIndex >= m_streamBuffers.size() || m_streamBuffers[slotIndex] == nullptr)
            {
                return false;
            }

            ::ResetEvent(m_streamOverlapped[slotIndex].hEvent);
            LONG transferLength = static_cast<LONG>(m_streamTransferSize);
            m_streamContexts[slotIndex] = m_bulkInEndpoint->BeginDataXfer(
                m_streamBuffers[slotIndex],
                transferLength,
                &m_streamOverlapped[slotIndex]);

            return m_streamContexts[slotIndex] != nullptr &&
                   !(m_bulkInEndpoint->NtStatus || m_bulkInEndpoint->UsbdStatus);
        }

        // 先统一 Abort，再逐槽 Finish、关事件和释放数组，保证没有悬空异步操作。
        void CyApiTransport::ReleaseStreamResources()
        {
            if (!m_streamBuffers.empty())
            {
                if (m_bulkInEndpoint != nullptr)
                {
                    m_bulkInEndpoint->Abort();
                }

                const std::size_t slotCount = m_streamBuffers.size();
                for (std::size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                {
                    if (m_bulkInEndpoint != nullptr &&
                        m_streamContexts.size() > slotIndex &&
                        m_streamContexts[slotIndex] != nullptr)
                    {
                        // FinishDataXfer 可能修改长度，因此每个槽位都从原始传输长度开始。
                        LONG transferLength = static_cast<LONG>(m_streamTransferSize);
                        bool waitOk = m_bulkInEndpoint->WaitForXfer(&m_streamOverlapped[slotIndex], 500);
                        if (!waitOk && m_bulkInEndpoint->LastError == ERROR_IO_PENDING)
                        {
                            ::WaitForSingleObject(m_streamOverlapped[slotIndex].hEvent, 2000);
                        }

                        m_bulkInEndpoint->FinishDataXfer(
                            m_streamBuffers[slotIndex],
                            transferLength,
                            &m_streamOverlapped[slotIndex],
                            m_streamContexts[slotIndex]);
                        m_streamContexts[slotIndex] = nullptr;
                    }

                    if (m_streamOverlapped.size() > slotIndex && m_streamOverlapped[slotIndex].hEvent != NULL)
                    {
                        ::CloseHandle(m_streamOverlapped[slotIndex].hEvent);
                        m_streamOverlapped[slotIndex].hEvent = NULL;
                    }

                    delete[] m_streamBuffers[slotIndex];
                    m_streamBuffers[slotIndex] = nullptr;
                }
            }

            m_streamBuffers.clear();
            m_streamContexts.clear();
            m_streamOverlapped.clear();
            m_streamReadIndex = 0;
            m_streamTransferSize = 0;
        }

        // 清除连接和流缓存字段；m_device 对象本身保留供下次 Open 复用。
        void CyApiTransport::ResetState()
        {
            m_bulkInEndpoint = nullptr;
            m_bulkOutEndpoint = nullptr;
            m_isOpen = false;
            m_isUsb2 = false;
            m_streaming = false;
            m_deviceCount = 0;
            m_streamReadIndex = 0;
            m_streamTransferSize = 0;
            m_streamQueueDepth = 0;
        }
    }
}

