# MeyerScan Logger 变更记录

## 2026-07-13

- QString 便捷重载改为先保存命名 `QByteArray`，再调用 UTF-8 `const char*` 主接口；补充缓冲区生命周期实现注释，便于理解 Qt 调用方与无 Qt DLL ABI 的关系。
- Logger DLL 本体、日志格式、同步落盘规则和版本号均不变；根 CMake 已把 `LoggerTest` 注册进统一 CTest 清单。

## 2026-07-12

- 清理 `Logger.h` 宏说明中位于 `//` 注释行末尾的反斜杠，改用无续行符的等价执行流程示例，避免 C/C++ 预处理阶段把下一物理行拼入注释。
- 本次只修正文档性头文件注释，不改变日志 ABI、写入规则和模块版本。

## 2026-07-10

- `Version.rc` 补齐 `LegalCopyright` 文件详细信息字段；日志接口、逐条同步写入和非 Qt 边界保持不变。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 参与全模块偏移复查：确认 Logger 继续保持无 Qt 依赖基础设施边界，运行路径仍由调用方传入，模块本体不读取 `QDir::currentPath()`，日志写入规则和 `ILogger* m_logger` 生命周期缓存用法不变。
- 复测根输出目录 `LoggerTest.exe` 返回 0；本轮不改变日志格式、轮转规则或写文件策略。

## 2026-07-02

- 调整模块 `CMakeLists.txt`，纳入根目录公共 CMake 规则，继续保持 Logger 无 Qt 依赖和静态 CRT 策略。
- 按评审结论确认 Logger 是非界面基础设施模块模板：能不用 Qt 就不用 Qt，公共 ABI 继续保持 `const char*` / POD / 纯虚接口。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试项目和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `LoggerTest.cpp`：补充 `GetModuleFileNameA`、函数内 static 测试目录、`GetLocalTime` 日期生成、整文件读取、`std::thread::join`、`std::rename` 验证文件句柄关闭和日志格式反向断言说明。
- 前一轮已补强 Logger 实现中日志轮转、跨进程互斥、逐条同步写入、`CreateFileW` 和关闭句柄等说明。
- 重构规则复核时修正 `LoggerTest.exe` 的测试日志目录：由原先写死 `F:\MeyerScan\MyLogger\test\logs` 改为基于测试 exe 所在目录推导到同级 `logs`。
- 该调整只影响测试宿主，正式 Logger 仍由调用方传入 `MeyerScan.exe` 同级 `logs` 目录；测试项目也必须遵守“不依赖开发机绝对路径”的运行路径规则。

## 2026-06-26

- 复查全局文档和代码契约后，确认日志正文标签必须固定为 `[Content:]`，并修正 `LogFormat.cpp` 中残留的历史 `[Txt:]` 输出。
- 修正公开头文件示例：模块名示例改为当前 `MeyerScan_CaseOrderService` 命名风格；没有真实操作员上下文时继续传空字符串，不再在示例中传 `"System"`。

## 2026-06-25

- `LoggerTest` 启动前清理测试目录当天日志文件，避免旧版本历史日志中的 `[Txt:]`、`[INFO ]` 等格式影响本次紧凑格式断言。
- Logger 写文件路径改为 UTF-8 转 UTF-16 后调用 `CreateFileW` / `SHCreateDirectoryExW`，支持 MeyerScan 安装在中文路径或 OEM 自定义路径下仍能创建日志。
- 日志正文分类标记由 `[Txt:]` 调整为 `[Content:]`，与 module / operation 的分类标记规则保持直观一致，也便于后续工具按字段解析。
- README 日志格式示例更新为当前 `[Mod:] [Op:] [Content:]` 分类标记，并明确 `operator` 字段使用 `[Opr:]`，避免与操作字段 `[Op:]` 混淆。
- 明确 Qt 模块的 `QString` 重载是正式便捷接口，但跨 DLL ABI 仍为 UTF-8 `const char*`，不把 Qt 对象传入 Logger.dll。
- 根据 `glm52` 建议在 VS2015 工程和 CMake 工程中补充 `MEYER_MODULE_NAME="MeyerScan_Logger"`，保证 Logger 自身如使用日志宏时也有稳定模块名。
- Logger 导出宏仍保留 `MEYER_LOGGER_API` / `MEYER_LOGGER_EXPORTS`，后续如做主版本 ABI 收口，再统一评估是否迁移到 `MEYERSCAN_LOGGER_API`。

## 2026-06-24

- 根据日志实际测试反馈重构写入策略：每天默认生成 `MeyerScan_YYYYMMDD.log`，达到 10 MiB 后再生成 `MeyerScan_YYYYMMDD_001.log` 等尾部序号文件。
- 日志格式改为紧凑输出，空 `deviceId` / `caseId` / `operator` 字段不再输出 `[Dev:-]`、`[Case:-]`、`[Op:-]` 占位，也不再输出 `[INFO ]` 这种固定宽度空格。
- Logger 写入改为每条日志打开文件、追加一行、`FlushFileBuffers`、关闭句柄；后台可以移动、删除或打包日志文件。
- 保留 `ILogger::Write(...)` 原 ABI；明确推荐每个模块在初始化阶段缓存一份 `ILogger* m_logger`，后续在该变量生命周期内持续通过 `m_logger->Write(...)` 输出日志；Qt 模块可直接输出 `QString`，但 Logger.dll 本体仍不依赖 Qt。
- 新增 `README.md` 记录日志文件规则、写入策略、内容格式和推荐调用方式。
- 根据“初学者可读”要求补强函数体内部注释：日志日期生成、日志文件路径生成、LogWriter 析构和关闭流程均增加关键说明，继续保持 Logger 低依赖基础设施边界。
- 版本号更新为 `MeyerScan_Logger v1.1.0 (2026-06-24)`，删除旧 `LogBuffer.*`，并从 VS2015/CMake 工程中移除后台缓冲构建引用。
- 记录调用约束：模块如果没有真实操作员上下文，应传空字符串，不应为了占位传 `"System"`，避免日志中出现无意义 `[Op:System]`。

## 2026-06-22

- 新增模块级变更记录文件。
- 确认 Logger 边界不变：Logger 是低依赖基础设施 DLL，不依赖 Qt，也不依赖任何业务模块。
- 公共 ABI 继续基于 `const char*`；`std::string` 和 `QString` 便捷封装保留在调用方内联层。
- 修正 `Write()` 防护：Logger 未 `Init()` 或已经 `Shutdown()` 后，写日志请求会被安全丢弃，避免模块关闭阶段的迟到日志重新使用已关闭的日志会话。

## 2026-06-17

- 新增 `GetModuleVersion()` 和 `Version.rc`。
- 保持 `/MT` 静态 CRT，便于 Logger 在 Qt 和其他模块之前加载。
- 验证 Logger 可作为后续模块的日志模板。

## 2026-06-15

- 完成 VS2015 与 VSCode/MSBuild 构建配置。
- 验证 `LoggerTest.exe`，包括多线程压力写日志测试。
- 已按 `MyLogger/` 目录提交到 GitHub。
