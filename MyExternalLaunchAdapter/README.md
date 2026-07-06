# MyExternalLaunchAdapter

`MyExternalLaunchAdapter` 输出 `MeyerScan_ExternalLaunchAdapter.dll`，用于第三方软件拉起 MeyerScan 后，把第三方下发的建单 JSON 转换为 `OrderCreateUI` 可直接显示的标准建单上下文。

## 当前定位

- 本模块是第三方/HIS/命令行模拟输入与 MeyerScan 内部建单流程之间的适配层。
- 当前支持从命令行传入 JSON 文件路径，由 MainExe 调用本模块读取并归一化。
- 标准输出 JSON 结构固定为 `source / patient / order / scanPlan`，其中 `source.thirdPartyType` 必须记录第三方类型。
- 后续新增多个第三方时，优先在本模块按 `thirdPartyType` 增加字段映射和默认规则，不改 MainExe 和 OrderCreateUI。
- MainExe 负责显示工作台和建单界面；本模块只输出标准上下文，不直接参与 UI 切换。

## 边界

- 不显示 UI。
- 不保存数据库。
- 不决定加载订单规则。
- 不启动扫描进程。
- 不直接调用 OrderCreateUI。
- 公共 ABI 使用 `const char*`、POD 结构体和调用方缓冲区，不暴露 Qt 类型。
- `thirdPartyType` 是多第三方分流字段；同一个输入 JSON 在调试时也可以通过命令行传不同类型来验证不同映射规则。

## 标准上下文示例

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

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_ExternalLaunchAdapter.sln /p:Configuration=Release /p:Platform=x64
```

## 测试

```powershell
F:\MeyerScan\MyExternalLaunchAdapter\bin\Release\ExternalLaunchAdapterTest.exe
```

测试会读取 `test/external_order_sample.json`，验证第三方类型、患者、订单和扫描方案字段能被归一化为标准上下文。

## MainExe 调用方式

```powershell
F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --external-order F:\MeyerScan\MyExternalLaunchAdapter\test\external_order_sample.json --external-order-type cmd_demo
```

MainExe 调用本模块后，会后台准备首页 Create 入口并直接显示 `OrderScanWorkspaceShell/OrderCreateUI`。客户不会看到首页闪现。

## 维护要求

- 源码注释使用中文，关键代码需要解释实现方式和边界原因。
- 自研源码保存为 UTF-8 BOM，避免 VS2015 按本地代码页误读中文注释。
- CHANGELOG 使用中文记录。
- 新增第三方时必须明确 `thirdPartyType`，并在 README 或映射说明中记录字段差异。
- 不使用 `QDir::currentPath()` 推导路径，所有路径由 MainExe 传入或基于 `QCoreApplication::applicationDirPath()`。
