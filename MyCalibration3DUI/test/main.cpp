#include "Calibration3DUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QWidget>
#include <cstdio>

namespace {

// 测试断言辅助函数。
// 这里不使用 QtTest，是为了保持测试宿主简单，VS2015 双击运行也能看懂输出。
bool Check(bool condition, const char* message) {
    // 通过时输出 PASS，便于批量测试日志中确认每个检查点。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败时输出 FAIL 并让 main 返回不同错误码，定位失败阶段更直接。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// 三维校准 UI 测试入口：验证 DLL 工厂、初始化、根控件创建和可选人工显示流程。
int main(int argc, char* argv[]) {
    // 三维校准模块会创建 QWidget，因此测试宿主必须使用 QApplication。
    QApplication app(argc, argv);

    // applicationDirPath 指向当前测试 exe 所在目录，符合所有模块路径获取规范。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志目录仍按正式程序结构放在 exe 同级 logs 下。
    const QString logDir = QDir(appDir).filePath("logs");
    // mkpath 在目录已存在时不会失败，适合测试启动前防御性创建目录。
    QDir().mkpath(logDir);

    // DLL 工厂函数返回接口对象，是测试模块链接和导出的第一步。
    ICalibration3DUI* calibration = GetCalibration3DUI();
    if (!Check(calibration != nullptr, "Calibration3DUI 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 接收应用目录和日志目录，后续真实算法/设备资源也应通过这些根路径查找。
    if (!Check(calibration->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "Calibration3DUI 初始化成功")) {
        return 2;
    }

    // CreateWidget 只创建 UI 根控件，不直接 show，由调用方决定嵌入设置模块还是单独显示。
    QWidget* widget = calibration->CreateWidget();
    if (!Check(widget != nullptr, "Calibration3DUI 能创建根 QWidget")) {
        return 3;
    }
    // 固定 objectName 方便设置模块查找、样式调试和自动化冒烟测试。
    if (!Check(widget->objectName() == "MeyerScanCalibration3DUIRoot", "三维校准根对象名正确")) {
        return 4;
    }

    // --show 用于人工验收界面；默认不显示，避免批量测试卡在事件循环。
    if (QCoreApplication::arguments().contains("--show")) {
        // 固定一个接近主界面的尺寸，便于人工观察布局是否合理。
        widget->resize(1100, 720);
        widget->show();
        return app.exec();
    }

    // 默认自动化模式下手动删除根控件，验证模块不依赖外部父对象回收。
    delete widget;
    // Shutdown 释放模块内部状态，为后续扩展算法资源释放测试预留入口。
    calibration->Shutdown();
    std::printf("Calibration3DUITest passed.\n");
    return 0;
}
