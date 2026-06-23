#include "ConfigCenterImpl.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>
#include <cstring>

namespace {
const char* kModuleVersion = "MeyerScan_ConfigCenter v0.1.0 (2026-06-23)";
}

ConfigCenterImpl& ConfigCenterImpl::Instance() {
    static ConfigCenterImpl instance;
    return instance;
}

bool ConfigCenterImpl::Init(const char* appDirUtf8) {
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    if (m_appDir.isEmpty()) {
        return false;
    }

    QDir dir(m_appDir);
    dir.mkpath("config");
    m_configPath = dir.filePath("config/runtime_config.json");
    EnsureDefaultConfig(m_configPath);

    QFile file(m_configPath);
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

bool ConfigCenterImpl::GetBool(const char* key, bool defaultValue) const {
    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    const QJsonValue value = object.value(leafName);
    return value.isBool() ? value.toBool() : defaultValue;
}

int ConfigCenterImpl::GetInt(const char* key, int defaultValue) const {
    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    const QJsonValue value = object.value(leafName);
    return value.isDouble() ? value.toInt() : defaultValue;
}

bool ConfigCenterImpl::GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const {
    if (!buffer || bufferSize <= 0) {
        return false;
    }

    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    const QJsonValue value = object.value(leafName);
    const QString text = value.isString() ? value.toString() : QString::fromUtf8(defaultValue ? defaultValue : "");
    const QByteArray bytes = text.toUtf8();
    const int copySize = qMin(bufferSize - 1, bytes.size());
    if (copySize > 0) {
        memcpy(buffer, bytes.constData(), static_cast<size_t>(copySize));
    }
    buffer[copySize] = '\0';
    return value.isString();
}

const char* ConfigCenterImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void ConfigCenterImpl::Shutdown() {
    m_root = QJsonObject();
    m_initialized = false;
}

QJsonObject ConfigCenterImpl::ResolveObject(const QString& key, QString* leafName) const {
    const QStringList parts = key.split('.', QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (leafName) {
            *leafName = QString();
        }
        return QJsonObject();
    }

    QJsonObject object = m_root;
    for (int i = 0; i < parts.size() - 1; ++i) {
        object = object.value(parts[i]).toObject();
    }
    if (leafName) {
        *leafName = parts.last();
    }
    return object;
}

void ConfigCenterImpl::EnsureDefaultConfig(const QString& configPath) const {
    if (QFile::exists(configPath)) {
        return;
    }

    QJsonObject root;
    QJsonObject database;
    database.insert("type", "mysql");

    QJsonObject feature;
    QJsonObject home;
    home.insert("settingsVisible", true);
    QJsonObject caseUi;
    caseUi.insert("backHomeVisible", true);
    feature.insert("home", home);
    feature.insert("case", caseUi);

    QJsonObject startup;
    startup.insert("showWaitPage", true);
    startup.insert("singleInstance", true);

    root.insert("database", database);
    root.insert("feature", feature);
    root.insert("startup", startup);

    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

extern "C" MEYERSCAN_CONFIGCENTER_API IConfigCenter* GetConfigCenter() {
    return &ConfigCenterImpl::Instance();
}
