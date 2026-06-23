#include "MainWindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWidget>
#include <QTimer>
#include <cstring>

namespace {
const char* kSingleInstanceName = "MeyerScan_MainExe_SingleInstance";

bool NotifyExistingInstance() {
    QLocalSocket socket;
    socket.connectToServer(kSingleInstanceName);
    if (!socket.waitForConnected(200)) {
        return false;
    }
    socket.write("activate");
    socket.flush();
    socket.waitForBytesWritten(200);
    return true;
}
}

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    const bool smokeMain = argc > 1 && std::strcmp(argv[1], "--smoke-main") == 0;
    if (!smoke && !smokeMain && NotifyExistingInstance()) {
        return 0;
    }

    QLocalServer::removeServer(kSingleInstanceName);
    QLocalServer singleServer;
    if (!smoke && !smokeMain) {
        singleServer.listen(kSingleInstanceName);
    }

    MainWindow window;
    QObject::connect(&singleServer, &QLocalServer::newConnection, [&singleServer, &window]() {
        QLocalSocket* socket = singleServer.nextPendingConnection();
        if (socket) {
            socket->deleteLater();
        }
        if (window.isVisible()) {
            window.showNormal();
            window.raise();
            window.activateWindow();
        }
    });

    if (smokeMain) {
        window.StartWithoutLoginForSmoke();
    } else {
        window.StartLogin();
    }

    if (smoke || smokeMain) {
        QTimer::singleShot(3000, &app, SLOT(quit()));
    }

    return app.exec();
}
