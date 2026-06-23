#pragma once

#ifdef MEYERSCAN_CONFIGCENTER_EXPORTS
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllimport)
#endif

class MEYERSCAN_CONFIGCENTER_API IConfigCenter {
public:
    virtual ~IConfigCenter() = default;

    virtual bool Init(const char* appDirUtf8) = 0;
    virtual bool GetBool(const char* key, bool defaultValue) const = 0;
    virtual int GetInt(const char* key, int defaultValue) const = 0;
    virtual bool GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_CONFIGCENTER_API IConfigCenter* GetConfigCenter();
