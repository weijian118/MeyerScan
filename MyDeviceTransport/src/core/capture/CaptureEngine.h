#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../../model/CaptureFrameConfig.h"
#include "../../model/ImageFrame.h"
#include "../../processing/imu/IImuProcessor.h"
#include "../../processing/imu/ImuAttitudeProcessor.h"
#include "CaptureTransportLoop.h"
#include "FramePostProcessor.h"
#include "FrameSyncAssembler.h"

namespace meyer
{
    namespace device
    {
        class DeviceSession;

        namespace capture
        {
            class CaptureEngine
            {
            public:
                // 创建尚未配置、尚未运行的采集引擎。
                CaptureEngine();
                // 析构时停止线程，保证成员销毁后没有后台访问。
                ~CaptureEngine();

                // 把已校验配置复制到传输、组帧和后处理三个子对象。
                void Configure(const CaptureFrameConfig& config);
                // 返回当前配置快照。
                const CaptureFrameConfig& GetConfig() const;
                // 查询配置是否通过内部防御性校验。
                bool IsConfigured() const;

                // 与旧代码的对应关系：Start/RunLoop/PumpOnce/GetFrame 分别替代
                // InitXferLoopNB/XferLoopNB/HandleCaptureLoopResult/GetFrameByQueue。
                // 绑定已打开会话，初始化异步队列并创建工作线程。
                bool Start(DeviceSession& session);
                // 通知线程退出、等待 join 并清空帧队列。
                void Stop();
                // 查询工作线程是否仍在接收数据。
                bool IsRunning() const;
                // 原子更新 IMU 暂停状态，避免与采集线程发生数据竞争。
                void SetImuPaused(bool paused);
                // 原子记录一次性 IMU 参考重置请求。
                void RequestImuReferenceReset();
                // 同步处理一个数据包，供工作线程循环调用。
                bool PumpOnce();
                // 从有界队列弹出最旧完整帧，无帧时立即返回 false。
                bool GetFrame(ImageFrame& frame);
                // 在互斥锁保护下读取待交付帧数量。
                std::size_t GetReadyFrameCount() const;

                // 返回传输循环，供同模块诊断统计使用。
                CaptureTransportLoop& GetTransportLoop();
                // 返回组帧状态机，供同模块诊断使用。
                FrameSyncAssembler& GetAssembler();
                // 返回后处理器，供同模块诊断使用。
                FramePostProcessor& GetPostProcessor();

            private:
                // 解码当前包中的 IMU 样本并更新最近姿态。
                void UpdateImuState(const CapturePacket& packet);
                // 后台线程入口，持续调用 PumpOnce 直到停止或连续失败。
                void RunLoop();

            private:
                CaptureFrameConfig m_config;
                DeviceSession* m_session;
                bool m_configured;
                std::atomic<bool> m_running;
                int m_consecutiveReceiveFailures;
                std::thread m_workerThread;
                mutable std::mutex m_queueMutex;
                std::queue<ImageFrame> m_readyFrames;
                imu::ImuAttitudeProcessor m_imuProcessor;
                std::atomic<bool> m_imuPaused;
                std::atomic<bool> m_imuResetRequested;
                ImuSample m_latestImuSample;
                std::vector<float> m_latestGyroLegacyOutput;
                std::chrono::steady_clock::time_point m_lastImuTick;
                bool m_hasLastImuTick;
                CaptureTransportLoop m_transportLoop;
                FrameSyncAssembler m_assembler;
                FramePostProcessor m_postProcessor;
            };
        }
    }
}
