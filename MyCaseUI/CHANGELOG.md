# MeyerScan CaseUI 变更记录

## 2026-06-22

- 2026-06-23 补充：新增 `SetActionVisible()`，用于接收 MainExe/Permission 下发的浏览模块操作显隐规则；当前可控制“返回首页”按钮显示状态。
- 新增 `SetActionCallback()` 客户操作事件接口，浏览模块只上报操作 ID，由 MainExe 负责跨页面切换。
- 在浏览模块顶部新增“返回首页”按钮，点击后上报 `CaseActionBackHome`。
- 患者页和订单页的工具按钮、搜索回车、页签切换均写入 `UserAction` 日志。
- MainExe 已接入返回首页操作，可从 CaseUI 丝滑切回 HomeUI。
- 2026-06-23 再次补充：测试宿主不再硬编码 `F:/MeyerScan/...`，改为根据 exe 所在目录推导模块日志目录和 MyDatabase 配置路径。
- 2026-06-23 再次补充：CaseUI 只借用进程级 Logger/Database 做框架期健康检查，`Shutdown()` 不再关闭 Logger/Database 单例，避免影响 MainExe 和其他模块。
- 2026-06-23 再次补充：按钮源文案改为稳定英文 `Back Home`，中文显示后续由模块 qm 提供，避免源码文案混用语言。
- 2026-06-23 复查优化：初始化数据库前优先检查进程级 Database 是否已连接；MainExe 已完成数据库健康检查时，CaseUI 只借用现有连接，不重复 Init/Connect。
- 2026-06-23 复查补充：明确 CaseUI 必须支持被 MainExe 释放并重建；后续进入扫描重建前由 MainExe 销毁 CaseUI widget，避免案例管理界面长期占用资源。
- 2026-06-23 复查补充：界面可见文字从 `QApplication::translate()` 统一改为 `tr("English source text")`；源码不写中文 UI 文案，中文显示后续由模块 `.qm` 提供。
- 2026-06-23 复查验证：`MeyerScan_CaseUI.sln` Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。
- 重新验证：VS2015 Release x64 构建通过，`CaseUITest.exe --smoke` 返回 0。
- 新增模块级变更记录文件。
- 重新确认当前 Database 调用只用于框架 smoke 健康检查，即验证 `Init()` / `Connect()` 链路，不代表正式业务调用方式。
- CaseUI 正式业务行为应调用 CaseService、OrderService、DataExport 和 OrderWorkflowService，不直接执行业务 SQL。
- 修正 Logger 生命周期边界：使用 `QLibrary::PreventUnloadHint` 避免退出阶段 DLL 卸载顺序问题。
- 当前框架期 Database 调用只用于健康检查，正式业务后续迁入 Service/Workflow。
- 调整 `CaseUITest.exe` 生命周期：退出前先关闭并删除顶层 widget，再执行模块 `Shutdown()`。

## 2026-06-18

- 在测试宿主中于 `QApplication` 创建前启用 High DPI 相关属性。
- `CaseUITest.exe` 增加按当前屏幕可用区域居中和限制初始尺寸的逻辑。
- 界面可见文本完成国际化预埋，为后续独立 `.qm` 翻译文件预留基础。
- README 补充 DPI 和多语言翻译说明。

## 2026-06-17

- 创建 Qt Widgets DLL 框架和 `CaseUITest.exe`。
- 新增患者和订单两个页签框架。
- 接入 Logger 和 Database，用于启动链路验证。
- 在 PostBuild 中加入 Qt 5.6.3 运行库复制规则。
