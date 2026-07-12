#include "UIResources.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>

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

    // 同时抽查 QSS 和 PNG，防止清单只收录了某一种资源类型。
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
