#pragma once

#ifdef MEYERSCAN_PERMISSION_EXPORTS
#  define MEYERSCAN_PERMISSION_API __declspec(dllexport)
#else
#  define MEYERSCAN_PERMISSION_API __declspec(dllimport)
#endif

class MEYERSCAN_PERMISSION_API IPermission {
public:
    virtual ~IPermission() = default;

    virtual bool Init(const char* appDirUtf8) = 0;
    virtual bool IsFeatureVisible(const char* featureId, bool defaultValue) const = 0;
    virtual bool IsFeatureEnabled(const char* featureId, bool defaultValue) const = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_PERMISSION_API IPermission* GetPermission();
