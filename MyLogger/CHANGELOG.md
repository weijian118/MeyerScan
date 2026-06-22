# MeyerScan Logger 变更记录

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
