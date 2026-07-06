#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstring>

namespace {

// 单实例通信使用的本地服务名。
// 该名称只在本机当前用户会话内使用，用于第二次启动时通知已经运行的 MeyerScan.exe。
const char* kSingleInstanceName = "MeyerScan_MainExe_SingleInstance";

// 从命令行读取指定参数后面的值。
// 这里不用 Qt 的 QCommandLineParser，是为了保持当前 main.cpp 简单并兼容 VS2015 既有写法。
QString ValueAfterArgument(const QStringList& arguments, const QString& name) {
    const int index = arguments.indexOf(name);
    if (index >= 0 && index + 1 < arguments.size()) {
        return arguments.at(index + 1);
    }
    return QString();
}

// 构造单实例消息。
// 使用 JSON 而不是裸字符串拼接，是为了以后继续扩展第三方参数时不破坏旧字段。
QByteArray BuildSingleInstanceMessage(const QString& type,
                                      const QString& externalOrderPath = QString(),
                                      const QString& externalOrderType = QString()) {
    QJsonObject message;
    message.insert("type", type);
    if (!externalOrderPath.isEmpty()) {
        message.insert("externalOrderPath", externalOrderPath);
    }
    if (!externalOrderType.isEmpty()) {
        message.insert("externalOrderType", externalOrderType);
    }
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

// 通知已经运行的 MeyerScan.exe 激活主窗口或处理第三方建单。
// 返回 true 表示本机已有实例接收了通知，当前进程可以直接退出。
bool NotifyExistingInstance(const QByteArray& message) {
    // QLocalSocket 是 Qt 提供的本机进程间通信 socket。
    // 这里不需要跨机器通信，只需要连接同名 QLocalServer。
    QLocalSocket socket;

    // 尝试连接已经运行实例创建的本地服务。
    // 如果没有已经运行的实例，waitForConnected 会在超时后返回 false。
    socket.connectToServer(kSingleInstanceName);
    if (!socket.waitForConnected(200)) {
        // 连接失败说明没有已运行实例，当前进程应该继续正常启动。
        return false;
    }

    // 发送单实例消息。当前支持 activate 和 external-order。
    // 第三方拉起时，新的进程只负责把 JSON 路径交给旧实例，避免同时运行两个 MeyerScan.exe。
    socket.write(message);

    // flush 把 Qt 缓冲区中的数据推给底层 socket。
    socket.flush();

    // 等待最多 200ms，避免当前进程过早退出导致消息还没写出去。
    socket.waitForBytesWritten(200);
    return true;
}

} // namespace

// MeyerScan.exe 的主入口。
// VS2015 工程使用 Windows 子系统 + mainCRTStartup，因此不会弹出 CMD 窗口，
// 但仍然可以保留标准 main() 写法，减少 Qt 入口代码改动。
int main(int argc, char* argv[]) {
    // High DPI 属性必须在 QApplication 构造之前设置。
    // Qt 创建 QApplication 后会锁定 DPI 行为，之后再设置这些属性不会生效。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // QApplication 管理 Qt Widgets 事件循环、窗口资源和命令行参数。
    QApplication app(argc, argv);

    // --smoke 走真实登录前启动链路，稍后自动退出。
    // --smoke-main 跳过登录，用于自动验证 MainExe + HomeUI + CaseUI 集成链路。
    // --smoke-external-order 走第三方建单链路，自动打开 OrderScanWorkspaceShell/OrderCreateUI 后退出。
    // std::strcmp 前先判断 argc，避免访问不存在的 argv[1]。
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    const bool smokeMain = argc > 1 && std::strcmp(argv[1], "--smoke-main") == 0;
    const QStringList arguments = QCoreApplication::arguments();
    const bool smokeExternalOrder = arguments.contains("--smoke-external-order");
    const QString externalOrderPath = ValueAfterArgument(arguments, "--external-order");
    const QString externalOrderType = ValueAfterArgument(arguments, "--external-order-type");
    const bool externalOrder = !externalOrderPath.isEmpty();

    // 单实例是固定启动流程，不由 runtime_config.json 控制。
    // 冒烟测试允许并行启动，避免自动化测试之间互相抢同一个本地服务名。
    const QByteArray singleInstanceMessage = externalOrder
        ? BuildSingleInstanceMessage("external-order", externalOrderPath, externalOrderType)
        : BuildSingleInstanceMessage("activate");

    if (!smoke && !smokeMain && !smokeExternalOrder && NotifyExistingInstance(singleInstanceMessage)) {
        // 已经通知旧实例后，本进程直接退出，保证客户桌面只保留一个主程序。
        return 0;
    }

    // 清理可能残留的本地服务名。
    // 上一次异常退出时，QLocalServer 名称可能还没有及时释放。
    QLocalServer::removeServer(kSingleInstanceName);

    // 当前进程作为唯一主实例时监听这个本地服务名。
    QLocalServer singleServer;
    if (!smoke && !smokeMain && !smokeExternalOrder) {
        // listen 失败时不阻塞主流程；最坏结果只是第二次启动无法激活旧窗口。
        singleServer.listen(kSingleInstanceName);
    }

    // MainWindow 是主程序编排层。
    // 构造阶段会创建主窗口容器，并尽早初始化日志。
    MainWindow window;

    // 已运行实例收到第二次启动消息时，只有在登录完成且主窗口可见后才激活窗口。
    // 数据库检查或登录过程中不强制弹主窗口，避免用户看到半初始化界面。
    QObject::connect(&singleServer, &QLocalServer::newConnection, [&singleServer, &window]() {
        // 取出待处理 socket，并交给 Qt 事件循环稍后删除。
        // deleteLater 比立即 delete 更适合在信号回调中释放 QObject。
        QLocalSocket* socket = singleServer.nextPendingConnection();
        if (socket) {
            // 第二个进程写完后会很快退出，这里短暂等待数据到达。
            // 如果读取不到内容，按旧的 activate 行为处理即可。
            socket->waitForReadyRead(200);
            const QByteArray payload = socket->readAll();
            socket->deleteLater();

            const QJsonDocument document = QJsonDocument::fromJson(payload);
            const QJsonObject message = document.isObject() ? document.object() : QJsonObject();
            const QString type = message.value("type").toString("activate");

            // 登录完成后才允许激活主界面或接收第三方订单。
            // 数据库检查或登录过程中收到第二次启动，按既有规则忽略。
            if (window.IsLoginCompleted()) {
                if (type == "external-order") {
                    window.StartExternalOrder(message.value("externalOrderPath").toString(),
                                              message.value("externalOrderType").toString());
                } else if (window.isVisible()) {
                    window.showNormal();
                    window.raise();
                    window.activateWindow();
                }
            }
        }
    });

    if (smokeMain) {
        // smoke-main 跳过登录模块，专门验证主程序和已拆 UI 模块之间的集成流程。
        window.StartWithoutLoginForSmoke();
    } else if (smokeExternalOrder) {
        // smoke-external-order 用于自动化验证第三方建单链路。
        // 它仍然走 ExternalLaunchAdapter -> 后台首页创建入口 -> OrderScanWorkspaceShell/OrderCreateUI，
        // 但会由下面的定时器自动退出，避免命令行测试卡住。
        window.StartExternalOrder(externalOrderPath, externalOrderType);
    } else if (externalOrder) {
        // 第三方拉起建单模拟：命令行传入 JSON 文件路径，可选传 thirdPartyType。
        // 视觉上直接进入 OrderScanWorkspaceShell/OrderCreateUI，不显示首页自动跳转过程。
        window.StartExternalOrder(externalOrderPath, externalOrderType);
    } else {
        // 正式流程和 --smoke 都从登录链路开始。
        window.StartLogin();
    }

    if (smoke || smokeMain || smokeExternalOrder) {
        // 冒烟测试必须自动退出，避免命令行构建/测试流程卡在 Qt 事件循环。
        // 5 秒给登录窗口、首页/创建/练习/案例切换和资源释放留出基本事件处理时间。
        QTimer::singleShot(5000, &app, SLOT(quit()));
    }

    // 进入 Qt 事件循环。窗口关闭或 smoke 定时器触发后返回退出码。
    return app.exec();
}
