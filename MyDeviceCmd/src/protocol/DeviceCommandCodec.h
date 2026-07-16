// =============================================================================
// 文件: DeviceCommandCodec.h
// 作用: 声明 A 类命令帧编码和响应帧校验，不包含 USB 或业务状态逻辑。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace meyer
{
    namespace devicecmd
    {
        namespace protocol
        {
            // 解码后的帧只在 DLL 内部使用，因此可以安全使用 std::vector。
            struct CommandFrame
            {
                std::uint8_t commandCode;
                std::vector<std::uint8_t> payload;

                // 默认构造将命令码清零，避免错误分支读取未初始化字段。
                CommandFrame()
                    : commandCode(0U)
                {
                }
            };

            class DeviceCommandCodec
            {
            public:
                // 按协议生成完整 A 类帧；trailerZeroCount 只允许 0~3。
                static bool Build(std::uint8_t commandCode,
                                  const std::uint8_t* payload,
                                  std::size_t payloadSize,
                                  std::size_t trailerZeroCount,
                                  std::vector<std::uint8_t>& frame,
                                  std::string& error);

                // 校验数据头、长度、求和校验和尾部补零，再返回命令码与数据。
                static bool Parse(const std::uint8_t* data,
                                  std::size_t size,
                                  CommandFrame& frame,
                                  std::string& error);

            private:
                // 计算命令码、两字节长度和命令数据的 16 位无符号累加和。
                static std::uint16_t CalculateChecksum(std::uint8_t commandCode,
                                                       std::size_t payloadSize,
                                                       const std::uint8_t* payload);
            };
        }
    }
}
