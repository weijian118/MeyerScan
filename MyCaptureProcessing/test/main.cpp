// =============================================================================
// 文件: test/main.cpp
// 作用: 无硬件验证六图组帧、旧解密、状态汇总和协议级慢处理。
// =============================================================================
#include "CaptureProcessing.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (condition)
        {
            std::cout << "[PASS] " << message << "\n";
            return true;
        }
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    // 与旧设备相同的正向 S-Box 中只取测试需要的几个字节。
    unsigned char EncryptedValueForImage(int imageIndex)
    {
        const unsigned char values[6] = { 0x43U, 0xBCU, 0xE8U, 0x67U, 0x64U, 0x86U };
        return values[imageIndex];
    }

    void MakePacket(const MeyerCaptureProfileConfig& profile,
                    int imageIndex,
                    int packetIndex,
                    bool lightOn,
                    int scanHead,
                    std::vector<unsigned char>& packet)
    {
        packet.assign(profile.packetBytes, EncryptedValueForImage(imageIndex));
        if (packetIndex != 0)
        {
            return;
        }
        const unsigned char header[8] = { 0xA5U, 0xCCU, 0x00U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U };
        std::memcpy(&packet[0], header, sizeof(header));
        packet[12] = static_cast<unsigned char>(imageIndex);
        packet[13] = lightOn ? 0xFFU : 0x00U;
        packet[14] = 0xFFU;
        packet[15] = static_cast<unsigned char>(scanHead);
    }
}

int main(int argc, char* argv[])
{
    const bool smoke = argc > 1 && std::string(argv[1]) == "--smoke";
    if (!smoke)
    {
        std::cout << "CaptureProcessingTest supports only --smoke.\n";
        return 0;
    }

    MeyerCaptureProfileConfig profile;
    MeyerCaptureDeviceContext device;
    MeyerCaptureGroupInfo info;
    if (!Check(MeyerCaptureProcessing_InitProfile(&profile) == MeyerCaptureProcessingResult_Ok,
               "profile structure initializes") ||
        !Check(MeyerCaptureProcessing_InitDeviceContext(&device) == MeyerCaptureProcessingResult_Ok,
               "device context initializes") ||
        !Check(MeyerCaptureProcessing_InitGroupInfo(&info) == MeyerCaptureProcessingResult_Ok,
               "group info initializes"))
    {
        return 1;
    }

    if (!Check(MeyerCaptureProcessing_GetDefaultProfile(
                   MeyerCaptureDeviceProfile_MyScan5,
                   MeyerCaptureMode_ColorCalibration,
                   &profile) == MeyerCaptureProcessingResult_Ok,
               "MyScan 5 profile is available") ||
        !Check(profile.width == 1024 && profile.height == 455 && profile.imageCount == 6,
               "profile uses 1024x455 six-image parameters") ||
        !Check(profile.packetsPerImage == 29 && profile.lastPacketValidBytes == 7168U,
               "profile uses 29 packets and 7168 final valid bytes") ||
        !Check(profile.queueDepth == 64U, "profile explicitly uses queueDepth=64"))
    {
        return 2;
    }

    std::strncpy(device.deviceSeriesUtf8, "mOS MyScan 5", sizeof(device.deviceSeriesUtf8) - 1U);
    std::strncpy(device.deviceProfileUtf8, "MyScan5_25fps", sizeof(device.deviceProfileUtf8) - 1U);
    std::strncpy(device.deviceIdUtf8, "6200005301203", sizeof(device.deviceIdUtf8) - 1U);
    device.deviceProfile = MeyerCaptureDeviceProfile_MyScan5;
    device.deviceSeries = 5;
    device.deviceIdStatus = 1;
    device.captureMode = MeyerCaptureMode_ColorCalibration;

    MeyerCaptureProcessingHandle handle = MeyerCaptureProcessing_Create();
    if (!Check(handle != nullptr, "processing handle creates"))
    {
        return 3;
    }
    if (!Check(MeyerCaptureProcessing_Configure(handle, &profile, &device) == MeyerCaptureProcessingResult_Ok,
               "processing handle configures"))
    {
        MeyerCaptureProcessing_Destroy(handle);
        return 4;
    }

    std::vector<unsigned char> packet;
    for (int image = 0; image < profile.imageCount; ++image)
    {
        for (int packetIndex = 0; packetIndex < profile.packetsPerImage; ++packetIndex)
        {
            MakePacket(profile, image, packetIndex, true, MeyerCaptureScanHead_Large, packet);
            const std::int32_t result = MeyerCaptureProcessing_PushPacket(
                handle, &packet[0], packet.size());
            const bool last = image == profile.imageCount - 1 &&
                              packetIndex == profile.packetsPerImage - 1;
            if (last)
            {
                if (!Check(result == MeyerCaptureProcessingResult_GroupCompleted,
                           "six images complete one decrypted group"))
                {
                    MeyerCaptureProcessing_Destroy(handle);
                    return 5;
                }
            }
            else if (!Check(result >= MeyerCaptureProcessingResult_NeedMorePackets,
                            "packet advances the group state machine"))
            {
                MeyerCaptureProcessing_Destroy(handle);
                return 6;
            }
        }
    }

    const std::size_t groupBytes = static_cast<std::size_t>(profile.width) *
                                   static_cast<std::size_t>(profile.height) * 6U;
    std::vector<unsigned char> decrypted(groupBytes, 0U);
    std::size_t requiredBytes = 0U;
    if (!Check(MeyerCaptureProcessing_CopyCompletedGroup(
                   handle, &decrypted[0], decrypted.size(), &requiredBytes, &info) ==
                   MeyerCaptureProcessingResult_Ok,
               "decrypted group copies through the two-call buffer contract") ||
        !Check(requiredBytes == groupBytes && info.ledOn == 1 && info.longPressed == 1,
               "group state reports all-lights-on and long-press") ||
        !Check(info.scanHeadType == MeyerCaptureScanHead_Large,
               "group state reports large scan head"))
    {
        MeyerCaptureProcessing_Destroy(handle);
        return 7;
    }

    std::vector<unsigned char> processed(groupBytes, 0U);
    MeyerCaptureGroupInfo processedInfo;
    MeyerCaptureProcessing_InitGroupInfo(&processedInfo);
    if (!Check(MeyerCaptureProcessing_ProcessSlowGroup(
                   &profile, &decrypted[0], decrypted.size(), &info,
                   &processed[0], processed.size(), &requiredBytes, &processedInfo) ==
                   MeyerCaptureProcessingResult_Ok,
               "slow processing reorders and subtracts black image") ||
        !Check(processed[100] == 10U && processed[groupBytes / 6U + 100U] == 90U &&
                   processed[groupBytes / 6U * 2U + 100U] == 110U &&
                   processed[groupBytes / 6U * 3U + 100U] == 130U,
               "saturated black subtraction produces expected grayscale values"))
    {
        MeyerCaptureProcessing_Destroy(handle);
        return 8;
    }

    // 错序包和超时中断只应清空不完整组，不影响句柄重新起始。
    MakePacket(profile, 2, 0, true, MeyerCaptureScanHead_Large, packet);
    Check(MeyerCaptureProcessing_PushPacket(handle, &packet[0], packet.size()) ==
              MeyerCaptureProcessingResult_SyncReset,
          "a group that does not start at image zero is rejected and resynchronized");
    Check(MeyerCaptureProcessing_AbortIncompleteGroup(handle) == MeyerCaptureProcessingResult_Ok,
          "incomplete group can be aborted safely");

    MeyerCaptureProcessing_Destroy(handle);
    std::cout << "CaptureProcessingTest passed.\n";
    return 0;
}
