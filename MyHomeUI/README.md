# MeyerScan_HomeUI

HomeUI 是 MeyerScan 的 Qt Widgets 首页入口模块。

- 显示创建、浏览、练习、设置四个首页入口。
- 通过 `SetEntryCallback()` 向 MainExe 上报入口 ID；HomeUI 不直接切换其他模块。
- 通过 `SetEntryVisible()` / `SetEntryEnabled()` 接收 MainExe 计算后的入口显隐和启用态；HomeUI 不直接读取权限文件。
- `visible=false` 时入口隐藏，`enabled=false` 时入口保留但不可点击；真正动作执行前仍由 MainExe / Workflow / Service 复核权限。
- 首页入口按钮优先使用 `MeyerScan_UIComponents.dll` 的 Entry 标准按钮样式；UIComponents 不可用时降级为本地按钮样式。
- 点击入口时写入结构化日志，MainExe 再记录跨模块导航和页面切换日志。
- 临时调用 `MeyerScan_Database.dll` 只用于框架 smoke 健康检查，不代表正式业务调用方式。
- 如果进程级 Database 已由 MainExe 初始化并连接，HomeUI 只借用现有连接，不重复 Init/Connect。
- 运行时用 `QLibrary` 加载 `MeyerScan_Logger.dll`；HomeUI 只借用进程级 Logger，`Shutdown()` 只 Flush，不关闭全局日志会话。
- 测试宿主在创建 `QApplication` 前启用 High DPI 属性，并按当前屏幕可用区域居中显示。
- 界面可见文字统一使用 `tr("English source text")` 包装；即使需求写中文按钮名，源码 source text 也写英文，中文显示由 `.qm` 翻译文件提供。
- 业务规则保留在 Permission 和 OrderWorkflowService；HomeUI 不执行 SQL、不判断加载订单流程。
- 测试宿主根据 exe 所在目录推导日志目录和数据库配置路径，不依赖固定开发机路径。
- 模块变更记录维护在 `CHANGELOG.md`。

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_HomeUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
