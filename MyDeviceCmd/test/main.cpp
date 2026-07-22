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

#if defined(_WIN32)
#include <windows.h>
#endif

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

    // 把 D4/D9 设备编号步骤的稳定整数状态转换为现场可读文本。
    // 返回文本同时说明“未收到回包、帧异常、求和校验触发生产模式”等差异。
    const char* DeviceNumberStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceNumberRead_NotRun:
            return "NotRun (0xD4/0xD9 was not executed)";
        case MeyerDeviceNumberRead_Valid:
            return "Valid (0xD9 received, parsed and checksum passed)";
        case MeyerDeviceNumberRead_ResponseMissing:
            return "ResponseMissing (0xD9 was not captured)";
        case MeyerDeviceNumberRead_FrameInvalid:
            return "FrameInvalid (0xD9 was captured but frame parsing failed)";
        case MeyerDeviceNumberRead_ChecksumIndicatesUnprogrammed:
            return "ChecksumIndicatesUnprogrammed (0xD9 checksum did not pass; production device)";
        case MeyerDeviceNumberRead_ValueInvalid:
            return "ValueInvalid (0xD9 frame passed but device number content is invalid)";
        case MeyerDeviceNumberRead_UninitializedLength:
            return "UninitializedLength (0xD9 payload length is 0xFFFF; production device)";
        default:
            return "Unknown";
        }
    }

    // 把 CD/CE 型号代码步骤状态转换为现场可读文本。
    const char* ModelCodeStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceModelCodeRead_NotRun:
            return "NotRun (0xCD/0xCE was not executed)";
        case MeyerDeviceModelCodeRead_Valid:
            return "Valid (0xCE received, parsed and checksum passed)";
        case MeyerDeviceModelCodeRead_FirmwareTooOld:
            return "FirmwareTooOld (0xCE was not captured)";
        case MeyerDeviceModelCodeRead_FrameInvalid:
            return "FrameInvalid (0xCE was captured but frame parsing failed)";
        case MeyerDeviceModelCodeRead_ChecksumInvalid:
            return "ChecksumInvalid (0xCE checksum did not pass)";
        case MeyerDeviceModelCodeRead_Uninitialized:
            return "Uninitialized (0xCE reports an uninitialized model code)";
        case MeyerDeviceModelCodeRead_ValueInvalid:
            return "ValueInvalid (0xCE frame passed but model code content is invalid)";
        default:
            return "Unknown";
        }
    }

    // 把生产设备 C2/C7 系列探测状态转换为现场可读文本。
    const char* SeriesProbeStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceSeriesProbe_NotRun:
            return "NotRun";
        case MeyerDeviceSeriesProbe_NotRequired:
            return "NotRequired (a valid device number was already reported)";
        case MeyerDeviceSeriesProbe_MyScan:
            return "MyScan candidate (0xC7 was not captured)";
        case MeyerDeviceSeriesProbe_MyScan5Or6:
            return "MyScan 5/6 candidate (0xC7 was captured)";
        case MeyerDeviceSeriesProbe_ResponseAbnormal:
            return "ResponseAbnormal (0xC7 was captured but could not be parsed normally)";
        default:
            return "Unknown";
        }
    }

    // 把主控板/投图板版本步骤状态转换为现场可读文本。
    const char* FirmwareVersionStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceFirmwareVersion_NotRun:
            return "NotRun";
        case MeyerDeviceFirmwareVersion_Valid:
            return "Valid (response received and version payload parsed)";
        case MeyerDeviceFirmwareVersion_NotRequired:
            return "NotRequired (this product family has no projection board)";
        case MeyerDeviceFirmwareVersion_ResponseMissing:
            return "ResponseMissing (version response was not captured)";
        case MeyerDeviceFirmwareVersion_FrameInvalid:
            return "FrameInvalid (version response could not be parsed)";
        case MeyerDeviceFirmwareVersion_PayloadInvalid:
            return "PayloadInvalid (version payload content is invalid)";
        default:
            return "Unknown";
        }
    }

    // 把颜色校准策略输出为现场可读文本，确认 MyScan 是否跳过 B9/BA。
    const char* ScanHeadColorPolicyText(std::int32_t policy)
    {
        switch (policy)
        {
        case MeyerDeviceScanHeadColorCalibrationPolicy_LargeOnlyShared:
            return "LargeOnlyShared";
        case MeyerDeviceScanHeadColorCalibrationPolicy_LargeAndSmall:
            return "LargeAndSmall";
        default:
            return "NotRun";
        }
    }

    // 把单个扫描头的状态输出为现场可读文本。校验和失败明确显示为
    // NotCalibrated，而不与超时/坏帧的读取失败混淆。
    const char* ScanHeadColorStatusText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceScanHeadColorCalibration_NotChecked:
            return "NotChecked";
        case MeyerDeviceScanHeadColorCalibration_Calibrated:
            return "Calibrated (checksum passed)";
        case MeyerDeviceScanHeadColorCalibration_NotCalibrated:
            return "NotCalibrated (checksum failed)";
        case MeyerDeviceScanHeadColorCalibration_NotRequired:
            return "NotRequired (shared parameters or no second head)";
        case MeyerDeviceScanHeadColorCalibration_ResponseMissing:
            return "ResponseMissing";
        case MeyerDeviceScanHeadColorCalibration_FrameInvalid:
            return "FrameInvalid";
        case MeyerDeviceScanHeadColorCalibration_PayloadInvalid:
            return "PayloadInvalid";
        default:
            return "Unknown";
        }
    }

    // 输出版本门禁的归一化结果，便于现场确认到底是版本不支持还是读取失败。
    const char* ScanHeadFirmwareCompatibilityText(std::int32_t status)
    {
        switch (status)
        {
        case MeyerDeviceColorCalibrationFirmware_Supported:
            return "Supported";
        case MeyerDeviceColorCalibrationFirmware_Unsupported:
            return "Unsupported";
        case MeyerDeviceColorCalibrationFirmware_ParseFailed:
            return "ParseFailed";
        case MeyerDeviceColorCalibrationFirmware_NotRequired:
            return "NotRequired";
        default:
            return "NotChecked";
        }
    }

    // 把 reported/effective 字段的来源转换为现场可读文本。
    const char* IdentityValueSourceText(std::int32_t source)
    {
        switch (source)
        {
        case MeyerDeviceIdentityValueSource_DeviceReported:
            return "DeviceReported";
        case MeyerDeviceIdentityValueSource_CompatibilityDefault:
            return "CompatibilityDefault";
        default:
            return "Unknown";
        }
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

    // 验证公开机器码语义接口同时返回 13 个原始数字和规范化 UTF-8 文本。
    void TestMachineCodeRead(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        MeyerDeviceCmdMachineCode machineCode;
        test.Expect(MeyerDeviceCmd_InitMachineCode(&machineCode) == MeyerDeviceCmdResult_Ok,
                    "machine-code output structure initializes");
        const std::int32_t result = MeyerDeviceCmd_ReadMachineCode(handle, &machineCode);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    std::strcmp(machineCode.machineCodeUtf8, "6200005301203") == 0,
                    "0xD4/0xD9 semantic API returns normalized machine code");
        test.Expect(machineCode.rawDigits[0] == 6U && machineCode.rawDigits[12] == 3U,
                    "machine-code semantic API preserves all raw digit bytes");
    }

    // 验证所有已知完整型号代码都精确映射，防止再次出现“看到首位 6 就当 MyScan 6”。
    void TestProductIdentificationCatalog(TestContext& test)
    {
        struct ProductCase
        {
            const char* deviceNumber;
            const char* modelCode;
            std::int32_t expectedProduct;
            std::int32_t expectedProfile;
        };

        const ProductCase cases[] =
        {
            { "6200002000001", "62000020", MeyerDeviceProductModel_MyScan_SY_KS1000_P1, MeyerDeviceModel_MyScan3 },
            { "6200002000002", "62010025", MeyerDeviceProductModel_MyScan_SY_KS1000_P2, MeyerDeviceModel_MyScan3 },
            { "6200002000003", "62010039", MeyerDeviceProductModel_MyScan_SY_KS1000_P3, MeyerDeviceModel_MyScan3 },
            { "6200002700001", "62000027", MeyerDeviceProductModel_MyScan_InternationalStandard, MeyerDeviceModel_MyScan3 },
            { "6200002700002", "62010036", MeyerDeviceProductModel_MyScan_InternationalPrivateLabel, MeyerDeviceModel_MyScan3 },
            { "6200005300001", "62000053", MeyerDeviceProductModel_MyScan5_DomesticStandard, MeyerDeviceModel_MyScan5 },
            { "6200005300002", "62010043", MeyerDeviceProductModel_MyScan5_InternationalStandard, MeyerDeviceModel_MyScan5 },
            { "6200005500001", "62000055", MeyerDeviceProductModel_MyScan5H_PublicHospital, MeyerDeviceModel_MyScan5H }
        };

        const std::size_t caseCount = sizeof(cases) / sizeof(cases[0]);
        for (std::size_t index = 0U; index < caseCount; ++index)
        {
            MeyerDeviceProductIdentity identity;
            MeyerDeviceCmd_InitProductIdentity(&identity);
            const std::int32_t result = MeyerDeviceCmd_IdentifyProduct(
                cases[index].deviceNumber,
                cases[index].modelCode,
                MeyerDeviceProductEvidence_CommandCapability,
                &identity);
            test.Expect(result == MeyerDeviceCmdResult_Ok &&
                        identity.identificationStatus ==
                            MeyerDeviceProductIdentification_ExactProduct &&
                        identity.productModel == cases[index].expectedProduct &&
                        identity.protocolProfile == cases[index].expectedProfile,
                        std::string("full model code maps exactly: ") + cases[index].modelCode);
        }

        // 同系列的 62000053 与 62000055 仍代表不同设备前缀，不能因 family 相同而放过。
        MeyerDeviceProductIdentity conflict;
        MeyerDeviceCmd_InitProductIdentity(&conflict);
        MeyerDeviceCmd_IdentifyProduct("6200005300001", "62000055", 0U, &conflict);
        test.Expect(conflict.identificationStatus == MeyerDeviceProductIdentification_Conflict,
                    "device prefix and full model-code conflict is rejected");

        // 生产阶段全零编号不阻止型号代码识别，但状态必须明确保留“未写号”。
        MeyerDeviceProductIdentity unprogrammed;
        MeyerDeviceCmd_InitProductIdentity(&unprogrammed);
        MeyerDeviceCmd_IdentifyProduct("0000000000000", "62010025", 0U, &unprogrammed);
        test.Expect(unprogrammed.identificationStatus ==
                        MeyerDeviceProductIdentification_DeviceNumberUnprogrammed &&
                    unprogrammed.productModel == MeyerDeviceProductModel_MyScan_SY_KS1000_P2 &&
                    unprogrammed.protocolProfile == MeyerDeviceModel_MyScan3,
                    "unprogrammed device number keeps exact model-code result");

        // 尚未登记的新型号代码可以保留系列候选，但不能伪造具体产品 ID。
        MeyerDeviceProductIdentity seriesOnly;
        MeyerDeviceCmd_InitProductIdentity(&seriesOnly);
        MeyerDeviceCmd_IdentifyProduct("6200005300001", "62999999", 0U, &seriesOnly);
        test.Expect(seriesOnly.identificationStatus ==
                        MeyerDeviceProductIdentification_SeriesOnly &&
                    seriesOnly.productFamily == MeyerDeviceProductFamily_MyScan5 &&
                    seriesOnly.productModel == MeyerDeviceProductModel_Unknown,
                    "known prefix and unknown model code returns series-only identity");
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
        test.Expect((state.validFields & MeyerDeviceStateField_FirmwareVersion) != 0U &&
                    std::string(state.firmwareVersionUtf8) == "1.3.1001" &&
                    (state.validFields & MeyerDeviceStateField_ProjectionBoardFirmwareVersion) == 0U &&
                    std::string(state.projectionBoardFirmwareVersionUtf8).empty(),
                    "non-MyScan device keeps main-board version and skips projection-board command");
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

    // 使用一组明确参数执行一次颜色校准预检。该辅助函数只减少测试重复代码，
    // 每种分支的期望状态仍在具体断言处完整写出，便于初学者对照流程图阅读。
    std::int32_t RunSimulatedPreflight(MeyerDeviceCmdHandle handle,
                                       MeyerDeviceCmdOpenParams& params,
                                       std::uint32_t simulatedFlags,
                                       const char* deviceNumber,
                                       std::int32_t modelHint,
                                       MeyerDeviceCalibrationPreflight& preflight)
    {
        // 每轮重新初始化输出结构，防止上一个场景的状态和值影响本轮结果。
        MeyerDeviceCmd_InitCalibrationPreflight(&preflight);
        // 模拟标志决定本轮命令在哪个步骤返回无回包、坏包或非法业务值。
        params.simulatedFlags = simulatedFlags;
        // modelHint 在模拟器中同时决定正常 0xCE 回包所携带的型号代码。
        params.modelHint = modelHint;
        // 固定数组先清零再复制，保证字符串始终以空字符结尾。
        std::memset(params.simulatedDeviceIdUtf8, 0, sizeof(params.simulatedDeviceIdUtf8));
        if (deviceNumber != nullptr)
        {
            std::strncpy(params.simulatedDeviceIdUtf8,
                         deviceNumber,
                         sizeof(params.simulatedDeviceIdUtf8) - 1U);
        }
        // Prepare 内部会先关闭旧会话再新建模拟会话，行为与 MainExe 单会话宿主一致。
        return MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
    }

    // 按设备型号读取流程逐项验证连接、D9、C7、CE、兼容默认值和证据冲突。
    void TestColorCalibrationPreflight(MeyerDeviceCmdHandle handle, TestContext& test)
    {
        MeyerDeviceCmdOpenParams params;
        MeyerDeviceCmd_InitOpenParams(&params);
        params.backendType = MeyerDeviceCmdBackend_SimulatorForTest;
        // 正式 MainExe 使用 Unknown 最小探测 profile；测试必须保持同一入口，
        // 防止已知型号的完整能力表掩盖探测 profile 缺少 D4/CD 权限的问题。
        params.modelHint = MeyerDeviceModel_Unknown;
        std::strcpy(params.simulatedDeviceIdUtf8, "6200005301203");

        MeyerDeviceCalibrationPreflight preflight;

        std::int32_t result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_DeviceNotConnected,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_DeviceNotConnected,
                    "color calibration preflight reports disconnected device");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_Usb2Connected,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Usb2Connected &&
                    preflight.state.isUsb2 == 1,
                    "color calibration preflight rejects USB 2.x connection");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_MachineCodeReadFailure,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal &&
                    preflight.commandResult == MeyerDeviceCmdResult_Timeout &&
                    preflight.detectionRecord.deviceNumberStatus ==
                        MeyerDeviceNumberRead_ResponseMissing,
                    "color calibration preflight stops when 0xD9 machine-code response is missing");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_DeviceNumberFrameInvalid,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_DeviceResponseAbnormal &&
                    preflight.commandResult == MeyerDeviceCmdResult_ProtocolError &&
                    preflight.detectionRecord.deviceNumberStatus ==
                        MeyerDeviceNumberRead_FrameInvalid,
                    "non-checksum 0xD9 frame errors remain response abnormalities");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_InvalidDeviceNumber,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_DeviceNumberInvalid &&
                    preflight.detectionRecord.deviceNumberStatus ==
                        MeyerDeviceNumberRead_ValueInvalid,
                    "a checksum-valid but illegal 0xD9 device number is rejected");

        // 新旧下位机表示“未写设备编号”的方式不同。本场景验证
        // 0xFFFF 长度能单独记录，同时仍继续 C7/CE 生产设备识别流程。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_DeviceNumberUninitialized |
                MeyerDeviceCmdSimulatedFlag_Camera1ProbeUnsupported |
                MeyerDeviceCmdSimulatedFlag_InvalidModelCode,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.deviceNumberStatus ==
                        MeyerDeviceNumberRead_UninitializedLength &&
                    preflight.detectionRecord.isProductionMode == 1 &&
                    preflight.detectionRecord.seriesProbeStatus ==
                        MeyerDeviceSeriesProbe_MyScan &&
                    preflight.detectionRecord.detectionStatus ==
                        MeyerDeviceDetection_ProductionInferred &&
                    std::strcmp(preflight.detectionRecord.reportedDeviceNumberUtf8, "") == 0 &&
                    std::strcmp(preflight.detectionRecord.effectiveDeviceNumberUtf8,
                                "6200002001200") == 0,
                    "0xD9 0xFFFF length records a distinct production-mode reason");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_ModelCodeReadFailure,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.modelCodeStatus ==
                        MeyerDeviceModelCodeRead_FirmwareTooOld &&
                    preflight.detectionRecord.modelCodeSource ==
                        MeyerDeviceIdentityValueSource_CompatibilityDefault &&
                    std::strcmp(preflight.detectionRecord.reportedModelCodeUtf8, "") == 0 &&
                    std::strcmp(preflight.detectionRecord.effectiveModelCodeUtf8,
                                "62000053") == 0 &&
                    preflight.state.modelSource == MeyerDeviceModelSource_AutoDetected,
                    "0xCE timeout keeps firmware diagnosis and a separate compatibility value");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_ModelCodeUninitialized,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.modelCodeStatus ==
                        MeyerDeviceModelCodeRead_Uninitialized &&
                    preflight.detectionRecord.usedCompatibilityDefaults == 1,
                    "0xCE 0xFFFF length records an uninitialized model code");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_ModelCodeChecksumFailure,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.modelCodeStatus ==
                        MeyerDeviceModelCodeRead_ChecksumInvalid &&
                    preflight.detectionRecord.usedCompatibilityDefaults == 1,
                    "0xCE checksum failure is recorded before compatibility fallback");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_ModelCodeFrameInvalid,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.modelCodeStatus ==
                        MeyerDeviceModelCodeRead_FrameInvalid &&
                    preflight.detectionRecord.usedCompatibilityDefaults == 1,
                    "non-checksum 0xCE frame errors are diagnosed before compatibility fallback");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_InvalidModelCode,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.modelCodeStatus ==
                        MeyerDeviceModelCodeRead_ValueInvalid &&
                    preflight.detectionRecord.usedCompatibilityDefaults == 1,
                    "a checksum-valid but illegal model code uses a labelled compatibility value");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_DeviceNumberChecksumFailure |
                MeyerDeviceCmdSimulatedFlag_Camera1ProbeUnsupported |
                MeyerDeviceCmdSimulatedFlag_InvalidModelCode,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.isProductionMode == 1 &&
                    preflight.detectionRecord.seriesProbeStatus ==
                        MeyerDeviceSeriesProbe_MyScan &&
                    preflight.detectionRecord.detectionStatus ==
                        MeyerDeviceDetection_ProductionInferred &&
                    std::strcmp(preflight.detectionRecord.effectiveDeviceNumberUtf8,
                                "6200002001200") == 0 &&
                    std::strcmp(preflight.detectionRecord.effectiveModelCodeUtf8,
                                "62000020") == 0,
                    "production mode uses the legacy MyScan defaults when 0xC7 is unsupported");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_DeviceNumberChecksumFailure,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.seriesProbeStatus ==
                        MeyerDeviceSeriesProbe_MyScan5Or6 &&
                    preflight.detectionRecord.detectionStatus ==
                        MeyerDeviceDetection_ProductionExactModel &&
                    preflight.detectionRecord.modelCodeSource ==
                        MeyerDeviceIdentityValueSource_DeviceReported &&
                    std::strcmp(preflight.detectionRecord.effectiveDeviceNumberUtf8,
                                "6200005301200") == 0,
                    "production mode combines a 0xC7 response with an exact 0xCE model code");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_DeviceNumberChecksumFailure |
                MeyerDeviceCmdSimulatedFlag_Camera1ProbeFrameInvalid,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.detectionRecord.seriesProbeStatus ==
                        MeyerDeviceSeriesProbe_MyScan5Or6 &&
                    std::strstr(preflight.detectionRecord.detailUtf8,
                                "0xC7 was received but its frame is abnormal") != nullptr,
                    "a malformed 0xC7 still records the MyScan 5/6 command capability candidate");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_DeviceNumberChecksumFailure |
                MeyerDeviceCmdSimulatedFlag_Camera1ProbeUnsupported,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status ==
                        MeyerDeviceCalibrationPreflight_ProductIdentityConflict &&
                    preflight.detectionRecord.detectionStatus ==
                        MeyerDeviceDetection_Conflict,
                    "production 0xC7 capability and exact 0xCE family conflicts are blocked");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_None,
            "6200005301203", MeyerDeviceModel_MyScan5H, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status ==
                        MeyerDeviceCalibrationPreflight_ProductIdentityConflict,
                    "preflight blocks conflicting device prefix and model code");

        result = RunSimulatedPreflight(
            handle, params, MeyerDeviceCmdSimulatedFlag_None,
            "6200005301203", MeyerDeviceModel_Unknown, preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.state.model == MeyerDeviceModel_MyScan5 &&
                    preflight.state.modelSource == MeyerDeviceModelSource_DeviceReported &&
                    std::strcmp(preflight.state.deviceIdUtf8, "6200005301203") == 0 &&
                    std::strcmp(preflight.deviceInfo.modelCodeUtf8, "62000053") == 0 &&
                    preflight.productIdentity.productModel ==
                        MeyerDeviceProductModel_MyScan5_DomesticStandard,
                    "preflight combines machine code and exact model code into product identity");
        test.Expect(preflight.detectionRecord.detectionStatus == MeyerDeviceDetection_Exact &&
                    preflight.detectionRecord.deviceNumberSource ==
                        MeyerDeviceIdentityValueSource_DeviceReported &&
                    preflight.detectionRecord.modelCodeSource ==
                        MeyerDeviceIdentityValueSource_DeviceReported &&
                    std::strcmp(preflight.detectionRecord.reportedDeviceNumberUtf8,
                                preflight.detectionRecord.effectiveDeviceNumberUtf8) == 0 &&
                    std::strcmp(preflight.detectionRecord.reportedModelCodeUtf8,
                                preflight.detectionRecord.effectiveModelCodeUtf8) == 0,
                    "exact detection keeps reported and effective identity values identical");
        test.Expect(preflight.firmwareVersions.mainBoardStatus == MeyerDeviceFirmwareVersion_Valid &&
                    preflight.firmwareVersions.projectionBoardStatus == MeyerDeviceFirmwareVersion_NotRequired &&
                    std::string(preflight.firmwareVersions.mainBoardVersionUtf8) == "1.3.1001" &&
                    std::string(preflight.firmwareVersions.projectionBoardVersionUtf8).empty(),
                    "MyScan5 preflight records main-board version and marks projection board not required");
        test.Expect(preflight.scanHeadColorCalibration.policy ==
                        MeyerDeviceScanHeadColorCalibrationPolicy_LargeAndSmall &&
                    preflight.scanHeadColorCalibration.firmwareCompatibility ==
                        MeyerDeviceColorCalibrationFirmware_Supported &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_Calibrated &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_Calibrated,
                    "MyScan5 preflight records both calibrated scan heads");
        test.Expect(MeyerDeviceCmd_IsOpen(handle) == 1,
                    "successful color calibration preflight keeps one device session open");
        MeyerDeviceCmd_Close(handle);

        // mOS MyScan 使用另一组协议 Profile，必须在识别完成后追加 0x12/0x13。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_None,
            "6200002002566",
            MeyerDeviceModel_MyScan3,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.firmwareVersions.mainBoardStatus == MeyerDeviceFirmwareVersion_Valid &&
                    preflight.firmwareVersions.projectionBoardStatus == MeyerDeviceFirmwareVersion_Valid &&
                    std::string(preflight.firmwareVersions.mainBoardVersionUtf8) == "1.3.1001" &&
                    std::string(preflight.firmwareVersions.projectionBoardVersionUtf8) == "2.3.300",
                    "mOS MyScan preflight reads and records both board firmware versions");
        test.Expect(preflight.scanHeadColorCalibration.policy ==
                        MeyerDeviceScanHeadColorCalibrationPolicy_LargeOnlyShared &&
                    preflight.scanHeadColorCalibration.firmwareCompatibility ==
                        MeyerDeviceColorCalibrationFirmware_NotRequired &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotChecked &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotRequired,
                    "mOS MyScan records one shared large-head calibration policy without B9");
        MeyerDeviceCmd_Close(handle);

        // MyScan 5/6 的 1.1.x 和 1.2.x 主控板不支持小扫描头颜色校准。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_UnsupportedColorCalibrationFirmware,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status ==
                        MeyerDeviceCalibrationPreflight_ColorCalibrationFirmwareUnsupported &&
                    preflight.scanHeadColorCalibration.firmwareCompatibility ==
                        MeyerDeviceColorCalibrationFirmware_Unsupported &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotChecked &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotChecked,
                    "MyScan5 firmware 1.2 blocks color calibration before scan-head queries");

        // A4/BA 求和失败分别表示对应扫描头未写入颜色参数，预检仍然 Ready。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_SmallHeadColorChecksumFailure,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_Calibrated &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotCalibrated,
                    "BA checksum failure means only the small scan head is uncalibrated");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_LargeHeadColorChecksumFailure,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotCalibrated &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_Calibrated,
                    "A4 checksum failure means only the large scan head is uncalibrated");

        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_LargeHeadColorChecksumFailure |
                MeyerDeviceCmdSimulatedFlag_SmallHeadColorChecksumFailure,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_Ready &&
                    preflight.scanHeadColorCalibration.largeHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotCalibrated &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_NotCalibrated,
                    "A4 and BA checksum failures record both scan heads as uncalibrated");

        // 没有 BA 回包属于通信失败，不能伪装成“小扫描头未校准”继续进入 UI。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_SmallHeadColorReadFailure,
            "6200005301203",
            MeyerDeviceModel_Unknown,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status ==
                        MeyerDeviceCalibrationPreflight_ScanHeadColorCalibrationReadFailed &&
                    preflight.scanHeadColorCalibration.smallHeadStatus ==
                        MeyerDeviceScanHeadColorCalibration_ResponseMissing,
                    "missing BA response blocks calibration instead of reporting uncalibrated");

        // 主控板版本对所有系列都是必需项；超时必须保留身份结果并阻止进入校准。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_MainBoardVersionReadFailure,
            "6200005301203",
            MeyerDeviceModel_MyScan5,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_FirmwareVersionReadFailed &&
                    preflight.firmwareVersions.mainBoardStatus ==
                        MeyerDeviceFirmwareVersion_ResponseMissing,
                    "main-board firmware timeout blocks calibration with a recorded version status");

        // mOS MyScan 的主控板成功但投图板失败时，要保留主控板版本并准确标记失败板卡。
        result = RunSimulatedPreflight(
            handle,
            params,
            MeyerDeviceCmdSimulatedFlag_ProjectionBoardVersionReadFailure,
            "6200002002566",
            MeyerDeviceModel_MyScan3,
            preflight);
        test.Expect(result == MeyerDeviceCmdResult_Ok &&
                    preflight.status == MeyerDeviceCalibrationPreflight_FirmwareVersionReadFailed &&
                    preflight.firmwareVersions.mainBoardStatus == MeyerDeviceFirmwareVersion_Valid &&
                    preflight.firmwareVersions.projectionBoardStatus ==
                        MeyerDeviceFirmwareVersion_ResponseMissing,
                    "mOS MyScan projection-board timeout preserves the valid main-board version");
    }

    // 运行无硬件模拟全链路。
    int RunSmoke()
    {
        TestContext test;

        // 三种版本各有不同用途：字符串 API 描述语义能力，整数 API 保护公共
        // POD/函数表兼容性，模块版本用于运行时版本清单。测试同时锁住三者，
        // 可及时发现只修改 Version.rc 或只修改代码常量造成的版本来源不一致。
        test.Expect(std::strcmp(MeyerDeviceCmd_GetApiVersion(), "2.4.0") == 0,
                    "semantic API version matches the current command behavior");
        test.Expect(GetMeyerModuleApiVersion() == MEYER_DEVICE_CMD_API_VERSION,
                    "integer ABI version matches the public header contract");
        test.Expect(std::strstr(GetMeyerModuleVersion(), "v0.8.0") != nullptr,
                    "module code version matches the current release");

        MeyerDeviceCmdHandle handle = MeyerDeviceCmd_Create();
        test.Expect(handle != nullptr, "device command context is created");
        if (handle == nullptr)
        {
            return 1;
        }

        TestProductIdentificationCatalog(test);

        MeyerDeviceCmdOpenParams params;
        test.Expect(MeyerDeviceCmd_InitOpenParams(&params) == MeyerDeviceCmdResult_Ok,
                    "open parameters initialize");
        test.Expect(params.commandTimeoutMs == 200U,
                    "open parameters use the 200 ms command timeout");
        params.backendType = MeyerDeviceCmdBackend_SimulatorForTest;
        params.modelHint = MeyerDeviceModel_MyScan6Wireless;
        std::strcpy(params.simulatedDeviceIdUtf8, "6200005301203");

        const std::int32_t openResult = MeyerDeviceCmd_Open(handle, &params);
        test.Expect(openResult == MeyerDeviceCmdResult_Ok, "simulated device session opens");
        if (openResult == MeyerDeviceCmdResult_Ok)
        {
            TestRawCommand(handle, test);
            TestMachineCodeRead(handle, test);
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
    // 与 --probe-real 只验证 DLL/ABI 不同，本模式会先发送 0xD4/接收 0xD9，
    // 再发送 0xCD/接收 0xCE，并按识别出的机型读取主控板/投图板版本，验证
    // 连接、USB、设备编号、型号、产品身份和下位机版本的完整只读链路。
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
#if defined(_WIN32)
        // 只在测试宿主中打开 DeviceCmd 内部预检时序输出，正式程序不会受控制台输出影响。
        ::SetEnvironmentVariableA("MEYERSCAN_DEVICE_CMD_TIMING", "1");
#endif
        const std::int32_t apiResult =
            MeyerDeviceCmd_PrepareColorCalibration(handle, &params, &preflight);
#if defined(_WIN32)
        ::SetEnvironmentVariableA("MEYERSCAN_DEVICE_CMD_TIMING", nullptr);
#endif

        // isUsb2=0 既可能表示 USB3，也可能只是结构默认值；必须结合 validFields
        // 判断该字段是否由 Transport 真实填充，避免未连接时误报为 USB3。
        const char* usbDescription = "unknown";
        if ((preflight.state.validFields & MeyerDeviceStateField_UsbSpeed) != 0U)
        {
            usbDescription = preflight.state.isUsb2 != 0 ? "2.x" : "3.x";
        }

        // 按字段逐项输出，现场调试时可以区分 API、连接、命令回包、解析、
        // reported/effective 身份、生产模式和版本读取的不同失败层次。
        std::cout << "Calibration preflight API result: " << apiResult << "\n"
                  << "Calibration preflight status: " << preflight.status << "\n"
                  << "Command result: " << preflight.commandResult << "\n"
                  << "Connection state: " << preflight.state.connectionState << "\n"
                  << "USB: " << usbDescription << "\n"
                  << "Device model: " << preflight.state.model << "\n"
                  << "Model source: " << preflight.state.modelSource << "\n"
                  << "Model name: " << preflight.state.modelNameUtf8 << "\n"
                  << "Machine code: " << preflight.state.deviceIdUtf8 << "\n"
                  << "Model code: " << preflight.deviceInfo.modelCodeUtf8 << "\n"
                  << "Detection status: " << preflight.detectionRecord.detectionStatus << "\n"
                  << "Production mode: " << preflight.detectionRecord.isProductionMode << "\n"
                  << "Compatibility defaults used: "
                  << preflight.detectionRecord.usedCompatibilityDefaults << "\n"
                  << "D4/D9 device number status: "
                  << preflight.detectionRecord.deviceNumberStatus << " - "
                  << DeviceNumberStatusText(preflight.detectionRecord.deviceNumberStatus) << "\n"
                  << "Reported device number: "
                  << preflight.detectionRecord.reportedDeviceNumberUtf8 << "\n"
                  << "Effective device number: "
                  << preflight.detectionRecord.effectiveDeviceNumberUtf8 << "\n"
                  << "Device number source: "
                  << IdentityValueSourceText(preflight.detectionRecord.deviceNumberSource) << "\n"
                  << "C2/C7 series probe status: "
                  << preflight.detectionRecord.seriesProbeStatus << " - "
                  << SeriesProbeStatusText(preflight.detectionRecord.seriesProbeStatus) << "\n"
                  << "CD/CE model code status: "
                  << preflight.detectionRecord.modelCodeStatus << " - "
                  << ModelCodeStatusText(preflight.detectionRecord.modelCodeStatus) << "\n"
                  << "Reported model code: "
                  << preflight.detectionRecord.reportedModelCodeUtf8 << "\n"
                  << "Effective model code: "
                  << preflight.detectionRecord.effectiveModelCodeUtf8 << "\n"
                  << "Model code source: "
                  << IdentityValueSourceText(preflight.detectionRecord.modelCodeSource) << "\n"
                  << "Product family code: " << preflight.productIdentity.productFamily << "\n"
                  << "Product family: " << preflight.productIdentity.seriesNameUtf8 << "\n"
                  << "Product model code: " << preflight.productIdentity.productModel << "\n"
                  << "Product model: " << preflight.productIdentity.productNameUtf8 << "\n"
                  << "Protocol profile: " << preflight.productIdentity.protocolProfile << "\n"
                  << "Product identification status: "
                  << preflight.productIdentity.identificationStatus << "\n"
                  << "0x14/0x15 main-board version status: "
                  << preflight.firmwareVersions.mainBoardStatus << " - "
                  << FirmwareVersionStatusText(preflight.firmwareVersions.mainBoardStatus) << "\n"
                  << "Main-board firmware version: "
                  << preflight.firmwareVersions.mainBoardVersionUtf8 << "\n"
                  << "0x12/0x13 projection-board version status: "
                  << preflight.firmwareVersions.projectionBoardStatus << " - "
                  << FirmwareVersionStatusText(preflight.firmwareVersions.projectionBoardStatus) << "\n"
                  << "Projection-board firmware version: "
                  << preflight.firmwareVersions.projectionBoardVersionUtf8 << "\n"
                  << "Scan-head color calibration policy: "
                  << ScanHeadColorPolicyText(
                         preflight.scanHeadColorCalibration.policy) << "\n"
                  << "Scan-head firmware compatibility: "
                  << ScanHeadFirmwareCompatibilityText(
                         preflight.scanHeadColorCalibration.firmwareCompatibility) << "\n"
                  << "Large scan-head color status: "
                  << ScanHeadColorStatusText(
                         preflight.scanHeadColorCalibration.largeHeadStatus)
                  << " (commandResult="
                  << preflight.scanHeadColorCalibration.largeHeadCommandResult << ")\n"
                  << "Small scan-head color status: "
                  << ScanHeadColorStatusText(
                         preflight.scanHeadColorCalibration.smallHeadStatus)
                  << " (commandResult="
                  << preflight.scanHeadColorCalibration.smallHeadCommandResult << ")\n"
                  << "Scan-head color detail: "
                  << preflight.scanHeadColorCalibration.detailUtf8 << "\n"
                  << "Detection detail: " << preflight.detectionRecord.detailUtf8 << "\n"
                  << "Firmware detail: " << preflight.firmwareVersions.detailUtf8 << "\n"
                  << "Preflight detail: " << preflight.detailUtf8 << "\n";

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
