#include "DataProcessUI.h"

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

// 处理页 smoke 的轻量断言函数，不额外引入 QtTest。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// 保存处理页上报的稳定动作编号。
struct ActionRecorder {
    QVector<int> actionIds;
};

// 模拟 MainExe 的纯 C 回调，只记录动作，不执行后处理算法。
void OnAction(void* context, int actionId) {
    auto* recorder = static_cast<ActionRecorder*>(context);
    if (recorder) {
        recorder->actionIds.append(actionId);
    }
    std::printf("[ACTION] %d\n", actionId);
}

}

// DataProcessUITest.exe 入口。
int main(int argc, char* argv[]) {
    // QVTKWidget 是 QWidget/OpenGL 对象，必须使用 QApplication。
    QApplication app(argc, argv);

    // 应用与日志路径从 EXE 目录推导，不读取 currentPath。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);
    // QByteArray 持有 QString 转换后的 UTF-8 数据，生命周期覆盖跨 DLL Init 调用。
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();

    // 只通过公共工厂访问 DLL 单例，保持测试与 MainExe 接入方式一致。
    IDataProcessUI* process = GetDataProcessUI();
    if (!Check(process != nullptr, "DataProcessUI factory returned an instance")) {
        return 1;
    }
    if (!Check(process->Init(appDirBytes.constData(), logDirBytes.constData()),
               "DataProcessUI initialized")) {
        return 2;
    }

    ActionRecorder recorder;
    process->SetActionCallback(&OnAction, &recorder);
    const char* validContext =
        "{\"orderId\":\"demo-order\",\"sessionId\":\"process-session-demo\","
        "\"scanProcess\":{\"steps\":["
        "{\"part\":\"maxilla\",\"label\":\"Custom maxilla\",\"code\":\"custom_maxilla\"},"
        "{\"part\":\"mandible\",\"label\":\"Custom mandible\",\"code\":\"custom_mandible\"}]}}";
    if (!Check(process->SetSessionContextJson(validContext), "DataProcessUI accepts valid session context")) {
        return 3;
    }
    if (!Check(!process->SetSessionContextJson("{invalid-json"), "DataProcessUI rejects invalid session context")) {
        return 4;
    }

    // 创建完成后由宿主显式 Activate，不让 CreateWidget 隐式重复激活。
    QWidget* widget = process->CreateWidget();
    if (!Check(widget != nullptr, "DataProcessUI created root QWidget")) {
        return 5;
    }
    if (!Check(widget->objectName() == "MeyerScanDataProcessUIRoot", "DataProcessUI root object name is correct")) {
        return 6;
    }
    process->Activate();

    // 使用稳定 objectName 定位步骤，不依赖翻译后的可见文字。
    QPushButton* customMaxillaButton =
        widget->findChild<QPushButton*>("ProcessStep_custom_maxilla_Button");
    QPushButton* customMandibleButton =
        widget->findChild<QPushButton*>("ProcessStep_custom_mandible_Button");
    if (!Check(customMaxillaButton && customMandibleButton,
               "DataProcessUI renders session scanProcess buttons")) {
        return 7;
    }
    if (!Check(customMaxillaButton && !customMaxillaButton->toolTip().isEmpty(),
               "DataProcessUI process button has tooltip")) {
        return 8;
    }
    if (!Check(customMaxillaButton && customMaxillaButton->cursor().shape() == Qt::PointingHandCursor,
               "DataProcessUI process button uses hand cursor")) {
        return 9;
    }
    if (customMandibleButton) {
        // click() 同步触发 clicked，验证处理步骤切换和动作上报。
        customMandibleButton->click();
    }
    if (!Check(recorder.actionIds.contains(DataProcessActionEdit),
               "DataProcessUI process button click reports stable action")) {
        return 10;
    }

    // Process 页面不得出现 Scan 页的 Start/Pause 控件。
    if (!Check(widget->findChild<QPushButton*>("ScanStartPauseButton") == nullptr,
               "DataProcessUI does not expose scan start controls")) {
        return 11;
    }

    // --show 仅供人工布局检查；自动 smoke 不显示窗口。
    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1360, 820);
        widget->show();
        return app.exec();
    }

    // 先释放 QVTK/OpenGL，再删除测试宿主持有的根 QWidget。
    process->DeactivateAndRelease();
    delete widget;
    process->Shutdown();
    std::printf("DataProcessUITest passed.\n");
    return 0;
}
