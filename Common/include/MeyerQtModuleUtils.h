#pragma once

#include "Logger.h"
#include "MeyerUiResourceContract.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLibrary>
#include <QString>
#include <QWidget>

namespace MeyerQtModule {

// 返回 MeyerScan.exe 所在目录。
// 这里必须使用 QApplication/QCoreApplication 的程序目录，不允许使用 currentPath()。
inline QString AppDir() {
    return QCoreApplication::applicationDirPath();
}

// 尝试加载并注册 MeyerScan_UIResources.dll。
//
// 公共头是 inline 实现，因此每个 UI DLL 内部都会保留一份很小的加载状态。
// Resource DLL 的初始化接口本身是幂等的，并且使用 PreventUnloadHint 保持到进程退出，
// 所以多个 UI 模块先后调用不会重复注册或出现函数指针悬空。
inline bool EnsureUiResourcesLoaded() {
    const QString resourceRoot = QString::fromLatin1(MEYER_UI_RESOURCE_RUNTIME_ROOT);
    if (QDir(resourceRoot).exists()) {
        // 其它模块或 MainExe 已经完成注册时直接复用全局 Qt 资源树。
        return true;
    }

    typedef bool (*InitializeUiResourcesFunc)();
    typedef int (*ReadUiResourceIntegerFunc)();
    typedef const char* (*ReadUiResourceTextFunc)();
    static bool attempted = false;
    static bool initialized = false;
    static QLibrary* resourceLibrary = nullptr;
    if (attempted) {
        return initialized;
    }
    attempted = true;

    // 使用 applicationDirPath 定位 DLL，第三方软件改变 currentPath 不会影响加载结果。
    const QString libraryPath = QDir(AppDir()).filePath("MeyerScan_UIResources.dll");
    resourceLibrary = new QLibrary(libraryPath);
    resourceLibrary->setLoadHints(QLibrary::PreventUnloadHint);
    if (!resourceLibrary->load()) {
        // 加载失败时不抛异常，ModuleResourceDir 会继续尝试安装目录和源码目录降级。
        return false;
    }

    // 初始化前先校验资源加载合同。仅有同名 DLL 不代表它使用相同的 RCDATA
    // 编号、qrc 路径或清单结构；严格校验可以在页面创建前发现错误覆盖。
    const ReadUiResourceIntegerFunc readApiVersion =
        reinterpret_cast<ReadUiResourceIntegerFunc>(
            resourceLibrary->resolve("GetMeyerUiResourcesApiVersion"));
    const ReadUiResourceIntegerFunc readPayloadId =
        reinterpret_cast<ReadUiResourceIntegerFunc>(
            resourceLibrary->resolve("GetMeyerUiResourcesPayloadId"));
    const ReadUiResourceIntegerFunc readManifestSchema =
        reinterpret_cast<ReadUiResourceIntegerFunc>(
            resourceLibrary->resolve("GetMeyerUiResourcesManifestSchemaVersion"));
    const ReadUiResourceTextFunc readPrefix =
        reinterpret_cast<ReadUiResourceTextFunc>(
            resourceLibrary->resolve("GetMeyerUiResourcesPrefix"));
    if (!readApiVersion || !readPayloadId || !readManifestSchema || !readPrefix ||
        readApiVersion() != MEYER_UI_RESOURCE_API_VERSION ||
        readPayloadId() != MEYER_UI_RESOURCE_PAYLOAD_ID ||
        readManifestSchema() != MEYER_UI_RESOURCE_MANIFEST_SCHEMA_VERSION ||
        QString::fromLatin1(readPrefix()) !=
            QString::fromLatin1(MEYER_UI_RESOURCE_QRC_PREFIX)) {
        return false;
    }

    const InitializeUiResourcesFunc initialize =
        reinterpret_cast<InitializeUiResourcesFunc>(
            resourceLibrary->resolve("MeyerScanInitializeUiResources"));
    if (!initialize) {
        return false;
    }

    initialized = initialize() && QDir(resourceRoot).exists();
    return initialized;
}

// 在源码树中定位模块 Resources，供单模块开发调试降级使用。
// 正式安装目录没有仓库级 CMakeLists.txt，因此不会误把客户目录识别为源码树。
inline QString DevelopmentModuleResourceDir(const QString& moduleName) {
    QDir cursor(AppDir());
    for (int level = 0; level < 7; ++level) {
        const QString repositoryCandidate = cursor.filePath(QString("%1/Resources").arg(moduleName));
        if (QFileInfo::exists(cursor.filePath("CMakeLists.txt")) &&
            QDir(repositoryCandidate).exists()) {
            return QDir::cleanPath(repositoryCandidate);
        }

        // 单模块输出通常位于 <模块>/bin/Release；走到模块根时直接检查 Resources。
        const QString moduleCandidate = cursor.filePath("Resources");
        if (cursor.dirName().compare(moduleName, Qt::CaseInsensitive) == 0 &&
            QDir(moduleCandidate).exists()) {
            return QDir::cleanPath(moduleCandidate);
        }

        if (!cursor.cdUp()) {
            break;
        }
    }
    return QString();
}

// 返回单个模块的资源根目录。
// 优先级：资源 DLL -> 源码树开发降级 -> 旧安装目录散文件兼容。
// moduleName 使用项目文件夹名，例如 MyHomeUI。
inline QString ModuleResourceDir(const QString& moduleName) {
    EnsureUiResourcesLoaded();

    const QString embeddedPath = QString(":/MeyerScan/Modules/%1").arg(moduleName);
    if (QDir(embeddedPath).exists()) {
        return embeddedPath;
    }

    // 开发环境优先读取源码资源，避免输出目录残留的旧散文件遮盖刚修改的 QSS。
    // 正式安装目录不存在仓库 CMakeLists.txt，因此不会进入该分支。
    const QString developmentPath = DevelopmentModuleResourceDir(moduleName);
    if (!developmentPath.isEmpty()) {
        return developmentPath;
    }

    // 保留一段安装兼容期：旧安装包仍可能把资源作为散文件放在 exe 同级目录。
    const QString installedPath = QDir(AppDir()).filePath(
        QString("Resources/Modules/%1").arg(moduleName));
    return installedPath;
}

// 返回模块资源子目录中的具体文件路径。
inline QString ModuleResourceFile(const QString& moduleName,
                                  const QString& subDir,
                                  const QString& fileName) {
    return QDir(ModuleResourceDir(moduleName)).filePath(QDir(subDir).filePath(fileName));
}

// Qt 模块日志便捷函数。
// 模块名由 MEYER_MODULE_NAME 自动填入，调用方只需要传操作名和 QString 内容。
inline void WriteQtLog(ILogger* logger,
                       LogLevel level,
                       const char* operation,
                       const QString& content) {
    if (!logger) {
        return;
    }
    const QByteArray moduleBytes = QString::fromLatin1(MEYER_MODULE_NAME).toUtf8();
    const QByteArray operationBytes = QString::fromLatin1(operation ? operation : "").toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    logger->Write(level,
                  moduleBytes.constData(),
                  operationBytes.constData(),
                  "",
                  "",
                  "",
                  contentBytes.constData());
}

// operation 也支持 QString，方便纯 Qt 调用点直接传入文本。
inline void WriteQtLog(ILogger* logger,
                       LogLevel level,
                       const QString& operation,
                       const QString& content) {
    const QByteArray operationBytes = operation.toUtf8();
    WriteQtLog(logger, level, operationBytes.constData(), content);
}

// 操作名不重要时使用默认 Message 操作，降低普通日志调用成本。
inline void WriteQtLog(ILogger* logger, LogLevel level, const QString& content) {
    WriteQtLog(logger, level, "Message", content);
}

// 最短日志重载，默认 Info 级别和 Message 操作。
inline void WriteQtLog(ILogger* logger, const QString& content) {
    WriteQtLog(logger, LogLevel::Info, "Message", content);
}

// 应用 QSS 前替换路径占位符。
// QSS 字符串从内存加载，url(...) 相对路径有时会受进程工作目录影响。
// 使用占位符可以把图片资源固定到 MeyerScan.exe 所在目录，避免第三方拉起时路径跑偏。
inline QString ExpandQssPlaceholders(QString qss, const QString& moduleName) {
    const QString appDir = QDir::fromNativeSeparators(AppDir());
    const QString moduleResourceDir = QDir::fromNativeSeparators(ModuleResourceDir(moduleName));
    qss.replace("@MEYER_APP_DIR@", appDir);
    qss.replace("@MEYER_MODULE_RESOURCE_DIR@", moduleResourceDir);
    return qss;
}

// 从 Resources/Modules/<moduleName>/qss/<qssFileName> 加载 QSS 并应用到根控件。
// 这是源码中唯一允许调用 setStyleSheet() 的入口，其它模块只调用本函数。
// QSS 缺失不是致命错误，调用方应继续用 Qt 默认样式运行并在日志中定位问题。
inline bool ApplyModuleQss(QWidget* root,
                           const QString& moduleName,
                           const QString& qssFileName,
                           ILogger* logger = nullptr) {
    if (!root) {
        WriteQtLog(logger, LogLevel::Warning, "ApplyModuleQss", "Root widget is null");
        return false;
    }

    const QString qssPath = ModuleResourceFile(moduleName, "qss", qssFileName);
    QFile file(qssPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        WriteQtLog(logger,
                   LogLevel::Warning,
                   "ApplyModuleQss",
                   QString("QSS file unavailable: %1").arg(QDir::fromNativeSeparators(qssPath)));
        return false;
    }

    const QString qss = ExpandQssPlaceholders(QString::fromUtf8(file.readAll()), moduleName);
    root->setStyleSheet(qss);
    WriteQtLog(logger,
               LogLevel::Info,
               "ApplyModuleQss",
               QString("QSS loaded: %1").arg(QDir::fromNativeSeparators(qssPath)));
    return true;
}

} // namespace MeyerQtModule
