# MeyerScan_CaseUI

CaseUI 是 MeyerScan 的 Qt Widgets 案例管理界面模块。

- 提供患者管理和订单管理页面，对应帮助文档中的浏览模块；顶部按钮负责页面级切换，默认订单页使用可随宽度自动换列的卡片流，患者页保留表格视图。
- 浏览页自行绘制品牌、设置、返回首页、最小化和关闭等页面语义顶部入口；最小化/关闭使用 `CaseActionMinimize` / `CaseActionClose` 上报 MainExe，CaseUI 不直接操作顶层窗口。
- 提供“返回首页”按钮，通过 `SetActionCallback()` 向 MainExe 上报 `CaseActionBackHome`；CaseUI 不直接切换首页。
- 通过 `SetActionVisible()` / `SetActionEnabled()` 接收 MainExe 计算后的操作显隐和启用态；CaseUI 不直接读取权限文件。
- `visible=false` 时入口隐藏，`enabled=false` 时入口保留但不可点击；真正动作执行前仍由 MainExe / Workflow / Service 复核权限。
- 顶部按钮和患者/订单工具栏按钮优先使用 `MeyerScan_UIComponents.dll` 的标准按钮样式；UIComponents 不可用时降级为本地按钮样式。
- UIComponents 加载后仍需检查 `Init()`；失败时清空接口并走 CaseUI 本地控件/QSS，不能把“DLL 存在”误判为“共享工厂可用”。
- 图标和 QSS 源文件归 `MyCaseUI/Resources` 管理，正式发布统一编译进 `MeyerScan_UIResources.dll`；公共加载器优先使用 `:/MeyerScan/Modules/MyCaseUI/...`。
- 点击返回、导入、导出、删除、新建、打开、搜索、页签切换等客户操作时写入结构化日志。
- 正式 `CaseUI` 不加载 Database、DatabaseQtAdapter 或 RuntimeDataCenter，也不接收数据库配置。
- MainExe 通过 `SetDataContextJson()` 注入 `local.patients` / `local.orders` 版本化快照；CaseUI 只读取 `domains.<name>.items` 并展示。
- 快照先完整解析并校验 `domains` 对象，成功后一次替换；非法输入保留上一份有效状态。
- 运行时用 `QLibrary` 加载 `MeyerScan_Logger.dll`；CaseUI 只借用进程级 Logger，`Shutdown()` 只 Flush，不关闭全局日志会话。
- 测试宿主在创建 `QApplication` 前启用 High DPI 属性，并按当前屏幕可用区域居中显示。
- 界面可见文字统一使用 `tr("English source text")` 包装；即使需求写中文按钮名，源码 source text 也写英文，中文显示由 `.qm` 翻译文件提供。
- CRUD、参考数据读取、导入导出和加载订单规则后续进入 CaseOrderService、DataExport、OrderWorkflowService；CaseUI 不执行业务 SQL。
- 新增/编辑/删除患者订单、状态变化、保存扫描方案等动作必须走 CaseOrderService / ScanSchemaService / Workflow；宿主快照只用于显示。
- CaseUI 必须可被 MainExe 随时释放并重建；后续从案例管理进入扫描重建前，MainExe 必须销毁 CaseUI widget，CaseUI 不保留必须跨页面存活的大缓存。
- 测试宿主根据 exe 所在目录推导日志目录，不依赖固定开发机路径。
- `CaseUITest.exe --smoke` 直接注入隔离 JSON fixture，验证患者表、订单卡片和顶部动作回调，不连接数据库或 RuntimeDataCenter。
- 测试宿主检查 CaseUI Init/CreateWidget 返回值；任何失败都显式 Shutdown 并用独立退出码报告，避免空指针错误掩盖真正初始化原因。
- `CaseUITest.exe --capture-screenshot <png> --capture-size <WxH>` 用于复查 1920x1080 四列和 1366x768 三列卡片布局。
- 模块变更记录维护在 `CHANGELOG.md`。

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
