# MeyerScan HomeUI 变更记录

## 2026-06-22

- 新增模块级变更记录文件。
- 重新确认当前 Database 调用只用于框架 smoke 健康检查，即验证 `Init()` / `Connect()` 链路，不代表正式业务调用方式。
- HomeUI 正式业务行为应调用 Permission 和 OrderWorkflowService，不直接执行业务 SQL，也不直接承载病例/订单规则。
- 修正 Logger 生命周期边界：smoke 宿主传入日志目录时，由 HomeUI 初始化并关闭本地 Logger 会话；使用 `QLibrary::PreventUnloadHint` 避免退出阶段 DLL 卸载顺序问题。
- 模块关闭时先调用 Database `Disconnect()` / `Shutdown()`，再关闭 Logger 会话。
- 调整 `HomeUITest.exe` 生命周期：退出前先关闭并删除顶层 widget，再执行模块 `Shutdown()`。

## 2026-06-18

- 在测试宿主中于 `QApplication` 创建前启用 High DPI 相关属性。
- `HomeUITest.exe` 增加按当前屏幕可用区域居中和限制初始尺寸的逻辑。
- 界面可见文本使用 `QApplication::translate("HomeUI", "...")` 包装，为后续独立 `.qm` 翻译文件预留基础。
- README 补充 DPI 和多语言翻译说明。

## 2026-06-17

- 创建 Qt Widgets DLL 框架和 `HomeUITest.exe`。
- 新增首页四个入口框架：创建、浏览、练习、设置。
- 接入 Logger 和 Database，用于启动链路验证。
- 在 PostBuild 中加入 Qt 5.6.3 运行库复制规则。
