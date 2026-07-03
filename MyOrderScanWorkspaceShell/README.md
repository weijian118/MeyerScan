# MeyerScan OrderScanWorkspaceShell

`MyOrderScanWorkspaceShell` 输出 `MeyerScan_OrderScanWorkspaceShell.dll`，用于统一建单、扫描、数据处理和发送之间的页面壳。

## 当前定位

- 建单模块和扫描重建模块直接相邻，用户体验上不能割裂。
- 本模块负责顶部步骤、页面容器、当前步骤切换和后续进程/窗口嵌入的统一边界。
- `OrderCreateUI`、`ScanReconstructStudio.exe`、处理页和发送页后续作为工作台内页面或进程容器接入。
- 本模块是 Qt Widgets UI 容器模块，可以使用 `QWidget`、`QStackedWidget`、Qt Layout、信号槽、`QString` 和 `QMap` 组织界面；建单保存、加载规则、扫描采集和数据处理不进入本模块。跨进程同步扫描状态时必须收敛为 IPC/POD/UTF-8 JSON。

## 边界

- 只做容器、导航和视觉一致性。
- 不保存建单数据。
- 不做加载订单规则。
- 不做扫描采集、算法重建或设备通信。
- 重资源释放仍由 MainExe 和 ScanReconstructStudio 的生命周期规则统一控制。
- 不负责把 Qt 对象跨进程传给 ScanReconstructStudio；扫描进程只接收订单 ID、上下文 JSON/文件路径和状态命令。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_OrderScanWorkspaceShell.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口
- VS2015：打开 `MeyerScan_OrderScanWorkspaceShell.sln`，构建并运行 `OrderScanWorkspaceShellTest.exe`。
- CMake/VSCode：默认开启 `OrderScanWorkspaceShellTest` 测试目标，可通过 `MEYER_BUILD_ORDERSCANWORKSPACESHELLTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
