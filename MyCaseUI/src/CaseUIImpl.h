#pragma once

#include "CaseUI.h"
#include "Database.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();

// CaseUIImpl 是案例管理 UI 模块的实现。
// 它只负责列表/按钮/页签等界面框架和动作上报，不直接做患者/订单 CRUD。
class CaseUIImpl : public ICaseUI {
    Q_DECLARE_TR_FUNCTIONS(CaseUI)

public:
    // 返回进程内单例，避免多个案例 UI 实例重复抢占基础设施。
    static CaseUIImpl& Instance();

    // 初始化案例 UI 的日志和数据库健康检查。
    bool Init(const char* databaseConfigPath, const char* logDir) override;

    // 注册按钮/页签/打开订单等动作回调，由 MainExe 统一处理跨模块流程。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;

    // 设置动作入口显隐，例如“返回首页”按钮。
    void SetActionVisible(int actionId, bool visible) override;

    // 设置动作入口启用态，例如“返回首页”按钮是否可点击。
    void SetActionEnabled(int actionId, bool enabled) override;

    // 创建案例管理 QWidget；调用方负责挂载和释放。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 释放模块引用；不关闭全局 Logger / Database。
    void Shutdown() override;

private:
    CaseUIImpl() = default;
    ~CaseUIImpl() = default;
    CaseUIImpl(const CaseUIImpl&) = delete;
    CaseUIImpl& operator=(const CaseUIImpl&) = delete;

    // 动态加载 Logger 并缓存 ILogger 指针。
    void LoadLogger(const char* logDir);

    // 做数据库健康检查；正式业务数据读取后续迁到 CaseOrderService。
    void InitDatabase(const char* databaseConfigPath);

    // 写结构化日志；日志未初始化时静默返回。
    void WriteLog(LogLevel level, const char* operation, const QString& content);

    // 统一记录并上报动作 ID。
    void NotifyAction(int actionId, const QString& actionName);

    // 判断动作入口是否显示。
    bool IsActionVisible(int actionId) const;

    // 判断动作入口是否可点击。
    bool IsActionEnabled(int actionId) const;

    // 创建患者管理 Tab 的框架页面。
    QWidget* CreatePatientTab(QWidget* parent);

    // 创建订单管理 Tab 的框架页面。
    QWidget* CreateOrderTab(QWidget* parent);

private:
    // Logger DLL 句柄使用 PreventUnloadHint，避免退出时卸载顺序破坏日志对象。
    QLibrary m_loggerLibrary;

    // 缓存后的日志接口指针。
    ILogger* m_logger = nullptr;

    // 框架期借用数据库实例做健康检查；正式列表/搜索不应直接用它。
    IDatabase* m_database = nullptr;

    // 数据库健康检查是否成功。
    bool m_databaseConnected = false;

    // 最近一次状态文本，显示在页面底部。
    QString m_lastStatus = "Not initialized";

    // MainExe 注册的动作回调和上下文指针。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionCallbackContext = nullptr;

    // 返回首页按钮是否显示。
    bool m_backHomeVisible = true;

    // 返回首页按钮是否可点击。
    bool m_backHomeEnabled = true;
};
