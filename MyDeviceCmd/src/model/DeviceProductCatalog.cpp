// =============================================================================
// 文件: DeviceProductCatalog.cpp
// 作用: 集中实现产品系列、具体产品型号、设备编号前缀和型号代码映射。
// =============================================================================
#include "DeviceProductCatalog.h"

#include <cctype>
#include <cstring>
#include <string>

namespace
{
    // 具体产品记录使用普通常量数组，VS2015 可直接编译，也不会引入动态分配。
    struct ProductRecord
    {
        std::int32_t productModel;
        std::int32_t productFamily;
        std::int32_t protocolProfile;
        const char* deviceNumberPrefix;
        const char* modelCode;
        const char* seriesName;
        const char* productName;
    };

    // 设备编号前缀只能确定系列候选和协议能力 Profile，不能单独确定 P1/P2/P3。
    struct PrefixRecord
    {
        const char* prefix;
        std::int32_t productFamily;
        std::int32_t protocolProfile;
        const char* seriesName;
    };

    const ProductRecord kProducts[] =
    {
        { MeyerDeviceProductModel_MyScan_SY_KS1000_P1,
          MeyerDeviceProductFamily_MyScan,
          MeyerDeviceModel_MyScan3,
          "62000020", "62000020", "mOS MyScan", "mOS MyScan/SY-KS1000(P1)" },
        { MeyerDeviceProductModel_MyScan_SY_KS1000_P2,
          MeyerDeviceProductFamily_MyScan,
          MeyerDeviceModel_MyScan3,
          "62000020", "62010025", "mOS MyScan", "mOS MyScan/SY-KS1000(P2)" },
        { MeyerDeviceProductModel_MyScan_SY_KS1000_P3,
          MeyerDeviceProductFamily_MyScan,
          MeyerDeviceModel_MyScan3,
          "62000020", "62010039", "mOS MyScan", "mOS MyScan/SY-KS1000(P3)" },
        { MeyerDeviceProductModel_MyScan_InternationalStandard,
          MeyerDeviceProductFamily_MyScan,
          MeyerDeviceModel_MyScan3,
          "62000027", "62000027", "mOS MyScan", "mOS MyScan/mOS MyScan" },
        { MeyerDeviceProductModel_MyScan_InternationalPrivateLabel,
          MeyerDeviceProductFamily_MyScan,
          MeyerDeviceModel_MyScan3,
          "62000027", "62010036", "mOS MyScan", "mOS MyScan/mOS MyScan-P1" },
        { MeyerDeviceProductModel_MyScan5_DomesticStandard,
          MeyerDeviceProductFamily_MyScan5,
          MeyerDeviceModel_MyScan5,
          "62000053", "62000053", "mOS MyScan 5", "mOS MyScan 5/mOS MyScan 5" },
        { MeyerDeviceProductModel_MyScan5_InternationalStandard,
          MeyerDeviceProductFamily_MyScan5,
          MeyerDeviceModel_MyScan5,
          "62000053", "62010043", "mOS MyScan 5", "mOS MyScan 5/mOS MyScan 5-P1" },
        { MeyerDeviceProductModel_MyScan5H_PublicHospital,
          MeyerDeviceProductFamily_MyScan5,
          MeyerDeviceModel_MyScan5H,
          "62000055", "62000055", "mOS MyScan 5", "mOS MyScan 5H/mOS MyScan 5H" }
    };

    const PrefixRecord kPrefixes[] =
    {
        { "62000020", MeyerDeviceProductFamily_MyScan, MeyerDeviceModel_MyScan3, "mOS MyScan" },
        { "62000027", MeyerDeviceProductFamily_MyScan, MeyerDeviceModel_MyScan3, "mOS MyScan" },
        { "62000053", MeyerDeviceProductFamily_MyScan5, MeyerDeviceModel_MyScan5, "mOS MyScan 5" },
        { "62000055", MeyerDeviceProductFamily_MyScan5, MeyerDeviceModel_MyScan5H, "mOS MyScan 5" }
    };

    // 固定数组文本必须始终以 '\0' 结束，避免跨 DLL 输出未终止字符串。
    template<std::size_t Capacity>
    void CopyText(char (&destination)[Capacity], const char* source)
    {
        std::memset(destination, 0, Capacity);
        if (source != nullptr && Capacity > 0U)
        {
            std::strncpy(destination, source, Capacity - 1U);
        }
    }

    // 检查字符串是否恰好由指定数量的十进制数字组成。
    bool IsFixedDecimal(const char* text, std::size_t expectedLength)
    {
        if (text == nullptr || std::strlen(text) != expectedLength)
        {
            return false;
        }
        for (std::size_t index = 0U; index < expectedLength; ++index)
        {
            if (text[index] < '0' || text[index] > '9')
            {
                return false;
            }
        }
        return true;
    }

    // 全零设备编号表示生产阶段尚未写号，不能把它当成一个真实前缀。
    bool IsAllZero(const char* text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return true;
        }
        for (std::size_t index = 0U; text[index] != '\0'; ++index)
        {
            if (text[index] != '0')
            {
                return false;
            }
        }
        return true;
    }

    // 通过完整型号代码精确查找产品；禁止使用首位或模糊前缀猜测。
    const ProductRecord* FindProductByModelCode(const char* modelCode)
    {
        if (!IsFixedDecimal(modelCode, 8U))
        {
            return nullptr;
        }
        const std::size_t count = sizeof(kProducts) / sizeof(kProducts[0]);
        for (std::size_t index = 0U; index < count; ++index)
        {
            if (std::strcmp(kProducts[index].modelCode, modelCode) == 0)
            {
                return &kProducts[index];
            }
        }
        return nullptr;
    }

    // 设备编号前 8 位只查询系列候选；多个具体产品可以共享同一条记录。
    const PrefixRecord* FindPrefix(const char* prefix)
    {
        if (!IsFixedDecimal(prefix, 8U))
        {
            return nullptr;
        }
        const std::size_t count = sizeof(kPrefixes) / sizeof(kPrefixes[0]);
        for (std::size_t index = 0U; index < count; ++index)
        {
            if (std::strcmp(kPrefixes[index].prefix, prefix) == 0)
            {
                return &kPrefixes[index];
            }
        }
        return nullptr;
    }

    // 根据协议能力 Profile 返回所属系列；MyScan 5H 仍属于 mOS MyScan 5 系列。
    void GetFamilyForProfile(std::int32_t profile, std::int32_t& family, const char*& seriesName)
    {
        family = MeyerDeviceProductFamily_Unknown;
        seriesName = "Unknown";
        if (profile == MeyerDeviceModel_MyScan3)
        {
            family = MeyerDeviceProductFamily_MyScan;
            seriesName = "mOS MyScan";
        }
        else if (profile == MeyerDeviceModel_MyScan5 || profile == MeyerDeviceModel_MyScan5H)
        {
            family = MeyerDeviceProductFamily_MyScan5;
            seriesName = "mOS MyScan 5";
        }
        else if (profile == MeyerDeviceModel_MyScan6 ||
                 profile == MeyerDeviceModel_MyScan6Wireless)
        {
            family = MeyerDeviceProductFamily_MyScan6;
            seriesName = "mOS MyScan 6";
        }
    }
}

namespace meyer
{
    namespace devicecmd
    {
        // 综合两条命令的结果，严格区分系列候选、精确产品、未写号和冲突。
        void DeviceProductCatalog::Identify(const char* deviceNumberUtf8,
                                            const char* modelCodeUtf8,
                                            std::uint64_t baseEvidence,
                                            MeyerDeviceProductIdentity& identity)
        {
            // 调用方可能复用旧结构，先保留版本头并清空其余字段。
            std::memset(&identity, 0, sizeof(identity));
            identity.structSize = sizeof(identity);
            identity.schemaVersion = MEYER_DEVICE_CMD_SCHEMA_VERSION;
            identity.productFamily = MeyerDeviceProductFamily_Unknown;
            identity.productModel = MeyerDeviceProductModel_Unknown;
            identity.identificationStatus = MeyerDeviceProductIdentification_Unknown;
            identity.protocolProfile = MeyerDeviceModel_Unknown;
            identity.evidence = baseEvidence;
            CopyText(identity.seriesNameUtf8, "Unknown");
            CopyText(identity.productNameUtf8, "Unknown");

            // 设备编号必须是 13 位且非全零才具有可用于识别的前缀。
            const bool numberProgrammed =
                IsFixedDecimal(deviceNumberUtf8, MEYER_DEVICE_CMD_MACHINE_CODE_BYTES) &&
                !IsAllZero(deviceNumberUtf8);
            const PrefixRecord* prefixRecord = nullptr;
            if (numberProgrammed)
            {
                char prefix[9] = {};
                std::memcpy(prefix, deviceNumberUtf8, 8U);
                CopyText(identity.deviceNumberPrefixUtf8, prefix);
                prefixRecord = FindPrefix(prefix);
                if (prefixRecord != nullptr)
                {
                    identity.evidence |= MeyerDeviceProductEvidence_DeviceNumberPrefix;
                }
            }

            // 型号代码必须完整匹配目录；即便尚未收录，也保留合法原值供日志分析。
            const bool modelCodeValid = IsFixedDecimal(modelCodeUtf8, 8U);
            const ProductRecord* productRecord = nullptr;
            if (modelCodeValid)
            {
                CopyText(identity.modelCodeUtf8, modelCodeUtf8);
                identity.evidence |= MeyerDeviceProductEvidence_ModelCode;
                productRecord = FindProductByModelCode(modelCodeUtf8);
            }

            // 两项已知证据指向不同前缀或不同系列时，不能任意选择一个继续。
            if (prefixRecord != nullptr && productRecord != nullptr &&
                (prefixRecord->productFamily != productRecord->productFamily ||
                 std::strcmp(identity.deviceNumberPrefixUtf8,
                             productRecord->deviceNumberPrefix) != 0))
            {
                identity.identificationStatus = MeyerDeviceProductIdentification_Conflict;
                CopyText(identity.detailUtf8,
                         "Device number prefix conflicts with the reported model code");
                return;
            }

            // 型号代码是确定具体产品的唯一当前证据，同时给出相应协议 Profile。
            if (productRecord != nullptr)
            {
                identity.productFamily = productRecord->productFamily;
                identity.productModel = productRecord->productModel;
                identity.protocolProfile = productRecord->protocolProfile;
                CopyText(identity.seriesNameUtf8, productRecord->seriesName);
                CopyText(identity.productNameUtf8, productRecord->productName);
            }
            else if (prefixRecord != nullptr)
            {
                // 只有编号前缀时只能确定系列和协议能力，具体销售型号仍保持 Unknown。
                identity.productFamily = prefixRecord->productFamily;
                identity.protocolProfile = prefixRecord->protocolProfile;
                CopyText(identity.seriesNameUtf8, prefixRecord->seriesName);
            }

            // 未写号状态优先保留，即使型号代码已能识别产品，也需要让生产流程可见。
            if (!numberProgrammed)
            {
                identity.identificationStatus =
                    MeyerDeviceProductIdentification_DeviceNumberUnprogrammed;
                CopyText(identity.detailUtf8,
                         productRecord != nullptr
                             ? "Exact product identified, but the device number is not programmed"
                             : "Device number is empty, invalid or not programmed");
            }
            else if (productRecord != nullptr)
            {
                identity.identificationStatus = MeyerDeviceProductIdentification_ExactProduct;
                CopyText(identity.detailUtf8, "Exact product identified from the full model code");
            }
            else if (prefixRecord != nullptr)
            {
                identity.identificationStatus = MeyerDeviceProductIdentification_SeriesOnly;
                CopyText(identity.detailUtf8,
                         modelCodeValid
                             ? "Product series identified; model code is not registered"
                             : "Product series identified from the device number prefix");
            }
            else
            {
                CopyText(identity.detailUtf8,
                         "No registered device number prefix or model code matched");
            }
        }

        // 明确协议标记只能补充系列和能力 Profile，不得伪造具体产品型号。
        void DeviceProductCatalog::MergeProtocolProfileHint(
            std::int32_t protocolProfile,
            MeyerDeviceProductIdentity& identity)
        {
            if (identity.identificationStatus == MeyerDeviceProductIdentification_Conflict ||
                protocolProfile == MeyerDeviceModel_Unknown)
            {
                return;
            }

            std::int32_t hintedFamily = MeyerDeviceProductFamily_Unknown;
            const char* hintedSeries = "Unknown";
            GetFamilyForProfile(protocolProfile, hintedFamily, hintedSeries);
            if (hintedFamily == MeyerDeviceProductFamily_Unknown)
            {
                return;
            }

            // 已有已知系列与明确协议标记冲突时，同样返回冲突，不能覆盖原证据。
            if (identity.productFamily != MeyerDeviceProductFamily_Unknown &&
                identity.productFamily != hintedFamily)
            {
                identity.identificationStatus = MeyerDeviceProductIdentification_Conflict;
                CopyText(identity.detailUtf8,
                         "Product series conflicts with the explicit protocol profile marker");
                return;
            }

            identity.productFamily = hintedFamily;
            identity.protocolProfile = protocolProfile;
            CopyText(identity.seriesNameUtf8, hintedSeries);
            if (identity.identificationStatus == MeyerDeviceProductIdentification_Unknown)
            {
                identity.identificationStatus = MeyerDeviceProductIdentification_SeriesOnly;
                CopyText(identity.detailUtf8,
                         "Product series identified from an explicit protocol profile marker");
            }
        }

        // 完整精确匹配型号代码并返回采集/协议能力 Profile。
        std::int32_t DeviceProductCatalog::ProtocolProfileForModelCode(const char* modelCodeUtf8)
        {
            const ProductRecord* record = FindProductByModelCode(modelCodeUtf8);
            return record == nullptr ? MeyerDeviceModel_Unknown : record->protocolProfile;
        }
    }
}
