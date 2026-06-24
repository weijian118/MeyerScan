# MeyerScan_CaseUI

CaseUI 是 MeyerScan 的 Qt Widgets 案例管理界面模块。

- 提供患者管理和订单管理页签框架，对应帮助文档中的浏览模块。
- 提供“返回首页”按钮，通过 `SetActionCallback()` 向 MainExe 上报 `CaseActionBackHome`；CaseUI 不直接切换首页。
- 通过 `SetActionVisible()` / `SetActionEnabled()` 接收 MainExe 计算后的操作显隐和启用态；CaseUI 不直接读取权限文件。
- `visible=false` 时入口隐藏，`enabled=false` 时入口保留但不可点击；真正动作执行前仍由 MainExe / Workflow / Service 复核权限。
- 点击返回、导入、导出、删除、新建、打开、搜索、页签切换等客户操作时写入结构化日志。
- 临时调用 `MeyerScan_Database.dll` 只用于框架 smoke 健康检查，不代表正式业务调用方式。
- 如果进程级 Database 已由 MainExe 初始化并连接，CaseUI 只借用现有连接，不重复 Init/Connect。
- 运行时用 `QLibrary` 加载 `MeyerScan_Logger.dll`；CaseUI 只借用进程级 Logger，`Shutdown()` 只 Flush，不关闭全局日志会话。
- 测试宿主在创建 `QApplication` 前启用 High DPI 属性，并按当前屏幕可用区域居中显示。
- 界面可见文字统一使用 `tr("English source text")` 包装；即使需求写中文按钮名，源码 source text 也写英文，中文显示由 `.qm` 翻译文件提供。
- CRUD、参考数据读取、导入导出和加载订单规则后续进入 CaseOrderService、DataExport、OrderWorkflowService；CaseUI 不执行业务 SQL。
- CaseUI 必须可被 MainExe 随时释放并重建；后续从案例管理进入扫描重建前，MainExe 必须销毁 CaseUI widget，CaseUI 不保留必须跨页面存活的大缓存。
- 测试宿主根据 exe 所在目录推导日志目录和数据库配置路径，不依赖固定开发机路径。
- 模块变更记录维护在 `CHANGELOG.md`。

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
