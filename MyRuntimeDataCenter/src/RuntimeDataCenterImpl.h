#pragma once

#include "RuntimeDataCenter.h"

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

#include "DatabaseQtAdapter.h"
#include "Logger.h"

/*
 * RuntimeDataCenterImpl 是运行时数据中心的具体实现。
 * 它从 Database 读取本地常用表，把结果缓存为 JSON；云端数据由外部模块注入。
 */
class RuntimeDataCenterImpl : public IRuntimeDataCenter {
public:
    /* 获取进程内单例。 */
    static RuntimeDataCenterImpl& Instance();

    /* 初始化日志和数据库引用。 */
    bool Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) override;

    /* 加载全部已知 domain。 */
    RuntimeDataCenterResult ReloadAll() override;

    /* 加载指定 domain。 */
    RuntimeDataCenterResult ReloadDomain(const char* domainUtf8) override;

    /* 获取指定 domain 的 JSON 快照。 */
    RuntimeDataCenterResult GetDomainJson(const char* domainUtf8,
                                          char* buffer,
                                          int bufferSize) override;

    /* 更新云端诊所信息快照。 */
    RuntimeDataCenterResult UpdateCloudClinicJson(const char* cloudClinicJsonUtf8) override;

    /* 返回模块版本字符串。 */
    const char* GetModuleVersion() const override;

    /* 释放缓存和外部模块引用。 */
    void Shutdown() override;

private:
    RuntimeDataCenterImpl() = default;
    ~RuntimeDataCenterImpl() = default;
    RuntimeDataCenterImpl(const RuntimeDataCenterImpl&) = delete;
    RuntimeDataCenterImpl& operator=(const RuntimeDataCenterImpl&) = delete;

    /* 构造成功返回值。 */
    RuntimeDataCenterResult Ok(const char* message = "OK") const;

    /* 构造失败返回值。 */
    RuntimeDataCenterResult Fail(int errorCode, const char* message) const;

    /* 将 QString 文本复制到调用方缓冲区。 */
    RuntimeDataCenterResult CopyToBuffer(const QString& text, char* buffer, int bufferSize) const;

    /* 确保 Database 已经初始化并连接。 */
    bool EnsureDatabaseReady();

    /* 判断 domain 是否属于云端注入数据。 */
    bool IsCloudDomain(const QString& domain) const;

    /* 返回本模块支持的全部 domain。 */
    QStringList KnownDomains() const;

    /* 根据 domain 返回允许尝试的旧表名列表。 */
    QStringList TablesForDomain(const QString& domain) const;

    /* 按旧表名生成安全 SQL。 */
    QString SelectSqlForTable(const QString& tableName) const;

    /* 从某个旧表读取通用 JSON 结果。 */
    bool QueryTableJson(const QString& tableName, QByteArray* output, QString* errorMessage) const;

    /* 把 Database 返回的表格 JSON 包装成本模块的 domain 快照 JSON。 */
    QString WrapTableJson(const QString& domain,
                          const QString& tableName,
                          const QByteArray& tableJson,
                          const QString& loadStatus,
                          const QString& lastError) const;

    /* 构造空 domain 快照，供缺表或查询失败时缓存。 */
    QString BuildEmptyDomainJson(const QString& domain,
                                 const QString& loadStatus,
                                 const QString& lastError) const;

    /* 写结构化日志。 */
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    /* 所有缓存读写都用同一把锁保护，避免 UI 和刷新线程同时访问 QHash。 */
    QMutex m_mutex;

    /* 借用的 Qt 数据库适配层单例，生命周期由 DatabaseQtAdapter 模块管理。 */
    DatabaseQtAdapter* m_databaseAdapter = nullptr;

    /* 借用的日志单例，生命周期由 Logger/MainExe 管理。 */
    ILogger* m_logger = nullptr;

    /* 初始化时传入的数据库配置路径，保存为 UTF-8 字节，避免临时对象失效。 */
    QByteArray m_databaseConfigPath;

    /* 初始化时传入的日志目录，保存为 UTF-8 字节，避免临时对象失效。 */
    QByteArray m_logDir;

    /* domain -> JSON 快照。所有调用方拿到的都是这个缓存的 UTF-8 文本。 */
    QHash<QString, QString> m_domainCache;

    /* 是否完成 Init。 */
    bool m_initialized = false;
};
