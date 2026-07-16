// =============================================================================
// 文件: test/main.cpp
// 项目: DeviceTransportTest.exe
//
// 作用:
//   同时提供不依赖设备的自动 smoke 测试和需要真实设备的人工联调命令。
//   默认不执行采图，避免开发机未连接设备时窗口一闪而退或误发硬件命令。
// =============================================================================
#include "DeviceTransport.h"
#include "processing/imu/ImuAttitudeProcessor.h"

#include <windows.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // RAII 包装器把 Create/Destroy 绑定到对象生命周期，所有提前 return 都不会泄漏句柄。
    class DeviceHandle
    {
    public:
        DeviceHandle()
            : m_handle(MeyerDeviceTransport_Create())
        {
        }

        ~DeviceHandle()
        {
            MeyerDeviceTransport_Destroy(m_handle);
            m_handle = nullptr;
        }

        MeyerDeviceTransportHandle Get() const
        {
            return m_handle;
        }

        bool IsValid() const
        {
            return m_handle != nullptr;
        }

    private:
        DeviceHandle(const DeviceHandle&);
        DeviceHandle& operator=(const DeviceHandle&);

        MeyerDeviceTransportHandle m_handle;
    };

    // 小型断言统计器让 smoke 测试失败时指出具体合同，而不是只返回一个数字。
    class SmokeRunner
    {
    public:
        SmokeRunner()
            : m_passed(0), m_failed(0)
        {
        }

        void Expect(bool condition, const char* description)
        {
            if (condition)
            {
                ++m_passed;
                std::cout << "[PASS] " << description << "\n";
            }
            else
            {
                ++m_failed;
                std::cerr << "[FAIL] " << description << "\n";
            }
        }

        int ExitCode() const
        {
            std::cout << "Smoke summary: passed=" << m_passed
                      << " failed=" << m_failed << "\n";
            return m_failed == 0 ? 0 : 1;
        }

    private:
        int m_passed;
        int m_failed;
    };

    // 输出命令行用法。硬件模式必须显式选择，smoke 是默认模式。
    void PrintUsage()
    {
        std::cout
            << "DeviceTransportTest usage:\n"
            << "  DeviceTransportTest --smoke\n"
            << "  DeviceTransportTest --info\n"
            << "  DeviceTransportTest --command <hex byte...>\n"
            << "  DeviceTransportTest --stream [packetCount] [transferSize] [queueDepth]\n"
            << "  DeviceTransportTest --capture <width> <height> <imageCount> <packetsPerImage> [frameCount]\n"
            << "  DeviceTransportTest --help\n";
    }

    // 把十进制或 0x 前缀文本转换为 size_t，并拒绝负数、尾随字符和溢出。
    bool ParseSize(const char* text, std::size_t& value)
    {
        if (text == nullptr || text[0] == '\0' || text[0] == '-')
        {
            return false;
        }

        errno = 0;
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(text, &end, 0);
        if (errno == ERANGE || end == text || *end != '\0' ||
            parsed > static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)()))
        {
            return false;
        }
        value = static_cast<std::size_t>(parsed);
        return true;
    }

    // 把一段十六进制文本转换为单字节，例如 FF、0A 或 0x1B。
    bool ParseHexByte(const char* text, unsigned char& value)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return false;
        }

        errno = 0;
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(text, &end, 16);
        if (errno == ERANGE || end == text || *end != '\0' || parsed > 0xFFUL)
        {
            return false;
        }
        value = static_cast<unsigned char>(parsed);
        return true;
    }

    // 使用两次调用读取错误文本：先取得所需长度，再分配恰好足够的 vector。
    std::string ReadLastError(MeyerDeviceTransportHandle handle)
    {
        std::size_t requiredSize = 0;
        const std::int32_t probeResult =
            MeyerDeviceTransport_GetLastError(handle, nullptr, 0, &requiredSize);
        if (probeResult != MeyerDeviceTransportResult_BufferTooSmall || requiredSize == 0U)
        {
            return std::string();
        }

        std::vector<char> buffer(requiredSize, '\0');
        if (MeyerDeviceTransport_GetLastError(handle, &buffer[0], buffer.size(), &requiredSize) !=
            MeyerDeviceTransportResult_Ok)
        {
            return std::string();
        }
        return std::string(&buffer[0]);
    }

    // 输出失败错误码和模块保存的详细文本，便于硬件联调定位。
    void PrintFailure(const char* operation,
                      std::int32_t result,
                      MeyerDeviceTransportHandle handle)
    {
        std::cerr << operation << " failed, result=" << result;
        const std::string detail = ReadLastError(handle);
        if (!detail.empty())
        {
            std::cerr << ", detail=" << detail;
        }
        std::cerr << "\n";
    }

    // 创建默认 CyAPI 配置并打开设备；硬件模式共用这一入口，减少重复清理代码。
    bool OpenDefaultDevice(DeviceHandle& device)
    {
        if (!device.IsValid())
        {
            std::cerr << "Could not allocate a device transport handle.\n";
            return false;
        }

        MeyerDeviceTransportOpenParams params;
        if (MeyerDeviceTransport_InitOpenParams(&params) != MeyerDeviceTransportResult_Ok)
        {
            std::cerr << "Could not initialize open parameters.\n";
            return false;
        }

        const std::int32_t result = MeyerDeviceTransport_Open(device.Get(), &params);
        if (result != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("Open", result, device.Get());
            return false;
        }
        return true;
    }

    // 无硬件 smoke 测试覆盖公共 ABI 的默认值、校验规则和错误文本合同。
    int RunSmoke()
    {
        SmokeRunner test;
        test.Expect(std::strcmp(MeyerDeviceTransport_GetModuleName(),
                                "MeyerScan_DeviceTransport") == 0,
                    "module name is stable");
        test.Expect(std::strcmp(MeyerDeviceTransport_GetApiVersion(), "1.0.0") == 0,
                    "API version is 1.0.0");
        test.Expect(GetMeyerModuleApiVersion() == 1,
                    "generic module ABI gate reports version 1");
        test.Expect(std::strcmp(GetMeyerModuleVersion(),
                                "MeyerScan_DeviceTransport v1.1.0 (2026-07-16)") == 0,
                    "code version follows the repository module format");

        test.Expect(MeyerDeviceTransport_InitOpenParams(nullptr) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "null open parameters are rejected");
        test.Expect(MeyerDeviceTransport_InitCaptureParams(nullptr) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "null capture parameters are rejected");

        MeyerDeviceTransportOpenParams openParams;
        test.Expect(MeyerDeviceTransport_InitOpenParams(&openParams) ==
                        MeyerDeviceTransportResult_Ok,
                    "open parameters initialize successfully");
        test.Expect(openParams.structSize == sizeof(openParams),
                    "open parameter structSize is initialized");
        test.Expect(openParams.schemaVersion == MEYER_DEVICE_TRANSPORT_SCHEMA_VERSION,
                    "open parameter schema version is initialized");
        test.Expect(openParams.transportType == MeyerDeviceTransportType_CyApiUsb,
                    "CyAPI USB is the default transport");

        MeyerDeviceCaptureStartParams captureParams;
        test.Expect(MeyerDeviceTransport_InitCaptureParams(&captureParams) ==
                        MeyerDeviceTransportResult_Ok,
                    "capture parameters initialize successfully");
        test.Expect(captureParams.maxReadyFrames == 3U,
                    "capture queue has a bounded default size");

        // 参数校验必须发生在硬件访问之前，因此无设备 smoke 也能验证异常内存
        // 请求不会进入 CyAPI 或 vector 分配路径。
        captureParams.width = static_cast<std::int32_t>(MEYER_DEVICE_TRANSPORT_MAX_DIMENSION + 1U);
        captureParams.height = 440;
        captureParams.imageCount = 6;
        captureParams.packetsPerImage = 28;
        test.Expect(MeyerDeviceTransport_StartCapture(nullptr, &captureParams) ==
                        MeyerDeviceTransportResult_InvalidHandle,
                    "null handle wins before capture parameter validation");

        MeyerDeviceFrameInfo frameInfo;
        test.Expect(MeyerDeviceTransport_InitFrameInfo(&frameInfo) ==
                        MeyerDeviceTransportResult_Ok,
                    "frame info initializes successfully");

        DeviceHandle device;
        test.Expect(device.IsValid(), "device handle is created");
        if (!device.IsValid())
        {
            return test.ExitCode();
        }

        test.Expect(MeyerDeviceTransport_IsOpen(device.Get()) == 0,
                    "new handle is closed");
        test.Expect(MeyerDeviceTransport_SendCommand(device.Get(), nullptr, 0, 100U) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "empty command is rejected before hardware access");
        test.Expect(!ReadLastError(device.Get()).empty(),
                    "failed operation stores a readable error");
        test.Expect(MeyerDeviceTransport_SetDeviceType(device.Get(), 999) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "unknown device type is rejected");
        test.Expect(MeyerDeviceTransport_SetPictureOrderMode(device.Get(), 0) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "unknown picture order is rejected");

        test.Expect(MeyerDeviceTransport_StartCapture(device.Get(), &captureParams) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "oversized capture dimension is rejected before hardware access");
        captureParams.width = 1024;
        captureParams.transferSize = MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE;
        captureParams.queueDepth = MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH;
        captureParams.packetPayloadSize = 16384;
        test.Expect(MeyerDeviceTransport_StartCapture(device.Get(), &captureParams) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "oversized in-flight transfer memory is rejected");
        captureParams.transferSize = 16384U;
        captureParams.queueDepth = 16U;
        captureParams.lastPacketValidSize = 1;
        test.Expect(MeyerDeviceTransport_StartCapture(device.Get(), &captureParams) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "inconsistent last packet size is rejected");
        captureParams.lastPacketValidSize = 0;
        test.Expect(MeyerDeviceTransport_StartCapture(device.Get(), &captureParams) ==
                        MeyerDeviceTransportResult_NotOpen,
                    "valid capture parameters reach the device state check");

        // 破坏 schemaVersion 可验证 DLL 不会把旧布局误读为当前结构。
        openParams.schemaVersion = 999U;
        test.Expect(MeyerDeviceTransport_Open(device.Get(), &openParams) ==
                        MeyerDeviceTransportResult_InvalidArgument,
                    "unsupported open parameter schema is rejected");
        test.Expect(MeyerDeviceTransport_Close(device.Get()) ==
                        MeyerDeviceTransportResult_Ok,
                    "close is idempotent without hardware");

        std::int32_t count = -1;
        test.Expect(MeyerDeviceTransport_GetDeviceCount(nullptr, &count) ==
                        MeyerDeviceTransportResult_InvalidHandle,
                    "null handle is rejected");

        // 私有 IMU 算法不依赖硬件，可用静止样本验证零范数保护和重置语义。
        meyer::device::imu::ImuAttitudeProcessor imuProcessor;
        std::vector<double> stationarySample(6U, 0.0);
        stationarySample[2] = 1.0;
        meyer::device::imu::ImuProcessResult imuResult;
        test.Expect(imuProcessor.Update(stationarySample, 0.0, imuResult) && imuResult.valid,
                    "IMU accepts first stationary sample with default delta");
        test.Expect(imuProcessor.Update(stationarySample, 1.0 / 180.0, imuResult) &&
                        std::isfinite(imuResult.sample.quatW) &&
                        std::isfinite(imuResult.sample.quatX) &&
                        std::isfinite(imuResult.sample.quatY) &&
                        std::isfinite(imuResult.sample.quatZ),
                    "IMU quaternion remains finite after warmup");

        std::vector<double> zeroAccelerationSample(6U, 0.0);
        zeroAccelerationSample[5] = 0.01;
        test.Expect(imuProcessor.Update(zeroAccelerationSample, 1.0 / 180.0, imuResult) &&
                        std::isfinite(imuResult.sample.yaw),
                    "IMU zero acceleration does not divide by zero");
        imuProcessor.SetResetRequested(true);
        test.Expect(imuProcessor.Update(stationarySample, 1.0 / 180.0, imuResult) &&
                        std::fabs(imuResult.sample.quatW - 1.0) < 1.0e-6,
                    "IMU reset publishes a new identity reference");
        return test.ExitCode();
    }

    // 枚举并显示当前设备连接信息。
    int RunInfo()
    {
        DeviceHandle device;
        if (!OpenDefaultDevice(device))
        {
            return 2;
        }

        std::int32_t deviceCount = 0;
        std::int32_t isUsb2 = 0;
        const std::int32_t countResult =
            MeyerDeviceTransport_GetDeviceCount(device.Get(), &deviceCount);
        const std::int32_t speedResult =
            MeyerDeviceTransport_GetIsUsb2(device.Get(), &isUsb2);
        if (countResult != MeyerDeviceTransportResult_Ok ||
            speedResult != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("Get device info", countResult != 0 ? countResult : speedResult, device.Get());
            return 2;
        }

        std::cout << "Open=1 DeviceCount=" << deviceCount
                  << " USB=" << (isUsb2 ? "2.x" : "3.x") << "\n";
        MeyerDeviceTransport_Close(device.Get());
        return 0;
    }

    // 发送命令行给出的十六进制命令，并尝试接收一包响应。
    int RunCommand(int argc, char* argv[])
    {
        if (argc < 3)
        {
            PrintUsage();
            return 1;
        }

        std::vector<unsigned char> command;
        for (int index = 2; index < argc; ++index)
        {
            unsigned char value = 0;
            if (!ParseHexByte(argv[index], value))
            {
                std::cerr << "Invalid hex byte: " << argv[index] << "\n";
                return 1;
            }
            command.push_back(value);
        }

        DeviceHandle device;
        if (!OpenDefaultDevice(device))
        {
            return 2;
        }

        std::int32_t result = MeyerDeviceTransport_SendCommand(
            device.Get(), &command[0], command.size(), 1500U);
        if (result != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("SendCommand", result, device.Get());
            return 2;
        }

        std::vector<unsigned char> response(512U, 0U);
        std::size_t receivedSize = 0;
        result = MeyerDeviceTransport_ReceiveCommand(
            device.Get(), &response[0], response.size(), &receivedSize, 1500U);
        if (result != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("ReceiveCommand", result, device.Get());
            return 2;
        }

        std::cout << "Received " << receivedSize << " bytes:";
        for (std::size_t index = 0; index < receivedSize; ++index)
        {
            std::cout << ' ' << std::uppercase << std::hex << std::setw(2)
                      << std::setfill('0') << static_cast<unsigned int>(response[index]);
        }
        std::cout << std::dec << "\n";
        return 0;
    }

    // 显式启动原始流并读取有限数量的数据包。
    int RunStream(int argc, char* argv[])
    {
        std::size_t packetCount = 10U;
        std::size_t transferSize = 16384U;
        std::size_t queueDepth = 8U;
        if ((argc > 2 && !ParseSize(argv[2], packetCount)) ||
            (argc > 3 && !ParseSize(argv[3], transferSize)) ||
            (argc > 4 && !ParseSize(argv[4], queueDepth)) ||
            packetCount == 0U || packetCount > 100000U ||
            transferSize == 0U ||
            transferSize > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_TRANSFER_SIZE) ||
            queueDepth == 0U ||
            queueDepth > static_cast<std::size_t>(MEYER_DEVICE_TRANSPORT_MAX_QUEUE_DEPTH))
        {
            std::cerr << "Invalid stream arguments.\n";
            return 1;
        }

        DeviceHandle device;
        if (!OpenDefaultDevice(device))
        {
            return 2;
        }

        std::int32_t result = MeyerDeviceTransport_StartStream(device.Get());
        if (result == MeyerDeviceTransportResult_Ok)
        {
            result = MeyerDeviceTransport_PrimeStream(device.Get(), transferSize, queueDepth);
        }
        if (result != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("Start/Prime stream", result, device.Get());
            return 2;
        }

        std::vector<unsigned char> packet(transferSize, 0U);
        for (std::size_t index = 0; index < packetCount; ++index)
        {
            std::size_t receivedSize = 0;
            result = MeyerDeviceTransport_ReceiveStreamPacket(
                device.Get(), &packet[0], packet.size(), &receivedSize, 1500U);
            if (result != MeyerDeviceTransportResult_Ok)
            {
                PrintFailure("ReceiveStreamPacket", result, device.Get());
                MeyerDeviceTransport_StopStream(device.Get());
                return 2;
            }
            std::cout << "Packet[" << index << "] bytes=" << receivedSize << "\n";
        }

        MeyerDeviceTransport_StopStream(device.Get());
        return 0;
    }

    // 按命令行尺寸启动完整组帧采集并读取有限帧数。
    int RunCapture(int argc, char* argv[])
    {
        if (argc < 6)
        {
            PrintUsage();
            return 1;
        }

        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t imageCount = 0;
        std::size_t packetsPerImage = 0;
        std::size_t frameCount = 1U;
        if (!ParseSize(argv[2], width) || !ParseSize(argv[3], height) ||
            !ParseSize(argv[4], imageCount) || !ParseSize(argv[5], packetsPerImage) ||
            (argc > 6 && !ParseSize(argv[6], frameCount)) ||
            width == 0U || width > MEYER_DEVICE_TRANSPORT_MAX_DIMENSION ||
            height == 0U || height > MEYER_DEVICE_TRANSPORT_MAX_DIMENSION ||
            imageCount == 0U || imageCount > MEYER_DEVICE_TRANSPORT_MAX_IMAGE_COUNT ||
            packetsPerImage == 0U ||
            packetsPerImage > MEYER_DEVICE_TRANSPORT_MAX_PACKETS_PER_IMAGE ||
            frameCount == 0U || frameCount > 10000U)
        {
            std::cerr << "Invalid capture arguments.\n";
            return 1;
        }

        DeviceHandle device;
        if (!OpenDefaultDevice(device))
        {
            return 2;
        }

        MeyerDeviceCaptureStartParams params;
        MeyerDeviceTransport_InitCaptureParams(&params);
        params.width = static_cast<std::int32_t>(width);
        params.height = static_cast<std::int32_t>(height);
        params.imageCount = static_cast<std::int32_t>(imageCount);
        params.packetsPerImage = static_cast<std::int32_t>(packetsPerImage);
        // 保持 0 让 DLL 用经过 64 位溢出保护的统一逻辑推导末包长度，测试
        // 宿主不重复协议公式，避免两处实现随协议调整后产生差异。
        params.lastPacketValidSize = 0;

        std::int32_t result = MeyerDeviceTransport_StartCapture(device.Get(), &params);
        if (result != MeyerDeviceTransportResult_Ok)
        {
            PrintFailure("StartCapture", result, device.Get());
            return 2;
        }

        const std::size_t expectedBytes = width * height * imageCount;
        std::vector<unsigned char> pixels(expectedBytes, 0U);
        for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            MeyerDeviceFrameInfo info;
            MeyerDeviceTransport_InitFrameInfo(&info);
            std::size_t frameBytes = 0;

            // GetFrame 非阻塞返回 NotReady；测试宿主最多轮询 5 秒。
            const DWORD deadline = GetTickCount() + 5000U;
            do
            {
                result = MeyerDeviceTransport_GetFrame(
                    device.Get(), &pixels[0], pixels.size(), &frameBytes, &info);
                if (result == MeyerDeviceTransportResult_NotReady)
                {
                    Sleep(5U);
                }
            } while (result == MeyerDeviceTransportResult_NotReady &&
                     static_cast<LONG>(deadline - GetTickCount()) > 0);

            if (result != MeyerDeviceTransportResult_Ok)
            {
                PrintFailure("GetFrame", result, device.Get());
                MeyerDeviceTransport_StopCapture(device.Get());
                return 2;
            }
            std::cout << "Frame[" << frameIndex << "] bytes=" << frameBytes
                      << " size=" << info.width << 'x' << info.height
                      << " images=" << info.imageCount << "\n";
        }

        MeyerDeviceTransport_StopCapture(device.Get());
        return 0;
    }
}

// 程序入口只负责模式分派；无参数时运行可自动回归的 smoke 测试。
int main(int argc, char* argv[])
{
    if (argc < 2 || std::strcmp(argv[1], "--smoke") == 0)
    {
        return RunSmoke();
    }
    if (std::strcmp(argv[1], "--info") == 0)
    {
        return RunInfo();
    }
    if (std::strcmp(argv[1], "--command") == 0)
    {
        return RunCommand(argc, argv);
    }
    if (std::strcmp(argv[1], "--stream") == 0)
    {
        return RunStream(argc, argv);
    }
    if (std::strcmp(argv[1], "--capture") == 0)
    {
        return RunCapture(argc, argv);
    }
    if (std::strcmp(argv[1], "--help") == 0 ||
        std::strcmp(argv[1], "-h") == 0)
    {
        PrintUsage();
        return 0;
    }

    std::cerr << "Unknown mode: " << argv[1] << "\n";
    PrintUsage();
    return 1;
}
