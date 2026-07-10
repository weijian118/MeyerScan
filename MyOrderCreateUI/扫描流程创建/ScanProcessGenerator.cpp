#include "ScanProcessGenerator.h"

ScanProcessGenerator::ScanProcessGenerator(const ScanConfig& config)
    : m_config(config)
{
}

QMap<EScanPartType, QList<EScanSiteType>> ScanProcessGenerator::generateAllParts() const
{
    QMap<EScanPartType, QList<EScanSiteType>> result;

    // 生成各部位流程
    result[ScanPart_Maxilla] = generateMaxilla();
    result[ScanPart_Mandible] = generateMandible();
    result[ScanPart_Occlusion] = generateOcclusion();

    // 根据规则判断是否生成数据交换位点
    if (needDataExchange())
    {
        QList<EScanSiteType> exchangeList;
        exchangeList << ScanSite_DataExchange;
        result[ScanPart_DataExchange] = exchangeList;
    }

    return result;
}

QList<EScanSiteType> ScanProcessGenerator::generateMaxilla() const
{
    QList<EScanSiteType> sites;
    const int types = m_config.getMaxillaTypes();
    const bool rod2 = m_config.isMaxillaRod2Enable();
    const EBeforePrep prep = m_config.getBeforePrep();
    const bool isTemp = m_config.isMaxillaTemporary();

    // 基础条件判断
    const bool hasNone = (types == Maxilla_None);
    const bool hasRepair = (types & Maxilla_Repair);
    const bool hasPlant = (types & Maxilla_Plant);
    const bool hasDiffRod = (types & Maxilla_DiffRod);
    const bool onlyRepair = hasRepair && !hasPlant && !hasDiffRod && !hasNone;
    const bool needPrep = onlyRepair && (prep == BeforePrep_Yes);

    // ========== 场景1：存在异性扫描杆（优先级最高） ==========
    if (hasDiffRod)
    {
        // 临时牙场景先追加自然牙位点
        if (isTemp)
            sites << ScanSite_Maxilla_Natural;

        // 异性扫描杆1
        sites << ScanSite_Maxilla_DiffRod1;
        // 异性扫描杆2（开关开启时追加）
        if (rod2)
            sites << ScanSite_Maxilla_DiffRod2;

        // 异性杆场景必须追加袖口位点
        sites << ScanSite_Maxilla_Cuff;

        return sites;
    }

    // ========== 场景2：无异性杆，存在种植 ==========
    if (hasPlant)
    {
        sites << ScanSite_Maxilla_Natural;

        // 仅临时牙种植场景追加袖口
        if (isTemp)
            sites << ScanSite_Maxilla_Cuff;

        sites << ScanSite_Maxilla_ScanRod1;
        if (rod2)
            sites << ScanSite_Maxilla_ScanRod2;

        return sites;
    }

    // ========== 场景3：仅修复/无修复 ==========
    if (hasNone || onlyRepair)
    {
        sites << ScanSite_Maxilla_Natural;
        if (needPrep)
            sites << ScanSite_Maxilla_Prepared;
        return sites;
    }

    return sites;
}

QList<EScanSiteType> ScanProcessGenerator::generateMandible() const
{
    QList<EScanSiteType> sites;
    const int types = m_config.getMandibleTypes();
    const bool rod2 = m_config.isMandibleRod2Enable();
    const EBeforePrep prep = m_config.getBeforePrep();
    const bool isTemp = m_config.isMandibleTemporary();

    // 基础条件判断
    const bool hasNone = (types == Mandible_None);
    const bool hasRepair = (types & Mandible_Repair);
    const bool hasPlant = (types & Mandible_Plant);
    const bool hasDiffRod = (types & Mandible_DiffRod);
    const bool onlyRepair = hasRepair && !hasPlant && !hasDiffRod && !hasNone;
    const bool needPrep = onlyRepair && (prep == BeforePrep_Yes);

    // ========== 场景1：存在异性扫描杆（优先级最高） ==========
    if (hasDiffRod)
    {
        // 临时牙场景先追加自然牙位点
        if (isTemp)
            sites << ScanSite_Mandible_Natural;

        // 异性扫描杆1
        sites << ScanSite_Mandible_DiffRod1;
        // 异性扫描杆2（开关开启时追加）
        if (rod2)
            sites << ScanSite_Mandible_DiffRod2;

        // 异性杆场景必须追加袖口位点
        sites << ScanSite_Mandible_Cuff;

        return sites;
    }

    // ========== 场景2：无异性杆，存在种植 ==========
    if (hasPlant)
    {
        sites << ScanSite_Mandible_Natural;

        // 仅临时牙种植场景追加袖口
        if (isTemp)
            sites << ScanSite_Mandible_Cuff;

        sites << ScanSite_Mandible_ScanRod1;
        if (rod2)
            sites << ScanSite_Mandible_ScanRod2;

        return sites;
    }

    // ========== 场景3：仅修复/无修复 ==========
    if (hasNone || onlyRepair)
    {
        sites << ScanSite_Mandible_Natural;
        if (needPrep)
            sites << ScanSite_Mandible_Prepared;
        return sites;
    }

    return sites;
}

QList<EScanSiteType> ScanProcessGenerator::generateOcclusion() const
{
    QList<EScanSiteType> sites;
    const EOcclusionType type = m_config.getOcclusionType();

    switch (type)
    {
    case Occlusion_Natural:
        sites << ScanSite_Occlusion_Natural;
        break;
    case Occlusion_Record:
        sites << ScanSite_Occlusion_Record;
        break;
    default:
        sites << ScanSite_Occlusion_Natural;
        break;
    }

    return sites;
}

EScanSiteType ScanProcessGenerator::getNextScanSite(EScanSiteType currentSite) const
{
    // 拼接完整跳转流程：上颌→下颌→咬合（天然排除数据交换）
    QList<EScanSiteType> fullFlow;
    fullFlow.append(generateMaxilla());
    fullFlow.append(generateMandible());
    fullFlow.append(generateOcclusion());

    // 查找当前位点索引
    const int currentIndex = fullFlow.indexOf(currentSite);

    // 未找到或已是最后一个，返回无效位点
    if (currentIndex == -1 || currentIndex == fullFlow.size() - 1)
    {
        return ScanSite_None;
    }

    // 返回下一个位点
    return fullFlow.at(currentIndex + 1);
}

bool ScanProcessGenerator::needDataExchange() const
{
    // 条件1：存在异性扫描杆 → 不生成
    const bool hasDiffRod = (m_config.getMaxillaTypes() & Maxilla_DiffRod)
                    || (m_config.getMandibleTypes() & Mandible_DiffRod);
    if (hasDiffRod)
        return false;

    // 条件2：备牙开启 → 不生成
    if (m_config.getBeforePrep() == BeforePrep_Yes)
        return false;

    // 条件3：任意扫描杆2开启 → 不生成
    if (m_config.isMaxillaRod2Enable() || m_config.isMandibleRod2Enable())
        return false;

    // 条件4：任意临时牙开启 → 不生成
    if (m_config.isMaxillaTemporary() || m_config.isMandibleTemporary())
        return false;

    // 所有条件都不满足，生成数据交换
    return true;
}