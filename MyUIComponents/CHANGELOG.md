# MeyerScan UIComponents 变更记录

## 2026-06-23

- 新增共享 UI 组件模块骨架。
- 提供 DPI 缩放系数、等待页、页面标题和主按钮工厂接口。
- 明确弹窗类先纳入 UIComponents，暂不单独拆模块。
- 等待页进度条高度改为按 `ScaleY()` 辅助缩放，避免高 DPI 下过细。
- 复查优化：补充次按钮、输入框、下拉框基础工厂接口，并设置多语言友好的 `QSizePolicy`、最小高度和标题换行策略；仍不承载业务规则和页面跳转。
- 补充 UI 文案规则：控件工厂只接收调用方已通过 `tr("English source text")` 翻译后的文本，不在 UIComponents 内写中文 source text。
- 复查验证：`MeyerScan_UIComponents.sln` Release x64 构建通过。
