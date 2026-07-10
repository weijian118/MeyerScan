// ==================== ScanProcessWidget.h ====================
#ifndef SCANPROCESSWIDGET_H
#define SCANPROCESSWIDGET_H

#include <QWidget>
#include "ScanProcessGenerator.h"
#include "EScanSiteType.h"

class ScanProcessWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ScanProcessWidget(QWidget *parent = nullptr);

    // 根据配置更新界面扫描流程
    void updateScanProcess(const ScanConfig& config);

    // 查询下一个扫描位点
    EScanSiteType getNextScanSite(EScanSiteType currentSite) const;

private:
    ScanProcessGenerator* m_generator;
    QMap<EScanPartType, QList<EScanSiteType>> m_scanProcess;

    // 界面按钮渲染（内部实现）
    void renderScanButtons();
};

#endif // SCANPROCESSWIDGET_H