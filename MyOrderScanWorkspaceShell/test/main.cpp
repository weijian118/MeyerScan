#include "OrderScanWorkspaceShell.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QWidget>
#include <cstdio>

namespace {

// 测试检查函数。
// 当前壳子模块只需要基础冒烟测试，因此用轻量返回码即可。
bool Check(bool condition, const char* message) {
    // 成功输出 PASS，方便批量回归时看到执行进度。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败输出 FAIL，并由 main 返回对应错误码。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// OrderScanWorkspaceShell 测试入口：验证工作台壳初始化、步骤页面挂载和资源释放流程。
int main(int argc, char* argv[]) {
    // 工作区壳子会创建 QWidget，必须用 QApplication 初始化 GUI 环境。
    QApplication app(argc, argv);

    // appDir 使用 exe 所在目录，符合所有模块资源路径规范。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志目录与正式程序保持一致，便于壳子模块写日志。
    const QString logDir = QDir(appDir).filePath("logs");
    // 目录不存在时先创建，避免 Init 内部初始化日志失败。
    QDir().mkpath(logDir);

    // 工厂函数验证 DLL 导出和链接关系是否正确。
    IOrderScanWorkspaceShell* shell = GetOrderScanWorkspaceShell();
    if (!Check(shell != nullptr, "OrderScanWorkspaceShell 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 保存应用目录和日志目录，后续真实建单/扫描模块挂载时继续复用。
    if (!Check(shell->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "OrderScanWorkspaceShell 初始化成功")) {
        return 2;
    }

    // CreateWidget 创建完整工作区根控件，但不主动 show，调用方决定嵌入位置。
    QWidget* widget = shell->CreateWidget();
    if (!Check(widget != nullptr, "工作区壳子能创建根 QWidget")) {
        return 3;
    }
    // objectName 是样式表、自动化测试和人工调试的稳定标识。
    if (!Check(widget->objectName() == "MeyerScanOrderScanWorkspaceShellRoot", "工作区壳子根对象名正确")) {
        return 4;
    }

    // 挂载扫描页占位控件，验证真实建单/扫描模块后续能以 QWidget 形式接入壳子。
    // 这里把父对象设为 widget，利用 Qt 父子机制随根控件自动释放。
    QLabel* scanPage = new QLabel("Scan Page", widget);
    // 设置 objectName，方便人工用 Qt 对象树或样式调试工具定位该占位页。
    scanPage->setObjectName("OrderScanWorkspaceShellTestScanPage");
    // AttachStepWidget 把外部模块页面接入指定步骤槽位。
    shell->AttachStepWidget(WorkspaceStepScan, scanPage);
    // 切到扫描步骤，验证已挂载页面可以被选中显示。
    shell->SetStep(WorkspaceStepScan);
    // 切到处理步骤，验证未挂载步骤也不会崩溃。
    shell->SetStep(WorkspaceStepProcess);
    // 非法 step 只应写 Warning 日志，不应导致崩溃。
    shell->SetStep(99);

    // --show 用于人工查看壳子界面；默认不进入事件循环，便于自动化测试结束。
    if (QCoreApplication::arguments().contains("--show")) {
        // 使用接近主程序的窗口尺寸观察步骤区域布局。
        widget->resize(1180, 760);
        widget->show();
        return app.exec();
    }

    // 删除根控件会连带释放 scanPage，验证资源释放路径没有悬挂引用。
    delete widget;
    // Shutdown 释放壳子模块内部状态和日志引用。
    shell->Shutdown();
    std::printf("OrderScanWorkspaceShellTest passed.\n");
    return 0;
}
