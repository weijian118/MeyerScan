#pragma once

#include <QSize>
#include <QString>

// 治疗方案 PNG 与 mask 的固定资源规则。
//
// 该文件只放纯映射，不创建 QWidget、不保存订单状态，也不进入 DLL 公共 ABI。
// UI 实现和测试共用同一份规则，避免测试复制数组后仍无法发现生产代码写反。
namespace MeyerScanTreatmentPlanRules {

// 把牙位 mask 的灰度编码转换为 FDI 牙位号。
inline int ToothNumberForMaskValue(bool maxilla, int maskValue) {
    // mask 从 30 开始，每颗牙递增 15；255 是第 16 颗牙的合法编码。
    if (maskValue < 30 || maskValue > 255 || (maskValue - 30) % 15 != 0) {
        return 0;
    }

    static const int maxillaTeeth[16] = {
        11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24, 25, 26, 27, 28
    };
    static const int mandibleTeeth[16] = {
        31, 32, 33, 34, 35, 36, 37, 38, 41, 42, 43, 44, 45, 46, 47, 48
    };

    const int index = (maskValue - 30) / 15;
    return maxilla ? maxillaTeeth[index] : mandibleTeeth[index];
}

// 把桥 mask 的灰度编码转换为相邻牙 key。
inline QString BridgeKeyForMaskValue(bool maxilla, int maskValue) {
    // 桥 mask 从 35 开始，每个连接点递增 15，共 15 个位置。
    if (maskValue < 35 || maskValue > 245 || (maskValue - 35) % 15 != 0) {
        return QString();
    }

    static const char* maxillaKeys[15] = {
        "11-12", "12-13", "13-14", "14-15", "15-16", "16-17", "17-18",
        "11-21", "21-22", "22-23", "23-24", "24-25", "25-26", "26-27", "27-28"
    };
    static const char* mandibleKeys[15] = {
        "31-32", "32-33", "33-34", "34-35", "35-36", "36-37", "37-38",
        "31-41", "41-42", "42-43", "43-44", "44-45", "45-46", "46-47", "47-48"
    };

    const int index = (maskValue - 35) / 15;
    return QString::fromLatin1(maxilla ? maxillaKeys[index] : mandibleKeys[index]);
}

// 返回当前五种修复类型对应的单颗牙叠加图序号。
inline int TreatmentTypeImageIndex(const QString& typeCode) {
    if (typeCode == "crown" || typeCode == "full_crown") {
        return 1;
    }
    if (typeCode == "missing") {
        return 3;
    }
    if (typeCode == "inlay") {
        return 4;
    }
    if (typeCode == "veneer") {
        return 5;
    }
    if (typeCode == "implant") {
        return 7;
    }
    return 0;
}

// 常见 2560x1440 及更高分辨率使用 2x 图标源文件。
inline bool ShouldUseHighResolutionIcons(const QSize& screenSize) {
    return screenSize.width() >= 2560 || screenSize.height() >= 1440;
}

} // namespace MeyerScanTreatmentPlanRules
