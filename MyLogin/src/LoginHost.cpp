#include "LoginHost.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QTimer>

namespace {
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";
}

LoginHost::LoginHost(QObject* parent)
    : QObject(parent) {
    connect(&m_loginWidget,
            &CBLMeyerLoginWidget::loginStatusReturn,
            this,
            &LoginHost::OnLoginStatusReturn);
}

void LoginHost::Start() {
    m_loginWidget.initLoginWidgetAndShow(BuildLoginParameters());
}

UserLoginParameters LoginHost::BuildLoginParameters() const {
    const QString appDir = QCoreApplication::applicationDirPath();

    UserLoginParameters params;
    params.nfaktoW = 1.0;
    params.nfaktoH = 1.0;
    params.dataPath = QDir(appDir).filePath("license.lic");
    params.languageType = G_LANG_SIMPLIFIED_CHINESE;
    params.AppPath = appDir;
    params.loginUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.registerUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.version = 100;
    return params;
}

void LoginHost::OnLoginStatusReturn(const LoginReturnParameters& result) {
    qDebug() << "Login status:" << result.currentStatus;

    if (result.currentStatus == LOGIN_SUCCESS ||
        result.currentStatus == WRITECLOUDMSG_SUCCESS ||
        result.currentStatus == USER_CANCEL_LOGIN) {
        QTimer::singleShot(0, qApp, SLOT(quit()));
    }
}
