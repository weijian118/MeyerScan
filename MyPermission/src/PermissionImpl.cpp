#include "PermissionImpl.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>

namespace {
const char* kModuleVersion = "MeyerScan_Permission v0.1.0 (2026-06-23)";
}

PermissionImpl& PermissionImpl::Instance() {
    static PermissionImpl instance;
    return instance;
}

bool PermissionImpl::Init(const char* appDirUtf8) {
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    if (m_appDir.isEmpty()) {
        return false;
    }

    QDir dir(m_appDir);
    dir.mkpath("config");
    const QString configPath = dir.filePath("config/permission_rules.json");
    EnsureDefaultConfig(configPath);

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return false;
    }

    m_root = document.object();
    m_initialized = true;
    return true;
}

bool PermissionImpl::IsFeatureVisible(const char* featureId, bool defaultValue) const {
    return ReadBool(QString("features.%1.visible").arg(QString::fromUtf8(featureId ? featureId : "")), defaultValue);
}

bool PermissionImpl::IsFeatureEnabled(const char* featureId, bool defaultValue) const {
    return ReadBool(QString("features.%1.enabled").arg(QString::fromUtf8(featureId ? featureId : "")), defaultValue);
}

const char* PermissionImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void PermissionImpl::Shutdown() {
    m_root = QJsonObject();
    m_initialized = false;
}

bool PermissionImpl::ReadBool(const QString& key, bool defaultValue) const {
    const QStringList parts = key.split('.', QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        return defaultValue;
    }

    QJsonObject object = m_root;
    for (int i = 0; i < parts.size() - 1; ++i) {
        object = object.value(parts[i]).toObject();
    }

    const QJsonValue value = object.value(parts.last());
    return value.isBool() ? value.toBool() : defaultValue;
}

void PermissionImpl::EnsureDefaultConfig(const QString& configPath) const {
    if (QFile::exists(configPath)) {
        return;
    }

    QJsonObject defaultRule;
    defaultRule.insert("visible", true);
    defaultRule.insert("enabled", true);

    QJsonObject home;
    home.insert("settings", defaultRule);

    QJsonObject caseUi;
    caseUi.insert("backHome", defaultRule);
    caseUi.insert("browse", defaultRule);

    QJsonObject order;
    order.insert("create", defaultRule);

    QJsonObject scan;
    scan.insert("practice", defaultRule);

    QJsonObject features;
    features.insert("home", home);
    features.insert("case", caseUi);
    features.insert("order", order);
    features.insert("scan", scan);

    QJsonObject root;
    root.insert("features", features);

    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

extern "C" MEYERSCAN_PERMISSION_API IPermission* GetPermission() {
    return &PermissionImpl::Instance();
}
