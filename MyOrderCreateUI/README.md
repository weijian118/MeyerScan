# MeyerScan OrderCreateUI

`MyOrderCreateUI` 输出 `MeyerScan_OrderCreateUI.dll`，用于提供初版建单界面。

## 当前定位

- 本模块负责建单 UI：患者基本信息、订单基本信息、扫描类型、牙位选择、已选明细和确认操作。
- 初版把原“基本信息”和“扫描方案”的主要内容放在同一个工作台界面内，减少页面切换。
- 本模块是 Qt Widgets UI 模块，可以使用 `QWidget`、Qt Layout、信号槽和 `QString`。
- 本模块不直接保存数据库，不加载订单规则，不启动扫描；但会根据建单页输入生成当前订单的扫描流程 JSON，供 MainExe 转发给 Scan/Process 页面。
- 对外动作通过 `OrderCreateActionId` 回调抛出，避免外部模块直接绑定内部按钮对象。
- 支持 `SetOrderContextJson(const char*)` 接收标准建单上下文；第三方拉起、HIS/Worklist 和手工建单补全后都应收敛到同一套上下文结构。
- 通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格基础样式优先通过 `MeyerScan_UIComponents.dll` 创建；牙位按钮、扫描类型按钮、扫描方案联动等业务控件留在本模块内部。
- `MeyerScan_UIComponents.dll` 通过 `QLibrary` 动态加载，工程只保留头文件依赖和 DLL 复制，不强制链接 `MeyerScan_UIComponents.lib`；共享 UI 缺失时建单界面使用本地降级样式继续运行。
- 当前会调用 UIComponents v0.4.0 新增的表格接口，因此加载成功后还会检查 `GetModuleVersion()`；运行目录里如果残留旧版 UIComponents，会主动降级到本地样式，避免旧 DLL vtable 不包含新接口导致崩溃。
- 分辨率适配优先依赖 Qt Layout、滚动区、伸缩策略、控件最小尺寸和多语言自动换行；不再按 1920x1080 到实际分辨率的比例直接缩放所有坐标和控件。

## 边界

- 只做建单界面展示和临时 UI 状态。
- 不写数据库。
- 不直接读取云端或 HIS/worklist。
- 不做扫描采集、算法重建、设备通信。
- 不直接切换到 Scan/Process 页面；进入下一步仍由 MainExe/OrderScanWorkspaceShell 编排。
- 不跨 DLL 传递 `QWidget*` 以外的复杂业务对象；患者/订单数据后续应使用专门 DTO 或 JSON/UTF-8 文本结构承载。
- 不解析第三方私有字段；第三方差异统一由 `MyExternalLaunchAdapter` 按 `source.thirdPartyType` 映射成标准字段。

## 扫描流程创建

- 建单页新增四个开关：`Maxilla special scanbody`、`Mandible special scanbody`、`Maxilla segmented scanbody`、`Mandible segmented scanbody`。
- 建单页新增 `Occlusion Type` 下拉框：`Natural occlusion`、`Maxilla temporary occlusion`、`Mandible temporary occlusion`、`Full temporary occlusion`、`Bite record`。
- `GetCurrentScanProcessJson()` 会读取牙位类型、种植牙位、上述开关和咬合类型，生成 `scanProcess` JSON。
- `Segmented scanbody` 只表示对应颌第二扫描杆/第二异性扫描杆是否显示；普通扫描杆流程仍由该颌是否存在 `implant` 牙位触发，避免用户只勾选分段时凭空生成种植扫描流程。
- `OrderCreateActionScanProcessChanged` 只表示流程输入变化；MainExe 收到后读取 JSON 并合并到工作台上下文。
- ScanWorkflowUI 和 DataProcessUI 只读取 `scanProcess.steps` 渲染按钮，不反向解析建单页开关，也不复制建单流程规则。

```json
{
  "schemaVersion": 1,
  "source": "OrderCreateUI",
  "config": {
    "maxillaDiffRod": false,
    "mandibleDiffRod": false,
    "maxillaSegmentedRod": false,
    "mandibleSegmentedRod": false,
    "occlusionType": "natural",
    "maxillaHasImplant": true,
    "mandibleHasImplant": false
  },
  "steps": [
    { "part": "maxilla", "code": "maxilla_natural", "label": "Natural maxilla", "enabled": true },
    { "part": "exchange", "code": "data_exchange", "label": "Exchange", "exchange": true, "enabled": true },
    { "part": "mandible", "code": "mandible_natural", "label": "Natural mandible", "enabled": true },
    { "part": "occlusion", "code": "natural_occlusion", "label": "Natural occlusion", "enabled": true }
  ]
}
```

## 标准建单上下文

```json
{
  "schemaVersion": 1,
  "source": {
    "launchType": "external",
    "thirdPartyType": "cmd_demo",
    "thirdPartyName": "Command Line Demo",
    "sourceSystem": "cmd-simulator",
    "sourceVersion": "0.1"
  },
  "patient": {
    "patientId": "EXT-P-001",
    "name": "External Patient",
    "age": 28,
    "birthDate": "1998/03/15",
    "gender": "female",
    "contact": "13800000000",
    "note": "Created by external launch simulation."
  },
  "order": {
    "orderId": "EXT-O-001",
    "doctor": "Dr. External",
    "lab": "External Partner Lab",
    "deliveryDate": "2026/07/08",
    "caseType": "restoration",
    "note": "External order note."
  },
  "scanPlan": {
    "items": [
      { "tooth": 15, "type": "crown", "material": "--", "shade": "A2" }
    ]
  }
}
```

- `source.thirdPartyType` 必须保留，用于区分多个第三方来源。
- `patient` 和 `order` 只负责填充当前界面；保存和字段校验后续交给服务层。
- `scanPlan.items` 用于填充牙位方案；空数组时保留当前 UI 选择。

## 初版界面

- 左侧：患者编号、姓名、年龄、出生日期、性别、病例类型、医生、订单号、技工所、交付日期、联系方式、患者备注；表单类控件走 UIComponents 统一样式。
- 中间：修复类型选择和 FDI 牙位按钮，支持点击选择/取消和清空；这些是建单业务控件，不进入 UIComponents。
- 右侧：基本摘要、已选牙位明细、标信息占位、订单备注、上一步/取消/确认/下一步；普通操作按钮和表格基础样式走 UIComponents，牙位明细数据和列含义仍由本模块维护。
- 当前根界面最小尺寸降为 960x600，左/右栏宽度、牙位按钮、类型按钮、表格高度和备注框高度都按低分辨率做了收敛；低高度屏幕下左侧表单通过滚动区访问完整字段。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_OrderCreateUI.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口

- VS2015：打开 `MeyerScan_OrderCreateUI.sln`，构建并运行 `OrderCreateUITest.exe`。
- CMake/VSCode：默认开启 `OrderCreateUITest` 测试目标，可通过 `MEYER_BUILD_ORDERCREATEUITEST` 控制。
- 双击 `OrderCreateUITest.exe` 默认打开建单界面，便于人工验收。
- `OrderCreateUITest.exe --smoke` 执行自动冒烟测试并立即退出，适合命令行/批量验证。
- 当前 smoke 覆盖扫描流程控件存在性和 `GetCurrentScanProcessJson()` 输出。

## 维护记录要求

- CHANGELOG 使用中文记录。
- 代码注释使用中文。
- 界面可见文本必须使用 `tr("English source text")`，不要在源码里直接写中文界面文案。
- 模块路径必须来自 `Init(appDirUtf8, logDirUtf8)` 或 `QCoreApplication::applicationDirPath()`，不要使用 `QDir::currentPath()`。
