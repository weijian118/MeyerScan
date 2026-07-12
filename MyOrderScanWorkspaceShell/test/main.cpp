#include "OrderScanWorkspaceShell.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidget>
#include <cstdio>

namespace {

// 记录壳子顶部窗口按钮最近一次上报的动作 ID。
// 测试宿主不真的最小化/关闭窗口，只验证 DLL 到调用方的回调链路。
int g_lastShellAction = 0;

void OnShellAction(void* /*context*/, int actionId) {
    g_lastShellAction = actionId;
}

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
    // DLL 接口使用 UTF-8 字节；两个命名缓冲区可在首次和重复 Init 时安全复用。
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();

    // 工厂函数验证 DLL 导出和链接关系是否正确。
    IOrderScanWorkspaceShell* shell = GetOrderScanWorkspaceShell();
    if (!Check(shell != nullptr, "OrderScanWorkspaceShell 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 保存应用目录和日志目录，后续真实建单/扫描模块挂载时继续复用。
    if (!Check(shell->Init(appDirBytes.constData(), logDirBytes.constData()),
               "OrderScanWorkspaceShell 初始化成功")) {
        return 2;
    }
    shell->SetShellActionCallback(&OnShellAction, nullptr);

    // CreateWidget 创建完整工作区根控件，但不主动 show，调用方决定嵌入位置。
    QWidget* widget = shell->CreateWidget();
    if (!Check(widget != nullptr, "工作区壳子能创建根 QWidget")) {
        return 3;
    }
    // objectName 是样式表、自动化测试和人工调试的稳定标识。
    if (!Check(widget->objectName() == "MeyerScanOrderScanWorkspaceShellRoot", "工作区壳子根对象名正确")) {
        return 4;
    }

    // 顶部返回按钮必须只上报动作 ID，不能由壳子直接操作 MainWindow。
    auto* backButton = widget->findChild<QToolButton*>("WorkspaceBackButton");
    if (!Check(backButton != nullptr, "能找到工作台返回按钮")) {
        return 17;
    }
    backButton->click();
    if (!Check(g_lastShellAction == WorkspaceShellActionBack, "返回按钮上报稳定动作 ID")) {
        return 18;
    }

    // 挂载扫描页占位控件，验证真实建单/扫描模块后续能以 QWidget 形式接入壳子。
    // 这里把父对象设为 widget，利用 Qt 父子机制随根控件自动释放。
    QLabel* scanPage = new QLabel(QObject::tr("Scan Page"), widget);
    // 设置 objectName，方便人工用 Qt 对象树或样式调试工具定位该占位页。
    scanPage->setObjectName("OrderScanWorkspaceShellTestScanPage");
    // AttachStepWidget 把外部模块页面接入指定步骤槽位。
    shell->AttachStepWidget(WorkspaceStepScan, scanPage);
    // 测试宿主通过对象名找到顶部按钮和页面栈，验证真实点击能触发切换。
    auto* scanButton = widget->findChild<QPushButton*>("WorkspaceStep2Button");
    if (!Check(scanButton != nullptr, "能找到 Scan 步骤按钮")) {
        return 5;
    }
    auto* processButton = widget->findChild<QPushButton*>("WorkspaceStep3Button");
    if (!Check(processButton != nullptr, "能找到 Process 步骤按钮")) {
        return 6;
    }
    auto* stack = widget->findChild<QStackedWidget*>();
    if (!Check(stack != nullptr, "能找到工作区页面栈")) {
        return 7;
    }
    // 切到扫描步骤，验证已挂载页面可以被选中显示。
    scanButton->click();
    if (!Check(stack->currentWidget() == scanPage, "点击 Scan 按钮后切换到扫描页")) {
        return 8;
    }
    if (!Check(scanButton->isChecked(), "Scan 步骤按钮显示选中")) {
        return 9;
    }
    // 切到处理步骤，验证未挂载步骤也能显示占位页，不会崩溃。
    processButton->click();
    if (!Check(processButton->isChecked(), "Process 步骤按钮显示选中")) {
        return 10;
    }
    // 非法 step 只应写 Warning 日志，不应导致崩溃。
    shell->SetStep(99);

    // 释放第一个根控件后切换到练习模式，验证壳子只显示 Scan/Process 两个步骤。
    delete widget;
    shell->Shutdown();

    if (!Check(shell->Init(appDirBytes.constData(), logDirBytes.constData()),
               "OrderScanWorkspaceShell 重新初始化成功")) {
        return 11;
    }
    shell->SetWorkspaceMode(WorkspaceModePractice);
    QWidget* practiceWidget = shell->CreateWidget();
    if (!Check(practiceWidget != nullptr, "练习模式能创建根 QWidget")) {
        return 12;
    }
    auto* orderButton = practiceWidget->findChild<QPushButton*>("WorkspaceStep1Button");
    auto* practiceScanButton = practiceWidget->findChild<QPushButton*>("WorkspaceStep2Button");
    auto* practiceProcessButton = practiceWidget->findChild<QPushButton*>("WorkspaceStep3Button");
    auto* sendButton = practiceWidget->findChild<QPushButton*>("WorkspaceStep4Button");
    if (!Check(orderButton == nullptr, "练习模式不显示 Order 步骤按钮")) {
        return 13;
    }
    if (!Check(practiceScanButton != nullptr && practiceProcessButton != nullptr,
               "练习模式显示 Scan 和 Process 步骤按钮")) {
        return 14;
    }
    if (!Check(sendButton == nullptr, "练习模式不显示 Send 步骤按钮")) {
        return 15;
    }
    if (!Check(practiceScanButton->isChecked(), "练习模式默认选中 Scan 步骤")) {
        return 16;
    }

    // --show 用于人工查看壳子界面；默认不进入事件循环，便于自动化测试结束。
    if (QCoreApplication::arguments().contains("--show")) {
        // 使用接近主程序的窗口尺寸观察步骤区域布局。
        practiceWidget->resize(1180, 760);
        practiceWidget->show();
        return app.exec();
    }

    // 删除根控件会连带释放占位页，验证资源释放路径没有悬挂引用。
    delete practiceWidget;
    // Shutdown 释放壳子模块内部状态和日志引用。
    shell->Shutdown();
    std::printf("OrderScanWorkspaceShellTest passed.\n");
    return 0;
}
