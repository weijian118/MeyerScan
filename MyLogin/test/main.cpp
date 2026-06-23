#include "LoginHost.h"

#include <QApplication>
#include <QTimer>
#include <cstring>

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    LoginHost host;
    host.Start();

    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        QTimer::singleShot(3000, &app, SLOT(quit()));
    }

    return app.exec();
}
