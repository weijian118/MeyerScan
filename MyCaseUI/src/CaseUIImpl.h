#pragma once

#include "CaseUI.h"
#include "Database.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();

class CaseUIImpl : public ICaseUI {
    Q_DECLARE_TR_FUNCTIONS(CaseUI)

public:
    static CaseUIImpl& Instance();

    bool Init(const char* databaseConfigPath, const char* logDir) override;
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    void SetActionVisible(int actionId, bool visible) override;
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    CaseUIImpl() = default;
    ~CaseUIImpl() = default;
    CaseUIImpl(const CaseUIImpl&) = delete;
    CaseUIImpl& operator=(const CaseUIImpl&) = delete;

    void LoadLogger(const char* logDir);
    void InitDatabase(const char* databaseConfigPath);
    void WriteLog(LogLevel level, const char* operation, const QString& content);
    void NotifyAction(int actionId, const QString& actionName);
    bool IsActionVisible(int actionId) const;
    QWidget* CreatePatientTab(QWidget* parent);
    QWidget* CreateOrderTab(QWidget* parent);

private:
    QLibrary m_loggerLibrary;
    ILogger* m_logger = nullptr;
    IDatabase* m_database = nullptr;
    bool m_databaseConnected = false;
    QString m_lastStatus = "Not initialized";
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionCallbackContext = nullptr;
    bool m_backHomeVisible = true;
};
