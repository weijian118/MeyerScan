# MeyerScan OrderCreateUI

`MyOrderCreateUI` 输出 `MeyerScan_OrderCreateUI.dll`，用于提供初版建单界面。

## 当前定位

- 本模块负责建单 UI：患者基本信息、订单基本信息、扫描类型、牙位选择、已选明细和确认操作。
- 本模块只提供工作台的 Order 步骤内容，不绘制 Order / Scan / Process / Send 步骤导航；唯一导航由 `MyOrderScanWorkspaceShell` 管理。
- 初版把原“基本信息”和“扫描方案”的主要内容放在同一个工作台界面内，减少页面切换。
- 本模块是 Qt Widgets UI 模块，可以使用 `QWidget`、Qt Layout、信号槽和 `QString`。
- 本模块不直接保存数据库，不加载订单规则，不启动扫描；但会根据建单页输入生成当前订单的扫描流程 JSON，供 MainExe 转发给 Scan/Process 页面。
- 对外动作通过 `OrderCreateActionId` 回调抛出，避免外部模块直接绑定内部按钮对象。
- 支持 `SetOrderContextJson(const char*)` 接收标准建单上下文；第三方拉起、HIS/Worklist 和手工建单补全后都应收敛到同一套上下文结构。
- 通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格优先通过 `MeyerScan_UIComponents.dll` 创建；所有视觉样式来自模块 QSS，牙位、治疗方案和扫描方案联动等业务控件留在本模块内部。
- `MeyerScan_UIComponents.dll` 通过 `QLibrary` 动态加载，工程只保留头文件依赖和 DLL 复制，不强制链接 `MeyerScan_UIComponents.lib`；共享 UI 缺失时建单界面使用本地降级样式继续运行。
- 当前会调用 UIComponents v0.4.0 新增的表格接口，因此加载成功后还会检查 `GetModuleVersion()`；运行目录里如果残留旧版 UIComponents，会主动降级到本地样式，避免旧 DLL vtable 不包含新接口导致崩溃。
- 分辨率适配优先依赖 Qt Layout、滚动区、伸缩策略、控件最小尺寸和多语言自动换行；不再按 1920x1080 到实际分辨率的比例直接缩放所有坐标和控件。
- 治疗方案选择区按当前软件视频复刻：左侧独立治疗类型卡片，中间大牙弓主视觉，右侧明细卡；上下颌使用 `maxilla.png` / `mandible.png` 显示，点击坐标反算到原始 600x400 mask；牙位叠加图和桥连接点叠加图按资源文件绘制。

## 边界

- 只做建单界面展示和临时 UI 状态。
- 不写数据库。
- 不直接读取云端或 HIS/worklist。
- 不做扫描采集、算法重建、设备通信。
- 不直接切换到 Scan/Process 页面；进入下一步仍由 MainExe/OrderScanWorkspaceShell 编排。
- 不使用源码局部 `setStyleSheet()`；UIComponents 降级路径也只设置 objectName/动态属性，由 `Resources/qss/order_create.qss` 决定外观。
- 不跨 DLL 传递 `QWidget*` 以外的复杂业务对象；患者/订单数据后续应使用专门 DTO 或 JSON/UTF-8 文本结构承载。
- 不解析第三方私有字段；第三方差异统一由 `MyExternalLaunchAdapter` 按 `source.thirdPartyType` 映射成标准字段。

## 扫描流程创建

- 建单页新增四个开关：`Maxilla special scanbody`、`Mandible special scanbody`、`Maxilla segmented scanbody`、`Mandible segmented scanbody`。
- 建单页新增 `Occlusion Type` 下拉框：`Natural occlusion`、`Maxilla temporary occlusion`、`Mandible temporary occlusion`、`Full temporary occlusion`、`Bite record`。
- `GetCurrentScanProcessJson()` 会读取牙位类型、种植牙位、上述开关和咬合类型，生成 `scanProcess` JSON。
- `Segmented scanbody` 只表示对应颌第二扫描杆/第二异性扫描杆是否显示；普通扫描杆流程仍由该颌是否存在 `implant` 牙位触发，避免用户只勾选分段时凭空生成种植扫描流程。
- `OrderCreateActionScanProcessChanged` 只表示流程输入变化；MainExe 收到后读取 JSON 并合并到工作台上下文。
- ScanWorkflowUI 和 DataProcessUI 只读取 `scanProcess.steps` 渲染按钮，不反向解析建单页开关，也不复制建单流程规则。

## 治疗方案选择

- 必须先选择一个治疗类型，再点击牙位。再次点击同一治疗类型的牙位表示取消；选择不同治疗类型后点击已选牙位表示修改类型。
- 治疗类型固定为五种：全冠 `crown`、缺失牙 `missing`、嵌体 `inlay`、贴面 `veneer`、种植体 `implant`；单颗牙叠加图序号分别为 `1/3/4/5/7`。`inner_crown` 和 `bridge` 不再作为修复类型。
- `ToothTreatmentPlanWidget` 只负责图片绘制、mask 命中、hover/tooltip 和点击回调，不保存数据库、不生成订单、不决定扫描流程。
- 牙位命中使用 `maskMaxilla.png` / `maskMandible.png` 的像素值映射 FDI 牙位号；像素递增顺序固定为上颌 `11..18,21..28`、下颌 `31..38,41..48`，该顺序来自 mask 与叠加 PNG 的逐像素重叠校验，禁止按屏幕左右视觉顺序反写。
- 桥连接点命中使用 `maskPonticMaxilla.png` / `maskPonticMandible.png`。任意相邻两颗已选牙先显示空心桥连接点；点击连接点后显示实心连接点，并记录到 `scanPlan.bridgeConnectors`。
- 外部上下文里的 `scanPlan.bridgeConnectors` 必须同时满足格式正确、同颌直接相邻且两端牙位均已选择，才会进入 UI 状态和扫描流程 JSON；内部会把 `18-17` 这类反向 key 归一化为 `17-18`，避免第三方脏数据或方向差异产生无效桥记录。
- 桥记录按旧软件规则聚合：`16-17` + `17-18` 显示为 `16-18`；跨中线示例 `11-12` + `11-21` 显示为 `11-22`。
- 多分辨率下牙弓图片只做等比缩放，点击坐标反算回 600x400 原图坐标，禁止按不同语言或不同分辨率写 if/else 调控件坐标。
- 修复类型按钮普通态使用 `*_b.png`，hover/选中态使用 `*_h.png`；屏幕达到 2560x1440 档位时加载 `*_2x.png` 源图，但按钮逻辑尺寸不翻倍。
- “Clear All” 按钮放在上下颌之间，点击时人工模式弹确认框，smoke 模式通过 `QApplication` 动态属性跳过确认框，避免自动化测试阻塞。

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

- 左侧：治疗类型图片按钮、当前治疗类型摘要、患者编号、姓名、年龄、出生日期、性别、病例类型、医生、订单号、技工所、交付日期、联系方式、患者备注；表单类控件走 UIComponents 统一样式，治疗类型属于建单业务控件。
- 中间：上下颌牙弓图片、mask 命中牙位选择、桥连接点选择、居中清空按钮和扫描流程输入；中间区域只承载牙弓主交互，不再放旧牙位按钮矩阵。
- 右侧：基本摘要、已选牙位明细、标信息占位、订单备注、上一步/取消/确认/下一步；普通操作按钮和表格基础样式走 UIComponents，牙位明细数据和列含义仍由本模块维护。
- 当前根界面最小尺寸为 960x600；左栏至少 380px，四个常用类型在首行完整显示，种植体使用第二行宽按钮；中间分栏可以扩展，但 Scan Plan 内容最大 980px 并保持居中，避免 2K/4K 下横向拉空；低高度屏幕下左侧表单通过滚动区访问完整字段。

## 资源文件规则

- 源码仓库中，治疗方案资源放在 `MyOrderCreateUI/Resources/icon/createModule/sacanPlan/`，跟随模块一起维护和提交。
- 构建时由 `MyUIResources/tools/GenerateResourceManifest.ps1` 自动收集本目录资源，编译进 `MeyerScan_UIResources.dll`，运行路径为 `:/MeyerScan/Modules/MyOrderCreateUI/icon/createModule/sacanPlan/`。
- 运行时首先使用资源 DLL；仅在资源 DLL 缺失的开发/旧安装兼容场景下，才查找源码目录、`Resources/Modules/MyOrderCreateUI/...` 和历史 `icon/createModule/sacanPlan`。
- 不再通过 CMake/VS2015 PostBuild 把牙位图、mask、叠加图和 QSS 复制为客户可修改的散文件。
- 多个模块共用的控件视觉归 UIComponents 管理；资源二进制承载归 UIResources，二者职责不要混合。
- 资源路径禁止使用 `QDir::currentPath()` 推导，防止第三方拉起 MeyerScan.exe 时工作目录变化导致资源缺失。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_OrderCreateUI.sln /p:Configuration=Release /p:Platform=x64
```

## 测试入口

- VS2015：打开 `MeyerScan_OrderCreateUI.sln`，构建并运行 `OrderCreateUITest.exe`。
- CMake/VSCode：默认开启 `OrderCreateUITest` 测试目标，可通过 `MEYER_BUILD_ORDERCREATEUITEST` 控制。
- 双击 `OrderCreateUITest.exe` 默认打开建单界面，便于人工验收。
- `OrderCreateUITest.exe --smoke` 执行自动冒烟测试并立即退出，适合命令行/批量验证。
- `OrderCreateUITest.exe --capture-screenshot <png> --capture-size <WxH>` 按指定尺寸渲染建单界面并保存截图；省略尺寸时默认 1920x1080，可用于和 `D:\wj\OrderTreatmentPlan\治疗方案选择.mp4` 提取帧做人工逐帧对齐。
- 当前 smoke 覆盖五种修复类型、`1/3/4/5/7` 叠加图序号、上下颌 mask 顺序、桥 mask 顺序、b/h hover 图切换、2K 资源阈值、相邻牙空心桥候选、无效桥过滤、普通桥区间 `16-18`、跨中线桥区间 `11-22`、清空/确认动作和扫描流程 JSON。

## 维护记录要求

- CHANGELOG 使用中文记录。
- 代码注释使用中文。
- 界面可见文本必须使用 `tr("English source text")`，不要在源码里直接写中文界面文案。
- 模块路径必须来自 `Init(appDirUtf8, logDirUtf8)` 或 `QCoreApplication::applicationDirPath()`，不要使用 `QDir::currentPath()`。
