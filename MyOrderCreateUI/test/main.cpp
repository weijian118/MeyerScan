#include "OrderCreateUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
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
    // --capture-screenshot <png> 面向视觉验收：固定 1920x1080 渲染并保存截图。
    const QStringList arguments = QCoreApplication::arguments();
    const bool smokeMode = arguments.contains("--smoke");
    const int captureArgumentIndex = arguments.indexOf("--capture-screenshot");
    const bool captureMode = captureArgumentIndex >= 0;
    QString capturePath = QDir::temp().filePath("OrderCreateUITest_treatment_plan_latest.png");
    if (captureMode && captureArgumentIndex + 1 < arguments.size()) {
        // 命令行显式给出路径时使用该路径，便于和视频关键帧放到同一个目录对比。
        capturePath = arguments.at(captureArgumentIndex + 1);
    }
    QSize captureSize(1920, 1080);
    const int captureSizeArgumentIndex = arguments.indexOf("--capture-size");
    if (captureSizeArgumentIndex >= 0 && captureSizeArgumentIndex + 1 < arguments.size()) {
        // 截图尺寸由测试参数控制，用同一套布局验证 1920x1080 和 1366x768。
        const QStringList parts = arguments.at(captureSizeArgumentIndex + 1).toLower().split('x');
        bool widthOk = false;
        bool heightOk = false;
        const int width = parts.size() == 2 ? parts.at(0).toInt(&widthOk) : 0;
        const int height = parts.size() == 2 ? parts.at(1).toInt(&heightOk) : 0;
        if (widthOk && heightOk && width >= 960 && height >= 600) {
            captureSize = QSize(width, height);
        }
    }
    // 模块代码中的确认框在 smoke 模式下会阻塞自动化测试。
    // 通过 QApplication 动态属性告知模块：当前是自动测试，危险操作可直接确认。
    app.setProperty("MeyerScanSmokeTest", smokeMode || captureMode);

    // 双击运行时不需要黑色控制台窗口；测试输出只在 --smoke 模式保留。
    // FreeConsole 只分离当前进程控制台，不影响 Qt 主窗口显示。
    if (!smokeMode && !captureMode) {
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
    if (captureMode) {
        // 截图模式使用更接近视频关键帧的牙位组合：上颌种植/贴面、下颌贴面混合显示。
        // 这样截图能同时覆盖类型叠加图、右侧明细表和清空按钮周围布局。
        items = QJsonArray();

        QJsonObject tooth12;
        tooth12.insert("tooth", 12);
        tooth12.insert("type", "implant");
        items.append(tooth12);

        QJsonObject tooth13;
        tooth13.insert("tooth", 13);
        tooth13.insert("type", "implant");
        items.append(tooth13);

        QJsonObject tooth14;
        tooth14.insert("tooth", 14);
        tooth14.insert("type", "veneer");
        items.append(tooth14);

        QJsonObject tooth16;
        tooth16.insert("tooth", 16);
        tooth16.insert("type", "veneer");
        items.append(tooth16);

        QJsonObject tooth47;
        tooth47.insert("tooth", 47);
        tooth47.insert("type", "veneer");
        items.append(tooth47);

        QJsonObject tooth48;
        tooth48.insert("tooth", 48);
        tooth48.insert("type", "veneer");
        items.append(tooth48);
    }

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

    // 截图模式用于视觉验收：不执行 smoke 后续点击，避免截图状态被测试逻辑修改。
    if (captureMode) {
        widget->resize(captureSize);
        widget->show();

        int captureExitCode = 0;
        QTimer::singleShot(500, [&]() {
            // QWidget::grab 只截取建单模块内容，不包含 Windows 桌面和窗口边框，便于和视频内容区对比。
            const QPixmap pixmap = widget->grab();
            if (!pixmap.save(capturePath)) {
                captureExitCode = 30;
                std::fprintf(stderr, "[FAIL] failed to save screenshot: %s\n", capturePath.toUtf8().constData());
            } else {
                std::printf("[PASS] screenshot saved: %s\n", capturePath.toUtf8().constData());
            }
            app.quit();
        });

        const int result = app.exec();
        orderCreate->Shutdown();
        return captureExitCode == 0 ? result : captureExitCode;
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
    auto* treatmentPlanWidget = widget->findChild<QWidget*>("OrderCreateTreatmentPlanWidget");
    auto* bridgeSummaryLabel = widget->findChild<QLabel*>("OrderCreateBridgeSummaryLabel");
    auto* clearButton = widget->findChild<QPushButton*>("OrderCreateClearAllButton");
    auto* confirmButton = widget->findChild<QPushButton*>("OrderCreateConfirmButton");
    if (!Check(nameEdit != nullptr, "能找到患者姓名输入框")) {
        return 5;
    }
    if (!Check(table != nullptr, "能找到已选牙位表格")) {
        return 6;
    }
    if (!Check(treatmentPlanWidget != nullptr, "能找到治疗方案图片控件")) {
        return 7;
    }
    if (!Check(clearButton != nullptr && confirmButton != nullptr, "能找到清空和确认按钮")) {
        return 8;
    }
    if (!Check(bridgeSummaryLabel != nullptr, "能找到桥记录摘要标签")) {
        return 8;
    }

    // 设置上下文后，默认示例牙位应被上下文里的 11/36 两颗牙覆盖。
    if (!Check(nameEdit->text() == "Context Patient", "上下文患者姓名已填充到界面")) {
        return 9;
    }
    if (!Check(table->rowCount() == 2, "上下文牙位明细为 2 行")) {
        return 9;
    }

    // 新版治疗方案已经从按钮矩阵改成图片 + mask 控件。
    // smoke 不用鼠标坐标点击图片，而是通过标准上下文再次注入扫描方案，验证同一套状态刷新链路。
    QJsonArray updatedItems;
    updatedItems.append(firstTooth);
    updatedItems.append(secondTooth);
    QJsonObject thirdTooth;
    thirdTooth.insert("tooth", 15);
    thirdTooth.insert("type", "crown");
    updatedItems.append(thirdTooth);
    QJsonObject updatedScanPlan;
    updatedScanPlan.insert("items", updatedItems);
    QJsonObject updatedContextRoot = contextRoot;
    updatedContextRoot.insert("scanPlan", updatedScanPlan);
    const QByteArray updatedContextBytes = QJsonDocument(updatedContextRoot).toJson(QJsonDocument::Compact);
    if (!Check(orderCreate->SetOrderContextJson(updatedContextBytes.constData()), "通过标准上下文更新治疗方案成功")) {
        return 10;
    }
    if (!Check(table->rowCount() == 3, "新增牙位后明细增加为 3 行")) {
        return 10;
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

    // 新增扫描流程控件必须存在，建单页通过这些输入影响后续 Scan/Process 按钮。
    auto* maxillaDiffRod = widget->findChild<QCheckBox*>("OrderCreateMaxillaDiffRodSwitch");
    auto* maxillaSegmentedRod = widget->findChild<QCheckBox*>("OrderCreateMaxillaSegmentedRodSwitch");
    auto* occlusionCombo = widget->findChild<QComboBox*>("OrderCreateOcclusionTypeCombo");
    if (!Check(maxillaDiffRod != nullptr && maxillaSegmentedRod != nullptr && occlusionCombo != nullptr,
               "能找到扫描流程配置开关和咬合类型下拉框")) {
        return 16;
    }

    // 模拟用户选择上颌异性扫描杆、上颌分段和咬合记录，验证输出 JSON 中包含对应流程按钮。
    maxillaDiffRod->setChecked(true);
    maxillaSegmentedRod->setChecked(true);
    const int biteRecordIndex = occlusionCombo->findData("record");
    if (biteRecordIndex >= 0) {
        occlusionCombo->setCurrentIndex(biteRecordIndex);
    }
    const QByteArray scanProcessBytes(orderCreate->GetCurrentScanProcessJson());
    const QJsonDocument scanProcessDocument = QJsonDocument::fromJson(scanProcessBytes);
    const QJsonArray scanSteps = scanProcessDocument.object().value("steps").toArray();
    bool hasDiffRod2 = false;
    bool hasBiteRecord = false;
    for (const QJsonValue& stepValue : scanSteps) {
        const QJsonObject step = stepValue.toObject();
        if (step.value("code").toString() == "maxilla_diff_rod_2") {
            hasDiffRod2 = true;
        }
        if (step.value("code").toString() == "bite_record") {
            hasBiteRecord = true;
        }
    }
    if (!Check(hasDiffRod2 && hasBiteRecord, "扫描流程 JSON 根据建单控件生成异性杆分段和咬合记录步骤")) {
        return 17;
    }

    // 验证外部脏数据：如果 bridgeConnectors 指向的两颗牙没有同时设置为 bridge 类型，
    // UI 不应该显示桥记录，扫描流程 JSON 也不应该保存该桥区间。
    QJsonArray invalidBridgeItems;
    QJsonObject invalidBridgeTooth16;
    invalidBridgeTooth16.insert("tooth", 16);
    invalidBridgeTooth16.insert("type", "crown");
    invalidBridgeItems.append(invalidBridgeTooth16);
    QJsonObject invalidBridgeTooth17;
    invalidBridgeTooth17.insert("tooth", 17);
    invalidBridgeTooth17.insert("type", "bridge");
    invalidBridgeItems.append(invalidBridgeTooth17);
    QJsonArray invalidBridgeConnectors;
    invalidBridgeConnectors.append("16-17");
    QJsonObject invalidBridgeScanPlan;
    invalidBridgeScanPlan.insert("items", invalidBridgeItems);
    invalidBridgeScanPlan.insert("bridgeConnectors", invalidBridgeConnectors);
    QJsonObject invalidBridgeContextRoot = contextRoot;
    invalidBridgeContextRoot.insert("scanPlan", invalidBridgeScanPlan);
    const QByteArray invalidBridgeContextBytes = QJsonDocument(invalidBridgeContextRoot).toJson(QJsonDocument::Compact);
    if (!Check(orderCreate->SetOrderContextJson(invalidBridgeContextBytes.constData()), "外部脏桥连接点上下文设置成功")) {
        return 18;
    }
    if (!Check(!bridgeSummaryLabel->text().contains("16-17"), "非 bridge 牙位对应的桥连接点不会显示")) {
        return 19;
    }

    // 验证桥记录：16-17 和 17-18 连续连接后应合并显示为 16-18。
    QJsonArray bridgeItems;
    QJsonObject bridgeTooth16;
    bridgeTooth16.insert("tooth", 16);
    bridgeTooth16.insert("type", "bridge");
    bridgeItems.append(bridgeTooth16);
    QJsonObject bridgeTooth17;
    bridgeTooth17.insert("tooth", 17);
    bridgeTooth17.insert("type", "bridge");
    bridgeItems.append(bridgeTooth17);
    QJsonObject bridgeTooth18;
    bridgeTooth18.insert("tooth", 18);
    bridgeTooth18.insert("type", "bridge");
    bridgeItems.append(bridgeTooth18);
    QJsonArray bridgeConnectors;
    // 第一条故意使用反向 key，验证模块会归一化为稳定的 "16-17"。
    bridgeConnectors.append("17-16");
    bridgeConnectors.append("17-18");
    QJsonObject bridgeScanPlan;
    bridgeScanPlan.insert("items", bridgeItems);
    bridgeScanPlan.insert("bridgeConnectors", bridgeConnectors);
    QJsonObject bridgeContextRoot = contextRoot;
    bridgeContextRoot.insert("scanPlan", bridgeScanPlan);
    const QByteArray bridgeContextBytes = QJsonDocument(bridgeContextRoot).toJson(QJsonDocument::Compact);
    if (!Check(orderCreate->SetOrderContextJson(bridgeContextBytes.constData()), "桥治疗方案上下文设置成功")) {
        return 20;
    }
    if (!Check(bridgeSummaryLabel->text().contains("16-18"), "连续桥连接点合并显示为 16-18")) {
        return 21;
    }
    const QByteArray bridgeScanProcessBytes(orderCreate->GetCurrentScanProcessJson());
    const QJsonObject bridgeConfig = QJsonDocument::fromJson(bridgeScanProcessBytes).object().value("config").toObject();
    const QJsonArray bridgeRanges = bridgeConfig.value("scanPlan").toObject().value("bridgeRanges").toArray();
    bool hasBridgeRange = false;
    for (const QJsonValue& value : bridgeRanges) {
        if (value.toString() == "16-18") {
            hasBridgeRange = true;
        }
    }
    if (!Check(hasBridgeRange, "扫描流程 JSON 中包含桥区间 16-18")) {
        return 22;
    }

    // 验证跨中线桥记录：11-12 和 11-21 连续连接后，应按旧软件规则显示为 11-22。
    QJsonArray midlineBridgeItems;
    QJsonObject bridgeTooth11;
    bridgeTooth11.insert("tooth", 11);
    bridgeTooth11.insert("type", "bridge");
    midlineBridgeItems.append(bridgeTooth11);
    QJsonObject bridgeTooth12;
    bridgeTooth12.insert("tooth", 12);
    bridgeTooth12.insert("type", "bridge");
    midlineBridgeItems.append(bridgeTooth12);
    QJsonObject bridgeTooth21;
    bridgeTooth21.insert("tooth", 21);
    bridgeTooth21.insert("type", "bridge");
    midlineBridgeItems.append(bridgeTooth21);
    QJsonArray midlineBridgeConnectors;
    midlineBridgeConnectors.append("11-12");
    midlineBridgeConnectors.append("11-21");
    QJsonObject midlineBridgeScanPlan;
    midlineBridgeScanPlan.insert("items", midlineBridgeItems);
    midlineBridgeScanPlan.insert("bridgeConnectors", midlineBridgeConnectors);
    QJsonObject midlineBridgeContextRoot = contextRoot;
    midlineBridgeContextRoot.insert("scanPlan", midlineBridgeScanPlan);
    const QByteArray midlineBridgeContextBytes = QJsonDocument(midlineBridgeContextRoot).toJson(QJsonDocument::Compact);
    if (!Check(orderCreate->SetOrderContextJson(midlineBridgeContextBytes.constData()), "跨中线桥治疗方案上下文设置成功")) {
        return 23;
    }
    if (!Check(bridgeSummaryLabel->text().contains("11-22"), "跨中线桥连接点合并显示为 11-22")) {
        return 24;
    }

    // 自动化模式下删除根控件，验证模块不依赖父对象才可退出。
    delete widget;
    // Shutdown 清理单例状态，便于后续测试在同一进程中重复初始化。
    orderCreate->Shutdown();
    std::printf("OrderCreateUITest passed.\n");
    return 0;
}

