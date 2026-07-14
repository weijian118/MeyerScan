#pragma once
// 内部类型只在 DLL 内部使用；公共 ABI 类型定义在 include/DeviceTransport.h。
#include <cstdint>

namespace meyer
{
    namespace device
    {
        typedef std::uint8_t Byte;

        enum class TransportType
        {
            Unknown = 0,
            CyApiUsb = 1
        };

        enum class DeviceType
        {
            Unknown = 0,
            Skys1000 = 6,
            Three = 7
        };

        enum class CaptureStatus
        {
            Timeout = -2,
            Failed = -1,
            Off = 0,
            OffPhoto = 1,
            Led = 2,
            LedPhoto = 3
        };

        enum class CaptureScanMode
        {
            Scan = 0,
            Calibration = 1,
            ColorCalibration = 2
        };

        enum class PictureOrderMode
        {
            Old = 1,
            NewOrder = 2,
            Aes = 3
        };
    }
}
