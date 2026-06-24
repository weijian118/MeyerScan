#pragma once

#include "Permission.h"

#include <QJsonObject>
#include <QString>

// PermissionImpl 读取 permission_rules.json 并提供 visible/enabled 判断。
// 当前是轻量骨架，后续六维权限会在这里扩展，不散落到 UI 模块。
class PermissionImpl : public IPermission {
public:
    // 返回进程内单例。
    static PermissionImpl& Instance();

    // 初始化权限规则文件。
    bool Init(const char* appDirUtf8) override;

    // 判断功能入口是否显示。
    bool IsFeatureVisible(const char* featureId, bool defaultValue) const override;

    // 判断功能入口或动作是否可执行。
    bool IsFeatureEnabled(const char* featureId, bool defaultValue) const override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 清空已加载规则。
    void Shutdown() override;

private:
    PermissionImpl() = default;
    ~PermissionImpl() = default;
    PermissionImpl(const PermissionImpl&) = delete;
    PermissionImpl& operator=(const PermissionImpl&) = delete;

    // 按点号 key 读取布尔值，不存在或类型不匹配时返回默认值。
    bool ReadBool(const QString& key, bool defaultValue) const;

    // 如果权限文件不存在，写入最小默认规则。
    void EnsureDefaultConfig(const QString& configPath) const;

private:
    // MeyerScan.exe 所在目录。
    QString m_appDir;

    // 已加载的权限 JSON 根对象。
    QJsonObject m_root;

    // 标记 Init 是否成功。
    bool m_initialized = false;
};
