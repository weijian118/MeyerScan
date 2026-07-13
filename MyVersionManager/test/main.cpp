#include "VersionManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cstdio>
#include <cstring>

namespace {

// 写入测试专用的版本模块清单。
// VersionManager 的设计是“先读 config/version_modules.json，再按清单输出版本信息”，
// 所以测试宿主需要先造一个最小配置文件，避免依赖人工提前拷贝配置。
bool WriteVersionManifest(const QString& appDir) {
    // QDir 使用应用程序所在目录作为根目录，避免 currentPath 被 VS 或第三方启动方式影响。
    QDir dir(appDir);
    // mkpath 会递归创建 config 目录；目录已存在时返回 true，不会破坏旧文件。
    dir.mkpath("config");
    // filePath 负责拼接路径分隔符，比手写 "\\" 更不容易出错。
    QFile file(dir.filePath("config/version_modules.json"));
    // WriteOnly + Truncate 表示每次测试都覆盖旧清单，保证测试结果可重复。
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        // QTextStream 用来写 UTF-8 文本，避免中文路径或注释说明在不同机器上编码漂移。
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        // 只记录拆分模块，不把 Qt/VC/第三方 DLL 混入版本清单。
        stream << "{\n"
               << "  \"schemaVersion\": 2,\n"
               << "  \"modules\": [\n"
               << "    {\n"
               << "      \"file\": \"MeyerScan_VersionManager.dll\",\n"
               << "      \"versionFunction\": \"GetMeyerModuleVersion\"\n"
               << "    },\n"
               << "    {\n"
               << "      \"file\": \"Missing_Test_Module.dll\",\n"
               << "      \"versionFunction\": \"GetMeyerModuleVersion\"\n"
               << "    }\n"
               << "  ]\n"
               << "}\n";
        return true;
    }
    return false;
}

// 简单断言工具。
// 测试项目不用引入复杂测试框架，直接用返回码区分失败阶段，便于 VS2015 和命令行同时使用。
bool Check(bool condition, const char* message) {
    // 通过时写 stdout，方便批量回归时快速看到每个检查点。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败时写 stderr，CI 或脚本可以单独收集错误输出。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// VersionManager 测试入口：验证 manifest 驱动的版本清单生成和输出路径返回。
int main(int argc, char* argv[]) {
    // QCoreApplication 提供 applicationDirPath、arguments 等 Qt 基础能力；
    // 本测试不创建 QWidget，因此不需要 QApplication。
    QCoreApplication app(argc, argv);

    // appDir 固定为 exe 所在目录，用来定位测试依赖和创建隔离测试根目录。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 测试不能写 appDir/config，否则根聚合输出中的正式 version_modules.json 会被两项测试清单覆盖。
    // 这里创建与正式目录结构相同、但完全隔离的 test_runtime/VersionManagerTest。
    const QString testAppDir = QDir(appDir).filePath("test_runtime/VersionManagerTest");
    const QString logDir = QDir(testAppDir).filePath("logs");
    QDir().mkpath(logDir);
    // 版本模块是 C ABI，两个命名缓冲区让路径指针生命周期覆盖 Init 调用。
    const QByteArray testAppDirUtf8 = testAppDir.toUtf8();
    const QByteArray logDirUtf8 = logDir.toUtf8();

    // VersionManager 按测试 appDir 查找清单中的 DLL，因此复制一份自研模块到隔离根目录。
    // QFile::remove 只删除明确的单个测试文件，不递归清理目录，重复执行也能得到同一结果。
    const QString sourceModule = QDir(appDir).filePath("MeyerScan_VersionManager.dll");
    const QString testModule = QDir(testAppDir).filePath("MeyerScan_VersionManager.dll");
    QFile::remove(testModule);
    if (!Check(QFile::copy(sourceModule, testModule), "测试模块复制到隔离目录")) {
        return 10;
    }

    // 先写测试专用 manifest，再初始化模块；任何时候都不触碰 MainExe 正式配置。
    if (!Check(WriteVersionManifest(testAppDir), "测试版本清单写入隔离目录")) {
        return 11;
    }

    // 工厂函数是 DLL 对外唯一入口；先验证入口有效，再测试具体功能。
    IVersionManager* manager = GetVersionManager();
    if (!Check(manager != nullptr, "VersionManager 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 接收应用目录和日志目录，内部会保存这些路径供后续 WriteVersionList 使用。
    if (!Check(manager->Init(testAppDirUtf8.constData(), logDirUtf8.constData()),
               "VersionManager 初始化成功")) {
        return 2;
    }
    // WriteVersionList 会读取 manifest，扫描文件版本，并写出带时间戳的 json 文件。
    if (!Check(manager->WriteVersionList(), "VersionManager 能写出版本清单")) {
        return 3;
    }

    // 模块把最近一次输出路径保存在内部，便于 MainExe 或测试宿主定位结果文件。
    const char* lastPath = manager->GetLastVersionListPath();
    if (!Check(lastPath != nullptr && std::strlen(lastPath) > 0, "最近一次版本清单路径非空")) {
        return 4;
    }
    // DLL 接口返回 const char*，测试侧立即转成 QString，方便用 Qt 文件 API 检查。
    const QString versionListPath = QString::fromUtf8(lastPath);
    if (!Check(QFile::exists(versionListPath), "版本清单文件真实存在")) {
        return 5;
    }

    // 重新打开输出文件，验证写出的不是空路径，而是真实可读的版本清单。
    QFile file(versionListPath);
    if (!Check(file.open(QIODevice::ReadOnly), "版本清单可读")) {
        return 6;
    }
    // readAll 读取小型 json 文件即可；版本清单文件不应该很大。
    const QByteArray content = file.readAll();
    // 这里同时检查存在文件和缺失文件，确认 VersionManager 不会静默丢弃 manifest 中的条目。
    if (!Check(content.contains("MeyerScan_VersionManager.dll") && content.contains("Missing_Test_Module.dll"),
               "版本清单包含 manifest 中声明的模块")) {
        return 7;
    }
    // 解析 JSON 结构，验证历史 VersionManager 已经同步到 schemaVersion=2，
    // 并且输出 fileVersion/codeVersion/versionMatch 三类版本字段。
    const QJsonDocument document = QJsonDocument::fromJson(content);
    if (!Check(document.isObject() && document.object().value("schemaVersion").toInt() == 2,
               "版本清单 schemaVersion 为 2")) {
        return 8;
    }
    const QJsonArray modules = document.object().value("modules").toArray();
    QJsonObject selfModule;
    for (const QJsonValue& value : modules) {
        const QJsonObject object = value.toObject();
        if (object.value("name").toString() == "MeyerScan_VersionManager.dll") {
            selfModule = object;
            break;
        }
    }
    if (!Check(!selfModule.isEmpty()
                   && !selfModule.value("fileVersion").toString().isEmpty()
                   && !selfModule.value("codeVersion").toString().isEmpty()
                   && selfModule.value("versionMatch").toBool(),
               "版本清单记录文件版本、代码版本且二者一致")) {
        return 9;
    }

    // Shutdown 释放模块内部状态，防止后续同进程扩展测试时受到上一次状态影响。
    manager->Shutdown();
    std::printf("VersionManagerTest passed.\n");
    return 0;
}
