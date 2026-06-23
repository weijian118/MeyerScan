# MeyerScan UIComponents

`MyUIComponents` 是共享 UI 组件模块的第一版骨架。

当前职责：

- 提供统一 DPI 缩放系数查询。
- 提供启动等待页、标题、主按钮、次按钮、输入框、下拉框等基础控件工厂。
- 后续承载主题、公共弹窗、表格样式、多语言刷新通知等 UI 基础能力。
- 控件工厂参数必须由调用方传入 `tr("English source text")` 后的结果；UIComponents 只负责显示和样式，不把中文 UI 源文案写进控件工厂。

设计结论：

- 弹窗先归入 UIComponents，不单独开模块。
- 业务型弹窗只组合 UIComponents 控件，业务规则仍放在对应 Service/Workflow/Permission。
- 多语言下不再按语言写 if/else 调整位置和大小，应使用 Qt Layout、最小宽度和内容自适应。
- 界面可见文字 source text 统一使用英文，并通过调用方 `tr()` 翻译；即使需求文档写中文按钮名，源码仍写英文 source text。
- 控件工厂只统一尺寸、样式和多语言适配基础，不决定按钮点击行为、不读取配置或权限。
