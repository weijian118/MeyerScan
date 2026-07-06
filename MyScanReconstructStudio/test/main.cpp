#include "ScanReconstructStudioWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <cstdio>

namespace {

// Returns the command-line value after a named argument.
QString ValueAfterArgument(const QStringList& arguments, const QString& name) {
    const int index = arguments.indexOf(name);
    if (index >= 0 && index + 1 < arguments.size()) {
        return arguments.at(index + 1);
    }
    return QString();
}

// Reads optional session JSON from disk.
QByteArray ReadContextJson(const QString& path) {
    if (path.isEmpty()) {
        return "{\"source\":\"standalone\",\"orderId\":\"demo-order\",\"sessionId\":\"standalone-scan\"}";
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

}

// Entry point for ScanReconstructStudio.exe.
int main(int argc, char* argv[]) {
    // High-DPI attributes must be set before QApplication is created.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // QVTKWidget is a QWidget/OpenGL object, so QApplication is required.
    QApplication app(argc, argv);

    // Derive paths from the executable folder, not from currentPath().
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    const QStringList arguments = QCoreApplication::arguments();
    const bool smoke = arguments.contains("--smoke");
    const QString contextPath = ValueAfterArgument(arguments, "--context");
    const QByteArray contextJson = ReadContextJson(contextPath);

    if (contextJson.isEmpty()) {
        std::fprintf(stderr, "Failed to read context json: %s\n", contextPath.toLocal8Bit().constData());
        return 2;
    }

    ScanReconstructStudioWindow window(appDir, logDir, contextJson);

    if (smoke) {
        if (!window.RunSmoke()) {
            std::fprintf(stderr, "ScanReconstructStudio smoke failed.\n");
            return 3;
        }
        std::printf("ScanReconstructStudio smoke passed.\n");
        return 0;
    }

    if (!window.Initialize()) {
        std::fprintf(stderr, "ScanReconstructStudio initialize failed.\n");
        return 4;
    }

    window.resize(1440, 860);
    window.show();
    return app.exec();
}
