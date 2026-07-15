#pragma once

#ifdef MEYERSCAN_SCANSCHEMASERVICE_EXPORTS
#  define MEYERSCAN_SCANSCHEMASERVICE_API __declspec(dllexport)
#else
#  define MEYERSCAN_SCANSCHEMASERVICE_API __declspec(dllimport)
#endif

// 扫描方案服务的公共 API 版本。MainExe/调用方可在取得接口前先校验该值。
static const int MEYER_SCAN_SCHEMA_SERVICE_API_VERSION = 1;

// ScanSchemaServiceResult 使用固定大小字段跨 DLL 返回结果，避免跨模块分配字符串。
struct ScanSchemaServiceResult {
    int errorCode;        // 0 表示成功，其它值表示参数、解析或容量错误。
    int bytesWritten;     // 成功时写入输出缓冲区的 UTF-8 字节数，不含结尾 '\0'。
    int requiredSize;     // 缓冲区不足时需要的总字节数，包含结尾 '\0'。
    char message[256];    // UTF-8 诊断消息，供日志和测试使用。

    // 调用方通过该函数判断成功，避免散落 errorCode == 0。
    bool IsSuccess() const { return errorCode == 0; }

    // 调用方通过该函数判断失败，使错误分支更易阅读。
    bool IsError() const { return errorCode != 0; }
};

// IScanSchemaService 负责把建单输入配置转换为稳定的扫描步骤合同。
// UI 只收集开关、咬合类型和牙位方案，不再自行维护步骤生成规则。
class MEYERSCAN_SCANSCHEMASERVICE_API IScanSchemaService {
public:
    // 虚析构函数保证通过接口指针使用实现对象时布局完整。
    virtual ~IScanSchemaService() = default;

    // 初始化日志目录。服务不访问 UI、数据库或当前工作目录。
    virtual bool Init(const char* logDirUtf8) = 0;

    // 根据 UTF-8 JSON 配置生成 scanProcess JSON。
    // 输入根对象可以直接是 config，也可以是包含 config 节点的 scanProcess 对象。
    virtual ScanSchemaServiceResult BuildScanProcessJson(const char* configJsonUtf8,
                                                         char* outputBuffer,
                                                         int outputBufferSize) = 0;

    // 返回模块代码版本。
    virtual const char* GetModuleVersion() const = 0;

    // 清理服务借用的日志接口和临时状态。
    virtual void Shutdown() = 0;
};

// 返回进程内扫描方案服务单例。
extern "C" MEYERSCAN_SCANSCHEMASERVICE_API IScanSchemaService* GetScanSchemaService();

// GetMeyerModuleApiVersion/GetMeyerModuleVersion 使用统一导出名供动态加载器解析。
// 它们不在公共头中声明，避免多个模块头同时包含时产生同名 dllimport 声明冲突。
