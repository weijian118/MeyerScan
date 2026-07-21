# MeyerScan SettingsUI

`MySettingsUI` 输出 `MeyerScan_SettingsUI.dll`，用于承载软件设置界面。

当前模块先搭建框架和流程：

- 左侧设置分类：General、Information、Calibration、Cloud、Scan、Data Processing、About。
- 设置底部操作：Confirm、Apply、Restore、Cancel。
- Calibration 分类中提供 3D Calibration 和 Color Calibration 入口。
- 三维校准通过 `MeyerScan_Calibration3DUI.dll` 保持设置内部嵌入流程；颜色校准通过 `MeyerScan_CalibrationColorUI.dll` 在设置窗口上方显示半透明模态弹窗，不把校准业务写入 MainExe 或 SettingsUI。
- 颜色校准弹窗打开后可以拖动自定义标题栏移动面板，拖动不会移动全屏遮罩，也不会被设置页 Layout 自动弹回中心位置。
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
| 医生管理 | 姓名/性别/电话/科室 | 从宿主注入的 `local.doctors` 读取只读快照 |
| 诊所管理 | 名称/地址/电话/城市 | 从宿主注入的 `local.clinics` 读取只读快照 |
| 技工所管理 | 名称/联系人/电话/地址 | 从宿主注入的 `local.labs` 读取只读快照 |

每个标签页包含搜索栏、数据表格和编辑/删除按钮。当前只做读取展示；新增、编辑、删除和主数据维护后续统一接入 CaseOrderService 或专门设置服务。

测试宿主直接构造医生、诊所、技工所 JSON fixture，通过 `SetDataContextJson()` 注入并验证三张表；测试不建库、不写配置，也不依赖 RuntimeDataCenter。

### 云端设置（Cloud，v0.2.0 新增）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 账号状态 | text | Not logged in | 云端账号登录状态 |
| 用户名/邮箱 | string | (空) | 登录用户名 |
| 密码 | string | (空) | 登录密码 |
| 服务器地址 | string | (空) | 云服务器 URL；正式值由 MainExe/设置服务从 ConfigCenter 读取后注入 |
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

- **骨架期（当前 v0.7.0）**：路径使用 `QStandardPaths::DocumentsLocation` 派生安全默认值，云端地址保持空白提示；正式配置由 MainExe/设置服务读取 ConfigCenter 后通过版本化上下文注入，不显示开发机 `D:/` 路径，也不由 UI 直接读取配置文件；
  修改后暂不持久化，仅停留在 UI 控件层面。
- **正式阶段（规划）**：设置项的读写统一走 `ConfigCenter.dll` 的 `runtime_config.json`，由 ConfigCenter 负责配置的版本校验、迁移回滚和变更通知。设置模块不直接访问文件系统或数据库。
- **路径字段备注**：订单存储路径和打包路径在正式阶段应从 ConfigCenter 读取用户/客户配置，
  不依赖硬编码默认值。

### 校准模块加载策略

- 3D Calibration 和 Color Calibration 通过 `QLibrary` 按需动态加载（懒加载）。
- DLL 加载、工厂解析和 `Init()` 是三个独立成功条件；Init 返回 false 时 SettingsUI 调用子模块 Shutdown、清空接口并显示不可用占位，不影响其它设置页。
- 颜色校准按钮先同步调用 MainExe 的 `DeviceSessionHost`，固定顺序为：创建/练习工作台门禁、连接、USB3、D4/D9 设备编号、生产模式 C2/C7 系列探测、CD/CE 型号代码和产品身份识别。
- 颜色校准不要求设备已经写入正式编号；生产调试设备可使用带来源标记的 effective 默认身份。后续三维校准接入设备预检时采用相同规则，但连接、USB3、系列/型号识别和证据冲突门禁仍然生效。
- 预检通过后只显示一次设备信息弹窗，内容包括 effective 设备编号、具体产品、effective 型号代码、生产模式和兼容来源；CE 降级原因附在同一弹窗中。真实 reported 值继续保存在 POD 和日志中。
- SettingsUI 把连接、产品身份和完整检测记录组成的只读 POD 快照注入 CalibrationColorUI，再显示全窗口半透明遮罩和居中面板；关闭时上报独立动作让宿主释放设备会话。
- `SettingsUITest --capture-color-calibration <png>` 验证设备信息弹窗和成功进入遮罩；`--test-preflight-status 2/3/4/7/9/10/11/12/13` 可验证工作台、连接、USB、型号、冲突、回包异常及非法值提示。
- 加载失败时记录 Warning；三维校准显示不可用占位，颜色校准不创建空白遮罩。
- 正式阶段需考虑：
  - 加载超时机制（如 `QTimer::singleShot` 兜底）
  - 加载慢时显示 loading 状态占位
  - 卸载/重载策略

## 边界规则

- 设置模块是界面模块，可以使用 Qt Widgets、Layout、信号槽、`QString`；设置保存、权限判断、业务数据维护和校准算法/设备重资源仍通过宿主、ConfigCenter、Permission、校准模块或后续服务接口完成。
- 设置模块不直接读写业务数据库；后续配置保存统一走 ConfigCenter 或专门设置服务。
- 设备原始回包由 DeviceCmd 集中解析；SettingsUI 只消费固定 POD，不接收 `std::string`、字符串数组、USB 句柄或原始命令缓冲区。
- 通用提示使用 UIComponents 独立 C ABI；共享 DLL 不可用时降级为 Qt 标准弹窗，业务门禁仍必须生效。
- Information 页面只读取 MainExe 注入的版本化 JSON 快照；SettingsUI 不知道 RuntimeDataCenter、表名或数据库配置。
- `SetDataContextJson()` 采用先校验后替换，非法 JSON 不覆盖上一份有效快照。
- 可见文本必须使用 `tr("English source text")`，中文显示由 qm 翻译。
- 日志目录由 MainExe 或测试宿主基于安装目录传入，禁止用当前工作目录推导运行资源。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_SettingsUI.sln /p:Configuration=Release /p:Platform=x64
```
