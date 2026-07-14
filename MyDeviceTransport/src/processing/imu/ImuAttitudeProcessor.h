// =============================================================================
// 文件: ImuAttitudeProcessor.h
// 作用: 声明不依赖 Eigen 的 IMU 四元数姿态处理器。
// =============================================================================
#pragma once

#include <vector>

#include "IImuProcessor.h"

namespace meyer
{
    namespace device
    {
        namespace imu
        {
            // 使用陀螺仪积分并通过加速度重力方向做比例校正。
            // 该算法只使用固定数量 double，不进行动态矩阵分配，适合采集线程持续调用。
            class ImuAttitudeProcessor : public IImuProcessor
            {
            public:
                // 初始化单位四元数、空参考点和未暂停状态。
                ImuAttitudeProcessor();
                // 处理器只持有值成员，使用默认析构语义。
                virtual ~ImuAttitudeProcessor();

                // 暂停时继续接收原始样本，但冻结向上层发布的相对姿态。
                virtual void SetPaused(bool paused) override;

                // true 表示下一份有效样本应重新建立相对姿态零点。
                virtual void SetResetRequested(bool resetRequested) override;

                // rawSample 顺序为 ax, ay, az, gx, gy, gz。
                virtual bool Update(const std::vector<double>& rawSample,
                                    double deltaSeconds,
                                    ImuProcessResult& result) override;

            private:
                struct Quaternion
                {
                    double w;
                    double x;
                    double y;
                    double z;

                    // 创建单位四元数。
                    Quaternion();
                    // 创建调用方指定四分量的四元数。
                    Quaternion(double valueW, double valueX, double valueY, double valueZ);
                };

                // 用一份 IMU 样本推进内部绝对四元数。
                void Integrate(double accelX,
                               double accelY,
                               double accelZ,
                               double gyroX,
                               double gyroY,
                               double gyroZ,
                               double deltaSeconds);

                // 四元数基础运算均为值语义，不向 DLL 外暴露内存。
                static Quaternion Multiply(const Quaternion& left, const Quaternion& right);
                static Quaternion Inverse(const Quaternion& value);
                static bool Normalize(Quaternion& value);
                static double Clamp(double value, double minimum, double maximum);

                // 将相对四元数写入统一结果结构和历史兼容 float 数组。
                void FillResult(const std::vector<double>& rawSample,
                                const Quaternion& relative,
                                ImuProcessResult& result) const;

            private:
                bool m_paused;
                bool m_resetRequested;
                bool m_referenceInitialized;
                int m_warmupSampleCount;
                double m_elapsedSeconds;
                Quaternion m_orientation;
                Quaternion m_reference;
                Quaternion m_publishedOrientation;
            };
        }
    }
}
