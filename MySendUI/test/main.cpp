#include "SendUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QStringList>
#include <QWidget>
#include <cstdio>

namespace {

// Small assertion helper for the smoke test.
// It keeps this executable independent from QtTest and prints readable output.
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }

    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// SendUI reports button clicks through a C callback.
// The test only counts callback arrivals; real business handling belongs to MainExe.
void OnAction(void* context, int actionId) {
    int* counter = static_cast<int*>(context);
    if (counter) {
        ++(*counter);
    }

    std::printf("[ACTION] %d\n", actionId);
}

}

// Entry point for SendUITest.exe.
// The default mode is a headless smoke test; pass --show to inspect the page manually.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Use the executable directory as appDir.
    // Do not use currentPath because third-party launchers can change it.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    QDir().mkpath(logDir);

    // Fetch the singleton-style DLL interface and initialize it like MainExe does.
    ISendUI* send = GetSendUI();
    if (!Check(send != nullptr, "SendUI factory returned an instance")) {
        return 1;
    }

    if (!Check(send->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData()), "SendUI initialized")) {
        return 2;
    }

    // Provide a minimal order context so the UI can fill its read-only fields.
    int actionCount = 0;
    send->SetActionCallback(&OnAction, &actionCount);
    send->SetSessionContextJson("{\"patient\":{\"name\":\"Send Test Patient\"},"
                                "\"order\":{\"orderId\":\"SEND-001\",\"doctor\":\"Dr. Send\","
                                "\"caseType\":\"restoration\",\"clinic\":\"Smoke Clinic\","
                                "\"dataFormat\":\"PLY\",\"note\":\"Smoke note\"}}");

    // Create the real widget and verify its stable object name.
    QWidget* widget = send->CreateWidget();
    if (!Check(widget != nullptr, "SendUI created root QWidget")) {
        return 3;
    }

    if (!Check(widget->objectName() == "MeyerScanSendUIRoot", "SendUI root object name is correct")) {
        return 4;
    }

    // Verify that context data reaches at least the patient-name field.
    bool hasPatientText = false;
    const QList<QLineEdit*> edits = widget->findChildren<QLineEdit*>();
    for (QLineEdit* edit : edits) {
        if (edit->text() == "Send Test Patient") {
            hasPatientText = true;
        }
    }

    if (!Check(hasPatientText, "SendUI applies session context to fields")) {
        return 5;
    }

    // Verify the finish command exists.
    // Real click-path testing is still kept as manual UI verification for now.
    bool hasFinishButton = false;
    const QList<QPushButton*> buttons = widget->findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == "Finish") {
            hasFinishButton = true;
        }
    }

    if (!Check(hasFinishButton, "SendUI renders finish action button")) {
        return 6;
    }

    // Optional manual mode keeps the window open for visual inspection.
    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1360, 820);
        widget->show();
        return app.exec();
    }

    delete widget;
    send->Shutdown();
    std::printf("SendUITest passed.\n");
    return 0;
}
