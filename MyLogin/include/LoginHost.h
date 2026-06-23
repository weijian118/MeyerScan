#pragma once

#include <QObject>

#include "MeyerLoginWidget.h"

class LoginHost : public QObject {
public:
    explicit LoginHost(QObject* parent = nullptr);

    void Start();

private:
    void OnLoginStatusReturn(const LoginReturnParameters& result);

private:
    UserLoginParameters BuildLoginParameters() const;

private:
    CBLMeyerLoginWidget m_loginWidget;
};
