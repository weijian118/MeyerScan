#include "HomeUI.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QRect>
#include <QSize>
#include <QTimer>
#include <QWidget>
#include <cstring>

namespace {
void ShowOnCurrentScreen(QWidget* widget) {
    QRect available = QApplication::desktop()->availableGeometry(widget);
    QSize initialSize = widget->sizeHint().expandedTo(widget->minimumSize());
    initialSize.setWidth(qMin(initialSize.width(), qMax(available.width() - 80, widget->minimumWidth())));
    initialSize.setHeight(qMin(initialSize.height(), qMax(available.height() - 80, widget->minimumHeight())));

    widget->resize(initialSize);
    const int x = available.left() + qMax(0, (available.width() - widget->width()) / 2);
    const int y = available.top() + qMax(0, (available.height() - widget->height()) / 2);
    widget->move(x, y);
    widget->show();
}
}

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    IHomeUI* home = GetHomeUI();
    if (!home) {
        return 1;
    }

    home->Init("F:/MeyerScan/MyDatabase/config/db_config.json", "F:/MeyerScan/MyHomeUI/logs");
    QWidget* widget = home->CreateWidget();
    widget->setWindowTitle(home->GetModuleVersion());
    ShowOnCurrentScreen(widget);
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        QTimer::singleShot(300, &app, SLOT(quit()));
    }
    int result = app.exec();
    home->Shutdown();
    return result;
}
