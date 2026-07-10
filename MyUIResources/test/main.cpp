#include "UIResources.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>

#include <cstdio>

// 输出单项检查结果并返回是否成功。
// 测试保持简单的控制台输出，便于 VS2015、CMake 和自动化脚本共同调用。
bool Check(bool condition, const char* name) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    return condition;
}

// 资源 DLL smoke 测试入口。
// 重点验证 DLL 注册、QSS 读取、PNG 定位、版本导出和注销生命周期。
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    int failed = 0;
    failed += Check(MeyerScanInitializeUiResources(), "initialize embedded resources") ? 0 : 1;
    failed += Check(MeyerScanUiResourcesInitialized(), "resource state is initialized") ? 0 : 1;

    const QString homeQssPath = ":/MeyerScan/Modules/MyHomeUI/qss/home.qss";
    const QString homeIconPath = ":/MeyerScan/Modules/MyHomeUI/icon/home/HomeCreate_b.png";
    failed += Check(QFile::exists(homeQssPath), "HomeUI qss exists in resource dll") ? 0 : 1;
    failed += Check(QFile::exists(homeIconPath), "HomeUI icon exists in resource dll") ? 0 : 1;

    QFile qssFile(homeQssPath);
    const bool qssOpened = qssFile.open(QIODevice::ReadOnly | QIODevice::Text);
    failed += Check(qssOpened, "embedded qss can be opened") ? 0 : 1;
    if (qssOpened) {
        // 读取后检查根控件选择器，避免资源存在但内容为空或清单指向错误文件。
        const QByteArray qss = qssFile.readAll();
        failed += Check(qss.contains("MeyerScanHomeUIRoot"), "embedded qss content is correct") ? 0 : 1;
    }

    const char* version = GetMeyerModuleVersion();
    failed += Check(version && version[0], "code version export is available") ? 0 : 1;

    MeyerScanShutdownUiResources();
    failed += Check(!MeyerScanUiResourcesInitialized(), "resource unregister lifecycle") ? 0 : 1;

    std::printf("UIResourcesTest: %s\n", failed == 0 ? "passed" : "failed");
    return failed == 0 ? 0 : 1;
}
