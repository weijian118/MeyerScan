# CHANGELOG

## 0.1.0 - 2026-07-24

- 建立标准化六图之后的场景级流水线和多输出 C ABI。
- 实现 RGB888 显示输出与重建六图深副本。
- 增加功能开关快照、revision 和未接入算法显式失败合同。
- 补齐 VS2015、CMake、VSCode 工程和 smoke 测试。
- 模块级 CMake 启用 CTest，`ctest --preset vs2015-x64-release` 可独立发现并执行 smoke。
