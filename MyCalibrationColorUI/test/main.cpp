#include "CalibrationColorUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QWidget>
#include <cstdio>

namespace {

// 测试断言辅助函数。
// 使用简单 C 输出和返回码，保证 VS2015、命令行、批处理脚本都能直接读取结果。
bool Check(bool condition, const char* message) {
    // 通过时写 stdout，作为测试步骤记录。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败时写 stderr，便于自动化脚本单独收集失败原因。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// 颜色校准 UI 测试入口：验证 DLL 工厂、初始化、根控件创建和可选人工显示流程。
int main(int argc, char* argv[]) {
    // 颜色校准模块创建 QWidget，必须使用 QApplication 初始化 Qt GUI 环境。
    QApplication app(argc, argv);

    // 所有路径都从测试 exe 所在目录推导，避免 currentPath 变化导致资源加载错误。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志目录与正式 MeyerScan.exe 保持同级 logs 结构。
    const QString logDir = QDir(appDir).filePath("logs");
    // 先创建日志目录，避免 Init 内部写首条日志时失败。
    QDir().mkpath(logDir);

    // 验证 DLL 导出的工厂函数能返回有效接口。
    ICalibrationColorUI* calibration = GetCalibrationColorUI();
    if (!Check(calibration != nullptr, "CalibrationColorUI 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 当前只打通路径和日志，后续颜色校准算法资源也从这里扩展。
    if (!Check(calibration->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "CalibrationColorUI 初始化成功")) {
        return 2;
    }

    // 由模块创建根 QWidget，调用方只负责嵌入或显示，保持 UI 资源边界清晰。
    QWidget* widget = calibration->CreateWidget();
    if (!Check(widget != nullptr, "CalibrationColorUI 能创建根 QWidget")) {
        return 3;
    }
    // objectName 是测试和样式定位的稳定锚点，不能随意修改。
    if (!Check(widget->objectName() == "MeyerScanCalibrationColorUIRoot", "颜色校准根对象名正确")) {
        return 4;
    }

    // 默认自动化模式不进入事件循环；传 --show 时才给人工查看窗口。
    if (QCoreApplication::arguments().contains("--show")) {
        // 使用固定尺寸模拟设置模块中较大的内容区域。
        widget->resize(1100, 720);
        widget->show();
        return app.exec();
    }

    // 自动化模式下手动释放根控件，保证模块可以被创建和销毁。
    delete widget;
    // 释放模块内部状态，后续接入算法资源时也从这里做清理。
    calibration->Shutdown();
    std::printf("CalibrationColorUITest passed.\n");
    return 0;
}
