# MeyerScan Calibration3DUI

`MyCalibration3DUI` 输出 `MeyerScan_Calibration3DUI.dll`，用于承载三维校准界面、流程编排和三维校准计算入口。

## 当前定位

- 当前版本是 Qt Widgets 最小骨架，用于先固定模块边界和集成方式。
- 本模块是界面模块，可以使用 Qt Widgets、Qt Layout、信号槽和 `QString`；算法、设备重资源和跨进程状态同步仍通过清晰接口隔离，不把 Qt UI 对象传到模块边界外。
- 后续三维校准算法 DLL、DeviceCmd、DeviceTransport 接入本模块内部，不把校准流程写入 MainExe 或 EngineeringSettings。
- 可见 UI 文案必须使用 `tr("English source text")`，源码不写中文 UI source text。
- 日志目录由 MainExe 或测试宿主基于安装目录传入，禁止使用当前工作目录推导运行资源。

## 边界

- 不做颜色校准。
- 不做病例/订单数据维护。
- 不做云端上传或发送流程。
- 不跨进程传递 Qt 对象；需要同步状态时使用订单 ID、UTF-8 JSON 或文件路径。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_Calibration3DUI.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口
- VS2015：打开 `MeyerScan_Calibration3DUI.sln`，构建并运行 `Calibration3DUITest.exe`。
- CMake/VSCode：默认开启 `Calibration3DUITest` 测试目标，可通过 `MEYER_BUILD_CALIBRATION3DUITEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。

## 2026-07-23 设备采集上下文

三维校准后续接入 `MyCaptureService`，本 UI 不直接创建 USB 会话、不读取原始 B 包、不解析设备命令回包。进入三维采集前，宿主必须注入并记录：

```text
deviceSeries（必须）
deviceProfile（必须）
deviceIdStatus（必须）
deviceId（有则记录）
deviceModel/modelCode（有则记录）
productionMode
firmwareVersion
captureMode=Calibration3D
scanHeadType
```

设备编号可以为空，但系列、USB3、型号能力和 MyScan 5/5H 版本门禁必须由宿主完成。采集、解密、自动曝光和图像预处理由设备链路模块负责，UI 只显示结果和上报动作。详细方案见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。

三维校准后续接入标定器连接检查；当前入口不检查“三维校准是否已经完成”，该状态后续作为校准结果记录或业务查询接入，不能作为进入三维校准的门禁。
