#pragma once

#include "ConfigCenter.h"

#include <QJsonObject>
#include <QString>

class ConfigCenterImpl : public IConfigCenter {
public:
    static ConfigCenterImpl& Instance();

    bool Init(const char* appDirUtf8) override;
    bool GetBool(const char* key, bool defaultValue) const override;
    int GetInt(const char* key, int defaultValue) const override;
    bool GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    ConfigCenterImpl() = default;
    ~ConfigCenterImpl() = default;
    ConfigCenterImpl(const ConfigCenterImpl&) = delete;
    ConfigCenterImpl& operator=(const ConfigCenterImpl&) = delete;

    QJsonObject ResolveObject(const QString& key, QString* leafName) const;
    void EnsureDefaultConfig(const QString& configPath) const;

private:
    QString m_appDir;
    QString m_configPath;
    QJsonObject m_root;
    bool m_initialized = false;
};
