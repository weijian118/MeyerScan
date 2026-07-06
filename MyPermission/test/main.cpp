#include "Permission.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>

namespace {

// 写入权限模块测试配置。
// Permission 只读取 JSON，不应该在 JSON 中写注释；字段解释放同级 md 文档。
QString WritePermissionRules(const QString& runtimeDir) {
    // runtimeDir 是测试专用目录，避免覆盖正式 config/permission_rules.json。
    QDir dir(runtimeDir);
    // 权限配置固定放在 config 目录下，和正式程序结构一致。
    dir.mkpath("config");
    // filePath 统一拼接路径，避免手写分隔符造成跨环境问题。
    const QString configPath = dir.filePath("config/permission_rules.json");
    QFile file(configPath);
    // 每次测试重写配置，确保 enabled/visible 的期望值不会被上一次运行污染。
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        // QTextStream 设置 UTF-8，保证后续如果加入中文 feature 说明也能稳定写入。
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        // JSON 内不写注释，字段说明放同目录 md；这里特意让 enabled=false，验证该字段已真正生效。
        stream << "{\n"
               << "  \"features\": {\n"
               << "    \"home\": {\n"
               << "      \"settings\": { \"visible\": true, \"enabled\": false }\n"
               << "    },\n"
               << "    \"case\": {\n"
               << "      \"backHome\": { \"visible\": false, \"enabled\": true }\n"
               << "    }\n"
               << "  }\n"
               << "}\n";
    }
    return configPath;
}

// 简单断言函数。
// 返回 bool 让 main 能就近返回不同错误码，便于定位失败步骤。
bool Check(bool condition, const char* message) {
    // 成功时写标准输出。
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }
    // 失败时写标准错误。
    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

}

// Permission 测试入口：验证 permission_rules.json 中 visible/enabled 的读取和默认值行为。
int main(int argc, char* argv[]) {
    // 权限模块只需要 QtCore 的路径和 JSON 能力，不创建 QWidget。
    QCoreApplication app(argc, argv);

    // 测试运行目录独立，避免污染正式发布配置。
    const QString runtimeDir = QDir(QCoreApplication::applicationDirPath()).filePath("PermissionTestRuntime");
    // 创建运行目录后再写权限 JSON。
    QDir().mkpath(runtimeDir);
    // 写入一份覆盖 visible/enabled 两种状态的规则文件。
    WritePermissionRules(runtimeDir);

    // 通过 DLL 工厂函数获取权限接口。
    IPermission* permission = GetPermission();
    if (!Check(permission != nullptr, "Permission 工厂函数返回有效实例")) {
        return 1;
    }
    // Init 会读取 runtimeDir/config/permission_rules.json 并缓存规则。
    if (!Check(permission->Init(runtimeDir.toUtf8().constData()), "Permission 初始化成功")) {
        return 2;
    }

    // visible 控制“看不看得见”，用于按钮/入口是否显示。
    if (!Check(permission->IsFeatureVisible("home.settings", false), "visible=true 时入口可见")) {
        return 3;
    }
    // enabled 控制“能不能执行”，用于按钮禁用或功能调用前拦截。
    if (!Check(!permission->IsFeatureEnabled("home.settings", true), "enabled=false 时入口不可执行")) {
        return 4;
    }
    // visible=false 时即使 enabled=true，也应该隐藏入口。
    if (!Check(!permission->IsFeatureVisible("case.backHome", true), "visible=false 时入口隐藏")) {
        return 5;
    }
    // 缺失功能返回调用方默认值，让不同模块可以按自己的安全策略兜底。
    if (!Check(permission->IsFeatureEnabled("missing.feature", true), "缺失功能按调用方默认值返回")) {
        return 6;
    }

    // 释放权限缓存，便于后续同进程扩展测试。
    permission->Shutdown();
    std::printf("PermissionTest passed.\n");
    return 0;
}
