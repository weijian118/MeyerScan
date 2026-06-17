#pragma once

#include "HomeUI.h"
#include "Database.h"
#include "Logger.h"
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();

class HomeUIImpl : public IHomeUI {
public:
    static HomeUIImpl& Instance();

    bool Init(const char* databaseConfigPath, const char* logDir) override;
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

    IDatabase* Database() const { return m_database; }
    bool IsDatabaseConnected() const { return m_databaseConnected; }
    QString LastStatus() const { return m_lastStatus; }

private:
    HomeUIImpl() = default;
    ~HomeUIImpl() = default;
    HomeUIImpl(const HomeUIImpl&) = delete;
    HomeUIImpl& operator=(const HomeUIImpl&) = delete;

    void WriteLog(LogLevel level, const char* operation, const QString& content);
    void LoadLogger(const char* logDir);
    void InitDatabase(const char* databaseConfigPath);

private:
    QLibrary m_loggerLibrary;
    ILogger* m_logger = nullptr;
    IDatabase* m_database = nullptr;
    bool m_databaseConnected = false;
    QString m_lastStatus = "Not initialized";
};
