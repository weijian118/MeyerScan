#include "CaseUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDir>
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

QString ResolveModuleRoot() {
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}
}

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    ICaseUI* caseUi = GetCaseUI();
    if (!caseUi) {
        return 1;
    }

    const QString moduleRoot = ResolveModuleRoot();
    QDir repoDir(moduleRoot);
    repoDir.cdUp();
    const QString databaseConfigPath = repoDir.filePath("MyDatabase/config/db_config.json");
    const QString logDir = QDir(moduleRoot).filePath("logs");

    const QByteArray databaseConfigBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    caseUi->Init(databaseConfigBytes.constData(), logDirBytes.constData());

    QWidget* widget = caseUi->CreateWidget();
    widget->setWindowTitle(caseUi->GetModuleVersion());
    ShowOnCurrentScreen(widget);
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        QTimer::singleShot(300, &app, SLOT(quit()));
    }
    int result = app.exec();
    widget->close();
    delete widget;
    caseUi->Shutdown();
    return result;
}
