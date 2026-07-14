#pragma once

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        struct ExposureParams
        {
            // 新曝光元数据全部置零。
            ExposureParams()
                : primary(0)
                , secondary(0)
                , analogGain(0)
            {
            }

            int primary;
            int secondary;
            int analogGain;
        };
    }
}
