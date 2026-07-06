# 变更记录

## 2026-07-06

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
