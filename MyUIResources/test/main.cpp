#include "UIResources.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>

#include <Windows.h>

#include <cstdio>

// 输出单项检查结果并返回是否成功。
// 测试保持简单的控制台输出，便于 VS2015、CMake 和自动化脚本共同调用。
bool Check(bool condition, const char* name) {
    // 统一使用 ASCII 输出，避免不同控制台代码页影响自动化日志读取。
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    return condition;
}

// 资源 DLL smoke 测试入口。
// 重点验证 DLL 注册、QSS 读取、PNG 定位、版本导出和注销生命周期。
int main(int argc, char* argv[]) {
    // QResource 注册只需要 Qt Core，不创建任何 QWidget，因此使用 QCoreApplication 即可。
    QCoreApplication app(argc, argv);

    // failed 累计全部检查结果，不在第一项失败时退出，便于一次看到完整资源问题。
    int failed = 0;

    // 初始化接口必须幂等；连续调用两次都应成功且只注册同一份资源树。
    failed += Check(MeyerScanInitializeUiResources(), "initialize embedded resources") ? 0 : 1;
    failed += Check(MeyerScanInitializeUiResources(), "repeated initialize is idempotent") ? 0 : 1;
    failed += Check(MeyerScanUiResourcesInitialized(), "resource state is initialized") ? 0 : 1;

    // 合同查询接口让新宿主在初始化前确认 DLL、RCDATA 和 qrc 路径属于同一批次。
    failed += Check(GetMeyerUiResourcesApiVersion() == MEYER_UI_RESOURCE_API_VERSION,
                    "resource API version matches public contract") ? 0 : 1;
    failed += Check(GetMeyerUiResourcesPayloadId() == MEYER_UI_RESOURCE_PAYLOAD_ID,
                    "RCDATA payload id matches public contract") ? 0 : 1;
    failed += Check(GetMeyerUiResourcesManifestSchemaVersion() ==
                        MEYER_UI_RESOURCE_MANIFEST_SCHEMA_VERSION,
                    "resource manifest schema matches public contract") ? 0 : 1;
    failed += Check(QString::fromLatin1(GetMeyerUiResourcesPrefix()) ==
                        QString::fromLatin1(MEYER_UI_RESOURCE_QRC_PREFIX),
                    "qrc prefix matches public contract") ? 0 : 1;

    // 显式从已加载 DLL 查找合同声明的 RCDATA，避免初始化函数和 Version.rc
    // 因人工修改不同步而仅在客户机器上暴露问题。
    const HMODULE resourceModule = GetModuleHandleW(L"MeyerScan_UIResources.dll");
    const HRSRC resourcePayload = resourceModule
        ? FindResourceW(resourceModule,
                        MAKEINTRESOURCEW(GetMeyerUiResourcesPayloadId()),
                        MAKEINTRESOURCEW(10))
        : nullptr;
    failed += Check(resourcePayload != nullptr,
                    "declared RCDATA payload exists in resource dll") ? 0 : 1;

    // 同时抽查 QSS 和 PNG，防止清单只收录了某一种资源类型。
    const QString resourceRoot = QString::fromLatin1(MEYER_UI_RESOURCE_RUNTIME_ROOT);
    const QString homeQssPath = resourceRoot + "/MyHomeUI/qss/home.qss";
    const QString homeIconPath = resourceRoot + "/MyHomeUI/icon/home/HomeCreate_b.png";
    failed += Check(QFile::exists(homeQssPath), "HomeUI qss exists in resource dll") ? 0 : 1;
    failed += Check(QFile::exists(homeIconPath), "HomeUI icon exists in resource dll") ? 0 : 1;

    // 颜色校准本轮新增三张图片和新版 QSS；逐项断言可防止清单规则或路径拼写漏包。
    const QString colorQssPath = resourceRoot +
        "/MyCalibrationColorUI/qss/calibration_color.qss";
    const QString colorPreviewPath =
        resourceRoot + "/MyCalibrationColorUI/icon/color_calibration/init_image.png";
    const QString colorCloseNormalPath =
        resourceRoot + "/MyCalibrationColorUI/icon/color_calibration/close_b.png";
    const QString colorCloseHoverPath =
        resourceRoot + "/MyCalibrationColorUI/icon/color_calibration/close_h.png";
    failed += Check(QFile::exists(colorQssPath), "CalibrationColorUI qss exists in resource dll") ? 0 : 1;
    failed += Check(QFile::exists(colorPreviewPath), "CalibrationColorUI preview exists in resource dll") ? 0 : 1;
    failed += Check(QFile::exists(colorCloseNormalPath), "CalibrationColorUI normal close icon exists") ? 0 : 1;
    failed += Check(QFile::exists(colorCloseHoverPath), "CalibrationColorUI hover close icon exists") ? 0 : 1;

    QFile colorQssFile(colorQssPath);
    const bool colorQssOpened = colorQssFile.open(QIODevice::ReadOnly | QIODevice::Text);
    failed += Check(colorQssOpened, "CalibrationColorUI qss can be opened") ? 0 : 1;
    if (colorQssOpened) {
        // 同时检查根选择器和资源占位符，确认拿到的是本轮新版样式而非旧骨架 QSS。
        const QByteArray colorQss = colorQssFile.readAll();
        const bool colorQssIsCurrent = colorQss.contains("MeyerScanCalibrationColorUIRoot") &&
                                       colorQss.contains("color_calibration/close_h.png");
        failed += Check(colorQssIsCurrent, "CalibrationColorUI qss content is current") ? 0 : 1;
    }

    QFile qssFile(homeQssPath);
    const bool qssOpened = qssFile.open(QIODevice::ReadOnly | QIODevice::Text);
    failed += Check(qssOpened, "embedded qss can be opened") ? 0 : 1;
    if (qssOpened) {
        // 读取后检查根控件选择器，避免资源存在但内容为空或清单指向错误文件。
        const QByteArray qss = qssFile.readAll();
        failed += Check(qss.contains("MeyerScanHomeUIRoot"), "embedded qss content is correct") ? 0 : 1;
    }

    // 版本函数返回 DLL 内静态字符串，测试只验证非空，不尝试释放。
    const char* version = GetMeyerModuleVersion();
    failed += Check(version && version[0], "code version export is available") ? 0 : 1;

    // 注销主要供测试使用；正式进程应在所有 UI 销毁前一直保持资源注册。
    MeyerScanShutdownUiResources();
    failed += Check(!MeyerScanUiResourcesInitialized(), "resource unregister lifecycle") ? 0 : 1;

    // 再注册一次验证 g_resourceData/g_resourceInitialized 已完整复位，而不是只修改查询标志。
    failed += Check(MeyerScanInitializeUiResources(), "initialize works after unregister") ? 0 : 1;
    failed += Check(QFile::exists(homeIconPath), "resources remain readable after reinitialize") ? 0 : 1;
    MeyerScanShutdownUiResources();

    // 返回 0/1 供 CTest、PowerShell 和 VS2015 构建后验证统一判断。
    std::printf("UIResourcesTest: %s\n", failed == 0 ? "passed" : "failed");
    return failed == 0 ? 0 : 1;
}
