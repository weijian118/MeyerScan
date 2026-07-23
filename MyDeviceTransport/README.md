# MyDeviceTransport

`MyDeviceTransport` 是 MeyerScan 的设备传输模块，正式产物为 `MeyerScan_DeviceTransport.dll`，测试产物为 `DeviceTransportTest.exe`。模块版本为 `1.2.0`。

## 职责边界

本模块负责：

- Cypress CyAPI USB 设备枚举、打开、关闭和重连。
- 原始命令字节发送与响应接收。
- Bulk IN 异步流队列、超时中止和资源回收。
- 图像包同步、平面组装、帧顺序和状态解析。
- 私有 IMU 解码与相对姿态兼容输出。

本模块不负责：

- 曝光、开灯、固件等命令的业务组装与响应语义，该职责属于已拆分的 `MyDeviceCmd`。
- 扫描 UI、扫描流程、重建算法、模型显示和订单逻辑。
- 串口、WinUSB 和网络传输。未实现能力不会以空类或成功返回值占位。

## 目录

```text
MyDeviceTransport/
|-- include/                    公共 C ABI
|-- src/api/                    ABI 转换、校验和异常边界
|-- src/core/                   会话与采集编排
|-- src/model/                  DLL 内部数据结构
|-- src/processing/imu/         私有 IMU 解码和姿态处理
|-- src/protocol/               设备包固定字节定义
|-- src/support/                可选日志适配
|-- src/transport/cyapi/        CyAPI 具体传输实现
|-- test/                       无硬件 smoke 和显式硬件测试
|-- thirdparty/CyAPI/           CyAPI 头文件与 x64 导入库
|-- CMakeLists.txt              VSCode/CMake 入口
|-- MeyerScan_DeviceTransport.sln  VS2015 入口
|-- CHANGELOG.md                中文修改记录
```

## 公共接口

公共头为 `include/DeviceTransport.h`，导出函数统一使用 `MeyerDeviceTransport_*`。结构体必须先调用对应 `Init*` 函数，DLL 会校验 `structSize` 和 `schemaVersion`。尺寸、队列、超时和内存预算使用公共头中的 `MEYER_DEVICE_TRANSPORT_MAX_*` 常量约束，非法参数会在访问硬件和分配大块内存前返回 `InvalidArgument`。

`MeyerDeviceTransport_InitOpenParams` 默认写入 `MEYER_DEVICE_TRANSPORT_AUTO_DEVICE_INDEX`。CyAPI 后端会遍历全部枚举项，逐个核对 VID/PID 和 USB 速度；显式覆盖 `deviceIndex` 时只尝试指定项。只有明确的 USB2/USB3 描述符才允许打开，未知速度不按 USB3 处理。

返回值 `0` 表示成功，负数表示失败或尚未就绪。失败后可用两次缓冲区调用读取错误文本：

```cpp
std::size_t required = 0;
MeyerDeviceTransport_GetLastError(handle, nullptr, 0, &required);
std::vector<char> text(required, '\0');
MeyerDeviceTransport_GetLastError(handle, &text[0], text.size(), &required);
```

`GetFrame` 是非阻塞接口：当前无完整帧时立即返回 `NotReady`。缓冲区不足时返回所需字节数并保留同一帧，调用方扩容后可再次读取。完整帧队列默认最多保留 3 帧，消费者落后时丢弃最旧帧，避免持续占用内存。

采集参数除单项上限外还受 512 MiB 总内存预算限制。预算同时覆盖 CyAPI 在途队列和组帧/待交付帧的估算副本；所有尺寸乘法使用 64 位中间值。该限制用于拦截配置错误，不代替真实设备型号的协议参数校验。

## 构建

要求：Windows x64、VS2015 v140、Windows SDK 8.1、CMake 3.15+。CyAPI SDK 文件必须位于项目内相对目录：

```text
thirdparty/CyAPI/include/CyAPI.h
thirdparty/CyAPI/lib/x64/CyAPI.lib
```

VS2015：打开 `MeyerScan_DeviceTransport.sln`，选择 `x64` 和 `Debug` 或 `Release`。

VSCode/CMake：用 VSCode 打开本目录，安装推荐的 CMake Tools 扩展后直接运行 `CMake: Configure`，或执行 `.vscode/tasks.json` 中的 configure/build/test 任务。命令行也可执行：

```powershell
cmake -S . -B build/vs2015-x64 -G "Visual Studio 14 2015" -A x64 -DBUILD_TESTING=ON
cmake --build build/vs2015-x64 --config Release
```

工程没有开发机绝对路径。移动整个目录后，只要 VS2015/CMake 和项目内 CyAPI SDK 完整即可重新配置和编译。

## 测试

```powershell
bin\Release\DeviceTransportTest.exe --smoke
bin\Release\DeviceTransportTest.exe --info
bin\Release\DeviceTransportTest.exe --command A5 01 02
bin\Release\DeviceTransportTest.exe --stream 10 16384 8
bin\Release\DeviceTransportTest.exe --capture 1024 440 6 28 1
```

无参数和 `--smoke` 不访问硬件，适合 CTest。其余模式会连接真实设备，无设备时明确返回非零，不会伪装通过。`--command` 在发送后按旧软件时序等待 200 ms 再接收；硬件命令必须由了解当前设备协议的人员提供，测试宿主不内置自动写入命令。

当前无硬件 smoke 共 32 项，覆盖公共版本、自动枚举默认值、参数结构、错误码、异常采集尺寸/队列、末包一致性、合法配置状态推进和 IMU 数值稳定性。

## 日志和版本

模块按需从 `MeyerScan_DeviceTransport.dll` 自身目录的绝对路径加载 `MeyerScan_Logger.dll`，不依赖 current directory。连接、命令发送、流和采集启停会写关键日志；轮询未就绪不会刷屏。

- 代码版本：`ModuleInfo::Version` 和 `GetMeyerModuleVersion()` 返回 `MeyerScan_DeviceTransport v1.2.0 (2026-07-17)`。
- API 版本：`MeyerDeviceTransport_GetApiVersion()` 返回纯语义版本 `1.0.0`。
- ABI 门禁：`GetMeyerModuleApiVersion()` 返回整数 `1`，供 DeviceCmd/MainExe 动态加载前检查。
- 文件版本：`src/Version.rc` 为 `1.2.0.0`。
- 修改版本时必须同时修改 CMake project 版本、代码常量、Version.rc、README 和 CHANGELOG。

## 当前未完成

- 已在当前环境实测枚举到 1 个匹配 VID/PID 的 Cypress 设备并正确判定为 USB3；发送只读 `0xCD` 成功，但设备在 1.5 秒内未返回 `0xCE`。拔插重连、长时间流和真实组帧仍需硬件联调。
- 温度字段沿用原始帧合同，当前协议解析未提供有效温度来源。
- ScanWorkflow/扫描算法尚未调用本 DLL；接入时应通过公共 C ABI，不得包含 `src` 内部头文件。

## 2026-07-23 采集方案和设备上下文合同

本模块在新采集链路中的定位是“持续原始传输层”。每个打开/采集会话必须记录：

```text
deviceSeries（必须）
deviceProfile（必须）
deviceIdStatus（必须）
deviceId（有则记录）
deviceModel/modelCode（有则记录）
captureMode
```

本模块不解析设备编号或型号，只接收 `MyDeviceCmd`/宿主注入的上下文并写入传输日志；未知字段不能伪造为已识别值。

新方案要求：

- 一个 USB 异步传输严格对应一个 `16384` 字节 B 包。
- 当前所有适配机型暂统一使用 `queueDepth=64`，即同时预提交 64 个 USB IN 接收任务；每个任务对应一个 `16384` 字节 B 包，允许请求环跨越组六图边界。该值必须由上层 Profile 显式传入，不能依赖本模块默认值。
- 每个接收任务完成后及时重新提交，不能为了发送无回包命令停止所有 IN 接收任务。
- 原始包持续放入有界队列；队列长度、高水位、接收失败和溢出必须可诊断。
- 图像序号、数据头、单图解密、自动曝光和 RGB 减黑图属于上层采集/处理模块，不新增到本层。

本轮只同步方案文档，当前代码中的默认参数仍需后续改为 `64` 并通过各机型实机验证；在此之前不能把代码现状描述成已经落实。

当前 `StartCapture/GetFrame` 兼容接口仍保留旧组帧行为；后续 `MyCaptureService` 接入时应优先使用原始 B 包接收接口，避免在 Transport 内重复实现新的组帧和后处理流程。详细边界见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。
