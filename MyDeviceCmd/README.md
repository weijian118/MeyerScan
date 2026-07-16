# MyDeviceCmd

`MyDeviceCmd` 生成 `MeyerScan_DeviceCmd.dll`，是设备协议命令层。它把口扫设备 A 类命令帧、设备基础信息、状态快照、灯光控制、相机/标定参数、固件分包和采集启停封装成稳定的纯 C++/C ABI，底层通过运行时动态加载 `MeyerScan_DeviceTransport.dll` 完成 USB 收发。

当前模块代码版本为 `0.2.0`，公共语义 API 版本为 `1.1.0`，整数 ABI 版本保持为 `1`。

## 当前范围

- 已按 `美亚无线口内扫描仪通讯协议-20250808.pdf` 实现 A 类帧编码、解码、长度检查、16 位求和校验和 0~3 字节零尾检查。
- 已按协议覆盖 48 组 A 类命令：机器码、主板版本、电池、设备信息/期限、灯光、相机参数、颜色矩阵、开窗位置、温度、帧率、固件擦除/分包烧写、两路相机标定、颜色标定和曝光参数。
- 固化类命令统一检查设备的 `0xFF` 成功/`0x00` 失败状态；大块数据使用固定 POD 结构和调用方缓冲区，不跨 DLL 传递 STL/Qt 容器。
- 已实现 `MyScan 6 Wireless` 的已核对协议目录。
- `MyScan 3`、`MyScan 5`、`MyScan 5H`、`MyScan 6` 已建立独立型号条目；能力和停止顺序是初始兼容配置，`protocolVerified=0`，必须经过对应实机协议核验后才可标记为已验证。
- 已提供显式 `SimulatorForTest` 后端，用于无硬件 smoke；它不代表真实设备成功。
- 当前 `state.model` 来自打开参数 `modelHint`，`state.modelSource=HostHint`；在完成 USB VID/PID、机器码与产品型号映射前，UI 不得把它描述成设备已自动上报的型号。
- 每组命令的命令码、响应码、公共接口和验证状态见 `docs/ProtocolCommandCoverage.md`。模拟后端已覆盖全部语义命令，真实设备、Flash 写入和长时间采集仍需实机联调。

## 边界

本模块不创建 QWidget、不依赖 Qt、不访问数据库/配置文件、不解释权限、不执行校准算法、不做重建算法。`SettingsUI`、校准 UI 和扫描 UI 不得各自创建 USB 会话；由 MainExe 后续的设备会话宿主持有一个 `MeyerDeviceCmdHandle`，其他模块通过宿主提供的状态和动作接口访问设备。

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
DeviceCmdTest.exe --help
```

`--smoke` 依次覆盖型号目录、基础状态、全部协议命令组、固定长度数据转换、固化状态、固件分包、灯光、启动采集、取一帧和停止采集；它使用显式模拟后端，不连接真实 USB。真实设备联调需要使用 `DeviceTransport` 后端，并提供设备 DLL 的绝对路径。
