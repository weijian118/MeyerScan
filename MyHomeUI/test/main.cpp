#include "HomeUI.h"
#include <QApplication>
#include <QTimer>
#include <QWidget>
#include <cstring>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    IHomeUI* home = GetHomeUI();
    if (!home) {
        return 1;
    }

    home->Init("F:/MeyerScan/MyDatabase/config/db_config.json", "F:/MeyerScan/MyHomeUI/logs");
    QWidget* widget = home->CreateWidget();
    widget->setWindowTitle(home->GetModuleVersion());
    widget->show();
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        QTimer::singleShot(300, &app, SLOT(quit()));
    }
    int result = app.exec();
    home->Shutdown();
    return result;
}
