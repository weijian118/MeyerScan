# MyScanWorkflowUI

`MyScanWorkflowUI` 输出 `MeyerScan_ScanWorkflowUI.dll`，是 `ScanReconstructStudio.exe` 内部“扫描”阶段的界面与流程边界模块。

## 职责

- 提供扫描阶段完整页面：扫描对象选择、右侧扫描工具、底部扫描控制、提示区和 VTK/QVTK 显示区。
- 后续承接连接设备、抓取下位机数据、把数据传给算法、接收重建结果并刷新显示的 UI 边界。
- 进入页面时创建 `QVTKWidget` / `vtkRenderer` 等重资源；离开页面时通过 `DeactivateAndRelease()` 主动释放。
- 当前已被 MainExe 的练习工作台接入；页面最小尺寸按 960x600 收敛，便于嵌入 `OrderScanWorkspaceShell` 的 Scan 步骤。
- 顶部扫描流程按钮来自 session JSON 的 `scanProcess.steps`；创建模式由 OrderCreateUI 生成并经 MainExe 转发，练习模式由 MainExe 固定生成默认流程。
- 顶部流程按钮必须是可点击扫描部位入口：hover 显示手型，提供 tooltip，点击后切换当前扫描部位的显示数据和按钮选中态。
- QVTK 视图滚轮缩放必须以鼠标所在位置为中心，并在允许范围内夹紧缩放值，避免先超过限制再“拉回”的观感。
- 通过稳定整数动作 ID 向 `ScanReconstructStudio.exe` 上报用户意图。

## 边界

- 不保存患者/订单，不访问数据库，不判断加载订单规则。
- 不解析建单页开关，不生成扫描流程规则；只消费 `scanProcess.steps` 并渲染按钮。
- 不直接实现底层设备传输、设备命令、扫描算法或数据处理算法。
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

- VS2015：打开 `MeyerScan_ScanWorkflowUI.sln`，编译 `Release|x64`。
- 测试宿主：运行 `bin\Release\ScanWorkflowUITest.exe`。
- 测试宿主会传入自定义 `scanProcess.steps`，验证顶部按钮不再硬编码旧扫描对象列表，并检查 tooltip、手型光标和点击回调。
- 人工看界面：运行 `bin\Release\ScanWorkflowUITest.exe --show`。
- CMake：已用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器完成根聚合 `Release` 构建；CMake 会优先使用 `VTK_HEADERS_ROOT` 中的 QVTK 头文件，避免误读 VTK 安装目录中的生成/占位头。
