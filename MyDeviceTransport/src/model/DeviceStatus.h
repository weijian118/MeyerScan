#pragma once

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        struct DeviceStatus
        {
            // 状态默认表示未打开且采集失败，防止空帧被误判为有效。
            DeviceStatus()
                : isOpen(false)
                , isUsb2(false)
                , deviceType(DeviceType::Unknown)
                , captureStatus(CaptureStatus::Failed)
                , scanMode(CaptureScanMode::Scan)
                , pictureOrderMode(PictureOrderMode::Old)
                , scanHeadType(3)
                , ledOn(true)
                , photoMode(false)
                , timeW(0)
                , timeC(0)
                , timeX(0)
                , gainW(0)
                , gainC(0)
                , gainX(0)
                , temperature0(0)
                , temperature1(0)
                , temperature2(0)
                , temperature3(0)
            {
            }

            bool isOpen;
            bool isUsb2;
            DeviceType deviceType;
            CaptureStatus captureStatus;
            CaptureScanMode scanMode;
            PictureOrderMode pictureOrderMode;

            int scanHeadType;
            bool ledOn;
            bool photoMode;

            int timeW;
            int timeC;
            int timeX;
            int gainW;
            int gainC;
            int gainX;

            int temperature0;
            int temperature1;
            int temperature2;
            int temperature3;
        };
    }
}
