# MySendUI 修改记录

## 2026-07-15

- 版本升级为 `v0.1.3`；新增公共接口 ABI 版本导出，并在动态使用 UIComponents 前校验其 ABI。

## 2026-07-12

- 版本升级为 `v0.1.2`；非法 session JSON 返回 false 并保留上一份有效上下文，删除生产界面中的测试患者、默认医生和 `LOCAL_ORDER` 伪数据。
- 新增末尾动作 `SendUIActionDataFormatChanged = 7`；程序填充上下文时用 `QSignalBlocker` 避免伪造客户操作，真实下拉选择才上报动作。
- Logger/UIComponents 初始化失败时清空接口并走显式降级；发送页仍只展示字段、记录日志和上报意图，不直接导出、压缩、发邮件或上传。
- 公开接口、内部控件状态、JSON 应用和测试宿主补充中文实现注释；`SendUITest.exe` 全部断言通过。

## 2026-07-10

- 版本升级为 `v0.1.1`，同步代码版本、CMake 和 `Version.rc`。
- STL/PLY/OBJ 虽是技术格式名，仍按全局多语言规则使用 `tr("English source text")` 包装。
- QSS 正式发布改由 `MeyerScan_UIResources.dll` 注册，源码 `Resources` 只作为资源归属和开发降级位置。
- 发送页样式迁入 `Resources/qss/send.qss`，源码通过公共资源/QSS/日志辅助函数加载；CMake/VS2015 工程补齐模块资源复制。
- SendUI 继续只提供 Send 步骤内容和动作上报，不复制工作台步骤导航；`Version.rc` 补齐版权字段。
- 删除 QSS 迁移后无引用的 C++ 颜色常量，界面样式只维护 `Resources/qss/send.qss`。

## 2026-07-07

- 新增 `MeyerScan_SendUI.dll` 初版框架，提供发送页 UI、动作回调、日志和版本导出。
- 新增 `ISendUI` 接口，动作 ID 包含 `Previous`、`Export`、`Compress`、`Email Send`、`Upload`、`Finish`。
- 新增 `SendUITest.exe` 测试宿主，覆盖模块初始化、上下文字段填充和完成按钮存在性。
- 新增 CMakeLists、VS2015 工程、版本资源文件和模块 README。
- 发送页只做界面和动作上报，不直接做真实导出、压缩、邮件或上传，后续由服务模块接入。
- CMake 输出目录补充复制 `MeyerScan_Logger.dll`，修复 `SendUITest.exe` 因缺运行时依赖无输出退出的问题。
- `SendUITest.exe` 入口重写为 VS2015 稳定格式，避免中文注释在当前代码页下被解析到同一行导致编译失败。
