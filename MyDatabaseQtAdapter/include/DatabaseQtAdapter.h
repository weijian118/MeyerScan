// =============================================================================
// 文件:    DatabaseQtAdapter.h
// 模块:    MeyerScan_DatabaseQtAdapter.dll
//
// 用途:
//   为已经使用 Qt 的 UI/Service 模块提供访问纯 C++ Database 的轻量适配层。
//
// 边界原则:
//   1. 本模块可以使用 QString、QByteArray、QJsonDocument 等 Qt Core 类型。
//   2. MeyerScan_Database.dll 不依赖本模块，也不知道 Qt 类型存在。
//   3. UI/Service 模块如需访问数据库，优先经过 RuntimeDataCenter/CaseOrderService；
//      测试造数或基础设施初始化时需要裸 SQL，才允许通过本适配层执行。
//   4. 本模块只做“类型转换 + 缓冲区管理”，不理解患者、订单、诊所、医生等业务含义。
// =============================================================================

#pragma once

#ifdef MEYERSCAN_DATABASEQTADAPTER_EXPORTS
#  define MEYERSCAN_DATABASEQTADAPTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_DATABASEQTADAPTER_API __declspec(dllimport)
#endif

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QString>
#include <QStringList>

class ILogger;

// IDatabaseQtAdapter 是给 MainExe/其它模块动态加载时使用的纯虚接口。
// 设计原因:
//   - 调用方只需要包含头文件，不需要链接 MeyerScan_DatabaseQtAdapter.lib。
//   - DLL 通过 GetDatabaseQtAdapter() 返回实现对象，调用方拿到接口后走虚函数表调用。
//   - 后续如果要继续去 Qt 化，只需要替换本 DLL 内部实现，不影响 MainExe 的动态加载方式。
class MEYERSCAN_DATABASEQTADAPTER_API IDatabaseQtAdapter {
public:
    virtual ~IDatabaseQtAdapter() = default;

    // 初始化并连接数据库。
    virtual bool EnsureConnected(const QString& configPath, QString* errorMessage = nullptr) = 0;

    // 初始化、按配置中心指定的数据库类型切换、再连接数据库。
    virtual bool EnsureConnected(const QString& configPath,
                                 const QString& databaseType,
                                 QString* errorMessage = nullptr) = 0;

    // 按字符串切换数据库类型。
    virtual bool SetDatabaseTypeName(const QString& databaseType, QString* errorMessage = nullptr) = 0;

    // 判断底层数据库连接是否已经打开。
    virtual bool IsConnected() const = 0;

    // 断开数据库连接。
    virtual void Disconnect() = 0;

    // 关闭数据库模块。
    virtual void Shutdown() = 0;

    // 返回底层 Database 模块版本，便于诊断底层数据库 DLL。
    virtual QString DatabaseModuleVersion() const = 0;

    // 返回 Adapter 自己的代码版本，便于 MainExe 生成运行时版本清单。
    virtual const char* GetModuleVersion() const = 0;

    // 执行无结果集 SQL，例如建表、插入、更新、删除。
    virtual bool ExecuteUpdate(const QString& sql, QString* errorMessage = nullptr) = 0;

    // 批量执行 QByteArray SQL 脚本。
    virtual int ExecuteScript(const QList<QByteArray>& scripts) = 0;

    // 批量执行 QString SQL 脚本。
    virtual int ExecuteScript(const QStringList& scripts) = 0;

    // 执行 SELECT 并返回 Database 通用表格 JSON。
    virtual bool ExecuteQueryJson(const QString& sql,
                                  QByteArray* outputJson,
                                  QString* errorMessage = nullptr,
                                  int initialBufferSize = 1024 * 1024,
                                  int maxBufferSize = 32 * 1024 * 1024) = 0;

    // 执行 SELECT 并解析为 QJsonDocument。
    virtual bool ExecuteQueryJsonDocument(const QString& sql,
                                          QJsonDocument* document,
                                          QString* errorMessage = nullptr) = 0;
};

// DatabaseQtAdapter 是 Qt 模块调用纯 C++ Database 的转换门面。
// 它不暴露 IDatabase 指针，目的是从编译期阻止 UI/Service 绕过适配层直连底层 Database。
class MEYERSCAN_DATABASEQTADAPTER_API DatabaseQtAdapter : public IDatabaseQtAdapter {
public:
    // 获取进程内适配器单例。
    static DatabaseQtAdapter& Instance();

    // 初始化并连接数据库。
    // configPath 使用 QString 方便 Qt 模块传入路径；函数内部会转换成 Database 接口要求的 UTF-8。
    bool EnsureConnected(const QString& configPath, QString* errorMessage = nullptr) override;

    // 初始化、按配置中心指定的数据库类型切换、再连接数据库。
    // databaseType 当前支持 sqlite/mysql；大小写不敏感，空字符串表示使用 db_config.json 中的类型。
    bool EnsureConnected(const QString& configPath,
                         const QString& databaseType,
                         QString* errorMessage = nullptr) override;

    // 按字符串切换数据库类型。
    // 该接口给配置中心或测试宿主使用，业务 UI 不应直接提供“任意切库”入口。
    bool SetDatabaseTypeName(const QString& databaseType, QString* errorMessage = nullptr) override;

    // 判断底层数据库连接是否已经打开。
    bool IsConnected() const override;

    // 断开数据库连接。
    // MainExe 析构或测试宿主收尾时使用；普通 UI 模块不要主动断开进程级数据库。
    void Disconnect() override;

    // 关闭数据库模块。
    // 这会清理底层 Database 单例状态，因此只允许 MainExe 或独立测试宿主在退出阶段调用。
    void Shutdown() override;

    // 返回底层 Database 模块版本，便于日志、版本清单和诊断界面显示。
    QString DatabaseModuleVersion() const override;

    // 返回 Adapter 自身版本。
    // 这个版本必须与 src/Version.rc 中的 FILEVERSION / ProductVersion 同步维护。
    const char* GetModuleVersion() const override;

    // 执行无结果集 SQL，例如建表、插入、更新、删除。
    // 正式业务 CRUD 应优先封装到 CaseOrderService，不要让 UI 直接拼 SQL。
    bool ExecuteUpdate(const QString& sql, QString* errorMessage = nullptr) override;

    // 批量执行 QByteArray SQL 脚本。
    // QList<QByteArray> 会在函数调用期间持有 UTF-8 字节，适合测试宿主批量建表/造数。
    int ExecuteScript(const QList<QByteArray>& scripts) override;

    // 批量执行 QString SQL 脚本。
    // 该重载会逐条转换为 UTF-8 后调用 QByteArray 版本。
    int ExecuteScript(const QStringList& scripts) override;

    // 执行 SELECT 并返回 Database 通用表格 JSON。
    // 函数内部会按需扩大调用方缓冲区，避免 Qt 模块自己处理固定 C 缓冲区。
    bool ExecuteQueryJson(const QString& sql,
                          QByteArray* outputJson,
                          QString* errorMessage = nullptr,
                          int initialBufferSize = 1024 * 1024,
                          int maxBufferSize = 32 * 1024 * 1024) override;

    // 执行 SELECT 并解析为 QJsonDocument。
    // 适合 RuntimeDataCenter/CaseOrderService 这类内部已经使用 Qt JSON 的模块。
    bool ExecuteQueryJsonDocument(const QString& sql,
                                  QJsonDocument* document,
                                  QString* errorMessage = nullptr) override;

    // 从 Database 通用表格 JSON 中提取 rows 数组。
    // Database 返回结构约定为 { "columns": [...], "rows": [...], "rowCount": n }。
    static QJsonArray RowsFromTableJson(const QByteArray& tableJson);
    static QJsonArray RowsFromTableJson(const QJsonDocument& tableDocument);

    // SQL 文本单引号转义。
    // 这是骨架期工具；正式 DAO 层引入参数绑定后，应逐步减少手写 SQL 拼接。
    static QString EscapeSqlText(const QString& text);

private:
    DatabaseQtAdapter() = default;
    ~DatabaseQtAdapter() = default;
    DatabaseQtAdapter(const DatabaseQtAdapter&) = delete;
    DatabaseQtAdapter& operator=(const DatabaseQtAdapter&) = delete;

    // 将 Database 返回的 const char* 消息复制成 QString，避免调用方关心底层消息生命周期。
    static QString ResultMessage(const char* message, const QString& fallback);

    // 获取 Logger 单例并缓存到 Adapter 单例中。
    // 这里只在私有区前向声明 ILogger，避免让公共头文件强迫调用方包含 Logger.h。
    ILogger* Logger() const;

    // 写入 Adapter 边界日志。
    // Adapter 只记录生命周期和失败/异常边界，不记录每条正常 SQL 成功，避免日志噪声。
    void WriteInfo(const QString& operation, const QString& content) const;
    void WriteWarning(const QString& operation, const QString& content) const;
    void WriteError(const QString& operation, const QString& content) const;

    // 适配器是进程级单例，缓存 ILogger* 可以避免每次写日志都重新 GetLogger()。
    mutable ILogger* m_logger = nullptr;
};

// C ABI 工厂函数，便于 QLibrary 或普通 C++ 调用方动态获取适配器。
// 这里继续返回具体类指针，兼容已有测试/服务模块源码；MainExe 动态加载后可按
// IDatabaseQtAdapter 基类使用，不需要链接 MeyerScan_DatabaseQtAdapter.lib。
extern "C" MEYERSCAN_DATABASEQTADAPTER_API DatabaseQtAdapter* GetDatabaseQtAdapter();
