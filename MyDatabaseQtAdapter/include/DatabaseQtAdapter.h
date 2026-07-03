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

// DatabaseQtAdapter 是 Qt 模块调用纯 C++ Database 的转换门面。
// 它不暴露 IDatabase 指针，目的是从编译期阻止 UI/Service 绕过适配层直连底层 Database。
class MEYERSCAN_DATABASEQTADAPTER_API DatabaseQtAdapter {
public:
    // 获取进程内适配器单例。
    static DatabaseQtAdapter& Instance();

    // 初始化并连接数据库。
    // configPath 使用 QString 方便 Qt 模块传入路径；函数内部会转换成 Database 接口要求的 UTF-8。
    bool EnsureConnected(const QString& configPath, QString* errorMessage = nullptr);

    // 初始化、按配置中心指定的数据库类型切换、再连接数据库。
    // databaseType 当前支持 sqlite/mysql；大小写不敏感，空字符串表示使用 db_config.json 中的类型。
    bool EnsureConnected(const QString& configPath,
                         const QString& databaseType,
                         QString* errorMessage = nullptr);

    // 按字符串切换数据库类型。
    // 该接口给配置中心或测试宿主使用，业务 UI 不应直接提供“任意切库”入口。
    bool SetDatabaseTypeName(const QString& databaseType, QString* errorMessage = nullptr);

    // 判断底层数据库连接是否已经打开。
    bool IsConnected() const;

    // 断开数据库连接。
    // MainExe 析构或测试宿主收尾时使用；普通 UI 模块不要主动断开进程级数据库。
    void Disconnect();

    // 关闭数据库模块。
    // 这会清理底层 Database 单例状态，因此只允许 MainExe 或独立测试宿主在退出阶段调用。
    void Shutdown();

    // 返回底层 Database 模块版本，便于日志、版本清单和诊断界面显示。
    QString DatabaseModuleVersion() const;

    // 执行无结果集 SQL，例如建表、插入、更新、删除。
    // 正式业务 CRUD 应优先封装到 CaseOrderService，不要让 UI 直接拼 SQL。
    bool ExecuteUpdate(const QString& sql, QString* errorMessage = nullptr);

    // 批量执行 QByteArray SQL 脚本。
    // QList<QByteArray> 会在函数调用期间持有 UTF-8 字节，适合测试宿主批量建表/造数。
    int ExecuteScript(const QList<QByteArray>& scripts);

    // 批量执行 QString SQL 脚本。
    // 该重载会逐条转换为 UTF-8 后调用 QByteArray 版本。
    int ExecuteScript(const QStringList& scripts);

    // 执行 SELECT 并返回 Database 通用表格 JSON。
    // 函数内部会按需扩大调用方缓冲区，避免 Qt 模块自己处理固定 C 缓冲区。
    bool ExecuteQueryJson(const QString& sql,
                          QByteArray* outputJson,
                          QString* errorMessage = nullptr,
                          int initialBufferSize = 1024 * 1024,
                          int maxBufferSize = 32 * 1024 * 1024);

    // 执行 SELECT 并解析为 QJsonDocument。
    // 适合 RuntimeDataCenter/CaseOrderService 这类内部已经使用 Qt JSON 的模块。
    bool ExecuteQueryJsonDocument(const QString& sql,
                                  QJsonDocument* document,
                                  QString* errorMessage = nullptr);

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
};

// C ABI 工厂函数，便于 QLibrary 或普通 C++ 调用方动态获取适配器。
extern "C" MEYERSCAN_DATABASEQTADAPTER_API DatabaseQtAdapter* GetDatabaseQtAdapter();
