# CHANGELOG

## 0.1.0 - 2026-07-24

- 建立会话级自动曝光 C ABI 和固定 16 字节命令输出结构。
- 预留设备上下文与历史曝光状态生命周期。
- 未实现算法明确返回 `NotImplemented`，禁止生成伪造有效参数。
- 补齐 VS2015、CMake、VSCode 工程和 smoke 测试。
- 模块级 CMake 启用 CTest，`ctest --preset vs2015-x64-release` 可独立发现并执行 smoke。
