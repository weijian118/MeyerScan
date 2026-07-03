#include "UIComponents.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include <cstdio>

namespace {

bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    IUIComponents* ui = GetUIComponents();
    if (!Check(ui != nullptr, "UIComponents 工厂函数返回有效实例")) {
        return 1;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!Check(ui->Init(appDir.toUtf8().constData()), "UIComponents 初始化成功")) {
        return 2;
    }
    if (!Check(ui->ScaleX() > 0.0 && ui->ScaleY() > 0.0, "屏幕辅助缩放系数有效")) {
        return 3;
    }

    QWidget root;
    root.setObjectName("UIComponentsTestRoot");

    // 逐个创建常用控件，验证统一控件工厂不会返回空指针。
    QWidget* waitWidget = ui->CreateWaitWidget("Loading", "Preparing test widgets", &root);
    QPushButton* primary = ui->CreatePrimaryButton("Primary", &root);
    QPushButton* secondary = ui->CreateSecondaryButton("Secondary", &root);
    QPushButton* danger = ui->CreateButton(MeyerButtonRoleDanger, MeyerButtonContentTextOnly, "Delete", "", &root);
    QToolButton* tool = ui->CreateToolButton(MeyerButtonRoleSecondary, MeyerButtonContentIconTopText, "Tool", "", &root);
    QLineEdit* edit = ui->CreateLineEdit("Input", &root);
    QComboBox* combo = ui->CreateComboBox(&root);
    QLabel* title = ui->CreatePageTitle("Page Title", &root);

    if (!Check(waitWidget && waitWidget->objectName() == "MeyerScanWaitWidget", "等待页控件创建成功")) {
        return 4;
    }
    if (!Check(primary && secondary && danger && tool && edit && combo && title, "常用 UI 控件全部创建成功")) {
        return 5;
    }

    // 默认不显示窗口，自动化测试只验证控件树和样式入口；传 --show 时可人工查看。
    if (QCoreApplication::arguments().contains("--show")) {
        root.resize(900, 600);
        root.show();
        return app.exec();
    }

    ui->Shutdown();
    std::printf("UIComponentsTest passed.\n");
    return 0;
}
