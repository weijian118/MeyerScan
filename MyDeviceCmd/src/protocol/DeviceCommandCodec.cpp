// =============================================================================
// 文件: DeviceCommandCodec.cpp
// 作用: 实现无线口扫协议 A 类命令帧的严格编码和防御性解析。
// =============================================================================
#include "DeviceCommandCodec.h"

#include "DeviceProtocolDefs.h"

#include <limits>

namespace meyer
{
    namespace devicecmd
    {
        namespace protocol
        {
            // 组装一条可直接交给 DeviceTransport 发送的 A 类命令。
            bool DeviceCommandCodec::Build(std::uint8_t commandCode,
                                           const std::uint8_t* payload,
                                           std::size_t payloadSize,
                                           std::size_t trailerZeroCount,
                                           std::vector<std::uint8_t>& frame,
                                           std::string& error)
            {
                // 先清空旧输出，保证失败时调用方不会误发上一次的残留帧。
                frame.clear();
                error.clear();

                // 协议长度字段只有 16 位，本模块再使用更小的业务上限保护内存。
                if (payloadSize > kMaximumPayloadBytes ||
                    payloadSize > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                {
                    error = "Command payload exceeds the supported protocol limit";
                    return false;
                }

                // 非空数据必须有真实缓冲区；空命令允许 payload 为 nullptr。
                if (payloadSize > 0U && payload == nullptr)
                {
                    error = "Command payload pointer is null";
                    return false;
                }

                // 协议明确响应尾可以是 0~3 字节，发送端也限制在相同范围内。
                if (trailerZeroCount > 3U)
                {
                    error = "Command trailer must contain zero to three bytes";
                    return false;
                }

                // 基础长度为头 2 + 命令 1 + 长度 2 + 数据 N + 校验 2。
                const std::size_t frameSize = 7U + payloadSize + trailerZeroCount;
                frame.assign(frameSize, 0U);

                // 逐字段写入而不是 memcpy 一个结构体，避免编译器对齐和大小端差异。
                frame[0] = kHeader0;
                frame[1] = kHeader1;
                frame[2] = commandCode;
                frame[3] = static_cast<std::uint8_t>((payloadSize >> 8U) & 0xFFU);
                frame[4] = static_cast<std::uint8_t>(payloadSize & 0xFFU);

                // 命令数据紧跟长度字段，从下标 5 开始复制。
                for (std::size_t index = 0U; index < payloadSize; ++index)
                {
                    frame[5U + index] = payload[index];
                }

                // 校验码按高 8 位、低 8 位顺序写入。
                const std::uint16_t checksum = CalculateChecksum(commandCode, payloadSize, payload);
                frame[5U + payloadSize] = static_cast<std::uint8_t>((checksum >> 8U) & 0xFFU);
                frame[6U + payloadSize] = static_cast<std::uint8_t>(checksum & 0xFFU);

                // frame 初始化时已经全部填零，末尾无需再次循环赋值。
                return true;
            }

            // 解析设备回复，拒绝短包、错误长度、错误校验和非零尾部字节。
            bool DeviceCommandCodec::Parse(const std::uint8_t* data,
                                           std::size_t size,
                                           CommandFrame& frame,
                                           std::string& error)
            {
                // 先重置输出，失败分支不会泄漏旧命令数据。
                frame.commandCode = 0U;
                frame.payload.clear();
                error.clear();

                // 最短帧包含 2 字节头、1 字节命令、2 字节长度和 2 字节校验。
                if (data == nullptr || size < 7U)
                {
                    error = "Command response is shorter than the protocol header";
                    return false;
                }

                // 数据头用于区分 A 类响应和 B 类图像包。
                if (data[0] != kHeader0 || data[1] != kHeader1)
                {
                    error = "Command response header is invalid";
                    return false;
                }

                // 长度字段是网络序/大端：先读高字节，再读低字节。
                const std::size_t payloadSize =
                    (static_cast<std::size_t>(data[3]) << 8U) |
                    static_cast<std::size_t>(data[4]);
                if (payloadSize > kMaximumPayloadBytes)
                {
                    error = "Command response payload exceeds the supported limit";
                    return false;
                }

                // requiredSize 不包含可选尾部补零；收到的数据至少要覆盖校验字段。
                const std::size_t requiredSize = 7U + payloadSize;
                if (size < requiredSize)
                {
                    error = "Command response is truncated";
                    return false;
                }

                // 按文档只接受 0~3 字节数据尾，避免把下一帧或垃圾数据误当作补零。
                const std::size_t trailerSize = size - requiredSize;
                if (trailerSize > 3U)
                {
                    error = "Command response trailer is longer than the protocol allows";
                    return false;
                }
                for (std::size_t index = requiredSize; index < size; ++index)
                {
                    if (data[index] != 0U)
                    {
                        error = "Command response trailer contains non-zero data";
                        return false;
                    }
                }

                // 重新计算累加和，与帧内高低字节组合出的值比较。
                const std::uint16_t expectedChecksum =
                    CalculateChecksum(data[2], payloadSize, payloadSize == 0U ? nullptr : data + 5U);
                const std::uint16_t actualChecksum =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[5U + payloadSize]) << 8U) |
                                               static_cast<std::uint16_t>(data[6U + payloadSize]));
                if (expectedChecksum != actualChecksum)
                {
                    error = "Command response checksum does not match";
                    return false;
                }

                // 所有校验通过后才写入输出对象，保证返回 true 的帧一定完整。
                frame.commandCode = data[2];
                frame.payload.assign(data + 5U, data + 5U + payloadSize);
                return true;
            }

            // 使用 32 位中间值累加，最后按协议截取低 16 位。
            std::uint16_t DeviceCommandCodec::CalculateChecksum(std::uint8_t commandCode,
                                                                std::size_t payloadSize,
                                                                const std::uint8_t* payload)
            {
                // 校验范围不包含 0x5A 0x33 数据头，包含命令码和两个长度字节。
                std::uint32_t sum = static_cast<std::uint32_t>(commandCode);
                sum += static_cast<std::uint32_t>((payloadSize >> 8U) & 0xFFU);
                sum += static_cast<std::uint32_t>(payloadSize & 0xFFU);

                // 逐字节相加与协议示例保持一致，不依赖 CPU 字节序。
                for (std::size_t index = 0U; index < payloadSize; ++index)
                {
                    sum += static_cast<std::uint32_t>(payload[index]);
                }

                return static_cast<std::uint16_t>(sum & 0xFFFFU);
            }
        }
    }
}
