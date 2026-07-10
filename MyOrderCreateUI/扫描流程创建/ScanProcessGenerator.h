#ifndef SCANPROCESSGENERATOR_H
#define SCANPROCESSGENERATOR_H

#include <QList>
#include <QMap>
#include "EScanSiteType.h"
#include "ScanConfig.h"

class ScanProcessGenerator
{
public:
    explicit ScanProcessGenerator(const ScanConfig& config);

    // ========== 核心功能接口 ==========
    // 生成所有部位的扫描流程（返回Map，界面展示顺序由Widget控制）
    QMap<EScanPartType, QList<EScanSiteType>> generateAllParts() const;

    // 单独生成上颌扫描流程
    QList<EScanSiteType> generateMaxilla() const;

    // 单独生成下颌扫描流程
    QList<EScanSiteType> generateMandible() const;

    // 单独生成咬合扫描流程
    QList<EScanSiteType> generateOcclusion() const;

    // 查询当前位点的下一个扫描位点（自动跳过交换按钮）
    EScanSiteType getNextScanSite(EScanSiteType currentSite) const;

private:
    ScanConfig m_config;

    // 内部辅助：判断是否需要生成数据交换位点
    bool needDataExchange() const;
};

#endif // SCANPROCESSGENERATOR_H