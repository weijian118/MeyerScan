#include "HomeUI.h"

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
// 将测试窗口显示在当前可用屏幕中央。
// 说明:
//   - 测试宿主不是正式 MainExe，没有统一页面容器。
//   - 这里根据当前屏幕可用区域计算初始大小和位置，避免窗口超出小屏幕。
void ShowOnCurrentScreen(QWidget* widget) {
    // availableGeometry 是扣除任务栏后的可用区域，比 screenGeometry 更适合摆放窗口。
    QRect available = QApplication::desktop()->availableGeometry(widget);
    // sizeHint 来自 Layout 推荐尺寸，minimumSize 来自模块设置的最小可用尺寸。
    // expandedTo 表示至少不能小于模块自己声明的最小尺寸。
    QSize initialSize = widget->sizeHint().expandedTo(widget->minimumSize());
    // 窗口最大不超过屏幕可用区域四周各留 40 像素，避免小屏幕下窗口出界。
    initialSize.setWidth(qMin(initialSize.width(), qMax(available.width() - 80, widget->minimumWidth())));
    initialSize.setHeight(qMin(initialSize.height(), qMax(available.height() - 80, widget->minimumHeight())));

    widget->resize(initialSize);
    // 计算居中坐标时使用 available.left()/top()，兼容副屏位于主屏左侧或上方的情况。
    const int x = available.left() + qMax(0, (available.width() - widget->width()) / 2);
    const int y = available.top() + qMax(0, (available.height() - widget->height()) / 2);
    widget->move(x, y);
    widget->show();
}

// 根据测试 EXE 的运行目录反推出模块根目录。
// Release 结构通常是 MyHomeUI/bin/Release/HomeUITest.exe，
// 所以从 applicationDirPath() 向上两级回到 MyHomeUI。
QString ResolveModuleRoot() {
    // applicationDirPath 指向 HomeUITest.exe 所在目录，例如 MyHomeUI/bin/Release。
    QDir dir(QCoreApplication::applicationDirPath());
    // 第一次 cdUp: Release -> bin。
    dir.cdUp();
    // 第二次 cdUp: bin -> MyHomeUI。
    dir.cdUp();
    return dir.absolutePath();
}
}

// HomeUI 测试宿主入口。
// 测试目标:
//   1. 验证 HomeUI DLL 可以被加载。
//   2. 验证首页 QWidget 可以创建并显示。
//   3. 验证日志目录和数据库配置路径能按模块目录正确传入。
int main(int argc, char* argv[]) {
    // 高 DPI 属性必须在 QApplication 创建前设置。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    // GetHomeUI 是 HomeUI DLL 暴露的 C ABI 工厂函数。
    // 测试宿主通过它验证 DLL 能正确加载并返回接口。
    IHomeUI* home = GetHomeUI();
    if (!home) {
        // 返回非 0 让脚本/CI 能识别 DLL 加载或导出函数异常。
        return 1;
    }

    // 测试宿主不依赖 currentPath，所有路径都从 EXE 目录推导，和正式 MainExe 规则一致。
    const QString moduleRoot = ResolveModuleRoot();
    QDir repoDir(moduleRoot);
    // 从 MyHomeUI 上一级回到 F:/MeyerScan，方便定位 MyDatabase/config。
    repoDir.cdUp();
    const QString databaseConfigPath = repoDir.filePath("MyDatabase/config/db_config.json");
    const QString logDir = QDir(moduleRoot).filePath("logs");

    // 公共接口使用 const char*，这里把 QString 转成 UTF-8 并保持到 Init 调用结束。
    const QByteArray databaseConfigBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    home->Init(databaseConfigBytes.constData(), logDirBytes.constData());

    // 创建首页并用模块版本做窗口标题，便于人工测试时确认加载的是当前 DLL。
    QWidget* widget = home->CreateWidget();
    // 测试窗口没有 MainExe 外壳，直接使用模块版本作为标题最直观。
    widget->setWindowTitle(home->GetModuleVersion());
    ShowOnCurrentScreen(widget);

    // 冒烟模式只验证创建和显示，短暂停留后自动退出，便于批量测试。
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        // 用定时器退出而不是立即 return，是为了让 Qt 真正跑一次事件循环并完成绘制。
        QTimer::singleShot(300, &app, SLOT(quit()));
    }

    // 退出前显式关闭和删除测试窗口，再调用模块 Shutdown，方便发现析构问题。
    int result = app.exec();
    // close 先触发 Qt 正常关闭流程，delete 再释放控件树。
    widget->close();
    delete widget;
    // Shutdown 用于验证模块清理路径，测试宿主结束前必须调用。
    home->Shutdown();
    return result;
}
