#include "CalibrationColorUI.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QMouseEvent>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <cstdio>
#include <cstring>

namespace {

// 测试断言辅助函数。
// 使用简单 C 输出和返回码，保证 VS2015、命令行、批处理脚本都能直接读取结果。
bool Check(bool condition, const char* message) {
    // 通过时写 stdout，作为测试步骤记录。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败时写 stderr，便于自动化脚本单独收集失败原因。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// 颜色校准 UI 测试入口：验证 DLL 工厂、初始化、根控件创建和可选人工显示流程。
int main(int argc, char* argv[]) {
    // 颜色校准模块创建 QWidget，必须使用 QApplication 初始化 Qt GUI 环境。
    QApplication app(argc, argv);
    // 双击测试程序属于人工验收模式，默认显示界面；只有自动化测试显式传入
    // --smoke 时才创建、检查并立即销毁页面，避免 CTest 卡在事件循环中。
    const bool smokeMode = app.arguments().contains("--smoke");
    const bool dragTestMode = app.arguments().contains("--drag-test");
    // --capture-screenshot <png> 用于固定尺寸视觉验收，保存后自动退出。
    QString capturePath;
    const int captureArgumentIndex = app.arguments().indexOf("--capture-screenshot");
    if (captureArgumentIndex >= 0 && captureArgumentIndex + 1 < app.arguments().size()) {
        capturePath = app.arguments().at(captureArgumentIndex + 1);
    }

    // 所有路径都从测试 exe 所在目录推导，避免 currentPath 变化导致资源加载错误。
    const QString appDir = QCoreApplication::applicationDirPath();
    // 日志目录与正式 MeyerScan.exe 保持同级 logs 结构。
    const QString logDir = QDir(appDir).filePath("logs");
    // 先创建日志目录，避免 Init 内部写首条日志时失败。
    QDir().mkpath(logDir);
    // 显式持有 UTF-8 字节，避免跨 DLL 示例依赖临时 QByteArray 的隐式生命周期。
    const QByteArray appDirUtf8 = appDir.toUtf8();
    const QByteArray logDirUtf8 = logDir.toUtf8();

    // 验证 DLL 导出的工厂函数能返回有效接口。
    ICalibrationColorUI* calibration = GetCalibrationColorUI();
    if (!Check(calibration != nullptr, "CalibrationColorUI 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 当前只打通路径和日志，后续颜色校准算法资源也从这里扩展。
    if (!Check(calibration->Init(appDirUtf8.constData(), logDirUtf8.constData()),
               "CalibrationColorUI 初始化成功")) {
        return 2;
    }

    // 独立测试没有真实设备，由测试宿主注入一份已经通过预检的确定性设备快照。
    // 正式 MeyerScan.exe 中该结构来自 DeviceSessionHost -> DeviceCmd -> DeviceTransport。
    CalibrationColorDeviceContext deviceContext = {};
    deviceContext.structSize = sizeof(deviceContext);
    deviceContext.schemaVersion = MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    deviceContext.deviceModel = 5;
    deviceContext.modelSource = 3;
    deviceContext.connectionState = 1;
    deviceContext.isUsb2 = 0;
    std::strcpy(deviceContext.modelNameUtf8, "MyScan 5");
    std::strcpy(deviceContext.deviceIdUtf8, "6200005301203");
    std::strcpy(deviceContext.modelCodeUtf8, "62000053");
    deviceContext.productEvidence = 0x17U;
    deviceContext.productFamily = 5;
    deviceContext.productModel = 5001;
    deviceContext.productIdentificationStatus = 2;
    deviceContext.protocolProfile = 5;
    std::strcpy(deviceContext.productSeriesNameUtf8, "mOS MyScan 5");
    std::strcpy(deviceContext.productNameUtf8, "mOS MyScan 5/mOS MyScan 5");
    // 模拟精确检测结果：真实上报值与最终有效值一致，不依赖兼容默认值。
    deviceContext.detection.structSize = sizeof(deviceContext.detection);
    deviceContext.detection.schemaVersion =
        MEYER_CALIBRATION_COLOR_DETECTION_SCHEMA_VERSION;
    deviceContext.detection.detectionStatus = CalibrationColorDeviceDetectionExact;
    deviceContext.detection.deviceNumberStatus = 1;
    deviceContext.detection.modelCodeStatus = 1;
    deviceContext.detection.seriesProbeStatus = 1;
    deviceContext.detection.isProductionMode = 0;
    deviceContext.detection.usedCompatibilityDefaults = 0;
    deviceContext.detection.deviceNumberSource = 1;
    deviceContext.detection.modelCodeSource = 1;
    std::strcpy(deviceContext.detection.reportedDeviceNumberUtf8, "6200005301203");
    std::strcpy(deviceContext.detection.effectiveDeviceNumberUtf8, "6200005301203");
    std::strcpy(deviceContext.detection.reportedModelCodeUtf8, "62000053");
    std::strcpy(deviceContext.detection.effectiveModelCodeUtf8, "62000053");
    std::strcpy(deviceContext.detection.detailUtf8, "Exact simulated device identity");
    // MyScan 5 只有主控板：主控板版本有效，投图板状态明确为 NotRequired。
    deviceContext.firmwareVersions.structSize = sizeof(deviceContext.firmwareVersions);
    deviceContext.firmwareVersions.schemaVersion =
        MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    deviceContext.firmwareVersions.mainBoardStatus =
        CalibrationColorFirmwareVersionValid;
    deviceContext.firmwareVersions.projectionBoardStatus =
        CalibrationColorFirmwareVersionNotRequired;
    std::strcpy(deviceContext.firmwareVersions.mainBoardVersionUtf8, "1.3.1001");
    // 独立测试模拟 MyScan 5 的双扫描头策略，两套颜色参数均已写入。
    deviceContext.scanHeadColorCalibration.structSize =
        sizeof(deviceContext.scanHeadColorCalibration);
    deviceContext.scanHeadColorCalibration.schemaVersion =
        MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION;
    deviceContext.scanHeadColorCalibration.policy =
        CalibrationColorScanHeadPolicyLargeAndSmall;
    deviceContext.scanHeadColorCalibration.firmwareCompatibility =
        CalibrationColorFirmwareCompatibilitySupported;
    deviceContext.scanHeadColorCalibration.largeHeadStatus =
        CalibrationColorScanHeadCalibrated;
    deviceContext.scanHeadColorCalibration.smallHeadStatus =
        CalibrationColorScanHeadCalibrated;
    deviceContext.scanHeadColorCalibration.largeHeadCommandResult = 0;
    deviceContext.scanHeadColorCalibration.smallHeadCommandResult = 0;
    if (!Check(calibration->SetDeviceContext(&deviceContext),
               "CalibrationColorUI 接受已验证设备快照")) {
        calibration->Shutdown();
        return 3;
    }

    // 由模块创建根 QWidget，调用方只负责嵌入或显示，保持 UI 资源边界清晰。
    QWidget* widget = calibration->CreateWidget();
    if (!Check(widget != nullptr, "CalibrationColorUI 能创建根 QWidget")) {
        // 即使页面创建失败，也要让 DLL 释放已经初始化的日志和路径状态。
        calibration->Shutdown();
        return 4;
    }
    // objectName 是测试和样式定位的稳定锚点，不能随意修改。
    if (!Check(widget->objectName() == "MeyerScanCalibrationColorUIRoot", "颜色校准根对象名正确")) {
        // 失败路径同样显式释放 QWidget，便于反复运行测试时检查资源生命周期。
        delete widget;
        calibration->Shutdown();
        return 5;
    }
    // 根控件属性应使用 effective 值，后续颜色算法无需理解生产模式兼容规则。
    if (!Check(widget->property("deviceId").toString() == "6200005301203" &&
               widget->property("modelCode").toString() == "62000053" &&
               widget->property("scanHeadColorCalibrationPolicy").toInt() ==
                   CalibrationColorScanHeadPolicyLargeAndSmall &&
               widget->property("largeScanHeadColorCalibrationStatus").toInt() ==
                   CalibrationColorScanHeadCalibrated &&
               widget->property("smallScanHeadColorCalibrationStatus").toInt() ==
                   CalibrationColorScanHeadCalibrated &&
               widget->property("mainBoardFirmwareVersion").toString() == "1.3.1001" &&
               widget->property("projectionBoardFirmwareVersion").toString().isEmpty() &&
               widget->property("deviceDetectionStatus").toInt() ==
                   CalibrationColorDeviceDetectionExact,
               "颜色校准根控件记录完整设备检测结果")) {
        delete widget;
        calibration->Shutdown();
        return 6;
    }

    // 通过 objectName 验证参考界面的四个关键控件，避免未来误退回只有空白根控件的骨架实现。
    QWidget* preview = widget->findChild<QWidget*>("CalibrationColorPreview");
    QPushButton* closeButton = widget->findChild<QPushButton*>("CalibrationColorCloseButton");
    QPushButton* calibrateButton = widget->findChild<QPushButton*>("CalibrationColorCalibrateButton");
    QPushButton* exitButton = widget->findChild<QPushButton*>("CalibrationColorExitButton");
    const bool structureValid = preview &&
                                closeButton &&
                                calibrateButton &&
                                exitButton &&
                                calibrateButton->property("calibrationAction").toBool() &&
                                exitButton->property("calibrationAction").toBool();
    if (!Check(structureValid, "颜色校准参考界面关键控件完整")) {
        delete widget;
        calibration->Shutdown();
        return 7;
    }

    if (dragTestMode) {
        // 显示后再发送鼠标事件，保证顶层窗口拥有有效的屏幕坐标。
        widget->resize(450, 585);
        widget->show();
        app.processEvents();
        QFrame* titleBar = widget->findChild<QFrame*>("CalibrationColorTitleBar");
        const QPoint localPress(100, 20);
        const QPoint dragDelta(36, 24);
        const QPoint globalPress = titleBar ? titleBar->mapToGlobal(localPress) : QPoint();
        const QPoint initialPosition = widget->pos();
        if (!titleBar) {
            delete widget;
            calibration->Shutdown();
            return 8;
        }

        // 依次发送按下、移动、释放三个事件，模拟用户拖动标题栏的完整鼠标手势。
        QMouseEvent press(QEvent::MouseButtonPress,
                          QPointF(localPress),
                          QPointF(localPress),
                          QPointF(globalPress),
                          Qt::LeftButton,
                          Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(titleBar, &press);
        const QPoint localMove = localPress + dragDelta;
        QMouseEvent move(QEvent::MouseMove,
                         QPointF(localMove),
                         QPointF(localMove),
                         QPointF(globalPress + dragDelta),
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(titleBar, &move);
        QMouseEvent release(QEvent::MouseButtonRelease,
                            QPointF(localMove),
                            QPointF(localMove),
                            QPointF(globalPress + dragDelta),
                            Qt::LeftButton,
                            Qt::NoButton,
                            Qt::NoModifier);
        QApplication::sendEvent(titleBar, &release);
        const bool moved = widget->pos() == initialPosition + dragDelta;
        Check(moved, "颜色校准标题栏拖动会改变窗口位置");
        delete widget;
        calibration->Shutdown();
        return moved ? 0 : 9;
    }

    // smoke 模式不显示窗口、不进入事件循环，只验证 DLL 链路和页面生命周期。
    if (smokeMode) {
        // 自动化模式下手动释放根控件，保证模块可以被创建和销毁。
        delete widget;
        // 释放模块内部状态，后续接入算法资源时也从这里做清理。
        calibration->Shutdown();
        std::printf("CalibrationColorUITest passed.\n");
        return 0;
    }

    // 人工模式使用接近设置模块内容区的尺寸，双击 exe 即可直接检查界面。
    widget->setWindowTitle(QCoreApplication::translate(
        "CalibrationColorUITest", "Color Calibration UI Test"));
    // 450x585 对应参考图中不含外部阴影的实际面板；模块内部布局仍允许小屏适度收缩。
    widget->resize(450, 585);
    widget->show();

    // 用户关闭最后一个窗口时 QApplication 会退出事件循环；aboutToQuit 信号
    // 在栈上的 app 析构前触发，因此这里可以按“页面 -> DLL 状态”的顺序清理。
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [calibration, widget]() {
        delete widget;
        calibration->Shutdown();
    });

    if (!capturePath.isEmpty()) {
        // 等待一次事件循环完成 QSS、布局和资源图片绘制后再抓取根窗口。
        QTimer::singleShot(300, &app, [&app, widget, capturePath]() {
            const QPixmap screenshot = widget->grab();
            // 退出码 0/5 让自动脚本能区分截图成功和磁盘写入失败。
            app.exit(screenshot.save(capturePath, "PNG") ? 0 : 5);
        });
    }
    return app.exec();
}
