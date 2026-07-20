# MeyerScan UIComponents 变更记录

## 2026-07-20 - 0.5.0

- 新增单按钮公共提示弹窗：信息、成功、失败/错误三级；新增双按钮选择弹窗：警告、高危两级。
- 弹窗通过独立 C ABI `MeyerUIComponents_ShowNoticeDialog` / `MeyerUIComponents_ShowDecisionDialog` 导出，不修改既有 `IUIComponents` 虚函数表。
- 调用方负责 `tr("English source text")`、业务日志和后续动作；UIComponents 只负责布局、标准图标、按钮和 QSS。
- 消息文本支持鼠标选择复制，便于复制设备机器码和错误详情；SettingsUI 和 OrderCreateUI 已接入，缺失新版 DLL 时保留 Qt 标准弹窗降级。
- `UIComponentsTest` 自动覆盖信息、成功、错误、警告和高危五种弹窗及确认/取消返回值。

## 2026-07-15

- 版本升级为 `v0.4.1`；新增公共接口 ABI 版本导出，Home/Case/OrderCreate/Send 等动态调用方在使用控件工厂前校验版本。

## 2026-07-13

- `UIComponentsTest` 使用命名 UTF-8 应用目录缓冲区初始化共享控件模块，继续覆盖按钮、输入框、表格、等待页和标题工厂。
- 共享控件 ABI/QSS 规则不变；测试已登记到根 CTest 清单，默认不显示窗口。

## 2026-07-10

- 共享控件视觉统一迁入 `Resources/qss/ui_components.qss`，模块初始化时通过公共 QSS 函数加载；控件工厂只设置 objectName/语义属性，不在源码拼接样式。
- CMake/VS2015 工程补齐模块资源复制；`Version.rc` 补齐版权字段；UIComponentsTest 和根方案构建通过。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。
- 版本升级为 `v0.4.0`，新增通用 `CreateTableWidget()` 和 `ApplyTableStyle()` 接口，先统一基础表格外观、表头样式、隔行色、只读默认行为和整行选择规则。
- 表格接口仍然只处理通用视觉和基础交互，不决定列名、数据来源、分页、排序、右键菜单、双击打开等业务行为。
- 新增接口继续追加在 `IUIComponents` 末尾，保持“不插入旧虚函数中间”的 ABI 规则。
- `UIComponentsTest.exe` 增加标准表格工厂验证。
- 验证：`MeyerScan_UIComponents.sln` Release x64 构建通过；根输出目录 `UIComponentsTest.exe` 返回 0；根方案 `MeyerScan_AllModules.sln` Release x64 构建通过。

## 2026-07-04

- 版本升级为 `v0.3.0`，新增通用 `CreateDateEdit()`、`CreateTextEdit()`、`CreateFieldLabel()` 接口，服务建单、设置等表单类界面。
- 新增输入类控件统一 QSS 生成函数，`QLineEdit`、`QComboBox`、`QDateEdit`、`QTextEdit` 复用同一套边框、背景、焦点态和 disabled/readOnly 规则。
- 新增接口追加在 `IUIComponents` 末尾，避免破坏旧模块已编译代码的 vtable 顺序；后续扩展虚接口也必须遵守“只追加不插入”的 ABI 规则。
- `UIComponentsTest.exe` 增加日期框、多行文本框和字段标签工厂验证。
- 验证：`MeyerScan_UIComponents.sln` Release x64 构建通过；`UIComponentsTest.exe` 返回 0。
- 补充 `UIComponentsTest.exe` 测试宿主中文注释，说明 UIComponents 初始化、缩放系数验证、常用控件工厂、等待页 objectName、`--show` 人工查看模式和 Shutdown 清理流程。
- 本轮仅补充注释，不改变 UIComponents 控件创建和样式逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步 UI 组件边界：本模块是 Qt 共享 UI 组件库，只统一控件、样式、等待页和多分辨率/多语言基础规则，不承载业务动作、权限判断或数据库访问。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `UIComponentsImpl.cpp`：补充 Qt Layout、`QSizePolicy`、QSS 选择器复用、按钮角色/内容布局、等待页无限进度条、图标加载、多语言文本变长和辅助缩放边界说明。
- 本轮只补充注释和文档记录，不改变控件工厂接口、样式或运行逻辑。

## 2026-06-26

- 版本升级为 `v0.2.0`，新增“按钮角色 + 内容布局”的标准按钮样式体系。
- 新增 `MeyerButtonRole`：Primary、Secondary、Text、Danger、Entry，用于表达按钮视觉层级，不表达业务权限。
- 新增 `MeyerButtonContentLayout`：TextOnly、IconOnly、IconLeftText、IconTopText，用于统一纯文字、纯图标、左图右文、上图下文等常见按钮结构。
- 新增 `CreateButton()` / `CreateToolButton()` / `ApplyButtonStyle()` / `ApplyToolButtonStyle()` 接口；调用模块可以创建标准控件，也可以对已有按钮套用统一样式。
- 保留 `CreatePrimaryButton()` / `CreateSecondaryButton()` 旧接口并转发到新工厂，降低已有模块迁移成本。
- 明确 UIComponents 只管理通用控件、通用样式、尺寸和多语言友好策略，不读取权限、不连接数据库、不决定页面跳转、不绑定业务 clicked 行为。
- HomeUI 首页入口按钮已接入 `MeyerButtonRoleEntry`；CaseUI 顶部返回/设置按钮和患者/订单工具栏按钮已接入 Primary / Secondary / Danger 样式。
- 更新 `Version.rc` 与 `ModuleInfo::Version` 到 `0.2.0`。
- 验证：`MeyerScan_UIComponents.sln` Release x64 构建通过；HomeUI / CaseUI / MainExe 联动 smoke 通过。

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；`GetModuleVersion()` 从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_UIComponents"`，保证后续日志宏输出正确共享 UI 模块名。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：屏幕可用区域、缩放系数限制、等待页布局、进度条状态、sizePolicy、多语言变长和 QSS 暂存边界均增加关键说明。
- 补充 `IUIComponents` 公共接口中文注释，明确控件工厂、等待页、辅助缩放系数和业务边界。
- 补充 UIComponents 头文件和实现文件的函数级中文注释，说明共享 UI 组件只统一基础控件、尺寸、样式和等待页，不承载业务点击行为、配置读取或权限判断。
- 进一步明确 1920x1080 缩放系数只用于图标、边距、控件高度等辅助尺寸，主布局仍应优先使用 Qt Layout。

## 2026-06-23

- 新增共享 UI 组件模块骨架。
- 提供 DPI 缩放系数、等待页、页面标题和主按钮工厂接口。
- 明确弹窗类先纳入 UIComponents，暂不单独拆模块。
- 等待页进度条高度改为按 `ScaleY()` 辅助缩放，避免高 DPI 下过细。
- 复查优化：补充次按钮、输入框、下拉框基础工厂接口，并设置多语言友好的 `QSizePolicy`、最小高度和标题换行策略；仍不承载业务规则和页面跳转。
- 补充 UI 文案规则：控件工厂只接收调用方已通过 `tr("English source text")` 翻译后的文本，不在 UIComponents 内写中文 source text。
- 复查验证：`MeyerScan_UIComponents.sln` Release x64 构建通过。

## 2026-07-03
- 新增 `UIComponentsTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
