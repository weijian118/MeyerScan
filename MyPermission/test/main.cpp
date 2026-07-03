#include "Permission.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>

namespace {

QString WritePermissionRules(const QString& runtimeDir) {
    QDir dir(runtimeDir);
    dir.mkpath("config");
    const QString configPath = dir.filePath("config/permission_rules.json");
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
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

    const QString runtimeDir = QDir(QCoreApplication::applicationDirPath()).filePath("PermissionTestRuntime");
    QDir().mkpath(runtimeDir);
    WritePermissionRules(runtimeDir);

    IPermission* permission = GetPermission();
    if (!Check(permission != nullptr, "Permission 工厂函数返回有效实例")) {
        return 1;
    }
    if (!Check(permission->Init(runtimeDir.toUtf8().constData()), "Permission 初始化成功")) {
        return 2;
    }

    if (!Check(permission->IsFeatureVisible("home.settings", false), "visible=true 时入口可见")) {
        return 3;
    }
    if (!Check(!permission->IsFeatureEnabled("home.settings", true), "enabled=false 时入口不可执行")) {
        return 4;
    }
    if (!Check(!permission->IsFeatureVisible("case.backHome", true), "visible=false 时入口隐藏")) {
        return 5;
    }
    if (!Check(permission->IsFeatureEnabled("missing.feature", true), "缺失功能按调用方默认值返回")) {
        return 6;
    }

    permission->Shutdown();
    std::printf("PermissionTest passed.\n");
    return 0;
}
