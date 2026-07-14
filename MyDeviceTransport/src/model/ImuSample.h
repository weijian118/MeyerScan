#pragma once

namespace meyer
{
    namespace device
    {
        struct ImuSample
        {
            // 默认四元数为单位旋转，其余数值为 0，valid 明确为 false。
            ImuSample()
                : accelX(0.0)
                , accelY(0.0)
                , accelZ(0.0)
                , gyroX(0.0)
                , gyroY(0.0)
                , gyroZ(0.0)
                , roll(0.0)
                , pitch(0.0)
                , yaw(0.0)
                , quatW(1.0)
                , quatX(0.0)
                , quatY(0.0)
                , quatZ(0.0)
                , timestampSeconds(0.0)
                , valid(false)
            {
            }

            double accelX;
            double accelY;
            double accelZ;

            double gyroX;
            double gyroY;
            double gyroZ;

            double roll;
            double pitch;
            double yaw;

            double quatW;
            double quatX;
            double quatY;
            double quatZ;

            double timestampSeconds;
            bool valid;
        };
    }
}
