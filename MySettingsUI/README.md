# MeyerScan SettingsUI

`MySettingsUI` 输出 `MeyerScan_SettingsUI.dll`，用于承载软件设置界面。

当前模块先搭建框架和流程：

- 左侧设置分类：General、Information、Calibration、Cloud、Scan、Data Processing、About。
- 设置底部操作：Confirm、Apply、Restore、Cancel。
- Calibration 分类中提供 3D Calibration 和 Color Calibration 入口。
- 三维校准和颜色校准通过 `MeyerScan_Calibration3DUI.dll` / `MeyerScan_CalibrationColorUI.dll` 嵌入设置模块，不把校准流程写入 MainExe。
- MainExe、HomeUI、CaseUI、后续 ScanReconstructStudio 只请求打开设置模块，不直接拼设置页面。

## 设置数据模型

### 一般设置（General）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 新手引导 | bool | 关/开 | 控制新手引导开关 |
| 数据格式 | enum | PLY | PLY / OBJ / STL |
| 订单存储路径 | string | QStandardPaths | 订单数据的本地存储目录 |
| 订单打包路径 | string | QStandardPaths | 订单打包数据的本地目录 |

### 信息管理（Information，v0.2.0 新增）

| 标签页 | 字段 | 说明 |
|--------|------|------|
| 医生管理 | 姓名/性别/电话/科室 | 从 RuntimeDataCenter 的 `local.doctors` 读取只读快照 |
| 诊所管理 | 名称/地址/电话/城市 | 从 RuntimeDataCenter 的 `local.clinics` 读取只读快照 |
| 技工所管理 | 名称/联系人/电话/地址 | 从 RuntimeDataCenter 的 `local.labs` 读取只读快照 |

每个标签页包含搜索栏、数据表格和编辑/删除按钮。当前只做读取展示；新增、编辑、删除和主数据维护后续统一接入 CaseOrderService 或专门设置服务。

测试宿主在空 SQLite 库中会创建最小演示表并写入医生、诊所、技工所、患者、订单各一条数据，用于验证“数据库 -> RuntimeDataCenter -> SettingsUI 表格”的链路。正式 `MeyerScan_SettingsUI.dll` 不负责建表、迁移或写入业务数据。

测试宿主从 exe 所在目录向上查找 `MeyerScan_AllModules.sln` 作为仓库根，因此同时兼容单模块输出目录 `MySettingsUI\bin\Release` 和根聚合输出目录 `F:\MeyerScan\bin\Release`。

测试宿主会在输出目录生成 `config/SettingsUITest/db_config.json` 和独立 SQLite 测试库，不复用公共 `config/db_config.json`，避免和 RuntimeDataCenterTest、CaseUITest 等测试互相污染表结构。

### 云端设置（Cloud，v0.2.0 新增）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 账号状态 | text | Not logged in | 云端账号登录状态 |
| 用户名/邮箱 | string | (空) | 登录用户名 |
| 密码 | string | (空) | 登录密码 |
| 服务器地址 | string | https://cloud.meyerscan.com | 云服务器 URL |
| 自动上传 | enum | Enabled | 完成后自动上传开关 |

### 扫描设置（Scan，v0.2.0 新增）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 扫描提示图 | bool | true | 扫描时显示提示图片 |
| 可续扫时间 | enum | 7 days | 允许继续扫描的时间范围 |
| 录屏 | bool | false | 扫描过程录屏 |
| 默认订单类型 | enum | Restoration | 新建订单默认类型 |
| 完成后跳转 | enum | Stay | 扫描完成后的页面跳转行为 |
| 体感控制 | bool | false | 体感手势控制开关 |

### 数据处理设置（Data Processing，v0.2.0 新增）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 处理配置 | enum | Standard | 数据处理品质配置 |
| 上下颌补洞范围 | enum | Medium | 上下颌模型补洞强度 |
| 扫描杆补洞范围 | enum | Small | 扫描杆补洞强度 |

### 设置持久化策略

- **骨架期（当前 v0.2.0）**：所有设置项的默认值硬编码在 UI 代码中，路径使用 `QStandardPaths::DocumentsLocation` 派生的用户目录占位，不显示开发机 `D:/` 路径；
  修改后暂不持久化，仅停留在 UI 控件层面。
- **正式阶段（规划）**：设置项的读写统一走 `ConfigCenter.dll` 的 `runtime_config.json`，由 ConfigCenter 负责配置的版本校验、迁移回滚和变更通知。设置模块不直接访问文件系统或数据库。
- **路径字段备注**：订单存储路径和打包路径在正式阶段应从 ConfigCenter 读取用户/客户配置，
  不依赖硬编码默认值。

### 校准模块加载策略

- 3D Calibration 和 Color Calibration 通过 `QLibrary` 按需动态加载（懒加载）。
- 当前骨架期静默加载，加载失败显示 "Calibration module is not available."
- 正式阶段需考虑：
  - 加载超时机制（如 `QTimer::singleShot` 兜底）
  - 加载慢时显示 loading 状态占位
  - 卸载/重载策略

## 边界规则

- 设置模块是界面模块，可以使用 Qt Widgets、Layout、信号槽、`QString`；设置保存、权限判断、业务数据维护和校准算法/设备重资源仍通过 ConfigCenter、Permission、RuntimeDataCenter、校准模块或后续服务接口完成。
- 设置模块不直接读写业务数据库；后续配置保存统一走 ConfigCenter 或专门设置服务。
- Information 页面允许读取 RuntimeDataCenter 的 JSON 快照；解析跨 DLL JSON 缓冲区时必须按真实 UTF-8 字符串读取，不能把整块预分配缓冲区直接交给 JSON 解析器。
- SettingsUI 只初始化 RuntimeDataCenter，不主动 `ReloadAll()`；MainExe 启动期负责全域刷新，独立测试或缓存为空时由 `GetDomainJson()` 按需懒加载 Information 页需要的医生、诊所、技工所 domain。
- 可见文本必须使用 `tr("English source text")`，中文显示由 qm 翻译。
- 日志目录由 MainExe 或测试宿主基于安装目录传入，禁止用当前工作目录推导运行资源。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_SettingsUI.sln /p:Configuration=Release /p:Platform=x64
```

## 2026-07-01 补充说明

- `SettingsUITest.exe --smoke` 的演示库会补齐 RuntimeDataCenter 当前全部本地 domain 的最小表结构和一条演示数据。
- SettingsUI Information 页面当前只展示医生、诊所、技工所，但 MainExe 集成时会刷新软件信息、设置、账号、患者、订单、设备等 domain；演示库补齐这些表可以让日志保持干净。
- 正式 `MeyerScan_SettingsUI.dll` 仍只读取 RuntimeDataCenter 快照，不承担 schema 初始化、旧库迁移或业务数据写入。
