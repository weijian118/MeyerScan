// =============================================================================
// 文件: test/main.cpp
// 作用: 运行 MyDeviceCmd 的无硬件 smoke 和可选真实后端检查。
//
// 测试按照真实调用顺序验证公共 ABI：创建会话、打开模拟设备、刷新状态、
// 下发灯光命令、启动采集、读取一帧、停止采集和关闭会话。这样可以在没有 USB
// 设备的开发机上先发现接口/状态机问题，但不会把模拟成功误认为硬件成功。
// =============================================================================
#include "../include/DeviceCmd.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    class TestContext
    {
    public:
        // 测试开始时清零失败计数，所有断言共用该计数器。
        TestContext() : failures(0) {}

        // 统一输出断言结果，失败数最后转换为进程返回码。
        void Expect(bool condition, const std::string& name)
        {
            if (condition)
            {
                std::cout << "[PASS] " << name << "\n";
            }
            else
            {
                std::cout << "[FAIL] " << name << "\n";
                ++failures;
            }
        }

        int failures;
    };

    // 输出指定句柄的最近错误，便于 VS2015 直接运行测试时定位失败原因。
    void PrintLastError(MeyerDeviceCmdHandle handle)
    {
        std::size_t required = 0U;
        MeyerDeviceCmd_GetLastError(handle, nullptr, 0U, &required);
        if (required == 0U || required > 4096U)
        {
            return;
        }
        std::vector<char> message(required, '\0');
        MeyerDeviceCmd_GetLastError(handle, &message[0], message.size(), &required);
        std::cout << "       error: " << &message[0] << "\n";
    }

    // 验证协议帧能从 DeviceCmd 端到达模拟设备并获得期望响应。
    void TestRawCommand(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        MeyerDeviceCmdRawResponse response;
        MeyerDeviceCmd_InitRawResponse(&response);
        const std::int32_t result = MeyerDeviceCmd_ExecuteRawCommand(
            handle,
            0xD4U,
            nullptr,
            0U,
            0xD9,
            &response,
            100U);
        test.Expect(result == MeyerDeviceCmdResult_Ok, "machine-code request/response round trip");
        if (result != MeyerDeviceCmdResult_Ok)
        {
            PrintLastError(handle);
            return;
        }
        test.Expect(response.commandCode == 0xD9 && response.payloadSize == 13U,
                    "machine-code response has the expected command and payload size");
    }

    // 验证基础信息查询后 validFields 和关键内容都被写入。
    void TestStateRefresh(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        const std::int32_t result = MeyerDeviceCmd_RefreshBasicState(handle);
        test.Expect(result == MeyerDeviceCmdResult_Ok, "basic device state refresh succeeds");
        if (result != MeyerDeviceCmdResult_Ok)
        {
            PrintLastError(handle);
            return;
        }

        MeyerDeviceStateSnapshot state;
        MeyerDeviceCmd_InitStateSnapshot(&state);
        test.Expect(MeyerDeviceCmd_GetStateSnapshot(handle, &state) == MeyerDeviceCmdResult_Ok,
                    "state snapshot can be read without another USB request");
        test.Expect((state.validFields & MeyerDeviceStateField_MachineCode) != 0U &&
                    std::string(state.deviceIdUtf8) == "6200005301203",
                    "device number is decoded from protocol digit bytes");
        test.Expect(state.model == MeyerDeviceModel_MyScan6Wireless &&
                    state.modelSource == MeyerDeviceModelSource_HostHint,
                    "model value is explicitly marked as a host hint");
        test.Expect((state.validFields & MeyerDeviceStateField_Battery) != 0U &&
                    state.batteryLevel == 80 && state.batteryHealth == 90,
                    "battery information is decoded and range checked");
        test.Expect((state.validFields & MeyerDeviceStateField_DeviceSecurityInfo) != 0U &&
                    state.expirationCodeHex[0] != '\0',
                    "device security information keeps the raw expiration code");
    }

    // 覆盖协议中相机、颜色矩阵、窗口位置、温度和帧率命令，并验证固定长度
    // 响应已经转换为公共结构，而不是只验证命令发送成功。
    void TestCameraAndCalibrationCommands(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        // 先从模拟设备读取相机参数，验证两个字节的大端坐标没有被反转。
        MeyerDeviceCmdCameraParameters cameraParameters;
        MeyerDeviceCmd_InitCameraParameters(&cameraParameters);
        test.Expect(MeyerDeviceCmd_ReadCameraParameters(handle, &cameraParameters) == MeyerDeviceCmdResult_Ok &&
                    cameraParameters.camera1WindowX == 248U && cameraParameters.camera1WindowY == 108U,
                    "camera parameter read decodes big-endian window coordinates");

        // 修改一个字段后固化，验证 0xA8 请求和 0xA9 状态回复均被处理。
        cameraParameters.camera1WindowX = 200U;
        test.Expect(MeyerDeviceCmd_StoreCameraParameters(handle, &cameraParameters) == MeyerDeviceCmdResult_Ok,
                    "camera parameter persistent command succeeds");

        // 在线设置窗口位置不会写 Flash，模拟后端会把新位置反映到下一次读取。
        MeyerDeviceCmdCameraWindowPosition windowPosition;
        MeyerDeviceCmd_InitCameraWindowPosition(&windowPosition);
        windowPosition.camera1X = 120U;
        windowPosition.camera1Y = 80U;
        windowPosition.camera2X = 120U;
        windowPosition.camera2Y = 80U;
        test.Expect(MeyerDeviceCmd_SetCameraWindowPosition(handle, &windowPosition) == MeyerDeviceCmdResult_Ok,
                    "camera window position command succeeds");
        test.Expect(MeyerDeviceCmd_ReadCameraParameters(handle, &cameraParameters) == MeyerDeviceCmdResult_Ok &&
                    cameraParameters.camera1WindowX == 120U && cameraParameters.camera1WindowY == 80U,
                    "online window position is reflected by the simulator");

        // 颜色矩阵是 416 字节大块数据，重点验证首尾字节和完整长度。
        MeyerDeviceCmdColorMatrix matrix;
        MeyerDeviceCmd_InitColorMatrix(&matrix);
        test.Expect(MeyerDeviceCmd_ReadColorMatrix(handle, &matrix) == MeyerDeviceCmdResult_Ok &&
                    matrix.data[0] == 0U && matrix.data[415] == 159U,
                    "color matrix read returns all 416 bytes");
        matrix.data[0] = 0xA5U;
        test.Expect(MeyerDeviceCmd_StoreColorMatrix(handle, &matrix) == MeyerDeviceCmdResult_Ok,
                    "color matrix persistent command succeeds");

        // 温度命令返回的是三个大端毫伏值，设备层不转换为摄氏度。
        MeyerDeviceCmdTemperature temperature;
        MeyerDeviceCmd_InitTemperature(&temperature);
        test.Expect(MeyerDeviceCmd_ReadTemperature(handle, &temperature) == MeyerDeviceCmdResult_Ok &&
                    temperature.lensMillivolts == 300U &&
                    temperature.boardMillivolts == 320U &&
                    temperature.scanHeadMillivolts == 340U,
                    "temperature response decodes three big-endian millivolt values");

        // 有效帧率转换为协议单字节值；非法帧率应在发送前被拒绝。
        test.Expect(MeyerDeviceCmd_SetFrameRate(handle, 20) == MeyerDeviceCmdResult_Ok,
                    "supported frame rate command succeeds");
        test.Expect(MeyerDeviceCmd_SetFrameRate(handle, 21) == MeyerDeviceCmdResult_InvalidArgument,
                    "unsupported frame rate is rejected before transport");
    }

    // 覆盖两路相机标定、颜色标定和设备信息固化命令，检查 382/72 字节数据不会
    // 在公共结构和协议 payload 转换过程中截断或错位。
    void TestCalibrationAndDeviceInfo(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        // 相机 1 和相机 2 使用不同命令码，但共享同一种 382 字节结构布局。
        MeyerDeviceCmdCameraCalibration camera1;
        MeyerDeviceCmd_InitCameraCalibration(&camera1);
        test.Expect(MeyerDeviceCmd_ReadCamera1Calibration(handle, &camera1) == MeyerDeviceCmdResult_Ok &&
                    camera1.cameraData[0] == 1U && camera1.padding == static_cast<unsigned char>(382U & 0xFFU),
                    "camera 1 calibration read returns the complete 382-byte payload");
        camera1.cameraData[0] = 0xA1U;
        test.Expect(MeyerDeviceCmd_StoreCamera1Calibration(handle, &camera1) == MeyerDeviceCmdResult_Ok,
                    "camera 1 calibration persistent command succeeds");

        MeyerDeviceCmdCameraCalibration camera2;
        MeyerDeviceCmd_InitCameraCalibration(&camera2);
        test.Expect(MeyerDeviceCmd_ReadCamera2Calibration(handle, &camera2) == MeyerDeviceCmdResult_Ok &&
                    camera2.cameraData[0] == 2U,
                    "camera 2 calibration read selects the second sensor payload");
        camera2.cameraData[0] = 0xA2U;
        test.Expect(MeyerDeviceCmd_StoreCamera2Calibration(handle, &camera2) == MeyerDeviceCmdResult_Ok,
                    "camera 2 calibration persistent command succeeds");

        // 颜色标定使用独立的 72 字节参数，不应与 416 字节颜色矩阵混淆。
        MeyerDeviceCmdColorCalibration colorCalibration;
        MeyerDeviceCmd_InitColorCalibration(&colorCalibration);
        test.Expect(MeyerDeviceCmd_ReadColorCalibration(handle, &colorCalibration) == MeyerDeviceCmdResult_Ok &&
                    colorCalibration.data[0] == 0x80U && colorCalibration.data[71] == 0xC7U,
                    "color calibration read returns all 72 bytes");
        colorCalibration.data[0] = 0xB0U;
        test.Expect(MeyerDeviceCmd_StoreColorCalibration(handle, &colorCalibration) == MeyerDeviceCmdResult_Ok,
                    "color calibration persistent command succeeds");

        // 设备信息包含授权原始字段，测试只验证保存/读取，不解释期限业务含义。
        MeyerDeviceCmdDeviceInfo deviceInfo;
        MeyerDeviceCmd_InitDeviceInfo(&deviceInfo);
        test.Expect(MeyerDeviceCmd_ReadDeviceInfo(handle, &deviceInfo) == MeyerDeviceCmdResult_Ok &&
                    deviceInfo.encrypted == 1U &&
                    std::string(deviceInfo.deviceIdUtf8) == "6200005301203",
                    "device info read decodes encryption and device number");
        deviceInfo.expirationCode[0] = 0x5AU;
        test.Expect(MeyerDeviceCmd_StoreDeviceInfo(handle, &deviceInfo) == MeyerDeviceCmdResult_Ok,
                    "device info persistent command succeeds");
    }

    // 覆盖曝光、固件擦除/分包烧写、控制器复位和机器码固化命令。
    void TestExposureFirmwareAndControl(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        // 读取曝光响应时协议多一个预留字节，公共结构只暴露前 16 个业务字段。
        MeyerDeviceCmdExposureParameters exposure;
        MeyerDeviceCmd_InitExposureParameters(&exposure);
        test.Expect(MeyerDeviceCmd_ReadExposureParameters(handle, &exposure) == MeyerDeviceCmdResult_Ok &&
                    exposure.camera1WhiteExposure == 0x11U,
                    "exposure read returns the 17-byte response fields");
        exposure.camera1WhiteExposure = 0x42U;
        test.Expect(MeyerDeviceCmd_SetExposureParameters(handle, &exposure) == MeyerDeviceCmdResult_Ok,
                    "exposure set command succeeds");
        test.Expect(MeyerDeviceCmd_ReadExposureParameters(handle, &exposure) == MeyerDeviceCmdResult_Ok &&
                    exposure.camera1WhiteExposure == 0x42U,
                    "exposure set value is visible in the simulator");

        // 固件测试只验证命令帧、进度字段和分包确认，不执行真实 Flash 升级。
        MeyerDeviceCmdFirmwareEraseProgress eraseProgress;
        MeyerDeviceCmd_InitFirmwareEraseProgress(&eraseProgress);
        test.Expect(MeyerDeviceCmd_EraseFirmware(handle, &eraseProgress, 100U) == MeyerDeviceCmdResult_Ok &&
                    eraseProgress.totalSectors == eraseProgress.erasedSectors,
                    "firmware erase command parses progress response");

        // 构造一个带包序和有效长度的 262 字节请求，验证设备回传信息匹配。
        MeyerDeviceCmdFirmwareWritePacket packet;
        MeyerDeviceCmd_InitFirmwareWritePacket(&packet);
        packet.totalPackets = 3U;
        packet.packetIndex = 1U;
        packet.actualDataSize = 4U;
        packet.data[0] = 0x10U;
        packet.data[1] = 0x20U;
        packet.data[2] = 0x30U;
        packet.data[3] = 0x40U;
        MeyerDeviceCmdFirmwareWriteProgress writeProgress;
        MeyerDeviceCmd_InitFirmwareWriteProgress(&writeProgress);
        const std::int32_t writeResult = MeyerDeviceCmd_WriteFirmwarePacket(handle,
                                                                              &packet,
                                                                              &writeProgress,
                                                                              100U);
        test.Expect(writeResult == MeyerDeviceCmdResult_Ok &&
                    writeProgress.packetIndex == packet.packetIndex &&
                    writeProgress.actualDataSize == packet.actualDataSize,
                    "firmware packet command checks the progress acknowledgement");
        if (writeResult != MeyerDeviceCmdResult_Ok)
        {
            PrintLastError(handle);
        }

        test.Expect(MeyerDeviceCmd_StoreMachineCode(handle, "6200005301203") == MeyerDeviceCmdResult_Ok,
                    "machine code persistent command succeeds");
        test.Expect(MeyerDeviceCmd_StoreMachineCode(handle, "invalid") == MeyerDeviceCmdResult_InvalidArgument,
                    "invalid machine code is rejected before transport");
        test.Expect(MeyerDeviceCmd_ResetController(handle) == MeyerDeviceCmdResult_Ok,
                    "controller reset command succeeds");
    }

    // 用小尺寸参数验证“启动底层流 -> 下发 0x0A -> 取一帧”的最小采集链路。
    void TestCapture(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        // 从型号目录取得默认采集参数，再缩小为测试所需的 128 字节帧。
        MeyerDeviceCmdCaptureParams params;
        test.Expect(MeyerDeviceCmd_InitCaptureParamsForModel(
                        MeyerDeviceModel_MyScan6Wireless, &params) == MeyerDeviceCmdResult_Ok,
                    "model catalog supplies default capture parameters");

        // 16*4=64 字节/平面，两个 32 字节包，最后一包仍为 32 字节。
        params.width = 16;
        params.height = 4;
        params.imageCount = 2;
        params.packetsPerImage = 2;
        params.transferSize = 32U;
        params.queueDepth = 1U;
        params.packetPayloadSize = 32;
        params.lastPacketValidSize = 32;
        params.maxReadyFrames = 1U;

        // StartCapture 内部先启动接收队列，再发送 0x0A，顺序错误会丢失首帧。
        const std::int32_t startResult = MeyerDeviceCmd_StartCapture(handle, &params);
        test.Expect(startResult == MeyerDeviceCmdResult_Ok, "capture command chain starts");
        if (startResult != MeyerDeviceCmdResult_Ok)
        {
            PrintLastError(handle);
            return;
        }

        test.Expect(MeyerDeviceCmd_RefreshBasicState(handle) == MeyerDeviceCmdResult_Busy,
                    "response-based state queries are blocked while image capture is active");

        // 调用方提供缓冲区，GetFrame 只复制完整帧并返回实际字节数。
        std::vector<unsigned char> frame(128U, 0U);
        MeyerDeviceCmdFrameInfo frameInfo;
        MeyerDeviceCmd_InitFrameInfo(&frameInfo);
        std::size_t frameBytes = 0U;
        const std::int32_t frameResult = MeyerDeviceCmd_GetFrame(
            handle, &frame[0], frame.size(), &frameBytes, &frameInfo);
        test.Expect(frameResult == MeyerDeviceCmdResult_Ok && frameBytes == 128U,
                    "one complete capture frame is delivered");
        test.Expect(frame[0] == 0U && frame[1] == 1U && frame[127] == 127U,
                    "capture frame bytes are copied to the caller buffer");

        const std::int32_t stopResult = MeyerDeviceCmd_StopCapture(handle, 1);
        test.Expect(stopResult == MeyerDeviceCmdResult_Ok, "capture command chain stops and turns light off");
        if (stopResult != MeyerDeviceCmdResult_Ok)
        {
            PrintLastError(handle);
        }
    }

    // 验证颜色校准入口的四个关键设备分支。每次调用都由 DeviceCmd 自己
    // 关闭旧会话并重建模拟会话，行为与 MainExe 的单会话宿主一致。
    void TestColorCalibrationPreflight(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        MeyerDeviceCmdOpenParams params;
        MeyerDeviceCmd_InitOpenParams(&params);
        params.backendType = MeyerDeviceCmdBackend_SimulatorForTest;
        params.modelHint = MeyerDeviceModel_MyScan6;
        std::strcpy(params.simulatedDeviceIdUtf8, "6200005301203");

        MeyerDeviceCalibrationPreflight preflight;
        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);

        params.simulatedFlags = MeyerDeviceCmdSimulatedFlag_DeviceNotConnected;
        std::int32_t result = MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_DeviceNotConnected,
                    "color calibration preflight reports disconnected device");

        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);
        params.simulatedFlags = MeyerDeviceCmdSimulatedFlag_Usb2Connected;
        result = MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Usb2Connected &&
                    preflight.state.isUsb2 == 1,
                    "color calibration preflight rejects USB 2.x connection");

        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);
        params.simulatedFlags = MeyerDeviceCmdSimulatedFlag_OmitModelMarker;
        result = MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_ModelUnknown,
                    "color calibration preflight keeps unreported model unknown");

        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);
        params.simulatedFlags = MeyerDeviceCmdSimulatedFlag_None;
        result = MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.state.model == MeyerDeviceModel_MyScan6 &&
                    preflight.state.modelSource == MeyerDeviceModelSource_DeviceReported &&
                    std::strcmp(preflight.deviceInfo.deviceIdUtf8, "6200005301203") == 0,
                    "color calibration preflight reads device information and reported model");
        test.Expect(MeyerDeviceCmd_IsOpen(handle) == 1,
                    "successful color calibration preflight keeps one device session open");
        MeyerDeviceCmd_Close(handle);
    }

    // 运行无硬件模拟全链路。
    int RunSmoke()
    {
        TestContext test;
        MeyerDeviceCmdHandle handle = MeyerDeviceCmd_Create();
        test.Expect(handle != nullptr, "device command context is created");
        if (handle == nullptr)
        {
            return 1;
        }

        MeyerDeviceCmdOpenParams params;
        test.Expect(MeyerDeviceCmd_InitOpenParams(&params) == MeyerDeviceCmdResult_Ok,
                    "open parameters initialize");
        params.backendType = MeyerDeviceCmdBackend_SimulatorForTest;
        params.modelHint = MeyerDeviceModel_MyScan6Wireless;
        std::strcpy(params.simulatedDeviceIdUtf8, "6200005301203");

        const std::int32_t openResult = MeyerDeviceCmd_Open(handle, &params);
        test.Expect(openResult == MeyerDeviceCmdResult_Ok, "simulated device session opens");
        if (openResult == MeyerDeviceCmdResult_Ok)
        {
            TestRawCommand(handle, test);
            TestStateRefresh(handle, test);
            test.Expect(MeyerDeviceCmd_SetLight(handle, 1) == MeyerDeviceCmdResult_Ok,
                        "light-on command succeeds");
            test.Expect(MeyerDeviceCmd_SetForceLight(handle, 1) == MeyerDeviceCmdResult_Ok,
                        "force-light command succeeds");
            TestCameraAndCalibrationCommands(handle, test);
            TestCalibrationAndDeviceInfo(handle, test);
            TestExposureFirmwareAndControl(handle, test);
            TestCapture(handle, test);
            TestColorCalibrationPreflight(handle, test);
            test.Expect(MeyerDeviceCmd_Close(handle) == MeyerDeviceCmdResult_Ok,
                        "device session closes safely");
        }
        else
        {
            PrintLastError(handle);
        }

        MeyerDeviceCmd_Destroy(handle);
        std::cout << "Failures: " << test.failures << "\n";
        return test.failures == 0 ? 0 : 1;
    }

    // 验证真实后端至少可以从显式路径加载并通过 ABI 门禁；没有连接设备时，
    // DeviceNotFound 是本机正常结果，不应被误判成动态加载失败。
    int RunRealTransportProbe(const char* transportPath)
    {
        if (transportPath == nullptr || transportPath[0] == '\0')
        {
            std::cerr << "Transport DLL path is required.\n";
            return 1;
        }

        MeyerDeviceCmdHandle handle = MeyerDeviceCmd_Create();
        if (handle == nullptr)
        {
            std::cerr << "Failed to create DeviceCmd context.\n";
            return 1;
        }

        MeyerDeviceCmdOpenParams params;
        MeyerDeviceCmd_InitOpenParams(&params);
        params.modelHint = MeyerDeviceModel_MyScan6Wireless;
        std::strncpy(params.transportLibraryPathUtf8,
                     transportPath,
                     sizeof(params.transportLibraryPathUtf8) - 1U);

        const std::int32_t result = MeyerDeviceCmd_Open(handle, &params);
        std::cout << "Real transport probe result: " << result << "\n";
        if (result != MeyerDeviceCmdResult_Ok && result != MeyerDeviceCmdResult_DeviceNotFound)
        {
            PrintLastError(handle);
            MeyerDeviceCmd_Destroy(handle);
            return 2;
        }

        std::cout << "Real transport DLL loaded and ABI gate passed.\n";
        MeyerDeviceCmd_Destroy(handle);
        return 0;
    }

    // 使用真实 DeviceTransport 执行颜色校准入口的只读预检。
    // 与 --probe-real 只验证 DLL/ABI 不同，本模式会继续发送 0xCD 请求并接收
    // 0xCE 设备信息，从而验证“连接 -> USB 速率 -> 设备信息 -> 型号解析”完整链路。
    // 本函数不会调用任何 Store/Set/Flash 接口，因此不会修改设备参数。
    int RunRealCalibrationPreflight(const char* transportPath)
    {
        if (transportPath == nullptr || transportPath[0] == '\0')
        {
            std::cerr << "Transport DLL path is required.\n";
            return 1;
        }

        // 测试宿主持有一个 DeviceCmd 句柄，作用等同于 MainExe 中的 DeviceSessionHost。
        MeyerDeviceCmdHandle handle = MeyerDeviceCmd_Create();
        if (handle == nullptr)
        {
            std::cerr << "Failed to create DeviceCmd context.\n";
            return 1;
        }

        // 必须先使用初始化函数写入结构版本和默认超时，不能依赖栈内存初始值。
        MeyerDeviceCmdOpenParams params;
        MeyerDeviceCmd_InitOpenParams(&params);
        // Unknown 表示不由宿主猜测机型，而是等待 0xCE 数据提供明确型号标记。
        params.modelHint = MeyerDeviceModel_Unknown;
        std::strncpy(params.transportLibraryPathUtf8,
                     transportPath,
                     sizeof(params.transportLibraryPathUtf8) - 1U);

        MeyerDeviceCalibrationPreflight preflight;
        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);
        const std::int32_t apiResult =
            MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);

        // isUsb2=0 既可能表示 USB3，也可能只是结构默认值；必须结合 validFields
        // 判断该字段是否由 Transport 真实填充，避免未连接时误报为 USB3。
        const char* usbDescription = "unknown";
        if ((preflight.state.validFields & MeyerDeviceStateField_UsbSpeed) != 0U)
        {
            usbDescription = preflight.state.isUsb2 != 0 ? "2.x" : "3.x";
        }

        // 按字段逐项输出，现场调试时可以区分“API 调用失败”和“业务门禁未通过”。
        std::cout << "Calibration preflight API result: " << apiResult << "\n"
                  << "Calibration preflight status: " << preflight.status << "\n"
                  << "Command result: " << preflight.commandResult << "\n"
                  << "Connection state: " << preflight.state.connectionState << "\n"
                  << "USB: " << usbDescription << "\n"
                  << "Device model: " << preflight.state.model << "\n"
                  << "Model source: " << preflight.state.modelSource << "\n"
                  << "Model name: " << preflight.state.modelNameUtf8 << "\n"
                  << "Device ID: " << preflight.deviceInfo.deviceIdUtf8 << "\n"
                  << "Detail: " << preflight.detailUtf8 << "\n";

        if (apiResult != MeyerDeviceCmdResult_Ok)
        {
            // API 层失败时补充句柄中的详细错误，便于定位 DLL、ABI 或传输层问题。
            PrintLastError(handle);
            MeyerDeviceCmd_Destroy(handle);
            return 2;
        }

        const bool ready = preflight.status == MeyerDeviceCalibrationPreflight_Ready;
        // Ready 分支会按设计保留连接；手工测试结束时必须显式关闭并销毁句柄。
        MeyerDeviceCmd_Close(handle);
        MeyerDeviceCmd_Destroy(handle);
        return ready ? 0 : 3;
    }

    // 输出测试宿主支持的模式，避免直接运行 exe 时只能看到空白窗口。
    void PrintUsage()
    {
        std::cout << "DeviceCmdTest usage:\n"
                  << "  DeviceCmdTest --smoke\n"
                  << "  DeviceCmdTest --probe-real <absolute-transport-dll-path>\n"
                  << "  DeviceCmdTest --preflight-real <absolute-transport-dll-path>\n"
                  << "  DeviceCmdTest --help\n";
    }
}

// 根据命令行选择无硬件 smoke、帮助信息或真实 Transport 动态加载探测。
int main(int argc, char* argv[])
{
    if (argc <= 1 || std::string(argv[1]) == "--smoke")
    {
        return RunSmoke();
    }
    if (std::string(argv[1]) == "--help")
    {
        PrintUsage();
        return 0;
    }
    if (std::string(argv[1]) == "--probe-real" && argc >= 3)
    {
        return RunRealTransportProbe(argv[2]);
    }
    if (std::string(argv[1]) == "--preflight-real" && argc >= 3)
    {
        return RunRealCalibrationPreflight(argv[2]);
    }
    PrintUsage();
    return 1;
}
