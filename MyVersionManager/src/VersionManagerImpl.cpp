#include "VersionManagerImpl.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <windows.h>

namespace {
namespace ModuleInfo {
// 模块名用于日志和工程识别，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_VersionManager";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_VersionManager v0.1.0 (2026-06-23)";
}

const char* kDefaultVersionModules[] = {
    "MeyerScan.exe",
    "MeyerLoginWidget.dll",
    "MeyerScan_Logger.dll",
    "MeyerScan_Database.dll",
    "MeyerScan_ConfigCenter.dll",
    "MeyerScan_Permission.dll",
    "MeyerScan_UIComponents.dll",
    "MeyerScan_HomeUI.dll",
    "MeyerScan_CaseUI.dll",
    "MeyerScan_CaseOrderService.dll",
    "MeyerScan_OrderScanWorkspaceShell.dll",
    "MeyerScan_Calibration3DUI.dll",
    "MeyerScan_CalibrationColorUI.dll",
};
}

// 返回版本清单模块单例。
// 当前模块只保存路径和最近一次输出结果，使用进程内单例可以避免重复状态。
VersionManagerImpl& VersionManagerImpl::Instance() {
    static VersionManagerImpl instance;
    return instance;
}

// 初始化版本清单模块。
// 这里不创建文件，只保存路径；真正写文件发生在 WriteVersionList()。
bool VersionManagerImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // appDir 是需要扫描 EXE/DLL 的目录，logDir 是版本清单输出目录。
    // 二者都由 MainExe 基于 applicationDirPath() 传入，不从 currentPath 推导。
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QString::fromUtf8(logDirUtf8 ? logDirUtf8 : "");

    // 两个路径缺一不可。返回 false 让调用方记录版本清单不可用，而不是写到错误目录。
    return !m_appDir.isEmpty() && !m_logDir.isEmpty();
}

// 写出版本清单。
// 历史骨架也改为读取 config/version_modules.json，只记录 MeyerScan 拆分模块。
bool VersionManagerImpl::WriteVersionList() {
    // 运行目录里会放 Qt、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方 DLL。
    // 因此不能再用 "*.dll/*.exe" 目录枚举，必须按 manifest 精确记录拆分模块。
    QDir appDir(m_appDir);
    QDir versionDir(m_logDir + "/versionList");
    if (!versionDir.exists()) {
        // versionList 目录不存在时主动创建，避免启动前必须由安装包预建。
        QDir().mkpath(versionDir.absolutePath());
    }

    const QString manifestPath = appDir.filePath("config/version_modules.json");
    EnsureDefaultVersionManifest(manifestPath);
    const QStringList moduleFiles = LoadVersionManifest(manifestPath);

    QJsonArray modules;
    for (const QString& moduleFile : moduleFiles) {
        const QFileInfo fileInfo(appDir.filePath(moduleFile));
        QJsonObject module;
        // 缺失模块也写入清单，便于安装包阶段发现漏复制。
        module.insert("name", moduleFile);
        module.insert("path", QDir::fromNativeSeparators(fileInfo.absoluteFilePath()));
        module.insert("exists", fileInfo.exists());
        if (fileInfo.exists()) {
            // 每个文件只记录现场排查最常用的信息：文件名、路径、版本、大小、修改时间。
            module.insert("fileVersion", ReadFileVersion(fileInfo.absoluteFilePath()));
            module.insert("size", static_cast<double>(fileInfo.size()));
            module.insert("lastModified", fileInfo.lastModified().toString(Qt::ISODate));
        } else {
            module.insert("fileVersion", QString());
            module.insert("size", 0);
            module.insert("lastModified", QString());
        }
        modules.append(module);
    }

    QJsonObject root;
    root.insert("generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("appDir", QDir::fromNativeSeparators(m_appDir));
    root.insert("generator", QString::fromLatin1(ModuleInfo::Version));
    root.insert("manifest", QDir::fromNativeSeparators(manifestPath));
    root.insert("modules", modules);

    // 文件名带时间戳，保留多次启动记录，便于对比现场某次启动加载了哪些 DLL。
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_lastPath = versionDir.filePath(QString("versionList_%1.json").arg(stamp));
    QFile file(m_lastPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    // 使用缩进格式写出，人工打开也能直接阅读。
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    // 缓存 UTF-8 路径，供 GetLastVersionListPath() 返回 const char*。
    m_lastPathUtf8 = QDir::fromNativeSeparators(m_lastPath).toUtf8();
    return true;
}

// 返回最近一次版本清单路径。
// 返回的是 QByteArray 内部缓存指针，因此 Shutdown 后不能继续使用。
const char* VersionManagerImpl::GetLastVersionListPath() const {
    return m_lastPathUtf8.constData();
}

// 返回模块版本字符串。
const char* VersionManagerImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 清空路径缓存。
void VersionManagerImpl::Shutdown() {
    // 清理路径缓存，不删除已生成的清单文件。
    // 版本清单属于运行记录，退出时应保留给现场排查。
    m_appDir.clear();
    m_logDir.clear();
    m_lastPath.clear();
    m_lastPathUtf8.clear();
}

// 读取 Windows 文件版本资源。
// 第三方 DLL 可能没有版本资源，这种情况下返回空字符串即可。
QString VersionManagerImpl::ReadFileVersion(const QString& filePath) const {
    DWORD handle = 0;

    // Windows 版本资源 API 使用宽字符路径，先把 Qt 路径转成本机分隔符和 std::wstring。
    const std::wstring nativePath = QDir::toNativeSeparators(filePath).toStdWString();
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &handle);
    if (size == 0) {
        // 文件没有版本资源或读取失败时返回空字符串，不影响整个清单生成。
        return QString();
    }

    // 版本资源大小由系统 API 返回，按字节分配缓冲区。
    QByteArray data(static_cast<int>(size), 0);
    if (!GetFileVersionInfoW(nativePath.c_str(), handle, size, data.data())) {
        return QString();
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    // "\\" 表示读取根版本信息块，里面包含四段 FileVersion。
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || !info) {
        return QString();
    }

    // dwFileVersionMS/LS 各包含两个 16 位段，组合成 a.b.c.d 字符串。
    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
}

// 读取版本清单 manifest。
// JSON 内部不允许写注释，字段说明应放在同目录 version_modules.md。
QStringList VersionManagerImpl::LoadVersionManifest(const QString& manifestPath) const {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringList();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return QStringList();
    }

    QStringList modules;
    const QJsonArray items = document.object().value("modules").toArray();
    for (const QJsonValue& item : items) {
        if (item.isString()) {
            const QString fileName = item.toString().trimmed();
            if (!fileName.isEmpty()) {
                modules.append(fileName);
            }
        } else if (item.isObject()) {
            const QString fileName = item.toObject().value("file").toString().trimmed();
            if (!fileName.isEmpty()) {
                modules.append(fileName);
            }
        }
    }
    return modules;
}

// 写入默认拆分模块 manifest。
// 默认列表与 MainExe 保持一致，避免历史 VersionManager 恢复使用时口径漂移。
void VersionManagerImpl::EnsureDefaultVersionManifest(const QString& manifestPath) const {
    if (QFileInfo::exists(manifestPath)) {
        return;
    }

    QDir dir(QFileInfo(manifestPath).absolutePath());
    if (!dir.exists()) {
        QDir().mkpath(dir.absolutePath());
    }

    QJsonArray modules;
    for (const char* moduleName : kDefaultVersionModules) {
        modules.append(QString::fromLatin1(moduleName));
    }

    QJsonObject root;
    root.insert("description", "MeyerScan split modules recorded in startup versionList");
    root.insert("modules", modules);

    QFile file(manifestPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

// 导出版本清单模块接口。
// 保留该历史骨架，后续版本清单再次复杂化时可恢复由独立 DLL 负责。
extern "C" MEYERSCAN_VERSIONMANAGER_API IVersionManager* GetVersionManager() {
    return &VersionManagerImpl::Instance();
}
