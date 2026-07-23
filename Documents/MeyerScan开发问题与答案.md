# MeyerScan 开发问题与答案

本文集中记录开发过程中反复确认的架构、设备链路、数据保存、资源加载和运行方式。
它面向人工维护者，不替代各模块接口头文件、协议覆盖表和 CHANGELOG。

## 1. 设备 DLL 如何分工

- `MeyerScan_DeviceTransport.dll`：枚举 Cypress USB、打开/关闭设备、判断 USB2/USB3、发送原始字节、接收命令回包和图像流。它不理解 `0xD4`、`0xCE` 的业务含义。
- `MeyerScan_DeviceCmd.dll`：编码协议命令、校验帧、解析回包、识别设备身份、读取版本和编排采集。它通过绝对路径动态加载 Transport，不依赖 Qt。

`DeviceSessionHost` 不是 DLL，而是 MeyerScan.exe 内部唯一的设备会话所有者。SettingsUI、CalibrationColorUI、ScanWorkflowUI 都不能各自创建 DeviceCmd/Transport 句柄。

## 2. 设备型号判断顺序

```text
连接/USB3
  -> 0xD4 请求/0xD9 回包读取 13 位设备编号
  -> D9 长度 0xFFFF 或求和校验失败时标记生产未写号
  -> 生产状态发送 0xC2 等待 0xC7 探测系列能力
  -> 0xCD 请求/0xCE 回包读取完整型号代码
  -> DeviceProductCatalog 综合编号、型号和命令证据
  -> 型号确认后读取 0x14/0x15 主控板版本
  -> 只有 mOS MyScan 读取 0x12/0x13 投图板版本
```

设备能力 Profile、产品系列和具体销售型号是三个不同层次，不能使用同一个枚举替代。13 位设备编号前缀只能形成系列候选，完整 8 位型号代码才能区分 P1/P2/P3 等具体产品。

`0xD9` 两种生产回包必须区分：长度字段 `0xFFFF` 记录为 `UninitializedLength`，求和校验失败记录为 `ChecksumIndicatesUnprogrammed`。两者都设置 `isProductionMode=1` 并继续探测，但 reported 设备编号保持为空。错误帧头、截断、异常尾部或非 D9 回包仍是通信异常，不得判为生产模式。

## 3. 颜色校准如何取得设备编号和版本

```text
SettingsUI
  -> MainExe::HandleCalibrationPreflight
  -> DeviceSessionHost::PrepareColorCalibration
  -> DeviceCmd/Transport 执行一次完整预检
  -> MainExe 复制 MeyerDeviceCalibrationPreflight
  -> SettingsUI 复制 SettingsCalibrationDeviceContext
  -> CalibrationColorUI::SetDeviceContext
  -> CalibrationColorUI 按值保存 m_deviceContext
```

颜色校准读取：

```cpp
m_deviceContext.detection.reportedDeviceNumberUtf8;
m_deviceContext.detection.effectiveDeviceNumberUtf8;
m_deviceContext.firmwareVersions.mainBoardVersionUtf8;
m_deviceContext.firmwareVersions.projectionBoardVersionUtf8;
```

`reportedDeviceNumberUtf8` 是设备真实回包值；`effectiveDeviceNumberUtf8` 是当前流程允许使用的值，生产未写号时可能是兼容默认值。两者必须结合 `deviceNumberSource`、`isProductionMode` 和 `usedCompatibilityDefaults` 解释。

MyScan 5/5H/6 还要读取 `scanHeadColorCalibration`：`policy=LargeAndSmall` 表示大、小扫描头分别校准，`largeHeadStatus` 和 `smallHeadStatus` 分别表示已校准、未校准或读取失败；MyScan 使用 `LargeOnlyShared`，小扫描头状态为 `NotRequired`。A4/BA 收到期望响应但求和失败记录为 `NotCalibrated`，不能和超时/坏帧混淆。颜色校准 UI 从 `CalibrationColorDeviceContext.scanHeadColorCalibration` 获取这些信息，不重新访问设备。

## 4. 设备信息保存在哪里

1. 设备本身保存编号、型号、期限和授权原始数据。
2. DeviceCmd 的 `m_state` 保存当前句柄的状态快照。
3. DeviceSessionHost 的 `m_snapshot` 和 `m_lastPreflight` 保存 MeyerScan 进程当前会话副本。
4. SettingsUI 和 CalibrationColorUI 保存自己的固定 POD 副本，页面释放后失效。
5. 创建/练习工作台把设备身份、版本和来源写入 `deviceIdentity` JSON，供 Scan/Process/Send 共享。
6. `device_info_tbl2` 是本地业务数据库的设备信息来源，由 RuntimeDataCenter 读取；当前设备预检结果不会自动写入该表。

如果以后需要保存“最近一次检测结果”或“校准记录”，应由 DeviceInfoService/RuntimeDataCenter 显式写数据库，不能让 DeviceCmd 或 UI 直接连接数据库。

## 5. 生产模式准入

```json
{
    "device": {
        "practiceAllowProductionMode": true,
        "orderCreateAllowProductionMode": false
    }
}
```

练习默认允许带来源的兼容身份；创建默认必须取得真实设备编号。颜色校准和三维校准不读取这两个工作台开关，当前允许生产设备，但仍必须通过连接、USB3、型号、证据冲突和版本读取检查。

## 6. C ABI 和 ExecuteCommand

DeviceCmd 对外使用 `extern "C"`、固定布局 POD 和不透明 `MeyerDeviceCmdHandle`，避免 `QString`、`std::string`、QObject 和 USB 内部对象跨 DLL。

`ExecuteCommand` 是 A 类命令的唯一通道：检查设备和机型能力、组装 `5A 33 + 命令码 + 长度 + payload + 校验和`、调用 Transport 发送、接收回包、检查帧头/长度/尾部/校验和/期望响应码。它只返回通用 `CommandFrame`，`ReadMachineCode` 等上层函数再解释 payload 的业务含义。公共 API 层还通过每句柄 mutex 串行调用。

当前低频命令默认回包超时为 `200 ms`。上一条命令未收到并解析出期望回包时，下一条命令发送前等待 `20 ms`；上一条命令已收到期望命令码，并解析为普通合法帧或业务可识别的终态回包时，立即发送下一条。D9/CE 的 `0xFFFF` 未初始化及已识别校验失败都表示设备已经响应，不再补固定间隔。实机确认 MyScan 3 的主控板到投图板版本读取需要机型特定的 `20 ms` 板间切换等待，该等待只由 MyScan 3 Profile 提供。D4/D9、CD/CE 请求发送后仍等待 `50 ms` 再读回包，这是当前命令的接收时序，不是 A/B 两条命令之间的固定间隔；主控板/投图板版本命令发送后立即提交阻塞式 Bulk IN。图像流仍使用独立的 `1500 ms` 默认超时。

`--preflight-real` 的 `[COMMAND_TIMING]` 把一条命令拆成 `preSendWaitUs`、`profileSettleWaitUs`、`sendUs`、`postSendWaitUs`、`receiveUs`、`frameParseUs` 和 `exchangeTotalUs`。`preSendWaitUs` 只表示上一条命令尚未得到普通合法帧或业务可识别终态时的 `20 ms` 兜底等待；`profileSettleWaitUs` 表示机型 Profile 为硬件板间切换增加的等待；`postSendWaitUs` 表示 D4/D9、CD/CE 当前命令发送后、开始接收前的协议时序等待。`[SEMANTIC_TIMING]` 是业务字段解析时间。`FirmwareTotal` 和 `Total` 带 `type=AGGREGATE`，只是子步骤合计，不是额外命令，不得与子步骤重复相加。

## 7. UI 资源编译与使用

各模块只维护自己的 `Resources` 目录。构建时 `GenerateResourceManifest.ps1` 扫描所有模块资源，生成 qrc；Qt `rcc -binary` 生成 `.rcc`；Windows `Version.rc` 把 `.rcc` 作为 RCDATA 嵌入 `MeyerScan_UIResources.dll`。

运行时第一个 UI 模块通过 `MeyerQtModule::ModuleResourceFile()` 从 MeyerScan.exe 同级目录加载资源 DLL，调用 `MeyerScanInitializeUiResources()` 注册 Qt 资源树。正式路径为：

```text
:/MeyerScan/Modules/<模块名>/icon/...
:/MeyerScan/Modules/<模块名>/qss/...
```

正式安装包只需要资源 DLL，不需要散落 PNG/QSS。单模块开发时公共加载器可以回退到源码 `Resources`，但完整集成测试必须使用同批次资源 DLL。

### 7.1 资源 DLL 能否单独覆盖旧版客户

可以，但资源 DLL 是完整资源包，不是只包含几张图片的增量文件。最稳妥的方式是从客户原交付版本的 Git tag/worktree 构建，只替换相同 alias 的图标，并保留该版本使用的全部旧资源路径。项目后来仅新增资源时不会影响旧代码；新增路径也不会自动生效，因为旧 EXE/DLL 不会引用它。

覆盖前必须关闭 MeyerScan，并保持 `MeyerScan_UIResources.dll` 文件名、Qt 5.6.3、MSVC2015、Release x64、既有导出函数和资源路径合同不变。完成后运行 `UIResourcesTest.exe`、客户原版 `MeyerScan.exe --smoke-main` 和关键页面截图回归。

### 7.2 RCDATA 101 与导出函数如何匹配

`101 RCDATA` 是 DLL 内嵌 `.rcc` 的 Win32 资源编号，不是导出函数序号。资源合同集中在 `Common/include/MeyerUiResourceContract.h`：API 版本 `1`、RCDATA 编号 `101`、清单 schema `1`、qrc 前缀 `/MeyerScan/Modules`。`Version.rc`、C++ 注册代码和 qrc 生成脚本都引用该合同。

资源 DLL 继续保留旧版使用的初始化、状态、注销和模块版本函数，并追加 API、RCDATA、清单版本和路径前缀查询函数。新加载器会在注册前校验这些值；旧客户程序不调用新增函数，仍可加载保持旧导出函数的新版资源 DLL。

## 8. minMain 的当前含义

历史沟通中的 `minMain` 现在统一指：

```text
MeyerScan.exe --smoke-main
```

它不是独立解决方案或 EXE。该模式跳过登录和真实 USB，只验证 MainExe、资源 DLL、Home/Case/Settings/Workspace 及页面切换链路；模拟成功不能当作真实设备联调成功。

## 9. 修改规则

- 修改公共 POD 时同步升级 schema、API/版本号、所有使用方、静态尺寸断言和测试。
- 设备命令实现、模拟验证、真实设备验证分开记录。
- 真实设备值和兼容默认值必须分字段保存，不能只保留最终显示值。
- 任何 UI 模块不得重复连接设备；设备所有权、关闭顺序和跨页面快照由宿主管理。

## 2026-07-23：数据采集和原始图像预处理方案确认

### 问题：采集数据是否直接交给颜色校准 UI 处理？

不直接交给 UI。`MyDeviceTransport` 持续取走原始 B 包，`MyCaptureService` 负责快速链路和命令时序，`CaptureProcessing` 负责数据处理，UI 只接收最终结果。这样颜色校准、三维校准、练习扫描和创建扫描共享同一套采集逻辑，UI 不重复创建线程和 USB 会话。

### 问题：自动曝光是否每组图都执行？

不是。六张单图的开灯状态使用逻辑 AND 汇总：只要一张关灯，整组视为关灯，跳过自动曝光和自动曝光命令；但关灯组仍复制到慢速后处理队列，正常执行排序、镜像、减黑图并向 UI/算法发布结果，也仍允许发送队列中的其他合法无回包命令。六张都开灯时，使用同一个采集会话级 `AutoExposureSession` 计算参数。

### 问题：为什么单图要立即解密？

MyScan 5/5H/暂定 MyScan 6 的每张单图完成后即可解密，能够利用两张单图之间的时间。六张图完成时，解密工作已基本完成，组间 25~30 ms 的最短间隔主要用于状态汇总、自动曝光和命令发送。

### 问题：采集期间如何发送命令？

一组六图间隔内最多发送两条无回包命令。第一条 USB OUT 传输完成后至少等待 5 ms，再发送第二条。自动曝光符合条件时优先；自动曝光被跳过时仍允许发送开灯/关灯等命令。当前“发送成功”只表示 USB OUT 完成，不等同于设备内部已应用参数。这里的 5 ms 只属于采集命令窗口；低频设备信息命令在没有可识别终态时的 20 ms 兜底等待是另一条链路的规则，不能混用。

### 问题：慢速图像预处理会不会堵塞采集？

不会。快速链路完成解密、状态判断和条件式自动曝光后，复制一份已解密六图放入有界后处理队列；即使整组关灯跳过自动曝光，也要正常复制和后处理。排序、相机 1 Y 轴镜像和 RGB 减黑图由后处理线程异步完成，减法使用 `clamp(int(white) - int(black), 0, 255)`，下限为 0、上限为 255。队列满时丢弃新的后处理副本，保留已排队数据。

### 问题：`queueDepth` 使用多少？

当前所有适配机型暂统一使用 `queueDepth=64`。它表示同时预提交的 USB IN 接收任务数量，每个任务对应一个 16384 字节 B 包，不是图像组数量，也不是 `RawPacketQueue` 的长度。请求环允许跨越组六图边界；每个任务完成后要及时重新提交，避免设备 USB 缓冲区被占满。后续如果某个机型需要不同值，必须只修改对应 Profile 并增加实机验证，不能依赖传输模块的隐式默认值。

### 问题：颜色校准和三维校准的准入检查是否相同？

不相同。颜色校准当前不检查标定器连接；三维校准才预留标定器连接检查，后续接入。当前三维校准也不判断“三维校准是否已经完成”来阻止进入，完成状态后续作为结果记录或业务查询接入。

### 问题：MyScan 6 是否和 MyScan 5 共用参数变量？

不共用变量。MyScan 6 当前只复制 MyScan 5 25 帧模式的参数值，使用独立 Profile 和独立版本记录，后续可以独立修改。

### 设备上下文记录要求

设备命令、采集服务、解密/预处理、自动曝光和所有消费采集结果的 UI 都必须记录 `deviceSeries` 和 `deviceProfile`；设备编号有则记录，设备型号/型号代码有则记录，同时保留生产模式、固件、采集模式、扫描头和 reported/effective 来源。

完整方案和流程图见 `设备相关/数据采集-原始图像预处理方案.md`。
