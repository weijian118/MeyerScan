#pragma once

#include "CaseOrderService.h"

#include <QByteArray>
#include <QString>

#include "Database.h"
#include "Logger.h"

// CaseOrderServiceImpl 是患者/订单组合数据和相关参考数据的服务层实现。
// 它负责把 JSON/DTO 映射到数据库，不负责 UI 渲染、扫描方案、扫描采集或流程决策。
class CaseOrderServiceImpl : public ICaseOrderService {
public:
    // 返回进程内单例，保证服务状态和数据库引用集中管理。
    static CaseOrderServiceImpl& Instance();

    // 初始化日志和数据库连接。
    bool Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) override;

    // 创建或检查当前骨架需要的患者/订单表和参考数据表。
    CaseOrderServiceResult EnsureSchema() override;

    // 保存患者/订单组合 JSON。
    CaseOrderServiceResult SavePatientOrderJson(const char* patientOrderJsonUtf8) override;

    // 按订单 ID 读取患者/订单组合 JSON。
    CaseOrderServiceResult GetPatientOrderJson(const char* orderIdUtf8, char* buffer, int bufferSize) override;

    // 按 category 读取医生、诊所、技工所、操作人等参考数据。
    CaseOrderServiceResult ListReferenceDataJson(const char* categoryUtf8, char* buffer, int bufferSize) override;

    // 稳定查询入口，调用方传 queryName 和 JSON 参数，避免 UI 拼 SQL。
    CaseOrderServiceResult QueryJson(const char* queryNameUtf8,
                                     const char* queryArgsJsonUtf8,
                                     char* buffer,
                                     int bufferSize) override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 释放服务引用；不关闭进程级 Database / Logger。
    void Shutdown() override;

private:
    CaseOrderServiceImpl() = default;
    ~CaseOrderServiceImpl() = default;
    CaseOrderServiceImpl(const CaseOrderServiceImpl&) = delete;
    CaseOrderServiceImpl& operator=(const CaseOrderServiceImpl&) = delete;

    // 构造成功返回值。
    CaseOrderServiceResult Ok(const char* message = "OK") const;

    // 构造失败返回值。
    CaseOrderServiceResult Fail(int errorCode, const char* message) const;

    // 将 QString 结果复制到调用方缓冲区。
    CaseOrderServiceResult CopyToBuffer(const QString& text, char* buffer, int bufferSize) const;

    // SQL 文本转义；当前骨架先做单引号转义，后续 DAO 应改为参数绑定。
    QString EscapeSqlText(const QString& text) const;

    // 将外部 category 别名映射成内部稳定分类名。
    QString ReferenceCategoryToTable(const QString& category) const;

    // 写服务层结构化日志。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // 借用的数据库基础设施接口；生命周期由 MainExe 或数据库模块自身管理。
    IDatabase* m_database = nullptr;

    // 缓存后的日志接口指针。
    ILogger* m_logger = nullptr;

    // 初始化时传入的数据库配置路径，保存 UTF-8 字节避免临时对象失效。
    QByteArray m_databaseConfigPath;

    // 初始化时传入的日志目录，保存 UTF-8 字节避免临时对象失效。
    QByteArray m_logDir;

    // 标记服务是否已经完成 Init。
    bool m_initialized = false;
};
