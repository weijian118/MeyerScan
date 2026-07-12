# 变更记录

## 2026-07-12

- 版本升级为 `v0.2.3`；非法 session JSON 返回 false 且不覆盖上一份有效上下文，CreateWidget 与 Activate 生命周期分离。
- 当前步骤禁用时选择第一个 enabled 步骤；全部禁用时显示无可用步骤并清空旧 actor，不再越权强制选择索引 0。
- Logger 初始化失败时显式降级；补充公开接口、QVTK/VTK 生命周期、流程按钮、占位数据和测试代码的中文实现注释。
- `DataProcessUITest.exe` 覆盖上下文事务性、显式 Activate、流程切换和“不出现 Scan Start/Pause”规则并通过。

## 2026-07-10

- 版本升级为 `v0.2.2`；QVTK 释放顺序改为“移除 renderer -> 清内部 renderer 指针 -> 断开 render window/interactor -> 延迟删除控件 -> 删除 renderer”。
- 清空旧 Process 页面非 owning 控件指针，修复 Scan/Process/Scan 往返切换后的悬空引用风险。
- 数据处理页样式迁入 `Resources/qss/data_process.qss`，源码通过公共资源/QSS/日志辅助函数加载；CMake/VS2015 工程补齐模块资源复制。
- 保持 Process 页只提供步骤内容和部位流程按钮，不复制 OrderScanWorkspaceShell 的工作台步骤导航；`Version.rc` 补齐版权字段。
- 与 ScanReconstructStudio 联合回归覆盖 Scan -> Process -> Scan 二次创建流程，模块测试和独立 EXE smoke 均返回 0。
- 删除 QSS 迁移后无引用的 C++ 颜色常量，避免后续维护者误以为界面仍有第二套源码样式。

## 2026-07-07

- 版本升级为 `v0.2.1`，顶部处理流程按钮补充手型 hover、tooltip、选中态刷新和点击切换当前处理部位显示数据。
- Process 页新增独立的 `Process Hint` 提示框，不复用 Scan 页左下角提示内容；底部不增加扫描页的 `Start / Pause` 控制。
- QVTK 显示区滚轮缩放改为以鼠标位置为中心，并在最小/最大缩放范围内夹紧实际缩放比例，避免越界后再被拉回。
- `DataProcessUITest.exe` 增加 tooltip、手型光标和点击回调断言。
- 修复自定义 `DataProcessViewerWidget` 放在匿名命名空间导致 VS2015 与头文件前置声明类型不一致的编译问题。
- 版本升级为 `v0.2.0`，新增从 session JSON 的 `scanProcess.steps` 渲染顶部处理流程按钮；未传入流程时回退为练习默认流程。
- `SetSessionContextJson()` 现在会在页面已创建时刷新流程按钮，保证 Process 页面与 Scan 页面使用同一份建单扫描流程。
- `DataProcessUITest.exe` 补充自定义 `scanProcess` 按钮渲染断言，避免处理页继续硬编码旧模型按钮。
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
