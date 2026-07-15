# MyOrderCreateUI 修改记录

## 2026-07-15

- 版本升级为 `v0.5.4`，公共接口版本升级为 2；新增 `GetCurrentOrderContextJson()`，导出用户当前编辑后的患者、订单、治疗方案和扫描流程快照。
- 扫描步骤生成规则迁入独立 `MeyerScan_ScanSchemaService.dll`；OrderCreateUI 只构造输入配置并把稳定步骤编码翻译为界面文字。
- UIComponents/ScanSchemaService 均使用应用目录绝对路径和 API 版本门禁，服务缺失或 ABI 不匹配时明确失败并写日志。

## 2026-07-13

- 截图测试的 hover 类型和输出路径改为命名 UTF-8 缓冲区，补充定时 lambda 内部缓冲区生命周期注释；根 CMake 将 `OrderCreateUITest --smoke` 登记到统一 CTest 清单。
- `v0.5.3` 最终代码日期更新为 2026-07-13；标准建单 JSON 改为先解析候选字节、成功后才替换 pendingContext，非法 JSON 返回 false 并保留上一份有效缓存。
- 无上下文创建时使用空白患者、订单和牙位状态；删除生产 DLL 内固定患者编号、`Test Patient`、固定年龄/生日、测试医生/技工所、固定订单号及 15/16/47 示例牙位。
- 医生/技工所只保留可翻译的选择占位，真实列表后续由 RuntimeDataCenter 注入；生日使用最小日期哨兵和 `Select date` 占位，不伪造真实生日。
- smoke 新增“有效上下文后拒绝非法 JSON”和“Shutdown 后无上下文重建为空白表单”验证；单模块、根 VS2015/CMake 和 MainExe 集成链路通过。

## 2026-07-12

- 版本升级为 `v0.5.3`；Logger/UIComponents 初始化失败时清空接口并显式降级，UIComponents DLL 使用 `Init()` 传入的应用目录绝对定位，避免第三方拉起改变 currentPath 后误加载。
- 资源说明修正为“UIResources DLL 为正式入口，运行目录散文件仅兼容旧安装包”；补充治疗类型按钮图标缓存、hover/selected 状态和局部圆底合成的实现注释。
- 建单标准上下文、治疗方案和扫描流程规则边界不变；`OrderCreateUITest.exe --smoke` 全部断言通过。
- 版本升级为 `v0.5.2`。前四种修复类型 hover/选中时只切换资源自带的彩色圆形 `*_h.png` 和对应文字色，不再由 QSS 绘制整块深绿色矩形；种植体继续使用独立的浅绿普通态和深绿整行高亮态。
- 复核原始 PNG 后确认 `*_h.png` 实际只有白色图形、不含圆底；按钮从同类型 `*_b.png` 的非透明像素自动提取主色，在图标画布内合成圆底后叠加 h 图，1x/2x 共用同一算法且不在 C++ 硬编码主题颜色。
- 治疗类型按钮增加稳定英文 `treatmentCode` 动态属性，QSS 不依赖翻译后的显示文字即可分别管理全冠、缺失牙、嵌体、贴面和种植体视觉。
- 扫描流程预览改为固定单行高度并提供完整 tooltip，避免选择种植体后步骤增加、文字换行挤压中间牙弓，解决牙位图看起来缩小的问题。
- `OrderCreateUITest --smoke` 增加普通类型 hover 背景采样、QSS 类型属性和种植体前后牙弓尺寸不变断言。
- 截图宿主改为 `WA_DontShowOnScreen + setFixedSize` 离屏固定画布，避免 Windows 把 1920/2560 请求压缩到当前桌面可用区域；2K 图源选择通过仅测试动态属性使用目标视口尺寸。
- 真实 2560x1440 截图复核后给整个 Scan Plan 内容区增加 1060px 最大高度并在宿主中垂直居中，避免牙弓画布吃满 2K 高度后把上下颌过度拉散。
- 单模块 VS2015/CMake 测试增加 UIResources 构建依赖，PostBuild 复制同批资源 DLL；避免测试目录残留旧资源 DLL 时错误加载 7 月 10 日 QSS。
- 版本升级为 `v0.5.1`，治疗方案修复类型严格收口为全冠、缺失牙、嵌体、贴面、种植体五种，删除 `inner_crown` 和 `bridge` 类型按钮及旧编码入口。
- 单颗牙叠加图映射修正为全冠 `1`、缺失牙 `3`、嵌体 `4`、贴面 `5`、种植体 `7`，与现有 `12_1.png`、`12_3.png`、`12_4.png`、`12_5.png`、`12_7.png` 资源一致。
- 根据实际 mask 与叠加 PNG 的逐像素校验，修正上颌 `11..18,21..28`、下颌 `31..38,41..48` 牙位顺序及两颌桥 mask 顺序，解决左右牙位识别颠倒。
- 桥不再作为修复类型；任意两颗同颌相邻且已设置治疗方案的牙位显示空心连接点，点击后显示实心连接点。外部桥数据增加格式、相邻关系和两端牙位存在性校验。
- 修复类型按钮普通态使用 `*_b.png`，hover/选中态使用 `*_h.png`；2560x1440 及以上屏幕加载 `*_2x.png` 源图，高亮白色图标使用深绿色背景保证对比度。
- 多分辨率布局进一步收敛：左栏最小 380px、类型按钮最小 82px，避免 1366x768 下 `Missing Tooth` 截断；Scan Plan 内容最大 980px 并在可伸缩中栏内居中，避免 2K/4K 下区域过宽空泛。
- 新增 `TreatmentPlanResourceRules.h` 作为生产代码和测试共用的纯映射规则；测试不再直接链接 DLL 内部 C++ 类，桥候选通过 QWidget 只读动态属性验证，保持公共 C ABI 边界稳定。
- `OrderCreateUITest --smoke` 新增五类型、叠加序号、牙位/桥 mask 顺序、hover 图切换、2K 阈值、相邻牙桥候选和无效桥过滤断言。
- 验证：VS2015 单模块方案和根 `MeyerScan_AllModules.sln` Release x64 构建通过；模块目录与根目录 `OrderCreateUITest.exe --smoke`、`UIResourcesTest.exe --smoke` 返回 0；完成 1366x768、1920x1080、2560x1440 截图检查。

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
- 当时的阶段实现把 `bridge` 当作修复类型后显示连接点；该规则已在 v0.5.1 废止，当前 bridge 仅表示相邻已选牙之间的连接状态。
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
