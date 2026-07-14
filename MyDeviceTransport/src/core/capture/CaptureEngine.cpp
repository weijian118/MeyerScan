#include "../../stdafx.h"

#include "CaptureEngine.h"

#include "../DeviceSession.h"
#include "../../processing/imu/ImuPacketDecoder.h"

namespace meyer
{
    namespace device
    {
        namespace capture
        {
            CaptureEngine::CaptureEngine()
                : m_session(nullptr)
                , m_configured(false)
                , m_running(false)
                , m_consecutiveReceiveFailures(0)
                , m_imuPaused(false)
                , m_imuResetRequested(false)
                , m_hasLastImuTick(false)
            {
            }

            // 析构时等待后台线程退出，避免线程继续访问已经释放的成员。
            CaptureEngine::~CaptureEngine()
            {
                Stop();
            }

            void CaptureEngine::Configure(const CaptureFrameConfig& config)
            {
                m_config = config;
                m_configured = config.IsValid();
                m_transportLoop.Configure(config);
                m_assembler.Configure(config);
                m_postProcessor.Configure(config);
            }

            const CaptureFrameConfig& CaptureEngine::GetConfig() const
            {
                return m_config;
            }

            bool CaptureEngine::IsConfigured() const
            {
                return m_configured;
            }

            bool CaptureEngine::Start(DeviceSession& session)
            {
                if (!m_configured || m_running.load())
                {
                    return false;
                }

                // 上一次线程可能因接收错误自行结束；重新启动前先回收其线程句柄。
                if (m_workerThread.joinable())
                {
                    m_workerThread.join();
                }

                m_session = &session;
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    while (!m_readyFrames.empty())
                    {
                        m_readyFrames.pop();
                    }
                }
                m_assembler.Reset();
                m_consecutiveReceiveFailures = 0;
                m_latestImuSample = ImuSample();
                m_latestGyroLegacyOutput.clear();
                m_hasLastImuTick = false;
                m_imuPaused.store(m_config.gyroscopePauseFlag);
                m_imuResetRequested.store(m_config.resetImuReferenceRequested);

                if (!m_transportLoop.Start(session))
                {
                    return false;
                }

                if (!m_transportLoop.Prime())
                {
                    m_transportLoop.Stop();
                    return false;
                }

                // std::thread 作为值成员持有，消除裸 new 和重复 Start 覆盖指针的风险。
                m_running.store(true);
                m_workerThread = std::thread(&CaptureEngine::RunLoop, this);
                return true;
            }

            void CaptureEngine::Stop()
            {
                m_running.store(false);

                if (m_workerThread.joinable())
                {
                    m_workerThread.join();
                }

                m_transportLoop.Stop();
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    while (!m_readyFrames.empty())
                    {
                        m_readyFrames.pop();
                    }
                }
            }

            bool CaptureEngine::IsRunning() const
            {
                return m_running.load();
            }

            // 原子变量允许控制线程在采集运行期间更新暂停状态，而不与工作线程数据竞争。
            void CaptureEngine::SetImuPaused(bool paused)
            {
                m_imuPaused.store(paused);
            }

            // exchange(false) 会让工作线程只消费一次重置请求。
            void CaptureEngine::RequestImuReferenceReset()
            {
                m_imuResetRequested.store(true);
            }

            bool CaptureEngine::PumpOnce()
            {
                if (!m_running.load())
                {
                    return false;
                }

                CapturePacket packet;
                bool isPartialPacket = false;
                if (!m_transportLoop.ReceiveNextPacket(packet, isPartialPacket))
                {
                    ++m_consecutiveReceiveFailures;
                    if (m_consecutiveReceiveFailures > 16)
                    {
                        return false;
                    }

                    ::Sleep(1);
                    return true;
                }

                if (isPartialPacket)
                {
                    ++m_consecutiveReceiveFailures;
                    if (m_consecutiveReceiveFailures > 16)
                    {
                        return false;
                    }

                    return true;
                }

                m_consecutiveReceiveFailures = 0;

                if (packet.isImageHeader)
                {
                    UpdateImuState(packet);
                }

                ImageFrame completedFrame;
                const FrameSyncResult syncResult = m_assembler.PushPacket(packet, completedFrame);
                if (syncResult == FrameSyncResult::Error)
                {
                    return false;
                }

                if (syncResult == FrameSyncResult::SyncLost)
                {
                    return true;
                }

                if (syncResult != FrameSyncResult::FrameCompleted)
                {
                    return true;
                }

                if (!m_postProcessor.Process(completedFrame))
                {
                    return false;
                }

                completedFrame.imu = m_latestImuSample;

                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    // 只保留有限帧数，消费者变慢时丢弃最旧帧而不是无限占用内存。
                    const std::size_t maxFrames = m_config.maxReadyFrames == 0U ? 1U : m_config.maxReadyFrames;
                    while (m_readyFrames.size() >= maxFrames)
                    {
                        m_readyFrames.pop();
                    }
                    m_readyFrames.push(completedFrame);
                }
                return true;
            }

            bool CaptureEngine::GetFrame(ImageFrame& frame)
            {
                frame.Clear();

                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (m_readyFrames.empty())
                {
                    return false;
                }

                frame = m_readyFrames.front();
                m_readyFrames.pop();
                return true;
            }

            std::size_t CaptureEngine::GetReadyFrameCount() const
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                return m_readyFrames.size();
            }

            CaptureTransportLoop& CaptureEngine::GetTransportLoop()
            {
                return m_transportLoop;
            }

            FrameSyncAssembler& CaptureEngine::GetAssembler()
            {
                return m_assembler;
            }

            FramePostProcessor& CaptureEngine::GetPostProcessor()
            {
                return m_postProcessor;
            }

            void CaptureEngine::UpdateImuState(const CapturePacket& packet)
            {
                if (!m_config.ahrsEnabled)
                {
                    return;
                }

                if (m_config.deviceType == DeviceType::Skys1000 && m_config.imageCount != 9)
                {
                    return;
                }

                std::vector<double> rawSample;
                bool decoded = false;
                if (m_config.deviceType == DeviceType::Skys1000)
                {
                    decoded = imu::TryDecodeSkysPacketSample(packet.Data(), packet.Size(), rawSample);
                }
                else
                {
                    decoded = imu::TryDecodeThreePacketSample(packet.Data(), packet.Size(), rawSample);
                }

                if (!decoded)
                {
                    return;
                }

                const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                double deltaSeconds = 0.0;
                if (m_hasLastImuTick)
                {
                    deltaSeconds =
                        std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastImuTick).count() / 1000000.0;
                }
                m_lastImuTick = now;
                m_hasLastImuTick = true;

                m_imuProcessor.SetPaused(m_imuPaused.load());
                m_imuProcessor.SetResetRequested(m_imuResetRequested.exchange(false));

                imu::ImuProcessResult processResult;
                if (!m_imuProcessor.Update(rawSample, deltaSeconds, processResult) || !processResult.valid)
                {
                    return;
                }

                m_latestImuSample = processResult.sample;
                m_latestGyroLegacyOutput = processResult.legacyOutput;
            }

            void CaptureEngine::RunLoop()
            {
                while (m_running.load())
                {
                    if (!PumpOnce())
                    {
                        m_running.store(false);
                        break;
                    }
                }
            }
        }
    }
}
