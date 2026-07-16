// =============================================================================
// 文件: DeviceProtocolDefs.h
// 作用: 集中声明 2025-08-08 无线口扫协议的 A 类命令码和固定帧字段。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>

namespace meyer
{
    namespace devicecmd
    {
        namespace protocol
        {
            // A 类包固定以 0x5A 0x33 开始。
            static const std::uint8_t kHeader0 = 0x5AU;
            static const std::uint8_t kHeader1 = 0x33U;

            // 上位机发送命令时，现有 USB 示例固定追加两个 0x00。
            // 接收端仍按协议允许 0~3 个尾部 0x00，不能用发送规则反推响应长度。
            static const std::size_t kHostTrailerZeroCount = 2U;

            // 当前协议最大有效数据为 416 字节，保守放大到公共响应容量。
            static const std::size_t kMaximumPayloadBytes = 1024U;

            enum CommandCode : std::uint8_t
            {
                // 采集、复位、机器码和灯光基础命令。
                StartImageTransfer = 0x0AU,
                StopImageTransfer = 0x0BU,
                ForceLight = 0x0CU,
                StoreMachineCode = 0x0DU,
                SetLight = 0x0EU,
                ReadMainBoardVersion = 0x14U,
                UploadMainBoardVersion = 0x15U,
                ReadBattery = 0x1AU,
                UploadBattery = 0x1CU,
                MachineCodeStoreReply = 0x1DU,
                // 相机、颜色、温度和帧率命令。
                ReadCameraParameters = 0xA0U,
                UploadCameraParameters = 0xA1U,
                StoreCameraParameters = 0xA8U,
                CameraParametersStoreReply = 0xA9U,
                ReadColorMatrix = 0xA3U,
                UploadColorMatrix = 0xA4U,
                StoreColorMatrix = 0xA7U,
                ColorMatrixStoreReply = 0xAEU,
                SetCameraWindowPosition = 0xA5U,
                ReadTemperature = 0xAAU,
                UploadTemperature = 0xABU,
                SetFrameRate = 0xADU,
                EraseFirmware = 0xB6U,
                EraseFirmwareProgress = 0xB7U,
                WriteFirmware = 0xB4U,
                WriteFirmwareProgress = 0xB5U,
                // 两路相机标定、颜色标定和设备授权命令。
                StoreCamera1Calibration = 0xC3U,
                Camera1CalibrationStoreReply = 0xC5U,
                ReadCamera1Calibration = 0xC2U,
                UploadCamera1Calibration = 0xC7U,
                StoreCamera2Calibration = 0xD0U,
                Camera2CalibrationStoreReply = 0xD5U,
                ReadCamera2Calibration = 0xD2U,
                UploadCamera2Calibration = 0xD7U,
                StoreColorCalibration = 0xD1U,
                ColorCalibrationStoreReply = 0xD6U,
                ReadColorCalibration = 0xD3U,
                UploadColorCalibration = 0xD8U,
                StoreDeviceInfo = 0xC9U,
                DeviceInfoStoreReply = 0xCBU,
                ReadDeviceInfo = 0xCDU,
                UploadDeviceInfo = 0xCEU,
                ReadMachineCode = 0xD4U,
                UploadMachineCode = 0xD9U,
                SetExposure = 0xDBU,
                ReadExposure = 0xDCU,
                UploadExposure = 0xDEU,
                ResetController = 0xFFU
            };
        }
    }
}
