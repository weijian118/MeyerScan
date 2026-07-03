#include "VersionManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include <cstring>

namespace {

void WriteVersionManifest(const QString& appDir) {
    QDir dir(appDir);
    dir.mkpath("config");
    QFile file(dir.filePath("config/version_modules.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        // 只记录拆分模块，不把 Qt/VC/第三方 DLL 混入版本清单。
        stream << "{\n"
               << "  \"modules\": [\n"
               << "    \"MeyerScan_VersionManager.dll\",\n"
               << "    \"Missing_Test_Module.dll\"\n"
               << "  ]\n"
               << "}\n";
    }
}

bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);
    WriteVersionManifest(appDir);

    IVersionManager* manager = GetVersionManager();
    if (!Check(manager != nullptr, "VersionManager 工厂函数返回有效实例")) {
        return 1;
    }
    if (!Check(manager->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "VersionManager 初始化成功")) {
        return 2;
    }
    if (!Check(manager->WriteVersionList(), "VersionManager 能写出版本清单")) {
        return 3;
    }

    const char* lastPath = manager->GetLastVersionListPath();
    if (!Check(lastPath != nullptr && std::strlen(lastPath) > 0, "最近一次版本清单路径非空")) {
        return 4;
    }
    const QString versionListPath = QString::fromUtf8(lastPath);
    if (!Check(QFile::exists(versionListPath), "版本清单文件真实存在")) {
        return 5;
    }

    QFile file(versionListPath);
    if (!Check(file.open(QIODevice::ReadOnly), "版本清单可读")) {
        return 6;
    }
    const QByteArray content = file.readAll();
    if (!Check(content.contains("MeyerScan_VersionManager.dll") && content.contains("Missing_Test_Module.dll"),
               "版本清单包含 manifest 中声明的模块")) {
        return 7;
    }

    manager->Shutdown();
    std::printf("VersionManagerTest passed.\n");
    return 0;
}
