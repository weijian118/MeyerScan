#include "ConfigCenter.h"

#include <QCoreApplication>
#include <QDir>
#include <cstdio>
#include <cstring>

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
    QCoreApplication app(argc, argv);

    // 使用独立运行目录，避免测试生成的 runtime_config.json 覆盖正式发布配置。
    const QString runtimeDir = QDir(QCoreApplication::applicationDirPath()).filePath("ConfigCenterTestRuntime");
    QDir().mkpath(runtimeDir);

    IConfigCenter* config = GetConfigCenter();
    if (!Check(config != nullptr, "ConfigCenter 工厂函数返回有效实例")) {
        return 1;
    }
    if (!Check(config->Init(runtimeDir.toUtf8().constData()), "ConfigCenter 初始化成功")) {
        return 2;
    }

    // 默认配置会开启首页设置入口，这是 MainExe 合并权限前的产品默认策略。
    if (!Check(config->GetBool("feature.home.settingsVisible", false), "默认首页设置入口可见")) {
        return 3;
    }
    if (!Check(config->GetInt("missing.int", 42) == 42, "缺失整数配置返回调用方默认值")) {
        return 4;
    }

    char buffer[64] = {0};
    if (!Check(config->GetString("database.type", "fallback", buffer, sizeof(buffer)),
               "database.type 默认字符串可读取")) {
        return 5;
    }
    if (!Check(std::strlen(buffer) > 0, "database.type 读取结果非空")) {
        return 6;
    }

    config->Shutdown();
    std::printf("ConfigCenterTest passed.\n");
    return 0;
}
