// =============================================================================
// 文件: CaptureSlowProcessor.cpp
// 作用: 实现六图慢速后处理，不使用设备句柄和 Qt。
// =============================================================================
#include "CaptureSlowProcessor.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
    // 原始顺序 R/G/激光G/黑/B/激光B 调换为 黑/R/G/B/激光G/激光B。
    const int kOutputToSource[6] = { 3, 0, 1, 4, 2, 5 };

    // 白图的来源平面在调换后是 1(R)、2(G)、3(B)，黑图是 0。
    const int kWhiteOutputIndices[3] = { 1, 2, 3 };

    // 对一个平面做水平镜像，并在最后恢复前 40 字节头。
    void MirrorPlane(unsigned char* plane,
                     std::size_t planeBytes,
                     int width,
                     int height,
                     std::size_t headerBytes)
    {
        if (plane == nullptr || width <= 0 || height <= 0 ||
            planeBytes < static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        {
            return;
        }

        std::vector<unsigned char> header(headerBytes, 0U);
        if (headerBytes <= planeBytes)
        {
            std::memcpy(&header[0], plane, headerBytes);
        }

        // 逐行反转 x 坐标，不使用 QImage，保证纯 C++ 模块可移植。
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width / 2; ++x)
            {
                const std::size_t left =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x);
                const std::size_t right =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(width - x - 1);
                std::swap(plane[left], plane[right]);
            }
        }

        if (headerBytes > 0U && headerBytes <= planeBytes)
        {
            std::memcpy(plane, &header[0], headerBytes);
        }
    }
}

namespace meyer
{
    namespace captureprocessing
    {
        bool CaptureSlowProcessor::Process(const MeyerCaptureProfileConfig& profile,
                                           const unsigned char* decryptedGroup,
                                           std::size_t decryptedBytes,
                                           const MeyerCaptureGroupInfo& inputInfo,
                                           unsigned char* processedGroup,
                                           std::size_t capacity,
                                           std::size_t& requiredBytes,
                                           MeyerCaptureGroupInfo& outputInfo,
                                           std::string& error)
        {
            error.clear();
            requiredBytes = 0U;
            if (profile.structSize < sizeof(profile) ||
                profile.schemaVersion != MEYER_CAPTURE_TYPES_SCHEMA_VERSION ||
                profile.width <= 0 || profile.height <= 0 || profile.imageCount != 6 ||
                decryptedGroup == nullptr || processedGroup == nullptr)
            {
                error = "Slow processing profile or group pointer is invalid";
                return false;
            }

            const std::size_t planeBytes =
                static_cast<std::size_t>(profile.width) * static_cast<std::size_t>(profile.height);
            requiredBytes = planeBytes * 6U;
            if (decryptedBytes < requiredBytes || capacity < requiredBytes)
            {
                error = "Slow processing group buffer is too small";
                return false;
            }

            // 先按规定顺序复制，使后续算法只需固定访问 0..5。
            for (int outputIndex = 0; outputIndex < 6; ++outputIndex)
            {
                const int sourceIndex = kOutputToSource[outputIndex];
                std::memcpy(
                    processedGroup + static_cast<std::size_t>(outputIndex) * planeBytes,
                    decryptedGroup + static_cast<std::size_t>(sourceIndex) * planeBytes,
                    planeBytes);
            }

            // 相机 1 的 R/G/激光G 是原始平面 0/1/2，调换后对应输出 1/2/4。
            for (int index = 0; index < 3; ++index)
            {
                const int outputIndex = index == 0 ? 1 : (index == 1 ? 2 : 4);
                MirrorPlane(processedGroup + static_cast<std::size_t>(outputIndex) * planeBytes,
                             planeBytes,
                             profile.width,
                             profile.height,
                             profile.headerBytes);
            }

            // 减法先转 int，再限制到 [0,255]，不让 unsigned char 发生下溢回绕。
            const unsigned char* black = processedGroup;
            for (int whiteIndex = 0; whiteIndex < 3; ++whiteIndex)
            {
                const int outputIndex = kWhiteOutputIndices[whiteIndex];
                unsigned char* white = processedGroup +
                    static_cast<std::size_t>(outputIndex) * planeBytes;
                for (std::size_t pixel = profile.headerBytes; pixel < planeBytes; ++pixel)
                {
                    const int difference = static_cast<int>(white[pixel]) - static_cast<int>(black[pixel]);
                    white[pixel] = static_cast<unsigned char>((std::max)(0, (std::min)(255, difference)));
                }
            }

            outputInfo = inputInfo;
            outputInfo.slowProcessed = 1;
            outputInfo.groupBytes = requiredBytes;
            // 输出索引改为后处理顺序，同时保留 sourceImageIndex 方便诊断。
            for (int outputIndex = 0; outputIndex < 6; ++outputIndex)
            {
                outputInfo.planes[outputIndex].imageIndex = outputIndex;
                outputInfo.planes[outputIndex].sourceImageIndex = kOutputToSource[outputIndex];
            }
            return true;
        }

    }
}
