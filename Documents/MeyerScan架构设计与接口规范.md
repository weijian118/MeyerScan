# MeyerScan 口扫软件重构 — 架构设计与接口规范

> **文档目的**：定义模块边界划分、接口规范、依赖关系、调用时序、设计思路、注释规范、开发规约，输出接口声明（C++风格），确保模块解耦。
>
> **创建日期**：2026-06-16
>
> **适用范围**：所有参与 MeyerScan 重构的开发人员
>
> **2026-07-10 现行口径提示**：本文第 5-6 章包含早期 `Core.lib` / `CaseEntity.lib` 接口草案，只用于设计追溯，不是当前待实现清单。当前不把这两个静态库作为前置；患者/订单合同由 CaseOrderService DTO、版本化 UTF-8 JSON、`schemaVersion` 和 `extensions` 承接。任何与第 14 章冲突的旧草案，以第 14 章为准。
>
> **权威文档位置（2026-07-12 起）**：仓库内 `F:\MeyerScan\Documents\MeyerScan架构设计与接口规范.md`。`D:\wj\重构文档` 下同名文件仅为同步镜像；模块公共头文件仍是可编译接口的最终事实来源。

---

## 一、现有文档改进意见

### 1.1 文档结构改进

| 问题 | 改进建议 |
|------|----------|
| **文档内容过于分散** | 保留 4 个核心文档但严格分工：任务总览管范围/模块/原则；架构规范管边界/接口/依赖；进度跟踪只管状态/验证；AI 协作记录只管按时间追溯。四份权威文件统一存放在 `F:\MeyerScan\Documents`，不再合并不同用途的内容。 |
| **接口定义不够具体** | 当前文档中的接口设计多为伪代码片段，缺少完整的头文件声明、入参出参类型定义、错误码、枚举等 |
| **缺少模块边界明确定义** | 需要明确每个模块的"职责边界"（做什么/不做什么）、"对外接口边界"（哪些是公开的/私有的）、"依赖边界"（依赖谁/被谁依赖） |
| **缺少调用时序图** | 需要补充关键流程的调用时序图，如：启动流程、扫描流程、病例创建流程等 |
| **注释规范未落地** | 需要给出具体的注释模板，包括文件头注释、类注释、函数注释、枚举注释等 |
| **开发规约不够具体** | 需要细化命名规范、代码风格、错误处理规范、日志规范等 |

### 1.2 技术设计改进

| 问题 | 改进建议 |
|------|----------|
| **接口未定义错误码** | 所有模块需要统一的错误码定义，便于调用方判断返回结果 |
| **缺少结构体版本化** | 跨 DLL/跨进程传递的结构体需要版本号字段，便于后续扩展 |
| **缺少接口版本化机制** | 接口需要支持版本查询，便于兼容性检查 |
| **日志宏未考虑模块名获取** | Logger.h 中的 MEYER_MODULE_NAME 依赖编译定义，需要提供备用机制 |
| **IPC 协议未详细定义** | 需要定义具体的 IPC 命令枚举、响应枚举、超时策略 |
| **数据库切换机制不够具体** | 病例域服务的 MySQL/SQLite 切换需要明确的配置结构和切换流程 |

---

## 二、模块边界划分与解耦原则

### 2.1 模块分层架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           第五层：主壳与页面容器层                              │
│  ┌────────────────────┐  ┌──────────────────────────────┐  ┌──────────────┐ │
│  │   MeyerScan.exe    │  │ OrderScanWorkspaceShell.dll  │  │ HomeUI.dll   │ │
│  │ 主入口/单实例/编排 │  │ 建单-扫描-处理-发送统一壳       │  │ 首页入口 UI   │ │
│  └────────────────────┘  └──────────────────────────────┘  └──────────────┘ │
│         │                         │                              │             │
│         │ 统一切换页面/释放资源     │ 挂载 OrderCreateUI/扫描窗口     │ 上报入口 ID  │
│         ▼                         ▼                              ▼             │
└─────────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────────┐
│                           第四层：业务 UI / 交互层                             │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐  ┌──────────────┐   │
│  │   CaseUI     │  │ OrderCreateUI│  │    SendUI     │  │Calibration/Settings│ │
│  │    .dll      │  │    .dll      │  │     .dll      │  │      UI .dll        │ │
│  └──────────────┘  └──────────────┘  └────────────────┘  └──────────────┘   │
│         │                 │                  │                  │             │
│         │ 调用领域服务     │ 输出建单上下文     │ 调用算法/设备 DLL   │ 调用算法/设备 DLL │
│         ▼                 ▼                  ▼                  ▼             │
└─────────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────────┐
│                           第三层：业务服务层                                   │
│  ┌──────────────────┐ ┌──────────────────┐ ┌──────────────┐ ┌─────────────┐ │
│  │ CaseOrderService│ │RuntimeDataCenter │ │OrderWorkflow │ │ Permission  │ │
│  │ 患者/订单 CRUD   │ │本地/云端快照缓存  │ │加载/继续规则 │ │    .dll     │ │
│  └──────────────────┘ └──────────────────┘ └──────────────┘ └─────────────┘ │
│         │                  │                    │                 │          │
│         │ 经 Adapter 调用DB │ 经 Adapter 读DB/注入云端│ 读取订单/方案/权限 │ 读取规则快照 │
│         ▼                  ▼                    ▼                 ▼          │
└─────────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────────┐
│                           第二层：数据/设备/算法支撑层                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌─────────────────────┐ │
│  │  Database    │ │DatabaseQtAdapter│ │DeviceTransport│ │ ScanSchema/ScanData │ │
│  │  纯C++ .dll  │ │ Qt适配 .dll    │ │    .dll       │ │ IO/PreProcess       │ │
│  └──────────────┘ └──────────────┘ └──────────────┘ └─────────────────────┘ │
│         │               │                 │                    │             │
│         │ 裸 SQL/事务    │ Qt类型转换        │ 连接/收发             │ 数据文件/预处理 │
│         ▼               ▼                 ▼                    ▼             │
└─────────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────────┐
│                           第一层：基础设施层                                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
│  │   Logger     │ │ ConfigCenter │ │ UIComponents │ │  UIResources │       │
│  │    .dll      │ │    .dll      │ │    .dll      │ │    .dll      │       │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘        │
│         │               │                 │                  │              │
│         │ 最先加载       │ 配置唯一入口      │ 公共控件工厂       │ 只读资源注册   │
│         ▼               ▼                 ▼                  ▼              │
└─────────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────────┐
│                           独立进程层                                          │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    ScanReconstructStudio.exe                           │  │
│  │  独立扫描壳；动态加载 ScanWorkflowUI.dll + DataProcessUI.dll             │  │
│  └──────────────────────────────────────────────────────────────────────┘   │
│         │                                                                       │
│         │ IPC/窗口嵌入/状态同步，与主 EXE 和工作台壳保持上下文一致                 │
│         ▼                                                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

**本轮修正后的关键分层口径**：

1. 患者和订单在口扫软件内部、数据库内部强绑定，不再拆成 `CaseService.dll` 与 `OrderService.dll` 两个服务；统一由 `CaseOrderService.dll` 管理患者/订单组合数据和医生、技工所、诊所等数据库主数据/字典数据。
2. `CaseOrderService.dll` 是数据库领域服务，不做 UI 渲染、扫描方案、扫描采集、算法处理或设备通信；它负责把 UI/外部适配器传入的患者/订单 JSON/DTO 映射到版本化数据库 schema。
3. `RuntimeDataCenter.dll` 是运行时读模型/缓存中心，负责把本地诊所、技工所、软件、医生、设置、账号、患者、订单、设备等常用信息，以及云端诊所信息包装成稳定 domain JSON 快照；它不做保存、删除、权限判断或联网请求。
4. `OrderWorkflowService.dll` 只判断流程走向，不保存患者/订单数据；它读取 CaseOrderService、ScanSchemaService、Permission 的结果后决定新建、加载、继续扫描、处理或发送。
5. 校准不再拆成“入口 UI + 三维算法 DLL + 颜色算法 DLL”三段，改为两个 UI+流程+计算入口模块：`Calibration3DUI.dll` 和 `CalibrationColorUI.dll`。这两个模块可以调用算法 DLL、DeviceCmd/DeviceTransport，但不进入病例/订单业务。
6. 建单模块和扫描重建模块需要统一视觉和切换体验，`OrderScanWorkspaceShell.dll` 作为工作台壳是必要模块：它只做容器、步骤导航、窗口/页面挂载和一致性，不保存建单数据、不做扫描算法。
7. **Qt 使用边界（2026-07-03 收口）**：UI 模块、设置模块、校准 UI、工作台壳等界面相关模块正常使用 Qt Widgets、布局、信号槽和 Qt 容器；非界面模块默认优先评估纯 C++ / 标准库 / 专用 SDK，能不用 Qt 就不用 Qt。`Database.dll` 当前已改为纯 C++，Qt 模块不得直接把 `QString` / `QJsonDocument` 转换逻辑散落到各处，必须通过 `DatabaseQtAdapter.dll` 进入 Database。CaseOrderService、RuntimeDataCenter 等仍可在内部使用 Qt JSON，但公共头文件不得暴露 Qt 类型。
8. **UI 顶部和步骤导航归属（2026-07-10 收口）**：MainExe 只提供无边框全屏顶层窗口和单内容区，不绘制所有页面共享的可见标题栏；HomeUI、CaseUI、OrderScanWorkspaceShell 各自绘制语义顶部区域。Order/Scan/Process/Send 步骤导航唯一归 OrderScanWorkspaceShell，内容页不得复制第二套步骤条。
9. **UI 资源边界（2026-07-10 收口）**：UIComponents 管理控件工厂、角色、基础尺寸和公共交互；UIResources 只聚合各模块 PNG/QSS/qm 等只读数据并注册 Qt 资源树。资源源码继续放在所属模块 `Resources`，正式发布只交付 `MeyerScan_UIResources.dll`，二者不能合并成一个包含业务控件与所有资源的万能 UI 模块。

### 2.2 模块职责边界定义

| 模块 | 职责范围（做什么） | 禁止事项（不做什么） | 边界类型 |
|------|-------------------|-------------------|----------|
| **Logger.dll** | 结构化日志写入、多进程安全、文件轮转、级别过滤 | 业务逻辑、UI 渲染、设备通信 | 基础设施，无业务边界 |
| **UIResources.dll** | 聚合并注册各 UI 模块的 PNG/QSS/qm 等只读 Qt 资源，提供稳定 `:/MeyerScan/Modules/<ProjectName>/...` 路径 | 创建 QWidget、定义业务控件、权限判断、语言状态管理、加密保护承诺 | 基础设施，只读资源提供器；资源源码所有权仍归业务模块 |
| **公共稳定契约（按需）** | 已在多个模块真实重复且变化较低的 ErrorCode/Result、动作 ID、IPC 消息头或少量无状态工具 | 具体业务实现、配置读取、数据库访问、权限判断、Qt UI 工具、患者/订单高频字段 | 当前不单独创建 Core.lib；需要时抽取最小公共头/静态库 |
| **ConfigCenter.dll** | 配置读写、版本校验、迁移回滚、变更通知 | 业务逻辑、数据库操作 | 唯一配置入口 |
| **Crypto.dll** | AES 加解密、文件加解密、哈希计算、密钥轮换 | 业务逻辑、UI 渲染 | 纯加密服务 |
| **患者订单数据合同** | CaseOrderService 内 DTO、稳定字段 key、`schemaVersion`、`extensions` 和数据库 schema 映射 | UI 逻辑、扫描算法、跨 DLL 固定高频字段结构体 ABI | 服务内契约 + UTF-8 JSON；当前不单独创建 CaseEntity.lib |
| **CaseOrderService.dll** | 患者/订单组合数据 CRUD、查询、状态、操作留痕；医生/技工所/诊所等数据库主数据/字典数据读写；字段版本和 schema 映射 | UI 渲染、扫描方案、扫描采集、算法处理、设备通信、加载订单流程决策 | 患者/订单数据库领域服务；替代原 CaseService + OrderService 双模块 |
| **RuntimeDataCenter.dll** | 本地常用数据库信息和云端诊所信息的运行时 JSON 快照；本地 domain 包括诊所、技工所、软件、医生、设置、账号、订单、患者、设备；云端 domain 当前为 `cloud.clinicProfile` | UI 渲染、业务 CRUD、权限判断、云端请求、任意 SQL 查询通道 | 运行时读模型/缓存中心；通过稳定 domain 返回 UTF-8 JSON，吸收高频字段变化 |
| **ScanSchemaService.dll** | 扫描方案 CRUD、牙位/修复体/材料/齿色结构化保存、方案校验 | UI 渲染、扫描采集、数据处理 | 扫描方案服务 |
| **OrderWorkflowService.dll** | 新建/加载/继续扫描/进入发送前的规则判断，输出下一步动作和编辑策略 | UI 渲染、数据库连接管理、扫描采集实现 | 订单流程规则服务 |
| **DataExport.dll** | 患者/订单/扫描数据导入导出、打包、导出路径策略 | UI 渲染、邮件发送、数据库连接管理 | 数据导入导出服务 |
| **Statistics.dll** | 患者/订单/设备使用统计、查询聚合、报表数据准备 | UI 渲染、业务数据修改 | 统计服务 |
| **HomeUI.dll** | 首页入口展示、入口状态渲染、OEM 首页布局/Logo/文案定制 | 建单规则、订单恢复规则、权限判断核心逻辑、数据库操作 | 纯入口 UI，可替换定制 |
| **CaseUI.dll** | 患者列表、订单列表、详情、发送等界面展示；当前可读取 RuntimeDataCenter 的列表快照；内部按页面子模块组织 | 业务逻辑、数据库操作、设备通信、保存/删除患者订单 | 纯 UI 展示；读模型和业务 CRUD 分离 |
| **OrderCreateUI.dll** | 新建订单/编辑订单基本信息/扫描方案表单展示；治疗方案图片/mask 命中、牙位叠加图、桥连接点和扫描流程输入等建单业务控件留在本模块；测试宿主提供 smoke 和截图验收 | 订单保存规则、加载订单流程判断、扫描采集、通用控件样式定义、Order/Scan/Process/Send 步骤导航 | 纯表单内容 UI；工作台步骤导航归 OrderScanWorkspaceShell |
| **SendUI.dll** | 工作台 Send 步骤页面；显示案例信息、数据格式和备注；提供 Export、Compress、Email Send、Upload、Previous、Finish 动作按钮，并通过回调上报宿主 | 数据库访问、数据打包、网络上传、邮件发送、流程规则判断 | 纯发送页 UI；真实发送能力走服务模块 |
| **ExternalLaunchAdapter.dll** | 第三方软件拉起协议解析、患者/订单字段映射、来源校验；输出标准建单上下文 `source/patient/order/scanPlan`，并强制保留 `source.thirdPartyType` 等来源字段 | UI 渲染、数据库操作、扫描采集、直接调用 OrderCreateUI | 第三方建单适配 |
| **HisWorklistAdapter.dll** | HIS/Worklist 查询、患者信息下拉、字段映射、连接状态 | UI 渲染、业务保存、扫描采集 | 医院/连锁系统适配 |
| **Permission.dll** | 六维权限校验、功能授权状态、功能开关控制、授权规则加载 | UI 渲染、数据库操作 | 权限门禁 |
| **PermissionConfigUI.dll** | 授权状态展示、扫码授权入口、权限配置界面 | 权限判断核心逻辑、数据库操作 | 权限配置 UI |
| **Database.dll** | 纯 C++ 数据库基础设施；读取 `db_config.json`；SQLite 通过动态加载 `sqlite3.dll` 调 C API；保留 MySQL 配置与类型枚举，原生 MySQL C API 待 SDK 接入；提供连接、裸 SQL、事务、备份、脚本执行和通用 SELECT 结果 JSON 化；公共 ABI 保持 POD/UTF-8/调用方缓冲区 | 业务逻辑、UI 渲染、设备通信、权限判断、开发机路径兜底、公共头文件暴露 Qt 类型、Qt 类型转换 | 通用基础设施，裸 SQL 执行；Qt 调用方必须通过 DatabaseQtAdapter |
| **DatabaseQtAdapter.dll** | 为 Qt UI/Service 提供 `QString`、`QByteArray`、`QJsonDocument` 到 Database UTF-8/POD 接口的转换；管理调用方查询缓冲区扩容；提供骨架期 SQL 文本转义工具；使用 Logger 记录适配层生命周期和失败边界日志 | 业务 CRUD、字段含义判断、权限判断、UI 渲染、底层连接实现、逐条记录正常查询成功日志 | Qt 到纯 C++ Database 的唯一适配层 |
| **UIComponents.dll** | 通用按钮、字段标签、输入框、下拉框、日期框、多行文本框、基础表格、弹窗、日历控件、等待页、主题管理、多语言友好尺寸策略；当前已支持按钮角色（Primary/Secondary/Text/Danger/Entry）和内容布局（纯文字/纯图标/左图右文/上图下文），基础表格只统一外观和默认交互 | 业务逻辑、设备通信、页面跳转、权限判断、表格数据来源/列语义/分页排序、单模块专用页面控件、业务控件状态联动 | 纯 UI 控件 |
| **DeviceTransport.dll** | USB/串口/网口数据传输、连接管理 | 命令组装/解析、业务逻辑 | 传输层 |
| **DeviceCmd.dll** | 命令组装/解析/校验、设备状态查询 | 底层传输实现、业务逻辑 | 命令层 |
| **Calibration3DUI.dll** | 三维校准界面、标定器流程、25 幅采集编排、三维校准计算入口、结果展示和记录；可调用算法 DLL 与设备 DLL | 颜色校准、病例管理、云端上传、患者/订单数据维护 | 三维校准 UI + 流程 + 计算入口 |
| **CalibrationColorUI.dll** | 颜色校准界面、颜色标定器流程、颜色采集、颜色校正参数生成、结果展示和记录；可调用算法 DLL 与设备 DLL | 三维校准、病例管理、云端上传、患者/订单数据维护 | 颜色校准 UI + 流程 + 计算入口 |
| **ScanDataIO.dll** | 帧数据保存/读取、完整性校验、多设备格式解析 | 图像处理、算法重建 | 数据存储 |
| **ScanDataPreProcess.dll** | 数据解密/镜像/裁剪/颜色校准/AI消去 | 病例管理、UI 渲染 | 预处理流水线 |
| **NetworkHelper.dll** | 云端上传/下载/同步、邮件发送 | 病例管理、设备通信 | 云端服务 |
| **Login.dll** | 账号/手机/微信登录、授权验证 | 病例管理、设备通信 | 登录服务 |
| **MyUpdate.exe** | 检查更新、主动更新、云端更新策略拉取、本机条件比对、补丁下载、关闭主程序、覆盖升级、重启主程序 | 业务建单、扫描采集、病例管理 | 独立更新进程，与 MeyerScan.exe 同级发布 |
| **MyInstaller / Packaging** | 生成安装包、自定义安装界面、自定义安装流程、安装目录层级、依赖收集、版本清单写入 | 运行时业务逻辑、扫描采集、病例管理、数据库访问 | 发布交付模块，不作为运行时插件 |
| **EngineeringSettings.dll** | 设备参数设置、下位机固件升级、扫描参数设置、数据处理参数设置 | 扫描重建、算法处理、数据库管理、权限配置 | 高级设置入口 |
| **ScanWorkflowUI.dll** | 扫描阶段界面、扫描对象选择、扫描工具区、扫描控制区、QVTK 显示区和扫描阶段动作上报；顶部流程按钮具备手型 hover、tooltip、点击切换显示数据；滚轮缩放以鼠标位置为中心并限制范围；进入页面创建 QVTK/VTK 重资源，离开页面释放 | 患者/订单保存、数据库访问、设备传输实现、设备命令实现、扫描算法实现 | 扫描进程内部 UI DLL |
| **DataProcessUI.dll** | 数据处理阶段界面、模型选择、处理工具入口、独立 Process Hint、QVTK 显示区和处理阶段动作上报；顶部流程按钮具备手型 hover、tooltip、点击切换显示数据；不放扫描页 Start/Pause；滚轮缩放以鼠标位置为中心并限制范围；进入页面创建 QVTK/VTK 重资源，离开页面释放 | 扫描采集、设备连接、重算法实现、病例管理、数据库访问 | 扫描进程内部 UI DLL |
| **SendUI.dll** | 工作台发送步骤界面、案例信息显示、数据格式选择占位、导出/压缩/邮件发送/上传/上一步/完成动作上报 | 数据库访问、真实导出打包、网络上传、邮件发送、发送流程规则 | 发送页 UI DLL |
| **ScanReconstructStudio.exe** | 独立扫描重建壳子；动态加载 ScanWorkflowUI.dll 和 DataProcessUI.dll；承载“扫描/数据处理”两个大阶段；负责阶段导航、页面挂载、资源释放和后续 IPC 状态同步 | 病例管理、云端上传、权限核心、设备协议、扫描算法、后处理算法 | 独立进程壳 + 两个阶段 UI DLL |

### 2.3 解耦约束原则

| 约束类别 | 具体规则 |
|----------|----------|
| **禁止跨层调用** | UI → Service → DAO → Entity，仅允许单向依赖，禁止反向调用 |
| **禁止循环依赖** | A → B → C → A 的循环依赖必须通过事件/回调解耦 |
| **禁止跨进程传递 QObject/指针所有权** | IPC 不传 QObject、QString 指针、模型对象或大块内存所有权；复杂患者/订单上下文通过版本化 JSON 文本、上下文文件路径或订单 ID 传递 |
| **禁止直接操作配置文件** | 所有配置读写必须通过 ConfigCenter.dll 接口 |
| **禁止直接操作数据库（UI/业务流程）** | UI、Workflow、主业务流程不得绕过 Service/DAO 直接访问 Database.dll 执行业务 SQL；Database.dll 作为通用基础设施，只提供连接、事务和裸 SQL 能力，允许主 EXE/测试宿主做健康检查，允许 Service 内部 DAO 调用 |
| **禁止 UI 承载业务逻辑** | UI 模块仅做展示，业务逻辑必须下沉到 Service 层 |
| **禁止 UI 模块互相切换页面** | HomeUI、CaseUI、OrderCreateUI 等 UI DLL 只上报入口/操作 ID；跨页面切换由 MeyerScan.exe 统一处理，防止 UI DLL 之间形成循环依赖 |
| **禁止普通导航 close/show 顶层窗口** | 首页、浏览、建单、设置等主页面由 MainExe 单内容区容器替换显示，一次只挂载一个全屏页面；首页和浏览不是并列兄弟页；是否缓存或释放由 MainExe 按资源重量决定，避免 close/show 顶层窗口造成闪现 |
| **禁止用 currentPath 取运行资源** | 日志、配置、图标、语言、版本清单必须基于应用安装目录或 ConfigCenter 返回路径，禁止使用 `QDir::currentPath()` 作为运行资源路径 |
| **禁止运行期开发路径回退** | MeyerScan.exe、测试宿主和各 UI 模块运行参数不得回退到 `D:/wj`、`F:/MeyerScan` 等开发机绝对路径；开发期依赖来源只允许存在于 vcxproj/PostBuild 或开发说明中 |
| **禁止按语言硬编码布局 if/else** | 多语言文本长度差异必须通过 Layout、最小宽度、伸缩、换行、省略号/tooltip 处理，不能按语言写位置和尺寸分支 |
| **禁止过早拆弹窗模块** | 普通弹窗、Toast、确认框、等待页先归入 UIComponents；只有形成独立业务流程或独立发布价值时才考虑单独模块 |
| **禁止只靠 UI 做功能阉割** | 入口隐藏只能改善体验，安全边界必须在 Permission、Workflow、Service、IPC 接收端重复校验 |
| **禁止公共方法泛化** | 公共代码只有满足“无状态、无业务、三处以上真实复用、变化较低”才允许进入公共头/静态库；否则归入对应领域模块 |
| **非界面模块默认不依赖 Qt** | 新增非界面模块先评估纯 C++ 实现；确需 Qt JSON 等能力时必须说明原因，并优先限制在私有实现内；Database 已经纯 C++，Qt 类型转换统一放在 DatabaseQtAdapter |
| **接口只增不改** | 新增功能通过扩展接口实现，不修改已有接口签名 |

### 2.4 细模块归类与拆分判定

为了让后期人工维护和扩展成本最低，模块按“变化原因”归类，而不是按界面菜单简单归类。一个模块只应该因为一种主要原因变化。

| 类别 | 模块 | 拆分原则 |
|------|------|----------|
| **壳与导航** | MeyerScan.exe、HomeUI.dll、Login.dll、OrderScanWorkspaceShell.dll | 只负责启动、模块加载、入口导航、登录跳转、建单/扫描工作台容器，不承载业务规则 |
| **病例域服务** | CaseOrderService.dll、RuntimeDataCenter.dll、ScanSchemaService.dll、OrderWorkflowService.dll | 患者/订单保存和状态变更归 CaseOrderService；高频变化的本地/云端只读上下文归 RuntimeDataCenter；扫描方案和流程规则独立，避免数据库域服务、读模型缓存和流程规则混在一起 |
| **病例域 UI** | CaseUI.dll、OrderCreateUI.dll | 只做列表、详情、表单、页面状态展示；保存和跳转规则都调用服务 |
| **权限与定制** | Permission.dll、PermissionConfigUI.dll、UIComponents.dll、ConfigCenter.dll | 权限核心、权限配置 UI、通用控件、配置入口分离 |
| **数据基础设施** | Database.dll、DatabaseQtAdapter.dll、Crypto.dll、Logger.dll；患者/订单合同由 CaseOrderService 内维护 | Database 做裸 SQL；Adapter 只做 Qt/C++ 类型转换；高频业务字段不进入公共静态 ABI |
| **设备与扫描** | DeviceTransport.dll、DeviceCmd.dll、ScanDataIO.dll、ScanDataPreProcess.dll、ScanWorkflowUI.dll、DataProcessUI.dll、SendUI.dll、ScanReconstructStudio.exe | 传输、命令、数据 IO、预处理、扫描阶段 UI、数据处理阶段 UI、发送页 UI、独立扫描壳分离 |
| **外部集成** | NetworkHelper.dll、DataExport.dll、Statistics.dll、ExternalLaunchAdapter.dll、HisWorklistAdapter.dll、MyUpdate.exe | 云端、导出、统计、第三方拉起、HIS/Worklist、自动更新按职责独立 |
| **校准与设置** | Calibration3DUI.dll、CalibrationColorUI.dll、SettingsUI.dll、EngineeringSettings.dll | 三维校准和颜色校准分别自带 UI/流程/计算入口，用户设置 UI 独立，工程设置独立 |
| **发布交付** | MyInstaller/Packaging | 安装包生成、安装界面/流程、目录层级和依赖收集与运行时业务分离 |

#### 2.4.0 运行时数据与业务服务边界

本地数据库和云端上下文里有一类信息字段变化频繁，例如本地诊所、技工所、软件、医生、设置、账号、患者、订单、设备，以及云端诊所登录信息、云端诊所基本信息、云端合作技工所、云端设备信息等。该类信息不适合用到处传递的固定 C++ 结构体作为长期公共 ABI。

当前采用 `RuntimeDataCenter.dll` 承接运行时读模型：

| 类型 | 入口 | 说明 |
|------|------|------|
| 本地快照 | `GetDomainJson("local.xxx")` | 从 Database 内部读取白名单旧表或后续新表，包装为带 `schemaVersion/domain/items` 的 UTF-8 JSON |
| 云端诊所快照 | `UpdateCloudClinicJson()` + `GetDomainJson("cloud.clinicProfile")` | 登录/云端同步模块负责联网和鉴权，RuntimeDataCenter 只缓存已经拿到的 JSON |
| 患者/订单保存和状态变更 | `CaseOrderService.dll` | 保存、删除、编辑、状态变化、字段校验、权限复核仍归业务服务 |

设计原则：

1. UI、MainExe 和工作台不传 SQL、不传旧表名，只读稳定 domain。
2. JSON 快照允许字段自然增删，调用方按字段 key 读取自己需要的内容，不因为新增字段改 ABI。
3. RuntimeDataCenter 不替代 CaseOrderService；前者是读模型和上下文缓存，后者是业务 CRUD 和一致性边界。
4. 跨进程仍只传 UTF-8 JSON 字节、上下文文件路径或订单 ID，不传 `QJsonObject` / `QString` 对象指针。
5. RuntimeDataCenter 的快照有明确容量上限，只适合本地诊所、技工所、医生、账号、设备、患者/订单列表等轻量上下文；记录量过大、字段过重或需要分页搜索时，必须改为 `CaseOrderService.QueryJson()` / 专用 Service 分页接口，不能把大表无限制塞进运行时缓存。
6. 默认 SQLite 链路只代表数据库连接形态切换完成，不代表旧 MySQL 表结构已自动迁移；业务 schema 创建、旧数据迁移、字段映射版本化必须由 CaseOrderService/migration 负责，RuntimeDataCenter 不执行旧 `mysql.sql`。

#### 2.4.1 拆分模块总清单（中文名 / 项目名 / 产物名 / 功能详情）

> **命名规则**：项目名优先使用 `F:\MeyerScan` 仓库下一级目录；运行产物使用明确的 DLL/EXE/LIB 名称。已落地模块按当前项目名记录，规划模块按建议项目名记录，后续创建项目时不得随意改名，确需改名必须同步更新本清单和模块内 `README.md` / `CHANGELOG.md`。

| 模块中文名 | 项目名 / 目录 | DLL / EXE / LIB 名 | 形态 | 功能详情 | 边界与注意事项 |
|------------|---------------|--------------------|------|----------|----------------|
| 主程序壳 | `MyMainExe` | `MeyerScan.exe` | 主 EXE | 软件唯一用户入口；创建 `QApplication`；单实例控制；启动等待页；最早动态加载 Logger；运行时动态加载 ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter 和各 UI/流程 DLL；当前阶段由 MainExe 内部生成运行时版本清单；调用登录模块；统一管理首页、案例管理、建单、设置等主页面切换；启动或激活扫描重建进程；记录跨模块导航日志。 | 只做启动、编排、窗口容器、轻量版本清单和进程调度；不写业务 SQL、不写订单规则、不写扫描算法、不直接控制设备协议；自研 DLL 通过 `QLibrary + C ABI 工厂函数` 获取接口，避免 MainExe 链接大量 import lib。 |
| 登录测试宿主 / 登录适配入口 | `MyLogin`；后续可新增 `MyLoginAdapter` | 当前调用既有 `MeyerLoginWidget.dll`，测试宿主为 `MeyerLoginTest.exe`；后续建议产物 `MeyerScan_LoginAdapter.dll` | DLL / 测试 EXE | 当前负责验证既有登录 DLL 可被新仓库拉起；后续 LoginAdapter 负责组装登录参数、转换登录返回状态、隔离外部登录头文件编码/字段变化；支持云端账号登录、离线许可、联网/离线状态返回。 | 不重写既有登录业务；不做病例管理、云端上传细节；MainExe 不应长期直接理解登录模块内部字段。 |
| 日志模块 | `MyLogger` | `MeyerScan_Logger.dll`，测试宿主 `LoggerTest.exe` | DLL | 结构化日志写入、日志级别过滤、每天主文件 + 超限尾号分卷、逐条同步写入/刷盘/关闭句柄、多线程/多进程安全写入、支持模块缓存 `ILogger* m_logger` 后持续 `Write()`；供 MainExe、Database、UI、服务、扫描进程统一记录操作链路。 | 基础设施模块，不承载业务规则；日志目录由调用方传入，固定写入 `MeyerScan.exe` 同级 `logs`；默认文件 `MeyerScan_YYYYMMDD.log`，超过大小上限后生成 `_NNN`；空上下文字段必须传 `""` 并由 Logger 省略，不做 `-` 占位；Logger.dll 本体不依赖 Qt，Qt 模块通过头文件内联层使用 `QString`。 |
| 数据库模块 | `MyDatabase` | `MeyerScan_Database.dll`，测试宿主 `DatabaseTest.exe` | DLL | 纯 C++ 基础设施；读取 `config/db_config.json`；当前默认数据库类型为 SQLite；SQLite 通过动态加载 `sqlite3.dll` 调 C API；保留 MySQL 配置与类型枚举，原生 MySQL C API 待 SDK 接入；提供裸 SQL 执行、事务、备份、脚本执行、通用 SELECT 结果 JSON 化；相对路径按配置文件所在目录解析；SQLite 首次连接前自动创建数据库文件父目录。 | 不依赖 Qt，不理解患者、订单、医生、诊所、技工所、权限语义；不做业务 CRUD；不自动加载旧 `mysql.sql`；业务表结构由服务/迁移模块调用 Database 执行。 |
| 数据库 Qt 适配层 | `MyDatabaseQtAdapter` | `MeyerScan_DatabaseQtAdapter.dll` | DLL | Qt 模块访问纯 C++ Database 的转换层；把 `QString` SQL/路径转 UTF-8；把 Database 通用 JSON 转 `QJsonDocument` / `QJsonArray`；统一处理调用方缓冲区扩容；缓存 `ILogger*` 并记录适配层生命周期和失败边界日志。 | 不做业务 CRUD，不理解字段含义，不连接数据库，不被 Database 反向依赖；UI/Service 不得绕过它直接做 Qt/Database 转换；正常高频查询成功不逐条写日志。 |
| 运行时数据中心 | `MyRuntimeDataCenter` | `MeyerScan_RuntimeDataCenter.dll`，测试宿主 `RuntimeDataCenterTest.exe` | DLL | 本地常用数据库信息和云端诊所信息的运行时 JSON 快照；本地 domain 包括 `local.clinics`、`local.labs`、`local.software`、`local.doctors`、`local.settings`、`local.users`、`local.orders`、`local.patients`、`local.devices`；云端 domain 当前为 `cloud.clinicProfile`；MainExe 启动后初始化，CaseUI 先通过它读取患者/订单列表。 | 只做读模型/缓存；不做 UI、不做业务 CRUD、不做权限判断、不做云端请求；不允许调用方传 SQL 或表名；字段高频变化通过 JSON 快照和 schemaVersion 吸收，不让 UI/主程序频繁改 ABI；快照有容量上限，超出后必须改分页/服务查询。 |
| 配置中心 | `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | DLL | 读取和生成 `config/runtime_config.json`；提供布尔、整数、字符串配置读取；后续扩展配置版本、迁移、校验、配置变更通知、加密配置读取接口。 | 不做权限判断、不做数据库 CRUD、不写业务流程；配置只提供产品/客户默认策略。 |
| 权限核心 | `MyPermission` | `MeyerScan_Permission.dll` | DLL | 读取 `config/permission_rules.json`；提供功能 visible / enabled 查询；后续实现角色、客户、设备机型、设备序列号/加密狗、软件版本、时间有效期、配置方案等六维权限快照和高价值动作复核。 | 不做 UI、不直接操作业务数据库；不能只靠 UI 隐藏，Service/Workflow/IPC 必须复核。 |
| 共享 UI 组件 | `MyUIComponents` | `MeyerScan_UIComponents.dll` | DLL | 提供等待页、页面标题、字段标签、按钮、输入框、下拉框、日期框、多行文本框、基础表格等控件工厂；当前按钮样式按“角色 + 内容布局”统一管理，角色包括 Primary/Secondary/Text/Danger/Entry，内容布局包括纯文字、纯图标、左图右文、上图下文；基础表格统一表头、边框、隔行色、只读默认行为和整行选择；后续扩展公共弹窗、Toast、复杂表格能力、日历、主题、DPI/Layout 规则、LanguageManager。 | 只统一视觉、尺寸、多语言和基础交互体验；不承载业务按钮行为、不读取配置、不做权限判断、不决定页面跳转；表格列名、数据、分页、排序、右键菜单和双击动作由业务模块负责；仅单个模块使用的截图还原控件或特殊业务控件留在自身模块内部；公共虚接口新增方法只能追加到接口末尾，不能插入旧方法中间，避免破坏已编译模块的 vtable 顺序。 |
| 统一 UI 资源 | `MyUIResources` | `MeyerScan_UIResources.dll`，测试宿主 `UIResourcesTest.exe` | DLL / 资源提供器 | 自动扫描各模块 `Resources`，用 qrc + `rcc -binary` + Win32 `RCDATA` 聚合 PNG/QSS/qm 等只读资源；进程内注册后提供 `:/MeyerScan/Modules/<ProjectName>/...` 路径。 | 不创建控件、不承载业务、不决定语言；不把资源源码搬离所属模块。DLL 化只能防普通误改/误删，不是加密；完整性由版本清单、哈希/签名、安装包校验和修复负责。 |
| 版本清单能力 | 当前在 `MyMainExe` 内置；`MyVersionManager` 暂保留历史骨架 | 当前由 `MeyerScan.exe` 生成；历史骨架为 `MeyerScan_VersionManager.dll` | 主 EXE 内置 / 后续可再拆 DLL | 当前阶段启动时由 MainExe 读取 `config/version_modules.json`，只记录清单中声明的 MeyerScan 拆分模块 EXE/DLL，读取 Windows 文件版本、模块代码版本、大小、修改时间，生成 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`；供售后排查、安装打包、自动更新复用。 | 不扫描运行目录全部 DLL，不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库；manifest 使用 `file + versionFunction` 结构，自研 DLL 固定导出 `GetMeyerModuleVersion()`，输出 `fileVersion` / `codeVersion` / `versionMatch`；文件名包含毫秒，避免同一秒内多次启动覆盖快照；`MyVersionManager` 历史骨架也使用同一 manifest 规则，避免后续误用旧模块时口径漂移。若后续扩展到算法 DLL 哈希、签名校验、云端比对、自动更新策略，再恢复独立模块。 |
| 公共稳定契约候选 | 暂不单独建项目 | 按需公共头/静态库 | 候选 / 非前置 | 仅在 ErrorCode/Result、稳定动作 ID、IPC 消息头等出现真实重复后抽取。 | 禁止形成 Core/Helper 大包。 |
| 患者订单数据合同 | `MyCaseOrderService` 内维护 | 服务 DTO + UTF-8 JSON | 服务内契约 | 使用 `schemaVersion`、稳定字段 key、`extensions` 和 schema 映射吸收字段变化。 | 当前不生成 CaseEntity.lib，不把高频变化字段固化为跨 DLL ABI。 |
| 病例订单服务 | `MyCaseOrderService` | `MeyerScan_CaseOrderService.dll` | DLL | 患者/订单组合数据 CRUD、查询、状态管理、操作留痕；医生、技工所、诊所等数据库主数据/字典数据读写；字段版本、扩展字段和 schema 映射；向 CaseUI、OrderCreateUI、ExternalLaunchAdapter、HisWorklistAdapter 提供标准接口。 | 不做 UI 渲染、扫描方案、扫描采集、算法处理、设备通信和加载订单流程决策；替代原 `CaseService.dll` + `OrderService.dll`。 |
| 扫描方案服务 | `MyScanSchemaService`（规划） | `MeyerScan_ScanSchemaService.dll` | DLL | 扫描方案、牙位、修复体、材料、齿色、咬合/上下颌等结构化数据保存和读取；为建单、继续扫描、扫描重建提供标准方案数据。 | 不做相机采集、不做 3D 重建、不做 UI 表单展示。 |
| 加载订单规则服务 | `MyOrderWorkflowService`（规划） | `MeyerScan_OrderWorkflowService.dll` | DLL | 统一判断新建、加载、继续扫描、进入处理、进入发送；综合订单状态、扫描方案、数据文件完整性、权限、客户配置、机型、软件版本后输出决策。 | 不保存数据、不渲染 UI；HomeUI、CaseUI、OrderCreateUI、ScanReconstructStudio 不得重复实现这套规则。 |
| 首页 UI | `MyHomeUI` | `MeyerScan_HomeUI.dll`，测试宿主 `HomeUITest.exe` | DLL | 首页入口展示；Create / Browse / Practice / Settings 等入口状态；OEM 首页布局、Logo、主题、入口显隐；入口点击通过回调上报 MainExe。 | 只做入口 UI；不做建单规则、不判断加载订单、不直接决定扫描流程、不做权限核心判断。 |
| 案例管理 UI | `MyCaseUI` | `MeyerScan_CaseUI.dll`，测试宿主 `CaseUITest.exe` | DLL | 患者/订单列表、搜索、导入导出按钮、删除按钮、打开订单按钮、返回首页按钮、页签切换；模块内客户操作日志；通过操作 ID 上报 MainExe；当前患者/订单列表先读取 RuntimeDataCenter 的 JSON 快照。 | 正式业务不得直接访问 Database；列表读模型可走 RuntimeDataCenter，搜索/CRUD/保存必须走 CaseOrderService/DataExport；打开订单必须走 OrderWorkflowService 和 MainExe 扫描前资源释放流程。 |
| 建单 UI | `MyOrderCreateUI` | `MeyerScan_OrderCreateUI.dll`，测试宿主 `OrderCreateUITest.exe` | DLL | 当前 v0.5.2 提供三栏自适应建单字段、五种修复类型、治疗方案图片/mask 交互、相邻牙桥连接点、扫描流程输入和三档截图验收；前四种类型以 b 图主色合成局部圆底后叠加白色 h 图，种植体由 QSS 绘制整行状态；扫描流程预览固定高度，不改变牙弓几何。 | 只提供 Order 步骤内容，不保存数据库、不决定流程、不把 bridge 当修复类型、不绘制工作台步骤导航；后续保存走 CaseOrderService / ScanSchemaService / OrderWorkflowService。 |
| 建单扫描工作台壳 | `MyOrderScanWorkspaceShell` | `MeyerScan_OrderScanWorkspaceShell.dll` | DLL / UI 容器 | 当前 v0.1.2 提供品牌、返回、唯一的 Order/Scan/Process/Send 步骤导航、最小化/关闭入口和步骤页面容器；练习模式只显示 Scan/Process。 | 只做容器和导航；内容页不得复制工作台步骤条；窗口动作上报 MainExe 执行。 |
| 发送 UI | `MySendUI` | `MeyerScan_SendUI.dll`，测试宿主 `SendUITest.exe` | DLL / UI | 工作台 Send 步骤页面；显示患者/医生/订单编号/订单类型/诊所/数据格式/备注等案例信息；提供本地 `Export`、`Compress` 和技工所 `Email Send`、`Upload` 动作按钮，以及 `Previous` / `Finish` 流程按钮；按钮动作通过 `ISendUI` 回调上报 MainExe；当前 v0.1.1 已接入 UIComponents、UIResources、Logger、版本导出、VS2015/CMake 工程和测试宿主。 | 只做发送页展示、上下文字段填充、动作回调和日志；不直接访问数据库、不直接打包数据、不联网、不发送邮件、不上传云端；真实导出/压缩/邮件/上传后续进入 DataExport、NetworkHelper 或专用发送服务。 |
| 第三方拉起适配 | `MyExternalLaunchAdapter` | `MeyerScan_ExternalLaunchAdapter.dll`，测试宿主 `ExternalLaunchAdapterTest.exe` | DLL | 支持美亚美牙等第三方软件启动本地口扫软件；当前已支持 `MeyerScan.exe --external-order <json> --external-order-type <type>` 命令行模拟第三方拉起；读取第三方 JSON 后输出标准建单上下文 `source / patient / order / scanPlan`，其中 `source.thirdPartyType`、`source.thirdPartyName`、`sourceSystem`、`sourceVersion` 必须保留，用于区分多个第三方来源和后续字段映射规则。 | 不渲染 UI、不保存数据库、不直接调用 OrderCreateUI、不启动扫描进程；新增第三方时优先改本适配器和映射规则，不改 MainExe、OrderCreateUI 和扫描进程。 |
| HIS / Worklist 适配 | `MyHisWorklistAdapter`（规划） | `MeyerScan_HisWorklistAdapter.dll` | DLL | 对接公立医院或大型连锁 HIS/Worklist；查询患者列表、下拉选择患者信息、字段映射、接口异常处理。 | 不做建单 UI、不直接写病例表；只输出标准建单上下文给 OrderCreateUI/服务层。 |
| 权限配置 UI | `MyPermissionConfigUI`（规划） | `MeyerScan_PermissionConfigUI.dll` | DLL | 授权状态展示、扫码授权入口、权限配置界面、规则导入/替换结果展示；调用 Permission 核心接口。 | 不实现权限核心判断；不保存未校验权限规则；避免污染 Permission 热路径。 |
| 二维码授权入口 | `MyQRCodeAuthEntry`（规划） | `MeyerScan_QRCodeAuthEntry.dll` | DLL | 扫码授权入口、二维码内容读取、授权配置导入前的 UI/入口适配；向 PermissionConfigUI/Permission 提交待校验内容。 | 不自行决定授权是否生效；不绕过 Permission 验签和版本检查。 |
| 云端网络模块 | `MyNetworkHelper` / 既有网络模块 | 既有 `MeyerScanNetworkHelper.dll`；后续建议 `MeyerScan_NetworkHelper.dll` | DLL | 口扫云登录相关网络能力、订单上传/下载、同步、邮件发送、云端配置拉取、更新配置拉取等网络基础能力。 | 不做病例 CRUD；业务数据由 Service/DataExport 组织后交给网络模块；不把 UI 状态写进网络层。 |
| 数据导入导出 | `MyDataExport`（规划） | `MeyerScan_DataExport.dll` | DLL | 患者/订单/扫描数据导入导出、数据打包、发送前数据整理、第三方/云端格式转换。 | 不绕过 CaseOrderService 拼业务语义；不直接决定发送流程。 |
| 统计报表 | `MyStatistics`（规划） | `MeyerScan_Statistics.dll` | DLL | 患者数量、订单数量、设备使用、扫描次数、云端上传等统计查询和报表数据准备。 | 只读或聚合统计，不修改业务状态；复杂报表 UI 不放在服务内。 |
| 自动更新 | `MyUpdate`（规划） | `MyUpdate.exe` | 独立 EXE | 支持 MeyerScan 内“检查更新”和用户双击主动更新；读取本地 `myLocalUpdate.json/xml` 和云端 `MyCloudUpdate.json/xml`；比对版本、显存/驱动/内存、账号白名单、设备白名单；下载补丁；关闭 MeyerScan.exe；覆盖升级；重启主程序。 | 必须与 MeyerScan.exe 同级且独立运行；不做病例、扫描、登录业务；升级策略不写进主 EXE。 |
| 安装打包 | `MyInstaller` / `Packaging`（规划） | 安装包 / `MeyerScanInstaller.exe`（待定） | 发布交付模块 | 收集 MeyerScan.exe、MyUpdate.exe、ScanReconstructStudio.exe、插件 DLL、Qt 运行库、Qt 插件、配置模板、资源、帮助文档和版本清单；生成安装包；自定义安装界面、安装流程、目录层级。 | 不作为运行时插件；不做业务逻辑；安装包依赖必须和 MainExe 运行时版本清单及 Release 输出清单核对。 |
| 加解密模块 | `MyCrypto`（规划，当前可复用既有模块） | 当前既有 `deviceAuthAndCrypto.dll`；后续建议 `MeyerScan_Crypto.dll` | DLL | 配置加解密、权限规则验签、设备授权、敏感字段加解密、离线许可相关基础能力。 | 不做权限策略本身、不做 UI；ConfigCenter/Permission/Update 通过稳定接口调用。 |
| 设备传输层 | `MyDeviceTransport`（规划/已有可复用） | `MeyerScan_DeviceTransport.dll` | DLL | USB/串口/网口连接管理、数据收发、断线重连、传输错误码、底层通信日志。 | 不解析业务命令、不做扫描流程；通信方式变化不影响 DeviceCmd。 |
| 设备命令层 | `MyDeviceCmd`（规划/已有可复用） | `MeyerScan_DeviceCmd.dll` | DLL | 设备命令组装、解析、校验、状态查询、固件/相机/扫描参数命令封装。 | 不做底层传输实现；不做 UI；命令通过 DeviceTransport 发送。 |
| 三维校准 UI | `MyCalibration3DUI` | `MeyerScan_Calibration3DUI.dll` | DLL | 三维校准界面、标定器流程、25 幅采集编排、三维校准计算入口、结果展示和记录；可调用算法 DLL、DeviceCmd 和 DeviceTransport；当前已创建 v0.1.0 Qt Widgets 骨架。 | 不做颜色校准；不做病例/订单数据维护；UI 和计算入口在本模块内保持一致。 |
| 颜色校准 UI | `MyCalibrationColorUI` | `MeyerScan_CalibrationColorUI.dll` | DLL | 颜色校准界面、颜色标定器流程、颜色采集、颜色校正参数生成、结果展示和记录；可调用算法 DLL、DeviceCmd 和 DeviceTransport；当前已创建 v0.1.0 Qt Widgets 骨架。 | 不做三维校准；不做病例/订单数据维护；UI 和计算入口在本模块内保持一致。 |
| 扫描数据 IO | `MyScanDataIO`（规划） | `MeyerScan_ScanDataIO.dll` | DLL | 原始帧、深度图、纹理图、扫描中间数据的保存/读取；完整性校验；数据目录结构管理。 | 不做图像处理、不做 UI、不决定订单状态。 |
| 扫描预处理 | `MyScanDataPreProcess`（规划） | `MeyerScan_ScanDataPreProcess.dll` | DLL | 解密、镜像、裁剪、颜色校准预处理、AI 消去、机型差异预处理流水线。 | 不做 3D 显示、不做病例管理、不直接写订单状态。 |
| 扫描阶段 UI | `MyScanWorkflowUI` | `MeyerScan_ScanWorkflowUI.dll`，测试宿主 `ScanWorkflowUITest.exe` | DLL / UI | `ScanReconstructStudio.exe` 内部“扫描”大阶段页面；当前 v0.2.1 提供扫描对象选择、右侧扫描工具、底部扫描控制、提示区和 VTK/QVTK 显示区；顶部扫描流程按钮从 session JSON 的 `scanProcess.steps` 渲染，创建模式由 OrderCreateUI 生成并经 MainExe 转发，练习模式回退默认流程；流程按钮必须有手型 hover、tooltip、选中态和点击切换当前扫描部位显示数据；QVTK 滚轮缩放以鼠标位置为中心并在边界内夹紧，不允许先越界再拉回；离开页面时通过 `DeactivateAndRelease()` 释放 QVTKWidget、VTK renderer、OpenGL/显存等重资源。 | 不保存患者/订单，不访问数据库，不判断加载订单规则；不解析建单页开关、不生成扫描流程规则，只消费 `scanProcess.steps` 并渲染按钮；不直接实现设备传输、设备命令或算法；不跨 DLL/跨进程传递 VTK 对象、大块模型内存所有权、QObject 或 Qt 模型对象。 |
| 数据处理阶段 UI | `MyDataProcessUI` | `MeyerScan_DataProcessUI.dll`，测试宿主 `DataProcessUITest.exe` | DLL / UI | `ScanReconstructStudio.exe` 内部“数据处理”大阶段页面；当前 v0.2.1 提供模型选择栏、右侧处理工具栏、底部状态栏、独立的 Process Hint 提示框和 VTK/QVTK 显示区；顶部处理流程按钮从同一份 session JSON 的 `scanProcess.steps` 渲染，确保 Scan/Process 页面流程按钮一致；流程按钮必须有手型 hover、tooltip、选中态和点击切换当前处理部位显示数据；Process 页不放扫描页底部中间的 Start/Pause 控制；QVTK 滚轮缩放以鼠标位置为中心并在边界内夹紧；离开页面时通过 `DeactivateAndRelease()` 释放 QVTKWidget、VTK renderer、OpenGL/显存等重资源。 | 不连接设备，不做扫描采集；不解析建单页开关、不生成扫描流程规则，只消费 `scanProcess.steps` 并渲染按钮；不在 UI 模块内实现重算法；后续编辑、颈缘、测量、倒凹、咬合、底座、数据 IO 和预处理等能力优先拆成专用 DLL 或独立库。 |
| 扫描重建工作台 | `MyScanReconstructStudio` | `ScanReconstructStudio.exe` | 独立 EXE / 壳子 | 扫描重建独立进程壳；当前初版已动态加载 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll`，承载“扫描”和“数据处理”两个大阶段，并在阶段切换前调用离开模块的 `DeactivateAndRelease()`；后续通过 IPC 与 MeyerScan.exe / OrderScanWorkspaceShell 同步订单上下文、扫描状态和处理进度。 | EXE 只做壳子、UI/交互/流程编排和进程隔离；不做病例管理、云端上传、权限核心、设备协议、扫描算法或后处理算法；打开前 MainExe 必须释放 CaseUI 等非必要资源。 |
| 工程设置 | `MyEngineeringSettings`（规划） | `MeyerScan_EngineeringSettings.dll` | DLL | 设备参数设置、固件升级入口、扫描参数设置、数据处理参数设置、日志/诊断入口、高级维护工具入口。 | 不做数据库管理、不做权限核心；高风险操作必须经 Permission/Workflow 复核。 |
| 用户设置 UI | `MySettingsUI`（已落地） | `MeyerScan_SettingsUI.dll` | DLL | 软件设置界面，提供 General / Information / Calibration / Cloud / Scan / Data Processing / About 设置分类页面；嵌入 3D 校准和颜色校准模块 UI；通过回调上报设置动作给 MainExe。 | 只做用户设置 UI 和交互；不做设备参数设置（归 EngineeringSettings）；不做权限规则判断；设置持久化通过 ConfigCenter 间接完成。 |

**新增模块判定规则**：

1. 如果功能有独立交付、独立升级、独立客户定制需求，优先做 DLL。
2. 如果功能只是多个模块共享的数据结构、错误码、命令结构，放入 `.lib` 或头文件契约。
3. 如果功能只是 UI 视觉复用，放入 UIComponents.dll；不得放业务规则。
4. 如果功能同时被新建订单、加载订单、扫描进程、发送流程复用，优先抽成服务模块，而不是放在某个 UI 中。
5. 如果功能只在 ScanReconstructStudio.exe 的 UI/显示编排内部使用，可先做源码子目录；但编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等数据处理能力应优先抽成 DLL 或独立库，避免 UI/交互与业务/算法处理混在一个超大工程里。

**扫描流程创建边界**：

- 当前链路固定为 `OrderCreateUI 生成 scanProcess JSON -> MainExe 合并/转发 -> ScanWorkflowUI/DataProcessUI 只渲染 steps`。
- ScanWorkflowUI / DataProcessUI 不解析建单页开关，不复制流程规则，不保存患者/订单，也不直接判断加载订单规则。
- `Segmented scanbody` 只表示对应颌第二扫描杆/第二异性扫描杆是否显示；普通扫描杆流程仍由对应颌是否存在 `implant` 牙位触发，避免仅勾选分段就生成种植扫描流程。
- 后续如果扫描流程规则继续复杂化，应迁入 ScanSchemaService / OrderWorkflowService，不继续堆在 UI 页面或 MainExe 中。

### 2.5 轻量化拆分判定（轻量化团队适用）

用户提出“足够小、代码量足够少是否更容易人工维护”的判断是成立的，但有前提：**模块边界必须正确，接口必须少，依赖必须单向**。如果拆分只是把一个大流程切成多个互相回调的小 DLL，维护成本会更高。

| 判定项 | 推荐做法 | 反例 |
|--------|----------|------|
| **变化原因** | 一个模块只因一种主要原因变化 | 一个 CaseManager 同时因患者、订单、导出、统计、权限变化而频繁修改 |
| **代码量** | 单模块业务源码尽量控制在 3000-5000 行以内，单文件尽量低于 800 行 | 为了追求行数，把强相关的 5 个小类拆成 5 个 DLL |
| **接口数量** | 对外接口少而稳定，常用接口可在半页内读完 | 暴露几十个细碎 setter/getter，让调用方理解内部状态 |
| **依赖方向** | UI → Workflow/Service → Database；主 EXE → 插件；扫描进程通过 IPC 与主进程通信 | UI 直接调 Database，Service 反向调 UI，扫描进程依赖主窗口对象 |
| **测试入口** | 每个 DLL 有测试宿主、smoke 或最小命令行验证 | 模块只能在整机环境里调试 |
| **测试宿主构建** | 修改测试宿主源码后，必须单独构建对应测试 `.vcxproj` 并运行测试；聚合解决方案不等于所有测试 EXE 都已更新 | 只跑旧测试二进制造成“源码已改但验证仍是假通过” |
| **DLL 数量** | 有独立升级/替换/并行开发价值才 DLL 化 | 纯内部工具也 DLL 化，导致版本和部署成本上升 |

**拆分决策流程**：

1. 先问“这个功能为什么会变”：客户定制、业务规则、UI 表现、设备协议、算法实现、配置策略必须分开。
2. 再问“是否需要独立交付”：需要替换、升级、灰度、客户差异化的，优先 DLL。
3. 再问“是否只在一个模块内部使用”：如果是，先作为源码子目录或内部类，不急于 DLL 化。
4. 最后看调试成本：如果拆分后无法独立测试，或每次调试要跨 4 个模块单步，说明拆分过度。

**最终取舍**：

- 病例域服务拆细是必要的：患者、订单、扫描方案、流程规则、导出、统计的变化原因不同。
- UI 拆成 HomeUI、CaseUI、OrderCreateUI 是必要的：入口、列表/详情、建单表单的定制频率和职责不同。
- ScanReconstructStudio 保持独立 EXE 是必要的：VTK/PCL/OpenCV/算法崩溃风险高，必须进程隔离。
- ScanReconstructStudio 的 UI/显示/交互可以先用源码子目录保证调试效率；但编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等数据处理能力优先拆成 DLL 或独立库，不能长期混在工作台 UI 里。
- 公共契约必须按需且足够小：只有真实重复的稳定类型才抽取，公共业务规则和患者/订单高频字段不能进入公共层。

---

## 三、整体架构图

### 3.1 系统架构总览

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              MeyerScan.exe (主程序壳)                                 │
│  ┌───────────────────────────────────────────────────────────────────────────────   │
│  │ 功能: 插件加载器 + 进程调度 + 全局异常捕获 + UI 框架                              │
│  │ 禁止: 业务逻辑 / 算法 / 设备通信 / 数据库操作                                    │
│  └───────────────────────────────────────────────────────────────────────────────   │
│                                    │                                                  │
│                                    ▼                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────────────  │
│  │                           插件清单 (plugin_manifest.json)                        │
│  │  [                                                                               │
│  │    {"name": "Logger",       "path": "MeyerScan_Logger.dll",      "priority": 1}, │
│  │    {"name": "ConfigCenter", "path": "MeyerScan_ConfigCenter.dll", "priority": 2},│
│  │    {"name": "Crypto",       "path": "MeyerScan_Crypto.dll",      "priority": 3}, │
│  │    {"name": "Permission",   "path": "MeyerScan_Permission.dll",  "priority": 4}, │
│  │    {"name": "CaseOrderService", "path": "MeyerScan_CaseOrderService.dll", "priority": 5}, │
│  │    {"name": "ScanSchemaService","path": "MeyerScan_ScanSchemaService.dll","priority": 6}, │
│  │    {"name": "OrderWorkflow",    "path": "MeyerScan_OrderWorkflowService.dll","priority": 7}, │
│  │    {"name": "OrderScanShell",   "path": "MeyerScan_OrderScanWorkspaceShell.dll","priority": 8}, │
│  │    ...                                                                           │
│  │  ]                                                                               │
│  └─────────────────────────────────────────────────────────────────────────────────  │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ IPC (Named Pipe)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                         ScanReconstructStudio.exe (独立扫描进程)                      │
│  ┌───────────────────────────────────────────────────────────────────────────────   │
│  │ 功能: 扫描流程 + 图像预处理 + 算法重建 + 3D显示/编辑                             │
│  │ 特性: 进程隔离 + 状态同步 + 异常记录（心跳仅作状态检测）                         │
│  │ 依赖: Logger.dll + Core.lib (IPC) + Algorithm DLLs                             │
│  └───────────────────────────────────────────────────────────────────────────────   │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 模块依赖关系图

```
                          MeyerScan.exe (主进程)
                               │
        ┌──────────────────────┼──────────────────────┐
        ▼                      ▼                      ▼
   Logger.dll            ConfigCenter.dll       Permission.dll
   (最先加载)            (配置唯一入口)          (权限门禁)
        │                      │                      │
        └──────────────────────┼──────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        ▼                      ▼                      ▼
    Core.lib               Crypto.dll            UIComponents.dll
   (静态链接)             (加解密)               (通用UI控件)
        │                      │                      │
        └──────────────────────┼──────────────────────┘
                               │
   ┌───────────────────────────┼───────────────────────────┐
   ▼                           ▼                           ▼
CaseEntity.lib       CaseOrder/Schema/Workflow        CaseUI.dll
(数据契约)             (领域服务/流程规则)             (UI展示)
   │                           │                           │
   └───────────────────────────┼───────────────────────────┘
                               │
   ┌───────────────────────────┼───────────────────────────┐
   ▼                           ▼                           ▼
DeviceTransport.dll       DeviceCmd.dll        Calibration3DUI/ColorUI.dll
(传输层)                  (命令层)             (校准 UI + 流程 + 计算入口)
                               │
   ┌───────────────────────────┼───────────────────────────┐
   ▼                           ▼                           ▼
ScanDataIO.dll          ScanDataPreProcess.dll    NetworkHelper.dll
(数据存储)              (图像预处理)              (云端服务)
                               │
                               ▼
                ScanReconstructStudio.exe (独立进程)
                ├── 集成 VTK / PCL / OpenCV
                ├── 集成 Algorithm/ 下 4 个算法 DLL
                ├── 链接 Core.lib（IPC 通信）
                └── 加载 Logger.dll（统一日志）
```

### 3.3 加载时序图

```
时间线
  │
  │  [主程序启动]
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 1. 加载 Core.lib（静态链接，已嵌入主程序）                        │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 2. 加载 Logger.dll → GetLogger() → Init(logDir, Info)          │
  │  │    MEYER_LOG_INFO("Startup", "", "", "", "Main process started")│
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 3. 加载 ConfigCenter.dll → LoadConfig(configPath)              │
  │  │    读取: database.type, logger.logDir, scan.defaultOrderType... │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 4. 加载 Crypto.dll → Init(keyPath)                             │
  │  │    加载密钥配置，准备加解密服务                                   │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 5. 加载 Permission.dll → LoadRuleFile(rulePath)                │
  │  │    校验授权：CheckAccess("Startup") → 返回可访问功能列表          │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 6. 加载病例域服务 → Init(dbConfig)                              │
  │  │    CaseOrderService / ScanSchemaService / OrderWorkflowService   │
  │  │    初始化数据访问、扫描方案和流程规则服务                         │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 7. 加载 UIComponents.dll → InitTheme(themeName)                │
  │  │    加载 QSS 样式，初始化 ThemeManager                            │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 8. 加载业务 DLL（按 plugin_manifest.json 顺序）                  │
  │  │    HomeUI / CaseUI / Login / SettingsUI / OrderScanWorkspaceShell│
  │  │    / Calibration3DUI / CalibrationColorUI / EngineeringSettings...│
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  ┌───────────────────────────────────────────────────────────────┐
  │  │ 9. 显示主界面 → 首页（创建/浏览/练习/设置）                       │
  │  └───────────────────────────────────────────────────────────────┘
  │      │
  │      ▼
  │  [用户操作]
  │
  ▼
```

---

## 四、模块间入参/出参规范

### 4.1 跨 DLL 数据传递规范

| 数据类型 | 跨 DLL 传递 | 跨进程传递 | 说明 |
|----------|-------------|------------|------|
| `const char*` (UTF-8) | ✅ 允许 | ✅ 允许 | 最安全的传递方式 |
| `std::string` | ❌ 禁止（内联重载） | ❌ 禁止 | 仅在调用方编译时使用 |
| `QString` / `QJsonObject` / `QVariantMap` | ✅ 允许（同一进程、同一 Qt/编译环境、无跨 DLL 内存所有权时） | ❌ 禁止 | Qt 模块之间可用 Qt 类型提高开发效率；不能跨进程传对象或让另一模块释放内部内存 |
| POD 结构体 | ✅ 允许 | ✅ 允许 | 必须含版本号 + 预留字段 |
| `QObject` / 指针 | ❌ 禁止 | ❌ 禁止 | 跨进程禁止，跨 DLL 有风险 |
| `QList` / `QMap` | ✅ 允许（跨 DLL） | ❌ 禁止 | 仅在 Qt 模块间传递 |
| 枚举 (enum class) | ✅ 允许 | ✅ 允许 | 值为 int32，必须统一定义在 Core.lib |

**Qt 使用边界补充**：同一进程内由 MeyerScan 统一构建、统一 Qt 5.6.3 运行库加载的 UI 模块，可以使用 Qt Widgets、布局、信号槽和 Qt 容器组织界面流程。非界面模块不再默认鼓励使用 Qt；Database 当前已去 Qt，Qt 模块访问 Database 必须经 `MyDatabaseQtAdapter`。CaseOrderService/RuntimeDataCenter 当前因 Qt JSON 维持字段快照链路时，必须把 Qt 留在实现文件内部，公开接口继续使用 `const char*`、POD、调用方缓冲区和 UTF-8 JSON。跨进程、第三方插件 ABI、长期兼容接口和不确定调用方运行库的边界必须收敛为 POD/UTF-8/文件路径/订单 ID。

### 4.2 POD 结构体通用模板

```cpp
// 所有跨进程/跨 DLL 传递的结构体必须遵循此模板
struct PodStructTemplate {
    // 必须放在第一个字段，便于版本校验和迁移
    uint32_t version;           // 结构体版本号，初始为 1，每次新增字段递增

    // 业务字段...
    int32_t  fieldA;
    double   fieldB;
    char     fixedString[64];   // 固定大小字符串，禁止使用指针
    char     fixedArray[256];   // 固定大小数组

    // 预留扩展字段（便于后续扩展）
    int32_t  reserved[8];
    char     extData[256];
};

// 序列化时必须包含版本号
// 接收方根据版本号判断字段有效性
```

---

## 五、公共类型定义

### 5.1 统一错误码定义

```cpp
// =============================================================================
// 文件:    ErrorCode.h
// 模块:    MeyerScan_Core.lib
// 用途:    统一错误码定义，所有模块使用此枚举返回操作结果
// =============================================================================

#pragma once

#include <cstdint>

namespace MeyerScan {

// 错误码分类：
//   0xxxx: 通用错误（Core.lib 定义）
//   1xxxx: 数据库/病例域相关（Database 与病例域服务定义）
//   2xxxx: 设备相关（DeviceCmd/DeviceTransport 定义）
//   3xxxx: 网络相关（NetworkHelper 定义）
//   4xxxx: 权限相关（Permission 定义）
//   5xxxx: 加密相关（Crypto 定义）
//   6xxxx: 配置相关（ConfigCenter 定义）
//   7xxxx: 扫描相关（ScanReconstructStudio 定义）
//   8xxxx: 校准相关（Calibration 定义）
//   9xxxx: UI 相关（各 UI 模块定义）

enum class ErrorCode : int32_t {
    // ========== 通用错误 0xxxx ==========
    Success                 = 0,      // 操作成功
    UnknownError            = 1,      // 未知错误
    InvalidParameter        = 2,      // 参数无效
    NullPointer             = 3,      // 空指针
    OutOfMemory             = 4,      // 内存不足
    NotInitialized          = 5,      // 未初始化
    AlreadyInitialized      = 6,      // 已初始化（重复初始化）
    Timeout                 = 7,      // 超时
    OperationCancelled      = 8,      // 操作被取消
    NotImplemented          = 9,      // 功能未实现
    VersionMismatch         = 10,     // 版本不匹配

    // ========== 数据库错误 1xxxx ==========
    DbConnectionFailed      = 10001,  // 数据库连接失败
    DbQueryFailed           = 10002,  // SQL 查询失败
    DbInsertFailed          = 10003,  // 插入失败
    DbUpdateFailed          = 10004,  // 更新失败
    DbDeleteFailed          = 10005,  // 删除失败
    DbRecordNotFound        = 10006,  // 记录不存在
    DbDuplicateKey          = 10007,  // 主键/唯一键冲突
    DbTransactionFailed     = 10008,  // 事务失败
    DbMigrationFailed       = 10009,  // 数据库迁移失败

    // ========== 设备错误 2xxxx ==========
    DeviceNotConnected      = 20001,  // 设备未连接
    DeviceConnectionLost    = 20002,  // 设备连接断开
    DeviceTimeout           = 20003,  // 设备响应超时
    DeviceCommandFailed     = 20004,  // 设备命令执行失败
    DeviceFirmwareUpdateFailed = 20005, // 固件升级失败
    DeviceCalibrationFailed = 20006,  // 设备校准失败
    DeviceHardwareError     = 20007,  // 硬件错误
    DeviceProtocolError     = 20008,  // 协议解析错误

    // ========== 网络错误 3xxxx ==========
    NetworkConnectionFailed = 30001,  // 网络连接失败
    NetworkTimeout          = 30002,  // 网络超时
    NetworkUploadFailed     = 30003,  // 上传失败
    NetworkDownloadFailed   = 30004,  // 下载失败
    NetworkAuthFailed       = 30005,  // 认证失败
    NetworkServerError      = 30006,  // 服务器错误
    EmailSendFailed         = 30007,  // 邮件发送失败

    // ========== 权限错误 4xxxx ==========
    PermissionDenied        = 40001,  // 权限不足
    LicenseExpired          = 40002,  // 授权过期
    LicenseInvalid          = 40003,  // 授权无效
    FeatureNotAuthorized    = 40004,  // 功能未授权
    RuleFileInvalid         = 40005,  // 权限规则文件无效
    QRCodeAuthFailed        = 40006,  // 扫码认证失败

    // ========== 加密错误 5xxxx ==========
    CryptoEncryptFailed     = 50001,  // 加密失败
    CryptoDecryptFailed     = 50002,  // 解密失败
    CryptoKeyNotFound       = 50003,  // 密钥未找到
    CryptoKeyExpired        = 50004,  // 密钥过期
    CryptoHashFailed        = 50005,  // 哈希计算失败
    CryptoSignatureFailed   = 50006,  // 签名验证失败

    // ========== 配置错误 6xxxx ==========
    ConfigFileNotFound      = 60001,  // 配置文件不存在
    ConfigParseError        = 60002,  // 配置解析错误
    ConfigValueInvalid      = 60003,  // 配置值无效
    ConfigMigrationFailed   = 60004,  // 配置迁移失败
    ConfigBackupFailed      = 60005,  // 配置备份失败

    // ========== 扫描错误 7xxxx ==========
    ScanNotStarted          = 70001,  // 扫描未启动
    ScanAlreadyRunning      = 70002,  // 扫描已在进行
    ScanFrameCaptureFailed  = 70003,  // 帧采集失败
    ScanProcessInterrupted  = 70004,  // 扫描被中断
    ScanReconstructFailed   = 70005,  // 重建失败
    ScanDataCorrupted       = 70006,  // 数据损坏
    ScanCalibrationRequired = 70007,  // 需要校准
    ScanDeviceNotReady      = 70008,  // 设备未就绪

    // ========== 校准错误 8xxxx ==========
    CalibNotStarted         = 80001,  // 校准未启动
    CalibFrameCountNotEnough = 80002, // 采集帧数不足（需25幅）
    CalibQualityNotPass     = 80003,  // 校准质量不达标
    CalibColorFailed        = 80004,  // 颜色校准失败
    Calib3DFailed           = 80005,  // 三维校准失败
    CalibCalibratorNotConnected = 80006, // 标定器未连接

    // ========== UI 错误 9xxxx ==========
    UIResourceNotFound      = 90001,  // UI 资源未找到
    UIStyleLoadFailed       = 90002,  // 样式加载失败
    UILanguageFileNotFound  = 90003,  // 语言文件未找到
    UIComponentInitFailed   = 90004,  // UI 组件初始化失败
};

// 错误码辅助函数
inline bool IsSuccess(ErrorCode code) {
    return code == ErrorCode::Success;
}

inline bool IsError(ErrorCode code) {
    return code != ErrorCode::Success;
}

inline int32_t GetErrorCategory(ErrorCode code) {
    return static_cast<int32_t>(code) / 10000;
}

} // namespace MeyerScan
```

### 5.2 统一结果包装结构

```cpp
// =============================================================================
// 文件:    Result.h
// 模块:    MeyerScan_Core.lib
// 用途:    统一操作结果包装，包含错误码和可选数据
// =============================================================================

#pragma once

#include "ErrorCode.h"
#include <utility>

namespace MeyerScan {

// 泛型结果包装，用于所有 Service 层接口返回
template<typename T>
struct Result {
    ErrorCode   error;      // 错误码（Success 表示成功）
    T           data;       // 返回数据（成功时有效）
    const char* message;    // 错误描述（失败时可选）

    // 构造函数
    Result() : error(ErrorCode::UnknownError), message(nullptr) {}

    Result(ErrorCode err, const char* msg = nullptr)
        : error(err), message(msg) {}

    Result(T value)
        : error(ErrorCode::Success), data(std::move(value)), message(nullptr) {}

    // 状态检查
    bool IsSuccess() const { return error == ErrorCode::Success; }
    bool IsError()   const { return error != ErrorCode::Success; }

    // 静态工厂方法
    static Result<T> Ok(T value) {
        return Result<T>(std::move(value));
    }

    static Result<T> Fail(ErrorCode err, const char* msg = nullptr) {
        return Result<T>(err, msg);
    }
};

// 无数据返回的简单结果
struct VoidResult {
    ErrorCode   error;
    const char* message;

    VoidResult() : error(ErrorCode::UnknownError), message(nullptr) {}

    VoidResult(ErrorCode err, const char* msg = nullptr)
        : error(err), message(msg) {}

    bool IsSuccess() const { return error == ErrorCode::Success; }
    bool IsError()   const { return error != ErrorCode::Success; }

    static VoidResult Ok() {
        return VoidResult(ErrorCode::Success);
    }

    static VoidResult Fail(ErrorCode err, const char* msg = nullptr) {
        return VoidResult(err, msg);
    }
};

} // namespace MeyerScan
```

### 5.3 IPC 命令/响应定义

```cpp
// =============================================================================
// 文件:    IpcCommand.h
// 模块:    MeyerScan_Core.lib
// 用途:    定义主进程与 ScanReconstructStudio.exe 之间的 IPC 命令
// =============================================================================

#pragma once

#include <cstdint>

namespace MeyerScan {

// IPC 命令枚举（请求方发送）
enum class IpcCommand : int32_t {
    // ========== 流程控制 ==========
    Cmd_Init                = 1,   // 初始化扫描进程
    Cmd_StartScan           = 2,   // 开始扫描
    Cmd_PauseScan           = 3,   // 暂停扫描
    Cmd_ResumeScan          = 4,   // 继续扫描
    Cmd_CompleteScan        = 5,   // 完成扫描
    Cmd_CancelScan          = 6,   // 取消扫描

    // ========== 数据操作 ==========
    Cmd_LoadCase            = 7,   // 加载病例数据
    Cmd_SaveScanData        = 8,   // 保存扫描数据
    Cmd_ImportScanData      = 9,   // 导入扫描数据
    Cmd_ExportModel         = 10,  // 导出模型（STL/PLY/OBJ）

    // ========== 编辑操作 ==========
    Cmd_StartEdit           = 11,  // 进入编辑模式
    Cmd_DeleteRegion        = 12,  // 删除选中区域
    Cmd_FlipModel           = 13,  // 模型翻转
    Cmd_Undo                = 14,  // 撤销
    Cmd_Redo                = 15,  // 重做

    // ========== 分析操作 ==========
    Cmd_MeasureDistance     = 16,  // 距离测量
    Cmd_MeasureAngle        = 17,  // 角度测量
    Cmd_AnalyzeUndercut     = 18,  // 倒凹分析
    Cmd_AnalyzeBite         = 19,  // 咬合分析

    // ========== 视图控制 ==========
    Cmd_SetViewAngle        = 20,  // 设置视角
    Cmd_ResetView           = 21,  // 重置视角
    Cmd_LockView            = 22,  // 锁定视角

    // ========== 状态查询 ==========
    Cmd_GetScanState        = 23,  // 获取扫描状态
    Cmd_GetProcessProgress  = 24,  // 获取处理进度
    Cmd_GetModelInfo        = 25,  // 获取模型信息

    // ========== 心跳 ==========
    Cmd_Ping                = 100, // 心跳请求
};

// IPC 响应枚举（响应方返回）
enum class IpcResponse : int32_t {
    Resp_Success            = 0,   // 命令执行成功
    Resp_Failed             = 1,   // 命令执行失败
    Resp_NotSupported       = 2,   // 命令不支持
    Resp_Busy               = 3,   // 进程忙碌，无法执行
    Resp_Timeout            = 4,   // 命令超时
    Resp_DeviceError        = 5,   // 设备错误
    Resp_DataError          = 6,   // 数据错误
    Resp_Pong               = 100, // 心跳响应
};

// IPC 超时配置
struct IpcTimeoutConfig {
    static constexpr int32_t DefaultTimeoutMs  = 30000;  // 默认超时 30 秒
    static constexpr int32_t HeartbeatIntervalMs = 3000; // 心跳间隔 3 秒
    static constexpr int32_t HeartbeatTimeoutMs = 10000; // 心跳超时 10 秒
};

} // namespace MeyerScan
```

#### 5.3.1 跨进程患者/订单上下文传递规范

MeyerScan.exe、CaseUI/OrderCreateUI 和 ScanReconstructStudio.exe 都可以使用 Qt，但**跨进程边界不能传递 QObject、QString 指针、QJsonObject 对象指针、模型对象或由另一进程释放的大块内存**。推荐规则如下：

1. **进程内 / DLL 内**：同一进程内、同一 Qt/VS2015 运行环境下的 MeyerScan 模块可以使用 `QString`、`QJsonObject`、`QVariantMap`、信号槽等 Qt 类型，前提是不跨 CRT/进程边界传所有权。
2. **稳定 ABI / 非 Qt 调用方边界**：如果接口要长期兼容、可能被第三方调用、调用方不确定是否使用同一 Qt/CRT，则优先使用 `const char*` UTF-8、固定 POD 结构体或调用方提供缓冲区；Qt 便捷重载可以作为同项目内辅助接口，不必刻意禁止。
3. **跨进程 IPC**：命名管道消息头使用 POD，复杂上下文使用以下三种方式之一：
   - **订单 ID / 文件 ID**：只传 `orderId`、`caseId`、`scanDataPath`，扫描进程通过服务或文件读取详情。
   - **版本化 JSON 文本**：IPC payload 是 UTF-8 JSON 字节数组，字段包含 `schemaVersion`、`caseId`、`orderId`、`patient`、`scanSchema`、`source`、`extensions`。
   - **上下文文件路径**：主进程将完整上下文写入临时 JSON 文件，只通过 IPC 传路径、hash、schemaVersion，适合字段多或后续经常扩展的订单上下文。
4. **字段经常增删时**：优先使用 JSON 上下文 + `extensions` 扩展对象；稳定核心字段保留固定 key，不要求每次字段变化都修改 IPC POD 结构。
5. **稳定性要求**：每个上下文必须带 `schemaVersion`、`requestId`、`sourceModule`、`createdAt`、`hash`。接收方发现未知字段应忽略，缺少必填字段才拒绝。

示例：

```json
{
  "schemaVersion": 1,
  "requestId": "20260622-0001",
  "sourceModule": "OrderCreateUI",
  "caseId": "C202606220001",
  "orderId": "O202606220001",
  "patient": {
    "name": "Test",
    "gender": "unknown",
    "age": 0
  },
  "scanSchema": {
    "orderType": "repair",
    "teeth": []
  },
  "source": {
    "entry": "manual",
    "externalSystem": ""
  },
  "extensions": {}
}
```

结论：**可以用 JSON 表达一组经常变化的患者/订单信息，但跨进程传的是 UTF-8 JSON 字节或 JSON 文件路径，不是 QString/QJsonObject 对象本身。**这样既方便扩展字段，又不会把 Qt 对象生命周期、内存释放和 ABI 风险带进 IPC。

---

## 六、模块接口头文件定义

### 6.1 Core.lib 公共类型（历史候选草案，当前不实施）

```cpp
// =============================================================================
// 文件:    Core.h
// 模块:    MeyerScan_Core.lib
// 用途:    Core.lib 的总入口头文件，包含所有公共类型定义
//
// 依赖:    无第三方依赖
// 导出:    静态库（所有模块静态链接）
// =============================================================================

#pragma once

// 公共类型
#include "ErrorCode.h"
#include "Result.h"
#include "IpcCommand.h"
#include "PodStructs.h"

// 日志宏（供所有模块使用）
#include "Logger.h"  // 引用 Logger.dll 的公开头文件

namespace MeyerScan {

// 模块版本查询接口（每个模块必须实现）
// 说明: 版本号以简单字符串格式返回，如 "MeyerScan_Database v1.1.0 (2026-06-17)"。
// 选择字符串而非 ModuleVersion 结构体的原因：
//   1. 字符串在调试器中一目了然，无需展开结构体查看
//   2. 不依赖复杂类型定义，DLL 边界更安全
//   3. 后续如需要程序化比较版本号，再增加结构体格式的扩展接口
using ModuleVersion = const char*;

} // namespace MeyerScan
```

### 6.2 ConfigCenter.dll 接口

```cpp
// =============================================================================
// 文件:    ConfigCenter.h
// 模块:    MeyerScan_ConfigCenter.dll
// 用途:    全局配置读写唯一入口
//
// 依赖:    Logger.dll, Core.lib
// 导出:    IConfigCenter 纯虚接口
//
// 使用约束:
//   1. 所有模块禁止直接操作配置文件，必须调用本模块接口
//   2. 配置修改后自动触发注册的回调函数
//   3. 版本不兼容时自动尝试迁移，失败则回滚
// =============================================================================

#pragma once

#include "Core.h"
#include <functional>

namespace MeyerScan {

// 配置变更回调函数类型
using ConfigChangeCallback = std::function<void(const char* key, const char* value)>;

// 配置中心接口
class IConfigCenter {
public:
    virtual ~IConfigCenter() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // configPath: 配置文件路径（JSON 格式）
    // 返回值: ErrorCode::Success 或错误码
    virtual ErrorCode Init(const char* configPath) = 0;

    // -----------------------------------------------------------------
    // 基础读写
    // -----------------------------------------------------------------
    // GetValue: 获取配置值（UTF-8 字符串）
    // key: 配置键名，如 "database.type"
    // defaultValue: 默认值（键不存在时返回）
    // 返回值: 配置值字符串指针（内部缓存，调用方不应修改）
    virtual const char* GetValue(const char* key,
                                  const char* defaultValue = nullptr) = 0;

    // SetValue: 设置配置值（立即写入磁盘）
    // key: 配置键名
    // value: 配置值（UTF-8）
    // 返回值: ErrorCode::Success 或错误码
    virtual ErrorCode SetValue(const char* key, const char* value) = 0;

    // GetIntValue / SetIntValue: 整数类型便捷接口
    virtual int32_t GetIntValue(const char* key, int32_t defaultValue = 0) = 0;
    virtual ErrorCode SetIntValue(const char* key, int32_t value) = 0;

    // GetBoolValue / SetBoolValue: 布尔类型便捷接口
    virtual bool GetBoolValue(const char* key, bool defaultValue = false) = 0;
    virtual ErrorCode SetBoolValue(const char* key, bool value) = 0;

    // -----------------------------------------------------------------
    // 批量操作
    // -----------------------------------------------------------------
    // GetValues: 批量获取多个配置值
    // keys: 键名数组
    // count: 键数量
    // values: 输出值数组（调用方分配）
    virtual ErrorCode GetValues(const char** keys, int32_t count,
                                 const char** values) = 0;

    // -----------------------------------------------------------------
    // 变更回调
    // -----------------------------------------------------------------
    // RegisterChangeCallback: 注册配置变更回调
    // key: 监听的键名（支持通配符 "*" 监听所有变更）
    // callback: 回调函数
    // 返回值: 回调 ID（用于注销）
    virtual int32_t RegisterChangeCallback(const char* key,
                                            ConfigChangeCallback callback) = 0;

    // UnregisterChangeCallback: 注销回调
    virtual ErrorCode UnregisterChangeCallback(int32_t callbackId) = 0;

    // -----------------------------------------------------------------
    // 版本管理
    // -----------------------------------------------------------------
    // GetConfigVersion: 获取当前配置版本号
    virtual const char* GetConfigVersion() const = 0;

    // GetModuleVersion: 获取模块版本信息
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 导入导出
    // -----------------------------------------------------------------
    // ExportToFile: 导出配置到指定文件
    virtual ErrorCode ExportToFile(const char* filePath) = 0;

    // ImportFromFile: 从文件导入配置（覆盖当前配置）
    virtual ErrorCode ImportFromFile(const char* filePath) = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_CONFIGCENTER_API IConfigCenter* GetConfigCenter();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_CONFIGCENTER_EXPORTS
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllexport)
#else
#  define MEYERSCAN_CONFIGCENTER_API __declspec(dllimport)
#endif
```

### 6.3 Crypto.dll 接口

```cpp
// =============================================================================
// 文件:    Crypto.h
// 模块:    MeyerScan_Crypto.dll
// 用途:    文件/数据加解密服务
//
// 依赖:    Logger.dll, Core.lib
// 导出:    ICrypto 纯虚接口
//
// 算法:    AES-256-CBC
// 密钥管理: 支持多密钥 ID 和密钥轮换
// =============================================================================

#pragma once

#include "Core.h"

namespace MeyerScan {

// 加密算法类型
enum class CryptoAlgorithm : int32_t {
    AES_256_CBC = 0,    // AES-256 CBC 模式（默认）
    AES_128_CBC = 1,    // AES-128 CBC 模式（兼容旧数据）
};

// 密钥信息结构（POD，可跨进程传递）
struct CryptoKeyInfo {
    uint32_t version;           // 结构体版本 = 1
    char     keyId[64];         // 密钥标识，如 "default", "backup"
    int32_t  algorithm;         // CryptoAlgorithm 值
    char     createTime[32];    // 创建时间
    int32_t  isActive;          // 是否激活（1=是，0=否）
    int32_t  reserved[8];       // 预留字段
};

// 加密接口
class ICrypto {
public:
    virtual ~ICrypto() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // keyConfigPath: 密钥配置文件路径
    virtual ErrorCode Init(const char* keyConfigPath) = 0;

    // -----------------------------------------------------------------
    // 数据加解密
    // -----------------------------------------------------------------
    // Encrypt: 加密数据
    // plainData: 明文数据指针
    // plainSize: 明文大小
    // cipherData: 输出密文数据（调用方分配，大小需 >= plainSize + 32）
    // cipherSize: 输出密文大小
    // keyId: 密钥标识（默认 "default"）
    virtual ErrorCode Encrypt(const uint8_t* plainData, int32_t plainSize,
                              uint8_t* cipherData, int32_t* cipherSize,
                              const char* keyId = "default") = 0;

    // Decrypt: 解密数据
    virtual ErrorCode Decrypt(const uint8_t* cipherData, int32_t cipherSize,
                              uint8_t* plainData, int32_t* plainSize,
                              const char* keyId = "default") = 0;

    // -----------------------------------------------------------------
    // 文件加解密
    // -----------------------------------------------------------------
    // EncryptFile: 加密文件
    // srcPath: 源文件路径（明文）
    // dstPath: 目标文件路径（密文）
    virtual ErrorCode EncryptFile(const char* srcPath, const char* dstPath,
                                   const char* keyId = "default") = 0;

    // DecryptFile: 解密文件
    virtual ErrorCode DecryptFile(const char* srcPath, const char* dstPath,
                                   const char* keyId = "default") = 0;

    // -----------------------------------------------------------------
    // 哈希计算
    // -----------------------------------------------------------------
    // ComputeHash: 计算 SHA-256 哈希值
    // data: 输入数据
    // size: 数据大小
    // hashOut: 输出哈希值（64 字符十六进制字符串）
    virtual ErrorCode ComputeHash(const uint8_t* data, int32_t size,
                                   char* hashOut) = 0;

    // ComputeFileHash: 计算文件哈希值
    virtual ErrorCode ComputeFileHash(const char* filePath, char* hashOut) = 0;

    // -----------------------------------------------------------------
    // 密钥管理
    // -----------------------------------------------------------------
    // GetActiveKeyId: 获取当前激活的密钥 ID
    virtual const char* GetActiveKeyId() const = 0;

    // ListKeys: 列出所有密钥信息
    virtual ErrorCode ListKeys(CryptoKeyInfo* keyList, int32_t* count) = 0;

    // RotateKey: 密钥轮换（切换到新密钥）
    // newKeyId: 新密钥 ID
    virtual ErrorCode RotateKey(const char* newKeyId) = 0;

    // ReEncryptData: 用新密钥重新加密数据
    virtual ErrorCode ReEncryptData(const uint8_t* oldCipher, int32_t oldSize,
                                    const char* oldKeyId,
                                    uint8_t* newCipher, int32_t* newSize,
                                    const char* newKeyId) = 0;

    // -----------------------------------------------------------------
    // 版本信息
    // -----------------------------------------------------------------
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_CRYPTO_API ICrypto* GetCrypto();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_CRYPTO_EXPORTS
#  define MEYERSCAN_CRYPTO_API __declspec(dllexport)
#else
#  define MEYERSCAN_CRYPTO_API __declspec(dllimport)
#endif
```

### 6.4 Permission.dll 接口

```cpp
// =============================================================================
// 文件:    Permission.h
// 模块:    MeyerScan_Permission.dll
// 用途:    六维权限校验与授权管理
//
// 依赖:    Logger.dll, Core.lib, ConfigCenter.dll, Crypto.dll
// 导出:    IPermission 纯虚接口
//
// 六维校验: 角色 + 客户 + 机型 + 软件版本 + 时间 + 配置
// 约束:     所有权限修改需重启软件后生效
// =============================================================================

#pragma once

#include "Core.h"

namespace MeyerScan {

// 功能授权状态结构（POD，可跨进程传递）
struct FeatureAuthStatus {
    uint32_t version;           // 结构体版本 = 1
    char     featureId[64];     // 功能标识，如 "scan.ai_remove_soft_tissue"
    char     featureName[128];  // 功能名称（显示用）
    int32_t  isAuthorized;      // 是否授权（1=是，0=否）
    int32_t  authLevel;         // 授权级别（0=禁用，1=基础，2=高级，3=专业）
    char     expireTime[32];    // 过期时间（永久授权为空）
    char     constraint[256];   // 约束条件（JSON 格式）
    int32_t  reserved[8];
};

// 功能显示规则结构
struct FeatureDisplayRule {
    uint32_t version;           // 结构体版本 = 1
    char     featureId[64];     // 功能标识
    int32_t  isVisible;         // 是否可见
    int32_t  isClickable;       // 是否可点击
    char     styleRule[128];    // UI 样式规则（如按钮颜色、图标等）
    int32_t  reserved[4];
};

// 六维校验参数结构
struct PermissionCheckContext {
    uint32_t version;           // 结构体版本 = 1
    char     role[32];          // 用户角色
    char     customerId[64];    // 客户标识
    char     deviceModel[64];   // 设备机型
    char     deviceSerial[64];  // 设备序列号/加密狗 ID
    char     softwareVersion[32]; // 软件版本
    char     currentTime[32];   // 当前时间；最终以 Permission 内部可信时间校验为准
    char     configProfileId[64]; // 配置/版本方案 ID
    int32_t  reserved[8];
};

// 权限接口
class IPermission {
public:
    virtual ~IPermission() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // ruleFilePath: 权限规则文件路径（加密文件）
    virtual ErrorCode Init(const char* ruleFilePath) = 0;

    // -----------------------------------------------------------------
    // 权限校验
    // -----------------------------------------------------------------
    // CheckAccess: 校验功能是否可访问
    // featureId: 功能标识
    // context: 六维校验参数
    // 返回值: ErrorCode::Success（允许）或 ErrorCode::PermissionDenied（拒绝）
    virtual ErrorCode CheckAccess(const char* featureId,
                                   const PermissionCheckContext& context) = 0;

    // CheckAccessSimple: 简化校验（使用当前系统上下文）
    virtual ErrorCode CheckAccessSimple(const char* featureId) = 0;

    // GetAccessibleFeatures: 获取当前用户可访问的功能列表
    virtual ErrorCode GetAccessibleFeatures(const PermissionCheckContext& context,
                                             FeatureAuthStatus* featureList,
                                             int32_t* count) = 0;

    // -----------------------------------------------------------------
    // 显示规则
    // -----------------------------------------------------------------
    // GetFeatureDisplayRule: 获取功能的 UI 显示规则
    virtual ErrorCode GetFeatureDisplayRule(const char* featureId,
                                             FeatureDisplayRule& rule) = 0;

    // -----------------------------------------------------------------
    // 权限配置界面（原 PermissionUI 功能）
    // -----------------------------------------------------------------
    // GetAllFeatureStatus: 获取所有功能的授权状态（供配置界面显示）
    virtual ErrorCode GetAllFeatureStatus(FeatureAuthStatus* statusList,
                                           int32_t* count) = 0;

    // UpdateAuthByQRCode: 通过扫码更新权限
    // qrCodeContent: 扫码内容（加密的权限配置）
    // 返回值: Success 表示更新成功，需重启生效
    virtual ErrorCode UpdateAuthByQRCode(const char* qrCodeContent) = 0;

    // -----------------------------------------------------------------
    // 规则文件管理
    // -----------------------------------------------------------------
    // LoadRuleFile: 加载新的权限规则文件
    virtual ErrorCode LoadRuleFile(const char* ruleFilePath) = 0;

    // GetRuleVersion: 获取当前规则版本
    virtual const char* GetRuleVersion() const = 0;

    // BackupRuleFile: 备份当前规则文件
    virtual ErrorCode BackupRuleFile(const char* backupPath) = 0;

    // -----------------------------------------------------------------
    // 版本信息
    // -----------------------------------------------------------------
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_PERMISSION_API IPermission* GetPermission();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_PERMISSION_EXPORTS
#  define MEYERSCAN_PERMISSION_API __declspec(dllexport)
#else
#  define MEYERSCAN_PERMISSION_API __declspec(dllimport)
#endif
```

#### 6.4.1 六维权限控制流程与防绕过规则

六维权限不是 UI 显示规则，而是全链路门禁。六个维度为：角色、客户、设备（机型 + 序列号/加密狗 ID）、软件版本、时间有效期、配置方案。

**启动加载流程**：

1. ConfigCenter 读取本地配置：客户 ID、配置方案 ID、软件版本、规则文件路径。
2. Permission 通过 Crypto 解密并验签权限规则文件。
3. Permission 校验规则文件版本、客户 ID、设备序列号/加密狗 ID、软件版本范围、有效期。
4. Permission 合并规则：内置基础规则 → 客户规则 → 设备/授权规则 → 本地临时规则。
5. 合并后生成只读权限快照，后续热路径 `CheckAccess` 只查内存，不做文件 IO。
6. 权限更新（扫码/导入规则）只写入新规则文件和版本记录，默认重启后生效。

**规则合并优先级**：

| 规则 | 优先级 | 说明 |
|------|--------|------|
| 显式 Deny | 最高 | 任何上层规则显式禁用时，后续规则不得重新开启 |
| 授权文件 Allow | 高 | 必须验签、未过期、设备/客户匹配后才生效 |
| 客户定制规则 | 中 | 可限制功能、改显示样式、改默认配置；不得越权开启未授权高级功能 |
| 基础功能白名单 | 低 | 登录、基础浏览、安全退出等必要功能默认可用 |
| 未声明功能 | 默认 Deny | 除基础白名单外，规则文件未声明的功能一律不可用 |

**防绕过校验点**：

| 层级 | 必须做的事 | 原因 |
|------|------------|------|
| HomeUI / CaseUI / OrderCreateUI | 根据 `GetFeatureDisplayRule` 隐藏或禁用入口 | 改善体验，但不是安全边界 |
| OrderWorkflowService | 新建、加载、继续扫描、发送前调用 `CheckAccess` 并输出流程决策 | 防止 UI 绕过直接进入流程 |
| CaseOrderService / ScanSchemaService | 患者/订单写入、删除、参考数据修改、扫描方案修改前再次校验 | 防止外部代码直接调用 Service |
| ScanReconstructStudio.exe | IPC 接收订单后校验权限快照和功能 ID | 防止主进程 UI 被绕过后直接启动扫描工具 |
| DataExport / NetworkHelper | 导出、打包、邮件、云上传前独立校验 | 数据外发属于高风险动作 |

**时间与回滚防护**：

- Permission 不完全信任调用方传入的 `currentTime`，内部应使用系统时间 + 上次可信时间记录 + 授权文件签名时间综合判断。
- 如果检测到系统时间回拨，进入降级策略：允许基础浏览和安全退出，禁止新建、扫描、导出、授权更新等高价值动作。
- 权限规则文件必须带 `ruleVersion` 和 `issuedAt`，更新时只允许升级或同版本重放，禁止旧规则覆盖新规则。

**定制客户与功能阉割策略**：

1. **强阉割**：安装包/插件清单不交付对应 DLL，或模块清单不加载该模块。适合高级算法、云服务、数据导出等高价值功能。
2. **中阉割**：Permission 禁用功能，UI 隐藏入口，Service/Workflow/IPC 拒绝调用。适合按客户/试用期/机型授权的功能。
3. **弱定制**：ConfigCenter 改默认参数、语言、Logo、主题、字段默认值。适合 OEM 视觉和流程偏好，不作为安全边界。

预期目标：普通定制通过配置和权限完成；高价值功能通过“模块不交付 + 权限拒绝 + 服务复核”三层保证，避免只隐藏按钮导致功能被绕过。

### 6.5 CaseEntity.lib 数据结构（历史候选草案，当前不实施）

```cpp
// =============================================================================
// 文件:    CaseEntity.h
// 模块:    MeyerScan_CaseEntity.lib
// 用途:    病例/订单数据实体定义（纯数据结构，静态库）
//
// 依赖:    无第三方依赖
// 导出:    静态库（嵌入调用方）
//
// 约束:
//   1. 纯数据载体，无业务逻辑
//   2. 预留扩展字段，便于后续新增
//   3. 版本号字段便于序列化/迁移
// =============================================================================

#pragma once

#include "Core.h"
#include <cstdint>

namespace MeyerScan {

// 性别枚举
enum class Gender : int32_t {
    Unknown = 0,
    Male    = 1,
    Female  = 2,
};

// 病例状态枚举
enum class CaseStatus : int32_t {
    Created     = 0,    // 已创建
    Scanning    = 1,    // 扫描中
    Processing  = 2,    // 处理中
    Completed   = 3,    // 已完成
    Sent        = 4,    // 已发送
    Archived    = 5,    // 已归档
    Deleted     = 6,    // 已删除
};

// 订单类型枚举
enum class OrderType : int32_t {
    Orthodontic = 1,    // 正畸
    Restoration = 2,    // 修复
};

// 牙颌类型枚举
enum class JawType : int32_t {
    Upper       = 1,    // 上颌
    Lower       = 2,    // 下颌
    Bite        = 3,    // 咬合
};

// 订单状态枚举
enum class OrderStatus : int32_t {
    Created     = 0,    // 已创建
    Scanned     = 1,    // 已扫描
    Processed   = 2,    // 已处理
    Exported    = 3,    // 已导出
    Sent        = 4,    // 已发送
};

// 病例信息结构（POD，可跨进程传递）
struct CaseInfo {
    uint32_t version;               // 结构体版本 = 1

    // 基本信息
    char     caseId[64];            // 病例唯一标识
    char     patientName[128];      // 患者姓名
    int32_t  patientAge;            // 患者年龄
    int32_t  patientGender;         // Gender 枚举值

    // 诊所/医生信息
    char     clinicId[64];          // 诊所标识
    char     clinicName[128];       // 诊所名称
    char     doctorId[64];          // 医生标识
    char     doctorName[128];       // 医生姓名

    // 技工所信息
    char     technicianId[64];      // 技工所标识
    char     technicianName[128];   // 技工所名称
    char     technicianEmail[128];  // 技工所邮箱

    // 时间信息
    char     createTime[32];        // 创建时间（ISO 8601）
    char     updateTime[32];        // 更新时间

    // 状态
    int32_t  status;                // CaseStatus 枚举值

    // 扩展字段
    char     extData[256];          // 扩展数据（JSON 格式）
    int32_t  reserved[8];           // 预留字段
};

// 订单信息结构（POD，可跨进程传递）
struct OrderInfo {
    uint32_t version;               // 结构体版本 = 1

    // 基本信息
    char     orderId[64];           // 订单唯一标识
    char     caseId[64];            // 所属病例标识

    // 订单类型
    int32_t  orderType;             // OrderType 枚举值
    int32_t  jawType;               // JawType 枚举值

    // 牙位信息
    char     toothPosition[32];     // FDI 牙位编号（如 "11,12,13"）

    // 修复信息
    char     restorationType[64];   // 修复体类型
    char     restorationMaterial[64]; // 修复体材料
    char     toothColor[32];        // 齿色（如 "A2"）

    // 时间信息
    char     createTime[32];
    char     updateTime[32];

    // 状态
    int32_t  status;                // OrderStatus 枚举值

    // 扩展字段
    char     extData[256];
    int32_t  reserved[8];
};

// 扫描方案信息结构（POD）
struct ScanSchemaInfo {
    uint32_t version;               // 结构体版本 = 1

    char     schemaId[64];          // 方案标识
    char     caseId[64];            // 所属病例

    int32_t  scanMode;              // 扫描模式（1=正畸，2=修复）
    int32_t  jawType;               // JawType 枚举

    // 扫描参数
    int32_t  aiSoftTissueRemoval;   // AI 软组织消去（1=开启，0=关闭）
    int32_t  aiGloveRemoval;        // AI 手套色去除
    int32_t  colorCorrection;       // 颜色校准

    // 扩展字段
    char     extData[256];
    int32_t  reserved[8];
};

// 辅助函数：初始化结构体默认值
inline void InitCaseInfo(CaseInfo& info) {
    info.version = 1;
    info.patientAge = 0;
    info.patientGender = static_cast<int32_t>(Gender::Unknown);
    info.status = static_cast<int32_t>(CaseStatus::Created);
    // 其他字段由调用方填充
}

inline void InitOrderInfo(OrderInfo& info) {
    info.version = 1;
    info.orderType = static_cast<int32_t>(OrderType::Restoration);
    info.jawType = static_cast<int32_t>(JawType::Upper);
    info.status = static_cast<int32_t>(OrderStatus::Created);
}

} // namespace MeyerScan
```

### 6.6 病例域服务接口

```cpp
// =============================================================================
// 文件:    CaseDomainServices.h
// 模块:    MeyerScan_CaseOrderService.dll /
//          MeyerScan_ScanSchemaService.dll / MeyerScan_DataExport.dll /
//          MeyerScan_OrderWorkflowService.dll
// 用途:    病例域服务接口声明
//
// 依赖:    Logger.dll, Core.lib, ConfigCenter.dll, Crypto.dll, CaseEntity.lib
// 导出:    ICaseOrderService / IScanSchemaService / IDataExportService / IOrderWorkflowService
//
// 架构:
//   UI 层 → 领域 Service → DAO → Database.dll
//
// 约束:
//   1. UI 层必须调用对应 Service，禁止绕过 Service 直接调用 DAO
//   2. 患者/订单强绑定，由 CaseOrderService 统一管理
//   3. 医生、诊所、技工所等数据库主数据也归 CaseOrderService 管理
//   4. 扫描方案、导入导出、流程决策仍独立，避免 CaseOrderService 膨胀
//   5. 所有操作留痕（调用 Logger）
// =============================================================================

#pragma once

#include "Core.h"
#include "CaseEntity.h"

namespace MeyerScan {

// 简单结果，后续统一迁入 Core.lib/Result.h。
struct CaseOrderServiceResult {
    int32_t errorCode;
    char    message[256];
};

// 患者/订单组合服务。
// 说明:
//   患者与订单在口扫业务和数据库内高度耦合，不再拆成 CaseService/OrderService。
//   字段变化频繁，因此 DLL 边界优先使用 UTF-8 JSON + 调用方缓冲区。
//   JSON 内部必须带 schemaVersion，服务内部负责映射到版本化数据库表。
class ICaseOrderService {
public:
    virtual ~ICaseOrderService() = default;

    // databaseConfigPathUtf8: db_config.json 路径，由 MainExe/服务宿主从 ConfigCenter 或应用目录规则传入。
    // CaseOrderService 内部必须通过 DatabaseQtAdapter 访问纯 C++ Database，不直接包含 Database.h。
    // logDirUtf8: 日志目录，由 MainExe/服务宿主基于 applicationDirPath 传入。
    virtual bool Init(const char* databaseConfigPathUtf8, const char* logDirUtf8) = 0;

    // 创建/迁移本服务拥有的轻量 schema。
    // 当前骨架包含 ms_patient_order 与 ms_reference_data；
    // 正式版本应改为版本化 migration，不把旧 mysql.sql 直接作为运行脚本。
    virtual CaseOrderServiceResult EnsureSchema() = 0;

    // 保存患者/订单组合 JSON。
    // 必填建议: schemaVersion、orderId；可选 patientId/caseId/status/extensions。
    virtual CaseOrderServiceResult SavePatientOrderJson(const char* patientOrderJsonUtf8) = 0;

    // 按 orderId 读取患者/订单组合 JSON。
    // buffer 由调用方分配，避免跨 DLL 内存所有权。
    virtual CaseOrderServiceResult GetPatientOrderJson(const char* orderIdUtf8,
                                                       char* buffer,
                                                       int32_t bufferSize) = 0;

    // 读取医生、诊所、技工所等参考数据。
    // category 建议值: doctor / clinic / lab / operator；空值表示全部。
    virtual CaseOrderServiceResult ListReferenceDataJson(const char* categoryUtf8,
                                                         char* buffer,
                                                         int32_t bufferSize) = 0;

    // 统一查询入口，用 queryName 稳定调用场景，queryArgsJsonUtf8 承载扩展参数。
    // 当前建议 queryName:
    //   patientOrder.byOrderId
    //   referenceData.list
    virtual CaseOrderServiceResult QueryJson(const char* queryNameUtf8,
                                             const char* queryArgsJsonUtf8,
                                             char* buffer,
                                             int32_t bufferSize) = 0;

    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

// 扫描方案服务接口
class IScanSchemaService {
public:
    virtual ~IScanSchemaService() = default;

    virtual ErrorCode Init(const DatabaseConfig& dbConfig) = 0;

    virtual Result<ScanSchemaInfo> CreateScanSchema(const ScanSchemaInfo& info) = 0;
    virtual VoidResult UpdateScanSchema(const ScanSchemaInfo& info) = 0;
    virtual Result<ScanSchemaInfo> GetScanSchemaByOrder(const char* orderId) = 0;
    virtual VoidResult DeleteScanSchema(const char* schemaId) = 0;

    virtual ModuleVersion GetModuleVersion() const = 0;
    virtual ErrorCode Shutdown() = 0;
};

// 数据导入导出服务接口
class IDataExportService {
public:
    virtual ~IDataExportService() = default;

    virtual ErrorCode Init(const DatabaseConfig& dbConfig) = 0;

    virtual ErrorCode ExportCasesToFile(const char* filePath,
                                         const char* caseIds) = 0;
    virtual ErrorCode ImportCasesFromFile(const char* filePath) = 0;
    virtual ErrorCode ExportOrdersToFolder(const char* folderPath,
                                            const char* orderIds) = 0;
    virtual ErrorCode PackageOrderData(const char* orderId,
                                        const char* outputPackagePath) = 0;

    virtual ModuleVersion GetModuleVersion() const = 0;
    virtual ErrorCode Shutdown() = 0;
};

// 订单流程动作
enum class OrderWorkflowAction : int32_t {
    Deny                = 0,   // 拒绝进入
    ShowOrderCreate     = 1,   // 显示建单/订单编辑界面
    StartScan           = 2,   // 直接进入扫描
    ContinueScan        = 3,   // 继续未完成扫描
    OpenProcessing      = 4,   // 进入数据处理
    OpenSend            = 5,   // 进入发送/导出
};

// 订单流程上下文
struct OrderWorkflowContext {
    uint32_t version;               // 结构体版本 = 1
    char     orderId[64];           // 订单 ID，新建时为空
    char     caseId[64];            // 病例 ID
    char     entrySource[32];       // home/case_ui/recent/practice/ipc
    int32_t  isPractice;            // 是否练习扫描
    int32_t  reserved[8];
};

// 订单流程决策结果
struct OrderWorkflowDecision {
    uint32_t version;               // 结构体版本 = 1
    int32_t  action;                // OrderWorkflowAction
    int32_t  orderCreateEditable;   // OrderCreateUI 是否允许编辑
    int32_t  requirePermissionCheck;// 调用方是否必须再次展示权限提示
    char     scanLaunchMode[32];    // new/restore/practice/process/send
    char     denyReason[256];       // 拒绝原因，空表示允许
    int32_t  reserved[8];
};

// 订单流程规则服务
class IOrderWorkflowService {
public:
    virtual ~IOrderWorkflowService() = default;

    virtual ErrorCode Init(const DatabaseConfig& dbConfig) = 0;

    // EvaluateNewOrder: 首页点击“创建”时调用，决定是否显示建单界面
    virtual Result<OrderWorkflowDecision> EvaluateNewOrder(
        const PermissionCheckContext& permissionContext) = 0;

    // EvaluateOpenOrder: CaseUI/最近订单/外部唤起加载订单时调用
    virtual Result<OrderWorkflowDecision> EvaluateOpenOrder(
        const OrderWorkflowContext& workflowContext,
        const PermissionCheckContext& permissionContext) = 0;

    // EvaluatePracticeScan: 首页点击“练习”时调用
    virtual Result<OrderWorkflowDecision> EvaluatePracticeScan(
        const PermissionCheckContext& permissionContext) = 0;

    virtual ModuleVersion GetModuleVersion() const = 0;
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_CASEORDERSERVICE_API ICaseOrderService* GetCaseOrderService();
extern "C" MEYERSCAN_SCANSCHEMASERVICE_API IScanSchemaService* GetScanSchemaService();
extern "C" MEYERSCAN_DATAEXPORT_API IDataExportService* GetDataExportService();
extern "C" MEYERSCAN_ORDERWORKFLOW_API IOrderWorkflowService* GetOrderWorkflowService();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_CASEORDERSERVICE_EXPORTS
#  define MEYERSCAN_CASEORDERSERVICE_API __declspec(dllexport)
#else
#  define MEYERSCAN_CASEORDERSERVICE_API __declspec(dllimport)
#endif

// 其余服务 DLL 使用同样的 __declspec(dllexport/dllimport) 宏模式。
```

#### 6.6.1 患者/订单与数据库主数据存储流程

患者/订单与医生、诊所、技工所、操作人等数据统一归入 `CaseOrderService.dll` 的数据库领域边界。存储结构可以后续继续细化，但调用流程必须先固定：

| 数据类别 | 建议存储方式 | 读取入口 | 使用界面 | 边界 |
|----------|--------------|----------|----------|------|
| 患者/订单组合数据 | `ms_patient_order` 轻量组合表 + `payload_json`；正式版再按查询性能拆出索引字段和扩展表 | `GetPatientOrderJson()` / `QueryJson("patientOrder.byOrderId")` | CaseUI、OrderCreateUI、ExternalLaunchAdapter、HisWorklistAdapter | UI 不直接查患者表/订单表；字段变化通过 `schemaVersion` 和 `extensions` 吸收 |
| 医生列表 | `ms_reference_data(category='doctor')` 或正式医生主表 + 视图 | `ListReferenceDataJson("doctor")` | 建单、发送、设置/信息管理 | 不放在 UI 配置文件里；新增字段不影响 UI ABI |
| 诊所列表 | `ms_reference_data(category='clinic')` 或正式诊所主表 + 视图 | `ListReferenceDataJson("clinic")` | 建单、发送、设置/信息管理 | 由 CaseOrderService 统一查询和更新 |
| 技工所列表 | `ms_reference_data(category='lab')` 或正式技工所主表 + 视图 | `ListReferenceDataJson("lab")` | 建单、发送、云端/邮箱发送 | 邮箱、地址等可放 `payload_json` 或后续字段表 |
| 其他字典/主数据 | 先用 `category + payload_json` 承载，稳定后再拆表 | `QueryJson("referenceData.list")` | 各业务 UI | 必须有 category 白名单和权限复核，不允许 UI 任意 SQL |

关键规则：

1. `Database.dll` 只负责执行 SQL、事务和通用结果 JSON 化，不知道“医生/诊所/技工所”的业务意义。
2. `CaseOrderService.dll` 负责决定这些数据的 category、字段 key、schemaVersion、默认值、状态过滤和权限复核。
3. UI 模块只消费 JSON DTO 或稳定查询名，不直接依赖实际表结构。
4. 高频变化字段放入 `payload_json/extensions`，稳定且需要检索的字段再提升为独立列或索引。
5. 后续 migration 必须版本化，不直接把旧 `mysql.sql` 作为正式运行脚本。

### 6.7 DeviceCmd.dll 接口

```cpp
// =============================================================================
// 文件:    DeviceCmd.h
// 模块:    MeyerScan_DeviceCmd.dll
// 用途:    设备命令层（组装/解析/校验）
//
// 依赖:    Logger.dll, Core.lib, DeviceTransport.dll
// 导出:    IDeviceCmd 纯虚接口
//
// 分层说明:
//   上层业务 → DeviceCmd.dll（命令层）→ DeviceTransport.dll（传输层）→ 硬件
//
// 职责:
//   1. 命令组装：将高层操作转换为设备协议帧
//   2. 响应解析：解析设备返回数据为业务结构
//   3. 校验日志：命令执行前校验、执行后记录日志
// =============================================================================

#pragma once

#include "Core.h"

namespace MeyerScan {

// 设备命令类型枚举
enum class DeviceCmdType : int32_t {
    Cmd_GetDeviceInfo     = 0x01,  // 获取设备信息
    Cmd_StartScan         = 0x02,  // 开始扫描
    Cmd_StopScan          = 0x03,  // 停止扫描
    Cmd_PauseScan         = 0x04,  // 暂停扫描
    Cmd_GetFrame          = 0x05,  // 获取帧数据
    Cmd_SetParam          = 0x06,  // 设置参数
    Cmd_GetParam          = 0x07,  // 获取参数
    Cmd_FirmwareUpdate    = 0x08,  // 固件升级
    Cmd_Calibrate         = 0x09,  // 校准命令
    Cmd_Reset             = 0x0A,  // 重置设备
};

// 设备状态枚举
enum class DeviceState : int32_t {
    Disconnected  = 0,    // 未连接
    Connecting    = 1,    // 连接中
    Ready         = 2,    // 就绪
    Scanning      = 3,    // 扫描中
    Calibrating   = 4,    // 校准中
    Error         = 5,    // 错误
    Updating      = 6,    // 固件升级中
};

// 设备信息结构（POD）
struct DeviceInfo {
    uint32_t version;               // 结构体版本 = 1
    char     serialNumber[64];      // 设备序列号
    char     model[64];             // 设备型号
    char     firmwareVersion[32];   // 固件版本
    int32_t  state;                 // DeviceState 枚举值
    char     lastConnectTime[32];   // 最后连接时间
    int32_t  reserved[8];
};

// 帧数据元信息结构（POD）
struct FrameMeta {
    uint32_t version;               // 结构体版本 = 1
    int32_t  frameIndex;            // 帧序号
    char     captureTime[32];       // 采集时间
    int32_t  width;                 // 图像宽度
    int32_t  height;                // 图像高度
    int32_t  format;                // 图像格式
    char     deviceId[64];          // 设备序列号
    char     caseId[64];            // 病例标识
    int32_t  reserved[8];
};

// 设备命令接口
class IDeviceCmd {
public:
    virtual ~IDeviceCmd() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // transportDllPath: DeviceTransport.dll 路径
    virtual ErrorCode Init(const char* transportDllPath) = 0;

    // -----------------------------------------------------------------
    // 设备连接
    // -----------------------------------------------------------------
    // Connect: 连接设备
    // deviceId: 设备标识（序列号或 IP）
    // timeoutMs: 超时时间（毫秒）
    virtual VoidResult Connect(const char* deviceId, int32_t timeoutMs = 5000) = 0;

    // Disconnect: 断开设备
    virtual VoidResult Disconnect() = 0;

    // GetDeviceInfo: 获取设备信息
    virtual Result<DeviceInfo> GetDeviceInfo() = 0;

    // GetDeviceState: 获取设备状态
    virtual DeviceState GetDeviceState() const = 0;

    // -----------------------------------------------------------------
    // 扫描控制
    // -----------------------------------------------------------------
    // StartScan: 开始扫描
    // caseId: 病例标识（用于日志关联）
    virtual VoidResult StartScan(const char* caseId) = 0;

    // StopScan: 停止扫描
    virtual VoidResult StopScan() = 0;

    // PauseScan: 暂停扫描
    virtual VoidResult PauseScan() = 0;

    // ResumeScan: 继续扫描
    virtual VoidResult ResumeScan() = 0;

    // -----------------------------------------------------------------
    // 帧数据获取
    // -----------------------------------------------------------------
    // GetFrame: 获取单帧数据
    // frameData: 输出帧数据缓冲区（调用方分配）
    // frameSize: 输出帧大小
    // meta: 输出帧元信息
    virtual ErrorCode GetFrame(uint8_t* frameData, int32_t* frameSize,
                                FrameMeta& meta) = 0;

    // -----------------------------------------------------------------
    // 参数设置
    // -----------------------------------------------------------------
    // SetParam: 设置设备参数
    // paramId: 参数标识
    // value: 参数值（JSON 格式）
    virtual VoidResult SetParam(const char* paramId, const char* value) = 0;

    // GetParam: 获取设备参数
    virtual Result<const char*> GetParam(const char* paramId) = 0;

    // -----------------------------------------------------------------
    // 固件升级
    // -----------------------------------------------------------------
    // StartFirmwareUpdate: 开始固件升级
    // firmwarePath: 固件文件路径
    // progressCallback: 进度回调函数
    virtual ErrorCode StartFirmwareUpdate(const char* firmwarePath,
                                           void (*progressCallback)(int32_t progress)) = 0;

    // CancelFirmwareUpdate: 取消固件升级
    virtual VoidResult CancelFirmwareUpdate() = 0;

    // -----------------------------------------------------------------
    // 版本信息
    // -----------------------------------------------------------------
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_DEVICECMD_API IDeviceCmd* GetDeviceCmd();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_DEVICECMD_EXPORTS
#  define MEYERSCAN_DEVICECMD_API __declspec(dllexport)
#else
#  define MEYERSCAN_DEVICECMD_API __declspec(dllimport)
#endif
```

### 6.8 ScanDataIO.dll 接口

```cpp
// =============================================================================
// 文件:    ScanDataIO.h
// 模块:    MeyerScan_ScanDataIO.dll
// 用途:    扫描数据读写与存储管理
//
// 依赖:    Logger.dll, Core.lib, Crypto.dll, CaseEntity.lib
// 导出:    IScanDataIO 纯虚接口
//
// 职责:
//   1. 帧数据保存/读取
//   2. 数据完整性校验
//   3. 多设备格式解析
//   4. 病例数据目录管理
// =============================================================================

#pragma once

#include "Core.h"
#include "CaseEntity.h"

namespace MeyerScan {

// 帧数据保存选项
struct FrameSaveOptions {
    uint32_t version;               // 结构体版本 = 1
    int32_t  encryptData;           // 是否加密保存（1=是，0=否）
    int32_t  compressData;          // 是否压缩（1=是，0=否）
    char     keyId[64];             // 加密密钥标识
    int32_t  reserved[8];
};

// 帧数据加载选项
struct FrameLoadOptions {
    uint32_t version;               // 结构体版本 = 1
    int32_t  decryptData;           // 是否解密（1=是，0=否）
    char     keyId[64];             // 解密密钥标识
    int32_t  reserved[8];
};

// 扫描数据接口
class IScanDataIO {
public:
    virtual ~IScanDataIO() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // dataRootPath: 数据存储根目录
    // cryptoDllPath: Crypto.dll 路径
    virtual ErrorCode Init(const char* dataRootPath,
                           const char* cryptoDllPath) = 0;

    // -----------------------------------------------------------------
    // 帧数据保存
    // -----------------------------------------------------------------
    // SaveFrame: 保存单帧数据
    // caseId: 病例标识
    // orderId: 订单标识
    // frameIndex: 帧序号
    // frameData: 帧数据
    // frameSize: 帧大小
    // meta: 帧元信息
    // options: 保存选项
    virtual ErrorCode SaveFrame(const char* caseId, const char* orderId,
                                int32_t frameIndex,
                                const uint8_t* frameData, int32_t frameSize,
                                const FrameMeta& meta,
                                const FrameSaveOptions& options) = 0;

    // LoadFrame: 加载单帧数据
    virtual ErrorCode LoadFrame(const char* caseId, const char* orderId,
                                int32_t frameIndex,
                                uint8_t* frameData, int32_t* frameSize,
                                const FrameLoadOptions& options) = 0;

    // -----------------------------------------------------------------
    // 帧列表管理
    // -----------------------------------------------------------------
    // GetFrameList: 获取病例的所有帧列表
    // metaList: 输出元信息数组
    // count: 输出数量
    virtual ErrorCode GetFrameList(const char* caseId, const char* orderId,
                                   FrameMeta* metaList, int32_t* count) = 0;

    // GetFrameCount: 获取帧数量
    virtual int32_t GetFrameCount(const char* caseId, const char* orderId) = 0;

    // -----------------------------------------------------------------
    // 数据完整性校验
    // -----------------------------------------------------------------
    // VerifyIntegrity: 校验病例数据完整性
    // 返回值: Success 表示数据完整，否则返回错误类型
    virtual ErrorCode VerifyIntegrity(const char* caseId, const char* orderId) = 0;

    // ComputeDataHash: 计算数据哈希值
    virtual ErrorCode ComputeDataHash(const char* caseId, const char* orderId,
                                      char* hashOut) = 0;

    // -----------------------------------------------------------------
    // 数据删除
    // -----------------------------------------------------------------
    // DeleteCaseData: 删除病例的所有数据
    virtual ErrorCode DeleteCaseData(const char* caseId) = 0;

    // DeleteOrderData: 删除订单的所有数据
    virtual ErrorCode DeleteOrderData(const char* caseId, const char* orderId) = 0;

    // -----------------------------------------------------------------
    // 数据导出
    // -----------------------------------------------------------------
    // ExportCaseData: 导出病例数据到指定目录
    virtual ErrorCode ExportCaseData(const char* caseId, const char* exportPath) = 0;

    // ImportCaseData: 导入病例数据
    virtual ErrorCode ImportCaseData(const char* importPath) = 0;

    // -----------------------------------------------------------------
    // 存储空间管理
    // -----------------------------------------------------------------
    // GetStorageUsage: 获取存储空间使用情况
    // totalSize: 总空间（字节）
    // usedSize: 已用空间
    virtual ErrorCode GetStorageUsage(int64_t* totalSize, int64_t* usedSize) = 0;

    // -----------------------------------------------------------------
    // 版本信息
    // -----------------------------------------------------------------
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_SCANDATAIO_API IScanDataIO* GetScanDataIO();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_SCANDATAIO_EXPORTS
#  define MEYERSCAN_SCANDATAIO_API __declspec(dllexport)
#else
#  define MEYERSCAN_SCANDATAIO_API __declspec(dllimport)
#endif
```

### 6.9 NetworkHelper.dll 接口

```cpp
// =============================================================================
// 文件:    NetworkHelper.h
// 模块:    MeyerScan_NetworkHelper.dll
// 用途:    云端上传/下载/同步、邮件发送
//
// 依赖:    Logger.dll, Core.lib, Crypto.dll, CaseEntity.lib
// 导出:    INetworkHelper 纯虚接口
//
// 已开发:  已完成，待集成
// =============================================================================

#pragma once

#include "Core.h"
#include "CaseEntity.h"

namespace MeyerScan {

// 云端状态枚举
enum class CloudStatus : int32_t {
    Offline       = 0,    // 离线
    Connecting    = 1,    // 连接中
    Online        = 2,    // 在线
    AuthRequired  = 3,    // 需认证
    Error         = 4,    // 错误
};

// 邮件信息结构（POD）
struct EmailInfo {
    uint32_t version;               // 结构体版本 = 1
    char     to[128];               // 收件人邮箱
    char     subject[256];          // 主题
    char     body[1024];            // 正文
    char     attachmentPath[256];   // 附件路径
    char     from[128];             // 发件人（可选）
    int32_t  reserved[8];
};

// 上传进度回调
using UploadProgressCallback = void (*)(int32_t progress, const char* status);

// 下载进度回调
using DownloadProgressCallback = void (*)(int32_t progress, const char* status);

// 网络助手接口
class INetworkHelper {
public:
    virtual ~INetworkHelper() = default;

    // -----------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------
    // cloudConfigPath: 云端配置文件路径
    virtual ErrorCode Init(const char* cloudConfigPath) = 0;

    // -----------------------------------------------------------------
    // 云端上传
    // -----------------------------------------------------------------
    // UploadCase: 上传病例数据
    // caseInfo: 病例信息
    // dataPath: 数据路径（ZIP 包）
    // onProgress: 进度回调
    virtual ErrorCode UploadCase(const CaseInfo& caseInfo, const char* dataPath,
                                 UploadProgressCallback onProgress) = 0;

    // UploadFile: 上传单个文件
    virtual ErrorCode UploadFile(const char* localPath, const char* cloudPath,
                                 UploadProgressCallback onProgress) = 0;

    // -----------------------------------------------------------------
    // 云端下载
    // -----------------------------------------------------------------
    // DownloadCase: 下载病例数据
    virtual ErrorCode DownloadCase(const char* caseId, const char* savePath,
                                   DownloadProgressCallback onProgress) = 0;

    // DownloadFile: 下载单个文件
    virtual ErrorCode DownloadFile(const char* cloudPath, const char* localPath,
                                   DownloadProgressCallback onProgress) = 0;

    // -----------------------------------------------------------------
    // 云端同步
    // -----------------------------------------------------------------
    // SyncToCloud: 同步本地数据到云端
    // syncType: 同步类型（"full" 或 "incremental"）
    virtual ErrorCode SyncToCloud(const char* syncType) = 0;

    // SyncFromCloud: 从云端同步数据到本地
    virtual ErrorCode SyncFromCloud(const char* syncType) = 0;

    // -----------------------------------------------------------------
    // 邮件发送
    // -----------------------------------------------------------------
    // SendEmail: 发送邮件
    virtual ErrorCode SendEmail(const EmailInfo& email) = 0;

    // SendCaseEmail: 发送病例数据邮件（自动打包附件）
    virtual ErrorCode SendCaseEmail(const char* caseId, const char* toEmail) = 0;

    // -----------------------------------------------------------------
    // 状态查询
    // -----------------------------------------------------------------
    // GetCloudStatus: 获取云端连接状态
    virtual CloudStatus GetCloudStatus() const = 0;

    // GetCloudAccount: 获取当前登录的云端账号
    virtual const char* GetCloudAccount() const = 0;

    // -----------------------------------------------------------------
    // 版本信息
    // -----------------------------------------------------------------
    virtual ModuleVersion GetModuleVersion() const = 0;

    // -----------------------------------------------------------------
    // 关闭
    // -----------------------------------------------------------------
    virtual ErrorCode Shutdown() = 0;
};

// DLL 导出工厂函数
extern "C" MEYERSCAN_NETWORKHELPER_API INetworkHelper* GetNetworkHelper();

} // namespace MeyerScan

// DLL 导出宏
#ifdef MEYERSCAN_NETWORKHELPER_EXPORTS
#  define MEYERSCAN_NETWORKHELPER_API __declspec(dllexport)
#else
#  define MEYERSCAN_NETWORKHELPER_API __declspec(dllimport)
#endif
```

---

## 七、调用时序图

### 7.1 患者/订单创建与参考数据读取流程时序

```
用户              OrderCreateUI.dll      CaseOrderService.dll       Database.dll        Logger.dll
 │                      │                      │                     │                    │
 │  [打开建单界面]        │                      │                     │                    │
 │──────────────────────>│                      │                     │                    │
 │                      │                      │                     │                    │
 │                      │ ListReferenceDataJson("doctor/clinic/lab") │                    │
 │                      │─────────────────────>│                     │                    │
 │                      │                      │  SELECT ms_reference_data                │
 │                      │                      │────────────────────>│                    │
 │                      │                      │  JSON rows          │                    │
 │                      │                      │<────────────────────│                    │
 │                      │  参考数据 JSON        │                     │                    │
 │                      │<─────────────────────│                     │                    │
 │  [填写患者/订单]       │                      │                     │                    │
 │──────────────────────>│                      │                     │                    │
 │                      │  CheckAccess("order.create")               │                    │
 │                      │─────────────────────────────────────────────────────────────────>│
 │                      │                      │                     │   (Permission.dll) │
 │                      │                      │                     │                    │
 │                      │  [权限校验通过]        │                     │                    │
 │                      │                      │                     │                    │
 │                      │  MEYER_LOG_INFO("SavePatientOrder", ... )   │                    │
 │                      │─────────────────────────────────────────────────────────────────>│
 │                      │                      │                     │                    │
 │                      │  SavePatientOrderJson(patientOrderJson)     │                    │
 │                      │─────────────────────>│                     │                    │
 │                      │                      │                     │                    │
 │                      │                      │  REPLACE/INSERT ms_patient_order         │
 │                      │                      │────────────────────>│                    │
 │                      │                      │                     │                    │
 │                      │                      │  MEYER_LOG_INFO(...) │                    │
 │                      │                      │─────────────────────────────────────────>│
 │                      │                      │                     │                    │
 │                      │  CaseOrderServiceResult                    │                    │
 │                      │<─────────────────────│                     │                    │
 │                      │                      │                     │                    │
 │  [进入工作台壳/扫描]    │                      │                     │                    │
 │<──────────────────────│                      │                     │                    │
 │                      │                      │                     │                    │
```

要点：

1. 医生、诊所、技工所、操作人等数据库主数据/字典数据统一从 `CaseOrderService.ListReferenceDataJson()` 或 `QueryJson("referenceData.list", ...)` 获取。
2. UI 不直接查 `Database.dll`，也不直接关心这些数据实际存储在单表、分表还是视图中。
3. 患者/订单组合数据通过 `SavePatientOrderJson()` 保存，字段变化通过 JSON 的 `schemaVersion`、稳定 key 和 `extensions` 吸收。
4. `Database.dll` 的 `ExecuteQueryJson()` 只做通用行列转 JSON，不理解患者、订单、医生、诊所或技工所业务语义。

### 7.2 扫描流程时序（跨进程）

```
用户          MeyerScan.exe      IPC(NamedPipe)     ScanReconstructStudio.exe      DeviceCmd.dll
 │                 │                   │                    │                       │
 │  [点击"开始扫描"]  │                   │                    │                       │
 │─────────────────>│                   │                    │                       │
 │                 │                   │                    │                       │
 │                 │  CheckAccess("scan.start")              │                       │
 │                 │───────────────────────────────────────────────────────────────>(Permission)
 │                 │                   │                    │                       │
 │                 │  LoadCase(caseId) │                    │                       │
 │                 │─────────────────> │                    │                       │
 │                 │                   │                    │                       │
 │                 │                   │  Cmd_LoadCase      │                       │
 │                 │                   │───────────────────>│                       │
 │                 │                   │                    │                       │
 │                 │                   │                    │  Connect(deviceId)    │
 │                 │                   │                    │──────────────────────>│
 │                 │                   │                    │                       │
 │                 │                   │                    │  StartScan(caseId)    │
 │                 │                   │                    │──────────────────────>│
 │                 │                   │                    │                       │
 │                 │                   │                    │  [设备开始采集]        │
 │                 │                   │                    │                       │
 │                 │                   │                    │  GetFrame(...)        │
 │                 │                   │                    │──────────────────────>│
 │                 │                   │                    │                       │
 │                 │                   │                    │  [帧数据回调]          │
 │                 │                   │                    │                       │
 │                 │                   │  Resp_FrameData    │                       │
 │                 │                   │<───────────────────│                       │
 │                 │                   │                    │                       │
 │                 │  [更新UI进度]      │                    │                       │
 │                 │                   │                    │                       │
 │                 │                   │  Cmd_Ping (心跳)    │                       │
 │                 │                   │───────────────────>│                       │
 │                 │                   │                    │                       │
 │                 │                   │  Resp_Pong         │                       │
 │                 │                   │<───────────────────│                       │
 │                 │                   │                    │                       │
 │  [完成扫描]      │                   │                    │                       │
 │                 │                   │  Cmd_CompleteScan  │                       │
 │                 │                   │───────────────────>│                       │
 │                 │                   │                    │                       │
 │                 │                   │                    │  StopScan()           │
 │                 │                   │                    │──────────────────────>│
 │                 │                   │                    │                       │
 │                 │                   │  Resp_Success      │                       │
 │                 │                   │<───────────────────│                       │
 │                 │                   │                    │                       │
 │  [显示扫描结果]   │                   │                    │                       │
 │<─────────────────│                   │                    │                       │
 │                 │                   │                    │                       │
```

### 7.3 进程状态同步与异常记录时序

```
MeyerScan.exe              IPC                  ScanReconstructStudio.exe
     │                      │                           │
     │  [发送订单上下文]      │                           │
     │─────────────────────>│                           │
     │                      │  Cmd_LoadCase / JSON ctx  │
     │                      │──────────────────────────>│
     │                      │                           │
     │                      │  Resp_Success             │
     │                      │<──────────────────────────│
     │  [进入扫描界面]        │                           │
     │                      │                           │
     │  [定期状态查询/Ping]   │                           │
     │─────────────────────>│                           │
     │                      │  Cmd_GetScanState/Ping    │
     │                      │──────────────────────────>│
     │                      │                           │
     │                      │  Resp_State/Pong          │
     │                      │<──────────────────────────│
     │  [更新 UI 进度/状态]   │                           │
     │                      │                           │
     │  [状态查询无响应]      │                           │
     │─────────────────────>│                           │
     │                      │  Cmd_GetScanState/Ping    │
     │                      │──────────────────────────>│
     │                      │                           │
     │                      │  [超时，无响应]            │
     │                      │                           │
     │  [记录异常状态]        │                           │
     │  [提示用户/修复订单状态]│                           │
```

说明：当前阶段重点规划 MeyerScan.exe 与 ScanReconstructStudio.exe 之间的订单上下文、扫描状态、处理进度同步，以及异常退出后的日志和订单状态修复。心跳/Ping 可作为状态检测手段，但暂不把“超时自动重启扫描进程”作为重点实现目标。
## 八、注释规范

### 8.0 强制注释规则

1. **每个函数必须有中文注释**：公开接口、内部函数、静态辅助函数、测试函数都必须在函数定义或声明附近说明用途、调用边界和注意事项。
2. **函数体内部也必须注释**：不能只在函数头写一段总说明。函数体内的关键判断、路径推导、资源申请/释放、Qt 父子关系、跨 DLL/跨进程调用、配置/权限合并、失败分支、降级策略、线程/事务边界都必须在代码附近写清楚“为什么这样做”和“不能怎么改”。
3. **关键代码必须有中文注释**：启动顺序、资源释放、日志初始化、权限合并、配置迁移、数据库事务、IPC、跨 DLL/跨进程边界、UI 切换、异常处理等逻辑必须写明“为什么这样做”。
4. **必须解释代码实现技巧**：注释不能只写“这个函数做什么”，还要解释“这段代码靠什么技术实现”。涉及 Qt Layout、Qt 父子对象、`deleteLater()` / 事件循环、信号槽、`QLibrary::resolve()`、`QJsonDocument` 解析、`QByteArray::constData()` 生命周期、调用方缓冲区、C ABI、RAII 锁、`QMutexLocker`、Windows API、文件版本资源、SQL 驱动、lambda 捕获等内容时，必须在代码附近说明机制和风险。
5. **接近逐行解释，但避免噪声**：复杂函数内部应对每个关键语句或连续代码块补注释，让初学维护者能顺着代码读懂实现路径；简单赋值、直接 `return`、普通 `include` 不需要机械逐行注释，但相邻注释必须覆盖其设计意图、生命周期或边界。
6. **测试项目同样适用**：测试宿主、smoke 入口、临时验证程序也要注释清楚测试目的、退出条件、路径来源、造数据原因、事件循环等待方式和预期结果；测试代码不能因为“只是测试”而省略内部注释。
7. **注释面向初学维护者**：默认代码阅读者不了解历史背景，注释要帮助读者判断职责边界、生命周期和禁止事项，不能只重复代码表面动作。
8. **中文注释 + UTF-8 BOM**：C++ 源码和头文件保存为 UTF-8 with BOM，避免 VS2015 按系统代码页 936 误解析中文注释和字符串。
9. **避免无意义注释**：禁止只写“设置变量”“调用函数”这类没有信息量的注释；应说明业务边界、设计原因、失败处理、代码机制和后续扩展点。
10. **第三方源码例外**：MySQL SDK、Qt 头文件、外部登录模块头文件等第三方/既有外部依赖不强制补中文注释；自研包装层、适配层、测试宿主和模块接口必须补齐中文注释，避免修改外部库导致升级困难。
11. **本轮落地范围**：2026-07-01 已在 2026-06-24 注释规则基础上再次复查 `F:\MeyerScan` 下自研 `.h/.cpp`，重点补强 ConfigCenter、Permission、UIComponents、CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、VersionManager、HomeUI、CaseUI、SettingsUI、RuntimeDataCenter、Database、Logger 和 MyLogin 测试宿主中的“实现技巧型注释”，尤其是 Qt、JSON、跨 DLL 缓冲区、动态加载、路径推导、事件循环、Windows API 和测试造数据链路。
12. **二次补强范围**：2026-07-02 继续按低注释密度和关键链路复查，重点补强 `SettingsUIImpl.cpp`、`RuntimeDataCenterImpl.cpp`、`MainWindow.cpp`、`SettingsUITest.exe`、`RuntimeDataCenterTest.exe`、`CaseUITest.exe` 等文件中的函数体内部说明；新增注释必须解释 QSS/objectName、Qt Layout 伸缩、`QStackedWidget` 内部分类页与 MainExe 业务页切换的区别、校准 DLL 懒加载、domain JSON 快照、旧表兼容、调用方缓冲区、版本 manifest、页面释放和测试宿主造数边界。
13. **注释必须独占物理行**：`//` 注释和后续代码不能处于同一物理行；代码从下一行开始。补丁后必须检查注释与代码没有因换行丢失而粘连。
14. **禁止注释行尾续行符**：`//` 注释末尾严禁反斜杠 `\`。C/C++ 在删除注释前执行“反斜杠+换行”拼接，错误写法会让下一行代码也进入注释。
15. **禁用代码不用块注释**：禁止用 `/* ... */` 包裹临时禁用代码；使用带原因和清理条件的 `#if 0`，避免嵌套注释或补丁换行破坏边界。
16. **静态检查与编译双验收**：批量改注释后先运行 `F:\MeyerScan\tools\CheckSourceCommentSafety.ps1`，再实际执行受影响目标的 VS2015 和 CMake 构建。静态扫描不能代替编译。
17. **资源脚本例外**：`.rc` 必须纯 ASCII 且无 BOM；含中文的 C/C++/头文件/PowerShell 脚本必须使用 UTF-8 BOM。二者由同一检查脚本分别约束。

### 8.1 文件头注释模板

```cpp
// =============================================================================
// 文件:    <FileName>.h/.cpp
// 模块:    <ModuleName>.dll/.lib
//
// 用途:
//   <一句话描述此文件的用途>
//
// 设计决策:
//   1. <关键设计决策点 1>
//   2. <关键设计决策点 2>
//   ...
//
// 线程安全:
//   <说明是否线程安全，如何实现>
//
// 使用示例:
//   <给出简要使用示例>
//
// 依赖:
//   <列出依赖的头文件和模块>
//
// 作者:    <AuthorName>
// 创建日期: <YYYY-MM-DD>
// 最后修改: <YYYY-MM-DD> by <AuthorName>
// =============================================================================
```

### 8.2 类注释模板

```cpp
// =========================================================================
// ClassName — <一句话描述类的职责>
// =========================================================================
// 这是 <模块名> 的 <主要/辅助/内部> 类。
//
// 职责:
//   1. <职责 1>
//   2. <职责 2>
//   ...
//
// 使用场景:
//   <说明何时使用此类，典型调用方式>
//
// 线程安全:
//   <说明是否可跨线程调用，内部如何同步>
//
// 生命周期:
//   <说明对象的生命周期管理方式>
//
// 禁止事项:
//   <说明禁止的用法>
//
// 相关类:
//   <列出相关的类名>
// =========================================================================
```

### 8.3 函数注释模板

```cpp
// -----------------------------------------------------------------
// FunctionName — <一句话描述函数功能>
// -----------------------------------------------------------------
// <详细描述函数的用途、行为、注意事项>
//
// 参数:
//   param1: <参数说明，包含单位、范围、有效值等>
//   param2: <参数说明>
//   ...
//
// 返回值:
//   <返回值说明，包含所有可能的值及其含义>
//
// 适用场景:
//   <说明何时应调用此函数>
//
// 调用示例:
//   auto result = FunctionName(arg1, arg2);
//   if (IsSuccess(result.error)) { ... }
//
// 线程安全:
//   <说明是否可跨线程调用>
//
// 异常情况:
//   <说明可能的错误情况及处理方式>
// -----------------------------------------------------------------
```

### 8.4 枚举注释模板

```cpp
// =========================================================================
// EnumName — <一句话描述枚举用途>
// =========================================================================
// 用于 <某场景>，值含义如下：
//
// 值排序说明:
//   <如有特殊排序规则，说明原因>
//
// 扩展约束:
//   <说明新增值时的约束>
// =========================================================================
enum class EnumName : int32_t {
    Value1 = 0,    // <值含义>
    Value2 = 1,    // <值含义>
    ...
};
```

### 8.5 结构体注释模板

```cpp
// =========================================================================
// StructName — <一句话描述结构体用途>
// =========================================================================
// 用于 <某场景> 的数据传递。
//
// POD 约束:
//   1. 第一个字段必须是 version（便于版本校验）
//   2. 禁止使用指针/QObject/动态分配
//   3. 预留 reserved 和 extData 字段
//
// 序列化说明:
//   <说明序列化方式、字节序、对齐等>
//
// 扩展规则:
//   新增字段必须放在末尾，version 递增
// =========================================================================
```

---

## 九、开发规约

### 9.1 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| **模块名** | `MeyerScan_<ModuleName>` | `MeyerScan_CaseOrderService` |
| **DLL 文件名** | `<ModuleName>.dll` | `MeyerScan_CaseOrderService.dll` |
| **静态库文件名** | `<ModuleName>.lib` | `MeyerScan_Core.lib` |
| **类名** | PascalCase，以职责命名 | `CaseOrderServiceImpl` |
| **接口名** | `I<ClassName>` | `ICaseOrderService` |
| **函数名** | PascalCase | `CreateCase()` |
| **参数名** | camelCase，带语义 | `caseId`, `timeoutMs` |
| **成员变量** | `m_<name>` | `m_initialized` |
| **静态变量** | `s_<name>` | `s_instance` |
| **常量** | UPPER_CASE | `MAX_FRAME_SIZE` |
| **枚举值** | PascalCase | `ErrorCode::Success` |
| **宏名** | UPPER_CASE | `MEYER_LOG_INFO` |

### 9.1.1 工程配置命名与模块名宏

每个 EXE/DLL 模块都必须在 `.vcxproj` 或 `CMakeLists.txt` 中定义 `MEYER_MODULE_NAME`，值固定为 `MeyerScan_<ModuleName>`。该宏用于 Logger 便捷宏自动填充模块名，缺失时日志模块字段会退化为 `Unknown`，后续排查跨模块问题会很困难。

```xml
<PreprocessorDefinitions>
  MEYER_MODULE_NAME="MeyerScan_CaseOrderService";%(PreprocessorDefinitions)
</PreprocessorDefinitions>
```

```cmake
target_compile_definitions(MeyerScan_Logger
    PRIVATE MEYER_MODULE_NAME="MeyerScan_Logger")
```

工程配置命名必须与模块清单一致：项目目录、产物名、`Version.rc` 的 `InternalName` / `OriginalFilename`、`MEYER_MODULE_NAME` 四者不得各写一套。测试宿主也要有独立模块名，例如 `LoggerTest` 或 `MeyerScan_LoginTest`，避免测试日志混入业务模块名。

### 9.2 代码风格

| 规范项 | 说明 |
|--------|------|
| **缩进** | 4 空格，禁用 Tab |
| **大括号** | 左大括号不换行 |
| **行宽** | 120 字符上限 |
| **空行** | 函数间 1 空行，逻辑块间可加空行 |
| **注释与记录语言** | 源码注释使用中文；模块 `CHANGELOG.md` 使用中文；GitHub commit message 使用中文；日志字段 key、内部错误 key、翻译 key 可保持稳定英文；UI 显示文字用 `tr("English source text")` 包裹，源码不写中文 UI source text |
| **头文件顺序** | 相关头文件 → C 系统头 → C++ 标准头 → 第三方头 → 本项目头 |
| **using namespace** | 禁止在头文件中使用，仅限 .cpp 内部小范围 |

### 9.3 错误处理规范

```cpp
// 规范：所有 Service 层接口必须返回 Result<T> 或 VoidResult
// 规范：调用方必须检查返回值

// 正确示例 1：有返回数据
Result<CaseInfo> result = caseService->CreateCase(info);
if (result.IsSuccess()) {
    CaseInfo createdCase = result.data;
    // 处理成功逻辑
} else {
    MEYER_LOG_ERROR("CreateCase", "", "", "",
                    ErrorCodeToString(result.error));
    // 处理失败逻辑
}

// 正确示例 2：无返回数据
VoidResult result = caseService->UpdateCase(info);
if (result.IsError()) {
    ShowErrorDialog(result.message);
    return;
}

// 错误示例：忽略返回值
caseService->DeleteCase(caseId);  // 禁止！必须检查返回值
```

### 9.4 日志规范

日志模块当前采用“统一文件、逐条同步写入、写完关闭句柄”的策略，不再使用后台缓冲线程。

文件规则：

1. **所有模块写入同一组日志文件**：MainExe、Database、UI、Service、扫描进程都写入 `MeyerScan.exe` 同级 `logs` 目录下的同一组日志文件。
2. **默认每天一个主文件**：当天日志默认写入 `MeyerScan_YYYYMMDD.log`。
3. **达到大小上限才分卷**：主文件达到上限后写入 `MeyerScan_YYYYMMDD_001.log`、`MeyerScan_YYYYMMDD_002.log`，默认单文件上限 10 MiB。
4. **每条日志写完关闭句柄**：写入流程为“格式化 -> 选择文件 -> 打开文件 -> 追加一行 -> `FlushFileBuffers` -> 关闭句柄”，后台可以移动、删除或打包日志文件。
5. **多进程顺序安全**：通过 Windows 命名互斥量串行化“选文件 + 写入”，避免多模块/多进程写同一文件时出现半行交错。

格式规则：

```text
[2026-06-24 08:42:14.893] [INFO] [MeyerScan_Database] [SetDatabaseType] Database type switched
```

1. **空字段不占位**：`deviceId`、`caseId`、`operator` 为空时不输出 `[Dev:-]`、`[Case:-]`、`[Op:-]`。
2. **无真实上下文传空字符串**：模块没有真实设备、订单或操作员上下文时传 `""`，不要为了占位传 `"System"`。
3. **有值才输出上下文**：有设备、订单、操作员时才输出 `[Dev:xxx]`、`[Case:xxx]`、`[Op:xxx]`。
4. **日志 key 保持稳定英文**：模块名、operation、字段 key 使用稳定英文，便于搜索、工具解析和多语言界面复用。

调用规则：

```cpp
// 推荐：每个模块 Init 阶段缓存一份 ILogger* 成员变量。
// 后续在该变量生命周期内持续使用 m_logger->Write(...) 输出日志。
m_logger = GetLogger();
m_logger->Write(LogLevel::Info, "MainExe", "Startup", "", "", "", "Logger initialized");
```

1. **每个模块只保存一份日志入口**：模块初始化时保存一份 `ILogger* m_logger`；需要动态加载 Logger 的模块也只解析一次 `GetLogger()` 并缓存结果，后续不要每次写日志都重新 `GetLogger()`。
2. **Qt 模块可直接输出 `QString`**：`QString` 支持位于 `Logger.h` 的头文件内联便利层，`Logger.dll` 本体仍不链接 Qt。
3. **Logger.dll 不强制引入 Qt**：日志是最早加载的基础设施，必须保持轻量；Qt UI/Service 模块的使用便利通过头文件适配解决。
4. **适配层可直接接入 Logger**：`MyDatabaseQtAdapter` 这类适配层属于跨模块边界，允许直接链接/使用 Logger 并缓存一份 `ILogger*`；日志重点记录输入非法、连接失败、类型转换失败、JSON 解析失败、缓冲区超限、切库/断开/关闭等边界事件。正常查询成功不逐条写日志，避免基础设施高频调用淹没客户操作和错误日志。

日志级别使用规范：

```cpp
// Debug:   仅开发调试时使用，客户稳定版本默认过滤
// Info:    正常操作事件（创建、启动、完成）
// Warning: 可恢复异常（重试成功、配置缺失使用默认值）
// Error:   功能异常，需通知用户或进入降级流程
// Fatal:   进程即将终止或进入不可用状态
```

日志内容规范：

1. 包含关键标识：`caseId`、`deviceId`、`operatorName`、`pageName`、`actionId`。
2. 内容简洁清晰，不堆无意义占位字段。
3. 日志只记录事实和关键参数，不替代业务返回码、权限校验和 UI 提示。

客户操作日志要求：

1. **每一步客户可见操作必须有日志**：按钮点击、菜单/工具栏、首页入口、浏览页返回首页、页签切换、搜索、打开订单、导入导出、删除、新建、登录状态、权限拒绝等都必须记录。
2. **跨模块导航由 MainExe 记录**：例如首页进入浏览、浏览返回首页、后续进入建单/设置/扫描入口，MainExe 统一写 `PageSwitch` 或同类操作日志。
3. **模块内操作由 UI 模块记录**：HomeUI 记录入口点击，CaseUI 记录浏览模块内按钮、搜索和页签切换；Service 后续记录业务成功/失败和关键状态变化。
4. **一类操作使用稳定 actionId/operation key**：便于后续按日志定位问题，不依赖当前显示语言。
5. **日志不替代业务校验**：权限拒绝、Workflow 拒绝、Service 失败都必须同时有返回码和日志，不能只写日志不处理错误。

### 9.5 禁止事项清单

| 禁止项 | 说明 |
|--------|------|
| 禁止跨层调用 | UI → Service → DAO → Entity，仅单向依赖 |
| 禁止循环依赖 | A 依赖 B，B 不能依赖 A |
| 禁止跨进程传递 QObject | IPC 仅允许 POD 结构体 |
| 禁止直接操作配置文件 | 必须通过 ConfigCenter |
| 禁止直接操作数据库 | UI 和业务流程必须通过对应 Service；Service 内部通过 DAO/Database.dll 访问数据库 |
| 禁止 UI 承载业务逻辑 | UI 仅做展示 |
| 禁止忽略返回值 | 所有 Service 调用必须检查 ErrorCode |
| 禁止硬编码配置 | 配置项必须从 ConfigCenter 读取 |
| 禁止在头文件使用 using namespace | 仅限 .cpp 内部 |
| 禁止传递 nullptr 到日志接口 | 空字段传 "" |
| 禁止修改已有接口签名 | 新增功能通过扩展接口实现 |

### 9.6 版本资源规范

#### 9.6.1 版本资源文件（Version.rc）

每个 EXE/DLL 模块必须在项目中包含一个 `Version.rc` 文件，编译后嵌入 Windows `VS_VERSION_INFO` 资源，使文件属性对话框显示正确的版本信息。

注意：Windows 资源管理器“属性 -> 详细信息”页读取的是 `Version.rc` 编译出的版本资源，不读取模块代码中的 `GetModuleVersion()`。如果 DLL 详细信息页没有版本号，优先检查 `.vcxproj` / CMake 是否把 `src/Version.rc` 编译进目标，以及 RC 内 `FILEVERSION`、`PRODUCTVERSION`、`StringFileInfo` 是否完整。

**Version.rc 模板**（以 Database.dll v1.1.0 为例）：

```cpp
// Version.rc — 每个 EXE/DLL 模块必须包含此文件
#include <windows.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION     1,1,0,0        // 二进制版本号（与 GetModuleVersion 一致）
 PRODUCTVERSION  1,1,0,0
 FILEFLAGSMASK   0x3fL
#ifdef _DEBUG
 FILEFLAGS       VS_FF_DEBUG
#else
 FILEFLAGS       0x0L
#endif
 FILEOS          VOS_NT_WINDOWS32
 FILETYPE        VFT_DLL        // DLL 用 VFT_DLL，EXE 用 VFT_APP
 FILESUBTYPE     0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"       // 英文（美国）, Unicode
        BEGIN
            VALUE "CompanyName",      "Hefei Meyer Optoelectronic Technology Co., Ltd."
            VALUE "FileDescription",  "MeyerScan Database Module"
            VALUE "FileVersion",      "1, 1, 0, 0"
            VALUE "InternalName",     "MeyerScan_Database"
            VALUE "OriginalFilename", "MeyerScan_Database.dll"
            VALUE "ProductName",      "MeyerScan Digital Dental Scanner"
            VALUE "ProductVersion",   "1, 1, 0, 0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```

#### 9.6.2 规范细则

| 规则 | 说明 |
|------|------|
| **必填字段** | `CompanyName`、`FileDescription`、`FileVersion`、`InternalName`、`OriginalFilename`、`ProductName`、`ProductVersion` |
| **版本号一致性** | `Version.rc` 的 `FILEVERSION`/`ProductVersion(字符串)` 必须与 `ModuleInfo::Version`、业务接口 `GetModuleVersion()` 和统一导出函数 `GetMeyerModuleVersion()` 返回的版本号一致；MainExe 运行时 versionList 会同时记录 `fileVersion` 和 `codeVersion` 并给出 `versionMatch` |
| **公司名统一** | `CompanyName` 固定为 `Hefei Meyer Optoelectronic Technology Co., Ltd.`，不得使用 `Meyer`、`MeyerScan Team` 等临时值 |
| **产品名统一** | `ProductName` 固定为 `MeyerScan Digital Dental Scanner`，便于 Windows 文件属性、版本清单和现场排查保持一致 |
| **描述格式** | DLL 使用 `MeyerScan <ModuleName> Module`；主程序使用 `MeyerScan Main Executable`；测试宿主使用 `MeyerScan <Name> Test Host` |
| **命名常量** | `FILEOS`、`FILETYPE`、Debug `FILEFLAGS` 必须使用 `VOS_NT_WINDOWS32`、`VFT_DLL` / `VFT_APP`、`VS_FF_DEBUG` 等 Windows SDK 命名常量，不写裸十六进制魔法值 |
| **FILETYPE** | DLL 模块使用 `VFT_DLL`，EXE 模块使用 `VFT_APP` |
| **版本升级规则** | 版本号遵循语义化版本（SemVer）：`主版本.次版本.修订号`。修改模块接口时递增主版本；新增向后兼容功能时递增次版本；Bug 修复时递增修订号 |
| **vcxproj 集成** | 在 `.vcxproj` 中添加 `<ResourceCompile Include="src\Version.rc" />`；在 `.vcxproj.filters` 中添加 Resource Files 筛选器分类 |

#### 9.6.3 版本清单（打包时生成）

每次正式构建/打包时，须生成 `version_manifest.json` 发布清单。该清单服务于安装包依赖核对，可覆盖随安装包发布的 EXE、DLL、Qt 运行库、Qt 插件、SQL 驱动、配置模板和资源；它不同于 MainExe 启动时写入 `logs/versionList/` 的运行时模块快照，后者只记录 `config/version_modules.json` 中声明的 MeyerScan 拆分模块 EXE/DLL。

运行时快照 `versionList_*.json` 当前包含：

1. `fileVersion`：从 Windows 版本资源读取，即 `Version.rc`。
2. `codeVersion`：通过 `version_modules.json` 中的 `versionFunction` 动态加载 DLL 后调用统一 C ABI 函数 `GetMeyerModuleVersion()` 读取；`MeyerScan.exe` 使用自身 `ModuleInfo::Version`。
3. `versionMatch`：比较文件版本和代码版本是否一致。允许 `0.4.0.0` 与 `0.4.0` 视为一致。
4. `codeVersionError`：DLL 加载失败、统一版本函数缺失或代码版本读取失败时记录错误，不阻断主程序启动。

`version_modules.json` 的 `versionFunction` 字段为空时，表示该模块没有可读取代码版本的统一 C ABI 函数，只记录文件版本。外部既有模块或第三方模块不强制补 `GetMeyerModuleVersion()`。旧字段 `factory` 仅作为历史清单兼容，不再作为新增模块推荐写法。

```json
{
  "product": "MeyerScan Digital Dental Scanner",
  "buildDate": "2026-06-17",
  "modules": [
    {"name": "MeyerScan_Logger.dll",      "version": "1.0.0", "type": "DLL"},
    {"name": "MeyerScan_Database.dll",     "version": "1.1.0", "type": "DLL"},
    {"name": "MeyerScan_CaseOrderService.dll", "version": "0.2.0", "type": "DLL"},
    {"name": "MeyerScan_OrderWorkflowService.dll", "version": "1.0.0", "type": "DLL"},
    {"name": "MeyerScan.exe",              "version": "1.0.0", "type": "EXE"}
  ]
}
```

打包发布清单的目的：
1. 发布前核对所有模块版本一致性
2. 现场排查版本不匹配问题（如 Logger.dll 旧版 + Database.dll 新版）
3. 追溯历史版本关系

#### 9.6.4 版本升级流程

```
1. 修改 Version.rc 中的 FILEVERSION / PRODUCTVERSION
2. 修改 `ModuleInfo::Version` 返回的版本字符串，并保证业务接口 `GetModuleVersion()` 和统一导出函数 `GetMeyerModuleVersion()` 都返回该值
3. 确认日志名、代码版本和文件版本一致
4. 如果新增模块，维护 MainExe `config/version_modules.json`，写入 `file` 和可选 `versionFunction`
5. CI 或打包脚本读取各模块 Version.rc 和发布依赖清单生成 version_manifest.json
6. 将 version_manifest.json 放入安装包
```

#### 9.6.5 安装打包规范（MyInstaller / Packaging）

安装打包暂作为独立发布交付模块规划，后续实现时再详细确定安装器技术选型、脚本目录、安装 UI 和回滚策略。当前先定义边界：

| 项目 | 规范 |
|------|------|
| **模块定位** | MyInstaller/Packaging 只负责发布交付，不作为 MeyerScan.exe 运行时插件 |
| **输入文件** | `MeyerScan.exe`、`MyUpdate.exe`、`ScanReconstructStudio.exe`、各 DLL、Qt 运行库、Qt 插件、默认配置、资源、帮助文档、`version_manifest.json` |
| **安装界面** | 支持品牌/产品名、语言、许可说明、安装路径、组件选择、安装进度、完成页、启动选项等自定义显示 |
| **安装流程** | 支持环境检查、旧版本检测、目录创建、文件复制、快捷方式创建、安装后初始化、卸载入口和失败提示 |
| **目录层级** | 固化安装目录层级，区分 `bin/`、`plugins/`、`ScanReconstructStudio/`、`platforms/`、`sqldrivers/`、`config/`、`resources/`、`docs/`、`data/`、`logs/`、`updates/` |
| **版本核对** | 打包前核对 EXE/DLL/Qt 运行库版本，禁止安装包出现来源不明或版本不匹配文件 |

> 运行时 `logs/versionList` 不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库；这些第三方依赖只在 MyInstaller/Packaging 的发布清单和依赖核对中管理。

建议安装目录层级：

```text
MeyerScan/
├── bin/
├── plugins/
├── ScanReconstructStudio/
├── platforms/
├── sqldrivers/
├── config/
├── resources/
├── docs/
├── data/
├── logs/
└── updates/
```

---

## 十、后续工作

### 10.1 待生成的详细接口头文件

| 模块 | 状态 | 文件名 |
|------|------|--------|
| **Logger.dll** | ✅ 已完成 | `Logger.h` |
| **公共稳定契约** | ⏸️ 按需抽取，当前不单独建项目 | 出现真实重复后再决定最小公共头/静态库 |
| **Database.dll** | ✅ 已完成（v1.2.0） | `Database.h`（含临时 ErrorCode/Result 定义） |
| **ConfigCenter.dll** | 🟡 骨架已完成（v0.1.0） | `ConfigCenter.h` |
| **Crypto.dll** | 🟡 待生成 | `Crypto.h` |
| **Permission.dll** | 🟡 骨架已完成（v0.1.0） | `Permission.h` |
| **患者订单数据合同** | 🟡 由 CaseOrderService 内维护 | 服务 DTO + UTF-8 JSON + schemaVersion/extensions |
| **CaseOrderService.dll** | 🟡 骨架已完成（v0.2.0） | `CaseOrderService.h` |
| **ScanSchemaService.dll** | 🟡 待生成 | `ScanSchemaService.h` |
| **DataExport.dll** | 🟡 待生成 | `DataExport.h` |
| **Statistics.dll** | 🟡 待生成 | `Statistics.h` |
| **DeviceCmd.dll** | 🟡 待生成 | `DeviceCmd.h` |
| **DeviceTransport.dll** | 🟡 待生成 | `DeviceTransport.h` |
| **ScanDataIO.dll** | 🟡 待生成 | `ScanDataIO.h` |
| **NetworkHelper.dll** | 🟡 待生成 | `NetworkHelper.h` |
| **OrderScanWorkspaceShell.dll** | 🟡 初版集成完成（v0.1.2） | `OrderScanWorkspaceShell.h` |
| **Calibration3DUI.dll** | 🟡 骨架已完成（v0.1.0） | `Calibration3DUI.h` |
| **CalibrationColorUI.dll** | 🟡 骨架已完成（v0.1.0） | `CalibrationColorUI.h` |
| **Login.dll** | 🟡 待生成 | `Login.h` |
| **UIComponents.dll** | 🟡 骨架增强（v0.4.0，已支持标准按钮角色/内容布局、字段标签、日期框、多行文本框和基础表格） | `UIComponents.h` |
| **UIResources.dll** | 🟢 已完成（v0.1.3，统一注册 PNG/QSS/qm 只读资源，包含建单五类型 b/h、1x/2x 图标与修正后 hover QSS） | `UIResources.h` |
| **VersionManager.dll** | ⏸️ 历史骨架保留，当前版本清单由 MainExe 内置生成 | `VersionManager.h` |
| **HomeUI.dll** | 🟡 框架增强（v0.3.1，参考背景、中英品牌和四入口响应式布局） | `HomeUI.h` |
| **CaseUI.dll** | 🟡 框架增强（v0.3.1，筛选栏、完整顶部动作和响应式订单卡片流） | `CaseUI.h` |

### 10.2 下一步任务

1. 优先为 ScanSchemaService、OrderWorkflowService、DataExport 等真实待接链路定义最小接口；Calibration3DUI / CalibrationColorUI 已有骨架接口，后续补真实流程接口。
2. ErrorCode/Result 先在模块边界稳定使用；出现多个模块真实重复且语义一致后，再迁入最小公共契约头。
3. 接口规范以仓库各模块 `include/` 中可编译头文件为权威源，文档只说明边界和示例，避免维护第二份不可编译副本。

### 10.2.1 UI 全局适配与多语言规则

多个模块都有 Qt 界面时，不能让每个 UI DLL 自己决定全局显示策略。统一规则如下：

1. **High DPI 属性只由进程入口设置**：MeyerScan.exe、ScanReconstructStudio.exe 和各测试宿主必须在创建 `QApplication` 前设置 `Qt::AA_EnableHighDpiScaling`、`Qt::AA_UseHighDpiPixmaps`。UI DLL 不在内部重复设置这些全局属性。
2. **不再以 1920x1080 绝对坐标等比缩放作为主布局方案**：1920x1080 只作为设计稿和图标/间距基准。窗口布局必须依赖 Qt Layout、伸缩因子、最小/最大尺寸、sizePolicy 和内容自适应。
3. **缩放系数仍可保留但只用于辅助尺寸**：UIComponents 可按实际屏幕与 1920x1080 计算 `scaleX/scaleY`，用于图标、边距、间距、少量固定规格控件；不得用它批量乘所有控件坐标。
4. **多语言布局不写 if/else 坐标分支**：按钮和标签必须预留文本弹性空间，必要时允许换行、省略号、tooltip、布局重排；翻译变长由布局消化，不由业务代码按语言调整控件位置。
5. **UI source text 固定英文**：所有界面可见文字必须写成 `tr("English source text")`，即使需求或帮助文档写的是中文按钮名，源码仍使用英文源文案，例如“回到首页”写 `tr("Back Home")`；中文、英文和其他语言显示由 `.qm` 翻译文件提供。
6. **窗口初始位置和屏幕适配统一下沉**：当前 HomeUITest/CaseUITest 已先实现“按当前屏幕可用区域限制大小并居中”的最小逻辑；正式实现迁入 `UIComponents.dll`，形成 `ScreenUtil` / `DpiUtil` / `LayoutRules`。
7. **UI 页面必须优先使用 Qt Layout**：禁止固定绝对坐标堆控件；页面级最小尺寸可以设置，但必须允许在不同分辨率和缩放比例下正常伸缩。
8. **公共样式和主题只放 UIComponents**：按钮、字段标签、输入框、下拉框、日期框、多行文本框、基础表格、弹窗、图标、边距、字体层级统一由 UIComponents 管理；HomeUI/CaseUI/OrderCreateUI 只描述自身页面结构和业务状态。当前 `QPushButton` / `QToolButton` 先按“按钮角色 + 内容布局”管理：角色包括 Primary/Secondary/Text/Danger/Entry，内容布局包括 TextOnly/IconOnly/IconLeftText/IconTopText。表格只统一基础外观和默认交互，列含义、数据源、分页、排序、右键菜单和双击动作仍归业务模块。业务模块负责按钮文案、权限状态、clicked 连接和动作 ID，UIComponents 只负责控件外观和基础尺寸。OrderCreateUI 的治疗方案图片/mask 命中、牙位叠加图、桥连接点、治疗类型业务图标和扫描流程输入属于建单业务控件，继续留在自身模块。
9. **特殊控件不强制公共化**：多个 UI 模块都会用到的控件、样式、弹窗和表格规则进入 UIComponents；只在某一个业务模块出现的截图还原控件、复杂设置页控件、单页面专用组合控件留在自身模块内部。是否放入 UIComponents 的判断标准是“至少两个以上模块复用，且不包含业务语义”，不是“看起来像控件就公共化”。
10. **主页面切换由 MainExe 集中管理**：HomeUI/CaseUI/OrderCreateUI 只暴露入口或操作事件，MainExe 用单内容区容器替换当前全屏页面；UI DLL 不直接持有其他 UI DLL 的 widget 指针，也不互相切换页面。
11. **主页面应按资源重量决定缓存或释放**：轻量页面可按白名单复用以保证切换无闪现；重资源页面离开时必须释放或暂停重资源，不把显存/大内存长期占住。从 CaseUI 进入 ScanReconstructStudio 前，MainExe 必须先切换到等待页并释放 CaseUI widget，不能只隐藏浏览界面。
12. **工作台重资源页离开即释放**：OrderScanWorkspaceShell 只保存步骤到 QWidget 的弱挂载关系；进入 Scan/Process 时由 MainExe 懒加载对应页面，离开时调用 `Shutdown()` / `DeactivateAndRelease()` 释放 QVTKWidget、VTK renderer、OpenGL/显存等资源，并用轻量占位页替换旧步骤，避免壳子继续持有等待删除的 QWidget。
13. **阶段性嵌入不改变最终进程边界**：当前 MainExe 直接动态加载 ScanWorkflowUI / DataProcessUI 并挂入 OrderScanWorkspaceShell，是为了先跑通创建/练习工作台和资源释放链路；真实设备、算法、扫描状态同步和异常隔离仍以 ScanReconstructStudio.exe 独立进程作为最终边界。
14. **多语言采用模块化 qm**：每个 UI 模块维护自己的翻译文件，公共控件维护 Common 翻译文件。例如：

```text
translations/
  MeyerScan_Common_zh_CN.qm
  MeyerScan_HomeUI_zh_CN.qm
  MeyerScan_CaseUI_zh_CN.qm
  MeyerScan_OrderCreateUI_zh_CN.qm
```

13. **翻译加载由 LanguageManager 统一处理**：主 EXE 根据配置加载 Common qm 和当前已加载 UI 模块 qm；语言切换时统一卸载/重载 translator，并通知 UI 刷新。模块 DLL 不各自读取语言配置。
14. **业务层不返回翻译字符串**：Service/Workflow/Permission 返回 `ErrorCode`、`ReasonCode`、`FeatureId` 或稳定英文 key；最终展示文案由 UI 模块结合当前语言翻译。

### 10.2.2 运行路径、版本清单与启动准备规则

1. **应用目录是唯一运行基准**：日志、配置、图标资源、语言文件、版本清单默认从 `QCoreApplication::applicationDirPath()` 推导；第三方软件拉起 MeyerScan.exe 时，工作目录可能不是安装目录，禁止用 `QDir::currentPath()`。
2. **日志目录固定在 EXE 同级 `logs/`**：Logger 初始化路径由 MainExe 或 ConfigCenter 传入，模块内部不自行猜测路径。
3. **版本清单当前由 MainExe 内置生成**：MeyerScan.exe 启动时读取 `config/version_modules.json`，只记录清单中声明的拆分模块 EXE/DLL，输出 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`。文件名包含毫秒，避免同一秒内连续 smoke、第三方拉起或重复启动覆盖现场快照。Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库不进入该运行时清单。当前 `version_modules.json` 使用 schemaVersion=2，模块项包含 `file` 和可选 `versionFunction`；自研 DLL 固定导出 `GetMeyerModuleVersion()`，MainExe 读取该函数作为 `codeVersion`，并与 Windows `Version.rc` 文件版本 `fileVersion` 比对输出 `versionMatch`。当前不依赖 `MeyerScan_VersionManager.dll`，减少运行时 DLL 和调试点；历史 VersionManager 骨架也必须读取同一 manifest，不得恢复目录全量扫描；后续若扩展算法 DLL、哈希、签名、云端比对和自动更新策略，再恢复独立 VersionManager。
4. **MainExe 自研模块动态加载规则**：MainExe 对 Logger、ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter 等自研模块优先使用 `QLibrary + extern "C" GetXxx()` 运行时加载；工程仍包含接口头文件用于强类型编译检查，但不链接这些模块的 import lib。Qt、Windows `Version.lib`、当前既有登录模块 `MeyerLoginWidget.lib` 可暂时保持现有链接方式。动态加载路径必须基于 `QCoreApplication::applicationDirPath()`，不依赖 current directory；加载失败必须写日志或状态并阻断当前功能，不能继续使用空接口。
5. **第三方运行库复制路径**：VS2015 PostBuild 不得写个人开发机绝对路径（如 `D:\wj\My-wj\...`）。当前 SQLite 运行时统一从 `F:\MeyerScan\ThirdParty\SQLite\win-x64\sqlite3.dll` 复制到各模块 `bin\Release` 和根聚合 `bin\Release`；该 DLL 必须是 x64，禁止再从旧 `MyCaseManager\SQLite\sqlitestudio311\SQLiteStudio` 目录复制，因为旧目录中的 `sqlite3.dll` 是 32 位，会导致 x64 程序 `LoadLibraryA("sqlite3.dll")` 失败。第三方 DLL 本体不提交 GitHub/本地仓库，只提交同目录 README 说明；后续由 MyInstaller/Packaging 的发布清单正式收口。Qt、登录模块等外部既有依赖若暂时必须使用固定安装路径，必须在模块 README/CHANGELOG 说明来源。
6. **启动检查由 MainExe 编排**：数据库检查、配置检查、资源文件存在性检查、版本清单生成等由 MainExe 调度各模块完成；等待页只展示进度和状态。
7. **等待页归 UIComponents**：等待界面、公共弹窗、Toast、确认框等通用 UI 能力先放 UIComponents；不单独开弹窗模块，避免过早拆分。
8. **单实例由主 EXE 控制**：正常运行时只允许一个 MeyerScan.exe；再次双击时通知已运行实例激活窗口。若旧实例仍处于数据库检查或登录窗口阶段，可忽略激活请求。
9. **配置和权限共同控制 UI 显隐与启用态**：`runtime_config.json` 由 ConfigCenter 读取，表示产品/客户默认策略；`permission_rules.json` 由 Permission 读取，表示授权结果；MainExe 按 `配置默认值 && 权限 visible` 合并最终显隐，并把 `enabled` 下发给 HomeUI/CaseUI 设置禁用态。UI 模块不直接读取配置或权限文件；MainExe、Workflow、Service、IPC 接收端必须在动作入口继续复核 `enabled`。
10. **等待页和单实例是固定流程**：`showWaitPage`、`singleInstance` 不属于运行配置项，不写入 `runtime_config.json`。旧配置中如残留 `startup.showWaitPage` / `startup.singleInstance`，ConfigCenter 初始化时应自动迁移清理。
11. **UI 模块不关闭全局基础设施**：HomeUI/CaseUI/SettingsUI 只借用进程级 Logger；正式 UI 不直接访问 Database，也不做数据库健康检查。Database 由 MainExe 通过 DatabaseQtAdapter 在启动期统一初始化和收尾；UI 模块 `Shutdown()` 不得关闭进程级 Logger/Database 单例。
12. **JSON 配置说明文件**：`runtime_config.json`、`permission_rules.json`、`version_modules.json` 内部不得写注释；字段含义、默认值、使用场景写在同级 md 文件中，例如 `runtime_config.md`、`permission_rules.md`、`version_modules.md`。
13. **Resources 目录规则**：登录离线许可文件继续放在 `Resources/license.lic`；模块私有图标、图片、mask、QSS、模块内 qm 的源码放在各模块 `Resources/`，构建时由 MyUIResources 统一编译进 `MeyerScan_UIResources.dll`，运行路径为 `:/MeyerScan/Modules/<ProjectName>/...`。正式安装不再展开 UI 散文件；源码调试可降级读取模块 `Resources`，旧安装包可短期兼容 `Resources/Modules/<ProjectName>`。运行定位以应用目录为基准，禁止使用 `QDir::currentPath()`。
14. **日志字段分类标记**：Logger 输出固定使用 `[Mod:] [Op:] [Content:]`，可选上下文使用 `[Dev:] [Case:] [Opr:]`；空字段省略，不输出 `-` 占位。后续日志分析工具按这些字段解析，模块不得私自改格式。

### 10.2.3 数据库脚本与迁移规则

`F:\MeyerScan\MyCaseManager\mysql.sql` 已检查，当前 MyDatabase/MyHomeUI/MyCaseUI 均未自动读取或执行该文件。后续规则如下：

1. `mysql.sql` 只作为旧版字段和旧表关系参考，不作为新系统启动脚本直接执行。
2. 新系统建表和迁移必须版本化，例如 `migrations/mysql/V001_init_case.sql`、`migrations/sqlite/V001_init_case.sql`。
3. 由 ConfigCenter 或病例域服务决定当前数据库版本、目标版本和需要执行的迁移脚本。
4. Database.dll 只负责事务、执行 SQL、返回错误和日志，不判断“病例表应该长什么样”。
5. MySQL 与 SQLite 的 schema 差异必须显式维护，不能靠运行时字符串拼接临时兼容。

### 10.3 近期设计决策记录

| 决策项 | 结论 | 原因 |
|--------|------|------|
| **Database 返回值风格** | 返回 Result\<T\>/VoidResult | 与架构规范一致 |
| **ErrorCode/Result 定义位置** | 当前留在各模块边界；只有真实重复且语义一致时才抽取最小公共契约 | 避免为形式统一先建公共大包 |
| **ModuleVersion 返回格式** | 简单字符串 `"ModuleName vX.Y.Z (YYYY-MM-DD)"` | 调试查看方便，不依赖 Core.lib |
| **注释与记录语言** | 源码注释用中文；模块 `CHANGELOG.md` 用中文；GitHub commit message 用中文；日志字段 key、内部错误 key、翻译 key 可保持稳定英文；UI 文字用 `tr("English source text")` 包裹，源码不写中文 UI source text | 团队理解 + 人工维护 + 国际化 |
| **注释内容深度** | 函数注释必须覆盖“做什么”；函数体内部注释必须覆盖“为什么这样做”和“靠什么代码机制实现”，尤其是 Qt、JSON、C ABI、跨 DLL 缓冲区、事件循环、Windows API、SQL、RAII 和测试造数据等实现技巧 | 让初学维护者不只知道功能，还能理解实现手法，减少后续维护时误改生命周期、路径和模块边界 |
| **Database 集成 Logger** | LoadLibrary 运行时动态加载 | 避免 CRT 冲突（/MT vs /MD） |
| **Database 角色** | 通用基础设施（方案 A），提供连接、事务、SQL 执行、备份和 MySQL/SQLite 适配；正式业务数据访问必须通过 Service 内部 DAO 调用 | 避免 UI/Workflow 直接拼 SQL，把业务语义重新塞回界面层 |
| **Database 去 Qt 与 Adapter** | Database 已改为纯 C++；SQLite 通过 sqlite3 C API；Qt 模块经 `MyDatabaseQtAdapter` 转换 `QString` / `QJsonDocument` 与 Database 的 UTF-8/POD 接口 | 基础设施减少 Qt 依赖，同时不让 Qt 转换逻辑散落到 UI/Service |
| **Database 配置路径解析** | `mysql.dataDir`、`sqlitePath` 支持相对路径，统一按 `db_config.json` 所在目录解析；测试宿主可生成独立测试配置 | 正式发布配置和测试配置语义分离，避免运行时回退开发机绝对路径或依赖当前工作目录 |
| **旧 mysql.sql 定位** | `F:\MeyerScan\MyCaseManager\mysql.sql` 只作为旧版表结构参考，不由 HomeUI/CaseUI/Database 自动硬编码加载 | 旧脚本包含历史表、旧字段和旧编码痕迹，直接自动执行会把旧业务语义塞回基础设施层 |
| **数据库建表/迁移职责** | Database.dll 只提供连接、事务、裸 SQL 执行和 ExecuteScript；业务表结构由 ConfigCenter/病例域服务按版本化脚本选择并调用 Database 执行 | Database 不理解病例/订单/权限语义，避免变成隐形业务层 |
| **UI 不直连 Database** | HomeUI 不访问 Database；CaseUI/SettingsUI 正式代码只读 RuntimeDataCenter 快照；测试宿主造数可经 DatabaseQtAdapter 调用 Database | 防止 UI 模块重新变成数据库入口 |
| **基础设施生命周期规则** | Database 由 MainExe 通过 DatabaseQtAdapter 做启动期初始化；UI 模块 Shutdown 不关闭进程级 Logger/Database；测试宿主退出时可自行收尾 | MainExe 统一管理基础设施生命周期，避免多个 UI DLL 互相重置全局状态 |
| **VS2015 源码编码** | C++ 源码保存为 UTF-8 with BOM | VS2015 按系统代码页解析无 BOM UTF-8 时会破坏中文注释和字符串 |
| **公共方法模块边界** | 当前不建通用 Core.lib；公共头/静态库只按需放稳定契约和少量无状态工具 | 防止公共模块膨胀成隐形业务层 |
| **首页模块** | 保留 HomeUI.dll，但只做入口和 OEM 展示定制 | 首页变化频繁，独立 DLL 有价值；业务规则不得进入首页 |
| **界面切换边界** | MainExe 统一负责首页、浏览、建单、设置等主页面切换；UI 模块只发入口/操作 ID；MainExe 单内容区一次只挂载一个全屏页面，离开页面按资源重量释放或复用 | 防止 UI DLL 互相依赖，避免顶层窗口闪现，同时避免不可见重资源页面长期占用内存/显存 |
| **扫描前资源释放** | 从案例管理进入 `ScanReconstructStudio.exe` 前，MainExe 必须先切到等待页、释放 CaseUI widget、处理 Qt 延迟删除事件，再启动扫描重建进程 | 确保浏览模块表格、缩略图、查询结果等资源不会和扫描重建争抢内存/显存 |
| **MainExe 集成 smoke** | `--smoke-main` 必须自动覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前释放 CaseUI，并检查对应结构化日志 | 将模块交互、页面生命周期和资源释放纳入自动回归，不只验证启动成功 |
| **单实例激活细则** | 重复启动只在登录完成且主窗口可见后激活旧窗口；数据库检查、等待页或登录窗口阶段不抢占前台 | 避免重复双击打断初始化或登录流程 |
| **客户操作日志** | 客户每一步可见操作和关键内部步骤都必须写结构化日志；UI 记模块内操作，MainExe 记跨模块导航 | 后续现场问题需要通过日志还原用户操作链路 |
| **加载订单规则** | 新增 OrderWorkflowService.dll | 新建/加载/继续扫描/发送规则跨多个 UI 和扫描进程复用，必须集中管理 |
| **功能阉割策略** | 模块清单不加载 + Permission 拒绝 + Service/Workflow/IPC 复核 | 只隐藏按钮不能达到预期安全目的 |
| **Qt UI 运行库** | HomeUI/CaseUI/SettingsUI 输出目录必须复制编译所用 Qt 5.6.3 的 Qt5Core/Qt5Gui/Qt5Widgets、platforms/qwindows；QtSql 仅在外部既有登录模块确需时保留 | UI 模块必须能独立运行；运行库版本必须和编译版本一致，不能混用文档同级目录附近 SQLiteStudio 的 Qt 5.7 DLL |
| **多屏/DPI 适配边界** | 主 EXE 和测试宿主在 QApplication 创建前设置 High DPI 属性；UIComponents 后续统一提供 ScreenUtil/DpiUtil/LayoutRules；各 UI DLL 只使用布局和公共工具，不抢全局 DPI 策略 | 多个界面模块必须行为一致，避免每个 DLL 各自适配导致窗口尺寸、字体和缩放策略不一致 |
| **多语言 qm 策略** | 采用 Common qm + 每个 UI 模块独立 qm；LanguageManager 统一加载、切换、回退；Service/Workflow 返回 ErrorCode/ReasonCode/FeatureId，不返回已翻译显示文案；所有 UI source text 统一英文并用 `tr()` 包裹 | UI 文案归 UI 翻译，业务模块只返回稳定代码，便于多语言和客户定制 |
| **UIComponents 控件工厂边界** | 当前提供等待页、标题、字段标签、按钮、输入框、下拉框、日期框、多行文本框、基础表格等工厂；按钮按“角色 + 内容布局”统一管理，角色包括 Primary/Secondary/Text/Danger/Entry，内容布局包括纯文字/纯图标/左图右文/上图下文；表格只统一基础外观和默认交互，不决定业务列和数据；只统一尺寸、样式和多语言适配基础，不承载点击行为、配置读取或权限判断；新增虚接口只能追加在接口末尾，不能插入旧接口中间；动态加载并调用新增虚接口的模块必须做运行时版本检查，不满足时走降级 | 多个 UI 模块保持一致体验，同时避免共享 UI 模块变成业务中心，并降低 DLL ABI 兼容风险 |
| **GitHub 模块目录** | `F:\MeyerScan` 是 Git 根目录；GitHub `weijian118/MeyerScan` 下每个模块以一级文件夹提交和记录 | HomeUI/CaseUI 必须与 MyLogger/MyDatabase 同级，形成 `MyHomeUI/`、`MyCaseUI/` 等独立模块目录，便于后续单模块维护和追踪 |
| **模块内变更记录** | 每个模块目录维护中文 `CHANGELOG.md`，按时间记录修改、验证和注意事项 | 后续人工维护时不用只靠 Git log 猜上下文 |
| **GitHub 提交日志** | commit message 统一使用中文，描述具体变更内容和影响范围，避免“更新/修复/修改”这类空泛信息 | 保证提交历史能被维护人员直接阅读和追溯 |
| **安装打包模块** | MyInstaller/Packaging 负责安装包生成、自定义安装界面、自定义安装流程、安装目录层级、依赖收集和版本清单写入 | 发布交付过程独立管理，避免人工复制 DLL 和目录导致安装包不可追溯 |
| **轻量化拆分尺度** | 模块小有利于人工维护，但必须满足边界清晰、接口少、依赖单向、可独立验证 | 防止为了“拆得细”引入过多 DLL、过多接口和过高调试成本 |
| **患者订单合同形态** | 由 CaseOrderService 内 DTO + 版本化 UTF-8 JSON 管理，使用 schemaVersion、extensions 和 schema 映射；当前不建 CaseEntity.lib | 避免字段变化导致大量模块频繁重编译或 ABI 不稳定 |
| **跨进程订单上下文** | IPC 消息头保持 POD；复杂患者/订单信息用 UTF-8 JSON payload、上下文文件路径或订单 ID 传递 | 兼顾字段扩展便利性和跨进程稳定性 |
| **自动更新模块** | MyUpdate.exe 独立进程，与 MeyerScan.exe 同级；支持按钮检查更新和双击主动更新 | 覆盖升级必须脱离正在运行/被覆盖的主程序 |
| **校准拆分** | 三维校准和颜色校准拆为 `Calibration3DUI.dll` 与 `CalibrationColorUI.dll`，各自包含 UI、流程和计算入口 | 两类校准现场维护频率和风险不同；UI 与流程强绑定，拆成两个完整校准模块比“三段式入口+算法”更易维护 |
| **病例域服务合并** | 患者/订单不再拆为 `CaseService.dll` 与 `OrderService.dll`，统一为 `CaseOrderService.dll`；扫描方案、流程规则、导入导出、统计仍保持独立 | 患者和订单在口扫软件与数据库中强绑定，强拆会增加接口和一致性维护成本；合并后由 JSON/DTO 边界吸收字段变化 |
| **权限配置 UI 拆分** | Permission 只保留权限核心，PermissionConfigUI 负责展示和扫码授权入口 | 避免权限热路径被 UI 依赖污染 |
| **扫描进程内部拆分** | ScanReconstructStudio 保持独立 EXE 壳；当前已落地 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll` 两个阶段 UI DLL；编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等业务/算法处理能力后续继续优先拆 DLL 或独立库 | 保留崩溃隔离，同时避免扫描 UI、数据处理 UI、算法处理和设备协议混成一个难维护大工程 |
| **扫描第三方依赖路径** | 扫描相关 VS2015 工程统一引用 `cmake/MeyerScanScanThirdParty.props`，CMake 统一引用 `cmake/MeyerScanScanThirdParty.cmake`；路径优先读 `QT_ROOT`、`VTK_ROOT`、`VTK_HEADERS_ROOT`、`OPENCV_ROOT`，再尝试仓库 `ThirdParty` 和当前开发机参考路径 | 便于把工程复制到其他电脑后少改路径；第三方库路径不散落在各模块项目文件中 |
| **扫描 CMake 验证状态** | 2026-07-06 已安装 CMake 3.31.6（`F:\Tools\CMakePython\cmake\data\bin\cmake.exe`），并使用 VS2015 x64 生成器完成根聚合工程 `Release` 配置和构建；扫描 UI / 数据处理 UI / ScanReconstructStudio 均纳入 CMake 验证 | 文档必须区分“工程文件已写好”和“实际 configure/build 已验证”；后续复制到其他电脑时优先检查 CMake 路径、Qt/VTK/OpenCV 环境变量和 VS2015 生成器 |
| **CMake 双构建入口** | 每个模块和测试宿主都必须有 `CMakeLists.txt`；根目录 `F:\MeyerScan\CMakeLists.txt` 作为 VSCode/CMake Tools 聚合入口，VS2015 继续保留 `.sln/.vcxproj` | 支持 VSCode 和 VS2015 双环境编译，也便于移植到其他电脑 |
| **本地备份仓库** | `F:\MeyerScan-Reposit` 作为本地整体备份仓库；除 Qt、VTK、OpenCV、PCL、VC/UCRT、OpenSSL、AWS、SQL 驱动等第三方库外，自研源码、测试项目、工程文件、CMake、文档、配置模板和自研 DLL/EXE/LIB 都要整体备份 | GitHub 之外增加离线恢复点；每次备份所有模块一起提交，避免跨模块状态不一致 |
| **本地备份提交日志** | 本地仓库提交日志使用中文，并详细描述本次变更内容、影响模块和验证结果 | 便于人工追溯完整工程快照，而不是只看单模块历史 |
| **本地备份过滤规则** | 统一使用 `tools/BackupToLocalRepository.ps1`；脚本只同步自研源码、测试项目、VS2015 工程、CMake、配置说明、README/CHANGELOG、重构 Markdown 快照和自研 DLL/EXE/LIB。Qt 插件、VC/UCRT、OpenSSL、AWS、MySQL/SQLiteStudio/SQL 驱动、日志、数据库现场文件、IDE 临时文件必须过滤。镜像完成后必须主动清理目标仓库中被 `/XD`、`/XF` 排除的历史遗留内容，并验证排除规则命中数为 0；脚本保存为带 BOM 的 UTF-8，以兼容 Windows PowerShell 5.1 | 避免本地仓库变成第三方依赖仓库或运行现场仓库，并防止排除规则只阻止新增却无法清除旧提交内容 |
| **文档更新** | 代码改动完成后统一更新 4 个 MD 文档 | 保证文档与实际代码一致 |
| **测试宿主覆盖规则** | 当前活跃自研 DLL 模块必须有独立 `*Test.exe`、模块 `.sln`、测试 `.vcxproj` 和同模块 `CMakeLists.txt` 测试目标；主 EXE 使用 `MeyerScan.exe --smoke-main` 验证主链路；根 `MeyerScan_AllModules.sln` 必须纳入活跃自研测试项目 | 测试入口随模块存在，既能单模块调试，也能在根输出目录暴露跨模块依赖和复制问题 |
| **HomeUI/CaseUI 框架** | 已创建 Qt Widgets DLL 框架，接入 Logger、UIComponents、RuntimeDataCenter 和 MainExe 回调；数据库健康检查由 MainExe + DatabaseQtAdapter 统一处理 | 先跑通 UI 模块生命周期和底座依赖，避免 UI 直连 Database 扩散 |
| **MyLogin 测试宿主** | 新增 `F:\MeyerScan\MyLogin`，输出 `MeyerLoginTest.exe`，只负责验证既有 `MeyerLoginWidget.dll` 可被新仓库工程拉起 | 登录模块本身已由外部完成，当前不重写登录业务；先验证 DLL、qm、许可文件和间接依赖完整 |
| **MainExe 最小集成链路** | 新增 `F:\MeyerScan\MyMainExe`，输出 `MeyerScan.exe`，当前流程为 Logger → ConfigCenter/Permission/UIComponents → DatabaseQtAdapter + Database 健康检查 → RuntimeDataCenter 全域刷新 → Login → HomeUI → CaseUI；HomeUI 浏览入口和 CaseUI 返回首页均通过回调交给 MainExe 切换 | 主 EXE 只做壳和编排，不承载业务规则；Database 只通过 DatabaseQtAdapter 接入，现有模块继续逐步接入 Service/Workflow |
| **HomeUI/CaseUI 事件接口** | HomeUI 通过 `SetEntryCallback()` 上报入口 ID；CaseUI 通过 `SetActionCallback()` 上报操作 ID；MainExe 使用单内容区替换页面并写 `PageSwitch` 日志；HomeUI/CaseUI 另接收 `visible/enabled` 最终状态 | 当前已满足首页进浏览、浏览回首页的基本导航链路，并降低切换闪现风险 |
| **登录头文件风险** | 既有 `MeyerLoginWidget.h` / `globalLoginValue.h` 存在 VS2015 代码页/声明警告，当前只读取 `LoginReturnParameters.currentStatus` | 避免把不稳定字段扩散到 MainExe；后续用 `LoginAdapter` 将登录 DLL 的参数和返回状态转换为稳定本地类型 |
| **Release 依赖闭包** | MyLogin/MyMainExe PostBuild 必须复制登录模块间接依赖：Qt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi、平台插件、SQL 驱动等 | 运行目录必须能独立启动；安装包依赖收集不能只看一层 DLL |
| **MyMainExe 复测结论** | 2026-06-22 重新验证 `MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerLoginTest.exe --smoke` 均返回 0，Release 依赖闭包无非 API Set DLL 缺失 | 当前 MainExe 可继续作为最小集成壳；下一阶段不应继续在 MainExe 中增加业务规则，而应接入 LoginAdapter、ConfigCenter、Permission 和 Service/Workflow |
| **基础设施骨架接入** | 2026-06-23 新增并接入 ConfigCenter、Permission、UIComponents；版本清单能力当前由 MainExe 内置生成；MainExe 启动阶段生成 `logs/versionList`，使用等待页、单实例检查和权限显隐 | 先打通配置、权限、共享 UI、版本清单的最小闭环；VersionManager 骨架保留，后续复杂化再独立 |
| **页面资源释放** | MainExe 当前按需创建首页、浏览页和等待页，切换后释放非当前页面 widget；释放使用 `deleteLater()`；已预留扫描前准备流程，后续打开扫描重建前先释放 CaseUI | 回应资源占用约束，同时避免在按钮点击信号尚未返回时直接销毁控件树 |
| **运行路径复查** | 2026-06-23 删除 MainExe 数据库配置开发路径回退；2026-06-25 登录许可运行参数改为应用目录 `Resources/license.lic`；HomeUI/CaseUI 测试宿主改为按 exe 所在目录推导日志和配置路径 | 避免第三方拉起或安装目录变化时误读开发机路径 |
| **VS2015 聚合解决方案** | `F:\MeyerScan\MeyerScan_AllModules.sln` 用于一次打开当前已拆分模块工程，并纳入当前活跃自研模块的测试宿主；外部既有登录 DLL 与交互登录测试宿主独立维护，不作为根方案自动测试前置条件 | 满足人工维护时一次浏览多个模块的需求，同时不把外部已完成模块混入当前仓库工程 |
| **模块信息来源** | 每个模块维护 `ModuleInfo::Name` / `ModuleInfo::Version` 或等价结构；`Name` 与 `MEYER_MODULE_NAME` 保持一致，`Version` 与 `Version.rc` / `GetModuleVersion()` / `GetMeyerModuleVersion()` 保持一致；版本以 `Version.rc` 为文件属性权威源，以 `GetMeyerModuleVersion()` 为运行时清单代码版本读取入口。已开发模块中 Logger、Database、ConfigCenter、Permission、UIComponents、HomeUI、CaseUI、CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、VersionManager、MainExe 均应按该规则维护。 | 日志、版本清单、文件属性和人工阅读看到同一套模块信息，避免多处版本漂移 |
| **显隐合并规则** | 首页“设置”和浏览“返回首页”当前由 ConfigCenter 与 Permission 同时允许才显示 | 配置负责产品/客户默认策略，Permission 负责授权过滤，两层边界清楚 |
| **glm52 建议采纳原则** | 立即采纳低风险工程一致性建议：统一 `Version.rc` 公司名/产品名/描述/命名常量，补齐 `MEYER_MODULE_NAME`，给 `CaseOrderServiceResult` 增加 `IsSuccess()` / `IsError()`；2026-07-03 已完成 HomeUI/CaseUI 正式代码移除 Database 直连，并新增 DatabaseQtAdapter 收口 Qt 到纯 C++ Database 的转换 | 工程配置可马上统一；ConfigCenter/Permission 返回值、CaseOrderService 结果类型、Logger 导出宏等涉及 ABI 或多模块调用链的建议仍需按阶段集中迁移，避免为形式统一破坏已跑通链路 |
| **骨架接口迁移规则** | ConfigCenter/Permission 当前 `bool Init(...)` 属于骨架期过渡接口，头文件必须写明 TODO；待真实失败语义稳定后升级为带错误原因的结果类型，是否抽公共类型另行评估 | 防止骨架接口被误认为最终接口，也避免当前阶段为追求形式统一造成主链路不稳定 |
| **新模块工程验收清单** | 新模块创建后必须同时具备：`MEYER_MODULE_NAME`、规范 `Version.rc`、中文 README、中文 CHANGELOG、测试宿主或 smoke 入口、UTF-8 BOM 源码、运行路径不依赖开发机绝对路径；测试宿主源码变更后必须单独构建对应测试项目，并确认根方案能构建该测试入口 | 把工程一致性前移到模块创建阶段，避免后续靠人工大范围补漏 |
| **2026-07-04 全模块复查结果** | 已按当前文档规则复查活跃自研模块和今日新增模块：Database 仍为纯 C++，Qt 调用方仍经 DatabaseQtAdapter；正式 UI 未发现绕过 RuntimeDataCenter/Service 直连业务库；运行资源路径未发现实际使用 `QDir::currentPath()`；UI 可见文案未发现 `tr("中文")`；HomeUI 的 Create 入口进入 OrderScanWorkspaceShell 后挂载 OrderCreateUI，外部拉起经 ExternalLaunchAdapter 归一化 JSON 后直接显示工作台/建单页 | 作为当前阶段防偏移基线；后续新增模块或修改链路时必须继续按这些扫描项和 smoke 项回归 |
| **2026-07-05 UIComponents/OrderCreateUI 样式收口** | UIComponents 升级到 v0.4.0，新增字段标签、日期框、多行文本框和基础表格工厂；OrderCreateUI 升级到 v0.2.1，建单表单通用控件和已选牙位表格基础样式改为动态加载 UIComponents，牙位/扫描类型等业务控件留在自身模块；OrderCreateUI 不再链接 `MeyerScan_UIComponents.lib`，只保留头文件依赖和 DLL 复制，运行时通过 `QLibrary` 加载，缺失或版本低于新增表格接口要求时走本地降级样式。2026-07-07 进一步升级到 v0.3.0，补充建单扫描流程输入和 `scanProcess` JSON 输出；2026-07-08 升级到 v0.4.0，治疗方案区改为上下颌图片 + mask 命中 + 叠加图 + 桥连接点，并落地模块资源复制规则。 | 共享 UI 负责通用视觉，业务 UI 负责页面结构和状态；可选动态加载避免共享 UI 缺失或旧 DLL 残留时启动期硬失败/运行时崩溃 |
| **2026-07-05 本轮验证结果** | `MeyerScan_UIComponents.sln`、`MeyerScan_OrderCreateUI.sln` 和根 `MeyerScan_AllModules.sln` Release x64 构建通过；模块输出和根输出目录的 `UIComponentsTest.exe`、`OrderCreateUITest.exe --smoke` 返回 0；根输出目录 `MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke-external-order ... --external-order-type cmd_demo` 返回 0 | 根方案仍有外部登录头文件既有 C4819/C4091 警告，非本轮共享 UI/建单界面变更引入；`git diff --check -- MyUIComponents MyOrderCreateUI README.md` 仅有 LF/CRLF 提示，无空白错误 |
| **2026-07-12 建单治疗方案规则** | 修复类型固定为 `crown/missing/inlay/veneer/implant`，图片序号固定为 `1/3/4/5/7`；牙位 mask 顺序固定为上颌 `11..18,21..28`、下颌 `31..38,41..48`；任意同颌相邻已选牙显示空心桥点，点击后记录实心桥点；普通/高亮图分别使用 b/h，2K 及以上使用 2x 源图 | bridge 是相邻牙连接状态，不是修复类型；纯映射规则由模块内部头同时供生产和测试使用，但不加入 C ABI；跨 DLL 继续只暴露 `IOrderCreateUI` 和稳定工厂函数 |
| **2026-07-12 建单多分辨率验证** | 左栏最小 380px，类型按钮最小 82px；中间分栏可伸缩，Scan Plan 内容最大 980px 并居中；1366x768、1920x1080、2560x1440 使用同一套 Layout/QSS | 不按分辨率缩放坐标，不按语言写尺寸分支；VS2015 单模块/根方案、OrderCreateUI smoke 和 UIResources smoke 均通过 |
| **2026-07-04 测试验证结果** | 根 `MeyerScan_AllModules.sln` Release x64 构建通过；`LoggerTest`、`DatabaseTest`、`ConfigCenterTest`、`PermissionTest`、`VersionManagerTest`、`DatabaseQtAdapterTest`、`CaseOrderServiceTest`、`RuntimeDataCenterTest`、`UIComponentsTest`、`Calibration3DUITest`、`CalibrationColorUITest`、`OrderScanWorkspaceShellTest`、`OrderCreateUITest --smoke`、`ExternalLaunchAdapterTest`、`HomeUITest --smoke`、`CaseUITest --smoke`、`SettingsUITest --smoke`、`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke-external-order ... --external-order-type cmd_demo` 均返回 0；`git diff --check` 仅有 LF/CRLF 提示，无空白错误。该轮当时未执行 CMake configure/build，已在 2026-07-06 使用 CMake 3.31.6 和 VS2015 x64 生成器完成根聚合 Release 补验证 | 明确“已验证”和“历史受环境限制未验证”的边界，避免把 CMakeLists 存在误写为 CMake 已构建通过 |

---

> **第十章阶段版本**：v1.26（2026-07-12，后续现行补充继续见第十四章及文档末尾版本；本行不再作为全文当前版本）

---

## 十一、架构优化方案说明

> **说明**：以下内容为架构优化过程中讨论过的方案记录，部分决策后续有调整，详见 §10.3 近期设计决策记录。

### 11.1 用户反馈与优化需求

基于用户提出的两个核心需求：
1. **数据库操作独立化**：将数据库连接、SQL 执行抽取为独立模块，供多个业务模块复用
2. **模块细粒度拆分**：将现有模块拆分得更细，每个模块职责单一，便于维护

### 11.2 优化方案概述

| 优化项 | 原方案 | 优化方案 | 变化 |
|--------|--------|----------|------|
| **数据库基础设施** | 嵌入在原 CaseManager 中 | 独立 Database.dll | +1 DLL |
| **原 CaseManager 拆分** | 1 个大模块 | 拆分为 7 个模块（含流程规则服务） | +6 DLL |
| **Permission 拆分** | 1 个模块含 UI | 核心逻辑 + UI 分离 | +1 DLL |
| **通用服务新增** | 无 | DataExport.dll + Statistics.dll | +2 DLL |
| **DLL 总数** | 16 个 | 21 个左右（按 Statistics/客户定制模块是否独立微调） | +5 左右 |

> **实际决策调整**（2026-06-23）：在继续梳理口扫业务后，确认患者和订单在软件内部、数据库内部强绑定，`CaseService.dll` 与 `OrderService.dll` 不再作为两个独立 DLL 推进，改为统一 `CaseOrderService.dll`。扫描方案、加载订单规则、导入导出、统计仍保持独立服务。CaseEntity 保持 `.lib`/契约层暂定；IPC 仍归入 Core.lib；Permission 核心与 PermissionConfigUI 分离。原则是：按真实变化原因拆分，不能为了“细”强拆强耦合数据。

### 11.3 新增模块

| 模块名 | 职责 | 被调用方 |
|--------|------|----------|
| **Database.dll** | 数据库连接、SQL 执行、事务管理、迁移、通用查询结果 JSON 化 | CaseOrderService、ScanSchemaService、Permission 等 |
| **CaseOrderService.dll** | 患者/订单组合数据 CRUD、医生/诊所/技工所等参考数据、字段版本和 schema 映射 | CaseUI、OrderCreateUI、ExternalLaunchAdapter、HisWorklistAdapter |
| **ScanSchemaService.dll** | 扫描方案 CRUD（从原 CaseManager 拆分） | ScanReconstructStudio |
| **OrderWorkflowService.dll** | 新建/加载/继续扫描/进入处理/进入发送的规则决策 | HomeUI、CaseUI、OrderCreateUI、ScanReconstructStudio |
| **OrderScanWorkspaceShell.dll** | 建单、扫描、处理、发送的统一工作台壳和页面容器 | MainExe、OrderCreateUI、ScanWorkflowUI、DataProcessUI、SendUI、ScanReconstructStudio |
| **SendUI.dll** | 发送步骤 UI、案例信息展示、发送动作回调 | MainExe、OrderScanWorkspaceShell、DataExport/Network 后续服务 |
| **DataExport.dll** | 数据导入导出（通用服务） | CaseOrderService、Statistics |
| **Statistics.dll** | 统计分析（从原 CaseManager 拆分） | CaseUI、EngineeringSettings |
| **PermissionConfigUI.dll** | 权限配置界面（从 Permission 拆分） | EngineeringSettings |
| **Calibration3DUI.dll** | 三维校准 UI、流程、采集编排和计算入口 | DeviceCmd、DeviceTransport、算法 DLL |
| **CalibrationColorUI.dll** | 颜色校准 UI、流程、采集编排和计算入口 | DeviceCmd、DeviceTransport、算法 DLL |

### 11.4 Database.dll 接口设计要点

```cpp
// 核心接口（Result<T> 风格，v1.1.0）
IDatabase* db = GetDatabase();

// 初始化（过渡方案：自己读 JSON，后续从 ConfigCenter 获取配置）
VoidResult result = db->Init("config/db_config.json");
if (result.IsError()) {
    return;
}

// 建立连接
result = db->Connect();
if (result.IsError()) {
    return;
}

// 执行查询
Result<DbResult> queryResult = db->ExecuteQuery("SELECT * FROM cases WHERE patient_name LIKE '%张%'");
if (queryResult.IsSuccess()) {
    int64_t rows = queryResult.data.affectedRows;
}

// 执行更新
Result<DbResult> updateResult = db->ExecuteUpdate("UPDATE cases SET status = 1 WHERE case_id = 'C001'");
if (updateResult.IsSuccess()) {
    printf("影响行数: %lld", updateResult.data.affectedRows);
}

// 事务操作
if (db->BeginTransaction().IsSuccess()) {
    db->ExecuteUpdate("INSERT INTO ...");
    db->ExecuteUpdate("UPDATE ...");
    db->Commit();  // 或 db->Rollback();
}

// 关闭
db->Shutdown();
```

详见接口头文件：`接口规范/Database.h`

### 11.5 实施建议

采用 **平衡策略**，优先拆分：
1. 第一阶段：抽取 Database.dll
2. 第二阶段：开发 CaseOrderService + ScanSchemaService + OrderWorkflowService，替代单一 CaseManager
3. 第三阶段：开发 DataExport + Statistics，并拆分 PermissionConfigUI

---

## 十二、首页、建单与加载订单规则分离方案

> **说明**：首页独立为 HomeUI.dll，建单表单独立为 OrderCreateUI.dll，订单进入扫描/处理/发送前的规则独立为 OrderWorkflowService.dll，扫描重建独立为 ScanReconstructStudio.exe。

### 12.1 软件启动流程

```
软件启动 → Logger.dll → ConfigCenter.dll / Permission.dll / UIComponents.dll → DatabaseQtAdapter.dll → Database.dll（连接验证）→ RuntimeDataCenter.dll（只读快照）→ Login.dll（登录）→ HomeUI.dll（首页）
```

当前工程落地阶段（2026-07-03）采用已收口的最小可运行链路：

```text
MeyerScan.exe
  → 初始化 Logger.dll
  → 通过 DatabaseQtAdapter.dll 调用纯 C++ Database.dll 读取运行目录 config/db_config.json 做连接健康检查
  → 调用既有 MeyerLoginWidget.dll 显示登录界面
  → 登录成功后加载 HomeUI.dll
  → HomeUI 上报“浏览”入口 ID
  → MainExe 替换单内容区为 CaseUI.dll 全屏页面
  → CaseUI 上报“返回首页”操作 ID
  → MainExe 切回 HomeUI.dll
  → 首页/浏览点击设置入口时加载 SettingsUI.dll
  → SettingsUI 根据打开来源刷新内容，非扫描重建来源允许进入 3D/颜色校准
```

说明：

1. ConfigCenter、Permission、Case/Order/ScanSchema/Workflow 服务尚未完全接入前，MainExe 只允许做基础设施健康检查、权限下发和 UI 编排。
2. `--smoke` 用于验证登录链路，`--smoke-main` 用于跳过登录验证 HomeUI/CaseUI 装载；这两个参数只用于开发/集成验证，不作为正式用户入口。
3. Login 模块后续建议包一层 `LoginAdapter`，使 MainExe 不直接依赖外部登录头文件的编码、字段和枚举细节。

### 12.2 首页模块设计

| 方案 | 推荐度 | 说明 |
|------|--------|------|
| **独立 HomeUI.dll** | ⭐⭐⭐ 推荐 | 定制化只需替换DLL，OEM友好 |
| 放入主程序EXE | ⭐ | 定制需重编译，成本高 |
| 放入UIComponents.dll | ⭐⭐ | UIComponents职责变重 |

HomeUI.dll 的边界：

| 项 | 结论 |
|----|------|
| 做什么 | 显示创建、浏览、练习、设置/校准/云端等入口；根据 Permission 的显示规则隐藏/禁用入口；承载 OEM 首页布局、Logo、语言、主题差异 |
| 不做什么 | 不判断订单能否加载、不保存订单、不直接启动扫描参数、不读取数据库、不实现权限核心逻辑 |
| 是否必要 | 必要。首页是 OEM/客户定制最频繁的界面，独立 DLL 可以降低重编译和分支维护成本 |

### 12.3 建单功能与扫描重建分离

**目标**：建单功能独立为 DLL，加载订单规则独立为服务，扫描重建独立为 EXE，界面视觉融合但规则不混入 UI。

| 模式 | 流程 | 建单页面 |
|------|------|----------|
| 新建订单 | HomeUI → OrderWorkflowService.EvaluateNewOrder → OrderCreateUI → Case/Order/ScanSchema Service → ScanReconstructStudio.exe | ✅ 显示，可编辑 |
| 加载订单 | CaseUI/最近订单 → OrderWorkflowService.EvaluateOpenOrder → 按决策进入 OrderCreateUI/Scan/Processing/Send | 按规则决定 |
| 练习扫描 | HomeUI → OrderWorkflowService.EvaluatePracticeScan → OrderScanWorkspaceShell 练习模式（Scan/Process）→ 阶段性挂载 ScanWorkflowUI/DataProcessUI；后续切换为 ScanReconstructStudio.exe（默认参数） | ❌ 跳过 |

### 12.3.1 第三方拉起建单标准链路

第三方软件拉起 MeyerScan 时，允许用命令行或系统注册协议把“建单上下文文件路径 + 第三方类型”传给 `MeyerScan.exe`。当前开发验证命令为：

```powershell
MeyerScan.exe --external-order F:\path\external_order.json --external-order-type cmd_demo
```

处理链路固定为：

```text
第三方软件 / cmd 模拟
  → MeyerScan.exe 单实例检查
  → ExternalLaunchAdapter.NormalizeOrderFile(jsonPath, thirdPartyType)
  → 输出标准建单上下文 JSON
  → MainExe 后台准备 HomeUI 的 Create 入口并复核 order.create visible/enabled
  → MainExe 显示 OrderScanWorkspaceShell
  → MainExe 将 OrderCreateUI 挂入 WorkspaceStepOrderCreate
  → OrderCreateUI.SetOrderContextJson(contextJson) 填充患者/订单/扫描方案
```

用户视觉要求：第三方自动拉起时，客户只能看到 `OrderScanWorkspaceShell / OrderCreateUI` 界面出现，不能看到首页闪现，也不能看到自动点击“Create”的过程。实现上可以后台初始化 HomeUI 和入口规则，但不得把首页挂到内容区显示。

标准上下文中的 `source` 必须包含第三方来源字段：

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
  "patient": {},
  "order": {},
  "scanPlan": { "items": [] }
}
```

规则：

1. `thirdPartyType` 是多第三方分流字段，不允许省略；命令行未传时可从 JSON 的 `source.thirdPartyType` 读取，仍为空时只能作为 `generic` 测试兜底。
2. 不同第三方字段差异优先在 `MyExternalLaunchAdapter` 内按 `thirdPartyType` 增加映射规则，不能把第三方私有字段判断散落到 MainExe 或 OrderCreateUI。
3. ExternalLaunchAdapter 不显示 UI、不写数据库、不启动扫描、不直接调用 OrderCreateUI。
4. MainExe 负责单实例转发：第二次启动带 `--external-order` 时，通过本机 IPC 把 JSON 路径和第三方类型转给已登录完成的主实例；数据库检查或登录阶段收到外部订单先忽略，不强制弹出半初始化界面。
5. OrderCreateUI 只消费标准上下文，不认识每个第三方私有字段；后续保存由 CaseOrderService / ScanSchemaService / OrderWorkflowService 决定。

### 12.4 加载订单规则服务

OrderWorkflowService.dll 的必要性：加载订单规则会同时受订单状态、扫描方案、数据文件完整性、权限、客户定制、机型、软件版本影响。如果放在 HomeUI、CaseUI 或 OrderCreateUI 中，后续会形成重复判断和隐藏业务逻辑。

| 输入 | 来源 |
|------|------|
| 订单状态、患者订单关系 | CaseOrderService |
| 扫描方案完整性 | ScanSchemaService |
| 数据目录/扫描数据完整性 | ScanDataIO |
| 功能权限与客户限制 | Permission |
| 默认流程、客户配置 | ConfigCenter |

| 输出 | 用途 |
|------|------|
| `OrderWorkflowAction` | 决定拒绝、显示建单、开始扫描、继续扫描、进入处理、进入发送 |
| `orderCreateEditable` | 决定 OrderCreateUI 是编辑模式还是只读恢复模式 |
| `scanLaunchMode` | 决定扫描进程启动参数：new/restore/practice/process/send |
| `denyReason` | 给 UI 展示明确拒绝原因，也写入日志 |

典型规则：

1. 订单已删除或病例已删除：拒绝进入。
2. 扫描方案缺失：进入 OrderCreateUI，允许补全扫描方案。
3. 订单已完成且数据完整：可进入数据处理或发送，不默认重新扫描。
4. 订单未完成但存在扫描缓存：进入继续扫描。
5. 练习扫描不创建病例/订单，不允许发送、云上传、写入正式病例库。
6. 权限不允许某功能时，Workflow 返回 Deny；UI 只能展示原因，不能自行放行。

### 12.5 技术可行性

**✅ 完全可行**
- 技术栈一致：Qt 5.6.3 + C++
- 界面融合：Qt窗口嵌入技术（`QWidget::createWindowContainer()`）
- 进程通信：Qt Named Pipe IPC

### 12.6 新增模块

| 模块 | 类型 | 职责 |
|------|------|------|
| **HomeUI.dll** | DLL | 首页界面（四大入口） |
| **OrderCreateUI.dll** | DLL | 建单界面（基本信息+扫描方案） |
| **OrderWorkflowService.dll** | DLL | 新建/加载/继续扫描/发送前的流程规则判断 |
| **ScanWorkflowUI.dll** | DLL | 扫描阶段 UI，承载扫描对象、扫描工具、扫描控制和 QVTK 显示区 |
| **DataProcessUI.dll** | DLL | 数据处理阶段 UI，承载编辑/颈缘/倒凹/测量等处理入口和 QVTK 显示区 |
| **SendUI.dll** | DLL | 发送步骤 UI，承载案例信息确认、数据格式占位和发送动作入口 |
| **ScanReconstructStudio.exe** | EXE | 扫描重建独立进程壳，动态加载扫描 UI 和数据处理 UI |

### 12.7 最终模块数量

| 类别 | 数量 |
|------|------|
| DLL 总计 | **约 26-28 个**（按是否独立发布 Statistics、PermissionConfigUI、扫描处理工具等微调） |
| EXE 总计 | **2 个**（MeyerScan.exe + ScanReconstructStudio.exe） |
>
> **维护说明**：本文档随模块开发进度持续更新，每次新增模块接口后更新"待生成"状态。

## 13. 2026-07-09 工程与 UI 约束补充

### 13.1 Qt 模块日志接口

依赖 Qt 的模块统一使用 `Common/include/MeyerQtModuleUtils.h` 中的日志辅助函数：

```cpp
MeyerQtModule::WriteQtLog(m_logger, LogLevel::Info, "OperationName", QString("message"));
```

模块名由 `MEYER_MODULE_NAME` 自动注入，调用方不再重复传模块名。这样可以减少模块名拼写漂移，也便于后续日志分析按 `[Mod:]` 归类。

约束：

1. 每个模块只缓存一份 `ILogger* m_logger`。
2. DLL 加载成功/失败、工厂函数解析成功/失败、页面切换、按钮点击、资源释放、Shutdown 都必须写日志。
3. Logger.dll 本体仍保持非 Qt；Qt 便利层只放公共头文件，不反向污染日志模块。

### 13.2 QSS-only 样式规则

所有界面控件样式都必须放入模块 `Resources/qss/*.qss`。源码中禁止直接调用 `setStyleSheet()`，唯一例外是公共函数 `MeyerQtModule::ApplyModuleQss()`。

QSS 中引用图片时使用占位符：

```qss
background-image: url("@MEYER_MODULE_RESOURCE_DIR@/icon/home/background.png");
```

公共函数会把占位符替换为模块当前资源根：正式运行时是 `:/MeyerScan/Modules/<ProjectName>`，源码调试时是模块 `Resources`，旧安装兼容时才是 EXE 同级散文件目录。三种路径都不依赖 current directory。

### 13.3 Resources 目录规划

模块源码目录保留本模块资源：

```text
MyModule/
  Resources/
    icon/
    qss/
```

正式构建/打包输出统一为：

```text
MeyerScan.exe
MeyerScan_UIResources.dll
Resources/
  license.lic
```

`MyUIResources/tools/GenerateResourceManifest.ps1` 自动扫描模块资源并生成 qrc，Qt `rcc -binary` 生成 RCC 数据，VS2015/CMake 再把该数据以 Win32 `RCDATA` 编入 `MeyerScan_UIResources.dll`。MainExe、根解决方案、CMake 和后续安装包脚本只复制这个资源 DLL；新增资源时必须验证清单脚本已收集、资源测试能打开、MainExe versionList 已记录资源 DLL。

资源加载优先级固定为：资源 DLL -> 源码树开发降级 -> 旧安装目录散文件兼容。`QLibrary::PreventUnloadHint` 保证已注册资源的 DLL 不在 UI 页面切换时卸载；资源模块初始化接口必须幂等。资源 DLL 不是加密容器，安装完整性仍由文件版本、代码版本、哈希/签名和修复流程保证。

### 13.4 全屏无边框界面

首页、浏览、创建、练习都作为 MainExe 内容区的全屏页面显示，不使用 Qt 原生标题栏或系统右上角按钮。需要的窗口动作由模块内部自绘按钮完成：

1. 首页：自绘校准、云端、帮助、最小化、关闭。
2. 浏览：自绘设置、返回首页、最小化、关闭等工具入口。
3. 创建/练习：当前只保留最小化和关闭。

MainExe 不绘制跨页面通用可见标题栏。HomeUI、CaseUI、OrderScanWorkspaceShell 分别拥有页面语义顶部区域；其中 OrderScanWorkspaceShell 是创建/练习场景唯一的步骤导航所有者。页面按钮只上报稳定动作 ID，顶层窗口动作由 MainExe 执行。

页面切换仍遵守“单内容区替换 + 离开页释放资源”的规则，不能退回首页/浏览并列 `QStackedWidget` 长期缓存方案。

### 13.5 版本字段与版本清单

每个自研 DLL/EXE 的 `Version.rc` 必须包含以下详细信息字段：

`CompanyName`、`LegalCopyright`、`FileDescription`、`FileVersion`、`InternalName`、`OriginalFilename`、`ProductName`、`ProductVersion`。

运行时版本清单只记录自研拆分模块，不记录第三方库。清单输出同时记录：

1. `fileVersion`：Windows 文件详细信息版本。
2. `codeVersion`：`GetMeyerModuleVersion()` 返回的代码版本。
3. `versionMatch`：两者是否同步。

新增模块时必须同步：

1. `src/Version.rc`。
2. `GetMeyerModuleVersion()`。
3. `MyMainExe/config/version_modules.json`。
4. CMake / VS2015 / 安装包复制规则。

### 13.6 MyScanReconstructStudio 双形态边界

`MyScanReconstructStudio` 同时产出：

1. `MeyerScan_ScanReconstructStudio.dll`：供 MainExe 后续嵌入或大客户定制。
2. `ScanReconstructStudio.exe`：供独立练习进程、SDK/API 或第三方独立打开。

当前创建主链路仍是：

```text
MainExe
  -> OrderScanWorkspaceShell.dll
      -> OrderCreateUI.dll
      -> ScanWorkflowUI.dll
      -> DataProcessUI.dll
      -> SendUI.dll
```

后续如果把练习或创建中的 Scan/Process 切换为 `MeyerScan_ScanReconstructStudio.dll`，必须先确认 UI 层级，避免 `OrderScanWorkspaceShell` 和 `ScanReconstructStudioWindow` 两个壳同时显示相似顶部步骤条。

稳定性要求：

1. 扫描/处理切换前必须释放离开页面的 QVTK/VTK/OpenGL 资源。
2. 持有重资源的 DLL 不做热卸载，退出时由进程统一回收。
3. 嵌入窗口使用 Qt 生命周期感知指针，避免父对象释放后模块二次 delete。

## 14. 2026-07-10 评审复核后的现行架构决策

### 14.1 建议采纳边界

`glm52/01-04` 中关于服务层补齐、DPI/Layout、路径可移植性、版本一致性和高风险测试增强的建议采纳。以下建议不直接照搬：

1. 不把 `Core.lib` 和固定字段 `CaseEntity.lib` 作为当前前置底座。
2. 不要求 UI/VTK DLL 支持运行期热替换；自动更新在主程序退出后覆盖。
3. 不删除 MySQL 扩展位置；当前 SQLite 为默认和已验证链路，MySQL SDK 能力明确标记为未完成。
4. 不按固定测试数量考核；按业务规则复杂度、ABI 风险和资源生命周期补测试。
5. 不让 MainExe 绘制所有页面共用的可见标题栏，也不让 OrderCreateUI 绘制第二套工作台步骤条。

完整逐项结论见 `glm52/05-2026-07-10-建议复核与采纳结论.md`。

### 14.2 当前依赖和数据合同

```text
MyCaseUI / MySettingsUI / MyOrderCreateUI
  -> RuntimeDataCenter（只读快照）或 CaseOrderService/Workflow（业务操作）
  -> MyDatabaseQtAdapter
  -> MyDatabase

第三方/HIS 输入
  -> ExternalLaunchAdapter / HisWorklistAdapter
  -> 标准版本化建单 JSON
  -> OrderCreateUI + CaseOrderService + ScanSchemaService
```

患者/订单数据合同使用 UTF-8 JSON、`schemaVersion`、稳定字段 key 和 `extensions`。服务内部可以使用 DTO，但公共 DLL 边界不直接暴露高频变化固定结构体或 Qt 对象所有权。

### 14.3 UI 所有权

```text
MainExe：无边框全屏顶层窗口 + 单内容区 + 窗口动作执行
HomeUI：首页品牌、入口和首页顶部动作
CaseUI：浏览页品牌、内容工具区和浏览页顶部动作
OrderScanWorkspaceShell：品牌、返回、唯一步骤导航、最小化/关闭、步骤容器
OrderCreateUI / ScanWorkflowUI / DataProcessUI / SendUI：各步骤内容
```

ScanReconstructStudio DLL 嵌入 WorkspaceShell 时必须关闭内部重复导航；独立 EXE 形态可以绘制自身窗口顶部区域。DLL 形态使用进程内 C ABI/UTF-8 JSON，只有独立 EXE 形态需要 IPC。

### 14.4 当前优先实现顺序

1. 真实建单保存、患者/订单查询和打开订单链路。
2. ScanSchemaService 与 OrderWorkflowService 的最小真实实现，逐步移出 UI 中增长的流程规则。
3. UIComponents 的 ScreenUtil/DpiUtil/LayoutRules 和自动化静态规范检查。
4. ScanReconstructStudio 独立进程最小 IPC，再接设备、算法和处理工具。
5. 发送服务、更新与安装打包能力。

### 14.5 PowerShell 与构建脚本约束

Windows PowerShell 5.1、VS2015 构建事件和命令行脚本统一遵循 `PowerShell开发与自动化脚本规范.md`。仓库脚本的可执行摘要位于 `F:\MeyerScan\tools\README.md`。核心要求包括：`.ps1` 使用带 BOM UTF-8、禁止 PowerShell 7 专属语法、复杂引号正则拆分、原生工具检查 `$LASTEXITCODE`、正确解释 robocopy/rg 特殊退出码、路径使用 `-LiteralPath`、递归删除前验证目标边界、脚本重复执行保持幂等。VS2015 与 CMake 当前共享模块 `bin\Release` 产物目录，必须串行构建；锁冲突清理后使用 `/nodeReuse:false` 重跑。

### 14.6 资源与版本清单验收合同

1. `MeyerScan_UIResources.dll` 是 UI PNG/QSS/qm 的正式发布载体，当前通过 qrc、`rcc -binary` 和 Windows `RCDATA` 注册 608 个资源；业务模块继续拥有资源源码，资源 DLL 不拥有业务语义。
2. 资源注册必须早于第一个 UI 页面创建，MainExe 使用 `QLibrary::PreventUnloadHint` 保证运行期不卸载资源载体；模块仍通过标准 Qt 资源 API 读取 `:/MeyerScan/Modules/<ProjectName>/...`。
3. MainExe 的正式 `version_modules.json` 当前声明 24 个自研 EXE/DLL。任何测试都不得写入正式配置目录，VersionManager 测试必须使用隔离应用目录、隔离模块副本和隔离日志目录。
4. 每次发布验收至少检查：清单项数、`exists`、`fileVersion`、`codeVersion`、`versionMatch`、`codeVersionError`。2026-07-12 基线为 24 项、0 缺失、0 不一致、0 代码版本读取错误。
5. UI 自动验收固定覆盖 1366x768、1920x1080 与 2560x1440；检查内容包括品牌和顶部动作不重叠、入口/卡片响应式换列、长翻译可容纳、建单三栏可操作、hover 状态和滚动区域完整。

### 14.7 UI 动态状态与资源批次约束

1. QSS 负责控件矩形背景、边框、字体和状态；图片资源负责业务图形。需要“图标局部圆底”时，可以在业务控件内部组合资源，但颜色必须从资源或主题数据读取，禁止在 C++ 再硬编码一套颜色。
2. OrderCreateUI 前四种修复类型的 h 图只有白色图形，运行时从同类型 b 图非透明像素提取主色并只在图标画布内合成圆底；种植体继续由 QSS 提供整行浅绿/深绿状态。
3. 动态文本不得改变固定格式主内容的几何尺寸。扫描流程预览使用固定单行高度和完整 tooltip，种植体或分段扫描杆增加步骤时不得挤压牙弓；Scan Plan 整体最大高度为 1060px，高分辨率额外高度留在无边框宿主背景中，不拉大上下颌间距。
4. UI 测试必须加载与业务 DLL 同批次的 `MeyerScan_UIResources.dll`。VS2015/CMake 项目应声明构建依赖；只更新业务 DLL 而沿用旧资源 DLL 的 smoke 或截图结果无效。
5. 当前 `minMain` 验证口径为根/单模块 `MeyerScan.exe --smoke-main`，不再维护独立测试项目。主 EXE 源码未变但依赖 DLL 变化时，发布验收仍应强制 Rebuild 主 EXE 并核对时间戳和 versionList。
6. 截图测试必须验证输出 PNG 的实际像素尺寸，不只相信命令行参数。Qt 顶层测试窗口使用 `WA_DontShowOnScreen + setFixedSize` 离屏渲染，避免被当前桌面可用区域自动压缩；目标视口尺寸只通过测试属性影响 1x/2x 选图，不进入生产路径。

### 14.8 模块可用状态、上下文事务与页面生命周期

动态模块的“可用”必须由完整状态链决定：

```text
DLL loaded
  -> factory resolved
  -> interface acquired
  -> Init succeeded
  -> context accepted
  -> QWidget created
  -> attached by host
  -> explicitly activated
```

1. 每个返回 bool 的接口都是合同，不是提示。调用方不得忽略 `Init()`、`Set*ContextJson()`、页面确保函数或服务结果；失败必须写结构化日志并阻止后续依赖动作。
2. `Set*ContextJson()` 使用临时解析结果完成 schema/类型检查，成功后一次替换缓存；失败不得清空或部分修改上一份有效状态。跨 DLL 仍传 UTF-8 `const char*`，接口实现不得保存调用方临时缓冲区指针。
3. 重 UI 固定采用 `CreateWidget -> attach -> Activate`，离开采用 `DeactivateAndRelease -> remove/delete QWidget -> Shutdown`。CreateWidget 内不得隐式激活 QVTK/VTK/OpenGL；宿主只有在目标页创建成功后才能更新步骤导航。
4. 禁用流程步骤不得被回退逻辑绕过。当前步骤失效时选择第一个 enabled 步骤；全部 disabled 时保持无活动步骤、清理旧 actor 并显示不可用状态。
5. 降级矩阵固定：Logger 失败允许无日志运行；UIComponents 失败回退本模块原生 Qt 控件和模块 QSS；SettingsUI 的单个校准 DLL 失败只禁用对应校准能力。失败接口必须清空，不能继续调用半初始化对象。
6. 测试宿主与正式程序使用同一合同。测试不能只断言 DLL 工厂非空，还要验证非法 JSON、Init/CreateWidget 失败保护、显式 Activate、动作回调和 Shutdown/资源释放。
7. 2026-07-12 静态复核结论：近期 UI 模块未直接依赖数据库/配置/权限/网络业务；SendUI 只上报动作；样式入口集中；运行路径来自 appDir/applicationDirPath；当前模块拆分无新增架构偏移。

### 14.9 生产数据与测试数据隔离

1. 正式模块不得用硬编码示例值伪造患者、订单、医生、技工所、牙位或治疗方案。手工创建入口不携带业务上下文时，OrderCreateUI 必须保持空白；下拉框可以显示 `tr("Select doctor")`、`tr("Select lab")` 等非业务占位文本。
2. 练习模式不创建正式病例，可以使用默认扫描上下文；所有可追踪标识必须使用 `PRACTICE_*` 前缀，使日志、数据库和问题现场能够立即区分练习数据。单元/smoke 测试数据只允许在测试宿主或隔离测试数据库内创建。
3. MainExe 只负责识别入口来源并组装标准上下文。手工创建使用空的 `patientId` / `orderId`；第三方拉起保留外部提供值及标准 `source` 对象；字段名统一使用合同定义的 `patientId`、`orderId`，不得退回含义不清的 `id`。
4. OrderCreateUI 的 pending context 采用事务提交：先将 UTF-8 JSON 解析到局部候选对象，成功后再整体替换成员缓存。非法 JSON 返回 `false`，不得覆盖上一份有效内容，也不得触发页面切换。
5. 生产模块默认值检查纳入代码审查和 smoke：至少断言无上下文时患者/订单字段为空、无预选牙位；随后验证有效 JSON、非法 JSON 保留旧状态以及 Shutdown 后重新 Init。
6. 2026-07-13 架构复核仍未发现 UI 直连 Database、SendUI 越界实现发送业务、运行路径使用 `QDir::currentPath()` 或业务源码直接调用 `setStyleSheet()`。最新运行清单为 `versionList_20260713_072234_645.json`，24 项均存在且文件版本与代码版本一致。

> **当前文档版本**：v1.32（2026-07-13，补齐生产/测试数据隔离、标准建单上下文和事务式缓存规则）
