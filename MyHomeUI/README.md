# MeyerScan_HomeUI

HomeUI 是 MeyerScan 的 Qt Widgets 首页入口模块。

- 显示创建、浏览、练习、设置四个首页入口。
- 首页自行绘制符合参考图的品牌和顶部动作区，包括校准、云端、帮助、最小化和关闭；这些按钮只上报稳定入口 ID，真正窗口动作由 MainExe 执行。
- 通过 `SetEntryCallback()` 向 MainExe 上报入口 ID；HomeUI 不直接切换其他模块。
- 通过 `SetEntryVisible()` / `SetEntryEnabled()` 接收 MainExe 计算后的入口显隐和启用态；HomeUI 不直接读取权限文件。
- `visible=false` 时入口隐藏，`enabled=false` 时入口保留但不可点击；真正动作执行前仍由 MainExe / Workflow / Service 复核权限。
- 首页入口按钮优先使用 `MeyerScan_UIComponents.dll` 的 Entry 标准按钮语义；UIComponents 不可用时仍由首页根 QSS 提供降级样式，源码不拼接局部样式。
- UIComponents DLL 即使加载成功也必须检查 `Init()`；返回 false 时清空接口并走本地控件降级，禁止继续调用半初始化工厂。
- 首页背景、品牌、入口图标和 QSS 的源码仍归 `MyHomeUI/Resources` 管理；正式发布由 `MeyerScan_UIResources.dll` 注册为 `:/MeyerScan/Modules/MyHomeUI/...`，客户目录不再散放 PNG/QSS。
- `HomeUITest.exe --capture-screenshot <png> --capture-size <WxH>` 可生成固定尺寸模块截图；当前至少复查 1920x1080 和 1366x768。
- 点击入口时写入结构化日志，MainExe 再记录跨模块导航和页面切换日志。
- HomeUI 不直接调用 `MeyerScan_Database.dll`；首页只做入口展示和动作上报，数据库健康检查由 MainExe 通过 `MyDatabaseQtAdapter` 统一完成。
- HomeUI 不重复 Init/Connect 数据库，也不读取 RuntimeDataCenter 快照。
- `Init(appDir, logDir)` 不再接收数据库配置；Logger/UIComponents 使用 appDir 下绝对路径，并在获取虚接口前校验 API 版本。
- 运行时用 `QLibrary` 加载 `MeyerScan_Logger.dll`；HomeUI 只借用进程级 Logger，`Shutdown()` 只 Flush，不关闭全局日志会话。
- 测试宿主在创建 `QApplication` 前启用 High DPI 属性，并按当前屏幕可用区域居中显示。
- 界面可见文字统一使用 `tr("English source text")` 包装；即使需求写中文按钮名，源码 source text 也写英文，中文显示由 `.qm` 翻译文件提供。
- 业务规则保留在 Permission 和 OrderWorkflowService；HomeUI 不执行 SQL、不判断加载订单流程。
- 测试宿主根据 exe 所在目录推导应用目录和日志目录，不依赖固定开发机路径。
- 测试宿主必须检查 HomeUI 的 Init/CreateWidget 返回值，失败时 Shutdown 并返回独立错误码，不能对空 QWidget 继续查找控件。
- 模块变更记录维护在 `CHANGELOG.md`。

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_HomeUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
