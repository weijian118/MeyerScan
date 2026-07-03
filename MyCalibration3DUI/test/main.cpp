#include "Calibration3DUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QWidget>
#include <cstdio>

namespace {

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
    QApplication app(argc, argv);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    ICalibration3DUI* calibration = GetCalibration3DUI();
    if (!Check(calibration != nullptr, "Calibration3DUI 工厂函数返回有效实例")) {
        return 1;
    }
    if (!Check(calibration->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "Calibration3DUI 初始化成功")) {
        return 2;
    }

    QWidget* widget = calibration->CreateWidget();
    if (!Check(widget != nullptr, "Calibration3DUI 能创建根 QWidget")) {
        return 3;
    }
    if (!Check(widget->objectName() == "MeyerScanCalibration3DUIRoot", "三维校准根对象名正确")) {
        return 4;
    }

    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1100, 720);
        widget->show();
        return app.exec();
    }

    delete widget;
    calibration->Shutdown();
    std::printf("Calibration3DUITest passed.\n");
    return 0;
}
