// =============================================================================
// 文件: DeviceProductCatalog.h
// 作用: 声明设备编号前缀、型号代码与产品身份之间的集中映射目录。
// =============================================================================
#pragma once

#include "../../include/DeviceCmd.h"

#include <cstdint>

namespace meyer
{
    namespace devicecmd
    {
        // 产品目录只保存稳定映射和纯识别规则，不访问 USB，也不依赖 UI 或 Qt。
        class DeviceProductCatalog
        {
        public:
            // 综合设备编号、8 位型号代码和调用方已有证据，生成完整 POD 识别结果。
            static void Identify(const char* deviceNumberUtf8,
                                 const char* modelCodeUtf8,
                                 std::uint64_t baseEvidence,
                                 MeyerDeviceProductIdentity& identity);

            // 根据已经确认的协议 Profile 补充系列级信息；该提示不能覆盖冲突结果，
            // 也不能把“系列已知”提升成“具体产品已知”。
            static void MergeProtocolProfileHint(std::int32_t protocolProfile,
                                                 MeyerDeviceProductIdentity& identity);

            // 通过完整 8 位型号代码查询协议能力 Profile；未知代码返回 Unknown。
            static std::int32_t ProtocolProfileForModelCode(const char* modelCodeUtf8);
        };
    }
}
