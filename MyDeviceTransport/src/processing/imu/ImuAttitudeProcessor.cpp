// =============================================================================
// 文件: ImuAttitudeProcessor.cpp
// 作用: 实现无第三方矩阵库依赖的稳定 IMU 姿态估计。
// =============================================================================
#include "ImuAttitudeProcessor.h"

#include <cmath>

namespace
{
    const double kDefaultDeltaSeconds = 1.0 / 180.0;
    const double kMaximumDeltaSeconds = 0.1;
    const double kMinimumNormSquared = 1.0e-12;
    const double kGravityCorrectionGain = 0.8;
}

namespace meyer
{
    namespace device
    {
        namespace imu
        {
            // 默认四元数为单位旋转。
            ImuAttitudeProcessor::Quaternion::Quaternion()
                : w(1.0), x(0.0), y(0.0), z(0.0)
            {
            }

            // 使用明确的四个分量构造四元数。
            ImuAttitudeProcessor::Quaternion::Quaternion(double valueW,
                                                         double valueX,
                                                         double valueY,
                                                         double valueZ)
                : w(valueW), x(valueX), y(valueY), z(valueZ)
            {
            }

            // 初始化姿态、参考零点和累计时间。
            ImuAttitudeProcessor::ImuAttitudeProcessor()
                : m_paused(false)
                , m_resetRequested(false)
                , m_referenceInitialized(false)
                , m_warmupSampleCount(0)
                , m_elapsedSeconds(0.0)
            {
            }

            // 所有资源都是值成员，析构无需额外释放。
            ImuAttitudeProcessor::~ImuAttitudeProcessor()
            {
            }

            // 保存发布暂停状态。采集线程会串行调用 Update，因此成员无需再次加锁。
            void ImuAttitudeProcessor::SetPaused(bool paused)
            {
                m_paused = paused;
            }

            // 只接受 true 请求；false 不会取消一个尚未消费的重置动作。
            void ImuAttitudeProcessor::SetResetRequested(bool resetRequested)
            {
                if (resetRequested)
                {
                    m_resetRequested = true;
                }
            }

            // 校验输入、推进姿态、处理零点并生成输出。
            bool ImuAttitudeProcessor::Update(const std::vector<double>& rawSample,
                                              double deltaSeconds,
                                              ImuProcessResult& result)
            {
                result.Clear();
                if (rawSample.size() != 6U)
                {
                    return false;
                }

                // 首包和长时间停顿后的 delta 不可信，回退到设备历史采样频率 180 Hz。
                if (deltaSeconds <= 0.0 || deltaSeconds > kMaximumDeltaSeconds)
                {
                    deltaSeconds = kDefaultDeltaSeconds;
                }
                m_elapsedSeconds += deltaSeconds;

                Integrate(rawSample[0], rawSample[1], rawSample[2],
                          rawSample[3], rawSample[4], rawSample[5],
                          deltaSeconds);

                // 预热两份样本后用当前绝对姿态的逆建立零点，使初次输出接近单位旋转。
                if (!m_referenceInitialized)
                {
                    ++m_warmupSampleCount;
                    if (m_warmupSampleCount >= 2)
                    {
                        m_reference = Inverse(m_orientation);
                        m_publishedOrientation = Quaternion();
                        m_referenceInitialized = true;
                    }
                }

                // 重置请求只在此处消费一次；参考乘当前姿态后得到单位旋转。
                if (m_resetRequested)
                {
                    m_reference = Inverse(m_orientation);
                    m_publishedOrientation = Quaternion();
                    m_referenceInitialized = true;
                    m_resetRequested = false;
                }

                Quaternion relative = Multiply(m_reference, m_orientation);
                Normalize(relative);

                // 暂停只冻结发布值，不停止内部积分；恢复后不会因累计陀螺数据丢失而跳变。
                if (!m_paused)
                {
                    m_publishedOrientation = relative;
                }

                FillResult(rawSample, m_publishedOrientation, result);
                result.sample.timestampSeconds = m_elapsedSeconds;
                result.valid = true;
                return true;
            }

            // 使用 Mahony 风格的重力误差反馈校正陀螺积分。
            void ImuAttitudeProcessor::Integrate(double accelX,
                                                 double accelY,
                                                 double accelZ,
                                                 double gyroX,
                                                 double gyroY,
                                                 double gyroZ,
                                                 double deltaSeconds)
            {
                const double accelNormSquared =
                    accelX * accelX + accelY * accelY + accelZ * accelZ;
                if (accelNormSquared > kMinimumNormSquared)
                {
                    // 归一化后只使用加速度方向，不让量纲和幅值影响姿态校正。
                    const double inverseAccelNorm = 1.0 / std::sqrt(accelNormSquared);
                    accelX *= inverseAccelNorm;
                    accelY *= inverseAccelNorm;
                    accelZ *= inverseAccelNorm;

                    // 根据当前四元数估计机体坐标系中的重力方向。
                    const double gravityX = 2.0 * (m_orientation.x * m_orientation.z -
                                                   m_orientation.w * m_orientation.y);
                    const double gravityY = 2.0 * (m_orientation.w * m_orientation.x +
                                                   m_orientation.y * m_orientation.z);
                    const double gravityZ = m_orientation.w * m_orientation.w -
                                            m_orientation.x * m_orientation.x -
                                            m_orientation.y * m_orientation.y +
                                            m_orientation.z * m_orientation.z;

                    // 测量重力与估计重力的叉积就是旋转误差方向。
                    const double errorX = accelY * gravityZ - accelZ * gravityY;
                    const double errorY = accelZ * gravityX - accelX * gravityZ;
                    const double errorZ = accelX * gravityY - accelY * gravityX;
                    gyroX += kGravityCorrectionGain * errorX;
                    gyroY += kGravityCorrectionGain * errorY;
                    gyroZ += kGravityCorrectionGain * errorZ;
                }

                // q_dot = 0.5 * q * omega；先保存旧值，避免分量原地更新互相污染。
                const double halfDelta = 0.5 * deltaSeconds;
                const double oldW = m_orientation.w;
                const double oldX = m_orientation.x;
                const double oldY = m_orientation.y;
                const double oldZ = m_orientation.z;
                m_orientation.w += halfDelta * (-oldX * gyroX - oldY * gyroY - oldZ * gyroZ);
                m_orientation.x += halfDelta * ( oldW * gyroX + oldY * gyroZ - oldZ * gyroY);
                m_orientation.y += halfDelta * ( oldW * gyroY - oldX * gyroZ + oldZ * gyroX);
                m_orientation.z += halfDelta * ( oldW * gyroZ + oldX * gyroY - oldY * gyroX);

                // 数值积分会产生长度漂移，逐样本归一化以保持合法旋转。
                if (!Normalize(m_orientation))
                {
                    m_orientation = Quaternion();
                }
            }

            // Hamilton 乘法用于组合参考旋转和当前旋转。
            ImuAttitudeProcessor::Quaternion ImuAttitudeProcessor::Multiply(
                const Quaternion& left,
                const Quaternion& right)
            {
                return Quaternion(
                    left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
                    left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
                    left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
                    left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w);
            }

            // 单位四元数的逆等于共轭；先归一化可容忍轻微数值漂移。
            ImuAttitudeProcessor::Quaternion ImuAttitudeProcessor::Inverse(const Quaternion& value)
            {
                Quaternion normalized = value;
                if (!Normalize(normalized))
                {
                    return Quaternion();
                }
                return Quaternion(normalized.w, -normalized.x, -normalized.y, -normalized.z);
            }

            // 归一化成功返回 true；接近零长度时不执行除法。
            bool ImuAttitudeProcessor::Normalize(Quaternion& value)
            {
                const double normSquared = value.w * value.w + value.x * value.x +
                                           value.y * value.y + value.z * value.z;
                if (normSquared <= kMinimumNormSquared)
                {
                    return false;
                }
                const double inverseNorm = 1.0 / std::sqrt(normSquared);
                value.w *= inverseNorm;
                value.x *= inverseNorm;
                value.y *= inverseNorm;
                value.z *= inverseNorm;
                return true;
            }

            // 把浮点数限制在闭区间，主要用于防止 asin 因舍入误差收到区间外参数。
            double ImuAttitudeProcessor::Clamp(double value, double minimum, double maximum)
            {
                return value < minimum ? minimum : (value > maximum ? maximum : value);
            }

            // 将原始数据、欧拉角和四元数装入统一结果。
            void ImuAttitudeProcessor::FillResult(const std::vector<double>& rawSample,
                                                  const Quaternion& relative,
                                                  ImuProcessResult& result) const
            {
                result.sample.accelX = rawSample[0];
                result.sample.accelY = rawSample[1];
                result.sample.accelZ = rawSample[2];
                result.sample.gyroX = rawSample[3];
                result.sample.gyroY = rawSample[4];
                result.sample.gyroZ = rawSample[5];

                result.sample.roll = std::atan2(
                    2.0 * (relative.w * relative.x + relative.y * relative.z),
                    1.0 - 2.0 * (relative.x * relative.x + relative.y * relative.y));
                result.sample.pitch = std::asin(Clamp(
                    2.0 * (relative.w * relative.y - relative.z * relative.x), -1.0, 1.0));
                result.sample.yaw = std::atan2(
                    2.0 * (relative.w * relative.z + relative.x * relative.y),
                    1.0 - 2.0 * (relative.y * relative.y + relative.z * relative.z));
                result.sample.quatW = relative.w;
                result.sample.quatX = relative.x;
                result.sample.quatY = relative.y;
                result.sample.quatZ = relative.z;
                result.sample.valid = true;

                result.legacyOutput.push_back(static_cast<float>(result.sample.roll));
                result.legacyOutput.push_back(static_cast<float>(result.sample.pitch));
                result.legacyOutput.push_back(static_cast<float>(result.sample.yaw));
                result.legacyOutput.push_back(static_cast<float>(relative.w));
                result.legacyOutput.push_back(static_cast<float>(relative.x));
                result.legacyOutput.push_back(static_cast<float>(relative.y));
                result.legacyOutput.push_back(static_cast<float>(relative.z));
            }
        }
    }
}
