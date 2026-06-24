#include "MainWindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWidget>
#include <QTimer>
#include <cstring>

namespace {
const char* kSingleInstanceName = "MeyerScan_MainExe_SingleInstance";

// 尝试通知已经运行的 MeyerScan.exe 激活窗口。
// 返回 true 表示已有实例接收了通知，当前进程可以直接退出。
bool NotifyExistingInstance() {
    // QLocalSocket 用于和已经运行的 MeyerScan.exe 通信。
    // 如果连接成功，说明已有实例正在监听单实例服务名。
    QLocalSocket socket;
    socket.connectToServer(kSingleInstanceName);
    if (!socket.waitForConnected(200)) {
        // 200ms 内连不上，认为没有已运行实例，当前进程继续启动。
        return false;
    }
    // 发送一个很小的激活消息；当前协议只需要知道“有人再次启动了我”。
    socket.write("activate");
    socket.flush();
    // 等待写入完成，避免当前进程过早退出导致消息丢失。
    socket.waitForBytesWritten(200);
    return true;
}
}

// MeyerScan.exe 入口函数。
// Release 工程使用 Windows 子系统 + mainCRTStartup，因此不会出现 CMD 窗口，
// 但仍然可以保留普通 main()，减少 Qt 入口代码改动。
int main(int argc, char* argv[]) {
    // High DPI 属性必须在 QApplication 创建之前设置。
    // 否则 Qt 会在应用启动后锁定 DPI 行为，后续设置不生效。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // --smoke: 验证登录前基础流程后自动退出。
    // --smoke-main: 跳过登录，验证首页/案例页/资源释放链路。
    // strcmp 只在 argc > 1 时调用，避免访问不存在的 argv[1]。
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    const bool smokeMain = argc > 1 && std::strcmp(argv[1], "--smoke-main") == 0;

    // 单实例是固定流程，不再由 runtime_config.json 控制。
    // 登录中或数据库检查中时，已有实例不会强制弹出主窗口。
    if (!smoke && !smokeMain && NotifyExistingInstance()) {
        // 已通知已有实例后，当前进程直接退出，保证只运行一个 MeyerScan.exe。
        return 0;
    }

    // 重新监听前先清理可能残留的本地服务名。
    // 上次进程异常退出时，QLocalServer 名称可能尚未释放。
    QLocalServer::removeServer(kSingleInstanceName);
    QLocalServer singleServer;
    if (!smoke && !smokeMain) {
        // 只有正式运行需要单实例监听；smoke 测试允许并行执行，避免互相影响。
        singleServer.listen(kSingleInstanceName);
    }

    // MainWindow 构造时会创建主窗口外壳并尽早初始化日志。
    MainWindow window;

    // 已运行实例收到 activate 消息时，只在登录完成且主窗口可见后激活。
    // 这样不会打断数据库检查或登录模块。
    QObject::connect(&singleServer, &QLocalServer::newConnection, [&singleServer, &window]() {
        // 取出待处理连接并安排删除，避免 socket 对象泄漏。
        QLocalSocket* socket = singleServer.nextPendingConnection();
        if (socket) {
            socket->deleteLater();
        }
        // 登录前主窗口可能隐藏或正在等待页，不做激活，避免用户看到半初始化界面。
        if (window.IsLoginCompleted() && window.isVisible()) {
            window.showNormal();
            window.raise();
            window.activateWindow();
        }
    });

    if (smokeMain) {
        // smoke-main 跳过真实登录，专门验证 MainExe + HomeUI + CaseUI 的集成链路。
        window.StartWithoutLoginForSmoke();
    } else {
        // 正式流程和 --smoke 都会进入登录启动路径。
        window.StartLogin();
    }

    // 冒烟测试必须自动退出，避免 CI/脚本卡在 Qt 事件循环。
    if (smoke || smokeMain) {
        // 给登录窗口/页面切换/资源释放留足事件循环时间。
        QTimer::singleShot(3000, &app, SLOT(quit()));
    }

    return app.exec();
}
