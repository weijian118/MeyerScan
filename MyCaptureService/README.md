# MeyerScan_CaptureService

`MeyerScan_CaptureService.dll` 是颜色校准、三维校准、练习扫描和创建扫描共用的
采集编排层。DLL 本身是纯 C++；当前第一版先打通颜色校准采集链路，自动曝光只
加载占位 DLL，暂不计算或下发真实曝光参数。Qt 只用于测试程序。

## 线程职责

- 快速线程：轻量连接检查、原始 B 包接收、单图组装/解密、组六图状态汇总、自动
  曝光占位调用和组六图边界无回包命令窗口。
- 慢线程：消费已深复制的解密组六图，调用 CaptureProcessing 完成协议级标准化，
  再调用 CaptureImagePipeline 生成 RGB888、重建六图和后续场景输出。
- UI 线程：只轮询结构化快照、事件和图像副本，不接触 DeviceCmd 或 USB 句柄。

## 动态依赖

运行目录需要提供：

- `MeyerScan_DeviceCmd.dll`
- `MeyerScan_CaptureProcessing.dll`
- `MeyerScan_CaptureImagePipeline.dll`
- `MeyerScan_AutoExposure.dll`
- `MeyerScan_Logger.dll`（可选，缺失时不影响采集）

路径为空时从 `MeyerScan_CaptureService.dll` 所在目录推导，禁止使用
`QDir::currentPath()` 或进程当前目录。

## 异常策略

单次超时、部分包、错序和坏头会丢弃当前不完整组六图并等待新的 0 号图；连续两次
超时、设备拔出和底层 I/O 失败会进入 `Faulted`。所有异常都通过 `PollEvent` 上报，
由上层 UI 使用 `tr("English source")` 选择提示文案。

## 构建与测试

- VS2015：打开 `MeyerScan_CaptureService.sln`，选择 `Release|x64`。
- VSCode/CMake：使用 `vs2015-x64` 配置和构建预设。
- 离线回归：依次运行 `--smoke`、`--smoke-timeout-once`、
  `--smoke-timeout-always`、`--smoke-disconnect` 和 `--smoke-partial`。

单模块构建测试前需先生成同仓库的下层 DLL。完整构建优先使用根目录
`MeyerScan_AllModules.sln`。真实设备测试必须在设备连接后由人工明确启动。
