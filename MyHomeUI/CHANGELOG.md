# MeyerScan HomeUI 变更记录

## 2026-07-10

- 版本升级为 `v0.3.1`，首页品牌图改用参考界面的中英组合 Logo，并保持按高度等比缩放。
- 版本升级为 `v0.3.0`：根控件按窗口尺寸等比绘制完整产品背景，右侧四入口继续使用 Qt Layout 自适应，不再把背景图塞进左侧局部面板。
- 修复历史 HomeCreate/HomeBrowse 等资源实际是整张卡片、直接缩为 92px 导致圆形图标过小的问题；运行时按相对区域裁出入口视觉，普通/悬停态均完整登记。
- `HomeUITest.exe` 新增 `--capture-screenshot <png> --capture-size <WxH>`，已用于 1920x1080 和 1366x768 实际截图验收。
- 图标/QSS 正式发布改走 `MeyerScan_UIResources.dll`；资源 DLL 缺失时，源码树单模块调试才回退到本模块 `Resources`。
- 版本升级为 `v0.2.1`，同步更新代码版本、CMake 和 `Version.rc`。
- 首页顶部区域增加品牌图片、校准、云端、帮助、最小化和关闭入口；窗口动作使用稳定 ID 回调 MainExe，HomeUI 不直接操作顶层窗口。
- 改为统一使用 `Common/include/MeyerQtModuleUtils.h` 解析模块资源、加载 QSS 和写 Qt 日志，删除模块内重复工具函数。
- HomeUITest 增加顶部动作按钮回调验证，确认页面动作 ID 与 MainExe 约定一致。
- 最终截图复核覆盖 1920x1080 与 1366x768：品牌、五个顶部动作、完整背景和四入口均无裁切或重叠。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 参与全模块偏移复查：确认首页仍只展示入口、渲染 visible/enabled 状态并通过入口 ID 回调 MainExe；`Create` 入口由 MainExe 编排进入 `OrderScanWorkspaceShell/OrderCreateUI`，HomeUI 不直接创建建单页、不保存订单、不判断第三方字段。
- 复测根输出目录 `HomeUITest.exe --smoke` 返回 0；本轮不改变 HomeUI 业务逻辑。

## 2026-07-02

- 更新模块 `CMakeLists.txt`，改为复用根目录公共 CMake 规则，并补齐 Logger、UIComponents 等当前依赖；HomeUI 正式 DLL 不再依赖 Database。
- 按评审结论同步 UI/业务分离规则：HomeUI 继续作为 Qt 入口 UI，只展示入口、记录操作和上报入口 ID；建单规则、加载订单规则、权限核心判断不得进入首页模块。
- HomeUI 不再直接接入 Database 健康检查；启动期数据库检查由 MainExe 通过 `MyDatabaseQtAdapter` 统一完成。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、测试宿主和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `HomeUIImpl.cpp`：补充 `QLibrary` 动态加载、UIComponents 可选降级、入口回调 C ABI、按钮 lambda 捕获、`QByteArray::constData()` 生命周期、Qt Layout 和基础设施借用边界说明。
- 本轮只补充注释和文档记录，不改变首页入口、显隐/启用态或 UIComponents 接入逻辑。

## 2026-06-26

- 首页入口按钮接入 `MeyerScan_UIComponents.dll` 的标准按钮工厂，统一使用 `MeyerButtonRoleEntry` 样式。
- 新增运行时动态加载 UIComponents 的缓存逻辑；共享 UI 模块不可用时保留本地 `QPushButton` 降级样式，保证首页仍可启动。
- 明确 HomeUI 只负责入口页面结构、入口 ID 上报和入口状态渲染；按钮样式归 UIComponents，权限显隐/启用态仍由 MainExe 下发。
- VS2015 工程增加 `..\MyUIComponents\include`，Release PostBuild 复制 `MeyerScan_UIComponents.dll`，保证测试宿主和主程序运行目录可加载共享样式模块。
- 复查版本信息：`ModuleInfo::Version` 和 `Version.rc` 升级为 v0.2.0，避免版本清单继续显示旧框架版本。
- 验证：`MeyerScan_HomeUI.sln` Release x64 构建通过，`HomeUITest.exe --smoke` 返回 0。

## 2026-06-25

- 新增 `SetEntryEnabled()` 接口，接收 MainExe 下发的入口启用态；`enabled=false` 时按钮显示但禁用。
- 首页入口按钮创建时同时应用 `visible` 和 `enabled`，后续配置/权限变化可继续沿用同一入口状态模型。
- Qt 模块日志调用改为使用 `Logger.h` 的 `QString` 便捷接口，跨 DLL ABI 仍保持 UTF-8 `const char*`。
- 日志 `module` 字段统一使用 `MeyerScan_HomeUI`，与 VS2015 工程 `MEYER_MODULE_NAME` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名、Debug 标志、`FILEOS` 和 `FILETYPE` 与全项目版本资源规范保持一致。
- 确认 `MEYER_MODULE_NAME="MeyerScan_HomeUI"` 已存在，日志宏可输出稳定首页模块名。
- 为 `HomeUITest` 测试宿主补充独立 `MEYER_MODULE_NAME`，避免测试日志混入正式模块名。

## 2026-06-24

- 对齐新版 Logger 规则：HomeUI 当前没有真实登录人/操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段，避免输出无意义 `[Op:System]`。
- 根据“初学者可读”要求补强函数体内部注释：初始化、日志加载、数据库借用、入口显隐、Qt 父子关系、按钮回调、Shutdown 和测试宿主路径推导均增加关键说明，不只保留函数头注释。
- 补充 `IHomeUI` 公共接口和 `HomeUITest` 测试宿主的中文注释，明确入口回调、显隐控制、路径推导和冒烟测试流程。
- 补充 HomeUI 头文件和实现文件的函数级中文注释，说明首页只负责入口展示和入口 ID 上报，不承载建单、订单加载、扫描启动或权限核心判断。
- 补充 Logger/Database 生命周期注释：HomeUI 只缓存并借用接口，不关闭进程级 Logger / Database。

## 2026-06-22

- 2026-06-23 补充：新增 `SetEntryVisible()`，用于接收 MainExe/Permission 下发的首页入口显隐规则；当前可控制“设置”入口显示状态。
- 新增 `SetEntryCallback()` 入口事件接口，首页按钮只上报入口 ID，由 MainExe 负责页面切换。
- 点击创建、浏览、练习、设置入口时写入 `EntryClicked` 日志。
- “浏览”入口已接入 MainExe，可切换到 CaseUI；其他入口暂保留日志和状态提示。
- 2026-06-23 再次补充：测试宿主不再硬编码 `F:/MeyerScan/...`，改为根据 exe 所在目录推导模块日志目录和 MyDatabase 配置路径。
- 2026-06-23 历史口径：HomeUI 当时曾借用进程级 Logger/Database 做框架期健康检查；2026-07-03 起已改为正式 DLL 不接入 Database，数据库健康检查统一由 MainExe 通过 DatabaseQtAdapter 完成。
- 2026-06-23 历史口径：当时曾检查进程级 Database 是否已连接；2026-07-03 起 HomeUI 不再重复 Init/Connect，也不再包含 Database 依赖。
- 2026-06-23 复查补充：界面可见文字从 `QApplication::translate()` 统一改为 `tr("English source text")`；源码不写中文 UI 文案，中文显示后续由模块 `.qm` 提供。
- 2026-06-23 复查验证：`MeyerScan_HomeUI.sln` Release x64 构建通过，`HomeUITest.exe --smoke` 返回 0。
- 新增模块级变更记录文件。
- 历史记录：当时 Database 调用只用于框架 smoke 健康检查；当前 HomeUI 已移除 Database 调用。
- HomeUI 正式业务行为应调用 Permission 和 OrderWorkflowService，不直接执行业务 SQL，也不直接承载病例/订单规则。
- 修正 Logger 生命周期边界：使用 `QLibrary::PreventUnloadHint` 避免退出阶段 DLL 卸载顺序问题。
- 当前 HomeUI 不做 Database 健康检查；正式业务后续迁入 Service/Workflow。
- 调整 `HomeUITest.exe` 生命周期：退出前先关闭并删除顶层 widget，再执行模块 `Shutdown()`。

## 2026-06-18

- 在测试宿主中于 `QApplication` 创建前启用 High DPI 相关属性。
- `HomeUITest.exe` 增加按当前屏幕可用区域居中和限制初始尺寸的逻辑。
- 界面可见文本完成国际化预埋，为后续独立 `.qm` 翻译文件预留基础。
- README 补充 DPI 和多语言翻译说明。

## 2026-06-17

- 创建 Qt Widgets DLL 框架和 `HomeUITest.exe`。
- 新增首页四个入口框架：创建、浏览、练习、设置。
- 历史记录：初始框架曾接入 Logger 和 Database 用于启动链路验证；当前正式 DLL 仅接入 Logger/UIComponents，不直连 Database。
- 在 PostBuild 中加入 Qt 5.6.3 运行库复制规则。
