#include "LoginHost.h"

#include <QApplication>
#include <QTimer>
#include <cstring>

// 登录模块测试宿主入口。
// 测试目标:
//   1. 验证 MeyerLoginWidget.dll 及其依赖 DLL 能被 VS2015/运行目录正确加载。
//   2. 验证登录窗口能正常弹出并返回 loginStatusReturn 信号。
//   3. 通过 --smoke 参数支持自动化冒烟测试，避免无人值守测试时界面一直停留。
int main(int argc, char* argv[]) {
    // 启用 Qt 高 DPI 支持，让测试界面在高分屏下不至于模糊。
    // 必须在 QApplication 构造之前设置。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // LoginHost 只封装登录 DLL 调用，不模拟 MainExe 后续首页流程。
    // 这样测试失败时可以先确认问题在登录 DLL/依赖加载，而不是首页流程。
    LoginHost host;
    // Start 内部会组装 UserLoginParameters 并调用 MeyerLoginWidget.dll 显示窗口。
    host.Start();

    // 冒烟模式只验证窗口可创建和依赖可加载，3 秒后自动退出。
    // 人工测试时不传 --smoke，就可以真实点击登录按钮观察返回状态。
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        // 登录窗口来自外部 DLL，给 3 秒比普通 UI smoke 更稳，避免慢机器刚弹出就退出。
        QTimer::singleShot(3000, &app, SLOT(quit()));
    }

    return app.exec();
}
