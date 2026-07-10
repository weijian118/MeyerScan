#ifndef ESCANSITETYPE_H
#define ESCANSITETYPE_H

// 扫描位点类型枚举
enum EScanSiteType
{
    ScanSite_None = 0,       // 无效/无位点

    // 上颌位点
    ScanSite_Maxilla_Natural,    // 自然牙上颌
    ScanSite_Maxilla_Prepared,   // 备牙上颌
    ScanSite_Maxilla_Cuff,       // 袖口上颌
    ScanSite_Maxilla_ScanRod1,   // 扫描杆1上颌
    ScanSite_Maxilla_ScanRod2,   // 扫描杆2上颌
    ScanSite_Maxilla_DiffRod1,   // 异性扫描杆1上颌
    ScanSite_Maxilla_DiffRod2,   // 异性扫描杆2上颌

    // 下颌位点
    ScanSite_Mandible_Natural,   // 自然牙下颌
    ScanSite_Mandible_Prepared,  // 备牙下颌
    ScanSite_Mandible_Cuff,      // 袖口下颌
    ScanSite_Mandible_ScanRod1,  // 扫描杆1下颌
    ScanSite_Mandible_ScanRod2,  // 扫描杆2下颌
    ScanSite_Mandible_DiffRod1,  // 异性扫描杆1下颌
    ScanSite_Mandible_DiffRod2,  // 异性扫描杆2下颌

    // 咬合位点
    ScanSite_Occlusion_Natural,  // 自然牙咬合
    ScanSite_Occlusion_Record,   // 咬合记录

    // 数据交换位点
    ScanSite_DataExchange        // 数据交换按钮
};

// 扫描部位枚举
enum EScanPartType
{
    ScanPart_None = 0,
    ScanPart_Maxilla,        // 上颌
    ScanPart_DataExchange,   // 数据交换
    ScanPart_Mandible,       // 下颌
    ScanPart_Occlusion       // 咬合
};

// 咬合类型枚举
enum EOcclusionType
{
    Occlusion_Natural = 0,   // 自然咬合
    Occlusion_Record          // 咬合记录
};

// 备牙状态枚举
enum EBeforePrep
{
    BeforePrep_No = 0,       // 关闭备牙
    BeforePrep_Yes            // 开启备牙
};

// 上颌修复类型枚举（位或组合）
enum EMaxillaType
{
    Maxilla_None    = 0x00,
    Maxilla_Repair  = 0x01,
    Maxilla_Plant   = 0x02,
    Maxilla_DiffRod = 0x04
};

// 下颌修复类型枚举（位或组合）
enum EMandibleType
{
    Mandible_None    = 0x00,
    Mandible_Repair  = 0x01,
    Mandible_Plant   = 0x02,
    Mandible_DiffRod = 0x04
};

#endif // ESCANSITETYPE_H