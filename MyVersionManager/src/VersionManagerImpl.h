#pragma once

#include "VersionManager.h"

#include <QByteArray>
#include <QString>

class VersionManagerImpl : public IVersionManager {
public:
    static VersionManagerImpl& Instance();

    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    bool WriteVersionList() override;
    const char* GetLastVersionListPath() const override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    VersionManagerImpl() = default;
    ~VersionManagerImpl() = default;
    VersionManagerImpl(const VersionManagerImpl&) = delete;
    VersionManagerImpl& operator=(const VersionManagerImpl&) = delete;

    QString ReadFileVersion(const QString& filePath) const;

private:
    QString m_appDir;
    QString m_logDir;
    QString m_lastPath;
    QByteArray m_lastPathUtf8;
};
