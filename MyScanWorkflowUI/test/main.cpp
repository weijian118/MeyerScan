#include "ScanWorkflowUI.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QPushButton>
#include <QWidget>
#include <QVector>
#include <cstdio>

namespace {

// 扫描页 smoke 的轻量断言函数。
// 不引入 QtTest，保证 VS2015、CMake 和发布目录可以直接运行同一个测试 EXE。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// 保存扫描页上报的稳定动作编号。
struct ActionRecorder {
    QVector<int> actionIds;
};

// 模拟 MainExe 的纯 C 回调，只记录动作，不执行设备或算法业务。
void OnAction(void* context, int actionId) {
    auto* recorder = static_cast<ActionRecorder*>(context);
    if (recorder) {
        recorder->actionIds.append(actionId);
    }
    std::printf("[ACTION] %d\n", actionId);
}

}

// ScanWorkflowUITest.exe 入口。
int main(int argc, char* argv[]) {
    // QVTKWidget 是 QWidget/OpenGL 对象，必须创建 QApplication。
    QApplication app(argc, argv);

    // 路径从测试 EXE 所在目录推导，不使用可被第三方启动器改变的 currentPath。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);
    // 显式保存 UTF-8 缓冲区，避免测试代码示范临时 constData 指针写法。
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();

    // 测试只通过公共工厂访问 DLL，不直接构造内部实现类。
    IScanWorkflowUI* scan = GetScanWorkflowUI();
    if (!Check(scan != nullptr, "ScanWorkflowUI factory returned an instance")) {
        return 1;
    }
    if (!Check(scan->Init(appDirBytes.constData(), logDirBytes.constData()),
               "ScanWorkflowUI initialized")) {
        return 2;
    }

    // 回调上下文由测试栈对象持有，生命周期覆盖所有点击操作。
    ActionRecorder recorder;
    scan->SetActionCallback(&OnAction, &recorder);

    // 先设置有效流程，再验证非法 JSON 被拒绝且不会覆盖上一份有效上下文。
    const char* validContext =
        "{\"orderId\":\"demo-order\",\"sessionId\":\"scan-session-demo\","
        "\"scanProcess\":{\"steps\":["
        "{\"part\":\"maxilla\",\"label\":\"Custom maxilla\",\"code\":\"custom_maxilla\"},"
        "{\"part\":\"mandible\",\"label\":\"Custom mandible\",\"code\":\"custom_mandible\"}]}}";
    if (!Check(scan->SetSessionContextJson(validContext), "ScanWorkflowUI accepts valid session context")) {
        return 3;
    }
    if (!Check(!scan->SetSessionContextJson("{invalid-json"), "ScanWorkflowUI rejects invalid session context")) {
        return 4;
    }

    // CreateWidget 只创建页面，随后显式 Activate，和 MainExe/扫描壳调用顺序保持一致。
    QWidget* widget = scan->CreateWidget();
    if (!Check(widget != nullptr, "ScanWorkflowUI created root QWidget")) {
        return 5;
    }
    if (!Check(widget->objectName() == "MeyerScanScanWorkflowUIRoot", "ScanWorkflowUI root object name is correct")) {
        return 6;
    }
    scan->Activate();

    // 按稳定 objectName 定位步骤按钮，测试不依赖当前翻译文字。
    QPushButton* customMaxillaButton =
        widget->findChild<QPushButton*>("ScanProcessStep_custom_maxilla_Button");
    QPushButton* customMandibleButton =
        widget->findChild<QPushButton*>("ScanProcessStep_custom_mandible_Button");
    if (!Check(customMaxillaButton && customMandibleButton,
               "ScanWorkflowUI renders session scanProcess buttons")) {
        return 7;
    }
    if (!Check(customMaxillaButton && !customMaxillaButton->toolTip().isEmpty(),
               "ScanWorkflowUI process button has tooltip")) {
        return 8;
    }
    if (!Check(customMaxillaButton && customMaxillaButton->cursor().shape() == Qt::PointingHandCursor,
               "ScanWorkflowUI process button uses hand cursor")) {
        return 9;
    }
    if (customMandibleButton) {
        // click() 同步触发 clicked，用来验证步骤切换和宿主动作回调链路。
        customMandibleButton->click();
    }
    if (!Check(recorder.actionIds.contains(ScanWorkflowActionJawModeChanged),
               "ScanWorkflowUI process button click reports stable action")) {
        return 10;
    }

    // --show 仅供人工检查，自动 smoke 不显示窗口，避免界面闪现。
    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1360, 820);
        widget->show();
        return app.exec();
    }

    // 先释放 QVTK/OpenGL，再删除根 widget，顺序与正式页面切换保持一致。
    scan->DeactivateAndRelease();
    delete widget;
    scan->Shutdown();
    std::printf("ScanWorkflowUITest passed.\n");
    return 0;
}
