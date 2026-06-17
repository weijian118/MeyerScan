#include "CaseUI.h"
#include <QApplication>
#include <QTimer>
#include <QWidget>
#include <cstring>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ICaseUI* caseUi = GetCaseUI();
    if (!caseUi) {
        return 1;
    }

    caseUi->Init("F:/MeyerScan/MyDatabase/config/db_config.json", "F:/MeyerScan/MyCaseUI/logs");
    QWidget* widget = caseUi->CreateWidget();
    widget->setWindowTitle(caseUi->GetModuleVersion());
    widget->show();
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        QTimer::singleShot(300, &app, SLOT(quit()));
    }
    int result = app.exec();
    caseUi->Shutdown();
    return result;
}
