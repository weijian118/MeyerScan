// ==================== ScanProcessWidget.cpp ====================
#include "ScanProcessWidget.h"

ScanProcessWidget::ScanProcessWidget(QWidget *parent)
    : QWidget(parent)
    , m_generator(nullptr)
{
}

void ScanProcessWidget::updateScanProcess(const ScanConfig& config)
{
    if (m_generator)
        delete m_generator;
    
    m_generator = new ScanProcessGenerator(config);
    m_scanProcess = m_generator->generateAllParts();
    
    // 刷新界面按钮布局
    renderScanButtons();
}

EScanSiteType ScanProcessWidget::getNextScanSite(EScanSiteType currentSite) const
{
    if (!m_generator) 
        return ScanSite_None;
    return m_generator->getNextScanSite(currentSite);
}

void ScanProcessWidget::renderScanButtons()
{
    // 界面展示固定顺序：上颌 → 数据交换 → 下颌 → 咬合
    QList<EScanPartType> partOrder = {
        ScanPart_Maxilla,
        ScanPart_DataExchange,
        ScanPart_Mandible,
        ScanPart_Occlusion
    };
    
    for (EScanPartType part : partOrder)
    {
        if (!m_scanProcess.contains(part))
            continue;
        
        const QList<EScanSiteType>& sites = m_scanProcess[part];
        // 遍历sites创建对应按钮...
    }
}