# 变更记录

## 2026-07-13

- 补充底部 Start/Pause、Complete、Delete 按钮如何通过 Qt 信号/lambda 转换为稳定 actionId 的内部注释，强调 UI 不保存流程业务状态。
- 扫描页面行为和版本不变；`ScanWorkflowUITest` 已登记到根 CTest 清单。

## 2026-07-12

- 版本升级为 `v0.2.3`；`SetSessionContextJson()` 对非法 JSON 返回 false 并保留上一份有效上下文，避免缓存状态与已显示按钮分裂。
- `CreateWidget()` 只创建页面，不再隐式 `Activate()`；宿主完成挂载后显式激活，避免双重激活。当前步骤被禁用时选择第一个 enabled 步骤，全部禁用时保持无活动步骤。
- Logger 初始化失败时无日志降级；公开接口、内部状态、QVTK 释放、流程解析和测试宿主补充中文实现注释。
- `ScanWorkflowUITest.exe` 新增有效/非法上下文事务性、显式 Activate、流程按钮 tooltip/手型光标/动作回调验证并通过。

## 2026-07-10

- 版本升级为 `v0.2.2`；释放 QVTK 时先移除 renderer、清空 Viewer 的 renderer 指针并断开 render window/interactor，再延迟销毁原生窗口。
- `DeactivateAndRelease()` 同步清空旧页面非 owning 控件指针，避免页面延迟删除后误访问悬空地址。
- 扫描页样式迁入 `Resources/qss/scan_workflow.qss`，源码通过公共资源/QSS/日志辅助函数加载；CMake/VS2015 工程补齐模块资源复制。
- 保持 Scan 页只提供步骤内容和部位流程按钮，不复制 OrderScanWorkspaceShell 的工作台步骤导航；`Version.rc` 补齐版权字段。
- 与 ScanReconstructStudio 联合回归覆盖 Scan -> Process -> Scan 二次创建流程，模块测试和独立 EXE smoke 均返回 0。
- 删除 QSS 迁移后无引用的 C++ 颜色常量，避免后续维护者误以为界面仍有第二套源码样式。

## 2026-07-07

- 版本升级为 `v0.2.1`，顶部扫描流程按钮补充手型 hover、tooltip、选中态刷新和点击切换当前扫描部位显示数据。
- QVTK 显示区滚轮缩放改为以鼠标位置为中心，并在最小/最大缩放范围内夹紧实际缩放比例，避免越界后再被拉回。
- `ScanWorkflowUITest.exe` 增加 tooltip、手型光标和点击回调断言。
- 修复自定义 `ScanWorkflowViewerWidget` 放在匿名命名空间导致 VS2015 与头文件前置声明类型不一致的编译问题。
- 版本升级为 `v0.2.0`，新增从 session JSON 的 `scanProcess.steps` 渲染顶部扫描流程按钮；未传入流程时回退为练习默认流程：Natural maxilla / Exchange / Natural mandible / Natural occlusion。
- `SetSessionContextJson()` 现在会在页面已创建时刷新流程按钮，保证建单页修改扫描流程后，Scan 页面按钮能同步变化。
- `ScanWorkflowUITest.exe` 补充自定义 `scanProcess` 按钮渲染断言，验证 Scan 页面不再硬编码固定扫描对象按钮。
- 版本升级为 `v0.1.1`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本。
- 复核 MainExe / OrderScanWorkspaceShell 阶段性集成链路：扫描页可被练习工作台和创建工作台懒加载，离开页面时释放 QVTK/VTK/OpenGL 重资源。
- 当前仍只做扫描阶段 UI、动作上报和显示占位，不连接真实设备、不实现扫描算法。

## 2026-07-06

- 配合 MainExe 练习工作台接入，扫描页根控件最小尺寸从 1280x720 降为 960x600，QVTK 显示区和提示面板尺寸同步收敛，便于嵌入 `OrderScanWorkspaceShell`。
- 已通过 MainExe `--smoke-main` 验证 Practice → Scan → Process 链路中的扫描页创建、激活和离开释放流程。
- CMake 根聚合 `Release` 构建已通过；修正 CMake include 顺序，优先使用 `VTK_HEADERS_ROOT/include/vtk-8.0` 中可编译的 `QVTKWidget.h`。
- MainExe 运行时版本清单已能成功读取本模块 `GetMeyerModuleVersion()`，记录 `fileVersion=0.1.0.0`、`codeVersion=MeyerScan_ScanWorkflowUI v0.1.0`、`versionMatch=true`。
- 修复新增源码在 VS2015 代码页下中文注释乱码导致的编译解析问题，源码注释临时改为稳定 ASCII，模块说明继续在 README/CHANGELOG 中使用中文。
- 将信号连接改为 `QObject::connect()`，避免实现类不是 `QObject` 子类时编译失败。
- VS2015 `Release|x64` 编译通过，生成 `MeyerScan_ScanWorkflowUI.dll` 和 `ScanWorkflowUITest.exe`。
- 运行 `ScanWorkflowUITest.exe` 通过，验证 DLL 工厂、初始化、创建根 QWidget、对象名和资源释放链路。
- CMake 改为复用 `cmake/MeyerScanScanThirdParty.cmake`，第三方路径优先读取环境变量，便于项目迁移。

## 2026-07-05

- 新增 `MeyerScan_ScanWorkflowUI.dll` 初版骨架。
- 新增扫描阶段接口、VTK/QVTK 占位视图、扫描对象栏、右侧工具栏和底部控制区。
- 新增 `DeactivateAndRelease()`，明确离开扫描阶段必须释放 QVTKWidget / VTK renderer 等重资源。
- 新增 VS2015、CMake 和 `ScanWorkflowUITest.exe` 测试宿主。
