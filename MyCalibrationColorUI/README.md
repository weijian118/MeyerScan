# MeyerScan CalibrationColorUI

`MyCalibrationColorUI` 输出 `MeyerScan_CalibrationColorUI.dll`，用于承载颜色校准界面、流程编排和颜色校正参数生成入口。

## 当前定位

- 当前版本是 Qt Widgets 最小骨架，用于先固定模块边界和集成方式。
- 本模块可以优先使用 Qt 默认能力，包括 `QWidget`、Qt Layout、信号槽、`QString` 和 `QJsonDocument`。
- 后续颜色校准算法 DLL、DeviceCmd、DeviceTransport 接入本模块内部，不把颜色校准流程写入 MainExe 或 EngineeringSettings。
- 可见 UI 文案必须使用 `tr("English source text")`，源码不写中文 UI source text。
- 日志目录由 MainExe 或测试宿主基于安装目录传入，禁止使用当前工作目录推导运行资源。

## 边界

- 不做三维校准。
- 不做病例/订单数据维护。
- 不做云端上传或发送流程。
- 不跨进程传递 Qt 对象；需要同步状态时使用订单 ID、UTF-8 JSON 或文件路径。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CalibrationColorUI.sln /p:Configuration=Release /p:Platform=x64
```
