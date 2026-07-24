// =============================================================================
// 文件: DeviceCmdLibrary.h
// 作用: 声明 DeviceCmd DLL 的动态加载适配器。
// =============================================================================
#pragma once

#include "DeviceCmd.h"

#include <string>

namespace meyer
{
    namespace captureservice
    {
        class DeviceCmdLibrary
        {
        public:
            // 构造空加载器，不访问文件或设备。
            DeviceCmdLibrary();
            // 释放设备句柄和 DLL 模块。
            ~DeviceCmdLibrary();

            // 按绝对路径加载并校验 DeviceCmd ABI。
            std::int32_t Load(const std::string& pathUtf8);
            // 使用 DeviceCmd 的初始化函数写入与其版本匹配的打开参数。
            std::int32_t InitOpenParams(MeyerDeviceCmdOpenParams& params);
            // 初始化预检、状态和流诊断 POD 的结构头。
            std::int32_t InitCalibrationPreflight(
                MeyerDeviceCalibrationPreflight& preflight);
            std::int32_t InitStateSnapshot(MeyerDeviceStateSnapshot& snapshot);
            std::int32_t InitStreamDiagnostics(
                MeyerDeviceCmdStreamDiagnostics& diagnostics);
            // 关闭当前设备会话，但保留已加载 DLL。
            std::int32_t Close();
            // 通过 DeviceCmd 打开设备。
            std::int32_t Open(const MeyerDeviceCmdOpenParams& params);
            // 执行颜色校准准入预检。
            std::int32_t PrepareColorCalibration(
                const MeyerDeviceCmdOpenParams& params,
                MeyerDeviceCalibrationPreflight& preflight);
            // 复制最近状态，不产生 USB 请求。
            std::int32_t GetStateSnapshot(MeyerDeviceStateSnapshot& snapshot);
            // 轻量连接检查，不发送 A 类命令。
            std::int32_t IsDeviceConnectedLightweight();
            // 取得模型默认的 DeviceCmd 采集参数。
            std::int32_t InitCaptureParamsForModel(
                std::int32_t model,
                MeyerDeviceCmdCaptureParams& params);
            // 启动、停止和接收原始 B 包。
            std::int32_t StartRawCapture(const MeyerDeviceCmdCaptureParams& params);
            std::int32_t StopRawCapture(bool turnLightOff);
            std::int32_t ReceiveRawCapturePacket(unsigned char* buffer,
                                                 std::size_t capacity,
                                                 std::size_t& receivedSize,
                                                 std::uint32_t timeoutMs);
            // 复制底层流诊断。
            std::int32_t GetStreamDiagnostics(MeyerDeviceCmdStreamDiagnostics& diagnostics);
            // 开关灯命令只在服务指定的串行命令窗口中调用。
            std::int32_t SetLight(bool on);
            // 在线下发自动曝光模块产生的 16 字节参数；当前占位算法不会调用。
            std::int32_t SetExposureParameters(
                const MeyerDeviceCmdExposureParameters& parameters);

            // 返回最近一次动态加载或设备调用错误。
            const std::string& LastError() const;
            // 返回是否已经加载 DLL。
            bool IsLoaded() const;
            // 返回是否存在打开的 DeviceCmd 会话。
            bool IsOpen() const;

        private:
            struct Functions;
            // 解析 DLL 导出并建立函数表。
            std::int32_t ResolveFunctions();
            // 释放函数表、设备句柄和模块句柄。
            void Unload();
            // 将 DeviceCmd 最近错误复制到本地字符串。
            void ReadLastError();

            void* m_module;
            MeyerDeviceCmdHandle m_handle;
            Functions* m_functions;
            std::string m_lastError;
        };
    }
}
