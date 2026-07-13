#include "ExternalLaunchAdapter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <cstdio>
#include <vector>

namespace {

// 简单断言函数。
// 测试宿主不用引入 gtest，避免为小模块增加额外第三方依赖。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// 解析测试样例 JSON 路径。
// 同一个测试程序可能从单模块 bin/Release 或根聚合 bin/Release 运行，
// 所以按“输出目录 -> 单模块 test 目录 -> 从当前目录逐级向上搜索模块目录”的顺序查找。
QString ResolveSampleJsonPath(const QString& appDir) {
    // CMake/VS PostBuild 会把样例文件复制到 exe 同级目录。
    const QString directPath = QDir(appDir).filePath("external_order_sample.json");
    if (QFileInfo::exists(directPath)) {
        return directPath;
    }

    // 单模块输出目录下，../../test 通常能回到模块测试目录。
    const QString modulePath = QDir(appDir).filePath("../../test/external_order_sample.json");
    if (QFileInfo::exists(modulePath)) {
        return QFileInfo(modulePath).absoluteFilePath();
    }

    // 根聚合输出目录不一定复制样例文件时，从 exe 目录逐级向上找模块目录。
    // 这种写法不绑定 F:/MeyerScan，工程移动到其他盘符或其他电脑后仍能工作。
    QDir searchDir(appDir);
    for (int i = 0; i < 6; ++i) {
        const QString candidate = searchDir.filePath("MyExternalLaunchAdapter/test/external_order_sample.json");
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
        if (!searchDir.cdUp()) {
            break;
        }
    }
    return QString();
}

}

// ExternalLaunchAdapter 测试入口：验证第三方 JSON 文件能被归一化为标准建单上下文。
int main(int argc, char* argv[]) {
    // ExternalLaunchAdapter 只需要 Qt Core，不需要 QApplication 或图形事件循环。
    QCoreApplication app(argc, argv);

    // 测试路径基于 exe 所在目录，避免 currentPath 受 VS2015/命令行启动方式影响。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    // 测试日志写在输出目录下，避免污染源码目录。
    QDir().mkpath(logDir);
    // 适配器是 C ABI；命名 UTF-8 缓冲区覆盖 Init 调用，避免临时指针示范。
    const QByteArray appDirUtf8 = appDir.toUtf8();
    const QByteArray logDirUtf8 = logDir.toUtf8();

    // 通过 C ABI 工厂函数获取模块单例，模拟 MainExe 的真实调用方式。
    IExternalLaunchAdapter* adapter = GetExternalLaunchAdapter();
    if (!Check(adapter != nullptr, "factory returns adapter")) {
        return 1;
    }

    // Init 会缓存 appDir/logDir，并初始化 Logger 指针。
    if (!Check(adapter->Init(appDirUtf8.constData(), logDirUtf8.constData()),
               "adapter init ok")) {
        return 2;
    }

    const QString samplePath = ResolveSampleJsonPath(appDir);
    if (!Check(!samplePath.isEmpty(), "sample json found")) {
        return 3;
    }

    // 调用方分配输出缓冲区，模块只负责写入 UTF-8 JSON。
    // 16KB 足够当前测试样例；正式 MainExe 会在缓冲区不足时按 requiredBufferSize 扩容重试。
    std::vector<char> output(16 * 1024, '\0');
    ExternalLaunchResult result = {};
    // 文件路径转换结果需要活到 NormalizeOrderFile 返回；适配器在调用内同步读取它。
    const QByteArray samplePathUtf8 = samplePath.toUtf8();
    const bool normalizeOk = adapter->NormalizeOrderFile(samplePathUtf8.constData(),
                                                         "cmd_demo",
                                                         output.data(),
                                                         static_cast<int>(output.size()),
                                                         &result);
    if (!Check(normalizeOk, "normalize external json")) {
        std::fprintf(stderr, "errorCode=%d message=%s\n", result.errorCode, result.message);
        return 4;
    }

    // 输出缓冲区是 '\0' 结尾的 UTF-8 JSON，Qt 可以直接从 QByteArray 解析。
    const QJsonDocument normalizedDocument = QJsonDocument::fromJson(QByteArray(output.data()));
    if (!Check(normalizedDocument.isObject(), "output json is object")) {
        return 5;
    }

    // 验证 source/patient/order/scanPlan 四段关键字段，确保第三方类型和建单信息没有丢失。
    const QJsonObject root = normalizedDocument.object();
    const QJsonObject source = root.value("source").toObject();
    const QJsonObject patient = root.value("patient").toObject();
    const QJsonObject order = root.value("order").toObject();
    const QJsonObject scanPlan = root.value("scanPlan").toObject();

    if (!Check(source.value("launchType").toString() == "external", "launchType external")) {
        return 6;
    }
    if (!Check(source.value("thirdPartyType").toString() == "cmd_demo", "thirdPartyType kept")) {
        return 7;
    }
    if (!Check(patient.value("name").toString() == "External Patient", "patient mapped")) {
        return 8;
    }
    if (!Check(order.value("doctor").toString() == "Dr. External", "doctor mapped")) {
        return 9;
    }
    if (!Check(scanPlan.value("items").toArray().size() == 2, "scan plan item count")) {
        return 10;
    }

    // Shutdown 只清理适配器缓存，不负责关闭整个进程 Logger。
    adapter->Shutdown();
    std::printf("ExternalLaunchAdapterTest passed.\n");
    return 0;
}
