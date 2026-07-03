#include "OrderScanWorkspaceShell.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
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

    IOrderScanWorkspaceShell* shell = GetOrderScanWorkspaceShell();
    if (!Check(shell != nullptr, "OrderScanWorkspaceShell 工厂函数返回有效实例")) {
        return 1;
    }
    if (!Check(shell->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "OrderScanWorkspaceShell 初始化成功")) {
        return 2;
    }

    QWidget* widget = shell->CreateWidget();
    if (!Check(widget != nullptr, "工作区壳子能创建根 QWidget")) {
        return 3;
    }
    if (!Check(widget->objectName() == "MeyerScanOrderScanWorkspaceShellRoot", "工作区壳子根对象名正确")) {
        return 4;
    }

    // 挂载扫描页占位控件，验证真实建单/扫描模块后续能以 QWidget 形式接入壳子。
    QLabel* scanPage = new QLabel("Scan Page", widget);
    scanPage->setObjectName("OrderScanWorkspaceShellTestScanPage");
    shell->AttachStepWidget(WorkspaceStepScan, scanPage);
    shell->SetStep(WorkspaceStepScan);
    shell->SetStep(WorkspaceStepProcess);
    // 非法 step 只应写 Warning 日志，不应导致崩溃。
    shell->SetStep(99);

    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1180, 760);
        widget->show();
        return app.exec();
    }

    delete widget;
    shell->Shutdown();
    std::printf("OrderScanWorkspaceShellTest passed.\n");
    return 0;
}
