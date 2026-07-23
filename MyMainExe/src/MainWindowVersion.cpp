
#include "MainWindow.h"
#include "MainWindowInternal.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegExp>
#include <windows.h>
#include <vector>

#include "Logger.h"

// 生成版本清单 JSON。
// 清单写入 logs/versionList，内容只包含 manifest 中列出的拆分模块 EXE/DLL。
void MainWindow::WriteVersionList() {
    // appDirPath 是发布目录。版本清单不再无差别扫描同级 EXE/DLL，
    // 因为发布目录中会有大量 Qt、VTK、OpenCV、OpenSSL、AWS、VC 运行库等第三方依赖。
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QString logDirPath = ResolveLogDir();
    const QString manifestPath = QDir(appDirPath).filePath("config/version_modules.json");
    // manifest 不存在时写默认清单；存在时尊重文件内容，便于后续新增模块无需改 MainExe 代码。
    EnsureDefaultVersionManifest(manifestPath);

    QDir versionDir(logDirPath + "/versionList");
    if (!versionDir.exists()) {
        // versionList 目录不存在时自动创建，失败会在后面写文件阶段体现。
        QDir().mkpath(versionDir.absolutePath());
    }

    QJsonArray modules;
    const QList<QPair<QString, QString>> moduleEntries = LoadVersionManifest(manifestPath);
    for (const QPair<QString, QString>& moduleEntry : moduleEntries) {
        const QString moduleFile = moduleEntry.first;
        const QString versionFunctionName = moduleEntry.second;
        // manifest 中只写文件名或相对路径，最终都以 appDir 为根解析。
        QFileInfo fileInfo(QDir(appDirPath).filePath(moduleFile));
        QJsonObject module;
        // name 便于人工快速查看，path 便于工具定位实际文件。
        module.insert("name", moduleFile);
        module.insert("versionFunction", versionFunctionName);
        module.insert("path", QDir::fromNativeSeparators(fileInfo.absoluteFilePath()));
        module.insert("exists", fileInfo.exists());
        if (fileInfo.exists()) {
            // 某些 DLL 没有 Windows 版本资源，ReadFileVersion 会返回空字符串。
            const QString fileVersion = ReadFileVersion(fileInfo.absoluteFilePath());
            QString codeVersionError;
            const QString codeVersion = moduleFile.compare("MeyerScan.exe", Qt::CaseInsensitive) == 0
                ? QString::fromLatin1(ModuleInfo::Version)
                : ReadCodeVersion(fileInfo.absoluteFilePath(), versionFunctionName, &codeVersionError);
            module.insert("fileVersion", fileVersion);
            module.insert("codeVersion", codeVersion);
            if (!versionFunctionName.isEmpty() || moduleFile.compare("MeyerScan.exe", Qt::CaseInsensitive) == 0) {
                // versionMatch 只对有代码版本来源的模块有意义。
                // 对没有统一版本函数的外部模块不写 true/false，避免误导维护者。
                module.insert("versionMatch", AreVersionsConsistent(fileVersion, codeVersion));
            }
            if (!codeVersionError.isEmpty()) {
                module.insert("codeVersionError", codeVersionError);
            }
            // JSON 没有 64 位整数的稳定跨解析器表示，这里转 double 足够记录文件大小。
            module.insert("size", static_cast<double>(fileInfo.size()));
            module.insert("lastModified", fileInfo.lastModified().toString(Qt::ISODate));
        } else {
            // 缺失项仍写入清单，方便安装包阶段发现模块漏复制。
            module.insert("fileVersion", QString());
            module.insert("codeVersion", QString());
            module.insert("versionMatch", false);
            module.insert("size", 0);
            module.insert("lastModified", QString());
        }
        modules.append(module);
    }

    QJsonObject root;
    root.insert("generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("appDir", QDir::fromNativeSeparators(appDirPath));
    root.insert("generator", QString::fromLatin1(ModuleInfo::Version));
    root.insert("schemaVersion", 2);
    root.insert("manifest", QDir::fromNativeSeparators(manifestPath));
    root.insert("modules", modules);

    // 文件名带毫秒时间戳，每次启动保留一份现场版本快照。
    // smoke 或第三方拉起测试可能在同一秒内连续启动两个 MeyerScan.exe；
    // 如果只精确到秒，后一次启动会覆盖前一次版本清单，现场追溯时会少一份启动快照。
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString versionListPath = versionDir.filePath(QString("versionList_%1.json").arg(stamp));
    QFile file(versionListPath);
    const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (opened) {
        // 使用 Indented 方便人工直接打开阅读。
        // versionList 是排查文件，不走 Compact；可读性优先于体积。
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    if (m_logger) {
        // 版本清单写入成功时记录路径，失败时记录 Warning，方便定位权限问题。
        const QByteArray pathBytes = QDir::fromNativeSeparators(versionListPath).toUtf8();
        m_logger->Write(opened ? LogLevel::Info : LogLevel::Warning,
                        ModuleInfo::Name,
                        "VersionList",
                        "",
                        "",
                        "",
                        opened ? pathBytes.constData() : "Failed to write version list");
    }
}

// 读取版本清单 manifest。
// JSON 不允许写注释，字段解释写在 config/version_modules.md 中；这里仅解析机器可读字段。
QList<QPair<QString, QString>> MainWindow::LoadVersionManifest(const QString& manifestPath) const {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // 读不到 manifest 时返回空列表；启动不中断，后续日志/版本清单会暴露缺失。
        return QList<QPair<QString, QString>>();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        // JSON 格式错误时不猜测内容，避免把第三方 DLL 全扫进去。
        return QList<QPair<QString, QString>>();
    }

    QList<QPair<QString, QString>> modules;
    const QJsonArray items = document.object().value("modules").toArray();
    for (const QJsonValue& item : items) {
        if (item.isString()) {
            // 兼容旧格式：modules 数组中直接写文件名字符串。
            // 旧格式没有 versionFunction，因此只能记录文件版本，不能读取代码版本。
            const QString name = item.toString().trimmed();
            if (!name.isEmpty()) {
                // manifest 中按顺序追加，版本清单输出顺序也保持一致，方便人工比对。
                modules.append(qMakePair(name, QString()));
            }
        } else if (item.isObject()) {
            // 当前主格式：{ "file": "...", "versionFunction": "GetMeyerModuleVersion" }。
            // 旧字段 factory 记录的是业务工厂函数，例如 GetHomeUI / GetLogger。
            // 业务工厂返回的是接口对象指针，不能按 const char* 版本函数调用。
            // 因此旧配置只作为“该自研模块有代码版本来源”的提示，实际仍尝试读取统一版本函数。
            const QJsonObject object = item.toObject();
            const QString name = object.value("file").toString().trimmed();
            QString versionFunction = object.value("versionFunction").toString().trimmed();
            if (versionFunction.isEmpty() && !object.value("factory").toString().trimmed().isEmpty()) {
                versionFunction = "GetMeyerModuleVersion";
            }
            if (!name.isEmpty()) {
                // note 等说明字段只给人看，不进入版本扫描路径。
                modules.append(qMakePair(name, versionFunction));
            }
        }
    }
    return modules;
}

// 首次运行没有 manifest 时写入默认拆分模块清单。
// 后续新增模块时维护 config/version_modules.json，而不是修改扫描代码。
void MainWindow::EnsureDefaultVersionManifest(const QString& manifestPath) const {
    if (QFileInfo::exists(manifestPath)) {
        // 文件已存在时不覆盖，避免用户或打包脚本维护的清单被默认值重写。
        return;
    }

    QDir dir(QFileInfo(manifestPath).absolutePath());
    if (!dir.exists()) {
        // mkpath 可递归创建 config 目录；已存在时也安全。
        QDir().mkpath(dir.absolutePath());
    }

    QJsonArray modules;
    for (const VersionModuleEntry& moduleEntry : kDefaultVersionModules) {
        // 默认清单只包含拆分出来的自研模块，不包含 Qt、VC runtime、OpenSSL 等第三方库。
        QJsonObject module;
        module.insert("file", QString::fromLatin1(moduleEntry.file));
        module.insert("versionFunction", QString::fromLatin1(moduleEntry.versionFunction));
        modules.append(module);
    }

    QJsonObject root;
    root.insert("description", "MeyerScan split modules recorded in startup versionList");
    root.insert("schemaVersion", 2);
    root.insert("modules", modules);

    QFile file(manifestPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

// 从 Windows 文件版本资源读取 FileVersion。
// 有些第三方 DLL 没有版本资源，这种情况返回空字符串，不中断启动。
QString MainWindow::ReadFileVersion(const QString& filePath) const {
    DWORD handle = 0;
    // Windows 版本资源 API 使用宽字符路径，先转成本机分隔符和 std::wstring。
    const std::wstring nativePath = QDir::toNativeSeparators(filePath).toStdWString();
    // 第一次调用只问资源块大小；handle 在现代 Windows 中通常未使用，但 API 需要传地址。
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &handle);
    if (size == 0) {
        // 没有版本资源或读取失败都返回空，不能影响主程序启动。
        return QString();
    }

    // 版本资源大小由 Windows API 返回，按该大小申请缓冲区。
    QByteArray data(static_cast<int>(size), 0);
    // GetFileVersionInfoW 把整个版本资源块写入 data，后续 VerQueryValueW 在这块内存里取结构。
    if (!GetFileVersionInfoW(nativePath.c_str(), handle, size, data.data())) {
        return QString();
    }

    // "\\" 表示读取 VS_FIXEDFILEINFO 根结构。
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    // VerQueryValueW 返回的 info 指针指向 data 内部，不能在 data 析构后继续保存。
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || !info) {
        return QString();
    }

    // Windows 把版本号拆在两个 DWORD 中，高低字分别组成 a.b.c.d。
    // HIWORD/LOWORD 是 Windows 提供的宏，用来从 32 位整数中拆出高 16 位和低 16 位。
    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
}

// 从模块 DLL 读取代码版本。
// versionFunctionName 为空时表示该文件没有统一版本函数，只记录文件版本即可。
QString MainWindow::ReadCodeVersion(const QString& filePath,
                                    const QString& versionFunctionName,
                                    QString* errorMessage) const {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (versionFunctionName.trimmed().isEmpty()) {
        return QString();
    }

    QLibrary library(filePath);
    // PreventUnloadHint 让版本读取阶段加载的 DLL 保持驻留。
    // 后续如果 MainExe 再通过成员 QLibrary 加载同一 DLL，可复用系统已加载模块。
    library.setLoadHints(QLibrary::PreventUnloadHint);
    if (!library.load()) {
        if (errorMessage) {
            *errorMessage = library.errorString();
        }
        return QString();
    }

    const QByteArray functionBytes = versionFunctionName.toLatin1();
    QFunctionPointer pointer = library.resolve(functionBytes.constData());
    if (!pointer) {
        if (errorMessage) {
            *errorMessage = QString("Version function not found: %1").arg(versionFunctionName);
        }
        return QString();
    }

    // 统一版本函数签名固定为 const char* (*)()。
    // 它只返回模块静态版本字符串，不创建业务接口对象，避免版本扫描和业务初始化耦合。
    typedef const char* (*VersionFunction)();
    VersionFunction readVersion = reinterpret_cast<VersionFunction>(pointer);
    const char* version = readVersion ? readVersion() : nullptr;
    return version ? QString::fromUtf8(version) : QString();
}

// 把不同来源的版本字符串归一化为可比较的数字版本。
QString MainWindow::NormalizeVersionText(const QString& versionText) const {
    // 先把 rc 文件里偶尔出现的逗号版本 "1, 3, 0, 0" 归一成点号。
    QString normalized = versionText;
    normalized.replace(",", ".");

    // 从代码版本字符串中抓取第一个数字版本，例如:
    // "MeyerScan_UIComponents v0.4.0 (2026-07-05)" -> "0.4.0"。
    QRegExp expression("(\\d+(\\.\\d+)+)");
    if (expression.indexIn(normalized) >= 0) {
        return expression.cap(1);
    }
    return normalized.trimmed();
}

// 比较 Windows 文件版本和模块代码版本是否一致。
bool MainWindow::AreVersionsConsistent(const QString& fileVersion, const QString& codeVersion) const {
    const QString file = NormalizeVersionText(fileVersion);
    const QString code = NormalizeVersionText(codeVersion);
    if (file.isEmpty() || code.isEmpty()) {
        return false;
    }
    if (file == code) {
        return true;
    }

    // rc 文件常用四段版本 0.4.0.0，代码版本常用三段语义版本 0.4.0。
    // 只允许文件版本比代码版本多一个末尾 .0，避免 0.4.1 和 0.4.0 被误判一致。
    return file == code + ".0";
}
