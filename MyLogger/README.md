# MeyerScan Logger

`MyLogger` 是 MeyerScan 的统一日志基础设施模块，输出 `MeyerScan_Logger.dll`。

## 文件规则

- 所有模块写入同一个日志目录下的同一组日志文件。
- 日志目录由 MainExe 或调用方传入，当前约定为 `MeyerScan.exe` 同级 `logs` 目录。
- 日志目录禁止由 Logger 内部通过 `QDir::currentPath()` 或当前工作目录推导。第三方软件拉起 MeyerScan 时，当前工作目录可能不是安装目录。
- 默认每天只生成一个主文件：`MeyerScan_YYYYMMDD.log`。
- 当当天主文件达到大小上限后，再生成尾部序号文件：`MeyerScan_YYYYMMDD_001.log`、`MeyerScan_YYYYMMDD_002.log`。
- 默认单文件上限为 10 MiB。

## 写入策略

- 每条日志在跨进程互斥量保护下完成写入。
- 写入流程是：格式化 -> 选择日志文件 -> 打开文件 -> 追加一行 -> `FlushFileBuffers` -> 关闭句柄。
- Logger 不长期持有日志文件句柄，因此后台可以移动、删除或打包日志文件。
- 多模块、多进程写同一日志文件时，按获得互斥量的时间顺序逐行追加，避免半行交错。
- `Flush()` 仍保留给旧代码调用，但当前实现中每条日志已经同步落盘并关闭句柄，所以 `Flush()` 是兼容空操作。

## 内容格式

格式示例：

```text
[2026-06-24 08:42:14.893] [INFO] [Mod:MeyerScan_Database] [Op:SetDatabaseType] [Content:Database type switched]
```

- `deviceId`、`caseId`、`operator` 为空时不输出占位字段。
- `module`、`operation`、`content` 也带分类标记，分别输出 `[Mod:xxx]`、`[Op:xxx]`、`[Content:xxx]`。
- `deviceId`、`caseId`、`operator` 有值时才输出 `[Dev:xxx]`、`[Case:xxx]`、`[Opr:xxx]`。
- 日志字段 key 保持英文，便于搜索和工具解析。
- 模块如果没有明确操作员上下文，应传空字符串 `""`。不要为了占位传 `"System"`，否则日志会输出 `[Opr:System]`。

## 调用方式

兼容原有直接接口：

```cpp
GetLogger()->Write(LogLevel::Info,
                   "MainExe",
                   "Startup",
                   "",
                   "",
                   "",
                   "Logger initialized");
```

推荐每个模块在初始化阶段缓存一份 `ILogger*` 成员变量：

```cpp
class CaseUIImpl {
    ILogger* m_logger = nullptr;
};

bool CaseUIImpl::Init(const char* logDir) {
    m_logger = GetLogger();
    return m_logger && m_logger->Init(logDir, LogLevel::Info);
}

void CaseUIImpl::WriteLog(const QString& text) {
    if (!m_logger) {
        return;
    }
    const QByteArray bytes = text.toUtf8();
    m_logger->Write(LogLevel::Info, "CaseUI", "CreateWidget", "", "", "", bytes.constData());
}
```

Qt 模块也可以用 `QString` 重载直接写日志：

```cpp
m_logger->Write(LogLevel::Info,
                QString("HomeUI"),
                QString("EntryClicked"),
                QString(),
                QString(),
                QString(),
                QString("entryId=%1").arg(entryId));
```

说明：

- `QString` 支持只存在于头文件内联便捷层，`Logger.dll` 本体不依赖 Qt，跨 DLL ABI 仍然是 UTF-8 `const char*`。
- 不把 Qt 编译进 `Logger.dll` 是为了保持日志模块轻量、可最早加载、可被非 Qt 模块使用；Qt 模块仍然可以用 `QString` 便利接口。
- 推荐每个模块保存一份 `ILogger* m_logger`，不要在每次写日志时重复 `GetLogger()`。

## 维护注意

- 当前 `Logger.dll` 版本为 `MeyerScan_Logger v1.1.0 (2026-06-24)`。
- 旧版 `LogBuffer.*` 已删除；现在没有后台缓冲线程。
- 新增日志字段时优先通过 `content` 写稳定 key/value 文本，避免频繁改 DLL ABI。
