#pragma once

#ifdef MEYERSCAN_CASEORDERSERVICE_EXPORTS
#  define MEYERSCAN_CASEORDERSERVICE_API __declspec(dllexport)
#else
#  define MEYERSCAN_CASEORDERSERVICE_API __declspec(dllimport)
#endif

// CaseOrderServiceResult 是服务层统一的轻量返回结构。
// 设计原因:
//   - 只包含整数错误码和固定长度消息，避免跨 DLL 分配/释放字符串。
//   - errorCode == 0 表示成功，非 0 表示失败。
struct CaseOrderServiceResult {
    int errorCode;       // 错误码，0 表示成功。
    char message[256];   // UTF-8 短消息，主要用于日志和调试提示。

    // 判断本次服务调用是否成功。
    // 调用方优先使用这个小函数，而不是到处手写 errorCode == 0，
    // 后续若多个服务形成语义一致的稳定结果合同，也更容易集中抽取或统一替换。
    bool IsSuccess() const { return errorCode == 0; }

    // 判断本次服务调用是否失败。
    // 这个函数和 IsSuccess() 成对提供，便于调用方写出更直观的失败分支。
    bool IsError() const { return errorCode != 0; }
};

// ICaseOrderService 是患者/订单组合数据服务的公共接口。
// 模块边界:
//   - 负责患者/订单组合 JSON、医生/诊所/技工所等参考数据的读写。
//   - 可以调用 MyDatabase，但 UI 模块不应绕过本服务直接拼 SQL。
//   - 不负责 UI 渲染、扫描采集、扫描方案、算法处理或权限决策。
class MEYERSCAN_CASEORDERSERVICE_API ICaseOrderService {
public:
    // 虚析构函数用于保持跨 DLL 多态接口安全。
    virtual ~ICaseOrderService() = default;

    // 初始化服务。
    // databaseConfigPathUtf8 是数据库配置路径，logDirUtf8 是统一日志目录。
    virtual bool Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) = 0;

    // 创建/检查当前服务需要的数据库表。
    // 骨架阶段使用，正式版本后续应演进为版本化 migration。
    virtual CaseOrderServiceResult EnsureSchema() = 0;

    // 保存患者/订单组合 JSON。
    // JSON 内必须包含 orderId；字段可增删，服务先保留完整 payload 以支持扩展。
    virtual CaseOrderServiceResult SavePatientOrderJson(const char* patientOrderJsonUtf8) = 0;

    // 根据订单 ID 读取患者/订单组合 JSON。
    // buffer 由调用方提供，避免跨 DLL 返回动态字符串。
    virtual CaseOrderServiceResult GetPatientOrderJson(const char* orderIdUtf8, char* buffer, int bufferSize) = 0;

    // 读取参考数据列表。
    // category 可传 doctor/clinic/lab/operator 等白名单分类，空值表示读取全部。
    virtual CaseOrderServiceResult ListReferenceDataJson(const char* categoryUtf8, char* buffer, int bufferSize) = 0;

    // 稳定查询入口。
    // UI/外部适配器传 queryName + JSON 参数，服务内部决定对应 SQL，避免 UI 拼 SQL。
    virtual CaseOrderServiceResult QueryJson(const char* queryNameUtf8,
                                             const char* queryArgsJsonUtf8,
                                             char* buffer,
                                             int bufferSize) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭服务并清理缓存引用。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数。
extern "C" MEYERSCAN_CASEORDERSERVICE_API ICaseOrderService* GetCaseOrderService();
