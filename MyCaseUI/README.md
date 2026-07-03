# MeyerScan_CaseUI

CaseUI 是 MeyerScan 的 Qt Widgets 案例管理界面模块。

- 提供患者管理和订单管理页签框架，对应帮助文档中的浏览模块。
- 提供“返回首页”按钮，通过 `SetActionCallback()` 向 MainExe 上报 `CaseActionBackHome`；CaseUI 不直接切换首页。
- 通过 `SetActionVisible()` / `SetActionEnabled()` 接收 MainExe 计算后的操作显隐和启用态；CaseUI 不直接读取权限文件。
- `visible=false` 时入口隐藏，`enabled=false` 时入口保留但不可点击；真正动作执行前仍由 MainExe / Workflow / Service 复核权限。
- 顶部按钮和患者/订单工具栏按钮优先使用 `MeyerScan_UIComponents.dll` 的标准按钮样式；UIComponents 不可用时降级为本地按钮样式。
- 点击返回、导入、导出、删除、新建、打开、搜索、页签切换等客户操作时写入结构化日志。
- 正式 `CaseUI` 不直接调用 `MeyerScan_Database.dll`；列表展示读取 `RuntimeDataCenter` 快照，测试宿主造数才允许经 `MyDatabaseQtAdapter` 准备最小 SQLite 演示数据。
- 当前已动态加载 `MeyerScan_RuntimeDataCenter.dll`，患者页读取 `local.patients`，订单页读取 `local.orders`，用于展示运行时只读快照。
- CaseUI 只初始化 RuntimeDataCenter，不主动 `ReloadAll()`；MainExe 启动期负责全域刷新，独立测试或缓存为空时由 `GetDomainJson()` 按需懒加载当前 domain。
- 读取 RuntimeDataCenter domain 时采用有限扩容重试，避免字段扩展或列表稍大时固定缓冲区不足；超过上限后显示空列表并写 Warning，后续应改为分页/服务查询。
- 如果进程级 Database 已由 MainExe 通过 DatabaseQtAdapter 初始化并连接，CaseUI 只读取 RuntimeDataCenter 快照，不重复 Init/Connect。
- 运行时用 `QLibrary` 加载 `MeyerScan_Logger.dll`；CaseUI 只借用进程级 Logger，`Shutdown()` 只 Flush，不关闭全局日志会话。
- 测试宿主在创建 `QApplication` 前启用 High DPI 属性，并按当前屏幕可用区域居中显示。
- 界面可见文字统一使用 `tr("English source text")` 包装；即使需求写中文按钮名，源码 source text 也写英文，中文显示由 `.qm` 翻译文件提供。
- CRUD、参考数据读取、导入导出和加载订单规则后续进入 CaseOrderService、DataExport、OrderWorkflowService；CaseUI 不执行业务 SQL。
- RuntimeDataCenter 只用于读取上下文快照；新增/编辑/删除患者订单、状态变化、保存扫描方案等动作仍必须走 CaseOrderService / ScanSchemaService / Workflow。
- CaseUI 必须可被 MainExe 随时释放并重建；后续从案例管理进入扫描重建前，MainExe 必须销毁 CaseUI widget，CaseUI 不保留必须跨页面存活的大缓存。
- 测试宿主根据 exe 所在目录推导日志目录和数据库配置路径，不依赖固定开发机路径。
- `CaseUITest.exe --smoke` 会在空 SQLite 库中创建最小演示表并写入患者、订单、诊所、技工所、医生各一条数据，用于验证“数据库 -> RuntimeDataCenter -> CaseUI 患者/订单表格”的链路；正式 CaseUI 不负责建表、迁移或业务写入。
- 模块变更记录维护在 `CHANGELOG.md`。

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```

## 2026-07-01 补充说明

- `CaseUITest.exe --smoke` 会准备用于链路验证的 SQLite 演示库，当前不仅写入患者、订单、诊所、技工所、医生数据，也补齐软件信息、设置、账号、设备信息最小表。
- 这样做的目的只是让 `Database -> RuntimeDataCenter -> CaseUI/MainExe` 测试链路覆盖 RuntimeDataCenter 全部本地 domain，避免日志出现预期缺表 Warning。
- 正式 `MeyerScan_CaseUI.dll` 不负责建表、不负责旧库迁移、不负责业务写入；新增、编辑、删除、状态变更和正式 schema 初始化仍归 `CaseOrderService` / migration / Workflow。
