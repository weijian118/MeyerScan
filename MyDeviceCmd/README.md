# MyDeviceCmd

`MyDeviceCmd` 生成 `MeyerScan_DeviceCmd.dll`，是设备协议命令层。它把口扫设备 A 类命令帧、设备基础信息、状态快照、灯光控制、相机/标定参数、固件分包和采集启停封装成稳定的纯 C++/C ABI，底层通过运行时动态加载 `MeyerScan_DeviceTransport.dll` 完成 USB 收发。

当前模块代码版本为 `0.3.0`，公共语义 API 版本为 `1.2.0`，整数 ABI 版本保持为 `1`。

## 当前范围

- 已按 `美亚无线口内扫描仪通讯协议-20250808.pdf` 实现 A 类帧编码、解码、长度检查、16 位求和校验和 0~3 字节零尾检查。
- 已按协议覆盖 48 组 A 类命令：机器码、主板版本、电池、设备信息/期限、灯光、相机参数、颜色矩阵、开窗位置、温度、帧率、固件擦除/分包烧写、两路相机标定、颜色标定和曝光参数。
- 固化类命令统一检查设备的 `0xFF` 成功/`0x00` 失败状态；大块数据使用固定 POD 结构和调用方缓冲区，不跨 DLL 传递 STL/Qt 容器。
- 已实现 `MyScan 6 Wireless` 的已核对协议目录。
- `MyScan 3`、`MyScan 5`、`MyScan 5H`、`MyScan 6` 已建立独立型号条目；能力和停止顺序是初始兼容配置，`protocolVerified=0`，必须经过对应实机协议核验后才可标记为已验证。
- 已提供显式 `SimulatorForTest` 后端，用于无硬件 smoke；它不代表真实设备成功。
- 颜色校准入口使用 `MeyerDeviceCmd_PrepareColorCalibration`，按“打开 Cypress 设备 -> 检查 USB3 -> `0xCD/0xCE` 读取设备信息 -> 识别明确型号标记”执行。成功时状态为 `Ready` 并保留唯一会话，失败时返回稳定预检状态并关闭会话。
- 正式协议中 `0xCE` 只有加密、加密类型、设备编号、期限码和预留区，没有独立机型字段。当前只识别预留区中的明确型号标记，不按设备编号前缀猜测；没有标记时 `state.model` 保持 `Unknown`。
- 每组命令的命令码、响应码、公共接口和验证状态见 `docs/ProtocolCommandCoverage.md`。模拟后端已覆盖全部语义命令，真实设备、Flash 写入和长时间采集仍需实机联调。
- 当前实机已通过 Cypress 枚举和 USB3 检查；`0xCD` 发送成功但 1.5 秒内未收到 `0xCE`，因此预检稳定返回 `DeviceInfoReadFailed` 并关闭会话。需要结合设备固件版本或命令通道前置初始化继续联调，不能把本次结果解释为型号未知或预检 Ready。

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

`--smoke` 依次覆盖型号目录、基础状态、全部协议命令组、固定长度数据转换、固化状态、固件分包、灯光、启动采集、取一帧、停止采集，以及颜色校准预检的未连接/USB2/型号未知/成功分支；它使用显式模拟后端，不连接真实 USB。真实设备联调需要使用 `DeviceTransport` 后端，并提供设备 DLL 的绝对路径。

`--preflight-real` 只执行颜色校准入口所需的只读链路：枚举 Cypress 设备、判断 USB2/USB3、发送 `0xCD` 并解析 `0xCE`、读取设备编号和明确机型标记。它不会调用设备信息写入、参数固化或 Flash 命令；返回码 `0` 表示预检 Ready，`3` 表示链路已执行但被未连接、USB2、设备信息读取失败或机型未知等业务门禁拦截。
