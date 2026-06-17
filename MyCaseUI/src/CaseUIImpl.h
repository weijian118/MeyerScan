#pragma once

#include "CaseUI.h"
#include "Database.h"
#include "Logger.h"
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();

class CaseUIImpl : public ICaseUI {
public:
    static CaseUIImpl& Instance();

    bool Init(const char* databaseConfigPath, const char* logDir) override;
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
    QWidget* CreatePatientTab(QWidget* parent);
    QWidget* CreateOrderTab(QWidget* parent);

private:
    QLibrary m_loggerLibrary;
    ILogger* m_logger = nullptr;
    IDatabase* m_database = nullptr;
    bool m_databaseConnected = false;
    QString m_lastStatus = "Not initialized";
};
