#pragma once

#include "ConfigCenter.h"

#include <QJsonObject>
#include <QString>

// ConfigCenterImpl 负责加载 runtime_config.json 并提供按 key 读取的轻量接口。
// 当前只做运行配置读取，不做权限判断、数据库连接或业务决策。
class ConfigCenterImpl : public IConfigCenter {
public:
    // 返回进程内单例，避免多个模块重复加载和改写配置缓存。
    static ConfigCenterImpl& Instance();

    // 初始化配置文件路径、生成默认配置并加载 JSON。
    bool Init(const char* appDirUtf8) override;

    // 读取布尔值，不存在或类型不匹配时返回 defaultValue。
    bool GetBool(const char* key, bool defaultValue) const override;

    // 读取整数值，不存在或类型不匹配时返回 defaultValue。
    int GetInt(const char* key, int defaultValue) const override;

    // 读取字符串值并复制到调用方缓冲区。
    bool GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 清空内存中的 JSON 配置。
    void Shutdown() override;

private:
    ConfigCenterImpl() = default;
    ~ConfigCenterImpl() = default;
    ConfigCenterImpl(const ConfigCenterImpl&) = delete;
    ConfigCenterImpl& operator=(const ConfigCenterImpl&) = delete;

    // 将点号 key 拆成父对象和叶子字段名。
    // 例如 database.type 返回 database 对象，并把 leafName 设为 type。
    QJsonObject ResolveObject(const QString& key, QString* leafName) const;

    // 如果配置文件不存在，写入最小默认配置。
    // 固定启动流程（等待页、单实例）不写入配置。
    void EnsureDefaultConfig(const QString& configPath) const;

    // 清理旧版配置中的 startup.showWaitPage / startup.singleInstance。
    // 这两个开关已经改为 MainExe 固定流程，继续留在 JSON 中会误导维护人员。
    void MigrateDeprecatedStartupConfig();

private:
    // MeyerScan.exe 所在目录。
    QString m_appDir;

    // runtime_config.json 的绝对路径。
    QString m_configPath;

    // 已加载的 JSON 根对象。
    QJsonObject m_root;

    // 标记 Init 是否成功。
    bool m_initialized = false;
};
