// =============================================================================
// 文件: CaptureServiceTestController.h
// 作用: 把 CaptureService C ABI 封装成测试界面可调用的无 UI 控制器。
// =============================================================================
#pragma once

#include "CaptureService.h"

#include <cstdint>
#include <string>
#include <vector>

class CaptureServiceTestController
{
public:
    // 构造时只创建 Service 句柄，设备在 Configure/Prepare 后才访问。
    CaptureServiceTestController();
    // 析构按停止、关闭、销毁顺序释放资源。
    ~CaptureServiceTestController();

    // simulator=true 使用 DeviceCmd 确定性模拟后端。
    std::int32_t Configure(bool simulator, std::uint32_t simulatedFlags = 0U);
    // 执行颜色校准准入、开始/停止采集和灯光命令。
    std::int32_t PrepareColorCalibration();
    std::int32_t StartCapture();
    std::int32_t StopCapture();
    std::int32_t RequestLight(bool on);

    // 复制设备、采集和组六图状态。
    bool ReadDeviceInfo(MeyerCaptureServiceDeviceInfo& info) const;
    bool ReadState(MeyerCaptureServiceStateSnapshot& state) const;
    bool ReadLatestGroupInfo(MeyerCaptureGroupInfo& info) const;
    // 非阻塞读取一条事件。
    bool PollEvent(MeyerCaptureServiceEvent& eventInfo);
    // 复制当前单图或 RGB888 数据。
    std::int32_t CopyPlane(std::int32_t index,
                           std::vector<unsigned char>& bytes) const;
    std::int32_t CopyRgb888(std::vector<unsigned char>& bytes) const;
    bool ReadPipelineOutputInfo(std::int32_t outputType,
                                MeyerCapturePipelineOutputInfo& info) const;
    std::int32_t CopyPipelineOutput(std::int32_t outputType,
                                    std::vector<unsigned char>& bytes) const;
    // 返回最近错误文本。
    std::string LastError() const;

private:
    MeyerCaptureServiceHandle m_handle;
};
