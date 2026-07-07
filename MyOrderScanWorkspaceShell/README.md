# MeyerScan OrderScanWorkspaceShell

`MyOrderScanWorkspaceShell` 输出 `MeyerScan_OrderScanWorkspaceShell.dll`，用于统一建单、扫描、数据处理和发送之间的页面壳。

## 当前定位

- 建单模块和扫描重建模块直接相邻，用户体验上不能割裂。
- 本模块负责顶部步骤、页面容器、当前步骤切换和后续进程/窗口嵌入的统一边界。
- 顶部 Order / Scan / Process / Send 必须是可点击步骤按钮，点击后由壳子内部调用 `SetStep()` 切换 `QStackedWidget` 页面；不能用只展示文字的 `QLabel` 伪装按钮。
- 壳子支持两种模式：`WorkspaceModeOrderCreate` 显示 Order / Scan / Process / Send；`WorkspaceModePractice` 只显示 Scan / Process，用于首页“Practice”入口。
- 创建模块和练习模块右上角只保留 `Minimize` 和 `Close` 两个壳按钮；壳子只通过回调上报动作，具体最小化或关闭工作台回首页由 MainExe 处理。
- `OrderCreateUI`、`ScanWorkflowUI`、`DataProcessUI`、`SendUI` 和后续 `ScanReconstructStudio.exe` 作为工作台内页面或进程容器接入。
- 本模块是 Qt Widgets UI 容器模块，可以使用 `QWidget`、`QStackedWidget`、Qt Layout、信号槽、`QString` 和 `QMap` 组织界面；建单保存、加载规则、扫描采集和数据处理不进入本模块。跨进程同步扫描状态时必须收敛为 IPC/POD/UTF-8 JSON。
- 当前 MainExe 已把 `OrderCreateUI` 挂载到 `WorkspaceStepOrderCreate`；首页点击“Create”和第三方自动拉起建单都进入本工作台壳。

## 边界

- 只做容器、导航和视觉一致性。
- 不保存建单数据。
- 不做加载订单规则。
- 不做扫描采集、算法重建或设备通信。
- 重资源释放仍由 MainExe 和 ScanReconstructStudio 的生命周期规则统一控制。
- 壳子通过步骤变化回调通知 MainExe 懒加载 ScanWorkflowUI / DataProcessUI / SendUI；Scan / Process 离开页面时必须释放隐藏页重资源，SendUI 当前为轻量页面但仍由 MainExe 统一管理生命周期。
- 不负责把 Qt 对象跨进程传给 ScanReconstructStudio；扫描进程只接收订单 ID、上下文 JSON/文件路径和状态命令。
- 不知道第三方来源字段含义；第三方字段只在 ExternalLaunchAdapter 和 OrderCreateUI 标准上下文中处理。
- 是否允许进入某一步后续由 OrderWorkflowService / Permission 输出禁用策略；当前初版先保证按钮点击和页面切换链路可用。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_OrderScanWorkspaceShell.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口
- VS2015：打开 `MeyerScan_OrderScanWorkspaceShell.sln`，构建并运行 `OrderScanWorkspaceShellTest.exe`。
- CMake/VSCode：默认开启 `OrderScanWorkspaceShellTest` 测试目标，可通过 `MEYER_BUILD_ORDERSCANWORKSPACESHELLTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
- 当前测试覆盖创建模式步骤点击、练习模式只显示 Scan/Process、非法 step 防崩溃和根控件释放。
