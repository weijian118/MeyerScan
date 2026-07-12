#pragma once

#include "SendUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonObject>
#include <QLibrary>
#include <QString>

#include "Logger.h"
#include "UIComponents.h"

using GetUIComponentsFunc = IUIComponents* (*)();

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QComboBox;

// SendUI 模块的内部实现。
//
// 类使用 DLL 内单例保存当前页面的一组弱引用；QWidget 的真实所有权始终属于 Qt 父子树。
// 这允许 MainExe 释放整个工作台后调用 Shutdown 清空指针，而不发生跨 DLL delete。
class SendUIImpl : public ISendUI {
    Q_DECLARE_TR_FUNCTIONS(SendUI)

public:
    // 返回 DLL 内唯一实例，避免多份发送页状态和回调互相覆盖。
    static SendUIImpl& Instance();

    // 复制路径、缓存 Logger，并按需加载共享 UI 组件。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 使用 Qt Layout 创建发送页，不执行任何真实发送业务。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 校验并保存会话 JSON，成功后同步刷新已创建控件。
    bool SetSessionContextJson(const char* contextJsonUtf8) override;

    // 保存宿主动作回调及其上下文指针。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;

    // 返回代码版本常量。
    const char* GetModuleVersion() const override;

    // 清理本模块缓存，但不删除宿主持有的 QWidget。
    void Shutdown() override;

private:
    // 构造和析构私有化，外部只能通过 Instance/GetSendUI 访问单例。
    SendUIImpl() = default;
    ~SendUIImpl() = default;

    // QWidget 弱引用、QLibrary 和回调上下文都不能被浅复制。
    SendUIImpl(const SendUIImpl&) = delete;
    SendUIImpl& operator=(const SendUIImpl&) = delete;

    // 动态加载 UIComponents；失败时保留 Qt 原生控件降级路径。
    void LoadUIComponents();

    // 创建病例字段区，只负责展示标准上下文字段。
    QWidget* CreateCaseInfoSection(QWidget* parent);

    // 创建发送动作区，所有按钮只连接到 EmitAction。
    QWidget* CreateSendActionSection(QWidget* parent);

    // 创建共享样式的只读输入框；UIComponents 不可用时使用 QLineEdit。
    QLineEdit* CreateReadOnlyLineEdit(QWidget* parent);

    // 创建统一字段标签，并在跨 DLL 前把 QString 转成临时 UTF-8 缓冲区。
    QLabel* CreateFieldLabel(const QString& text, QWidget* parent);

    // 创建统一动作按钮；role 只决定视觉角色，不包含业务权限或行为。
    QPushButton* CreateActionButton(const QString& text, int role, QWidget* parent);

    // 把当前已校验的 JSON 对象投影到控件，程序性设置不应误触发用户动作。
    void ApplyContextToWidgets();

    // 从对象读取字符串或数值文本；字段缺失、空字符串和其它类型均返回 fallback。
    QString ReadString(const QJsonObject& object, const char* key, const QString& fallback = QString()) const;

    // 先写客户操作日志，再通过稳定 int 回调通知宿主。
    void EmitAction(int actionId, const QString& operation);

    // 使用 Qt 日志适配层把 QString 安全转换到 Logger 的 UTF-8 C ABI。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // 路径和上下文使用自有 QByteArray，不能保存调用方传入的 const char*。
    QByteArray m_appDir;
    QByteArray m_logDir;
    QByteArray m_contextJson;
    QJsonObject m_contextObject;

    // PreventUnloadHint 保证 m_uiComponents 指针在进程内持续有效。
    QLibrary m_uiComponentsLibrary;
    ILogger* m_logger = nullptr;
    IUIComponents* m_uiComponents = nullptr;

    // 以下 QWidget 指针均为非 owning 弱引用，真实对象由 m_root 的 Qt 父子树持有。
    QWidget* m_root = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_doctorEdit = nullptr;
    QLineEdit* m_orderNoEdit = nullptr;
    QLineEdit* m_orderTypeEdit = nullptr;
    QLineEdit* m_clinicEdit = nullptr;
    QComboBox* m_dataFormatCombo = nullptr;
    QTextEdit* m_noteEdit = nullptr;
    // 纯 C 回调避免把 QObject 信号或 std::function ABI 暴露到 DLL 边界。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};
