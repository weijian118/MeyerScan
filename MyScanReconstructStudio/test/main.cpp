#include "ScanReconstructStudioWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <cstdio>

namespace {

// 返回命令行指定参数后的一个值。
// 找不到参数或参数位于末尾时返回空字符串，调用方决定是否使用默认值。
QString ValueAfterArgument(const QStringList& arguments, const QString& name) {
    const int index = arguments.indexOf(name);
    if (index >= 0 && index + 1 < arguments.size()) {
        return arguments.at(index + 1);
    }
    return QString();
}

// 从磁盘读取可选会话 JSON。
// path 为空时返回内置最小合法上下文，让双击和 smoke 都不依赖开发机固定文件。
QByteArray ReadContextJson(const QString& path) {
    if (path.isEmpty()) {
        return "{\"source\":\"standalone\",\"orderId\":\"demo-order\",\"sessionId\":\"standalone-scan\"}";
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // 返回空数组作为明确失败信号，错误提示和退出码由 main 统一处理。
        return QByteArray();
    }
    return file.readAll();
}

}

// ScanReconstructStudio.exe 入口，同时承担独立程序和 smoke 测试宿主。
int main(int argc, char* argv[]) {
    // 高 DPI 属性必须在 QApplication 构造前设置，否则 Qt 已经锁定缩放策略。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // QVTKWidget 是 QWidget/OpenGL 对象，不能使用 QCoreApplication 代替。
    QApplication app(argc, argv);

    // 日志目录从 EXE 所在目录推导，不使用第三方启动器可改变的 currentPath。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    // 参数只解析一次；--context 后的路径可以来自第三方或人工测试。
    const QStringList arguments = QCoreApplication::arguments();
    const bool smoke = arguments.contains("--smoke");
    const QString contextPath = ValueAfterArgument(arguments, "--context");
    const QByteArray contextJson = ReadContextJson(contextPath);

    if (contextJson.isEmpty()) {
        // 文件读取失败时不创建壳窗口，避免后续子模块收到空的来源不明上下文。
        std::fprintf(stderr, "Failed to read context json: %s\n", contextPath.toLocal8Bit().constData());
        return 2;
    }

    // 窗口在栈上创建，main 返回时析构函数会按顺序释放当前 QVTK 页面和子模块。
    ScanReconstructStudioWindow window(appDir, logDir, contextJson);

    if (smoke) {
        // RunSmoke 复用真实 DLL 加载、Scan -> Process -> Scan 切换和释放路径。
        if (!window.RunSmoke()) {
            std::fprintf(stderr, "ScanReconstructStudio smoke failed.\n");
            return 3;
        }
        std::printf("ScanReconstructStudio smoke passed.\n");
        return 0;
    }

    if (!window.Initialize()) {
        // Initialize 只有在两个子 DLL、初始扫描页和上下文均成功时才返回 true。
        std::fprintf(stderr, "ScanReconstructStudio initialize failed.\n");
        return 4;
    }

    // 独立 EXE 的第一屏必须全屏无边框；窗口标志由 ScanReconstructStudioWindow 构造函数设置。
    window.showFullScreen();
    return app.exec();
}
