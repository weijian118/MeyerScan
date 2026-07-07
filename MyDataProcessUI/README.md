# MyDataProcessUI

`MyDataProcessUI` 输出 `MeyerScan_DataProcessUI.dll`，是 `ScanReconstructStudio.exe` 内部“数据处理”阶段的界面模块。

## 职责

- 提供数据处理阶段完整页面：模型选择栏、右侧处理工具栏、独立的 Process Hint 提示框、底部状态栏和 VTK/QVTK 显示区。
- 提供截图、编辑、颈缘、倒凹、色彩、测量等工具入口。
- 进入页面时创建 `QVTKWidget` / `vtkRenderer` 等重资源；离开页面时通过 `DeactivateAndRelease()` 主动释放。
- 当前已被 MainExe 的练习工作台接入；页面最小尺寸按 960x600 收敛，便于嵌入 `OrderScanWorkspaceShell` 的 Process 步骤。
- 顶部处理流程按钮来自 session JSON 的 `scanProcess.steps`，必须与 ScanWorkflowUI 使用同一份流程，避免扫描和处理页面按钮不一致。
- 顶部流程按钮必须是可点击处理部位入口：hover 显示手型，提供 tooltip，点击后切换当前处理部位的显示数据和按钮选中态。
- Process 页左下角提示框不复用 Scan 页文案；后续产品文案确定后只改本模块提示内容。
- Process 页不放扫描页底部中间的 `Start / Pause` 等扫描控制按钮，只保留处理页自己的 Previous / Next / 状态栏。
- QVTK 视图滚轮缩放必须以鼠标所在位置为中心，并在允许范围内夹紧缩放值，避免先超过限制再“拉回”的观感。
- 通过稳定整数动作 ID 向 `ScanReconstructStudio.exe` 上报用户意图。

## 边界

- 不连接设备，不做扫描采集。
- 不解析建单页开关，不生成扫描流程规则；只消费 `scanProcess.steps` 并渲染按钮。
- 不在 UI 模块内实现重算法；后续编辑、颈缘、测量、倒凹、咬合、底座、数据 IO 和预处理等能力优先拆成专用 DLL 或独立库。
- 不跨 DLL / 跨进程传递 VTK 对象、大块模型内存所有权、QObject 或 Qt 模型对象。
- UI 可见文本源码必须使用 `tr("English source text")`。

## 第三方依赖

- Qt 5.6.3
- VTK 8.0 + `QVTKWidget`
- OpenCV 3.3

VS2015 通过 `..\cmake\MeyerScanScanThirdParty.props` 查找依赖。CMake 通过 `..\cmake\MeyerScanScanThirdParty.cmake` 查找依赖。迁移到其他电脑时优先设置环境变量：

- `QT_ROOT`
- `VTK_ROOT`
- `VTK_HEADERS_ROOT`
- `OPENCV_ROOT`

## 验证

- VS2015：打开 `MeyerScan_DataProcessUI.sln`，编译 `Release|x64`。
- 测试宿主：运行 `bin\Release\DataProcessUITest.exe`。
- 测试宿主会传入自定义 `scanProcess.steps`，验证 Process 页面与 Scan 页面使用同一流程来源，并检查 tooltip、手型光标和点击回调。
- 人工看界面：运行 `bin\Release\DataProcessUITest.exe --show`。
- CMake：已用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器完成根聚合 `Release` 构建；CMake 会优先使用 `VTK_HEADERS_ROOT` 中的 QVTK 头文件，避免误读 VTK 安装目录中的生成/占位头。
