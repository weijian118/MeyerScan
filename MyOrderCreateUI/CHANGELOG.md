# MyOrderCreateUI 修改记录

## 2026-07-10

- 版本升级为 `v0.5.0`：左/中/右工作区改用不可折叠 `QSplitter`，中间牙弓获得主要伸缩空间，左右表单在 1366x768 下仍保持可读宽度。
- 治疗类型由四列调整为三列，避免英文和后续长翻译截断；右侧 Summary/Bridge/Shade 改成无嵌套卡片边框的语义分组。
- 窄屏下四个流程动作改为两行两列，消除按钮压缩和相邻按钮粘连。
- 治疗方案图片根目录优先使用 `MeyerScan_UIResources.dll` 的 `:/MeyerScan/Modules/MyOrderCreateUI/...`，保留旧散文件和源码目录兼容降级。
- 删除 VS2015/CMake 对治疗方案散文件的 PostBuild 复制和旧 `copy_treatment_assets.cmake`；测试宿主支持 1920x1080、1366x768 两种截图验收。
- 版本升级为 `v0.4.1`，同步更新代码版本、CMake 和 `Version.rc`。
- 删除建单页内部重复的 Order / Scan / Process / Send 步骤条及 `OrderCreateActionStepClicked`；工作台步骤导航唯一归 `MyOrderScanWorkspaceShell`，本模块只提供 Order 步骤内容。
- 清理源码中的局部 `setStyleSheet()`，治疗类型按钮、牙弓区域、摘要、表格和降级控件样式全部迁入 `Resources/qss/order_create.qss`。
- UIComponents 缺失或版本不兼容时仍只设置语义属性，由建单模块根 QSS 提供降级视觉，避免在 C++ 中重新拼样式字符串。
- 最终截图复核覆盖 1920x1080 与 1366x768；左侧长表单走滚动区，中间牙弓保持主工作区，右侧摘要和两行流程动作无重叠。
- 牙弓自绘画布背景和资源缺失提示颜色改由 QSS/Qt Palette 提供，C++ 只负责图片、mask 和交互绘制，不再保存界面色值。

## 2026-07-08

- 根据当前软件 `治疗方案选择.mp4` 继续校准布局：治疗类型面板从中间牙弓左侧迁移到整体左栏上方，左栏按“治疗类型 + 基本信息”组织，中间区域只保留牙弓主交互和扫描流程输入，右侧继续保留订单明细/标信息/操作按钮，更接近视频中的左/中/右工作台结构。
- 清理旧牙位按钮矩阵残留代码，牙位选择只保留 `ToothTreatmentPlanWidget + mask 命中 + 外层状态刷新` 一条链路，避免后续维护者误以为存在两套牙位选择实现。
- 治疗类型按钮恢复深色选中背景，保证高亮态白色图标在白底界面中可见；左侧新增当前治疗类型摘要，便于确认后续点击牙位使用的类型。
- `SetOrderContextJson()` 对外部 `scanPlan.bridgeConnectors` 增加校验：格式必须为 `牙位-牙位`，且两端牙位都必须是 `bridge` 类型，才允许进入桥记录和扫描流程 JSON；反向 key 会归一化为小号牙位在前的稳定格式。
- `OrderCreateUITest.exe --smoke` 增加非 bridge 脏桥连接点过滤断言，防止第三方或旧数据传入孤立桥连接点后污染 UI。
- `OrderCreateUITest.exe` 新增 `--capture-screenshot <png>` 视觉验收入口，固定 1920x1080 渲染建单界面并保存截图，便于和当前软件视频关键帧逐帧对齐。
- 补充本轮新增/调整函数内部中文注释，重点说明左侧工作区布局、截图验收模式、桥连接点校验和牙弓主交互刷新链路。
- 验证：`cmake --build F:\MeyerScan\build --config Release --target MeyerScan_OrderCreateUI OrderCreateUITest` 构建通过；`OrderCreateUITest.exe --smoke` 返回 0；`OrderCreateUITest.exe --capture-screenshot C:\Users\02241wj\AppData\Local\Temp\OrderCreateUITest_treatment_plan_latest.png` 成功生成截图。
- 版本升级为 `v0.4.0`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 新增 `ToothTreatmentPlanWidget`，使用 `maxilla.png` / `mandible.png` 显示上下颌牙弓，并通过 `maskMaxilla.png` / `maskMandible.png` 反查 FDI 牙位号。
- 治疗类型按钮改为图标在上、文字在下的轻量样式，并使用治疗方案资源中的普通态/高亮态图片。
- 支持牙位叠加图绘制：根据牙位号和治疗类型加载 `maxilla/<tooth>_<type>.png` 或 `mandible/<tooth>_<type>.png`。
- 支持桥连接点绘制和点击：相邻两颗牙均为 `bridge` 时显示空心连接点，点击后显示实心连接点。
- 桥记录按旧软件规则聚合：`16-17` + `17-18` 显示为 `16-18`；`11-12` + `11-21` 显示为 `11-22`。
- “Clear All” 按钮从左侧类型面板迁移到上下颌之间，人工模式弹确认框，smoke 模式跳过确认框避免阻塞。
- 治疗方案资源从历史 `bin/Release/icon/createModule/sacanPlan` 复制到源码目录 `Resources/icon/createModule/sacanPlan`，构建后复制到运行目录 `Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan`。
- `OrderCreateUITest.exe --smoke` 增加治疗方案、桥记录和跨中线桥记录断言。
- 验证：CMake/VS2015 Release 构建通过；`OrderCreateUITest.exe --smoke` 返回 0；已截图人工核对治疗方案区域布局。

## 2026-07-07

- 版本升级为 `v0.3.0`，新增建单页扫描流程创建功能，并同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 在建单页新增扫描流程输入控件：上颌异性扫描杆、下颌异性扫描杆、上颌扫描杆分段、下颌扫描杆分段四个开关，以及咬合类型下拉框（自然牙咬合、上颌临时牙咬合、下颌临时牙咬合、全口临时牙咬合、咬合记录）。
- 新增 `GetCurrentScanProcessJson()`，根据牙位/种植类型和扫描流程输入生成标准 `scanProcess` JSON；MainExe 只读取并转发该 JSON，不解析具体规则。
- 扫描流程 JSON 中包含 `schemaVersion/source/config/steps`，`steps` 为 ScanWorkflowUI/DataProcessUI 需要渲染的按钮列表；新增 `OrderCreateActionScanProcessChanged` 用于通知外部流程更新。
- `OrderCreateUITest.exe --smoke` 补充扫描流程控件和 JSON 生成断言，覆盖异性杆分段和咬合记录步骤。
- 版本升级为 `v0.2.2`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 优化 OrderCreateUI 低分辨率适配：根界面最小尺寸从 1280x720 降为 960x600，布局边距、三栏间距、左/右栏宽度、扫描类型按钮、牙位按钮、已选表格高度和备注框高度都做了收敛。
- 明确分辨率策略：优先使用 Qt Layout、滚动区、伸缩策略和多语言换行，不再按 1920x1080 比例直接缩放所有控件坐标。
- 保持 UIComponents 复用边界不变：普通按钮、字段标签、输入框、下拉框、日期框、多行备注框和基础表格仍优先走共享 UI DLL；牙位/扫描类型等业务控件仍由本模块维护。
- 验证：CMake/VS2015 Release 构建通过；`OrderCreateUITest.exe` 返回 0；MainExe `--smoke-main` 集成链路返回 0。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。
- 版本升级为 `v0.2.1`，右侧已选牙位明细表改为优先通过 `MeyerScan_UIComponents.dll` 的 `CreateTableWidget()` 创建，表头、牙位数据和业务联动仍由本模块维护。
- 新增 UIComponents 运行时版本兼容检查：当前建单模块需要 UIComponents `v0.4.0` 及以上；如果运行目录中残留旧版 UIComponents，模块会主动走本地降级表格/控件样式，避免调用旧 DLL 不存在的虚接口导致崩溃。
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
