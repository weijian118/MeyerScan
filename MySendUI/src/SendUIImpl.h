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

// Implementation of the SendUI module.
// It displays order context and reports user actions to the host.
class SendUIImpl : public ISendUI {
    Q_DECLARE_TR_FUNCTIONS(SendUI)

public:
    static SendUIImpl& Instance();

    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    bool SetSessionContextJson(const char* contextJsonUtf8) override;
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    SendUIImpl() = default;
    ~SendUIImpl() = default;
    SendUIImpl(const SendUIImpl&) = delete;
    SendUIImpl& operator=(const SendUIImpl&) = delete;

    void LoadUIComponents();
    QWidget* CreateCaseInfoSection(QWidget* parent);
    QWidget* CreateSendActionSection(QWidget* parent);
    QLineEdit* CreateReadOnlyLineEdit(QWidget* parent);
    QLabel* CreateFieldLabel(const QString& text, QWidget* parent);
    QPushButton* CreateActionButton(const QString& text, int role, QWidget* parent);
    void ApplyContextToWidgets();
    QString ReadString(const QJsonObject& object, const char* key, const QString& fallback = QString()) const;
    void EmitAction(int actionId, const QString& operation);
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    QByteArray m_appDir;
    QByteArray m_logDir;
    QByteArray m_contextJson;
    QJsonObject m_contextObject;
    QLibrary m_uiComponentsLibrary;
    ILogger* m_logger = nullptr;
    IUIComponents* m_uiComponents = nullptr;
    QWidget* m_root = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_doctorEdit = nullptr;
    QLineEdit* m_orderNoEdit = nullptr;
    QLineEdit* m_orderTypeEdit = nullptr;
    QLineEdit* m_clinicEdit = nullptr;
    QComboBox* m_dataFormatCombo = nullptr;
    QTextEdit* m_noteEdit = nullptr;
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};
