#pragma once

#include "Logger.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QWidget>

namespace MeyerQtModule {

// 返回 MeyerScan.exe 所在目录。
// 这里必须使用 QApplication/QCoreApplication 的程序目录，不允许使用 currentPath()。
inline QString AppDir() {
    return QCoreApplication::applicationDirPath();
}

// 返回单个模块在运行目录中的资源根目录。
// 运行目录约定：
//   MeyerScan.exe
//   Resources/Modules/<moduleName>/icon
//   Resources/Modules/<moduleName>/qss
// moduleName 使用项目文件夹名，例如 MyHomeUI。
inline QString ModuleResourceDir(const QString& moduleName) {
    return QDir(AppDir()).filePath(QString("Resources/Modules/%1").arg(moduleName));
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
