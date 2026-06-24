#pragma once

#include "OrderScanWorkspaceShell.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMap>

#include "Logger.h"

class QLabel;
class QStackedWidget;

// OrderScanWorkspaceShellImpl 是建单/扫描统一工作区壳子的骨架实现。
// 它只负责页面容器、步骤切换和日志，不实现建单表单、扫描算法或发送逻辑。
class OrderScanWorkspaceShellImpl : public IOrderScanWorkspaceShell {
    Q_DECLARE_TR_FUNCTIONS(OrderScanWorkspaceShell)

public:
    // 返回进程内单例。
    static OrderScanWorkspaceShellImpl& Instance();

    // 初始化路径和日志。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 创建工作区根界面和步骤栈。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 切换到指定步骤。
    void SetStep(int step) override;

    // 为指定步骤挂入真实业务页面。
    void AttachStepWidget(int step, QWidget* widget) override;

    // 返回模块版本。
    const char* GetModuleVersion() const override;

    // 清理壳子状态。
    void Shutdown() override;

private:
    // 构造/析构私有化，保证单例生命周期。
    OrderScanWorkspaceShellImpl() = default;
    ~OrderScanWorkspaceShellImpl() = default;

    // 禁止拷贝，避免一个壳子状态被复制到多个 QWidget 树中。
    OrderScanWorkspaceShellImpl(const OrderScanWorkspaceShellImpl&) = delete;
    OrderScanWorkspaceShellImpl& operator=(const OrderScanWorkspaceShellImpl&) = delete;

    // 根据步骤 ID 返回页面标题。
    QString StepTitle(int step) const;

    // 为暂未接入的步骤创建占位页面。
    QWidget* CreatePlaceholder(int step, QWidget* parent) const;

    // 刷新顶部当前步骤标签。
    void RefreshStepLabel();

    // 写壳子模块日志。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // MeyerScan.exe 所在目录。
    QByteArray m_appDir;

    // 统一日志目录。
    QByteArray m_logDir;

    // 缓存日志接口。
    ILogger* m_logger = nullptr;

    // 工作区根界面弱引用。
    QWidget* m_root = nullptr;

    // 顶部当前步骤标题标签。
    QLabel* m_stepLabel = nullptr;

    // 步骤页面栈。
    QStackedWidget* m_stack = nullptr;

    // stepId -> QWidget 的映射，用于切换和替换步骤页面。
    QMap<int, QWidget*> m_stepWidgets;

    // 当前步骤，默认从建单开始。
    int m_currentStep = WorkspaceStepOrderCreate;
};
