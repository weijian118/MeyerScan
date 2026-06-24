#pragma once

#include <QObject>

#include "MeyerLoginWidget.h"

// LoginHost 是登录模块测试宿主的最小封装类。
// 作用:
//   1. 按当前重构约定组装 UserLoginParameters。
//   2. 调用已经开发完成的 MeyerLoginWidget.dll 显示登录界面。
//   3. 接收登录模块返回的 loginStatusReturn 信号，方便测试宿主退出。
// 注意:
//   该类只用于验证登录 DLL 能被正确加载和调用，不承载 MainExe 的业务流程。
class LoginHost : public QObject {
public:
    // 构造测试宿主对象，并在构造阶段连接登录模块的返回信号。
    explicit LoginHost(QObject* parent = nullptr);

    // 启动登录窗口。
    // 调用后登录模块自己负责显示界面和内部语言文件加载。
    void Start();

private:
    // 接收登录模块返回的状态结构体。
    // 测试宿主只关心“流程已经结束”，所以成功、写云端成功、用户取消都会退出程序。
    void OnLoginStatusReturn(const LoginReturnParameters& result);

private:
    // 组装调用登录模块需要的参数。
    // 这里使用测试阶段固定值；MainExe 集成时会按安装目录、语言、缩放系数重新组装。
    UserLoginParameters BuildLoginParameters() const;

private:
    // 已交付的登录窗口对象，真实界面和账号/离线授权逻辑都在该对象内部。
    CBLMeyerLoginWidget m_loginWidget;
};
