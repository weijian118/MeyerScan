# MyDeviceCmd

`MyDeviceCmd` 生成 `MeyerScan_DeviceCmd.dll`，是设备协议命令层。它把口扫设备 A 类命令帧、设备基础信息、状态快照、灯光控制、相机/标定参数、固件分包和采集启停封装成稳定的纯 C++/C ABI，底层通过运行时动态加载 `MeyerScan_DeviceTransport.dll` 完成 USB 收发。

当前模块代码版本为 `0.9.0`，公共语义 API 版本为 `2.5.0`，公共结构 schema 为 `6`，整数 ABI 版本为 `7`。

默认命令回包超时为 `200 ms`，图像流超时仍为 `1500 ms`。真实 DeviceTransport 后端只有在上一条命令没有收到普通合法帧或业务可识别终态回包时，才在下一条命令发送前等待 `20 ms`；D9/CE 的 `0xFFFF` 未初始化及已识别校验失败均表示设备已经响应，下一条命令立即发送。D4/D9、CD/CE 当前命令发送后仍等待 `50 ms` 再提交 Bulk IN。实机确认 MyScan 3 在主控板 `0x15` 返回后仍需机型特定板间切换时间才能可靠接收投图板 `0x12`，当前设置为 `20 ms`，该特例只保存在 MyScan 3 Profile，不扩大为全局命令间隔。模拟后端不人为等待。

## 当前范围

- 已按 `美亚无线口内扫描仪通讯协议-20250808.pdf` 实现 A 类帧编码、解码、长度检查、16 位求和校验和 0~3 字节零尾检查。
- 已覆盖 52 个 A 类命令码：设备编号（API 历史名 MachineCode）、主控板/投图板版本、电池、设备信息/期限、灯光、相机参数、大小扫描头颜色矩阵、开窗位置、温度、帧率、固件擦除/分包烧写、两路相机标定、颜色标定和曝光参数。
- 重构软件只支持 `mOS MyScan 5/6`。旧 `mOS MyScan` 的 D9/C7/CE、产品目录、投图板和颜色参数协议代码继续保留，但识别后立即返回 `ProductFamilyUnsupported=18`，不再执行固件、校准或采集命令。
- 受支持的 `mOS MyScan 5/6` 要求主控板 `1.3.x` 及以上，并通过 `A3/A4`、`B9/BA` 分别读取大小扫描头颜色参数状态。期望响应求和失败表示对应扫描头未校准；超时、坏帧或错误长度阻止进入校准。
- 固化类命令统一检查设备的 `0xFF` 成功/`0x00` 失败状态；大块数据使用固定 POD 结构和调用方缓冲区，不跨 DLL 传递 STL/Qt 容器。
- 已实现 `MyScan 6 Wireless` 的已核对协议能力 Profile；`MyScan 3/5/5H/6` Profile 的能力和停止顺序仍是待实机核验的兼容配置。
- 协议能力 Profile、产品系列和具体产品型号已分开建模。Profile 决定命令/采集差异；系列用于业务归类；具体产品型号用于国内、海外、贴牌、技工和医院版本识别。
- 已提供显式 `SimulatorForTest` 后端，用于无硬件 smoke；它不代表真实设备成功。
- 颜色校准入口使用 `MeyerDeviceCmd_PrepareColorCalibration`，按“打开 Cypress 设备 -> 检查 USB3 -> `0xD4/0xD9` 读取 13 位设备编号 -> 编号未写入时用 `0xC2/0xC7` 探测系列 -> `0xCD/0xCE` 读取型号代码 -> 综合识别 -> 产品系列支持门禁 -> `0x14/0x15` 读取主控板版本 -> MyScan 5/6 双扫描头状态”执行。API 中 `MachineCode` 是历史协议命名，产品含义统一为设备编号。
- `MeyerDeviceDetectionRecord` 同时保存 D9/C7/CE 步骤状态、生产模式、真实 `reported*` 值、最终 `effective*` 值和值来源。兼容默认值只能写入 effective 字段，禁止覆盖或伪装成设备上报值。
- DeviceCmd 不决定生产兼容身份能否进入业务流程：只有创建订单扫描流程由宿主要求真实设备编号，被拦截时使用稳定状态 `ProductionDeviceNumberRequired=14`；练习、颜色校准和后续三维校准可使用带来源的兼容身份。
- D9 无回包或普通坏包返回回包异常；校验通过但编号不是 13 位/`620000` 前缀返回编号异常。D9 payload 长度为 `0xFFFF` 或求和校验失败时都进入“生产未写设备编号”分支，但分别记录 `UninitializedLength` 和 `ChecksumIndicatesUnprogrammed`。
- 生产模式下 C7 无回包形成 mOS MyScan 候选，收到 C7 形成 mOS MyScan 5/6 候选。MyScan 5 与 MyScan 6 的区分规则待定，当前兼容值明确标成推断。
- CE 无回包、普通坏包、坏校验、`0xFFFF` 未初始化或型号值非法都会保留各自诊断；旧设备可以继续使用与已知系列一致的 compatibility effective 值，但产品身份状态明确为 `CompatibilityInferred`。
- 已写合法设备编号时，compatibility 型号代码按已登记前缀选择同系列标准值（62000020/27/53/55），未知前缀才回退 62000020。该安全修正避免把已知 MyScan 5/5H 设备错误套用为 MyScan 3。
- `0xCE` 存在至少两种布局：旧有线 382 字节回包的前 8 字节是设备型号代码；MyScan 6 Wireless 文档布局才包含加密状态、设备编号、期限和预留区。两种布局分支解析，禁止按同一偏移解释。
- `DeviceProductCatalog` 使用完整 8 位型号代码精确映射具体产品；设备编号前 8 位只用于确定系列候选。证据冲突返回 `Conflict`，设备编号为空/全零/无效时返回 `DeviceNumberUnprogrammed` 并继续读取型号代码。
- `SeriesOnly` 可用于选择下一组识别命令，但不足以进入依赖具体产品参数的颜色校准；只有具体产品已识别时预检才返回 Ready。
- 回包查找、长度/命令码校验、逐字节转换和机型映射全部集中在 DeviceCmd；内部可用 `std::string` 和字节容器，公共 DLL 边界只返回固定 POD、固定 UTF-8 数组和原始字节，不返回 `std::string` 或字符串数组。
- 每组命令的命令码、响应码、公共接口和验证状态见 `docs/ProtocolCommandCoverage.md`。模拟后端已覆盖全部语义命令，真实设备、Flash 写入和长时间采集仍需实机联调。

## 内部文件职责

- `DeviceCommandService.cpp`：会话打开、关闭、错误状态和 Profile 切换。
- `DeviceCommandServicePreflight.cpp`：颜色校准/工作台共用身份预检编排。
- `DeviceCommandServiceCommands.cpp`：普通语义命令。
- `DeviceCommandServiceCapture.cpp`：采集启停和帧读取。
- `DeviceCommandServiceExchange.cpp`：所有 A 类命令共用的组帧、发送、接收和校验入口。
- `DeviceCommandServiceDetection.cpp`：D9/C7/CE 和 reported/effective 身份检测。
- `DeviceCommandServiceFirmware.cpp`：设备编号快照、固件版本和双扫描头颜色状态。
- 当前实机已通过 Cypress 枚举和 USB3 检查，`0xD4/0xD9` 已成功返回设备编号 `6200002002566`，前缀为 `62000020`；随后 `0xCD` 发送成功但 1.5 秒内未收到 `0xCE`。新流程会记录 `FirmwareTooOld + CompatibilityInferred` 并使用 `62000020` 兼容有效值继续，但这不等于实机已经确认 P1，P1/P2/P3 仍需取得合法 CE 型号代码后才能精确确定。

## 当前产品目录

| 产品系列 | 具体产品型号 | 设备编号前缀 | 型号代码 | 协议 Profile |
|---|---|---|---|---|
| mOS MyScan | mOS MyScan/SY-KS1000(P1) | 62000020 | 62000020 | MyScan3 |
| mOS MyScan | mOS MyScan/SY-KS1000(P2) | 62000020 | 62010025 | MyScan3 |
| mOS MyScan | mOS MyScan/SY-KS1000(P3) | 62000020 | 62010039 | MyScan3 |
| mOS MyScan | mOS MyScan/mOS MyScan | 62000027 | 62000027 | MyScan3 |
| mOS MyScan | mOS MyScan/mOS MyScan-P1 | 62000027 | 62010036 | MyScan3 |
| mOS MyScan 5 | mOS MyScan 5/mOS MyScan 5 | 62000053 | 62000053 | MyScan5 |
| mOS MyScan 5 | mOS MyScan 5/mOS MyScan 5-P1 | 62000053 | 62010043 | MyScan5 |
| mOS MyScan 5 | mOS MyScan 5H/mOS MyScan 5H | 62000055 | 62000055 | MyScan5H |
| mOS MyScan 5 | mOS MyScan 5/mOS MyScan 5-P2 | 62000053 | 待定 | MyScan5 |
| mOS MyScan 6 | 国内有线版/国内无线版 | 待定 | 待定 | MyScan6/MyScan6Wireless |

## 边界

本模块不创建 QWidget、不依赖 Qt、不访问数据库/配置文件、不解释权限、不执行校准算法、不做重建算法。`SettingsUI`、校准 UI 和扫描 UI 不得各自创建 USB 会话；当前 MainExe 的 `DeviceSessionHost` 持有一个 `MeyerDeviceCmdHandle`，其他模块只通过宿主动作和 POD 快照访问设备。

## 真实后端加载

正式后端打开时必须把 `MeyerScan_DeviceTransport.dll` 的绝对 UTF-8 路径写入 `transportLibraryPathUtf8`。路径应由 `QCoreApplication::applicationDirPath()` 或 Windows 应用目录接口组成，不能使用 `QDir::currentPath()`、进程 current directory 或开发机绝对路径。

## 编译

```powershell
cmake --preset vs2015-x64
cmake --build --preset vs2015-x64-release
ctest --preset vs2015-x64-release
```

VS2015 工程为 `MeyerScan_DeviceCmd.sln`。CMake 和 VS2015 不要并行写入同一个 `bin\Release`。

## 测试

```powershell
DeviceCmdTest.exe --smoke
DeviceCmdTest.exe --probe-real "F:\MeyerScan\bin\Release\MeyerScan_DeviceTransport.dll"
DeviceCmdTest.exe --preflight-real "F:\MeyerScan\bin\Release\MeyerScan_DeviceTransport.dll"
DeviceCmdTest.exe --help
```

`--smoke` 覆盖 8 个已知型号代码精确映射、D9 无回包/坏包/非法值/`0xFFFF` 未初始化/求和校验失败、生产模式 C7 两类候选、CE 精确/旧固件/未初始化/坏校验/坏包/非法值、证据冲突、基础状态、全部协议命令组和采集。它使用显式模拟后端，不连接真实 USB。

`--preflight-real` 只执行颜色校准入口所需的只读链路：枚举 Cypress 设备、判断 USB2/USB3、发送 D4/D9、生产模式下发送 C2/C7、发送 CD/CE、读取主控板/投图板版本，并逐项输出回包缺失、帧解析、求和校验、reported/effective 身份和兼容来源。它不会调用设备信息写入、参数固化或 Flash 命令；返回码 `0` 表示可以继续（可能带兼容推断），`3` 表示被未连接、USB2、D9 回包异常、编号非法、身份冲突或 Profile 未知等门禁拦截。

真实预检测试还会输出 `[TIMING]`：Open、USB、D4-D9、C2-C7、CD-CE、ProductCatalog、Firmware 和 Total。每条包含步骤用途与实际毫秒数；未需要的 C2-C7 显示 `SKIPPED` 及原因。该输出由测试宿主临时开启，正式 MainExe 默认不打印。

## 2026-07-23 采集命令与设备上下文合同

每个命令会话和采集会话必须保存以下设备信息：

```text
deviceSeries（必须）
deviceProfile（必须）
deviceIdStatus（必须）
deviceId（有则记录）
deviceModel/modelCode（有则记录）
reported/effective 来源
productionMode
firmwareVersion
captureMode
scanHeadType（有则记录）
```

`MyDeviceCmd` 是设备命令唯一语义入口：

- 负责 `0x0A/0x0B`、曝光、灯光等命令编码和发送。
- 负责识别设备系列、设备编号、设备型号和固件；保存真实回包值与兼容有效值的区别。
- 负责为上层采集服务提供受控的原始 B 包接收和无回包命令调度能力。
- 采集间隔内最多两条无回包命令；第一条 USB OUT 完成后至少等待 5 ms 再发送第二条。
- 自动曝光是否进入由整组开灯状态决定；关灯组跳过自动曝光，但仍允许发送其他合法无回包命令。
- 当前所有适配机型的 Capture Profile 暂统一传入 `queueDepth=64`；DeviceCmd 负责把该显式参数传给 DeviceTransport，不能依赖传输模块默认值。关灯组仍由上层采集服务正常后处理并发布结果。

`MyDeviceCmd` 不解析图像像素、不执行排序、镜像、减黑图或自动曝光算法。自动曝光由独立采集会话对象调用 `MeyerScan_AutoExposure.dll` 计算，命令仍由本模块发送。详细数据流见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。
