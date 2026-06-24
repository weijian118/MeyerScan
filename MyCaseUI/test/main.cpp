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
// 将测试窗口居中显示到当前屏幕。
// 测试宿主没有 MainExe 的统一窗口，因此这里做一个最小窗口摆放逻辑。
void ShowOnCurrentScreen(QWidget* widget) {
    // 使用当前屏幕可用区域，避免窗口覆盖任务栏或跑到屏幕外。
    QRect available = QApplication::desktop()->availableGeometry(widget);
    // sizeHint 由 Layout 计算，minimumSize 由模块约束，两者取较大值作为初始尺寸。
    QSize initialSize = widget->sizeHint().expandedTo(widget->minimumSize());
    // 小屏幕下给四周留白，防止窗口尺寸超过可用区域。
    initialSize.setWidth(qMin(initialSize.width(), qMax(available.width() - 80, widget->minimumWidth())));
    initialSize.setHeight(qMin(initialSize.height(), qMax(available.height() - 80, widget->minimumHeight())));

    widget->resize(initialSize);
    // 多显示器坐标可能为负数，所以不能假设屏幕左上角是 0,0。
    const int x = available.left() + qMax(0, (available.width() - widget->width()) / 2);
    const int y = available.top() + qMax(0, (available.height() - widget->height()) / 2);
    widget->move(x, y);
    widget->show();
}

// 从测试 EXE 所在目录反推出 MyCaseUI 模块根目录。
// 不能使用 currentPath，因为第三方启动或 VS 调试时工作目录可能不同。
QString ResolveModuleRoot() {
    // applicationDirPath 指向 MyCaseUI/bin/Release。
    QDir dir(QCoreApplication::applicationDirPath());
    // Release -> bin。
    dir.cdUp();
    // bin -> MyCaseUI。
    dir.cdUp();
    return dir.absolutePath();
}
}

// CaseUI 测试宿主入口。
// 测试目标:
//   1. 验证案例管理 DLL 可加载。
//   2. 验证案例管理界面可创建和显示。
//   3. 验证日志和数据库配置路径能按重构约定传入。
int main(int argc, char* argv[]) {
    // 启用 Qt 高 DPI 支持，必须放在 QApplication 构造之前。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    // 通过 C ABI 工厂函数获取 CaseUI 接口，验证 DLL 导出和依赖加载正常。
    ICaseUI* caseUi = GetCaseUI();
    if (!caseUi) {
        // 非 0 退出码让批量脚本知道测试失败。
        return 1;
    }

    // 所有运行路径都从 applicationDirPath() 推导，避免 currentPath 被外部进程污染。
    const QString moduleRoot = ResolveModuleRoot();
    QDir repoDir(moduleRoot);
    // MyCaseUI 的上级就是 F:/MeyerScan，用于定位其它模块配置。
    repoDir.cdUp();
    const QString databaseConfigPath = repoDir.filePath("MyDatabase/config/db_config.json");
    const QString logDir = QDir(moduleRoot).filePath("logs");

    // DLL 公共接口使用 UTF-8 char*，QByteArray 保证字节在 Init 调用期间有效。
    const QByteArray databaseConfigBytes = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();
    const QByteArray logDirBytes = QDir::fromNativeSeparators(logDir).toUtf8();
    caseUi->Init(databaseConfigBytes.constData(), logDirBytes.constData());

    // 创建案例管理界面并显示。窗口标题使用模块版本，便于人工确认 DLL 版本。
    QWidget* widget = caseUi->CreateWidget();
    // 测试宿主没有 MainExe 标题栏上下文，所以直接展示模块版本。
    widget->setWindowTitle(caseUi->GetModuleVersion());
    ShowOnCurrentScreen(widget);

    // 冒烟模式用于自动化测试，只停留 300ms 验证窗口创建成功。
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        // 让事件循环至少执行一次，才能暴露控件创建/显示阶段的问题。
        QTimer::singleShot(300, &app, SLOT(quit()));
    }

    // 退出前主动关闭窗口并调用 Shutdown，便于暴露资源释放问题。
    int result = app.exec();
    // close 触发 Qt 关闭事件，delete 释放整个控件树。
    widget->close();
    delete widget;
    // 模块 Shutdown 单独调用，验证 CaseUI 不依赖进程直接退出清理资源。
    caseUi->Shutdown();
    return result;
}
