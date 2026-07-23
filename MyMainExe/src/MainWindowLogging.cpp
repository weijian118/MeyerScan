#include "MainWindow.h"
#include "MainWindowInternal.h"

#include <QDir>
#include <QLabel>

#include "Logger.h"

// 写客户操作日志。日志对象使用构造期缓存的 m_logger，避免每次重新 GetLogger()。
void MainWindow::WriteUserAction(const QString& operation, const QString& content) {
    if (!m_logger) {
        // 日志未初始化时不阻断 UI 操作。
        return;
    }
    // operation/content 先转 UTF-8，保证跨 DLL char* 接口读到稳定字节。
    // QByteArray 局部变量必须活到 m_logger->Write 调用结束，所以不能把 toUtf8().constData() 拆到临时表达式里长期保存。
    const QByteArray operationBytes = operation.toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    m_logger->Write(LogLevel::Info,
                    ModuleInfo::Name,
                    operationBytes.constData(),
                    "",
                    "",
                    "",
                    contentBytes.constData());
}

// 更新主窗口状态栏。登录窗口显示期间主窗口隐藏，但状态仍保留给后续排查。
void MainWindow::WriteStatus(const QString& text) {
    if (m_status) {
        // 状态栏更新只影响本地 UI，不写日志，避免高频状态变化刷屏。
        m_status->setText(text);
    }
}

// 统一显示无边框全屏主窗口。
// 所有页面切换只替换 m_contentRoot 内的当前 QWidget，不再创建多个顶层窗口。
void MainWindow::ShowMainWindow() {
    // showFullScreen 会保留构造期设置的 FramelessWindowHint，并覆盖此前最小化状态。
    // 统一从这里显示可以防止某个流程误用普通 show() 后退回带边框的小窗口。
    showFullScreen();
    raise();
    activateWindow();
    WriteUserAction("WindowShow", "Frameless full-screen main window shown");
}

// 早期日志初始化。
// 此函数允许重复调用：第一次创建 logs 目录并 Init，后续调用只复用已缓存指针。
void MainWindow::InitLoggerEarly() {
    if (m_loggerInitialized) {
        // 已初始化时直接返回，保证多入口重复调用安全。
        return;
    }

    // 日志目录固定在 EXE 同级 logs，并在最早阶段创建。
    const QString logDir = ResolveLogDir();
    QDir().mkpath(logDir);
    m_logDirUtf8 = QDir::fromNativeSeparators(logDir).toUtf8();
    // Logger 也走运行时动态加载，保证 MainExe 启动期不再依赖 Logger.lib。
    m_logger = LoggerModule();
    if (m_logger && m_logger->Init(m_logDirUtf8.constData(), LogLevel::Info)) {
        m_loggerInitialized = true;
        m_logger->Write(LogLevel::Info, ModuleInfo::Name, "Startup", "", "", "", "Logger initialized early");
    }
}
