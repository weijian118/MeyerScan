#include "ConfigCenter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cstdio>
#include <cstring>

namespace {

// 通用检查函数。
// 测试宿主保持零框架依赖，便于 VS2015、VSCode 终端和批处理脚本直接运行。
bool Check(bool condition, const char* message) {
    // 检查通过时输出 PASS，形成清晰的测试步骤日志。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 检查失败时输出 FAIL，并让 main 返回对应错误码。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// ConfigCenter 测试入口：验证默认配置生成、字段读取和旧 startup 配置迁移流程。
int main(int argc, char* argv[]) {
    // ConfigCenter 只使用 QtCore 的路径和 JSON 能力，不需要 QApplication。
    QCoreApplication app(argc, argv);

    // 使用独立运行目录，避免测试生成的 runtime_config.json 覆盖正式发布配置。
    // applicationDirPath 固定指向 exe 所在目录，不受 currentPath 影响。
    const QString runtimeDir = QDir(QCoreApplication::applicationDirPath()).filePath("ConfigCenterTestRuntime");
    // mkpath 递归创建目录；目录已存在时不会删除旧文件。
    QDir().mkpath(runtimeDir);
    // 本测试要验证“首次生成默认配置”的代码路径，因此先删除上次测试留下的配置文件。
    // QFile::remove 对不存在的文件返回 false，但这里不影响后续 Init 自动创建新文件。
    QFile::remove(QDir(runtimeDir).filePath("config/runtime_config.json"));

    // 工厂函数是 DLL 的边界入口，先确认导出函数和链接关系正确。
    IConfigCenter* config = GetConfigCenter();
    if (!Check(config != nullptr, "ConfigCenter 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 会读取或创建 runtime_config.json，并把配置缓存到模块内部。
    // C ABI 只读取 UTF-8 指针；命名 QByteArray 让生命周期和调用范围清楚可见。
    const QByteArray runtimeDirUtf8 = runtimeDir.toUtf8();
    if (!Check(config->Init(runtimeDirUtf8.constData()), "ConfigCenter 初始化成功")) {
        return 2;
    }

    // 默认配置会开启首页设置入口，这是 MainExe 合并权限前的产品默认策略。
    // GetBool 的第二个参数是缺失字段默认值，用来区分“配置不存在”和“配置值为 false”。
    if (!Check(config->GetBool("feature.home.settingsVisible", false), "默认首页设置入口可见")) {
        return 3;
    }
    // 缺失整数应返回调用方默认值，避免 UI 模块因为配置漏项崩溃。
    if (!Check(config->GetInt("missing.int", 42) == 42, "缺失整数配置返回调用方默认值")) {
        return 4;
    }

    // DLL 接口使用调用方传入 buffer，避免 std::string 跨 DLL 边界造成运行库不一致问题。
    char buffer[64] = {0};
    if (!Check(config->GetString("database.type", "fallback", buffer, sizeof(buffer)),
               "database.type 默认字符串可读取")) {
        return 5;
    }
    // 当前重构主链路默认走 SQLite；这里固定断言，避免默认配置以后又漂回旧 MySQL 口径。
    if (!Check(std::strcmp(buffer, "sqlite") == 0, "database.type 默认值为 sqlite")) {
        return 6;
    }

    // Shutdown 清理内部缓存状态，为后续同进程扩展测试预留干净环境。
    config->Shutdown();
    std::printf("ConfigCenterTest passed.\n");
    return 0;
}
