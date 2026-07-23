# MyDataProcessUI

`MyDataProcessUI` 输出 `MeyerScan_DataProcessUI.dll`，是 `ScanReconstructStudio.exe` 内部“数据处理”阶段的界面模块。

## 职责

- 提供数据处理阶段完整页面：模型选择栏、右侧处理工具栏、独立的 Process Hint 提示框、底部状态栏和 VTK/QVTK 显示区。
- 提供截图、编辑、颈缘、倒凹、色彩、测量等工具入口。
- 进入页面时创建 `QVTKWidget` / `vtkRenderer` 等重资源；离开页面时通过 `DeactivateAndRelease()` 主动释放。
- 当前已被 MainExe 的练习工作台接入；页面最小尺寸按 960x600 收敛，便于嵌入 `OrderScanWorkspaceShell` 的 Process 步骤。
- 顶部处理流程按钮来自 session JSON 的 `scanProcess.steps`，必须与 ScanWorkflowUI 使用同一份流程，避免扫描和处理页面按钮不一致。
- 顶部流程按钮必须是可点击处理部位入口：hover 显示手型，提供 tooltip，点击后切换当前处理部位的显示数据和按钮选中态。
- `SetSessionContextJson()` 对非法 JSON 返回 false 并保留上一份有效上下文；全部步骤禁用时显示无可用步骤并清除旧 actor，不越权选择禁用步骤。
- `CreateWidget()` 与 `Activate()` 分离，宿主挂载完成后才激活；离开页面先 `DeactivateAndRelease()`，再由 Qt 父子关系销毁 QWidget。
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
- 测试宿主会传入自定义 `scanProcess.steps`，验证 Process 与 Scan 使用同一流程来源、非法 JSON 事务性、显式 Activate、tooltip、手型光标和点击回调。
- 人工看界面：运行 `bin\Release\DataProcessUITest.exe --show`。
- CMake：已用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器完成根聚合 `Release` 构建；CMake 会优先使用 `VTK_HEADERS_ROOT` 中的 QVTK 头文件，避免误读 VTK 安装目录中的生成/占位头。

## 2026-07-23 处理结果上下文

Process UI 不连接设备、不重新读取设备信息，也不创建采集线程。它消费后处理队列发布的 `ProcessedCaptureGroup`，并必须保留以下上下文：

```text
deviceSeries（必须）
deviceProfile（必须）
deviceIdStatus（必须）
deviceId（有则记录）
deviceModel/modelCode（有则记录）
productionMode
firmwareVersion
captureMode
scanHeadType
```

排序、相机 1 镜像、RGB 减黑图、解密和自动曝光不在本 UI 内实现。RGB 减黑图使用 `clamp(int(white) - int(black), 0, 255)`，下限为 0、上限为 255。若后处理队列丢弃新副本，UI 必须显示或记录可诊断状态，不得用上一组数据伪装成新组；关灯组跳过自动曝光但不跳过后处理结果。详细方案见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。
