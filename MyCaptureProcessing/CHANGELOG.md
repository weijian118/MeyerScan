# CHANGELOG

## 0.1.0 - 2026-07-24

- 建立原始 B 包、单图、组六图状态机和错序重同步。
- 接入历史图像逆替换解密，保留每张图 40 字节协议头。
- 实现组六图状态汇总、排序、相机 1 水平镜像和饱和减黑图。
- 提供纯 C ABI、VS2015、CMake、VSCode 工程和无硬件 smoke 测试。
- 模块级 CMake 启用 CTest，`ctest --preset vs2015-x64-release` 可独立发现并执行 smoke。
