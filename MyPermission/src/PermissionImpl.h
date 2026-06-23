#pragma once

#include "Permission.h"

#include <QJsonObject>
#include <QString>

class PermissionImpl : public IPermission {
public:
    static PermissionImpl& Instance();

    bool Init(const char* appDirUtf8) override;
    bool IsFeatureVisible(const char* featureId, bool defaultValue) const override;
    bool IsFeatureEnabled(const char* featureId, bool defaultValue) const override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    PermissionImpl() = default;
    ~PermissionImpl() = default;
    PermissionImpl(const PermissionImpl&) = delete;
    PermissionImpl& operator=(const PermissionImpl&) = delete;

    bool ReadBool(const QString& key, bool defaultValue) const;
    void EnsureDefaultConfig(const QString& configPath) const;

private:
    QString m_appDir;
    QJsonObject m_root;
    bool m_initialized = false;
};
