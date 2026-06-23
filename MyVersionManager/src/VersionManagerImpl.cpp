#include "VersionManagerImpl.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <windows.h>

namespace {
const char* kModuleVersion = "MeyerScan_VersionManager v0.1.0 (2026-06-23)";
}

VersionManagerImpl& VersionManagerImpl::Instance() {
    static VersionManagerImpl instance;
    return instance;
}

bool VersionManagerImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QString::fromUtf8(logDirUtf8 ? logDirUtf8 : "");
    return !m_appDir.isEmpty() && !m_logDir.isEmpty();
}

bool VersionManagerImpl::WriteVersionList() {
    QDir appDir(m_appDir);
    QDir versionDir(m_logDir + "/versionList");
    if (!versionDir.exists()) {
        QDir().mkpath(versionDir.absolutePath());
    }

    QJsonArray modules;
    const QFileInfoList files = appDir.entryInfoList(QStringList() << "*.exe" << "*.dll", QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
        QJsonObject module;
        module.insert("name", fileInfo.fileName());
        module.insert("path", QDir::fromNativeSeparators(fileInfo.absoluteFilePath()));
        module.insert("fileVersion", ReadFileVersion(fileInfo.absoluteFilePath()));
        module.insert("size", static_cast<double>(fileInfo.size()));
        module.insert("lastModified", fileInfo.lastModified().toString(Qt::ISODate));
        modules.append(module);
    }

    QJsonObject root;
    root.insert("generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("appDir", QDir::fromNativeSeparators(m_appDir));
    root.insert("modules", modules);

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_lastPath = versionDir.filePath(QString("versionList_%1.json").arg(stamp));
    QFile file(m_lastPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_lastPathUtf8 = QDir::fromNativeSeparators(m_lastPath).toUtf8();
    return true;
}

const char* VersionManagerImpl::GetLastVersionListPath() const {
    return m_lastPathUtf8.constData();
}

const char* VersionManagerImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void VersionManagerImpl::Shutdown() {
    m_appDir.clear();
    m_logDir.clear();
    m_lastPath.clear();
    m_lastPathUtf8.clear();
}

QString VersionManagerImpl::ReadFileVersion(const QString& filePath) const {
    DWORD handle = 0;
    const std::wstring nativePath = QDir::toNativeSeparators(filePath).toStdWString();
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &handle);
    if (size == 0) {
        return QString();
    }

    QByteArray data(static_cast<int>(size), 0);
    if (!GetFileVersionInfoW(nativePath.c_str(), handle, size, data.data())) {
        return QString();
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || !info) {
        return QString();
    }

    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
}

extern "C" MEYERSCAN_VERSIONMANAGER_API IVersionManager* GetVersionManager() {
    return &VersionManagerImpl::Instance();
}
