#pragma once

#include "HomeUI.h"
#include "Database.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();

class HomeUIImpl : public IHomeUI {
    Q_DECLARE_TR_FUNCTIONS(HomeUI)

public:
    static HomeUIImpl& Instance();

    bool Init(const char* databaseConfigPath, const char* logDir) override;
    void SetEntryCallback(void (*callback)(void* context, int entryId), void* context) override;
    void SetEntryVisible(int entryId, bool visible) override;
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
    void NotifyEntryClicked(int entryId);
    bool IsEntryVisible(int entryId) const;

private:
    QLibrary m_loggerLibrary;
    ILogger* m_logger = nullptr;
    IDatabase* m_database = nullptr;
    bool m_databaseConnected = false;
    QString m_lastStatus = "Not initialized";
    void (*m_entryCallback)(void* context, int entryId) = nullptr;
    void* m_entryCallbackContext = nullptr;
    bool m_entryVisible[5] = { true, true, true, true, true };
};
