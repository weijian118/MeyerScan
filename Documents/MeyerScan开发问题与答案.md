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
  -> D9 校验失败时标记生产未写号
  -> 生产状态发送 0xC2 等待 0xC7 探测系列能力
  -> 0xCD 请求/0xCE 回包读取完整型号代码
  -> DeviceProductCatalog 综合编号、型号和命令证据
  -> 型号确认后读取 0x14/0x15 主控板版本
  -> 只有 mOS MyScan 读取 0x12/0x13 投图板版本
```

设备能力 Profile、产品系列和具体销售型号是三个不同层次，不能使用同一个枚举替代。13 位设备编号前缀只能形成系列候选，完整 8 位型号代码才能区分 P1/P2/P3 等具体产品。

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
