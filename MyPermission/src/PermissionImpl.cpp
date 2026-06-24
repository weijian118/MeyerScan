#include "PermissionImpl.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>

namespace {
namespace ModuleInfo {
// 模块名用于日志和工程识别，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_Permission";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_Permission v0.1.0 (2026-06-23)";
}
}

// 返回权限模块单例。
// 权限规则是进程级只读缓存，使用单例可以保证不同 UI 调用方看到同一份规则。
PermissionImpl& PermissionImpl::Instance() {
    static PermissionImpl instance;
    return instance;
}

// 初始化权限模块。
// permission_rules.json 放在应用目录 config 下，保证第三方拉起时路径仍稳定。
bool PermissionImpl::Init(const char* appDirUtf8) {
    // 权限规则必须跟随安装目录读取，不能依赖当前工作目录。
    // 第三方程序拉起 MeyerScan.exe 时，currentPath 常常是第三方程序目录。
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    if (m_appDir.isEmpty()) {
        return false;
    }

    // 权限配置统一放在 config 目录，便于和 runtime_config.json 一起打包、备份和排查。
    QDir dir(m_appDir);
    dir.mkpath("config");
    const QString configPath = dir.filePath("config/permission_rules.json");

    // 首次运行写入全开放默认规则，保证框架期 UI 能完整显示，后续再由授权结果覆盖。
    EnsureDefaultConfig(configPath);

    // 读取授权结果。读取失败直接返回 false，让 MainExe 使用保守默认值或记录错误。
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return false;
    }

    // 缓存权限 JSON。当前模块只读，不在运行时修改授权规则。
    m_root = document.object();
    m_initialized = true;
    return true;
}

// 读取 featureId 对应的 visible 字段。
// visible 控制“看不看得到入口”，不等于最终能不能执行动作。
bool PermissionImpl::IsFeatureVisible(const char* featureId, bool defaultValue) const {
    // featureId 由 MainExe/模块约定，统一映射到 features.<featureId>.visible。
    // 例如 home.settings -> features.home.settings.visible。
    return ReadBool(QString("features.%1.visible").arg(QString::fromUtf8(featureId ? featureId : "")), defaultValue);
}

// 读取 featureId 对应的 enabled 字段。
// enabled 控制“能不能点击/执行”，后续 Service/Workflow 仍要复核。
bool PermissionImpl::IsFeatureEnabled(const char* featureId, bool defaultValue) const {
    // enabled 与 visible 分开：入口可见但不可用时，后续可以显示禁用态和原因提示。
    return ReadBool(QString("features.%1.enabled").arg(QString::fromUtf8(featureId ? featureId : "")), defaultValue);
}

// 返回模块版本字符串。
const char* PermissionImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 清空内存中的权限规则。
void PermissionImpl::Shutdown() {
    // 只清理内存缓存，不删除 permission_rules.json。
    // 授权文件通常由安装、登录或授权流程维护，不由普通退出流程改写。
    m_root = QJsonObject();
    m_initialized = false;
}

// 按点号 key 读取布尔值。
// 例如 features.home.settings.visible 会进入 features -> home -> settings -> visible。
bool PermissionImpl::ReadBool(const QString& key, bool defaultValue) const {
    const QStringList parts = key.split('.', QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        return defaultValue;
    }

    QJsonObject object = m_root;
    // 逐级进入 JSON 对象。缺少中间对象时会得到空对象，最终自然返回默认值。
    for (int i = 0; i < parts.size() - 1; ++i) {
        object = object.value(parts[i]).toObject();
    }

    // 只有 bool 类型才被认为是有效权限值，类型错误时回退 defaultValue。
    const QJsonValue value = object.value(parts.last());
    return value.isBool() ? value.toBool() : defaultValue;
}

// 生成默认权限规则。
// 默认规则全部可见且可用，便于框架期先打通流程；正式规则后续由授权/客户配置覆盖。
void PermissionImpl::EnsureDefaultConfig(const QString& configPath) const {
    // 不覆盖已有授权规则。现场授权结果可能由云端、离线 license 或安装包写入。
    if (QFile::exists(configPath)) {
        return;
    }

    // 骨架期默认全部打开，方便测试完整流程。
    // 正式客户裁剪时，应由真实授权文件把 visible/enabled 改为 false。
    QJsonObject defaultRule;
    defaultRule.insert("visible", true);
    defaultRule.insert("enabled", true);

    // 首页相关入口。
    QJsonObject home;
    home.insert("settings", defaultRule);

    // 案例管理相关入口。
    QJsonObject caseUi;
    caseUi.insert("backHome", defaultRule);
    caseUi.insert("browse", defaultRule);

    // 建单和扫描入口先预留，便于后续模块接入时直接沿用 featureId。
    QJsonObject order;
    order.insert("create", defaultRule);

    QJsonObject scan;
    scan.insert("practice", defaultRule);

    QJsonObject features;
    features.insert("home", home);
    features.insert("case", caseUi);
    features.insert("order", order);
    features.insert("scan", scan);

    QJsonObject root;
    root.insert("features", features);

    // 写默认文件失败时不抛异常；Init 随后的 open/parse 会体现失败。
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

// 导出权限模块接口。
// MainExe 通过该入口获取权限结果，再把最终显隐/启用状态下发给 UI 模块。
extern "C" MEYERSCAN_PERMISSION_API IPermission* GetPermission() {
    return &PermissionImpl::Instance();
}
