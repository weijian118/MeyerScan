#pragma once

#include <vector>

#include "../../model/ImuSample.h"

namespace meyer
{
    namespace device
    {
        namespace imu
        {
            struct ImuProcessResult
            {
                // 新结果默认无效。
                ImuProcessResult()
                    : valid(false)
                {
                }

                // 清理结构和兼容数组，避免读取上一次姿态。
                void Clear()
                {
                    sample = ImuSample();
                    legacyOutput.clear();
                    valid = false;
                }

                ImuSample sample;
                std::vector<float> legacyOutput;
                bool valid;
            };

            class IImuProcessor
            {
            public:
                // 通过虚析构允许持有接口指针时安全释放具体处理器。
                virtual ~IImuProcessor()
                {
                }

                // 暂停或恢复相对姿态发布。
                virtual void SetPaused(bool paused) = 0;
                // 请求重新建立姿态参考零点。
                virtual void SetResetRequested(bool resetRequested) = 0;
                // 用一份六轴样本和时间步长更新姿态结果。
                virtual bool Update(const std::vector<double>& rawSample, double deltaSeconds, ImuProcessResult& result) = 0;
            };
        }
    }
}
