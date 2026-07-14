#pragma once
//通讯会话编排层
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../api/DeviceTypes.h"
#include "../core/capture/CaptureEngine.h"
#include "../model/TransportConfig.h"
#include "../transport/ITransport.h"

namespace meyer
{
    namespace device
    {
        struct ImageFrame;

        struct CaptureConfig
        {
            // 初始化安全默认值；调用方仍需补充真实图像和分包尺寸。
            CaptureConfig()
                : width(0)
                , height(0)
                , imageCount(0)
                , packetsPerImage(0)
                , transferSize(0)
                , queueDepth(0)
                , packetPayloadSize(0)
                , lastPacketValidSize(0)
                , timeoutMs(1500)
                , maxReadyFrames(3)
            {
            }

            int width;
            int height;
            int imageCount;
            int packetsPerImage;
            std::size_t transferSize;
            std::size_t queueDepth;
            int packetPayloadSize;
            int lastPacketValidSize;
            std::uint32_t timeoutMs;
            std::size_t maxReadyFrames;
        };

        class DeviceSession
        {
        public:
            // 创建尚未绑定具体传输实现的会话。
            DeviceSession();
            // 接管调用方传入的传输对象，unique_ptr 明确唯一所有权。
            explicit DeviceSession(std::unique_ptr<ITransport> transport);
            // 析构前关闭设备，随后由 unique_ptr 自动释放传输对象。
            ~DeviceSession();

            // 替换底层传输；替换前会关闭旧连接。
            void SetTransport(std::unique_ptr<ITransport> transport);
            // 返回可写传输指针，仅供 DLL 内部编排层调用。
            ITransport* GetTransport();
            // 返回只读传输指针，供状态查询使用。
            const ITransport* GetTransport() const;

            // 判断工厂是否已经创建具体传输对象。
            bool HasTransport() const;
            // 返回当前传输类型；无对象时返回 Unknown。
            TransportType GetTransportType() const;

            // 按配置创建或复用传输对象并打开设备。
            bool Open(const TransportConfig& config);
            // 停止数据流并关闭设备，允许重复调用。
            void Close();
            // 查询底层传输是否仍持有有效设备句柄。
            bool IsOpen() const;
            // 使用最近一次打开配置尝试恢复连接。
            bool Reconnect();

            // 发送调用方提供的原始命令字节，不解释命令业务含义。
            bool SendCommand(const Byte* data, std::size_t size, std::uint32_t timeoutMs);
            // 接收一包命令响应，vector 由会话内部按容量创建。
            bool ReceiveCommand(std::vector<Byte>& response, std::size_t capacity = 512, std::uint32_t timeoutMs = 0);

            // 进入原始流接收状态。
            bool StartStream();
            // 停止原始流并让传输实现回收异步资源。
            void StopStream();
            // 接收一包原始流数据，0 超时表示使用 Open 配置默认值。
            bool ReceiveStreamPacket(std::vector<Byte>& packet, std::size_t capacity, std::uint32_t timeoutMs = 0);

        private:
            // 仅在传输类型变化或尚未创建对象时调用工厂。
            bool EnsureTransport(const TransportConfig& config);
            // 把调用参数中的 0 转换成会话默认命令超时。
            std::uint32_t ResolveCommandTimeout(std::uint32_t timeoutMs) const;
            // 把调用参数中的 0 转换成会话默认流超时。
            std::uint32_t ResolveStreamTimeout(std::uint32_t timeoutMs) const;

        private:
            std::unique_ptr<ITransport> m_transport;
            TransportConfig m_lastConfig;
        };

        class DeviceFacade
        {
        public:
            // 初始化协议状态、扫描头默认值和空闲采集引擎。
            DeviceFacade();
            // 析构时停止采集线程并关闭会话。
            ~DeviceFacade();

            // 打开设备会话。
            bool Open(const TransportConfig& config);
            // 先停止采集，再关闭底层设备。
            void Close();
            // 查询连接状态。
            bool IsOpen() const;
            // 尝试恢复最近连接。
            bool Reconnect();

            // 发送原始命令，业务命令编码由上层 DeviceCmd 负责。
            bool SendRawCommand(const Byte* cmdSend, std::size_t size, std::uint32_t timeoutMs);
            // 接收命令响应并复制到调用方缓冲区。
            bool ReceiveCommand(Byte* cmdReceive, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs);
            // 启动原始流模式。
            bool StartStream();
            // 为 CyAPI 流预提交固定深度的异步请求。
            bool PrimeStream(std::size_t transferSize, std::size_t queueDepth);
            // 停止采集和原始流。
            void StopStream();
            // 接收一个原始 USB 数据包。
            bool ReceiveStreamPacket(Byte* packetBuffer, std::size_t capacity, std::size_t& receivedSize, std::uint32_t timeoutMs);

            // 返回 CyAPI 枚举到的设备数量。
            int GetDeviceCount() const;
            // 返回当前连接是否为 USB 2.x。
            bool GetIsUSB2() const;

            // 设置后续帧解析使用的设备型号。
            void SetDeviceType(DeviceType deviceType);
            // 返回当前设备型号。
            DeviceType GetDeviceType() const;
            // 设置图像平面排列协议。
            void SetPictureOrderMode(PictureOrderMode mode);
            // 返回图像平面排列协议。
            PictureOrderMode GetPictureOrderMode() const;
            // 设置扫描、三维校准或颜色校准用途。
            void SetCaptureScanMode(CaptureScanMode mode);
            // 返回当前采集用途。
            CaptureScanMode GetCaptureScanMode() const;
            // 控制是否计算 IMU 相对姿态。
            void SetAhrsEnabled(bool enabled);
            // 查询 IMU 姿态计算开关。
            bool GetAhrsEnabled() const;
            // 暂停或恢复向上层发布 IMU 姿态。
            void SetImuPaused(bool paused);
            // 请求下一份有效 IMU 样本重新建立参考零点。
            void RequestImuReferenceReset();
            // 设置尚未取得帧数据时使用的扫描头类型。
            void SetScanHeadType(int scanHeadType);
            // 优先返回最近帧解析出的扫描头类型。
            int GetScanHeadType() const;

            // 按完整采集配置启动后台收包、组帧和后处理流水线。
            bool StartCapture(const CaptureConfig& config);
            // 停止采集线程并清空最近帧。
            void StopCapture();
            // 同时检查编排标志和采集线程状态。
            bool IsCaptureActive() const;
            // 非阻塞获取一帧，没有完整帧时返回 false。
            bool GetFrame(ImageFrame& frame);
            // 返回最近交付帧的采集状态。
            int GetCaptureStatus() const;
            // 返回最近交付帧的三组曝光与增益。
            bool GetExposureState(int& timeW, int& timeC, int& timeX, int& gainW, int& gainC, int& gainX) const;
            // 返回最近交付帧的四路温度占位值。
            bool GetTemperatureState(int& temperature0, int& temperature1, int& temperature2, int& temperature3) const;
            // 把每个平面的曝光元数据展平到整数数组。
            bool GetThreeGainTime(std::vector<int>& expoValues) const;
            // 返回 roll/pitch/yaw 与四元数共七个兼容值。
            bool GetGyroscopeData(std::vector<float>& gyroValues) const;

            // 返回内部会话，供同模块的采集传输循环使用。
            DeviceSession& GetSession();
            // 返回只读内部会话。
            const DeviceSession& GetSession() const;

        private:
            // 把编排配置转换为纯 C++ 采集引擎配置并启动。
            bool StartPureCaptureBackend(const CaptureConfig& config);
            // 立即尝试从采集引擎队列取帧，不在当前线程等待。
            bool TryGetPureFrame(ImageFrame& frame);
            // 用读取时的真实连接状态补充帧元数据。
            void FillPureFrameRuntimeStatus(ImageFrame& frame) const;

        private:
            DeviceSession m_session;
            DeviceType m_deviceType;
            PictureOrderMode m_pictureOrderMode;
            CaptureScanMode m_captureScanMode;
            bool m_ahrsEnabled;
            bool m_gyroscopePauseFlag;
            bool m_resetImuReferenceRequested;
            int m_scanHeadType;
            bool m_captureActive;
            CaptureConfig m_captureConfig;
            capture::CaptureEngine m_pureCaptureEngine;
            ImageFrame m_lastPureFrame;
        };
    }
}

