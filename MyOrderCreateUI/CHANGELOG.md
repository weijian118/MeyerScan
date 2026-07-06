# MyOrderCreateUI 修改记录

## 2026-07-07

- 版本升级为 `v0.2.2`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 优化 OrderCreateUI 低分辨率适配：根界面最小尺寸从 1280x720 降为 960x600，布局边距、三栏间距、左/右栏宽度、扫描类型按钮、牙位按钮、已选表格高度和备注框高度都做了收敛。
- 明确分辨率策略：优先使用 Qt Layout、滚动区、伸缩策略和多语言换行，不再按 1920x1080 比例直接缩放所有控件坐标。
- 保持 UIComponents 复用边界不变：普通按钮、字段标签、输入框、下拉框、日期框、多行备注框和基础表格仍优先走共享 UI DLL；牙位/扫描类型等业务控件仍由本模块维护。
- 验证：CMake/VS2015 Release 构建通过；`OrderCreateUITest.exe` 返回 0；MainExe `--smoke-main` 集成链路返回 0。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。
- 版本升级为 `v0.2.1`，右侧已选牙位明细表改为优先通过 `MeyerScan_UIComponents.dll` 的 `CreateTableWidget()` 创建，表头、牙位数据和业务联动仍由本模块维护。
- 新增 UIComponents 运行时版本兼容检查：当前建单模块需要 UIComponents `v0.4.0` 及以上；如果运行目录中残留旧版 UIComponents，模块会主动走本地降级表格/控件样式，避免调用旧 DLL 不存在的虚接口。
- 保留本地降级表格样式，确保共享 UI DLL 缺失或版本过旧时建单界面仍可打开。
- 验证：`MeyerScan_OrderCreateUI.sln` Release x64 构建通过；模块输出和根输出目录的 `OrderCreateUITest.exe --smoke` 均返回 0；根方案 `MeyerScan_AllModules.sln` Release x64 构建通过。

## 2026-07-04

- 版本升级为 `v0.2.0`，优化建单界面视觉层级和样式统一方式。
- 建单页面新增顶部标题/说明区，三栏布局保持左侧基本信息、中间牙位方案、右侧订单摘要，但整体颜色、边框、间距和表格样式调整为更清爽的工作台风格。
- 通用按钮、字段标签、输入框、下拉框、日期框和多行备注框接入 `MeyerScan_UIComponents.dll`；牙位按钮、扫描类型按钮等业务控件仍由本模块维护，避免共享 UI 模块承载建单业务。
- `MeyerScan_UIComponents.dll` 改为 `QLibrary` 动态加载和可选降级：VS2015/CMake 工程保留头文件和 DLL 复制，不再链接 `MeyerScan_UIComponents.lib`，避免共享 UI 缺失时进程启动期直接失败。
- 根样式表收敛为页面容器、分组、表格、牙位/类型按钮等本模块专属样式；普通控件样式由 UIComponents 或本地降级函数管理。
- 验证：`MeyerScan_OrderCreateUI.sln` Release x64 构建通过；`OrderCreateUITest.exe --smoke` 返回 0。
- 新增 `SetOrderContextJson(const char*)`，支持接收标准建单上下文并填充患者信息、订单信息、医生/技工所、交付日期和扫描方案牙位。
- 标准上下文支持 `source.thirdPartyType`、`thirdPartyName`、`sourceSystem`、`sourceVersion`，用于显示和日志记录第三方来源。
- 支持在 `CreateWidget()` 前先调用 `SetOrderContextJson()`，模块会缓存上下文并在界面创建后应用，适配 MainExe 外部拉起流程。
- README 补充标准建单上下文示例，明确本模块不解析第三方私有字段。
- 调整 `OrderCreateUITest.exe` 启动行为：双击默认打开建单界面，`--smoke` 才执行自动冒烟测试后退出，避免人工运行时只看到控制台窗口闪退。
- 新增 `MeyerScan_OrderCreateUI.dll` 初版建单界面模块。
- 新增 `IOrderCreateUI` 接口和 `GetOrderCreateUI()` C ABI 工厂函数。
- 新增单页建单工作台界面，把患者基本信息、订单信息、扫描类型、牙位选择、已选明细和确认操作放到一个界面内。
- 新增牙位选择、清空、确认等动作回调，便于后续 MainExe 或 `OrderScanWorkspaceShell` 接收流程动作。
- 新增 `OrderCreateUITest.exe` 测试宿主，覆盖工厂函数、初始化、根控件、核心控件、牙位点击、清空和确认回调。
- 新增 VS2015 工程、CMakeLists.txt、版本资源和模块 README。
- 验证：单模块 `MeyerScan_OrderCreateUI.sln` Release x64 构建通过；单模块输出和根输出目录的 `OrderCreateUITest.exe --smoke` 均返回 0。

