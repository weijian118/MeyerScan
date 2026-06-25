#include "SettingsUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>

// 解析模块根目录。
// Release 结构通常是 MySettingsUI/bin/Release/SettingsUITest.exe，
// 因此向上两级回到 MySettingsUI，用于独立测试时生成 logs。
QString ResolveModuleRoot() {
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}

// SettingsUI 回调测试函数。
// 独立测试宿主只验证动作能上报，不执行 MainExe 页面切换。
void OnSettingsAction(void* context, int actionId) {
    int* lastAction = static_cast<int*>(context);
    if (lastAction) {
        *lastAction = actionId;
    }
}

// 设置模块测试入口。
// --smoke 模式创建页面后立即退出，用于自动化验证 DLL 装载和 QWidget 创建。
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const QString moduleRoot = ResolveModuleRoot();
    const QString logDir = QDir(moduleRoot).filePath("logs");
    QDir().mkpath(logDir);

    int lastAction = 0;
    ISettingsUI* settings = GetSettingsUI();
    if (!settings) {
        return 2;
    }

    const QByteArray appDirBytes = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath()).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    if (!settings->Init(appDirBytes.constData(), logDirBytes.constData())) {
        return 3;
    }
    settings->SetActionCallback(&OnSettingsAction, &lastAction);

    QWidget* widget = settings->CreateWidget();
    if (!widget) {
        return 4;
    }
    widget->setWindowTitle(settings->GetModuleVersion());
    widget->resize(1180, 760);
    widget->show();

    const QStringList args = QCoreApplication::arguments();
    if (args.contains("--smoke")) {
        QTimer::singleShot(300, &app, [&]() {
            widget->close();
            settings->Shutdown();
            app.quit();
        });
    }

    return app.exec();
}

