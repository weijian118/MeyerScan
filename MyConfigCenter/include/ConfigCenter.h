#pragma once

#ifdef MEYERSCAN_CONFIGCENTER_EXPORTS
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllimport)
#endif

// IConfigCenter 是运行配置的唯一读取入口。
// 配置表达“产品/客户默认策略”，不表达授权结果；授权结果必须再经过 Permission 判断。
class MEYERSCAN_CONFIGCENTER_API IConfigCenter {
public:
    virtual ~IConfigCenter() = default;

    // 初始化配置中心。
    // appDirUtf8 必须是 MeyerScan.exe 所在目录，不能传当前工作目录。
    // TODO(v0.2.0): 当前骨架阶段为保持调用链简单暂时返回 bool。
    // 等真实失败语义稳定后，应升级为可携带失败原因的返回类型；是否与其它模块
    // 共用结果类型，要以真实重复为依据，不能为了统一形式先建立公共大包。
    // 同时要拆清“应用目录”和“配置文件路径”的语义，避免调用方误传。
    virtual bool Init(const char* appDirUtf8) = 0;

    // 读取布尔配置。key 使用点号分隔，例如 feature.home.settingsVisible。
    virtual bool GetBool(const char* key, bool defaultValue) const = 0;

    // 读取整数配置。当前骨架保留给后续数值类配置使用。
    virtual int GetInt(const char* key, int defaultValue) const = 0;

    // 读取字符串配置。buffer 由调用方分配，避免跨 DLL 分配/释放内存。
    virtual bool GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const = 0;

    // 返回模块版本字符串，用于版本清单和排查。
    virtual const char* GetModuleVersion() const = 0;

    // 释放配置缓存。
    virtual void Shutdown() = 0;
};

// 获取进程内配置中心单例。
extern "C" MEYERSCAN_CONFIGCENTER_API IConfigCenter* GetConfigCenter();
