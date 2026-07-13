#include "UIComponents.h"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include <cstdio>

namespace {

// 轻量断言函数。
// 当前测试只需要验证控件工厂返回值和基础属性，用简单返回码比引入测试框架更利于 VS2015 调试。
bool Check(bool condition, const char* message) {
    // 成功检查写入 stdout，批量测试时可以看到每一步通过情况。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败检查写入 stderr，脚本可以根据 stderr 快速定位失败原因。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// UIComponents 测试入口：验证共享控件工厂、按钮角色、等待页和基础样式创建能力。
int main(int argc, char* argv[]) {
    // UIComponents 会创建 QWidget/QPushButton 等界面控件，所以测试宿主必须使用 QApplication。
    QApplication app(argc, argv);

    // 通过 DLL 工厂函数拿到接口，先验证模块是否能被正确加载和导出。
    IUIComponents* ui = GetUIComponents();
    if (!Check(ui != nullptr, "UIComponents 工厂函数返回有效实例")) {
        return 1;
    }
    // applicationDirPath 始终指向测试 exe 所在目录，不依赖 currentPath。
    const QString appDir = QCoreApplication::applicationDirPath();
    // Init 会初始化缩放系数、样式入口等 UI 基础设施。
    const QByteArray appDirUtf8 = appDir.toUtf8();
    if (!Check(ui->Init(appDirUtf8.constData()), "UIComponents 初始化成功")) {
        return 2;
    }
    // 缩放系数必须为正数，否则控件尺寸计算会全部失效。
    if (!Check(ui->ScaleX() > 0.0 && ui->ScaleY() > 0.0, "屏幕辅助缩放系数有效")) {
        return 3;
    }

    // root 是所有临时控件的父对象；Qt 父子对象机制会帮助统一回收子控件。
    QWidget root;
    // objectName 方便人工调试样式表，也便于自动化测试定位窗口。
    root.setObjectName("UIComponentsTestRoot");

    // 逐个创建常用控件，验证统一控件工厂不会返回空指针。
    // 等待页是 MainExe 启动阶段的通用控件，需要独立验证 objectName。
    QWidget* waitWidget = ui->CreateWaitWidget("Loading", "Preparing test widgets", &root);
    // 主按钮用于主要操作，例如登录后进入下一步。
    QPushButton* primary = ui->CreatePrimaryButton("Primary", &root);
    // 次按钮用于普通操作，例如取消或返回。
    QPushButton* secondary = ui->CreateSecondaryButton("Secondary", &root);
    // 危险按钮用于删除类动作，验证 role 参数能走到对应样式。
    QPushButton* danger = ui->CreateButton(MeyerButtonRoleDanger, MeyerButtonContentTextOnly, "Delete", "", &root);
    // 工具按钮覆盖图标在上、文字在下的工具栏类布局。
    QToolButton* tool = ui->CreateToolButton(MeyerButtonRoleSecondary, MeyerButtonContentIconTopText, "Tool", "", &root);
    // 输入框和下拉框是设置、建单、案例筛选都会用到的基础控件。
    QLineEdit* edit = ui->CreateLineEdit("Input", &root);
    QComboBox* combo = ui->CreateComboBox(&root);
    // 日期框和多行备注框也是建单、设置等页面的常见控件，应由共享 UI 统一基础外观。
    QDateEdit* dateEdit = ui->CreateDateEdit(&root);
    QTextEdit* textEdit = ui->CreateTextEdit(&root);
    // 字段标签用于表单项标题，调用方传入已经翻译后的显示文本。
    QLabel* fieldLabel = ui->CreateFieldLabel("Field", &root);
    // 表格控件用于病例、订单、医生、建单明细等列表类界面，基础外观应由共享 UI 统一。
    QTableWidget* table = ui->CreateTableWidget(&root);
    // 页面标题用于统一各 UI 模块的标题字号和字重。
    QLabel* title = ui->CreatePageTitle("Page Title", &root);

    // 等待页 objectName 固定，方便 MainExe 或样式系统识别。
    if (!Check(waitWidget && waitWidget->objectName() == "MeyerScanWaitWidget", "等待页控件创建成功")) {
        return 4;
    }
    // 所有控件一次性检查，确保控件工厂没有因为样式或父对象导致返回空。
    if (!Check(primary && secondary && danger && tool && edit && combo && dateEdit && textEdit && fieldLabel && table && title,
               "常用 UI 控件全部创建成功")) {
        return 5;
    }

    // 默认不显示窗口，自动化测试只验证控件树和样式入口；传 --show 时可人工查看。
    if (QCoreApplication::arguments().contains("--show")) {
        // 人工查看模式下给一个固定窗口尺寸，便于观察多控件布局和样式差异。
        root.resize(900, 600);
        root.show();
        return app.exec();
    }

    // 关闭 UIComponents 内部状态，避免测试进程退出时留下未释放资源。
    ui->Shutdown();
    std::printf("UIComponentsTest passed.\n");
    return 0;
}
