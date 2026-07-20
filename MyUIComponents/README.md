# MeyerScan UIComponents

`MyUIComponents` 是共享 UI 组件模块。

当前职责：

- 提供统一 DPI 缩放系数查询。
- 提供启动等待页、标题、字段标签、按钮、输入框、下拉框、日期框、多行文本框、基础表格等基础控件工厂。
- 按“按钮角色 + 内容布局”统一管理常用按钮样式，例如主按钮、次按钮、纯文字按钮、危险按钮、首页入口按钮，以及纯文字/纯图标/左图右文/上图下文。
- 已提供公共分级弹窗；后续继续承载主题、复杂表格能力和多语言刷新通知等 UI 基础能力。
- 控件工厂参数必须由调用方传入 `tr("English source text")` 后的结果；UIComponents 只负责显示和样式，不把中文 UI 源文案写进控件工厂。

设计结论：

- 弹窗先归入 UIComponents，不单独开模块。
- 业务型弹窗只组合 UIComponents 控件，业务规则仍放在对应 Service/Workflow/Permission。
- 多语言下不再按语言写 if/else 调整位置和大小，应使用 Qt Layout、最小宽度和内容自适应。
- 界面可见文字 source text 统一使用英文，并通过调用方 `tr()` 翻译；即使需求文档写中文按钮名，源码仍写英文 source text。
- 控件工厂只统一尺寸、样式和多语言适配基础，不决定按钮点击行为、不读取配置或权限。
- 多个模块都会用到的通用控件和样式放入 UIComponents；只在单个业务模块出现的特殊/定制控件留在自身模块内部，避免共享模块膨胀成页面库。

## 按钮样式策略

`QPushButton` 和 `QToolButton` 先统一以下两个维度：

| 维度 | 当前取值 | 用途 |
|------|----------|------|
| 角色 | Primary / Secondary / Text / Danger / Entry | 表达视觉层级，不表达业务权限 |
| 内容布局 | TextOnly / IconOnly / IconLeftText / IconTopText | 表达图标和文字排列方式 |

调用方可以直接创建标准按钮，也可以对已经创建的按钮调用 `ApplyButtonStyle()` / `ApplyToolButtonStyle()` 套用统一样式。按钮的 `clicked` 连接、权限显隐、业务动作仍由调用方模块负责。

## 公共弹窗

- 单按钮 `MeyerUIComponents_ShowNoticeDialog` 支持信息、成功、失败/错误三级。
- 双按钮 `MeyerUIComponents_ShowDecisionDialog` 支持警告、高危两级；高危确认按钮使用危险角色样式。
- 两个接口是独立 C ABI，不插入 `IUIComponents` 虚函数表；动态调用方可只解析所需函数。
- 标题、正文和按钮文字必须由业务模块先通过 `tr("English source text")` 翻译；UIComponents 不决定错误原因、是否继续、权限或日志内容。
- 消息正文支持鼠标选择复制。正常发布使用公共 QSS，调用方可以在 DLL 缺失或 ABI 不兼容时保留功能性降级弹窗。

## 迁移状态

- MainExe 已使用 UIComponents 创建启动等待页。
- SettingsUI 的机器码和预检提示已使用公共单按钮弹窗；OrderCreateUI 的清空牙位确认已使用警告级双按钮弹窗。
- HomeUI 首页入口按钮已接入 `MeyerButtonRoleEntry`。
- CaseUI 顶部返回/设置按钮和患者/订单工具栏按钮已接入 Primary / Secondary / Danger 样式。
- OrderCreateUI 建单表单中的通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格基础样式已接入 UIComponents；牙位按钮、扫描类型按钮等建单业务控件仍留在 OrderCreateUI。
- SettingsUI 当前保留设置页内部专用样式，后续按页面逐步迁移通用按钮和表格样式；截图还原类布局仍属于 SettingsUI 自身职责。

## 表格样式策略

- `CreateTableWidget()` 只创建带统一视觉和默认只读/整行选择规则的 `QTableWidget`。
- `ApplyTableStyle()` 用于让已有表格逐步迁移到统一外观。
- 表头、列宽特殊规则、数据填充、排序、分页、右键菜单、双击打开等业务行为仍由调用模块负责。
- 新增表格接口同样只追加在 `IUIComponents` 末尾；动态加载 UIComponents 的模块如果会调用新接口，必须检查运行时 DLL 版本，旧版本不满足时走本地降级。

## 测试入口
- VS2015：打开 `MeyerScan_UIComponents.sln`，构建并运行 `UIComponentsTest.exe`。
- CMake/VSCode：默认开启 `UIComponentsTest` 测试目标，可通过 `MEYER_BUILD_UICOMPONENTSTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
