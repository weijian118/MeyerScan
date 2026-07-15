# MeyerScan 重构任务总览

> **文档职责**：说明产品范围、模块清单、关键流程、开发优先级和不可违反的规则。
>
> **唯一维护位置**：`F:\MeyerScan\Documents`。不再读取或同步仓库外旧文档。
>
> **事实来源**：模块是否存在、接口和版本以源码、公共头文件、`Version.rc`、模块 README/CHANGELOG 和 `MyMainExe/config/version_modules.json` 为准。

## 1. 产品范围

MeyerScan 是口腔扫描软件。重构不改变核心产品能力，重点是把原有大工程拆成边界清晰、可独立编译测试、便于人工阅读和替换的模块。

### 1.1 必须覆盖的功能

| 功能域 | 目标 |
|---|---|
| 启动与登录 | 单实例、启动检查、等待页、日志、数据库、离线许可、云端账号登录 |
| 首页与案例 | 创建、浏览、练习、设置入口；患者/订单列表、搜索、打开、删除、导入导出 |
| 建单 | 手工建单、第三方拉起建单、HIS/Worklist 下拉建单共用同一表单和标准上下文 |
| 扫描与处理 | 设备连接、采集、重建、显示、编辑、分析和处理；扫描与处理能力继续细分为独立能力 DLL |
| 校准 | 三维校准和颜色校准分别由独立 UI DLL 承担 |
| 发送与云端 | 本地导出/压缩、技工所发送、口扫云上传下载和同步 |
| 定制与权限 | 按角色、客户、设备、版本、有效期和配置方案控制功能可见、可用和服务端执行 |
| 更新与安装 | `MyUpdate.exe` 独立更新；安装包支持自定义界面、流程、目录、校验和恢复 |
| 运维与诊断 | 统一日志、模块版本清单、配置迁移、错误码、测试宿主和可复现构建 |

### 1.2 本轮重构不追求的内容

- 不为“架构完整”预先创建没有明确调用方和独立变化原因的 DLL。
- 不创建万能 Core、Common、Manager 或 Helper 集合。
- 不因模块数量少而把不相干功能重新塞回 MainExe、UI 或 Database。
- 不把页面可显示、smoke 通过或占位按钮存在写成业务功能已完成。
- 不依据“医疗”字样增加额外流程；只按产品和代码实现需求设计。

## 2. 总体架构

最终形态为：**主 EXE + 插件 DLL + 少量稳定静态库/公共头 + 必要独立进程**。

- `MeyerScan.exe`：启动、单实例、登录、页面容器、插件生命周期、轻量流程编排。
- 插件 DLL：UI、服务、适配器和基础设施模块。
- 静态库/公共头：只承载低变化、无运行时替换价值的稳定小契约；当前不预建大 Core.lib。
- 独立进程：扫描重建独立运行形态、自动更新和安装器。
- 同一套扫描重建实现同时产出 DLL 和 EXE：DLL 嵌入创建/练习工作台，EXE 用于独立练习、客户定制和 SDK/API 场景。

核心判断不是“模块越多越好”，而是模块是否同时满足：职责单一、依赖方向稳定、可独立验证、修改原因相对单一、代码量可人工理解。

## 3. 模块总清单

### 3.1 已落地或已接入模块

| 中文名 | 项目名 | 产物 | 当前职责 | 明确边界 |
|---|---|---|---|---|
| 主程序壳 | `MyMainExe` | `MeyerScan.exe` | 单实例、启动检查、登录、页面挂载、动态加载、导航、版本清单 | 不写业务 SQL、订单规则、设备协议或算法 |
| 登录接入 | `MyMainExe/MyLogin` | `MeyerLoginTest.exe`；调用既有 `MeyerLoginWidget.dll` | 验证登录 DLL，MainExe 接收登录信号和结果 | 不重写既有登录业务；后续由 LoginAdapter 隔离外部结构 |
| 日志 | `MyLogger` | `MeyerScan_Logger.dll` | 每日文件、超限分卷、结构化同步写入、逐条关闭句柄、多线程/多进程保护 | 纯 C++；不承载业务；Qt 便捷重载只在头文件适配层 |
| 数据库 | `MyDatabase` | `MeyerScan_Database.dll` | 纯 C++ SQLite/MySQL 基础连接、SQL、事务、备份和通用 JSON 结果 | 不理解患者/订单语义，不做业务 CRUD，不依赖 Qt |
| 数据库 Qt 适配 | `MyDatabaseQtAdapter` | `MeyerScan_DatabaseQtAdapter.dll` | QString/UTF-8、QJson/通用 JSON 和调用方缓冲区转换 | 不连接数据库、不写业务规则；Database 不反向依赖它 |
| 设备传输 | `MyDeviceTransport` | `MeyerScan_DeviceTransport.dll` | CyAPI USB 连接、原始命令/流收发、异步队列、图像包同步组帧和 IMU 解码 | 纯 C++、x64；不解释设备命令业务、不做 UI/重建算法；串口/网口尚未实现 |
| 配置中心 | `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | 读取 `runtime_config.json`，提供产品/客户配置 | 不做权限判断和业务流程 |
| 权限核心 | `MyPermission` | `MeyerScan_Permission.dll` | 读取 `permission_rules.json`，返回 `visible/enabled` | 不做 UI；高价值动作仍需 Service/Workflow/IPC 复核 |
| 运行时数据中心 | `MyRuntimeDataCenter` | `MeyerScan_RuntimeDataCenter.dll` | 缓存诊所、技工所、医生、设置、账号、设备、患者、订单和云端诊所 JSON 快照 | 只读快照，不做 CRUD、联网和 SQL 透传 |
| 病例订单服务 | `MyCaseOrderService` | `MeyerScan_CaseOrderService.dll` | 患者/订单组合 CRUD 和诊所、医生、技工所等参考数据；版本化 JSON 合同 | 不做 UI、扫描方案、设备和流程决策 |
| 扫描方案服务 | `MyScanSchemaService` | `MeyerScan_ScanSchemaService.dll` | 根据治疗牙位、修复类型、扫描杆选项和咬合类型生成稳定 `scanProcess.steps` | 不做 UI、翻译、数据库保存、设备或算法 |
| 共享 UI 组件 | `MyUIComponents` | `MeyerScan_UIComponents.dll` | 通用按钮、标签、输入、下拉、日期、表格、等待页等工厂和基础样式 | 不承载业务行为、权限和页面跳转；特殊控件留在业务模块 |
| 统一 UI 资源 | `MyUIResources` | `MeyerScan_UIResources.dll` | 聚合各模块 PNG/QSS/QM 等资源并注册 `:/MeyerScan/Modules/...` | 不创建控件、不加密资源、不管理业务 |
| 首页 | `MyHomeUI` | `MeyerScan_HomeUI.dll` | Create/Browse/Practice/Settings 入口和动作回调 | 不实现建单、加载订单或权限核心规则 |
| 案例管理 | `MyCaseUI` | `MeyerScan_CaseUI.dll` | 患者/订单列表、搜索入口、打开/删除/导入导出动作、返回首页 | 不直连 Database；CRUD 走 Service，打开走 Workflow |
| 设置 | `MySettingsUI` | `MeyerScan_SettingsUI.dll` | General、Information、Calibration、Cloud、Scan、Process、About 页面 | 不直连 Database/ConfigCenter；保存由宿主/服务编排 |
| 建单 | `MyOrderCreateUI` | `MeyerScan_OrderCreateUI.dll` | 患者订单输入、五种修复类型、FDI 牙位/桥交互、扫描流程输入和标准 JSON | 不保存数据库、不决定加载规则、不绘制工作台步骤条 |
| 创建/练习工作台 | `MyOrderScanWorkspaceShell` | `MeyerScan_OrderScanWorkspaceShell.dll` | 创建模式 Order/Scan/Process/Send，练习模式 Scan/Process；唯一顶部步骤导航 | 只做容器和导航，不实现各步骤业务 |
| 第三方拉起适配 | `MyExternalLaunchAdapter` | `MeyerScan_ExternalLaunchAdapter.dll` | 按第三方类型把输入 JSON 归一化为标准建单上下文 | 不显示 UI、不保存数据、不启动扫描 |
| 三维校准 | `MyCalibration3DUI` | `MeyerScan_Calibration3DUI.dll` | 三维校准 UI、采集编排和计算入口骨架 | 不做颜色校准和病例维护 |
| 颜色校准 | `MyCalibrationColorUI` | `MeyerScan_CalibrationColorUI.dll` | 颜色校准 UI、采集编排和计算入口骨架 | 不做三维校准和病例维护 |
| 扫描阶段 UI | `MyScanWorkflowUI` | `MeyerScan_ScanWorkflowUI.dll` | 扫描流程按钮、QVTK 显示、扫描控制、重资源激活/释放 | 不生成扫描方案、不实现设备和算法 |
| 数据处理 UI | `MyDataProcessUI` | `MeyerScan_DataProcessUI.dll` | 处理流程按钮、QVTK 显示、处理工具入口、重资源激活/释放 | 不实现重算法；无 Scan 页 Start/Pause 控件 |
| 发送 UI | `MySendUI` | `MeyerScan_SendUI.dll` | 展示订单和发送选项，上报 Export/Compress/Email/Upload/Finish 动作 | 不直接导出、压缩、联网或写数据库 |
| 扫描重建壳 | `MyScanReconstructStudio` | `MeyerScan_ScanReconstructStudio.dll` + `ScanReconstructStudio.exe` | 共用一套实现，动态装载 Scan/Process 页面；DLL 嵌入，EXE 独立 | 只做 UI/交互/编排；设备、算法和处理能力继续拆分 |
| 版本管理历史骨架 | `MyVersionManager` | `MeyerScan_VersionManager.dll` | 保留 manifest 驱动的历史实现和测试 | 当前启动版本清单由 MainExe 内置生成，复杂后再恢复独立模块 |

`MyCaseManager` 仅作为旧数据库/schema 参考，不是现行运行模块。既有 `MeyerLoginWidget.dll`、`MeyerScanNetworkHelper.dll`、`deviceAuthAndCrypto.dll` 属于外部或既有模块，先适配接入，不在本仓库重复开发。

### 3.2 规划模块

| 中文名 | 建议项目/产物 | 职责 | 何时创建 |
|---|---|---|---|
| 登录适配 | `MyLoginAdapter` / `MeyerScan_LoginAdapter.dll` | 隔离既有登录头文件、参数和返回结构，输出稳定登录结果 | MainExe 登录字段继续扩展前 |
| 订单流程服务 | `MyOrderWorkflowService` / `MeyerScan_OrderWorkflowService.dll` | 决定新建、打开、继续扫描、进入处理/发送 | CaseUI 正式打开订单前 |
| HIS/Worklist 适配 | `MyHisWorklistAdapter` / `MeyerScan_HisWorklistAdapter.dll` | 查询患者并归一化为标准建单上下文 | 接入首个 HIS 时 |
| 权限配置 UI | `MyPermissionConfigUI` / `MeyerScan_PermissionConfigUI.dll` | 展示授权、导入规则、扫码授权入口 | 权限配置需要面向客户时 |
| 二维码授权入口 | `MyQRCodeAuthEntry` / `MeyerScan_QRCodeAuthEntry.dll` | 读取二维码并把待校验内容交给 Permission | 权限扫码需求落地时 |
| 网络适配 | `MyNetworkHelper` 或既有网络 DLL | 云端登录、订单上传下载、同步、邮件和更新配置下载 | 复用既有模块，按调用边界封装 |
| 数据导入导出 | `MyDataExport` / `MeyerScan_DataExport.dll` | 订单/扫描数据导入、导出、打包和格式转换 | SendUI 动作进入真实实现前 |
| 统计 | `MyStatistics` / `MeyerScan_Statistics.dll` | 只读聚合患者、订单、设备和扫描数据 | 有稳定统计需求时 |
| 自动更新 | `MyUpdate` / `MyUpdate.exe` | 比较本地/云端条件、下载补丁、关闭覆盖并重启 MeyerScan | 更新功能实施阶段 |
| 安装打包 | `MyInstaller` / 安装包 | 自定义安装 UI/流程、依赖收集、目录、校验、修复和卸载 | 交付阶段 |
| 加解密 | `MyCrypto` 或既有 `deviceAuthAndCrypto.dll` | 配置/权限验签、设备授权、敏感字段和离线许可 | 优先复用既有模块 |
| 设备命令 | `MyDeviceCmd` / `MeyerScan_DeviceCmd.dll` | 命令组装、解析、校验和状态查询 | 与 DeviceTransport 分层接入 |
| 扫描数据 IO | `MyScanDataIO` / `MeyerScan_ScanDataIO.dll` | 原始帧、深度、纹理、中间数据和模型文件读写/校验 | 扫描数据正式落盘前 |
| 扫描预处理 | `MyScanDataPreProcess` / `MeyerScan_ScanDataPreProcess.dll` | 解密、镜像、裁剪、颜色校准、AI 消去等流水线 | 接入真实采集数据时 |
| 数据处理能力 | 按功能拆分 Registration/Edit/Measure/Margin/Undercut/Occlusion/Base 等 DLL | 纯数据处理、分析和算法能力 | 至少两个调用方或单项代码/依赖明显独立时逐个创建 |
| 工程设置 | `MyEngineeringSettings` / `MeyerScan_EngineeringSettings.dll` | 设备、固件、扫描/处理参数和诊断工具 | 高级设置需求明确时 |

## 4. 关键流程

### 4.1 启动

1. 单实例检查；重复启动仅激活已显示主窗口，数据库检查或登录阶段可忽略激活。
2. 以 `MeyerScan.exe` 所在目录为运行根目录，最早加载 Logger，创建 `logs`。
3. 显示等待页，加载配置、权限、资源和版本清单，检查数据库，初始化 CaseOrderService/RuntimeDataCenter 及必要文件。
4. 调用登录模块；收到登录成功信号和返回结构后关闭等待页。
5. 创建并显示全屏无边框首页；后续 Case/Settings 页面由 MainExe 注入所需只读 domain 快照。

### 4.2 手工创建订单

`HomeUI Create -> MainExe -> OrderScanWorkspaceShell -> OrderCreateUI -> CaseOrderService + ScanSchemaService -> OrderWorkflowService -> Scan/Process -> Send`

- OrderCreateUI 只收集输入并导出完整上下文；扫描步骤规则由 ScanSchemaService 生成。
- 患者/订单由 CaseOrderService 保存；Confirm 执行保存，Next 必须保存成功后才能进入扫描。
- 案例页列表由 MainExe 合并 CaseOrderService 新表摘要和 RuntimeDataCenter 旧表快照后注入；新记录优先，禁止为了列表显示反向硬写旧表。
- WorkspaceShell 只管理步骤和页面容器。
- 扫描流程 JSON 由 ScanSchemaService 生成，MainExe/Workflow 转发，Scan/Process 只消费稳定 `steps.code` 并各自翻译显示。

### 4.3 第三方或 HIS 建单

外部来源 -> 对应 Adapter -> 标准上下文 `source/patient/order/scanPlan` -> MainExe -> WorkspaceShell/OrderCreateUI。

- 标准上下文必须保留 `thirdPartyType`、来源名称、来源系统和版本。
- 用户视觉上直接看到建单工作台，不显示首页闪现或模拟点击过程。
- 新增第三方优先只改 Adapter 映射和校验，不在 MainExe、OrderCreateUI 或扫描模块写第三方分支。

### 4.4 浏览和打开订单

`HomeUI Browse -> CaseUI -> CaseOrderService 查询 -> OrderWorkflowService 决策 -> MainExe 释放 CaseUI -> 扫描重建工作区`

进入扫描前必须销毁案例管理等非必要页面及缓存，给 VTK、OpenGL、算法和模型数据释放内存/显存空间。

### 4.5 设置

Home、Case、Scan/Process 均可打开 SettingsUI。打开参数必须包含来源页；关闭后由 MainExe 根据来源刷新必要数据。三维/颜色校准只在非扫描重建场景可用。设置持久化由 ConfigCenter/专用服务完成，SettingsUI 不直连配置文件或数据库。

### 4.6 练习

HomeUI Practice 直接进入 WorkspaceShell 的 Scan/Process，不经过建单页。默认流程为自然上颌、交换、自然下颌、自然咬合，订单上下文使用明确的 `PRACTICE_*` 标识，不能污染正式患者数据。

### 4.7 更新

MeyerScan “检查更新”或用户双击 `MyUpdate.exe` -> 生成/读取本地环境和账号设备信息 -> 拉取云端策略 -> 比较版本、硬件、驱动、账号/设备白名单 -> 显示满足项 -> 下载补丁 -> 关闭 MeyerScan -> 校验并覆盖 -> 重启。

`MyUpdate.exe` 与 `MeyerScan.exe` 同级。更新模块独立实现，不把下载、覆盖和回滚逻辑塞进 MainExe。

## 5. 强制开发规则

### 5.1 模块与依赖

- UI 不直接访问 Database；只读链路为 `MainExe -> RuntimeDataCenter -> UI 快照注入`，写链路为 `UI 动作 -> MainExe/Workflow -> Service -> DatabaseQtAdapter -> Database`。
- Database 保持纯 C++；新增非界面模块能不用 Qt 就不用。Qt 模块可在内部合理使用 Qt，不刻意规避。
- MainExe 对自研插件优先使用 `QLibrary + extern "C"` 工厂动态加载；调用 C++ 虚接口前必须校验 `GetMeyerModuleApiVersion()`，加载成功、失败、ABI 拒绝和降级都写日志。
- 公共虚接口新增函数只能追加到末尾；跨不确定 ABI 边界不传 STL/Qt 对象所有权。
- 一个功能只有在职责、依赖、变化原因或资源生命周期明显独立时才拆模块；反之留在现有模块。

### 5.2 数据合同

- 患者和订单紧密相连，由 CaseOrderService 管理，不拆 CaseService/OrderService，也不创建固定字段 Entity.lib。
- 易变化数据使用版本化 UTF-8 JSON：至少包含 `schemaVersion`、稳定 key 和 `extensions`。
- 同进程、同 Qt/编译环境的内部调用可用 QString/QJson；公共 C ABI、跨进程和长期插件合同使用 UTF-8、POD、调用方缓冲区或文件/订单 ID。
- 接收 JSON 时先完整解析和校验候选值，成功后一次性替换旧状态；失败不得留下半更新对象。
- 正式路径不得含演示患者、订单、牙位或开发机数据；测试数据只能进入隔离目录或明确的 Practice/Test 上下文。

### 5.3 UI、多分辨率和多语言

- Home、Case、Create、Practice 均为全屏无边框页面，不使用 Qt 默认标题栏。
- 页面使用 Qt Layout、sizePolicy、最小/最大约束和滚动区；不按 1920x1080 对位置和控件整体乘缩放系数。
- DPI/分辨率系数只辅助图标、边距、间距等离散尺寸；必须验证 1366x768、1920x1080、2560x1440。
- 可见文字统一 `tr("English source text")`；禁止按语言写控件位置/大小 if/else。
- 所有业务样式从 QSS 读取；业务源码禁止直接 `setStyleSheet()`，统一入口除外。
- 通用控件进入 UIComponents，单模块特殊控件留在自身模块；弹窗先放 UIComponents，只有形成独立复杂流程时再拆模块。
- 页面切换由宿主单内容区替换完成；不把首页和浏览长期并列缓存。轻页面可短期复用，重页面离开必须 `DeactivateAndRelease()`。

### 5.4 资源和路径

- 运行路径基于 `QCoreApplication::applicationDirPath()`、Win32 模块路径或 ConfigCenter 安装根目录；禁止 `QDir::currentPath()`。
- 日志在运行根目录 `logs`；登录许可在 `Resources/license.lic`。
- UI 源资源由各模块维护在 `Resources/icon`、`Resources/qss`、`Resources/qm`，发布时编译进 `MeyerScan_UIResources.dll`。
- 资源 DLL 通过 `:/MeyerScan/Modules/<ProjectName>/...` 访问；资源 DLL 化不是加密，完整性仍需版本、哈希/签名和安装修复。

### 5.5 日志

- 每个进程只初始化一套 Logger；每个模块缓存一份借用的 `ILogger* m_logger`，生命周期内连续 `Write()`。
- 所有模块写入同一进程日志文件，按调用时间排列；每日主文件，超限生成 `_NNN`。
- 每条写入完成后刷盘并关闭句柄，允许后台移动或删除已关闭文件。
- 输出字段使用 `[Module:] [Operation:] [Content:]` 等分类标记；空字段不输出占位。
- 客户可见操作、页面切换、DLL 加载、配置/权限决策、资源创建释放、IPC、数据保存和失败路径都必须记录。

### 5.6 配置与权限

- `runtime_config.json` 管产品/客户默认配置；`permission_rules.json` 管授权结果，两者职责不能混用。
- `visible=false` 表示入口不显示；`enabled=false` 表示显示但不可操作。`enabled` 必须真实生效。
- UI 可自行消费宿主注入的权限快照；MainExe 负责导航前复核，Service/Workflow/IPC 负责高价值动作最终复核。
- 固定流程不进入配置：启动等待页和单实例始终启用。
- JSON 内不写注释；字段说明放同目录 Markdown。

### 5.7 版本、构建和测试

- 每个自研 DLL/EXE 同时维护代码版本和 `Version.rc` 文件版本，两者必须一致。
- 代码版本由 `ModuleInfo::Version`/`GetMeyerModuleVersion()` 提供；文件版本来自 Windows 版本资源。
- MainExe 只按 `config/version_modules.json` 记录自研模块，不扫描第三方 DLL；输出到 `logs/versionList`。
- 每个模块和测试宿主同时维护 VS2015 `.sln/.vcxproj` 与 CMakeLists；根方案和根 CMake 均需可构建。
- DLL 必须有独立 Test.exe 或 smoke；根 CTest 提供统一回归入口。VS2015 与 CMake 不得并行写同一输出目录。
- 业务成功不只看进程退出码，还要检查输出、状态、版本、数据隔离和失败路径。

### 5.8 注释、文档和仓库

- 源码、测试代码每个函数必须有中文注释；关键代码和不直观技巧要解释“为什么”和“如何实现”。
- `//` 注释独占物理行，末尾禁止反斜杠；修改中文注释后运行 `tools/CheckSourceCommentSafety.ps1`。
- UI 可见文字用英文 `tr()`，源码注释、README/CHANGELOG 和 Git 提交日志使用中文。
- 每个模块维护 README 和按日期记录的 CHANGELOG；全局文档只维护 `F:\MeyerScan\Documents`。
- GitHub 根仓库为 `F:\MeyerScan`；提交信息中文且具体。
- 每次 GitHub 提交后，用 `tools/BackupToLocalRepository.ps1` 整体备份到 `F:\MeyerScan-Reposit`；不备份第三方库、日志、现场数据库和 IDE 临时文件。

## 6. 当前开发优先级

1. 建立 LoginAdapter，隔离既有登录模块 ABI。
2. 完成 CaseOrderService、ScanSchemaService 和 OrderWorkflowService，打通真实创建、保存、列表、打开和继续扫描。
3. 完成 SettingsUI 的版本化上下文注入、保存、恢复默认和来源页刷新。
4. 完成 Permission 的六维快照与 UI/MainExe/Service/Workflow 多层复核。
5. 只为 ScanReconstructStudio 独立 EXE 形态建立最小版本化 IPC，再接真实设备、算法和数据 IO。
6. 将编辑、配准、测量、颈缘、倒凹、咬合、底座等处理能力按实际依赖逐项拆出。
7. 打通 DataExport/Network/Send，再实施自动更新和安装打包。

当前原则：不继续增加只有页面和按钮的占位模块，优先把患者/订单主链路做成可保存、可加载、可失败回滚的真实闭环。

## 7. 完成定义

一个模块或功能只有同时满足以下条件才能标记“完成”：

- 职责和禁止事项清楚，依赖方向符合架构规范。
- VS2015 Release x64 和 CMake Release 可构建。
- 测试宿主/smoke 覆盖成功、失败和关键边界。
- 关键操作、失败和资源生命周期有日志。
- 文件版本、代码版本和版本清单一致。
- 正式数据与测试数据隔离，无开发机绝对路径。
- README、CHANGELOG、全局进度和必要接口说明同步。
- 与上下游模块完成真实联调，而不只是单模块页面可显示。

---

> **文档版本**：v3.0（2026-07-14，删除重复历史方案，保留现行范围、完整模块清单、流程和强制规则）
