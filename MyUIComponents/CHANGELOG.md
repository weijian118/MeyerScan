# MeyerScan UIComponents 变更记录

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
