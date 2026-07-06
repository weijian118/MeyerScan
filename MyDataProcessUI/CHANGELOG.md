# 变更记录

## 2026-07-07

- 版本升级为 `v0.1.1`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 复核 MainExe / OrderScanWorkspaceShell 阶段性集成链路：处理页可被练习工作台和创建工作台懒加载，离开页面时释放 QVTK/VTK/OpenGL 重资源。
- 当前仍只做数据处理阶段 UI、工具入口、动作上报和显示占位，不实现真实编辑、测量、颈缘或后处理算法。

## 2026-07-06

- 配合 MainExe 练习工作台接入，数据处理页根控件最小尺寸从 1280x720 降为 960x600，QVTK 显示区尺寸同步收敛，便于嵌入 `OrderScanWorkspaceShell`。
- 已通过 MainExe `--smoke-main` 验证 Practice → Scan → Process 链路中的处理页创建、激活和离开释放流程。
- CMake 根聚合 `Release` 构建已通过；修正 CMake include 顺序，优先使用 `VTK_HEADERS_ROOT/include/vtk-8.0` 中可编译的 `QVTKWidget.h`。
- MainExe 运行时版本清单已能成功读取本模块 `GetMeyerModuleVersion()`，记录 `fileVersion=0.1.0.0`、`codeVersion=MeyerScan_DataProcessUI v0.1.0`、`versionMatch=true`。
- 修复新增源码在 VS2015 代码页下中文注释乱码导致的编译解析问题，源码注释临时改为稳定 ASCII，模块说明继续在 README/CHANGELOG 中使用中文。
- 将信号连接改为 `QObject::connect()`，避免实现类不是 `QObject` 子类时编译失败。
- 补充 `QVariant` 头文件，修复 `setProperty(..., true)` 在 VS2015 下无法完成类型转换的问题。
- VS2015 `Release|x64` 编译通过，生成 `MeyerScan_DataProcessUI.dll` 和 `DataProcessUITest.exe`。
- 运行 `DataProcessUITest.exe` 通过，验证 DLL 工厂、初始化、创建根 QWidget、对象名和资源释放链路。
- CMake 改为复用 `cmake/MeyerScanScanThirdParty.cmake`，第三方路径优先读取环境变量，便于项目迁移。

## 2026-07-05

- 新增 `MeyerScan_DataProcessUI.dll` 初版骨架。
- 新增数据处理阶段接口、VTK/QVTK 占位视图、模型选择栏、右侧处理工具栏和底部状态区。
- 新增 `DeactivateAndRelease()`，明确离开数据处理阶段必须释放 QVTKWidget / VTK renderer 等重资源。
- 新增 VS2015、CMake 和 `DataProcessUITest.exe` 测试宿主。
