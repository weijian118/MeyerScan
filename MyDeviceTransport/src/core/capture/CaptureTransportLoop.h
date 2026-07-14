#pragma once

#include <cstddef>

#include "../../model/CaptureFrameConfig.h"
#include "../../model/CapturePacket.h"

namespace meyer
{
    namespace device
    {
        class DeviceSession;

        namespace capture
        {
            class CaptureTransportLoop
            {
            public:
                // 初始化空会话和零统计值。
                CaptureTransportLoop();

                // 保存采集配置快照并执行基础合法性检查。
                void Configure(const CaptureFrameConfig& config);
                // 返回配置快照。
                const CaptureFrameConfig& GetConfig() const;
                // 查询是否已收到合法配置。
                bool IsConfigured() const;

                // 与旧代码的对应关系：Prime/ReceiveNextPacket/RequeueCurrentTransfer
                // 分别承接异步队列分配、包接收和重新提交语义。
                // 绑定设备会话并重置统计，暂不提交 USB 请求。
                bool Start(DeviceSession& session);
                // 停止底层数据流并解除运行状态。
                void Stop();
                // 启动流并向 CyAPI 预提交异步请求队列。
                bool Prime();
                // 接收一包并解析图像头索引及部分包状态。
                bool ReceiveNextPacket(CapturePacket& packet, bool& isPartialPacket);
                // CyAPI 自动重提槽位，本函数保留统一状态查询语义。
                bool RequeueCurrentTransfer();

                // 查询流队列是否已 Prime。
                bool IsRunning() const;
                // 返回成功接收包数。
                std::size_t GetSuccessCount() const;
                // 返回接收失败次数。
                std::size_t GetFailureCount() const;
                // 返回长度与预期不一致的成功包数。
                std::size_t GetPartialSuccessCount() const;

            private:
                // 返回配置中的协议包长度，缺省时回退为 USB 传输长度。
                std::size_t GetExpectedPacketSize() const;

            private:
                CaptureFrameConfig m_config;
                DeviceSession* m_session;
                bool m_configured;
                bool m_running;
                std::size_t m_packetSequence;
                std::size_t m_successCount;
                std::size_t m_failureCount;
                std::size_t m_partialSuccessCount;
            };
        }
    }
}
