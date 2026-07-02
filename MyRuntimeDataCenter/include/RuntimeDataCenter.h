#pragma once

#ifdef MEYERSCAN_RUNTIMEDATACENTER_EXPORTS
#  define MEYERSCAN_RUNTIMEDATACENTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_RUNTIMEDATACENTER_API __declspec(dllimport)
#endif

/* RuntimeDataCenterResult 是运行时数据中心的统一返回结构。 */
struct RuntimeDataCenterResult {
    int errorCode;       /* 错误码，0 表示成功。 */
    char message[256];   /* UTF-8 短消息，用于日志、状态栏和调试输出。 */

    /* 判断调用是否成功。 */
    bool IsSuccess() const { return errorCode == 0; }

    /* 判断调用是否失败。 */
    bool IsError() const { return errorCode != 0; }
};

/*
 * IRuntimeDataCenter 是本地/云端运行时数据快照中心的公共接口。
 * 本模块负责把常用本地数据库信息和云端诊所信息加载到进程内缓存。
 * 对外只返回 JSON 快照，不暴露旧表结构、Qt 对象或固定 C++ 业务结构体。
 */
class MEYERSCAN_RUNTIMEDATACENTER_API IRuntimeDataCenter {
public:
    /* 虚析构函数保证跨 DLL 多态接口安全。 */
    virtual ~IRuntimeDataCenter() = default;

    /*
     * 初始化数据中心。
     * databaseConfigPathUtf8 是 db_config.json 绝对路径。
     * logDirUtf8 是统一 logs 目录。
     */
    virtual bool Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) = 0;

    /*
     * 重新加载全部已登记 domain。
     * 某些旧表不存在时不崩溃；模块会缓存空数组并返回部分失败提示。
     */
    virtual RuntimeDataCenterResult ReloadAll() = 0;

    /* 重新加载指定 domain，例如 local.patients、local.orders、cloud.clinicProfile。 */
    virtual RuntimeDataCenterResult ReloadDomain(const char* domainUtf8) = 0;

    /*
     * 获取指定 domain 的 JSON 快照。
     * buffer 由调用方提供，避免跨 DLL 分配和释放内存。
     */
    virtual RuntimeDataCenterResult GetDomainJson(const char* domainUtf8,
                                                  char* buffer,
                                                  int bufferSize) = 0;

    /*
     * 更新云端诊所信息快照。
     * 登录/云端同步模块后续拿到云端诊所 JSON 后调用本接口写入内存。
     */
    virtual RuntimeDataCenterResult UpdateCloudClinicJson(const char* cloudClinicJsonUtf8) = 0;

    /* 返回模块版本字符串。 */
    virtual const char* GetModuleVersion() const = 0;

    /* 关闭模块，释放缓存引用。 */
    virtual void Shutdown() = 0;
};

/* C ABI 工厂函数。MainExe、CaseUI、OrderCreateUI 等模块通过它获取进程内单例。 */
extern "C" MEYERSCAN_RUNTIMEDATACENTER_API IRuntimeDataCenter* GetRuntimeDataCenter();
