# 变更记录

## 2026-07-06

- CMake 根聚合 `Release` 构建已通过，`ScanReconstructStudio.exe --smoke` 返回 0。
- MainExe 运行时版本清单已纳入本模块，记录 `fileVersion=0.1.0.0`、`codeVersion=ScanReconstructStudio v0.1.0`、`versionMatch=true`。
- MainExe 输出目录已补齐扫描 UI 所需 VTK/OpenCV 运行库复制，避免版本扫描或后续打开扫描工作区时因依赖缺失无法加载扫描 UI DLL。
- 修复新增源码在 VS2015 代码页下中文注释乱码导致的编译解析问题，源码注释临时改为稳定 ASCII，模块说明继续在 README/CHANGELOG 中使用中文。
- 将壳子内信号连接改为 `QObject::connect()`，保持非 QObject 辅助类和窗口类调用方式一致。
- VS2015 `Release|x64` 编译通过，生成 `ScanReconstructStudio.exe`。
- 运行 `ScanReconstructStudio.exe --smoke` 返回码为 0，验证动态加载扫描 UI / 数据处理 UI、页面创建、阶段切换和资源释放链路。
- CMake 工程规则已完成实际配置和构建验证。

## 2026-07-05

- 新增 `ScanReconstructStudio.exe` 初版壳子。
- 新增动态加载 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll` 的流程。
- 新增扫描/数据处理阶段切换，切换前释放离开页面的 VTK/QVTK 重资源。
- 新增 `--smoke` 自检入口，用于验证壳子动态加载、页面创建和切换释放链路。
