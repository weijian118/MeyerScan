#pragma once

#ifdef MEYERSCAN_VERSIONMANAGER_EXPORTS
#  define MEYERSCAN_VERSIONMANAGER_API __declspec(dllexport)
#else
#  define MEYERSCAN_VERSIONMANAGER_API __declspec(dllimport)
#endif

class MEYERSCAN_VERSIONMANAGER_API IVersionManager {
public:
    virtual ~IVersionManager() = default;

    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;
    virtual bool WriteVersionList() = 0;
    virtual const char* GetLastVersionListPath() const = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_VERSIONMANAGER_API IVersionManager* GetVersionManager();
