#include "OrderCreateUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>
#include <windows.h>
#include <cstdio>

namespace {

// 测试动作计数器。
// 回调函数只接收 void*，因此测试里把上下文转回这个简单结构体。
struct ActionCounter {
    int confirmCount = 0;
    int toothChangedCount = 0;
    int clearCount = 0;
};

// 测试断言辅助函数。
// 不引入 QtTest，保证 VS2015 双击运行也能看到最直接的 PASS/FAIL 输出。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// 建单动作回调。
// 这里模拟 MainExe 接收动作 ID，验证 DLL 可以把用户操作抛给外部流程。
void OnOrderCreateAction(void* context, int actionId) {
    // context 来自 SetActionCallback，测试中始终传 ActionCounter 指针。
    ActionCounter* counter = static_cast<ActionCounter*>(context);
    if (!counter) {
        return;
    }

    // 根据动作 ID 更新不同计数，便于 main 末尾断言。
    if (actionId == OrderCreateActionConfirm) {
        ++counter->confirmCount;
    } else if (actionId == OrderCreateActionToothSelectionChanged) {
        ++counter->toothChangedCount;
    } else if (actionId == OrderCreateActionClearAllTeeth) {
        ++counter->clearCount;
    }
}

}

// OrderCreateUI 测试入口：验证建单界面创建、标准上下文填充、牙位操作和动作回调。
int main(int argc, char* argv[]) {
    // 建单模块创建 QWidget，所以测试宿主必须使用 QApplication。
    QApplication app(argc, argv);

    // 命令行参数只在程序启动时解析一次。
    // 默认行为面向人工双击验收：显示界面并进入事件循环。
    // --smoke 面向自动化测试：执行控件检查和模拟点击后立即退出。
    const QStringList arguments = QCoreApplication::arguments();
    const bool smokeMode = arguments.contains("--smoke");

    // 双击运行时不需要黑色控制台窗口；测试输出只在 --smoke 模式保留。
    // FreeConsole 只分离当前进程控制台，不影响 Qt 主窗口显示。
    if (!smokeMode) {
        FreeConsole();
    }

    // applicationDirPath 指向测试 exe 所在目录，符合“不使用 currentPath 推导资源路径”的规则。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志统一放在 exe 同级 logs 目录，和正式 MeyerScan.exe 结构一致。
    const QString logDir = QDir(appDir).filePath("logs");
    // mkpath 在目录已存在时返回 true，适合测试启动前防御性创建日志目录。
    QDir().mkpath(logDir);

    // 工厂函数返回 DLL 内部单例，是验证导出符号和链接关系的第一步。
    IOrderCreateUI* orderCreate = GetOrderCreateUI();
    if (smokeMode && !Check(orderCreate != nullptr, "OrderCreateUI 工厂函数返回有效实例")) {
        return 1;
    }
    if (!smokeMode && orderCreate == nullptr) {
        QMessageBox::critical(nullptr, QObject::tr("OrderCreateUITest"), QObject::tr("Failed to load OrderCreateUI."));
        return 1;
    }

    // Init 传入应用目录和日志目录，后续真实资源查找也应通过这两个根路径实现。
    const bool initOk = orderCreate->Init(appDir.toUtf8().constData(), logDir.toUtf8().constData());
    if (smokeMode && !Check(initOk, "OrderCreateUI 初始化成功")) {
        return 2;
    }
    if (!smokeMode && !initOk) {
        QMessageBox::critical(nullptr, QObject::tr("OrderCreateUITest"), QObject::tr("Failed to initialize OrderCreateUI."));
        return 2;
    }

    // 注册动作回调，模拟 MainExe 或 OrderScanWorkspaceShell 接收建单操作。
    ActionCounter counter;
    orderCreate->SetActionCallback(&OnOrderCreateAction, &counter);

    // 标准建单上下文用于模拟第三方/HIS 已经完成字段归一化后的输入。
    // 先在 CreateWidget 前设置，验证模块能缓存上下文并在界面创建后自动应用。
    QJsonObject source;
    source.insert("launchType", "external");
    source.insert("thirdPartyType", "order_create_ui_test");
    source.insert("thirdPartyName", "OrderCreateUI Test");
    source.insert("sourceSystem", "unit-test");
    source.insert("sourceVersion", "0.1");
    QJsonObject patient;
    patient.insert("patientId", "CTX-P-001");
    patient.insert("name", "Context Patient");
    patient.insert("age", 31);
    patient.insert("birthDate", "1995/05/20");
    patient.insert("gender", "male");
    patient.insert("contact", "13900000000");
    patient.insert("note", "Context patient note.");
    QJsonObject order;
    order.insert("orderId", "CTX-O-001");
    order.insert("doctor", "Dr. Context");
    order.insert("lab", "Context Lab");
    order.insert("deliveryDate", "2026/07/09");
    order.insert("caseType", "restoration");
    order.insert("note", "Context order note.");
    QJsonArray items;
    QJsonObject firstTooth;
    firstTooth.insert("tooth", 11);
    firstTooth.insert("type", "crown");
    items.append(firstTooth);
    QJsonObject secondTooth;
    secondTooth.insert("tooth", 36);
    secondTooth.insert("type", "inlay");
    items.append(secondTooth);
    QJsonObject scanPlan;
    scanPlan.insert("items", items);
    QJsonObject contextRoot;
    contextRoot.insert("schemaVersion", 1);
    contextRoot.insert("source", source);
    contextRoot.insert("patient", patient);
    contextRoot.insert("order", order);
    contextRoot.insert("scanPlan", scanPlan);
    const QByteArray contextBytes = QJsonDocument(contextRoot).toJson(QJsonDocument::Compact);
    if (!Check(orderCreate->SetOrderContextJson(contextBytes.constData()), "CreateWidget 前设置标准建单上下文成功")) {
        return 15;
    }

    // CreateWidget 只创建根界面，不主动 show，由调用方决定嵌入方式。
    QWidget* widget = orderCreate->CreateWidget();
    if (smokeMode && !Check(widget != nullptr, "OrderCreateUI 能创建根 QWidget")) {
        return 3;
    }
    if (!smokeMode && widget == nullptr) {
        QMessageBox::critical(nullptr, QObject::tr("OrderCreateUITest"), QObject::tr("Failed to create order widget."));
        return 3;
    }

    // 人工模式只显示初始界面，不执行后面的模拟点击测试。
    // 这样双击 exe 看到的就是模块真实初始状态，而不是被测试代码清空后的状态。
    if (!smokeMode) {
        widget->resize(1440, 860);
        widget->show();
        const int result = app.exec();
        orderCreate->Shutdown();
        return result;
    }

    if (!Check(widget->objectName() == "MeyerScanOrderCreateUIRoot", "建单根对象名正确")) {
        return 4;
    }

    // 查找核心控件，验证初版页面骨架已经完整创建。
    auto* nameEdit = widget->findChild<QLineEdit*>("OrderCreatePatientNameEdit");
    auto* table = widget->findChild<QTableWidget*>("OrderCreateSelectionTable");
    auto* toothButton = widget->findChild<QPushButton*>("OrderCreateTooth15Button");
    auto* clearButton = widget->findChild<QPushButton*>("OrderCreateClearAllButton");
    auto* confirmButton = widget->findChild<QPushButton*>("OrderCreateConfirmButton");
    if (!Check(nameEdit != nullptr, "能找到患者姓名输入框")) {
        return 5;
    }
    if (!Check(table != nullptr, "能找到已选牙位表格")) {
        return 6;
    }
    if (!Check(toothButton != nullptr, "能找到 15 号牙位按钮")) {
        return 7;
    }
    if (!Check(clearButton != nullptr && confirmButton != nullptr, "能找到清空和确认按钮")) {
        return 8;
    }

    // 设置上下文后，默认示例牙位应被上下文里的 11/36 两颗牙覆盖。
    if (!Check(nameEdit->text() == "Context Patient", "上下文患者姓名已填充到界面")) {
        return 9;
    }
    if (!Check(table->rowCount() == 2, "上下文牙位明细为 2 行")) {
        return 9;
    }

    // 模拟点击 15 号牙位，上下文里原本没有 15，点击后应新增并触发牙位变化回调。
    toothButton->click();
    if (!Check(table->rowCount() == 3, "点击未选牙位后明细增加为 3 行")) {
        return 10;
    }
    if (!Check(counter.toothChangedCount == 1, "牙位变化动作回调触发一次")) {
        return 11;
    }

    // 模拟清空全部牙位，表格应清空并触发清空动作。
    clearButton->click();
    if (!Check(table->rowCount() == 0, "清空后明细表为空")) {
        return 12;
    }
    if (!Check(counter.clearCount == 1, "清空动作回调触发一次")) {
        return 13;
    }

    // 模拟确认按钮，验证外部流程能收到 Confirm 动作 ID。
    confirmButton->click();
    if (!Check(counter.confirmCount == 1, "确认动作回调触发一次")) {
        return 14;
    }

    // 自动化模式下删除根控件，验证模块不依赖父对象才可退出。
    delete widget;
    // Shutdown 清理单例状态，便于后续测试在同一进程中重复初始化。
    orderCreate->Shutdown();
    std::printf("OrderCreateUITest passed.\n");
    return 0;
}

