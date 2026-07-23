# MyScanReconstructStudio

`MyScanReconstructStudio` 同时输出 `MeyerScan_ScanReconstructStudio.dll` 和 `ScanReconstructStudio.exe`。DLL 用于进程内嵌入，EXE 用于独立练习进程、SDK/API 或第三方独立运行。

## 职责

- DLL 通过 `IScanReconstructStudio::CreateWidget()` 创建可嵌入 QWidget；EXE 可独立打开扫描重建工作区，也可由 MeyerScan.exe 启动或激活。
- 通过 `QLibrary` 动态加载 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll`。
- 统一承载“扫描”和“数据处理”两个大阶段。嵌入 OrderScanWorkspaceShell 时外层壳拥有品牌和步骤导航，模块必须避免重复顶部；独立 EXE 才使用自己的独立窗口显示形态。
- 切换阶段前调用离开模块的 `DeactivateAndRelease()`，释放 `QVTKWidget`、VTK renderer、OpenGL/显存等重资源。
- 当前通过 UTF-8 JSON 上下文接收订单/会话信息；后续再接 IPC 状态同步。
- 子 DLL 的加载、工厂解析、Init、上下文校验和 QWidget 创建均为独立失败点；任一步失败都向 Initialize/RunSmoke 返回 false，不能显示空壳后报告成功。
- `SwitchToStep()` 只有在目标重页面成功创建后才更新当前步骤；切换前仍先释放离开页 QVTK/VTK/OpenGL 资源。

## 边界

- EXE/DLL 都是壳子，不实现设备协议、扫描算法、后处理算法、病例管理、云端上传或权限核心。
- 不把内部 VTK 对象、大块模型内存、Qt 模型对象跨进程传给主程序。
- `OrderScanWorkspaceShell.dll` 只负责工作台导航和页面承接，不把扫描和数据处理细节搬到工作台壳；DLL 形态使用进程内 C ABI/UTF-8 JSON，只有 EXE 独立进程形态需要 IPC。
- UI 可见文本源码必须使用 `tr("English source text")`。

## 第三方依赖

- Qt 5.6.3
- 运行时需要扫描 UI / 数据处理 UI 所依赖的 VTK 8.0、OpenCV 3.3 DLL。

VS2015 通过 `..\cmake\MeyerScanScanThirdParty.props` 复制运行时 DLL。迁移到其他电脑时优先设置：

- `QT_ROOT`
- `VTK_ROOT`
- `VTK_HEADERS_ROOT`
- `OPENCV_ROOT`

## 验证

- VS2015：打开 `MeyerScan_ScanReconstructStudio.sln`，编译 `Release|x64`，同时验证 EXE 和 DLL 工程。
- 烟测：运行 `bin\Release\ScanReconstructStudio.exe --smoke`，返回码为 0 表示动态加载、页面创建、阶段切换和资源释放链路通过。
- 模块 QSS 源码仍保存在本模块 `Resources`，正式发布由 `MeyerScan_UIResources.dll` 统一注册；VS2015 PostBuild 不再复制散 QSS。
- 人工看界面：运行 `bin\Release\ScanReconstructStudio.exe`。
- CMake：已用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器完成根聚合 `Release` 构建，`ScanReconstructStudio.exe --smoke` 返回 0。

## 2026-07-23 设备采集和数据处理上下文

本模块仍然是 Scan/Process 壳，不直接实现 USB、设备命令、解密或算法。DLL 形态和独立 EXE 形态都必须通过同一套采集服务和设备会话规则获取数据。

独立进程或嵌入工作台传入的上下文必须记录：

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

Scan 阶段消费快速链路产生的结果，Process 阶段消费异步后处理结果。跨进程只传订单/会话 ID、版本化 UTF-8 JSON 或文件路径，不传 USB 句柄、QObject、VTK 对象或图像内存所有权。详细采集方案见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。
