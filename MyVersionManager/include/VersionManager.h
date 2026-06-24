#pragma once

#ifdef MEYERSCAN_VERSIONMANAGER_EXPORTS
#  define MEYERSCAN_VERSIONMANAGER_API __declspec(dllexport)
#else
#  define MEYERSCAN_VERSIONMANAGER_API __declspec(dllimport)
#endif

// IVersionManager 负责生成运行时 EXE/DLL 版本清单。
// 它只做本地文件枚举和版本读取，不执行更新，也不判断升级策略。
class MEYERSCAN_VERSIONMANAGER_API IVersionManager {
public:
    virtual ~IVersionManager() = default;

    // 初始化版本清单模块。
    // appDirUtf8 是 MeyerScan.exe 所在目录，logDirUtf8 是 logs 目录。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 读取 appDir/config/version_modules.json 中声明的拆分模块，
    // 并写入 logs/versionList/versionList_时间戳.json。
    // 不扫描运行目录全部 DLL，避免把 Qt、OpenSSL、AWS、VC/UCRT 等第三方库混入运行时版本清单。
    virtual bool WriteVersionList() = 0;

    // 返回最近一次成功写出的版本清单路径。
    virtual const char* GetLastVersionListPath() const = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 清空缓存路径。
    virtual void Shutdown() = 0;
};

// 获取进程内版本清单模块单例。
extern "C" MEYERSCAN_VERSIONMANAGER_API IVersionManager* GetVersionManager();
