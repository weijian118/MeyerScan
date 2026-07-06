#include "ScanConfig.h"

ScanConfig::ScanConfig()
    : m_maxillaTypes(Maxilla_None)
    , m_mandibleTypes(Mandible_None)
    , m_occlusionType(Occlusion_Natural)
    , m_isMaxillaTemporary(false)
    , m_isMandibleTemporary(false)
    , m_isFullMouthTemporary(false)
    , m_beforePrep(BeforePrep_No)
    , m_isMaxillaRod2Enable(false)
    , m_isMandibleRod2Enable(false)
{
}

void ScanConfig::setMaxillaTypes(int types)
{
    m_maxillaTypes = types;
}

void ScanConfig::setMandibleTypes(int types)
{
    m_mandibleTypes = types;
}

void ScanConfig::setOcclusionType(EOcclusionType type)
{
    m_occlusionType = type;
}

void ScanConfig::setMaxillaTemporary(bool enable)
{
    m_isMaxillaTemporary = enable;
}

void ScanConfig::setMandibleTemporary(bool enable)
{
    m_isMandibleTemporary = enable;
}

void ScanConfig::setFullMouthTemporary(bool enable)
{
    m_isFullMouthTemporary = enable;
}

void ScanConfig::setBeforePrep(EBeforePrep prep)
{
    m_beforePrep = prep;
}

void ScanConfig::setMaxillaRod2Enable(bool enable)
{
    m_isMaxillaRod2Enable = enable;
}

void ScanConfig::setMandibleRod2Enable(bool enable)
{
    m_isMandibleRod2Enable = enable;
}

int ScanConfig::getMaxillaTypes() const
{
    return m_maxillaTypes;
}

int ScanConfig::getMandibleTypes() const
{
    return m_mandibleTypes;
}

EOcclusionType ScanConfig::getOcclusionType() const
{
    return m_occlusionType;
}

bool ScanConfig::isMaxillaTemporary() const
{
    // 全口临时牙优先级更高，开启则强制上颌为临时牙
    return m_isFullMouthTemporary || m_isMaxillaTemporary;
}

bool ScanConfig::isMandibleTemporary() const
{
    // 全口临时牙优先级更高，开启则强制下颌为临时牙
    return m_isFullMouthTemporary || m_isMandibleTemporary;
}

bool ScanConfig::isFullMouthTemporary() const
{
    return m_isFullMouthTemporary;
}

EBeforePrep ScanConfig::getBeforePrep() const
{
    return m_beforePrep;
}

bool ScanConfig::isMaxillaRod2Enable() const
{
    return m_isMaxillaRod2Enable;
}

bool ScanConfig::isMandibleRod2Enable() const
{
    return m_isMandibleRod2Enable;
}