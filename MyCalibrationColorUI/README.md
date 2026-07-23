# MeyerScan CalibrationColorUI

`MyCalibrationColorUI` 输出 `MeyerScan_CalibrationColorUI.dll`，用于承载颜色校准界面、流程编排和颜色校正参数生成入口。

当前版本为 `0.8.0`，公共虚接口 ABI 为 7，设备上下文 schema 为 6。SettingsUI 必须在 `CreateWidget` 前调用 `SetDeviceContext` 注入已经通过 MainExe/DeviceCmd 校验的 USB3 设备快照；快照包含协议 Profile、产品身份、D9/C7/CE 检测记录、主控板/投图板版本，以及大小扫描头颜色校准策略和状态。颜色校准 UI 不自行解析回包、加载 DeviceCmd/DeviceTransport 或创建第二个 USB 会话。

## 当前定位

- 当前版本已按参考软件复原颜色校准弹窗：自定义标题栏、normal/hover 关闭按钮、400x400 初始相机预览、Calibrate 和 Exit 操作区。
- 本模块是界面模块，可以使用 Qt Widgets、Qt Layout、信号槽和 `QString`；算法、设备重资源和跨进程状态同步仍通过清晰接口隔离，不把 Qt UI 对象传到模块边界外。
- `init_image.png`、`close_b.png`、`close_h.png` 维护在 `Resources/icon/color_calibration`，根构建会统一编译进 `MeyerScan_UIResources.dll`。
- Calibrate/Exit 使用 `UIComponents` 主按钮工厂，视觉角色与浏览模块 Search 按钮一致；UIComponents 不可用时使用本地 Qt 按钮降级，业务入口仍可显示。
- 后续 Calibrate 点击只向宿主提交颜色校准动作，并调用本模块拥有的颜色算法适配层；DeviceCmd/DeviceTransport 句柄仍由进程级设备会话宿主持有，不下沉到 UI。
- 可见 UI 文案必须使用 `tr("English source text")`，源码不写中文 UI source text。
- 日志目录由 MainExe 或测试宿主基于安装目录传入，禁止使用当前工作目录推导运行资源。
- `mOS MyScan` 使用 `LargeOnlyShared` 策略，只校准大扫描头并让小扫描头共用参数；`mOS MyScan 5/6` 使用 `LargeAndSmall` 策略，后续实际校准流程必须分别完成两种扫描头。

## 界面约束

- 1920x1080 参考面板约为 `450x585`，预览区为 `400x400`；不包含参考截图外围的系统阴影尺寸。
- 使用 Qt Layout、方形预览控件、最小/最大尺寸和多语言自然宽度，不使用绝对坐标，也不把所有控件整体乘分辨率系数。
- 独立测试时根页面使用无边框 Dialog；SettingsUI 中由全窗口半透明遮罩承载并居中显示。
- 鼠标按住自定义标题栏可以拖动颜色校准面板；独立运行时移动测试窗口，设置弹窗中只移动面板并限制在遮罩范围内。
- 顶部关闭和 Exit 都关闭当前合法校准宿主，不允许误关闭 MeyerScan 主窗口；Calibrate 当前记录操作日志，设备取图和颜色计算尚未接入。

## 边界

- 不做三维校准。
- 不做病例/订单数据维护。
- 不做云端上传或发送流程。
- 不解析 D9、C7、CE 原始回包；DeviceCmd 完成解析后，本模块只保存固定 POD 副本。`reported*` 表示真实设备回包，`effective*` 表示颜色校准当前应使用的值，兼容值必须由来源和状态明确标记。
- 不跨进程传递 Qt 对象；需要同步状态时使用订单 ID、UTF-8 JSON 或文件路径。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CalibrationColorUI.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口
- VS2015：打开 `MeyerScan_CalibrationColorUI.sln`，构建并运行 `CalibrationColorUITest.exe`；无参数运行默认显示颜色校准界面，且不显示额外 CMD 窗口。
- CMake/VSCode：默认开启 `CalibrationColorUITest` 测试目标，可通过 `MEYER_BUILD_CALIBRATIONCOLORUITEST` 控制。
- 自动化回归必须传入 `--smoke`，此模式只验证 DLL 工厂、初始化、根控件和释放链路，然后用退出码返回结果。
- `--drag-test` 会模拟标题栏按下/移动/释放事件，验证独立窗口位置确实发生变化。
- `--capture-screenshot <png>` 会按参考面板尺寸渲染、抓图并自动退出，用于逐像素视觉比较。
- `--show` 作为旧人工启动参数继续兼容，其行为与无参数运行一致。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。

## 2026-07-23 采集上下文和后续接入边界

本模块不创建采集线程、不直接读取 `MyDeviceTransport`，也不直接发送设备命令。颜色校准采集由宿主提供的 `MyCaptureService` 统一编排，UI 只启动/停止采集、显示状态和消费最终处理结果。

颜色校准上下文必须至少保存：

```text
deviceSeries（必须）
deviceProfile（必须）
deviceIdStatus（必须）
deviceId（有则记录）
deviceModel/modelCode（有则记录）
productionMode
firmwareVersion
captureMode=CalibrationColor
scanHeadType
```

UI 不解析原始数据头、不判断单图状态、不执行 AES、不计算自动曝光。宿主/采集服务完成整组状态汇总后，把 `groupLedOn`、拍照/按钮长按状态、扫描头汇总、曝光状态和诊断文本以固定 POD 快照注入本模块。整组关灯时 UI 不应显示“自动曝光已执行”，但仍须接收和显示正常完成的慢速后处理结果。颜色校准当前不做标定器连接检查，标定器检查只属于三维校准且后续接入。详细链路见 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。
