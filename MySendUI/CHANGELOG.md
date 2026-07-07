# MySendUI 修改记录

## 2026-07-07

- 新增 `MeyerScan_SendUI.dll` 初版框架，提供发送页 UI、动作回调、日志和版本导出。
- 新增 `ISendUI` 接口，动作 ID 包含 `Previous`、`Export`、`Compress`、`Email Send`、`Upload`、`Finish`。
- 新增 `SendUITest.exe` 测试宿主，覆盖模块初始化、上下文字段填充和完成按钮存在性。
- 新增 CMakeLists、VS2015 工程、版本资源文件和模块 README。
- 发送页只做界面和动作上报，不直接做真实导出、压缩、邮件或上传，后续由服务模块接入。
- CMake 输出目录补充复制 `MeyerScan_Logger.dll`，修复 `SendUITest.exe` 因缺运行时依赖无输出退出的问题。
- `SendUITest.exe` 入口重写为 VS2015 稳定格式，避免中文注释在当前代码页下被解析到同一行导致编译失败。
