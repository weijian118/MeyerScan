#ifndef SCANCONFIG_H
#define SCANCONFIG_H

#include "EScanSiteType.h"

class ScanConfig
{
public:
    ScanConfig();

    // ========== 参数设置接口 ==========
    void setMaxillaTypes(int types);
    void setMandibleTypes(int types);
    void setOcclusionType(EOcclusionType type);

    void setMaxillaTemporary(bool enable);
    void setMandibleTemporary(bool enable);
    void setFullMouthTemporary(bool enable);

    void setBeforePrep(EBeforePrep prep);
    void setMaxillaRod2Enable(bool enable);
    void setMandibleRod2Enable(bool enable);

    // ========== 参数获取接口 ==========
    int getMaxillaTypes() const;
    int getMandibleTypes() const;
    EOcclusionType getOcclusionType() const;

    bool isMaxillaTemporary() const;
    bool isMandibleTemporary() const;
    bool isFullMouthTemporary() const;

    EBeforePrep getBeforePrep() const;
    bool isMaxillaRod2Enable() const;
    bool isMandibleRod2Enable() const;

private:
    // 修复类型
    int m_maxillaTypes;
    int m_mandibleTypes;

    // 咬合类型
    EOcclusionType m_occlusionType;

    // 临时牙开关
    bool m_isMaxillaTemporary;
    bool m_isMandibleTemporary;
    bool m_isFullMouthTemporary;

    // 扫描杆与备牙
    EBeforePrep m_beforePrep;
    bool m_isMaxillaRod2Enable;
    bool m_isMandibleRod2Enable;
};

#endif // SCANCONFIG_H