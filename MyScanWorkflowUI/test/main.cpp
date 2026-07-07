#include "ScanWorkflowUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QPushButton>
#include <QWidget>
#include <cstdio>

namespace {

// Minimal assertion helper that avoids adding a QtTest dependency to this smoke test.
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// Callback used to verify that UI actions can report back to the host.
void OnAction(void* context, int actionId) {
    int* counter = static_cast<int*>(context);
    if (counter) {
        // Count only, because this smoke test should not depend on exact UI wording.
        ++(*counter);
    }
    std::printf("[ACTION] %d\n", actionId);
}

}

// Entry point for ScanWorkflowUITest.exe.
int main(int argc, char* argv[]) {
    // QVTKWidget is a QWidget/OpenGL object, so QApplication is required.
    QApplication app(argc, argv);

    // Derive paths from the executable folder, not from currentPath().
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    IScanWorkflowUI* scan = GetScanWorkflowUI();
    if (!Check(scan != nullptr, "ScanWorkflowUI factory returned an instance")) {
        return 1;
    }
    if (!Check(scan->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()),
               "ScanWorkflowUI initialized")) {
        return 2;
    }

    int actionCount = 0;
    scan->SetActionCallback(&OnAction, &actionCount);
    scan->SetSessionContextJson("{\"orderId\":\"demo-order\",\"sessionId\":\"scan-session-demo\","
                                "\"scanProcess\":{\"steps\":["
                                "{\"label\":\"Custom maxilla\",\"code\":\"custom_maxilla\"},"
                                "{\"label\":\"Custom mandible\",\"code\":\"custom_mandible\"}]}}");

    QWidget* widget = scan->CreateWidget();
    if (!Check(widget != nullptr, "ScanWorkflowUI created root QWidget")) {
        return 3;
    }
    if (!Check(widget->objectName() == "MeyerScanScanWorkflowUIRoot", "ScanWorkflowUI root object name is correct")) {
        return 4;
    }

    // 自定义 scanProcess 只有两个步骤，所以顶部扫描流程按钮至少应包含这两个自定义按钮。
    // The custom scanProcess should drive the top process buttons.
    const QList<QPushButton*> buttons = widget->findChildren<QPushButton*>();
    bool hasCustomMaxilla = false;
    bool hasCustomMandible = false;
    QPushButton* customMaxillaButton = nullptr;
    QPushButton* customMandibleButton = nullptr;
    for (QPushButton* button : buttons) {
        if (button->text() == "Custom maxilla") {
            hasCustomMaxilla = true;
            customMaxillaButton = button;
        }
        if (button->text() == "Custom mandible") {
            hasCustomMandible = true;
            customMandibleButton = button;
        }
    }
    if (!Check(hasCustomMaxilla && hasCustomMandible, "ScanWorkflowUI renders session scanProcess buttons")) {
        return 5;
    }
    if (!Check(customMaxillaButton && !customMaxillaButton->toolTip().isEmpty(),
               "ScanWorkflowUI process button has tooltip")) {
        return 6;
    }
    if (!Check(customMaxillaButton && customMaxillaButton->cursor().shape() == Qt::PointingHandCursor,
               "ScanWorkflowUI process button uses hand cursor")) {
        return 7;
    }
    if (customMandibleButton) {
        // click() 直接触发 QPushButton 的 clicked 信号，用来验证步骤按钮能切换并上报动作。
        customMandibleButton->click();
    }
    if (!Check(actionCount > 0, "ScanWorkflowUI process button click reports action")) {
        return 8;
    }

    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1360, 820);
        widget->show();
        return app.exec();
    }

    // Exercise activation and release without showing a window.
    scan->Activate();
    scan->DeactivateAndRelease();
    delete widget;
    scan->Shutdown();
    std::printf("ScanWorkflowUITest passed.\n");
    return 0;
}
