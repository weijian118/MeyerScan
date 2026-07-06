#pragma once

#ifdef MEYERSCAN_EXTERNALLAUNCHADAPTER_EXPORTS
#  define MEYERSCAN_EXTERNALLAUNCHADAPTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_EXTERNALLAUNCHADAPTER_API __declspec(dllimport)
#endif

// ExternalLaunchResult 是第三方拉起适配器返回给调用方的结果结构体。
// 该结构体会跨 DLL 边界传递，所以只能放 int 和固定长度 char 数组这类 POD 字段，
// 不能放 QString、std::string、QJsonObject 等带构造/析构逻辑的 C++ 对象。
struct ExternalLaunchResult {
    int errorCode;
    int requiredBufferSize;
    char message[256];
    char thirdPartyType[64];
    char thirdPartyName[128];
    char sourceSystem[128];
};

// IExternalLaunchAdapter 是第三方建单输入适配器的公共接口。
// 它只负责把外部 JSON 文件转换成 MeyerScan 内部标准建单上下文 JSON。
// 公共接口保持 const char* / POD / 调用方缓冲区形式，Qt 只作为 DLL 内部实现细节。
class MEYERSCAN_EXTERNALLAUNCHADAPTER_API IExternalLaunchAdapter {
public:
    // 虚析构用于保证跨 DLL 多态接口的基本安全。
    virtual ~IExternalLaunchAdapter() = default;

    // 初始化适配器。
    // appDirUtf8 是 MeyerScan.exe 所在目录，logDirUtf8 是统一日志目录。
    // 模块内部会复制字符串内容，不保存调用方临时指针。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 读取第三方输入 JSON 文件，并把归一化后的 UTF-8 标准建单上下文写入调用方缓冲区。
    // thirdPartyTypeUtf8 用于区分不同第三方；为空时优先从 JSON 的 source.thirdPartyType 读取。
    // outputJsonUtf8 由调用方分配，若缓冲区不足，result.requiredBufferSize 会返回所需字节数。
    virtual bool NormalizeOrderFile(const char* inputJsonPathUtf8,
                                    const char* thirdPartyTypeUtf8,
                                    char* outputJsonUtf8,
                                    int outputSize,
                                    ExternalLaunchResult* result) = 0;

    // 返回模块版本字符串，要求与 Version.rc 保持一致。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块并清理缓存状态，但不负责关闭进程级 Logger。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数。
// MainExe 可直接链接 lib，也可以后续改为 LoadLibrary/GetProcAddress 动态获取。
extern "C" MEYERSCAN_EXTERNALLAUNCHADAPTER_API IExternalLaunchAdapter* GetExternalLaunchAdapter();
