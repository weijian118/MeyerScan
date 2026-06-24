#pragma once

#include "VersionManager.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// VersionManagerImpl 读取 MeyerScan.exe 同级 config/version_modules.json，
// 只记录清单中声明的拆分模块 EXE/DLL，生成可排查、可追踪的版本清单 JSON。
// 该历史骨架不得再扫描运行目录全部 DLL，避免把 Qt/OpenSSL/AWS 等第三方库写入清单。
class VersionManagerImpl : public IVersionManager {
public:
    // 返回进程内单例。
    static VersionManagerImpl& Instance();

    // 初始化应用目录和日志目录。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 写出版本清单 JSON。
    bool WriteVersionList() override;

    // 获取最近一次版本清单文件路径。
    const char* GetLastVersionListPath() const override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 清空缓存。
    void Shutdown() override;

private:
    VersionManagerImpl() = default;
    ~VersionManagerImpl() = default;
    VersionManagerImpl(const VersionManagerImpl&) = delete;
    VersionManagerImpl& operator=(const VersionManagerImpl&) = delete;

    // 从 Windows 文件版本资源读取 FileVersion。
    // 没有版本资源时返回空字符串，不把它当作错误。
    QString ReadFileVersion(const QString& filePath) const;

    // 读取 config/version_modules.json 中声明的拆分模块文件。
    // 历史 VersionManager 也使用清单驱动，避免误扫 Qt/OpenSSL/AWS 等第三方库。
    QStringList LoadVersionManifest(const QString& manifestPath) const;

    // 首次运行缺少清单时写入默认拆分模块列表。
    // 后续新增模块只维护 JSON 清单，不改扫描代码。
    void EnsureDefaultVersionManifest(const QString& manifestPath) const;

private:
    // MeyerScan.exe 所在目录。
    QString m_appDir;

    // logs 目录。
    QString m_logDir;

    // 最近一次写出的版本清单路径。
    QString m_lastPath;

    // m_lastPath 的 UTF-8 缓存，保证 GetLastVersionListPath 返回指针稳定。
    QByteArray m_lastPathUtf8;
};
