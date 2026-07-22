# 设备协议命令覆盖表

`MyDeviceCmd 0.8.0` 已为 48 个公共 A 类命令和项目扩展的 `0x12/0x13`、`0xB9/0xBA`
共 52 个命令码提供语义接口或对应的响应解析路径。完整帧结构、命令分组、机型适用范围、
十六进制示例和 B 类图像包定义见 `F:\MeyerScan\Documents\设备相关\口扫设备协议文档.md`；
本文只维护代码覆盖和验证状态。

状态含义：

- **已实现**：命令编码、固定长度校验、响应码校验和公共结构转换已经完成。
- **模拟通过**：`DeviceCmdTest --smoke` 已通过确定性模拟后端验证。
- **待实机**：尚未连接 MyScan 6 Wireless 验证设备时序、Flash 写入和长时间稳定性。

## 控制与基础信息

| 命令 | 响应 | 语义接口 | 当前状态 |
|---|---|---|---|
| `0x0A` 开始传图 | 无 | `MeyerDeviceCmd_StartCapture` | 已实现、模拟通过、待实机 |
| `0x0B` 停止传图 | 无 | `MeyerDeviceCmd_StopCapture` | 已实现、模拟通过、待实机 |
| `0xFF` 控制器复位 | 无 | `MeyerDeviceCmd_ResetController` | 已实现、模拟通过、待实机 |
| `0x0D` 固化机器码 | `0x1D` | `MeyerDeviceCmd_StoreMachineCode` | 已实现、模拟通过、待实机 |
| `0xD4` 读取设备编号（API 历史名 MachineCode） | `0xD9` | `MeyerDeviceCmd_ReadMachineCode`；`MeyerDeviceCmd_RefreshBasicState` | 已实现、模拟通过、实机已返回 13 位编号 |
| `0x0E` 开关灯 | 无 | `MeyerDeviceCmd_SetLight` | 已实现、模拟通过、待实机 |
| `0x0C` 强制开灯 | 无 | `MeyerDeviceCmd_SetForceLight` | 已实现、模拟通过、待实机 |
| `0x14` 读取主板版本 | `0x15` | `MeyerDeviceCmd_RefreshBasicState` | 已实现、模拟通过、待实机 |
| `0x12` 读取投图板版本（仅 mOS MyScan） | `0x13` | `MeyerDeviceCmd_RefreshBasicState`；`MeyerDeviceCmd_PrepareColorCalibration` | 已实现、模拟通过、待实机 |
| `0x1A` 读取电池状态 | `0x1C` | `MeyerDeviceCmd_RefreshBasicState` | 已实现、模拟通过、待实机 |

## 相机与采集参数

| 命令 | 响应 | 语义接口 | 当前状态 |
|---|---|---|---|
| `0xA0` 读取相机参数 | `0xA1` | `MeyerDeviceCmd_ReadCameraParameters` | 已实现、模拟通过、待实机 |
| `0xA8` 固化相机参数 | `0xA9` | `MeyerDeviceCmd_StoreCameraParameters` | 已实现、模拟通过、待实机 |
| `0xA5` 在线设置开窗位置 | 无 | `MeyerDeviceCmd_SetCameraWindowPosition` | 已实现、模拟通过、待实机 |
| `0xAA` 读取温度原始值 | `0xAB` | `MeyerDeviceCmd_ReadTemperature` | 已实现、模拟通过、待实机 |
| `0xAD` 设置帧率 | 无 | `MeyerDeviceCmd_SetFrameRate` | 已实现、模拟通过、待实机 |
| `0xDB` 在线设置曝光 | 无 | `MeyerDeviceCmd_SetExposureParameters` | 已实现、模拟通过、待实机 |
| `0xDC` 读取曝光 | `0xDE` | `MeyerDeviceCmd_ReadExposureParameters` | 已实现、模拟通过、待实机 |

温度命令返回热敏电阻采集电压，单位为 mV；设备层不擅自换算摄氏温度。
曝光和相机参数中的单字节小数保留协议原始编码，业务 UI 不直接解释原始字节。

## 标定参数

| 命令 | 响应 | 语义接口 | 当前状态 |
|---|---|---|---|
| `0xA3` 读取大扫描头 416 字节颜色矩阵 | `0xA4` | `MeyerDeviceCmd_ReadColorMatrix`；`MeyerDeviceCmd_PrepareColorCalibration` | 已实现、模拟通过、MyScan 5 实机通过 |
| `0xB9` 读取小扫描头颜色矩阵 | `0xBA` | `MeyerDeviceCmd_PrepareColorCalibration` | 已实现、模拟通过、MyScan 5 实机通过 |
| `0xA7` 固化颜色矩阵 | `0xAE` | `MeyerDeviceCmd_StoreColorMatrix` | 已实现、模拟通过、待实机 |
| `0xC2` 读取相机 1 标定 | `0xC7` | `MeyerDeviceCmd_ReadCamera1Calibration` | 已实现、模拟通过、待实机 |
| `0xC3` 固化相机 1 标定 | `0xC5` | `MeyerDeviceCmd_StoreCamera1Calibration` | 已实现、模拟通过、待实机 |
| `0xD2` 读取相机 2 标定 | `0xD7` | `MeyerDeviceCmd_ReadCamera2Calibration` | 已实现、模拟通过、待实机 |
| `0xD0` 固化相机 2 标定 | `0xD5` | `MeyerDeviceCmd_StoreCamera2Calibration` | 已实现、模拟通过、待实机 |
| `0xD3` 读取 72 字节颜色标定 | `0xD8` | `MeyerDeviceCmd_ReadColorCalibration` | 已实现、模拟通过、待实机 |
| `0xD1` 固化颜色标定 | `0xD6` | `MeyerDeviceCmd_StoreColorCalibration` | 已实现、模拟通过、待实机 |

标定数组以调用方管理的固定 POD 缓冲区跨 DLL 边界，不传递 `std::vector`、
Qt 容器或算法对象。标定算法和 UI 仍属于 Calibration3DUI/CalibrationColorUI，
DeviceCmd 只负责协议读写。

颜色校准入口对 `mOS MyScan 5/6` 先校验主控板版本：`1.1.x`、`1.2.x`
不支持小扫描头颜色校准，必须升级后再进入；无法解析的版本同样保守拦截。
版本满足后依次读取 A3/A4 和 B9/BA。收到期望响应码但求和校验失败表示对应
扫描头参数未写入，记录为 `NotCalibrated`；无回包、错误帧头、错误响应码、
截断或错误 payload 长度属于读取失败，不能误报为“未校准”。`mOS MyScan`
只使用大扫描头参数，小扫描头共享该参数，不发送 B9/BA。

## 设备授权信息

| 命令 | 响应 | 语义接口 | 当前状态 |
|---|---|---|---|
| `0xCD` 读取设备信息 | `0xCE` | `MeyerDeviceCmd_ReadDeviceInfo`；`MeyerDeviceCmd_PrepareColorCalibration` | 已实现、校准预检模拟通过、待实机 |
| `0xC9` 固化设备信息 | `0xCB` | `MeyerDeviceCmd_StoreDeviceInfo` | 已实现、模拟通过、待实机 |

颜色校准型号检测固定顺序为 D4/D9、必要时 C2/C7、CD/CE、身份确认后的 0x14/0x15，
mOS MyScan 再执行 0x12/0x13。详细解析会区分
无回包、普通坏包、求和校验失败、`0xFFFF` 未初始化和业务值非法；兼容默认值
只写入 `MeyerDeviceDetectionRecord.effective*`，真实 `reported*` 字段保持原样。

`0xCE` 不能全局按一种结构解释：旧 Cypress 有线软件实例返回 382 字节，前 8 字节
逐位组成设备型号代码；MyScan 6 Wireless 文档布局才包含加密标志、加密类型、
13 位设备编号、30 字节期限码和 337 字节预留区。`DeviceCmd` 根据协议 Profile
选择解析器，并在 `responseLayout` 中记录分支。

型号代码必须完整精确匹配 `DeviceProductCatalog`。设备编号前缀只确定产品系列候选；
前缀和代码冲突时返回 `Conflict`，未写设备编号时继续使用型号代码识别，UI 不解析原始回包。

## 主板固件升级

| 命令 | 响应 | 语义接口 | 当前状态 |
|---|---|---|---|
| `0xB6` 擦除主板固件 | `0xB7` | `MeyerDeviceCmd_EraseFirmware` | 已实现单次进度响应、模拟通过、待实机 |
| `0xB4` 烧写 256 字节分包 | `0xB5` | `MeyerDeviceCmd_WriteFirmwarePacket` | 已实现包序核对、模拟通过、待实机 |

DeviceCmd 只提供安全的擦除和分包烧写原语，不读取固件文件、不决定升级版本、
不处理断点续传，也不负责整体升级 UI。完整升级流程由后续固件升级宿主按
“校验文件 -> 擦除 -> 分包写入 -> 核对应答 -> 复位 -> 重连 -> 版本复核”编排。

## 通用原始命令

`MeyerDeviceCmd_ExecuteRawCommand` 保留用于协议诊断和未来新增命令。已登记命令仍受
型号能力门禁、A 类帧校验和采集中响应互斥约束，不能借原始接口绕过这些保护。
