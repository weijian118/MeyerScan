#include "ConfigCenterImpl.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>
#include <cstring>

namespace {
namespace ModuleInfo {
// 模块名用于日志和工程识别，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_ConfigCenter";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_ConfigCenter v0.1.0 (2026-06-23)";
}
}

// 返回配置中心单例。
// 配置中心在进程内只维护一份 runtime_config.json 缓存，避免多个模块重复读写文件。
ConfigCenterImpl& ConfigCenterImpl::Instance() {
    // 函数内 static 是最轻量的进程内单例写法。
    // C++11 起编译器会保证多线程首次进入时只构造一次，不需要我们手写锁。
    static ConfigCenterImpl instance;
    // 返回引用而不是指针，说明该对象一定存在，调用方也不需要负责释放。
    return instance;
}

// 初始化配置中心。
// 设计点：配置路径始终基于应用目录，避免第三方拉起软件时 current directory 错误。
bool ConfigCenterImpl::Init(const char* appDirUtf8) {
    // 调用方必须传 MeyerScan.exe 所在目录；这里不使用 QDir::currentPath()，
    // 因为软件可能由第三方 HIS/美亚美牙等程序拉起，工作目录并不可靠。
    // QString::fromUtf8 负责把跨 DLL 传入的 UTF-8 字节转换成 Qt 内部 Unicode 字符串。
    m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
    if (m_appDir.isEmpty()) {
        // 应用目录为空时继续往下会把配置写到不可预测位置，所以直接失败。
        return false;
    }

    // 配置目录统一放在应用目录 config 下，便于安装包、自动更新和现场排查。
    QDir dir(m_appDir);
    // mkpath 可递归创建目录；目录已存在时也会返回成功，适合初始化阶段幂等调用。
    dir.mkpath("config");
    // filePath 会使用当前 QDir 的基准路径拼出完整路径，避免手工拼接分隔符出错。
    m_configPath = dir.filePath("config/runtime_config.json");

    // 首次运行时生成最小默认配置，保证后续读取逻辑不用处理“文件不存在”分支。
    EnsureDefaultConfig(m_configPath);

    // 只读打开配置文件；读完后立即关闭，避免后续迁移写回时文件仍被占用。
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // QFile 打开失败一般是路径错误、权限问题或文件被异常占用。
        return false;
    }

    // 先把文件内容读入内存并关闭文件，再执行迁移写回。
    // Windows 下同一个 QFile 仍处于 ReadOnly 状态时，后续重新打开写入可能失败。
    const QByteArray configBytes = file.readAll();
    file.close();

    // QJsonDocument::fromJson 会解析 UTF-8 JSON；解析失败时 document 不是 object。
    const QJsonDocument document = QJsonDocument::fromJson(configBytes);
    if (!document.isObject()) {
        // runtime_config.json 顶层必须是对象，因为后续按点号路径逐级读取字段。
        return false;
    }

    // 缓存 JSON 根对象。ConfigCenter 当前是只读配置中心，运行期读取都走该缓存。
    m_root = document.object();

    // 清理历史 startup 配置，防止旧文件继续控制固定启动流程。
    MigrateDeprecatedStartupConfig();
    // 初始化标志只在配置已经读取、迁移完成后置 true，避免半初始化状态被其它模块使用。
    m_initialized = true;
    return true;
}

// 读取布尔配置。
// ConfigCenter 只给出“默认策略”，最终是否显示/启用还要交给 Permission 复核。
bool ConfigCenterImpl::GetBool(const char* key, bool defaultValue) const {
    // ResolveObject 返回父对象，leafName 返回最后一级字段名。
    // 例如 feature.home.settingsVisible -> 父对象 feature.home，叶子 settingsVisible。
    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    // QJsonObject::value 找不到字段时返回 undefined QJsonValue，不会抛异常。
    const QJsonValue value = object.value(leafName);

    // 只有 JSON 字段确实是 bool 时才采用配置值；类型不匹配时回退默认值，避免坏配置影响启动。
    return value.isBool() ? value.toBool() : defaultValue;
}

// 读取整数配置。
// 当前骨架还没有默认整数项，但接口先保留，避免后续为了一个 int 修改 ABI。
int ConfigCenterImpl::GetInt(const char* key, int defaultValue) const {
    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    const QJsonValue value = object.value(leafName);

    // Qt JSON 中整数也以 double 存储，toInt() 会做安全转换。
    // isDouble() 是 Qt JSON 对数字类型的统一判断，不代表配置必须写小数。
    return value.isDouble() ? value.toInt() : defaultValue;
}

// 读取字符串配置。
// 返回值表示 JSON 中是否存在字符串类型；即使返回 false，也会把 defaultValue 写入 buffer。
bool ConfigCenterImpl::GetString(const char* key, const char* defaultValue, char* buffer, int bufferSize) const {
    // 调用方提供缓冲区，避免跨 DLL 由一边分配内存、另一边释放内存。
    if (!buffer || bufferSize <= 0) {
        // buffer 无效时不能写默认值，只能通过 false 告知调用方读取失败。
        return false;
    }

    QString leafName;
    const QJsonObject object = ResolveObject(QString::fromUtf8(key ? key : ""), &leafName);
    const QJsonValue value = object.value(leafName);

    // 字段不存在或类型不是字符串时，仍把 defaultValue 写入 buffer，让调用方可以继续使用默认策略。
    const QString text = value.isString() ? value.toString() : QString::fromUtf8(defaultValue ? defaultValue : "");
    // 统一转 UTF-8，保证 C ABI 调用方拿到的是跨语言/跨模块都稳定的字节格式。
    const QByteArray bytes = text.toUtf8();

    // 预留 1 个字节写 '\0'，保证调用方收到的是 C 字符串。
    const int copySize = qMin(bufferSize - 1, bytes.size());
    if (copySize > 0) {
        // memcpy 只复制原始字节，不尝试解释字符；如果缓冲区太小，后面仍会补 '\0'。
        memcpy(buffer, bytes.constData(), static_cast<size_t>(copySize));
    }
    buffer[copySize] = '\0';

    // 返回值只表达“配置中是否存在字符串字段”，不表达 buffer 是否写入成功。
    return value.isString();
}

// 返回模块版本字符串。
const char* ConfigCenterImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 清理内存状态。配置文件不在 Shutdown 中写回，避免退出阶段误覆盖人工修改。
void ConfigCenterImpl::Shutdown() {
    // 仅清理内存缓存，不删除或重写配置文件。
    // 退出阶段尽量少做磁盘写入，避免异常退出时破坏配置。
    m_root = QJsonObject();
    // 标志复位后，后续调用方可以重新 Init 重新加载配置文件。
    m_initialized = false;
}

// 将点号分隔的 key 解析为父对象和叶子字段。
// 这个函数只做简单读取，不创建缺失节点，也不修改 JSON。
QJsonObject ConfigCenterImpl::ResolveObject(const QString& key, QString* leafName) const {
    // SkipEmptyParts 让 "feature..home" 这类异常 key 不产生空段，降低坏输入影响。
    const QStringList parts = key.split('.', QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (leafName) {
            // 调用方传了 leafName 指针时必须写入确定值，避免它继续使用旧内容。
            *leafName = QString();
        }
        return QJsonObject();
    }

    QJsonObject object = m_root;
    // 逐级向下找父对象。中间节点不存在时 toObject() 返回空对象，
    // 后续读取自然会回退默认值，不在这里抛错。
    for (int i = 0; i < parts.size() - 1; ++i) {
        // value(parts[i]) 可能不是对象；toObject() 会返回空对象，读取逻辑保持容错。
        object = object.value(parts[i]).toObject();
    }

    // 最后一段字段名交给调用方读取具体类型。
    if (leafName) {
        *leafName = parts.last();
    }
    return object;
}

// 生成最小默认配置。
// runtime_config.json 只保存产品/客户默认配置；授权相关开关在 permission_rules.json。
void ConfigCenterImpl::EnsureDefaultConfig(const QString& configPath) const {
    // 已有配置文件时绝不覆盖，避免升级后把现场客户配置重置掉。
    if (QFile::exists(configPath)) {
        return;
    }

    // 新架构当前默认走 SQLite，避免新电脑首次运行时误回到旧 MySQL 部署口径。
    QJsonObject root;
    QJsonObject database;
    // insert 会覆盖同名字段；这里是新对象，所以效果等同于写入一项。
    database.insert("type", "sqlite");

    // feature 只表达产品/客户默认显示策略，不表达授权。
    // 授权结果由 Permission 的 permission_rules.json 再过滤。
    QJsonObject feature;
    QJsonObject home;
    home.insert("settingsVisible", true);
    QJsonObject caseUi;
    caseUi.insert("backHomeVisible", true);
    feature.insert("home", home);
    feature.insert("case", caseUi);

    root.insert("database", database);
    root.insert("feature", feature);

    // 写入失败不抛异常；Init 随后打开文件失败会返回 false，让 MainExe 记录启动问题。
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Indented 格式方便人工打开检查；配置文件体积很小，不需要 Compact。
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

// 清理旧版 startup 配置段。
// 等待页和单实例属于 MainExe 的固定启动流程，不允许被配置文件关闭或改写。
void ConfigCenterImpl::MigrateDeprecatedStartupConfig() {
    // 旧版本曾把等待页/单实例写进 startup 配置。
    // 现在这两个属于 MainExe 固定流程，所以检测到就清掉，避免现场配置残留产生误解。
    if (!m_root.contains("startup")) {
        // 没有旧字段时直接返回，避免无意义地重写配置文件和改变文件修改时间。
        return;
    }

    // QJsonObject::remove 只修改内存中的对象；下面还需要写回磁盘。
    m_root.remove("startup");

    // 迁移后立即写回，后续人工查看 runtime_config.json 时不会再看到已废弃字段。
    QFile file(m_configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Truncate 先清空旧文件内容，再写入迁移后的完整 JSON，避免旧尾部残留。
        file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    }
}

// 导出配置中心接口。
// MainExe 通过该入口读取产品/客户默认策略，再与 Permission 授权结果合并。
extern "C" MEYERSCAN_CONFIGCENTER_API IConfigCenter* GetConfigCenter() {
    // 返回接口基类指针，隐藏 ConfigCenterImpl 具体类，降低调用方编译依赖。
    return &ConfigCenterImpl::Instance();
}

// 统一版本导出函数。
// 版本清单模块只解析该 C ABI 函数即可获得代码版本，
// 不需要创建配置中心业务接口或读取配置文件。
extern "C" MEYERSCAN_CONFIGCENTER_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
