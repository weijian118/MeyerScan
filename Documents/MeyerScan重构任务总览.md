# MeyerScan 口扫软件重构 — 任务总览

> **文档目的**：重构项目的权威参考文档，包含任务背景、功能需求、重构原则、预期目标、开发约束、开发顺序。
>
> **创建日期**：2026-06-16（合并自原任务总纲、方案优化建议）
>
> **适用范围**：所有参与 MeyerScan 重构的开发人员
>
> **权威文档位置（2026-07-12 起）**：仓库内 `F:\MeyerScan\Documents\MeyerScan重构任务总览.md`。`D:\wj\重构文档` 下同名文件仅为同步镜像；后续先修改仓库版本，再同步镜像，禁止两边分别演进。
>
> **四文档分工**：本文件只维护产品范围、总体模块清单、核心原则和全局约束；接口/依赖进入《架构设计与接口规范》；完成状态进入《开发进度跟踪》；逐次决策与验证证据进入《AI协作记录》。

---

## 一、任务背景

### 1.1 当前软件概述

MeyerScan 是美亚光电口腔数字印模仪（mOS MyScan / mOS MyScan 5/5H）的配套 PC 软件。软件核心定位为口腔扫描数据采集、处理、管理、上传和外部系统集成，主要面向口腔诊所、公立医院、大型连锁机构和 OEM/渠道客户，核心功能围绕 **"病例-订单-扫描-配置-云端-外部集成-自动更新"** 展开。

**核心功能清单**（基于 helpCenter.html 梳理）：

| 功能域 | 功能项 | 说明 |
|--------|--------|------|
| **病例管理** | 患者信息管理 | 姓名、年龄、性别等基本信息 CRUD |
| | 订单管理 | 订单编号、类型、状态、时间范围检索 |
| | 数据管理 | 导入/导出/删除患者及订单数据 |
| | 扫描方案 | 正畸/修复模式，牙位选择，修复体类型/材料/齿色 |
| **扫描采集** | 主扫描功能区 | 牙颌选择(上颌/下颌/咬合)、扫描控制(开始/暂停/完成/删除) |
| | 二维实时显示 | 口内实时扫描画面 |
| | 3D 模型显示 | VTK 重建渲染，支持旋转/缩放/平移 |
| | 数据补扫 | 缺失区域补充扫描 |
| | 辅助工具 | AI 软组织消去、牙模/真牙模式、色彩切换、编辑(笔刷/圈选)、除色、挖孔、数据锁定、视角锁定 |
| **数据处理** | 编辑 | 笔刷/圈选删除数据、模型内外侧翻转 |
| | 颈缘线 | 手动/半自动绘制、保存/导入 .xyz 坐标 |
| | 测量 | 距离测量(两点)、角度测量(三点) |
| | 倒凹分析 | 梯度颜色标注、自动/手动计算 |
| | 咬合分析 | 上下颌显示/隐藏、咬合距离计算、开合展示、六视图、剖面线 |
| | 色彩模式 | 石膏色显示，绿色标注自动补洞区域 |
| | 底座生成 | 自定义裁剪平面、添加底座、保存 STL |
| **发送导出** | 案例信息确认 | 患者/医生/技工所/邮箱信息编辑 |
| | 数据导出 | 导出到本地文件夹 |
| | 邮箱发送 | 打包数据发送至技工所邮箱 |
| **练习模块** | 简化创建流程 | 扫描+数据处理(无病例管理) |
| | 视频录制 | 扫描过程录屏(设置中开启) |
| **浏览模块** | 患者管理 | 多条件搜索、批量导入/导出/删除、患者信息修改 |
| | 订单管理 | 订单检索、详情查看、牙齿信息浏览、进入数据处理界面 |
| **设置模块** | 一般设置 | 新手引导开关、音乐风格、数据格式(STL/PLY/OBJ)、订单存储/打包路径 |
| | 信息管理 | 诊所/技工所/医生信息 CRUD，授权码检测 |
| | 校准 | 三维校准(标定器+25幅采集+计算)、颜色校准(颜色标定器) |
| | 云端账号 | 账号登录软件，可联网登录，也必须支持离线使用；设备信息管理 |
| | 云端上传 | 订单上传到口扫云，支持失败重试、状态记录和后续同步 |
| **外部集成** | 第三方软件拉起 | 支持第三方软件（如美亚美牙）打开本地口扫软件，并传入患者/订单信息自动建单 |
| | HIS / Worklist | 支持接入公立医院或大型连锁 HIS 系统，通过 Worklist 下拉患者信息并建单 |
| **自动更新** | 检查更新 | MeyerScan.exe 内点击“检查更新”，生成本地更新信息并调用 MyUpdate.exe |
| | 主动更新 | 用户双击 MyUpdate.exe 主动检查更新，界面展示满足/不满足项 |
| | 策略升级 | 支持云端按账号白名单、设备编号白名单、硬件要求、驱动要求等策略决定是否允许升级 |
| **安装打包** | 安装包生成 | 收集 EXE、DLL、Qt 运行库、插件、配置模板、资源文件和版本清单，生成正式安装包 |
| | 自定义安装界面 | 支持品牌、语言、许可、安装路径、组件选择、进度和完成页等安装向导自定义显示 |
| | 自定义安装流程 | 支持安装前检查、旧版本检测、目录创建、快捷方式、安装后初始化、卸载入口等流程 |
| | 关于 | 软件名称、版本、授权用户、有效期、设备编号 |
| | 数据处理设置 | 上下颌补洞范围、扫描杆补洞范围 |
| | 扫描设置 | 扫描提示图开关、可续扫时间(3/5/7/15天)、录屏、默认订单类型、完成后跳转、体感控制 |

### 1.2 当前软件核心痛点

| 痛点类别 | 具体表现 |
|----------|----------|
| **单体臃肿单进程架构** | 单一主程序 EXE + 少量零散 DLL；无插件化、无独立进程、无分层 |
| **无清晰架构层级** | UI、业务、扫描、算法、数据库、设备通信完全耦合在一个工程中 |
| **无模块化** | 仅按功能堆代码，无独立模块/插件，所有功能以代码文件夹形式混在主工程 |
| **调用关系混乱** | UI 按钮点击 → 直接操作数据库 → 直接发设备指令 → 直接启动扫描 → 直接调用 VTK 重建 |
| **单进程致命缺陷** | 扫描、重建、渲染、VTK、UI、数据库、设备全部在同一进程空间，任何模块异常 = 整机崩溃退出 |
| **数据传递无规范** | 跨模块直接传递 `QString`/`std::string`/`std::vector`/指针/自定义类，结构体多处定义版本不一致 |
| **配置/日志/权限散写** | 无统一配置文件格式、无统一日志接口、权限判断硬编码 |
| **编译发布低效** | 代码体量庞大，一处修改全量编译；现场升级必须完整重装 |
| **差异化交付困难** | 多客户/多机型/多区域需维护多套代码分支；OEM 定制需改源码重编译 |

---

## 二、功能需求说明

### 2.1 当前功能保留范围

重构需完整保留 helpCenter.html 中所描述的全部用户功能，不得删减功能。

### 2.2 结构调整要点

| 调整项 | 说明 |
|--------|------|
| 基本信息 + 扫描方案 | 从创建模块移入案例管理模块 |
| 发送界面 | 拆为 `MySendUI` 独立 UI 模块，挂入 `OrderScanWorkspaceShell` 的 Send 步骤；真实导出、压缩、邮件发送和上传继续由后续服务模块承接 |
| 扫描 + 数据处理 | 保留在 ScanReconstructStudio.exe 中（独立进程） |
| 练习模块 | 被 ScanReconstructStudio.exe 覆盖（无病例管理的扫描流程） |
| 浏览模块 | 归入案例管理模块 |
| 设置模块 | 拆分到各对应 DLL |

### 2.3 本次重构新增/增强功能

- SQLite 数据库支持（保留 MySQL，通过配置文件切换）
- 六维权限管控（角色 + 客户 + 设备机型/序列号 + 软件版本 + 时间 + 配置方案）
- 多语言支持（UI 显示文字统一用 `tr("English source text")` 包裹；即使需求写中文按钮名，源码 source text 也写英文，中文由 `.qm` 提供；源码注释、模块变更记录和 GitHub 提交日志统一使用中文）
- 多分辨率自适应
- 主界面切换无闪现：首页、浏览、设置、建单等主页面切换由主 EXE 统一管理，使用统一容器切换；是否缓存或释放按资源重量决定，避免反复 close/show 顶层窗口造成闪现
- 崩溃隔离（扫描重建独立 EXE）；优先规划 MeyerScan.exe 与 ScanReconstructStudio.exe 之间的信息传输/状态同步，心跳可保留为状态检测，暂不把超时自动重启作为重点目标
- 配置版本化管理（校验、自动迁移、安全回滚）
- 插件清单化（通过配置文件加载模块，不硬编码）
- 结构化日志（时间/级别/模块/操作/设备ID/案例ID/操作人）；客户每一步可见操作和关键内部步骤都必须输出日志
- 第三方/医院系统建单入口（第三方软件拉起、HIS/Worklist 下拉患者信息）
- 独立自动更新程序 MyUpdate.exe（与 MeyerScan.exe 同级目录）
- 安装打包模块（生成安装包、自定义安装界面/流程、安装目录层级和发布文件清单）

---

## 三、重构原则

### 3.1 架构解耦与稳定性

1. **模块高度解耦**：UI、业务、设备、算法、数据库彻底分离
2. **UI 与业务完全解耦**：UI 仅做展示，不承载设备、通信、算法逻辑
3. **核心/易变模块差异化处理**：稳定核心功能静态库化，易变模块 DLL 插件化
4. **高风险模块进程隔离**：扫描、重建、VTK 渲染独立为 EXE 进程
5. **双 EXE 状态同步**：MeyerScan.exe 与 ScanReconstructStudio.exe 之间保持订单上下文、扫描状态、处理进度的信息同步；心跳只作为状态检测手段，暂不重点规划自动重启

### 3.2 统一规范与安全传递

6. **全局唯一入口**：数据库、设备通信、配置、权限、日志、病例业务统一收口
7. **数据安全传递**：跨 DLL 可使用 Qt 容器，跨进程仅允许 POD 结构体
8. **接口兼容规范**：接口仅新增不修改，保证二进制兼容
9. **统一资源管理**：内存、显存、文件、数据库连接全局统一管控

### 3.3 产品稳定性与数据安全

10. **操作可排查**：客户每一步可见操作和关键内部步骤都必须留日志，便于定位用户现场问题
11. **数据可追踪**：患者、订单、扫描数据的关键状态变化可追踪
12. **异常可恢复**：崩溃、断电、网络中断后尽量保留可恢复状态
13. **六维权限管控**：基于"角色+客户+设备机型/序列号+软件版本+时间+配置方案"

### 3.4 扩展适配与开发效率

14. **配置/插件驱动**：配置版本化，插件清单化
15. **小团队轻量化**：架构清晰、易分工、易调试、易维护
16. **底座先行、分层开发、并行交付**

### 3.5 最终架构原则（2026-06-17 再梳理）

本次重构最终采用 **主 EXE + 静态库 + 插件 DLL + 独立进程** 的轻量化架构。目标不是把所有功能都 DLL 化，而是在产品稳定、代码可读、人工可维护、碎片化开发之间取得平衡。

| 形态 | 放什么 | 不放什么 | 判断标准 |
|------|--------|----------|----------|
| **主 EXE** | 启动、插件加载、窗口容器、进程调度、全局异常兜底 | 业务规则、数据库 SQL、设备通信、算法处理 | 只做壳和编排，代码越少越好 |
| **静态库 / 公共头** | 仅在多个模块已经出现稳定、重复的 ErrorCode/Result、动作 ID、IPC 消息头或无状态小工具时按需抽取 | 患者/订单高频字段、业务逻辑、配置读取、数据库访问、Qt UI 工具 | 不预先创建 Core/Entity 大包；先有真实复用再抽取，并保持单向依赖 |
| **插件 DLL** | Logger、Database、Config、Permission、病例/订单服务、UI、设备/云端/导出等可独立维护模块 | 高频内部小工具、只被一个模块使用的实现细节 | 有独立变化原因、独立测试价值、独立升级或客户定制价值；MainExe 对自研功能/支撑 DLL 优先运行时动态加载，不用 import lib 把主程序和插件硬绑死 |
| **独立进程** | ScanReconstructStudio.exe（独立扫描壳，动态加载扫描阶段 UI 和数据处理阶段 UI）、MyUpdate.exe（自动更新、补丁下载、覆盖升级） | 病例管理、权限核心、云端上传、全局配置、设备协议和算法细节 | 高风险、高算力、需独立退出/覆盖文件或需隔离主程序生命周期的功能才进程隔离；EXE 壳保持薄，扫描/处理阶段页面和后续处理能力继续模块化 |
| **发布交付模块** | MyInstaller/Packaging（安装包生成、安装向导、自定义安装流程、安装目录层级、依赖收集、版本清单写入） | 运行时业务逻辑、扫描采集、病例管理、数据库访问 | 只在发布/安装阶段运行，不作为 MeyerScan.exe 运行时插件 |

**模块拆分尺度**：

1. **足够小，但不是越碎越好**：模块应小到一个开发者能在半天内读懂主要流程；但如果拆分后接口数量、版本管理和调试成本明显超过收益，就先保留为源码子目录。
2. **按变化原因拆分**：患者、订单、扫描方案、加载订单规则、导入导出、统计分别变化，所以拆开；同一个扫描工作台内部工具只在扫描进程内变化，先做内部子目录。
3. **代码量只是信号，不是唯一标准**：单个 `.cpp` 超过约 800 行、单个模块业务源码超过约 3000-5000 行、公开接口超过约 20 个方法时，必须复盘是否拆分；但不能为了行数硬拆无边界模块。
4. **接口少于实现重要**：对外接口要少、稳定、可注释清楚。复杂度可以留在模块内部，不能扩散到调用方。
5. **优先可运行竖切**：每个阶段都要形成可编译、可 smoke、可独立验证的模块，不追求一次性完美设计。

**拆分模块总清单维护规则**：

- 正式清单位置：本节 `3.6 拆分模块总清单` 与 `MeyerScan架构设计与接口规范.md` 的 `2.4.1 拆分模块总清单（中文名 / 项目名 / 产物名 / 功能详情）`；两处必须保持一致。
- 清单必须包含：模块中文名、项目名/目录、DLL/EXE/LIB 名、形态、功能详情、边界与注意事项。
- 已落地模块按当前 `F:\MeyerScan` 一级目录记录；规划模块按建议项目名记录，后续创建项目时优先沿用清单命名。
- 新增、删除、合并、改名模块时，必须同步更新正式清单、进度文档和模块内 `README.md` / `CHANGELOG.md`。
- 清单不是为了追求 DLL 数量，而是让后期人工维护能快速判断“功能放哪个模块、哪个模块不该管什么”。

### 3.6 拆分模块总清单（中文名 / 项目名 / 产物名 / 功能详情）

> **说明**：本清单同时覆盖已落地模块和规划模块。已落地模块按当前 `F:\MeyerScan` 一级目录记录，规划模块按建议项目名记录。后续创建项目时优先沿用本清单命名，避免项目名、产物名和功能边界漂移。

| 模块中文名 | 项目名 / 目录 | DLL / EXE / LIB 名 | 形态 | 功能详情 | 边界与注意事项 |
|------------|---------------|--------------------|------|----------|----------------|
| 主程序壳 | `MyMainExe` | `MeyerScan.exe` | 主 EXE | 软件唯一用户入口；创建 `QApplication`；单实例控制；启动等待页；最早动态加载 Logger；运行时动态加载 ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter 和各 UI/流程 DLL；当前阶段由 MainExe 内部生成运行时版本清单；调用登录模块；统一管理首页、案例管理、建单、设置等主页面切换；启动或激活扫描重建进程；记录跨模块导航日志。 | 只做启动、编排、窗口容器、轻量版本清单和进程调度；不写业务 SQL、不写订单规则、不写扫描算法、不直接控制设备协议；自研 DLL 通过 `QLibrary + C ABI 工厂函数` 获取接口，避免 MainExe 链接大量 import lib。 |
| 登录测试宿主 / 登录适配入口 | `MyLogin`；后续可新增 `MyLoginAdapter` | 当前调用既有 `MeyerLoginWidget.dll`，测试宿主为 `MeyerLoginTest.exe`；后续建议产物 `MeyerScan_LoginAdapter.dll` | DLL / 测试 EXE | 当前负责验证既有登录 DLL 可被新仓库拉起；后续 LoginAdapter 负责组装登录参数、转换登录返回状态、隔离外部登录头文件编码/字段变化；支持云端账号登录、离线许可、联网/离线状态返回。 | 不重写既有登录业务；不做病例管理、云端上传细节；MainExe 不应长期直接理解登录模块内部字段。 |
| 日志模块 | `MyLogger` | `MeyerScan_Logger.dll`，测试宿主 `LoggerTest.exe` | DLL | 结构化日志写入、日志级别过滤、每天主文件 + 超限尾号分卷、逐条同步写入/刷盘/关闭句柄、多线程/多进程安全写入、支持模块缓存 `ILogger* m_logger` 后持续 `Write()`；供 MainExe、Database、UI、服务、扫描进程统一记录操作链路。 | 基础设施模块，不承载业务规则；日志目录由调用方传入，固定写入 `MeyerScan.exe` 同级 `logs`；默认文件 `MeyerScan_YYYYMMDD.log`，超过大小上限后生成 `_NNN`；空上下文字段必须传 `""` 并由 Logger 省略，不做 `-` 占位；Logger.dll 本体不依赖 Qt，Qt 模块通过头文件内联层使用 `QString`。 |
| 数据库模块 | `MyDatabase` | `MeyerScan_Database.dll`，测试宿主 `DatabaseTest.exe` | DLL | 纯 C++ 基础设施；读取 `config/db_config.json`；当前默认数据库类型为 SQLite；SQLite 通过动态加载 `sqlite3.dll` 调 C API；保留 MySQL 配置和类型枚举，原生 MySQL C API 待 SDK 接入；提供裸 SQL 执行、事务、备份、脚本执行、通用 SELECT 结果 JSON 化；相对路径按配置文件所在目录解析；SQLite 首次连接前自动创建数据库文件父目录。 | 不依赖 Qt，不理解患者、订单、医生、诊所、技工所、权限语义；不做业务 CRUD；不自动加载旧 `mysql.sql`；业务表结构由服务/迁移模块调用 Database 执行。 |
| 数据库 Qt 适配层 | `MyDatabaseQtAdapter` | `MeyerScan_DatabaseQtAdapter.dll` | DLL | Qt 模块访问纯 C++ Database 的转换层；把 `QString` 路径/SQL 转 UTF-8；把 Database 通用表格 JSON 转 `QJsonDocument` / `QJsonArray`；统一处理查询缓冲区扩容；供 MainExe、RuntimeDataCenter、CaseOrderService 和测试宿主使用；直接使用 Logger 记录适配层生命周期和失败边界日志。 | 只做类型转换和缓冲区管理；不做业务 CRUD、不理解字段含义、不连接数据库；Database 不反向依赖本模块；正常高频查询成功不逐条写日志，避免噪声。 |
| 运行时数据中心 | `MyRuntimeDataCenter` | `MeyerScan_RuntimeDataCenter.dll`，测试宿主 `RuntimeDataCenterTest.exe` | DLL | 将本地常用数据库信息和云端诊所信息加载为进程内 JSON 快照；本地 domain 包括诊所、技工所、软件信息、医生、设置、账号、订单、患者、设备；云端 domain 当前为 `cloud.clinicProfile`，由登录/云端同步模块注入；MainExe 启动后初始化，CaseUI 当前通过它读取患者/订单列表。 | 只做运行时读模型/缓存，不做 CRUD、不做 UI、不做权限判断、不联网请求；不允许调用方传 SQL 或表名；字段经常变时优先通过 JSON 快照、`schemaVersion` 和扩展字段吸收变化，避免频繁改 UI/主程序 ABI；快照有容量上限，超出后必须改分页/服务查询。 |
| 配置中心 | `MyConfigCenter` | `MeyerScan_ConfigCenter.dll` | DLL | 读取和生成 `config/runtime_config.json`；提供布尔、整数、字符串配置读取；后续扩展配置版本、迁移、校验、配置变更通知、加密配置读取接口。 | 不做权限判断、不做数据库 CRUD、不写业务流程；配置只提供产品/客户默认策略。 |
| 权限核心 | `MyPermission` | `MeyerScan_Permission.dll` | DLL | 读取 `config/permission_rules.json`；提供功能 visible / enabled 查询；后续实现角色、客户、设备机型、设备序列号/加密狗、软件版本、时间有效期、配置方案等六维权限快照和高价值动作复核。 | 不做 UI、不直接操作业务数据库；不能只靠 UI 隐藏，Service/Workflow/IPC 必须复核。 |
| 共享 UI 组件 | `MyUIComponents` | `MeyerScan_UIComponents.dll` | DLL | 提供等待页、页面标题、字段标签、按钮、输入框、下拉框、日期框、多行文本框、基础表格等控件工厂；当前按钮样式按“角色 + 内容布局”统一管理，角色包括 Primary/Secondary/Text/Danger/Entry，内容布局包括纯文字、纯图标、左图右文、上图下文；基础表格统一表头、边框、隔行色、只读默认行为和整行选择；后续扩展公共弹窗、Toast、复杂表格能力、日历、主题、DPI/Layout 规则、LanguageManager。 | 只统一视觉、尺寸、多语言和基础交互体验；不承载业务按钮行为、不读取配置、不做权限判断、不决定页面跳转；表格列名、数据、分页、排序、右键菜单和双击动作由业务模块负责；仅单个模块使用的截图还原控件或特殊业务控件留在自身模块内部；公共虚接口新增方法只能追加到接口末尾，不能插入旧方法中间。 |
| 统一 UI 资源 | `MyUIResources` | `MeyerScan_UIResources.dll`，测试宿主 `UIResourcesTest.exe` | DLL / 只读资源提供器 | 构建时扫描各 UI 模块 `Resources` 下允许发布的 PNG/QSS/SVG/ICO/JPG/BMP/GIF/QM，生成 qrc，使用 `rcc -binary` 生成 RCC 数据，再以 Win32 `RCDATA` 嵌入 DLL；注册后统一通过 `:/MeyerScan/Modules/<ProjectName>/...` 读取。 | 资源源码仍归各业务 UI 模块维护；本模块不创建控件、不承载业务、不管理语言切换。资源 DLL 化可防普通误改和单文件遗失，但不是加密；安装完整性仍需版本清单、哈希/签名、安装包校验和修复流程。当前不拆 Icon.dll/Qss.dll，避免重复加载和升级原子性问题。 |
| 版本清单能力 | 当前在 `MyMainExe` 内置；`MyVersionManager` 暂保留历史骨架 | 当前由 `MeyerScan.exe` 生成；历史骨架为 `MeyerScan_VersionManager.dll` | 主 EXE 内置 / 后续可再拆 DLL | 当前阶段启动时由 MainExe 读取 `config/version_modules.json`，只记录清单中声明的 MeyerScan 拆分模块 EXE/DLL；输出 Windows 文件版本 `fileVersion`、模块代码版本 `codeVersion`、文件大小、修改时间和 `versionMatch`，生成 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`；供售后排查、安装打包、自动更新复用。 | 不扫描运行目录全部 DLL，不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库；manifest 使用 `file + versionFunction` 结构，自研 DLL 固定通过统一导出函数 `GetMeyerModuleVersion()` 读取代码版本；文件名包含毫秒，避免同一秒内多次启动覆盖快照；`MyVersionManager` 历史骨架也使用同一 manifest 规则，避免后续误用旧模块时口径漂移。若后续扩展到算法 DLL 哈希、签名校验、云端比对、自动更新策略，再恢复独立模块。 |
| 公共稳定契约候选 | 暂不单独建项目 | 按需使用小型公共头或静态库 | 候选 / 非前置 | 只有 ErrorCode/Result、稳定动作 ID、IPC 消息头等已经在多个模块重复且变化较低时才抽取。 | 当前不创建通用 `Core.lib`；禁止把业务规则和各种 Helper 汇入公共层。 |
| 患者订单数据合同 | 由 `MyCaseOrderService` 管理 | 服务 DTO + UTF-8 JSON，不单独生成 Entity DLL/LIB | 服务内契约 | 使用 `schemaVersion`、稳定字段 key、`extensions` 和数据库迁移映射承接患者/订单字段变化；跨模块通过调用方缓冲区返回 UTF-8 JSON。 | 当前不创建固定字段 `CaseEntity.lib`；避免字段变化造成静态 ABI 漂移和全量重编译。 |
| 病例订单服务 | `MyCaseOrderService` | `MeyerScan_CaseOrderService.dll` | DLL | 患者/订单组合数据 CRUD、查询、状态管理、操作留痕；医生、技工所、诊所等数据库主数据/字典数据读写；字段版本、扩展字段和 schema 映射；当前已创建 v0.2.0 骨架，包含 `ms_patient_order`、`ms_reference_data` 轻量 schema 和 JSON 查询边界；向 CaseUI、OrderCreateUI、ExternalLaunchAdapter、HisWorklistAdapter 提供标准接口。 | 不做 UI 渲染、扫描方案、扫描采集、算法处理、设备通信和加载订单流程决策；替代原 `CaseService.dll` + `OrderService.dll`。 |
| 扫描方案服务 | `MyScanSchemaService`（规划） | `MeyerScan_ScanSchemaService.dll` | DLL | 扫描方案、牙位、修复体、材料、齿色、咬合/上下颌等结构化数据保存和读取；为建单、继续扫描、扫描重建提供标准方案数据。 | 不做相机采集、不做 3D 重建、不做 UI 表单展示。 |
| 加载订单规则服务 | `MyOrderWorkflowService`（规划） | `MeyerScan_OrderWorkflowService.dll` | DLL | 统一判断新建、加载、继续扫描、进入处理、进入发送；综合订单状态、扫描方案、数据文件完整性、权限、客户配置、机型、软件版本后输出决策。 | 不保存数据、不渲染 UI；HomeUI、CaseUI、OrderCreateUI、ScanReconstructStudio 不得重复实现这套规则。 |
| 首页 UI | `MyHomeUI` | `MeyerScan_HomeUI.dll`，测试宿主 `HomeUITest.exe` | DLL | 首页入口展示；Create / Browse / Practice / Settings 等入口状态；OEM 首页布局、Logo、主题、入口显隐；入口点击通过回调上报 MainExe。 | 只做入口 UI；不做建单规则、不判断加载订单、不直接决定扫描流程、不做权限核心判断。 |
| 案例管理 UI | `MyCaseUI` | `MeyerScan_CaseUI.dll`，测试宿主 `CaseUITest.exe` | DLL | 患者/订单列表、搜索、导入导出按钮、删除按钮、打开订单按钮、返回首页按钮、页签切换；模块内客户操作日志；通过操作 ID 上报 MainExe；当前患者/订单列表先读取 RuntimeDataCenter 的 JSON 快照。 | 正式业务不得直接访问 Database；列表读模型可走 RuntimeDataCenter，搜索/CRUD/保存必须走 CaseOrderService/DataExport；打开订单必须走 OrderWorkflowService 和 MainExe 扫描前资源释放流程。 |
| 建单 UI | `MyOrderCreateUI` | `MeyerScan_OrderCreateUI.dll`，测试宿主 `OrderCreateUITest.exe` | DLL | 手工建单、第三方传参建单、HIS/Worklist 下拉患者建单共用表单；当前 v0.5.2 采用单页三栏工作台内容布局；患者/订单基本信息、五种修复类型、治疗方案图片选择、扫描流程输入、已选牙位明细、标信息占位和确认操作均在同一个界面内；治疗方案区使用上下颌牙弓图片、mask 像素命中、`1/3/4/5/7` 牙位叠加图和桥连接点叠加图；前四种类型 hover/选中只在图标区域显示彩色圆底，种植体使用整行高亮；扫描流程预览固定高度，类型变化不挤压牙弓；通用控件基础样式通过 `MeyerScan_UIComponents.dll` 动态加载复用；`GetCurrentScanProcessJson()` 生成 `scanProcess` JSON。 | 只收集和展示输入；不保存数据库、不决定加载订单规则、不直接启动扫描进程；不把 bridge 当修复类型；不绘制 Order/Scan/Process/Send 步骤导航，步骤导航唯一归 `OrderScanWorkspaceShell`；后续保存走 CaseOrderService / ScanSchemaService / OrderWorkflowService。 |
| 建单扫描工作台壳 | `MyOrderScanWorkspaceShell` | `MeyerScan_OrderScanWorkspaceShell.dll` | DLL / UI 容器 | 将建单、扫描、数据处理、发送等步骤放入统一工作台外壳；当前 v0.1.2 支持创建模式（Order / Scan / Process / Send）和练习模式（Scan / Process）；顶部整合品牌、返回、唯一的步骤导航、最小化和关闭，并通过动作/步骤回调通知 MainExe。 | 只做容器和导航；不做建单保存、不做扫描算法、不做真实导出/上传；OrderCreateUI、ScanWorkflowUI、DataProcessUI 和 SendUI 不得重复绘制步骤导航。 |
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

**轻量化禁令**：

- 禁止为了“架构漂亮”增加没有独立交付价值的 DLL。
- 禁止创建万能 Manager、Common、Helper，把业务规则塞进公共层。
- 禁止 UI 直接访问数据库、直接判断权限核心、直接决定扫描/发送流程。
- 禁止跨 DLL 传递需要调用方释放的 C++ 对象。
- 禁止无测试宿主、无 smoke 验证、无版本资源的模块进入集成阶段。
- 扫描流程创建当前遵循 `OrderCreateUI 生成 scanProcess JSON -> MainExe 合并/转发 -> ScanWorkflowUI/DataProcessUI 只渲染 steps`；Scan/Process 页面不得反向解析建单开关或复制流程规则。`Segmented scanbody` 只表示对应颌第二扫描杆/第二异性扫描杆是否显示，普通扫描杆流程仍由对应颌是否存在 implant 牙位触发。

### 3.7 患者/订单与主数据读写口径

患者/订单、医生列表、诊所列表、技工所列表、操作人列表等数据分为两类入口：运行时只读列表/上下文快照走 `RuntimeDataCenter.dll`，正式保存、修改、删除、状态变更和强业务查询走 `CaseOrderService.dll`。UI 不直接访问 Database，也不直接写旧表名。

| 数据 | 归属模块 | 读取/保存方式 | 说明 |
|------|----------|---------------|------|
| 本地患者/订单列表快照 | `RuntimeDataCenter.dll` | `GetDomainJson("local.patients")` / `GetDomainJson("local.orders")` | 供 CaseUI 等界面先显示列表；只读缓存，不承担保存和删除 |
| 本地诊所/技工所/软件/医生/设置/账号/设备快照 | `RuntimeDataCenter.dll` | `local.clinics` / `local.labs` / `local.software` / `local.doctors` / `local.settings` / `local.users` / `local.devices` | 字段经常变，统一返回 JSON 快照；UI 按需读取字段，不传 SQL、不传表名 |
| 云端诊所信息 | `RuntimeDataCenter.dll` | `UpdateCloudClinicJson()` / `GetDomainJson("cloud.clinicProfile")` | 登录/云端同步模块拿到云端 JSON 后注入；当前不在 RuntimeDataCenter 内联网请求 |
| 患者/订单组合保存和强查询 | `CaseOrderService.dll` | `SavePatientOrderJson()` / `GetPatientOrderJson()` / `QueryJson("patientOrder.byOrderId")` | 患者与订单强绑定，字段变化通过 `schemaVersion`、稳定 key 和 `extensions` 吸收 |
| 医生/诊所/技工所等主数据维护 | `CaseOrderService.dll` | `ListReferenceDataJson("doctor/clinic/lab")` 或后续专用 CRUD | 涉及新增、删除、编辑、状态过滤、权限复核时必须走 Service |
| 扫描方案 | `ScanSchemaService.dll` | 后续扫描方案接口 | 牙位、修复体、材料、齿色等不放入 CaseOrderService |

`Database.dll` v1.3.0 当前为纯 C++ 基础设施，`ExecuteQueryJson()` 只是把通用 SELECT 结果转成 JSON；它不理解业务字段含义。Qt 模块必须通过 `DatabaseQtAdapter.dll` 完成 `QString` / `QJsonDocument` 与 Database UTF-8/POD 接口的转换。`RuntimeDataCenter.dll` 可以在内部读取旧表并包装成稳定 domain 快照，但它仍不是业务 CRUD 服务。真正的数据分类、字段版本、权限复核、状态过滤、保存和删除都在 `CaseOrderService.dll` / `ScanSchemaService.dll` 内完成。

SQLite 默认链路切换完成后，首次运行的本地库可能是空库。旧 `mysql.sql` 只作为旧字段、旧表关系和迁移映射参考，不由 Database 或 RuntimeDataCenter 自动执行。后续如果需要把旧数据迁入 SQLite，应建立独立 migration/adapter 流程，由 CaseOrderService 或专门迁移模块负责建表、字段映射、版本记录和回滚。

RuntimeDataCenter 的快照读取只适合轻量上下文：诊所/技工所/医生/账号/设备、患者列表、订单列表和云端诊所概要。若某类数据记录量大、字段重、需要搜索分页或导出，应改走 `CaseOrderService.QueryJson()`、DataExport 或专用分页接口，不允许继续扩大启动期全量缓存。

---

## 四、预期目标

### 4.1 整体目标

- 产品稳定优先：关键操作可排查、核心数据可追踪、异常状态可恢复
- 崩溃隔离 + 自恢复：高算力模块独立进程
- 稳定模块静态化，易变模块 DLL 化
- 全局唯一入口：配置、权限、设备、数据库、日志统一收口
- 六维权限控制
- 接口只增不改：保证二进制兼容
- UI 与业务彻底分离
- 高风险功能独立：固件升级、标定、算法单独发布与回滚
- 统一资源管理：模块私有图标、图片、mask、QSS 和模块内 qm 的源码仍放在各模块 `Resources/`；正式构建统一编译进 `MeyerScan_UIResources.dll`，运行时通过 `:/MeyerScan/Modules/<ProjectName>/...` 读取，不再把 PNG/QSS 散落到客户目录。登录许可等非 UI 运行文件继续位于 `MeyerScan.exe` 同级 `Resources/`；多个模块共用的控件工厂归 UIComponents，共用只读图片仍可放明确归属的资源目录
- 轻量化迭代
- 配置版本化 + 插件清单化

### 4.2 扩展性功能快速适配目标

| 扩展类别 | 目标场景 | 预期交付方式 |
|----------|----------|-------------|
| **UI 定制** | 客户 Logo/布局/主题/语言包定制；OEM 品牌适配 | 替换 UI.dll 或修改配置文件 |
| **病例管理** | 不同医院/国家临床流程定制；扩展患者/订单字段 | 独立升级 CaseUI.dll/CaseOrderService.dll，必要时调整字段映射或 migration |
| **设备适配** | 新增口扫机型；单一机型协议修复/升级 | 新增对应协议 DLL |
| **权限与商业化** | 试用/订阅/限时授权；按功能授权 | 升级 Permission.dll |
| **版本与交付** | 国内版/海外版/OEM 版切换 | 修改配置文件 + 可选 UI.dll 替换 |
| **云服务扩展** | 新平台对接；通信协议变更 | 独立升级 NetworkHelper.dll |
| **第三方软件建单** | 美亚美牙或其他第三方软件拉起本地口扫软件，传入患者/订单信息后自动进入建单流程 | 扩展 ExternalLaunchAdapter/OrderCreateUI/OrderWorkflowService，不改 CaseUI 列表和扫描进程 |
| **医院 HIS / Worklist** | 公立医院或大型连锁从 Worklist 下拉患者信息并建单 | 新增或替换 HisWorklistAdapter.dll，复用 OrderCreateUI 和病例域服务 |
| **自动更新策略** | 云端按账号、设备编号、显存/驱动/内存要求选择升级对象 | 独立升级 MyUpdate.exe 和云端更新配置，不把升级逻辑写进 MeyerScan.exe |

**扩展实现举例**：

1. **新增第三方建单入口**：第三方软件只负责传入患者/订单上下文文件或启动参数；MeyerScan.exe 调用 ExternalLaunchAdapter 归一化字段，并在后台复用首页“Create”入口权限后直接进入 OrderScanWorkspaceShell/OrderCreateUI；OrderCreateUI 负责展示/补全；CaseOrderService 保存患者/订单组合数据和医生/诊所/技工所等参考数据，ScanSchemaService 保存扫描方案。标准上下文必须携带 `source.thirdPartyType` 等第三方来源字段；后续新增第三方，只新增适配器映射规则和必要校验，不改扫描进程。
2. **接入 HIS / Worklist**：HisWorklistAdapter.dll 负责连接医院系统并转换字段；OrderCreateUI 只消费标准建单上下文；病例域服务只保存标准患者/订单结构。医院字段变化时优先改适配器和字段映射，不改 UI 主流程。
3. **OEM/海外版本差异**：Logo、语言、入口隐藏由 HomeUI/UIComponents/Permission 配置完成；业务权限由 Permission 和 Workflow 复核；不为每个客户维护独立分支。
4. **云端升级策略变化**：MyUpdate.exe 读取云端更新配置并做本机匹配；云端只调整 `MyCloudUpdate.json/xml` 策略即可影响账号/设备白名单和硬件门槛，不需要重新发布 MeyerScan.exe。

---

## 五、开发约束

### 5.1 团队约束

- **团队形态**：轻量化团队维护，允许多人分模块并行推进
- **非全职重构**：需同步维护旧版软件、新增功能和其他项目
- **协作模式**：模块内开发和测试尽量独立，公共接口统一评审，阶段性统一集成/联调
- **核心约束**：轻量化架构、拒绝过度工程化、支持碎片化开发、低调试/维护成本

### 5.2 技术约束

- **技术栈**：Qt 5.6.3 + VTK 8.0 + PCL 1.8.1 + OpenCV 3.3.0 + MySQL/SQLite
- **编译器**：MSVC 14.0 (Visual Studio 2015)，C++14
- **Qt 使用边界（2026-07-03 收口）**：界面模块正常使用 Qt Widgets、Qt Layout、信号槽、`QString` 和 `QJsonDocument`；非界面模块默认优先评估纯 C++ / 标准库 / 专用 SDK，能不用 Qt 就不用 Qt。`Database` 已经纯 C++，Qt 模块访问 Database 必须通过 `MyDatabaseQtAdapter`，禁止在 UI/Service 内重复散落 Qt/Database 类型转换。CaseOrderService/RuntimeDataCenter 可在内部暂用 Qt JSON，但公共头文件和长期 ABI 不暴露 Qt 类型。
- **跨 DLL 通信**：同一进程、同一 Qt/VS2015 运行环境内可使用 Qt 容器和 Qt JSON；对第三方 C ABI、长期兼容插件接口、不确定运行库边界，公共 ABI 优先使用 `const char*`、POD、调用方缓冲区或稳定 DTO
- **跨进程通信**：IPC 头部使用 POD；患者/订单等复杂上下文使用 UTF-8 JSON 字节、上下文文件路径或订单 ID，不传 QObject、QString/QJsonObject 对象指针或内存所有权
- **公共契约按需抽取**：当前不把 `Core.lib` / `CaseEntity.lib` 作为前置项目；只有稳定类型出现真实跨模块重复后才抽取小型公共头或静态库。患者/订单高频字段留在 CaseOrderService DTO 和版本化 JSON 中。
- **保留 MySQL + 引入 SQLite**：可通过配置文件切换数据库类型
- **Database 与 QtAdapter 规则**：Database.dll 保持纯 C++、POD、`const char*`、固定缓冲区和 Result 结构；`MyDatabaseQtAdapter.dll` 是 Qt 调用方进入 Database 的唯一类型转换层。后续若接入 MySQL C API、参数绑定或 DAO，也应优先放在 Database/Service 内，不让 UI 直接适配底层数据库。
- **UI 不直连业务数据库**：HomeUI 不访问 Database；CaseUI/SettingsUI 正式代码只读取 RuntimeDataCenter 快照；测试宿主造数据或 MainExe 启动健康检查必须经 DatabaseQtAdapter；正式业务功能必须走 CaseOrderService、ScanSchemaService、OrderWorkflowService 等服务，UI 不拼 SQL、不查表、不做 CRUD
- **旧 mysql.sql 不自动接入**：`F:\MeyerScan\MyCaseManager\mysql.sql` 只作为旧版表结构参考；新系统建表/迁移必须采用版本化脚本，由 ConfigCenter 或病例域服务选择后调用 Database 执行，不能让 Database 硬编码加载旧脚本
- **首页只做入口 UI**：HomeUI.dll 独立存在，便于 OEM/客户定制；不得放建单、加载订单、权限核心判断
- **界面切换统一由主 EXE 管理**：HomeUI、CaseUI、OrderCreateUI 等 UI DLL 只上报入口/操作 ID，不直接创建或切换其他模块页面；MeyerScan.exe 通过单内容区容器一次只挂载一个全屏页面，首页和浏览不是并列兄弟页。MainExe 不绘制所有页面共享的可见标题栏；HomeUI、CaseUI 和 OrderScanWorkspaceShell 各自维护符合页面语义的顶部区域，窗口动作仍由 MainExe 执行。第三方拉起建单时，MainExe 直接显示 OrderScanWorkspaceShell/OrderCreateUI，客户视觉上看不到首页和自动进入创建模块的动作。
- **主页面切换不得闪现**：首页、浏览、设置、建单等常用页面不得反复 close/show 顶层窗口；由 MainExe 先替换内容区页面，再按资源规则释放离开的页面。轻量页面可按白名单短期复用，重资源页面离开后必须释放或暂停；确需过渡动画时后续集中放在 MainExe 或 UIComponents 实现。
- **加载订单规则独立**：OrderWorkflowService.dll 统一判断新建、加载、继续扫描、进入处理、进入发送，避免规则散落在 HomeUI/CaseUI/OrderCreateUI/ScanReconstructStudio
- **功能阉割闭环**：高价值功能必须通过“模块清单不加载 + Permission 拒绝 + Service/Workflow/IPC 复核”实现，不能只隐藏按钮
- **Qt UI 运行库版本一致**：HomeUI/CaseUI/SettingsUI 是正式 Qt Widgets UI 模块，输出目录由 PostBuild 复制 Qt 5.6.3 的 Qt5Core/Qt5Gui/Qt5Widgets、platforms/qwindows；QtSql/SQL 驱动仅在外部既有登录模块确需时保留，不作为 Database 依赖；不得混用文档附近 SQLiteStudio 目录中的 Qt 5.7 DLL
- **多屏/DPI 统一适配**：主 EXE 和测试宿主在创建 QApplication 前设置 High DPI 属性；UIComponents 后续统一提供 ScreenUtil/DpiUtil/LayoutRules；各 UI 模块只用布局和公共工具，不各自定义全局适配策略
- **多分辨率布局策略**：不继续把 1920x1080 绝对坐标等比缩放作为主方案；1920x1080 只作为设计稿和图标/间距参考。页面必须使用 Qt Layout、最小/最大宽度、伸缩策略、滚动区和文本自适应；UIComponents 统一提供缩放系数、图标尺寸、边距和字体层级规则。当前 OrderCreateUI、ScanWorkflowUI、DataProcessUI 的根最小尺寸已按 960x600 收敛，避免嵌入工作台后低分辨率下被固定大尺寸撑爆。
- **多语言布局策略**：禁止按语言写 if/else 手工调整控件位置和大小；按钮、标签、输入框必须给足最小宽度/弹性空间，必要时允许文本换行、tooltip 或布局重排；业务层只返回稳定 key，显示文案由 UI 翻译
- **多语言按模块维护**：采用 Common qm + 每个 UI 模块独立 qm；LanguageManager 统一加载、切换、回退；业务服务返回错误码/原因码/功能 ID，显示文字由 UI 翻译；UI 源码中所有可见文案统一写 `tr("English source text")`，不得写中文 source text，也不得用 `QApplication::translate()` 分散上下文
- **代码注释强制规则**：所有源码、头文件和测试项目的每个函数都必须有中文注释；启动顺序、资源释放、日志初始化、配置迁移、权限合并、数据库事务、UI 切换、IPC/跨 DLL 边界等关键代码必须补充“为什么这样做”的注释。默认阅读者按初学维护者考虑，注释要说明职责边界、生命周期和禁止事项。C++ 源码/头文件保存为 UTF-8 with BOM，避免 VS2015 误解析中文注释。
- **函数体内部注释强制规则**：函数头注释不等于注释完成。函数体内凡是涉及关键判断、路径推导、Qt 父子对象所有权、跨模块调用、失败分支、降级策略、线程/事务边界、资源释放和禁止事项，都必须在对应代码附近补中文注释；不能只写“调用函数”“设置变量”这类表面动作。
- **实现技巧注释强制规则**：注释不仅说明“功能是什么”，还必须解释“代码怎么实现”。涉及 Qt Layout/父子对象/信号槽/`deleteLater()`、`QLibrary` 动态加载、JSON 解析、`QByteArray::constData()` 生命周期、调用方缓冲区、C ABI、RAII 锁、Windows API、SQL 驱动、lambda 捕获、事件循环等待、测试造数据等实现技巧时，要在代码附近写清楚机制、风险和为什么采用这种写法。
- **接近逐行解释要求**：复杂函数内部应尽可能对每个关键语句或连续代码块补注释，让初学维护者能顺着注释读懂实现路径；简单赋值、直接 return、普通 include 不机械堆注释，但相邻注释要覆盖设计意图和技术机制。2026-07-02 起，核心 UI/服务链路和测试宿主还要优先解释 QSS/objectName、Qt Layout 伸缩、`QStackedWidget` 内部页与 MainExe 业务页替换的区别、懒加载 DLL、domain JSON 快照、旧表兼容、调用方缓冲区和 smoke 造数边界。
- **注释覆盖落地要求**：新增或修改模块时，公共接口头文件、实现文件、静态辅助函数、测试宿主和 smoke 入口都要同步补注释；第三方 SDK/外部头文件不强行改注释，自研适配层必须解释第三方调用方式和边界。
- **客户操作日志必记**：按钮点击、页面切换、页签切换、搜索、导入导出、删除、打开订单、登录状态、权限拒绝、服务调用失败等都必须写结构化日志；UI 模块记录模块内操作，MainExe 记录跨模块导航和页面切换
- **适配层日志规则**：`MyDatabaseQtAdapter` 等适配层可以直接使用 Logger。适配层日志重点记录输入非法、类型转换失败、连接失败、SQL 失败、JSON 解析失败、缓冲区超限、生命周期切换等边界事件；不要记录每条正常查询成功，避免日志文件被高频基础调用淹没。
- **运行路径基准**：日志、配置、资源、语言、版本清单等路径必须以 `QCoreApplication::applicationDirPath()` 或 ConfigCenter 提供的安装目录为基准，禁止用 `QDir::currentPath()` 作为运行资源路径；配置文件内如使用相对路径，必须明确相对哪个配置文件或安装目录解析，禁止静默回退开发机绝对路径
- **禁止运行期开发路径回退**：MeyerScan.exe、测试宿主和各 UI 模块运行时不得回退到 `D:/wj`、`F:/MeyerScan` 等开发机绝对路径；开发期依赖来源可写在 vcxproj/PostBuild 中，运行参数必须以应用目录或模块目录推导
- **共享 UI 组件模块**：UIComponents 负责通用按钮、字段标签、输入框、下拉框、日期框、多行文本框、表格样式、DPI/Layout 规则、公共弹窗和等待页；按钮样式按“角色 + 内容布局”统一管理，角色包括 Primary/Secondary/Text/Danger/Entry，内容布局包括纯文字/纯图标/左图右文/上图下文；弹窗类先归入 UIComponents，不单独拆 DLL，业务型弹窗只组合控件，不承载业务规则；控件工厂只统一尺寸、样式和多语言适配基础，不决定点击行为、不读取权限、不连接数据库；公共虚接口新增方法只能追加到接口末尾，不能插入旧方法中间
- **特殊 UI 控件归属**：多个 UI 模块都会使用的控件和样式进入 UIComponents；只在单个业务模块出现的截图还原控件、复杂设置页控件或单页面专用组合控件留在自身模块内部。是否公共化以“至少两个模块复用且不含业务语义”为准，避免 UIComponents 膨胀成页面库。
- **配置中心与权限模块先行**：ConfigCenter/Permission 可以在当前阶段先做轻量骨架，用于控制首页“设置”、浏览“返回首页”、数据库类型等流程开关；配置加解密接口预留，后续接入 Crypto
- **配置与权限共同收口显隐/启用态**：普通 UI 显隐先由 ConfigCenter 提供产品/客户配置默认值，再由 Permission 做授权过滤；两者同时允许才显示。`visible` 控制是否显示，`enabled` 控制是否可点击/执行；UI 模块只接收 MainExe 下发的最终状态，不直接读取配置或权限文件；MainExe、Workflow、Service、IPC 接收端仍要复核 `enabled`。
- **配置说明文件**：`runtime_config.json`、`permission_rules.json`、`version_modules.json` 内部不写注释；字段含义和使用场景写在同级 `.md` 文件中，例如 `runtime_config.md`、`permission_rules.md`、`version_modules.md`。
- **自研 DLL 动态加载规则**：MainExe 对 Logger、ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter 等自研功能/支撑模块优先使用 `QLibrary + extern "C" GetXxx()` 动态加载；主程序工程只保留接口头文件依赖，不再链接这些模块的 import lib。Qt 库、Windows `Version.lib`、当前既有登录模块 `MeyerLoginWidget.lib` 可暂时保持原链接方式；测试宿主或模块内部低层依赖是否链接 `.lib` 由边界和稳定性决定，不强求一刀切。
- **运行时版本清单**：当前阶段由 MainExe 内置生成，输出到 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`；MainExe 只读取 `config/version_modules.json` 中声明的拆分模块 EXE/DLL，不再扫描 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT 等第三方库。`version_modules.json` 当前使用 schemaVersion=2，模块项包含 `file` 和可选 `versionFunction`；自研 DLL/EXE 固定导出 `GetMeyerModuleVersion()`，运行时版本清单同时记录 `fileVersion`、`codeVersion`、`versionMatch` 和 `codeVersionError`。文件版本来自 `Version.rc`，代码版本来自 `ModuleInfo::Version` / `GetMeyerModuleVersion()`，二者必须同步维护。文件名包含毫秒，避免 smoke、第三方拉起或重复启动在同一秒内覆盖版本快照。`MyVersionManager` 暂保留为历史骨架，但也必须读取同一个 `version_modules.json`，不得恢复目录全量扫描。后续算法 DLL、哈希/签名、安装打包、自动更新和云端版本比对变复杂后再恢复为独立模块。
- **运行时清单与打包清单分离**：`logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json` 是客户机器启动现场快照，只记录 `version_modules.json` 中声明的 MeyerScan 拆分模块 EXE/DLL；`version_manifest.json` 是 MyInstaller/Packaging 在正式构建/打包时生成的发布清单，可记录 Qt 运行库、平台插件、SQL 驱动、VC/UCRT 等安装依赖。二者用途不同，不允许用运行目录全量扫描替代 `version_modules.json`。
- **第三方运行库复制路径**：VS2015 PostBuild 不得写个人开发机绝对路径（如 `D:\wj\My-wj\...`）；当前 SQLite 运行时统一从 `F:\MeyerScan\ThirdParty\SQLite\win-x64\sqlite3.dll` 复制到各模块 `bin\Release` 和根聚合 `bin\Release`，该 DLL 必须是 x64。禁止再从旧 `MyCaseManager\SQLite\sqlitestudio311\SQLiteStudio` 目录复制 `sqlite3.dll`，因为旧目录中的 DLL 是 32 位，会导致 x64 程序加载失败。第三方 DLL 本体不提交 GitHub/本地仓库，只提交同目录 README 说明；后续打包模块再把它纳入安装清单。Qt、登录模块等外部既有依赖如果暂时必须使用固定安装路径，需在对应模块 README/CHANGELOG 标明来源和后续收口计划。
- **单模块与聚合输出一致**：同时支持打开单模块 `.sln` 和根 `MeyerScan_AllModules.sln`。凡是运行时需要 Database/DatabaseQtAdapter 的模块，单模块输出目录和根聚合输出目录都必须复制同一套 `MeyerScan_Database.dll`、`MeyerScan_DatabaseQtAdapter.dll` 和 x64 `sqlite3.dll`；冒烟测试必须覆盖两类目录，避免“聚合能跑、单模块失败”。
- **启动等待页**：启动检查、数据库检查、资源存在性检查等由 MainExe 编排，等待界面由 UIComponents 提供；等待页只展示状态，不承载检查逻辑
- **单实例规则**：MeyerScan.exe 同一时间只允许一个实例；重复双击时通知已运行实例激活窗口；如果旧实例仍处于数据库检查或登录模块阶段，可忽略激活，不打断启动/登录流程
- **资源释放策略**：不显示模块是否析构按资源重量分级处理。当前 MainExe 对首页、浏览、等待页采用“单内容区替换 + 离开页面释放”的方式；UI 模块只借用进程级 Logger/Database，不在自身 `Shutdown()` 中关闭全局基础设施。后续轻量页面可按白名单缓存以保证切换顺滑，重资源模块（扫描重建、3D、算法、相机、显存、点云/网格缓存）离开时必须释放或进入可控暂停态，把内存/显存留给扫描重建；从案例管理进入扫描重建前，MainExe 必须先切到等待页并释放 CaseUI widget，不能只 hide 浏览界面。当前工作台 Scan/Process 切换时由 MainExe 调用对应 UI 的 `Shutdown()` / `DeactivateAndRelease()` 并用占位页替换旧步骤，避免壳子继续持有待删除 QWidget。
- **运行资源目录**：登录离线许可文件放在 `Resources/license.lic`；UI 图标、图片、mask、QSS、qm 正式发布时编译进 `MeyerScan_UIResources.dll`，源码仍归各模块 `Resources`。资源 DLL、许可、配置、日志和其它运行文件都必须基于 `QCoreApplication::applicationDirPath()` 推导，禁止依赖 `QDir::currentPath()`。
- **VS2015 聚合解决方案**：`F:\MeyerScan\MeyerScan_AllModules.sln` 用于一次打开当前已拆分模块工程；外部已经开发完成的登录 DLL 不纳入该聚合方案。
- **模块信息规则**：每个模块应维护 `ModuleInfo` 命名空间/结构体/类，至少包含 `Name` 和 `Version`；`Name` 用于日志 `[Mod:]` 字段并与 `MEYER_MODULE_NAME` 保持一致；`Version.rc` 是 Windows 文件属性和安装包核对的权威来源，代码中的 `ModuleInfo::Version`、业务接口 `GetModuleVersion()` 和统一导出函数 `GetMeyerModuleVersion()` 是运行时代码版本来源，三者必须同步，不允许长期不一致。DLL 文件“详细信息”页没有版本号时，优先检查是否把 `src/Version.rc` 编进 `.vcxproj` / CMake 目标，而不是只改代码版本函数。
- **日志字段分类标记**：日志固定字段使用 `[Mod:模块] [Op:操作] [Content:内容]`，可选上下文字段使用 `[Dev:] [Case:] [Opr:]`；字段为空时不输出占位，禁止恢复 `[Dev:-]`、`[Case:-]` 或 `[INFO ]` 固定宽度格式。
- **建单与扫描工作台一致性**：`OrderScanWorkspaceShell` 是创建/练习场景唯一的工作台顶部与步骤导航所有者；OrderCreateUI、ScanWorkflowUI、DataProcessUI、SendUI 只提供步骤内容。ScanReconstructStudio DLL 嵌入时必须隐藏自身重复导航，独立 EXE 模式才绘制独立窗口顶部区域。
- **扫描重建数据处理分层**：当前初版已落地 `ScanReconstructStudio.exe` 壳、`MeyerScan_ScanWorkflowUI.dll` 扫描阶段 UI 和 `MeyerScan_DataProcessUI.dll` 数据处理阶段 UI。EXE 只负责独立进程、阶段导航、动态加载、页面挂载、资源释放和后续 IPC 编排；两个 UI DLL 只负责各自阶段界面、QVTK 显示占位和动作上报。编辑、颈缘、测量、倒凹、咬合、底座、数据预处理、数据 IO 等可复用或高变化的数据处理能力应继续拆成 DLL 或清晰独立库，界面/交互层只调用处理接口，不直接保存业务状态、不直接操作病例数据库。
- **扫描第三方依赖迁移规则**：扫描相关 Qt/VTK/OpenCV 路径统一写在 `cmake/MeyerScanScanThirdParty.props` 和 `cmake/MeyerScanScanThirdParty.cmake`，模块项目不得散落个人开发机路径。迁移到其他电脑时优先配置 `QT_ROOT`、`VTK_ROOT`、`VTK_HEADERS_ROOT`、`OPENCV_ROOT`，再使用仓库 `ThirdParty` 或当前开发机参考路径。MainExe 输出目录需要复制扫描 UI 所需的 VTK/OpenCV 运行库，保证版本扫描和后续打开扫描工作区能正常加载自研扫描 UI DLL；这些第三方 DLL 只是运行依赖，不进入运行时 `versionList`。
- **自动更新独立进程**：MyUpdate.exe 与 MeyerScan.exe 同级目录存放；MeyerScan.exe 可通过“检查更新”按钮生成本地更新信息并启动 MyUpdate.exe，MyUpdate.exe 也可被用户双击主动运行
- **安装打包独立模块**：安装包生成、自定义安装界面、自定义安装流程和安装目录层级暂归入 MyInstaller/Packaging 模块；该模块只负责发布交付，不承载运行时业务逻辑，后续实现时再详细设计技术选型和脚本结构
- **平台**：Windows 7/10/11 x64

#### CRT 链接规则（新增 v1.1.0）

> 由于 Qt 5.6.3 MSVC2015 预编译库使用动态 CRT（`/MD`），
> 而 Logger 作为"最先加载模块"需要零依赖，使用静态 CRT（`/MT`），
> 因此不同模块使用不同的 CRT 链接方式。具体规则如下：

| 模块类型 | CRT 链接 | Debug | Release | 典型模块 |
|---------|----------|-------|---------|---------|
| **无 Qt 依赖的基础模块** | 静态 CRT | `/MTd` | `/MT` | Logger.dll、Database.dll |
| **有 Qt 依赖的业务/UI/适配模块** | 动态 CRT | `/MDd` | `/MD` | DatabaseQtAdapter.dll、CaseOrderService.dll、ConfigCenter.dll、UI DLL 等 |
| **测试程序** | 与所测模块一致 | — | — | DatabaseTest.exe → `/MTd`/`/MT`；Qt UI 测试宿主 → `/MDd`/`/MD` |

**关键原则**：

1. **跨 CRT/第三方/长期 ABI 边界不传递所有权**：对不确定调用方、第三方插件或需要长期二进制兼容的接口，统一通过纯虚接口 + UTF-8/POD/调用方缓冲区通信，不传递需要另一侧释放的 `std::string`、`QString`、Qt 容器或 QObject 所有权；同一进程、同一 Qt/VS2015 环境内的 MeyerScan Qt 模块可以使用 Qt 类型提高开发效率
2. **Import Library 链接无害**：链接 DLL 的 `.lib`（导入库）不会引入 CRT 对象代码，不同 CRT 的 DLL 之间可以安全链接导入库
3. **静态库需 CRT 一致**：后续如确需新增静态契约库，其 CRT 设置必须与消费方一致
4. **强迫症方案（运行时加载）**：当调用方和被调用方 CRT 不同、且被调用方是 DLL 时，可使用 `LoadLibrary` + `GetProcAddress` 在运行时动态加载，彻底避免 CRT 冲突（如 Database.dll 加载 Logger.dll）

#### 工程设置调整说明（v1.1.0）

- **EngineeringSettings.dll 职责优化**：将"数据库类型切换"移入 ConfigCenter/病例域数据策略，由 CaseOrderService 读取最终数据源策略并通过 Database 执行；"功能开关控制"移入 Permission（本质为权限管控）。EngineeringSettings 保留：设备参数设置、下位机固件升级、扫描参数设置、数据处理参数设置。
- **Permission.dll 内部优化**：冷热路径分离——`CheckAccess`（热路径，纯内存判断）与 `LoadRuleFile`/`UpdateAuthByQRCode`（冷路径，涉及文件 IO），内部数据结构做隔离设计。

### 5.3 构建约束

- **每个模块必须同时支持 VSCode 和 VS2015 直接编译**
- **VS2015**：双击 `.sln` → F7 即可编译
- **VSCode**：优先使用模块或根目录 `CMakeLists.txt` 配合 CMake Tools / VS2015 生成器编译；已有模块也可保留 `Ctrl+Shift+B` 调用 MSBuild 的方式。每个模块和测试项目都必须有 `CMakeLists.txt`，便于移植到其他电脑后不依赖手工打开单个 `.vcxproj`。
- **CMake 双入口规则**：`F:\MeyerScan\CMakeLists.txt` 是聚合入口，可一次加载当前已拆分模块；每个模块目录也必须有独立 `CMakeLists.txt`，能被 VSCode 单独打开。测试宿主必须在同一模块 CMake 中声明，例如 `LoggerTest.exe`、`DatabaseTest.exe`、`CaseUITest.exe`。
- **当前 CMake 环境**：2026-07-06 已安装 CMake 3.31.6，位置为 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe`，并已写入用户 PATH。已用 `Visual Studio 14 2015 Win64` 生成器完成根聚合工程 `Release` 配置和构建验证；后续新模块必须保持该入口可配置、可构建。
- **VS2015 根方案测试规则**：`F:\MeyerScan\MeyerScan_AllModules.sln` 必须纳入当前活跃自研模块和对应测试宿主，便于一次构建发现跨模块依赖、PostBuild 复制和输出目录问题。DLL 模块优先提供独立 `*Test.exe`；主 EXE 使用 `MeyerScan.exe --smoke-main` 作为最小自动验证入口；外部既有登录 DLL 的交互测试宿主独立维护，不强制进入根方案自动测试。
- **CRT 链接规则**：无 Qt 依赖的基础模块（如 Logger、Database）用 `/MT` 静态链接 CRT；有 Qt 依赖的模块（如 DatabaseQtAdapter、CaseOrderService、UI DLL 等）用 `/MD` 动态链接 CRT。详见上方 §5.2 CRT 链接规则表。
- **C++ 标准**：C++14
- **字符集**：Unicode
- **源码编码**：UTF-8 with BOM（VS2015 RTM 不支持 `/utf-8` 编译选项）
- **测试程序 CRT**：必须与所测模块保持一致
- **版本资源文件**：每个 EXE/DLL 必须在项目中包含 `Version.rc` 文件，嵌入 Windows 版本信息（`VS_VERSION_INFO`），使得文件属性对话框显示正确的模块名、版本号、公司名等
- **版本号一致性**：`Version.rc` 中的 `FILEVERSION`/`PRODUCTVERSION` 必须与 `ModuleInfo::Version`、`GetModuleVersion()` 和 `GetMeyerModuleVersion()` 返回的版本号一致（参见架构文档 §9.6 版本资源规范）
- **版本资源字段统一**：`CompanyName` 固定为 `Hefei Meyer Optoelectronic Technology Co., Ltd.`；`ProductName` 固定为 `MeyerScan Digital Dental Scanner`；DLL 的 `FileDescription` 使用 `MeyerScan <ModuleName> Module`；EXE 使用明确的 Executable/Test Host 描述；`FILEOS`、`FILETYPE`、Debug `FILEFLAGS` 使用 Windows SDK 命名常量，不写裸十六进制值
- **模块名宏必填**：每个 EXE/DLL 的 `.vcxproj` / `CMakeLists.txt` 必须定义 `MEYER_MODULE_NAME="MeyerScan_<ModuleName>"`，测试宿主也必须定义自己的模块名，保证 Logger 便捷宏输出可追溯模块名

### 5.4 版本清单规范（新增 v1.1.0）

- 每次正式构建/打包时，必须生成一份 **模块版本清单**，列出所有 EXE/DLL 的版本号
- 版本清单格式为 JSON 或 CSV，包含：模块名、文件版本、产品版本、构建日期、备注（可选）
- 版本清单用于：① 发布前核对模块版本一致性 ② 现场排查版本不匹配问题 ③ 追溯历史版本
- 注意区分两类清单：正式打包的 `version_manifest.json` 可覆盖 Qt/VTK/OpenCV/VC/SQL 驱动等发布依赖；MeyerScan.exe 启动生成的 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json` 只记录 `config/version_modules.json` 声明的自研拆分模块，并同时记录 `fileVersion`、`codeVersion`、`versionMatch` 和必要的读取错误信息。
- 版本清单示例（JSON）：
  ```json
  {
    "product": "MeyerScan Digital Dental Scanner",
    "buildDate": "2026-06-17",
    "modules": [
      {"name": "MeyerScan_Logger.dll",         "version": "1.0.0", "type": "DLL"},
      {"name": "MeyerScan_Database.dll",        "version": "1.3.0", "type": "DLL"},
      {"name": "MeyerScan_DatabaseQtAdapter.dll","version": "0.1.0", "type": "DLL"},
      {"name": "MeyerScan_CaseOrderService.dll","version": "0.2.0", "type": "DLL"},
      {"name": "MeyerScan_OrderWorkflowService.dll","version": "1.0.0", "type": "DLL"},
      {"name": "MeyerScan.exe",                 "version": "1.0.0", "type": "EXE"}
    ]
  }
  ```
- 打包发布清单文件命名为 `version_manifest.json`，存放于安装包根目录或 `Docs/` 目录；它不同于 MainExe 启动时写入 `logs/versionList/` 的运行时模块快照。

### 5.4 GitHub 与本地仓库规范

- **仓库地址**：`https://github.com/weijian118/MeyerScan.git`
- **本地 Git 根目录**：`F:\MeyerScan`
- **本地备份仓库**：`F:\MeyerScan-Reposit`。除 Qt、VTK、OpenCV、PCL、VC/UCRT、OpenSSL、AWS、SQL 驱动等第三方库外，所有自研源码、测试项目、`.sln`、`.vcxproj`、`CMakeLists.txt`、配置模板、README、CHANGELOG、自研 DLL/EXE/LIB 等都要备份到该本地仓库。
- **本地仓库提交规则**：每次本地备份必须以所有模块为一个整体提交，不能只备份单个模块。备份提交日志必须使用中文，并尽量详细说明本次涉及的模块、工程文件、文档和验证结果，确保后续能按本地仓库恢复完整工程状态。
- **第三方依赖处理**：Qt、VTK、OpenCV、PCL、VC/UCRT、OpenSSL、AWS、SQL 驱动等第三方运行库不进入本地备份仓库；它们由安装包/依赖说明/开发环境配置负责。外部已开发但不在本仓库维护源码的模块，应按“是否自研且是否属于当前发布依赖”单独登记，避免和第三方库混在一起。
- **本地备份脚本**：统一使用 `F:\MeyerScan\tools\BackupToLocalRepository.ps1` 生成 `F:\MeyerScan-Reposit` 快照；调用时必须传入中文提交日志，例如 `-CommitMessage "本地完整备份：说明本次变更、影响模块和验证结果"`。脚本使用 `robocopy /MIR`，会排除 `.git/.vs/obj/build/logs/plugins/MySQL/SQLite/backup/CMakeFiles` 以及 Qt、VC/UCRT、OpenSSL、AWS、MySQL/SQLiteStudio、SQL 驱动等第三方运行文件。由于 `robocopy /MIR` 不会删除目标仓库中已被 `/XD`、`/XF` 排除的历史遗留内容，脚本必须在镜像后执行带目标路径边界校验的主动清理；脚本必须保存为“带 BOM 的 UTF-8”，确保 Windows PowerShell 5.1 正确解析中文注释和后续代码行。
- **重构文档快照**：本地仓库额外保存 `D:\wj\重构文档` 的 Markdown 快照到 `_RefactorDocs`，只同步 `.md` 文件，不同步 API token、临时 txt、图片、docx 提取缓存等非 Markdown 文件。
- **仓库结构**：`MeyerScan/ModuleName/`（每个模块独立一级目录）
- **模块目录规则**：日志、数据库、首页、案例管理等模块都必须和 `MyLogger/`、`MyDatabase/` 同级存放和提交；当前已采用 `MyHomeUI/`、`MyCaseUI/` 作为首页和案例管理模块目录
- **GitHub 提交范围规则**：提交 GitHub 时加入本次变更涉及的模块目录和必要根级文件，不得把临时输出、压缩包、无关实验目录混入；但涉及跨模块接口、CMake、主链路或文档规则时，应一次提交相关模块，避免 GitHub 上出现新旧方案混杂。
- **分支策略**：`master` 主分支，开发分支按模块名创建
- **提交规范**：GitHub 提交信息统一使用中文，必须明确描述变更内容和影响范围；避免只写“更新”“修复”“修改”这类空泛信息
- **模块内记录**：每个模块目录必须维护中文 `CHANGELOG.md`，按时间记录本模块每次修改内容、验证结果、注意事项；已完成模块也要补齐
- **注释与记录语言**：源码注释使用中文；模块 `CHANGELOG.md` 使用中文；GitHub commit message 使用中文。日志字段 key、内部错误 key、翻译 key 可保持稳定英文，便于程序处理和国际化。界面可见文字用 `tr("English source text")`，例如需求说“回到首页”按钮，源码仍写 `tr("Back Home")`，中文显示交给 `.qm`。

### 5.5 轻量化团队开发节奏

| 开发方式 | 主要内容 | 注意事项 |
|------|----------|----------|
| **分模块开发** | 按 Logger、Database、Config、Permission、病例域服务、UI、设备、扫描进程、更新程序等模块拆分任务 | 每个模块有清晰边界、公开接口、README、模块内变更记录和最小验证入口 |
| **模块内测试** | 每个 DLL/EXE 先完成本模块编译、测试宿主或 smoke 验证 | 不等待整机联调才发现基础问题 |
| **统一集成/联调** | 主 EXE 统一加载插件，按主链路做登录/权限、首页、建单、病例、扫描、上传、更新联调 | 联调只验证模块协作，不在联调阶段临时补业务边界 |
| **统一规范维护** | 接口命名、错误码、日志、版本资源、Git 提交、运行库复制、文档记录保持一致 | 防止每个模块形成自己的小规范 |

**碎片化开发规则**：

1. 每次任务只改一个模块或一条调用链，避免跨 5 个模块同时半成品。
2. 每个模块必须有测试宿主或最小验证程序；没有测试入口的模块不进入联调。
3. 修改测试宿主源码后，必须单独构建对应测试 `.vcxproj` 并运行测试；当前活跃自研测试项目也必须纳入 `MeyerScan_AllModules.sln`，用于根输出目录的批量构建和回归验证。主 EXE 通过 `--smoke-main` 作为测试入口，外部既有登录测试宿主独立维护。
4. GitHub 提交控制在可 review 范围内；本地仓库备份必须每次整体备份所有模块，保证恢复时不会缺少跨模块依赖。
5. 文档、接口、Version.rc、README 和 smoke 结果与代码同次更新。
6. 每周至少做一次“可运行主链路”验证：启动 → 登录/权限 → 首页 → 病例/订单 → 扫描进程入口。

**新模块工程验收清单**：

| 检查项 | 要求 |
|--------|------|
| 模块名宏 | `.vcxproj` / `CMakeLists.txt` 已定义 `MEYER_MODULE_NAME`，日志不会退化为 `Unknown` |
| 版本资源 | `Version.rc` 公司名、产品名、文件描述、命名常量、版本号均符合架构规范 |
| CMake 工程 | 模块目录和测试宿主已写入 `CMakeLists.txt`，根聚合 CMake 能引用该模块；VSCode 可通过 CMake 打开 |
| 文档记录 | 模块目录内有中文 `README.md` 和中文 `CHANGELOG.md`，记录职责边界、运行方式和每次修改 |
| 测试入口 | DLL 有测试宿主、`--smoke` 或最小验证入口；没有测试入口不进入联调；测试宿主源码变更后必须单独构建并运行对应测试；活跃自研测试项目必须进入根 `MeyerScan_AllModules.sln` |
| 注释与编码 | 自研源码/测试代码有函数级和关键路径中文注释，C++ 文件保存为 UTF-8 with BOM |
| 路径规则 | 运行期路径基于 exe 所在目录或配置中心，不回退 `D:/wj`、`F:/MeyerScan` 等开发机绝对路径 |
| 接口债务 | 若骨架阶段暂用 `bool` / 临时 Result，必须在头文件写明 TODO 和后续统一结果合同的迁移目标；是否抽公共库由真实复用决定 |

### 5.5.1 当前 MainExe 集成落地状态（2026-06-22）

已在 `F:\MeyerScan` 下新增两个一级模块目录：

| 模块目录 | 输出 | 当前定位 | 验证结果 |
|----------|------|----------|----------|
| `MyLogin/` | `MeyerLoginTest.exe` | 既有 `MeyerLoginWidget.dll` 的测试宿主，只验证登录模块可独立拉起和依赖完整 | VS2015 Release x64 构建通过；`MeyerLoginTest.exe --smoke` 返回 0 |
| `MyMainExe/` | `MeyerScan.exe` | 新架构主入口壳，负责日志初始化、数据库健康检查、登录、首页/案例 UI 编排 | VS2015 Release x64 构建通过；`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main` 返回 0 |

当前 MainExe 最小启动链路为：

```text
QApplication
  → Logger 初始化
  → Database 读取 config/db_config.json 并做健康检查
  → MeyerLoginWidget.dll 登录界面
  → 登录成功后加载 HomeUI
  → HomeUI 上报“浏览”入口 ID
  → MainExe 用单内容区替换为 CaseUI 全屏页面
  → CaseUI 上报“返回首页”操作 ID
  → MainExe 切回 HomeUI
  → CaseUI 上报“Open”操作 ID
  → MainExe 切到等待页并释放 CaseUI，预留启动 ScanReconstructStudio.exe
```

边界要求：

1. MainExe 只做启动、模块编排和窗口容器，不写业务 SQL、不写订单规则、不写扫描算法。
2. MainExe 通过 DatabaseQtAdapter 调用 Database 仅限启动健康检查；正式患者、订单、医生/诊所/技工所、扫描方案读写必须走 RuntimeDataCenter / CaseOrderService / ScanSchemaService / OrderWorkflowService。
3. HomeUI 通过 `SetEntryCallback()` 暴露入口点击事件；CaseUI 通过 `SetActionCallback()` 暴露浏览模块操作事件；MainExe 统一处理跨模块页面切换。
4. 主页面切换由 MainExe 单内容区集中完成；首页和浏览不是并列兄弟页。当前 MainExe 已按需创建首页、浏览页和等待页，每次只挂载一个全屏页面，并在切换完成后释放离开的页面 widget，避免不可见页面长期占用资源。
5. 后续打开 `ScanReconstructStudio.exe` 前必须先执行扫描前资源准备：切换到等待页、释放 CaseUI widget、处理 Qt 延迟删除事件，再启动扫描重建进程；案例管理页不能只隐藏不释放。
6. 当前 `CaseActionOpenOrder` 已接入扫描前准备流程，先保证资源释放和日志链路正确，后续再接入加载订单规则与扫描重建进程启动。
7. `--smoke-main` 必须覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前资源释放链路，不能只验证启动成功。
8. 登录模块当前直接使用既有 `MeyerLoginWidget.dll` 和外部头文件；因头文件存在 VS2015 代码页/声明警告，后续建议新增 `LoginAdapter` 收口参数构造、状态转换和头文件兼容风险。
9. Release 输出目录已补齐登录模块间接依赖，包括 Qt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi、平台插件和 SQL 驱动；后续 MyInstaller/Packaging 应以该清单为安装包依赖收集参考。

**复测记录（2026-06-22）**：

- `MeyerScan_MainExe.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan_MyLogin.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerLoginTest.exe --smoke` 均返回 0。
- 2026-06-22 追加验证：HomeUI 浏览入口回调、CaseUI 返回首页回调、页面切换日志和客户操作日志接入后，`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`CaseUITest.exe --smoke` 均返回 0。
- 2026-06-23 追加集成：ConfigCenter、Permission、UIComponents 接入 MainExe；MainExe 启动时生成运行时版本清单，使用等待页，执行单实例检查，并按需创建/释放首页、浏览页、等待页。2026-06-24 已将轻量版本清单生成逻辑并入 MainExe，`MyVersionManager` 暂保留历史骨架。
- 2026-07-03 再次复查：Database 已纯 C++ 化，配置路径仍按 `db_config.json` 所在目录解析；Qt 调用方统一经 DatabaseQtAdapter；HomeUI/CaseUI 不再借用 Database 做健康检查；UIComponents 继续保持纯 UI 边界。
- 2026-06-23 资源约束补充：后续从案例管理打开扫描重建前，必须释放 CaseUI widget 和其可释放缓存资源，不能仅隐藏浏览模块。
- 2026-06-23 交互复查：CaseUI `Open` 操作已接入 MainExe 扫描前准备流程；`--smoke-main` 自动覆盖首页/浏览双向切换、CaseUI 释放和扫描前等待页链路；单实例只在登录完成且主窗口可见后激活旧窗口。
- 递归依赖检查结果：非 API Set DLL 缺失为空，当前 MainExe Release 目录可独立装载。
- 架构复核结论：当前 MainExe 仍符合“薄主 EXE”要求；后续优先补 `LoginAdapter` 和配置驱动，不把登录参数、业务规则或权限判断继续写入 MainExe。

### 5.6 轻量化团队维护策略

轻量化团队最怕的不是模块数量本身，而是“谁也不敢改、改了不好测、出了问题找不到入口”。因此后续开发按 **小模块 + 少接口 + 可独立运行 + 文档同步** 的方式推进。

| 维护问题 | 规则 | 目的 |
|----------|------|------|
| 模块太大没人敢改 | 按变化原因拆分，单模块只承担一类职责 | 修改时能快速定位影响范围 |
| 模块太碎调试困难 | 只有独立升级、替换交付、并行开发价值的功能才 DLL 化 | 控制 DLL 数量和部署复杂度 |
| 接口太多读不懂 | 每个模块先设计最小接口，新增接口必须说明调用场景 | 降低调用方理解成本 |
| UI 容易塞业务 | UI 只展示和收集输入，保存、加载、跳转都调用 Service/Workflow | 避免后期规则散落 |
| 公共层容易膨胀 | 公共头/静态库按需抽取，只放稳定契约和无状态工具，禁止放业务规则 | 防止公共模块变成隐形业务层 |
| 出问题不好查 | 每个模块有日志、版本号、README、测试宿主或 smoke 验证 | 降低现场和开发调试成本 |
| 碎片化开发易断链 | 每次只改一个模块或一条调用链，完成后跑最小验证 | 避免多个半成品互相阻塞 |

**模块代码量建议**：

- 单个主要 `.cpp` 文件尽量低于 800 行；超过后优先按类或流程拆文件。
- 单个业务 DLL 的核心源码尽量控制在 3000-5000 行内；超过后复盘职责是否过宽。
- 对外接口尽量控制在 10-20 个核心方法内；大量细碎 getter/setter 说明抽象可能不对。
- UI 页面按页面/面板拆类，但不必每个小控件都单独 DLL 化。
- ScanReconstructStudio.exe 只做高风险独立进程和工作台编排；当前扫描阶段 UI 与数据处理阶段 UI 已先拆成两个 DLL。后续真实扫描采集、设备连接、算法重建、编辑、处理、数据 IO、预处理等业务/算法能力若存在复用、频繁变化或独立测试价值，应继续拆为 DLL 或独立库，避免 UI/交互和数据处理混在一个大工程里。

---

## 六、开发顺序

开发顺序遵循三条硬规则：

1. **先最小合同后实现**：先定义当前业务链路需要的 C ABI、动作 ID 和版本化 JSON，再写服务和 UI；不预建通用 Core/Entity 大包。
2. **先主链路后支线**：先跑通创建/加载订单进入扫描，再做统计、批量导入导出、复杂设置。
3. **先可验证后扩展**：每个阶段必须保留可运行验证点，避免多个模块同时失控。

### 第一阶段：底座搭建

| 序号 | 任务 | 模块 | 说明 |
|------|------|------|------|
| 1.1 | 搭建项目框架 | MeyerScan.exe | 创建项目文件层级、插件加载器、进程调度壳 |
| 1.2 | 开发日志模块 | MeyerScan_Logger.dll | ✅ 已完成 |
| 1.3 | 定义病例/订单数据合同 | CaseOrderService DTO + UTF-8 JSON | 使用 schemaVersion、稳定字段 key、extensions 和数据库迁移映射；当前不单独开发 CaseEntity.lib |
| 1.4 | 开发数据库模块 | MeyerScan_Database.dll | 数据库连接管理、SQL 执行引擎 |
| 1.5 | 开发病例订单服务 | MeyerScan_CaseOrderService.dll | 患者/订单组合数据 CRUD、医生/诊所/技工所等参考数据、字段版本和 schema 映射；🟡 v0.2.0 骨架已完成 |
| 1.6 | 预留/取消独立订单服务 | — | 患者与订单强绑定，不再单独开发 OrderService.dll；相关能力归 CaseOrderService |
| 1.7 | 开发扫描方案服务 | MeyerScan_ScanSchemaService.dll | 牙位/修复体/材料/齿色方案管理 |
| 1.8 | 开发订单流程规则服务 | MeyerScan_OrderWorkflowService.dll | 新建/加载/继续扫描/发送前规则决策 |
| 1.9 | 规划自动更新上下文 | MyUpdate.exe 配置契约 | 定义 myLocalUpdate.json/xml、MyCloudUpdate.json/xml 字段和版本策略 |
| 1.10 | 规划安装打包上下文 | MyInstaller/Packaging 发布契约 | 定义安装包输入清单、安装目录层级、安装过程自定义界面和自定义流程边界 |

**第一阶段关键注意事项**：

- 不急于做完整 UI，先保证基础设施和病例域服务能独立编译、独立测试。
- 不把 Core.lib/CaseEntity.lib 作为底座完成条件；先让 CaseOrderService、ScanSchemaService 和 OrderWorkflowService 的真实合同稳定下来，再评估是否有必要抽公共契约。
- 患者、订单字段经常增删，必须使用 `schemaVersion`、`extensions`、字段映射和数据库迁移策略；跨模块默认传 UTF-8 JSON 或订单 ID，不暴露高频变化固定结构体 ABI。
- Database 保持纯 C++ 基础设施，不懂病例、订单、权限语义；Qt 类型转换只放在 DatabaseQtAdapter。
- 数据库相关主链路不能停留在“Database 可连接”：还需要 ConfigCenter 提供连接配置、版本化 schema/migration、CaseOrderService/ScanSchemaService 内部 DAO、OrderWorkflowService 读取服务结果后决策。
- OrderWorkflowService 要尽早建立，因为它是防止流程规则散落到 UI 和扫描进程的关键。
- 阶段结束标准：Logger/Database/病例域服务具备测试入口，主 EXE 能加载基础插件，建单数据合同可版本化演进。

### 第二阶段：核心支撑 + 案例 UI

| 序号 | 任务 | 模块 | 说明 |
|------|------|------|------|
| 2.1 | 开发配置中心 | MeyerScan_ConfigCenter.dll | 全局配置读写 |
| 2.2 | 开发权限模块 | MeyerScan_Permission.dll | 六维权限校验 |
| 2.3 | 开发共享 UI 组件 | MeyerScan_UIComponents.dll | 🟡 v0.4.0 已支持标准按钮角色/内容布局、等待页、标题、字段标签、输入框、下拉框、日期框、多行文本框和基础表格；后续扩展公共弹窗、复杂表格、主题和 LanguageManager |
| 2.3.1 | 开发统一 UI 资源 | MeyerScan_UIResources.dll | 🟢 v0.1.3：自动聚合各 UI 模块 PNG/QSS/qm，正式发布只交付资源 DLL；当前包含建单五种修复类型 b/h、1x/2x 图标和修正后的 hover QSS；测试宿主验证注册、读取和注销生命周期 |
| 2.4 | 开发权限配置 UI | MeyerScan_PermissionConfigUI.dll | 授权状态展示、扫码授权入口 |
| 2.5 | 开发案例管理 UI | MeyerScan_CaseUI.dll | 🟡 v0.3.1：Orders/Patients 页面导航、筛选工具栏、响应式订单卡片流及完整顶部动作；正式 UI 只读 RuntimeDataCenter，不直连 Database |
| 2.6 | 开发建单界面 | MeyerScan_OrderCreateUI.dll | 🟡 v0.5.2：提供单页三栏 Order 内容，包含基本信息、五种修复类型、治疗方案图片/mask、扫描流程输入、订单明细和动作回调；修正牙位/mask、`1/3/4/5/7` 叠加图、桥连接点、hover 图标和种植体引发布局缩放问题；通用控件语义接入 UIComponents，只读资源接入 UIResources；支持 1366x768/1920x1080/2560x1440 截图验收；不绘制重复步骤条 |
| 2.7 | 开发首页 UI | MeyerScan_HomeUI.dll | 🟡 v0.3.1：四入口、全屏参考背景、中英品牌、页面语义顶部和权限状态；只做入口 UI，不直连 Database |

**第二阶段关键注意事项**：

- UI 模块只做页面展示、输入收集、入口状态渲染，不做保存规则和加载订单规则。
- Permission 的热路径必须轻：启动后内存判断，扫码授权、规则文件更新等冷路径隔离。
- UIComponents 只能放通用控件和样式，不放业务按钮行为；按钮统一按“角色 + 内容布局”管理，字段标签、输入框、下拉框、日期框、多行文本框、基础表格等可统一进入 UIComponents；表格只统一外观和默认交互，不决定列含义、数据来源、分页、排序、右键菜单或双击动作；业务模块只传入 `tr("English source text")` 后的文案、图标路径和后续 clicked 行为。
- 单模块专用控件不强制抽到 UIComponents；只有两个以上 UI 模块复用、且不含业务语义的控件/样式才进入共享模块。
- HomeUI 和 CaseUI 已有框架，应优先接 Permission/Workflow/Service，而不是先做复杂视觉细节。
- 阶段结束标准：创建订单、浏览订单、加载订单、练习扫描入口能形成最小业务闭环。

### 第三阶段：已有模块集成

| 序号 | 任务 | 模块 | 说明 |
|------|------|------|------|
| 3.1 | 集成文件加解密 | MeyerScan_Crypto.dll | 已开发完毕 |
| 3.2 | 集成云端服务 | MeyerScan_NetworkHelper.dll | 已开发完毕 |
| 3.3 | 集成登录模块 | MeyerScan_Login.dll | 已开发完毕 |
| 3.4 | 集成扫码验证 | MeyerScan_QRCodeAuthEntry.dll | 已开发完毕 |
| 3.5 | 开发数据导入导出 | MeyerScan_DataExport.dll | 患者/订单/扫描数据导入导出、打包 |
| 3.6 | 开发统计服务 | MeyerScan_Statistics.dll | 患者/订单/设备使用统计 |
| 3.7 | 开发第三方拉起适配 | MeyerScan_ExternalLaunchAdapter.dll | 第三方软件传入患者/订单信息并自动建单 |
| 3.8 | 开发 HIS/Worklist 适配 | MeyerScan_HisWorklistAdapter.dll | HIS/Worklist 患者信息查询、下拉选择、建单字段映射 |

**第三阶段关键注意事项**：

- 已有模块先包一层清晰接口，不要边集成边大改内部实现。
- Crypto、NetworkHelper、Login、QRCodeAuthEntry 接入时优先保证 ABI 边界和日志留痕。
- DataExport 涉及患者和订单数据，必须通过 Service 获取业务数据，不绕过业务层直接拼数据库语义。
- Statistics 只做查询聚合和报表数据准备，不修改业务状态。
- 阶段结束标准：登录、云端、导出、统计均可从主 EXE 插件链路调用，并具备失败日志。

### 第四阶段：设备 + 校准 + 扫描数据

| 序号 | 任务 | 模块 | 说明 |
|------|------|------|------|
| 4.1 | 开发三维校准 UI 模块 | MeyerScan_Calibration3DUI.dll | 🟡 v0.1.0 Qt Widgets 骨架已完成；后续接入标定器流程、25 幅采集、三维校准计算入口、结果记录 |
| 4.2 | 开发颜色校准 UI 模块 | MeyerScan_CalibrationColorUI.dll | 🟡 v0.1.0 Qt Widgets 骨架已完成；后续接入颜色标定器流程、颜色采集、颜色校正参数生成、结果记录 |
| 4.3 | 校准模块集成验证 | Calibration3DUI / CalibrationColorUI | 两个校准模块分别调用算法 DLL、DeviceCmd、DeviceTransport，互不耦合 |
| 4.4 | 集成设备传输层 | MeyerScan_DeviceTransport.dll | 已开发完毕 |
| 4.5 | 集成设备命令层 | MeyerScan_DeviceCmd.dll | 已开发完毕 |
| 4.6 | 开发扫描数据 IO | MeyerScan_ScanDataIO.dll | 数据读写存盘 |

**第四阶段关键注意事项**：

- DeviceTransport 只管连接和传输，DeviceCmd 只管命令封装，禁止把扫描流程写进设备层。
- 三维校准和颜色校准按两个完整 UI+流程+计算入口 DLL 规划，避免产品现场只修改一种校准流程时影响另一种校准。
- 不再规划单独的 CalibrationUI/Calibration.dll 入口协调层；主 EXE 或设置入口只负责打开对应校准模块。
- ScanDataIO 要把数据完整性校验作为一等能力，写入失败、文件损坏、路径不可用都要可追踪。
- 设备相关模块接入后必须保留模拟/离线测试模式，降低日常调试成本。
- 阶段结束标准：设备连接、命令发送、校准入口、扫描数据落盘都可独立验证。

### 第五阶段：扫描重建独立进程

| 序号 | 任务 | 模块 | 说明 |
|------|------|------|------|
| 5.1 | IPC 通信层 | ScanReconstructStudio IPC 子目录/后续独立小库 | 仅服务独立进程模式；消息头稳定、正文使用 UTF-8 JSON/订单 ID，不并入通用 Core 大包 |
| 5.2 | 开发扫描阶段 UI | MeyerScan_ScanWorkflowUI.dll | 🟡 v0.2.1 初版增强：负责扫描对象、扫描工具、扫描控制和 QVTK 显示区；顶部按钮读取 `scanProcess.steps`，具备手型 hover、tooltip、点击切换当前扫描部位和鼠标中心滚轮缩放；当前已被 MainExe 阶段性挂入工作台 Scan 步骤，离开时释放重资源；后续接设备/算法接口 |
| 5.3 | 开发数据处理阶段 UI | MeyerScan_DataProcessUI.dll | 🟡 v0.2.1 初版增强：负责模型选择、处理工具入口、独立 Process Hint 和 QVTK 显示区；顶部按钮读取同一份 `scanProcess.steps`，具备手型 hover、tooltip、点击切换当前处理部位和鼠标中心滚轮缩放；Process 页不放 Start/Pause；当前已被 MainExe 阶段性挂入工作台 Process 步骤，离开时释放重资源；后续接编辑/测量/颈缘等处理 DLL |
| 5.4 | 开发扫描采集/三维显示能力 | ScanCapture / ModelViewer / 后续 DLL | 连接设备、抓取下位机数据、传递给算法、获得重建数据、显示数据；UI 只调用接口 |
| 5.5 | 开发数据处理工具模块 | MeyerScan_ScanProcessingTools.dll（暂定） | 颈缘、测量、倒凹、咬合、色彩、底座等处理能力；可继续细拆 |
| 5.6 | 开发扫描重建工作台 | ScanReconstructStudio.exe | 🟡 初版壳已完成；独立进程动态加载扫描 UI 和数据处理 UI，阶段切换前释放离开页面重资源 |
| 5.7 | 开发发送 UI | MeyerScan_SendUI.dll | 🟡 v0.1.0 初版框架：挂入工作台 Send 步骤，显示案例信息并上报 Export / Compress / Email Send / Upload / Previous / Finish 动作；真实导出、压缩、邮件、上传后续由服务模块承接 |

**第五阶段关键注意事项**：

- ScanReconstructStudio 必须保持独立进程，这是崩溃隔离边界，不因调试方便回退到主进程。
- 进程内已先将“扫描阶段 UI”和“数据处理阶段 UI”拆成 DLL；ScanReconstructStudio 只动态加载和挂载它们，不把两个阶段代码继续塞回 EXE。
- 数据编辑、数据处理、预处理、数据 IO、设备采集和算法重建等业务/算法能力优先按 DLL 或独立库规划，至少保证接口层与 UI/交互层分离。
- IPC 头部只传 POD；患者/订单等复杂信息传 UTF-8 JSON 字节、上下文文件路径或订单 ID，不传 QObject、QString/QJsonObject 对象指针、模型对象或大块内存所有权。
- 优先实现主进程与扫描进程之间的订单上下文、扫描状态、处理进度同步；心跳只做状态检测和日志记录，暂不把超时自动重启作为重点目标。
- 阶段结束标准：主进程可启动扫描进程、传入订单上下文、收到状态/进度反馈，并在异常退出时记录日志和修复订单状态。当前仅完成独立壳和两个阶段 UI DLL 的动态加载/切换/释放链路，真实 IPC、设备、算法和处理工具仍未完成。

### 第六阶段：联调测试与交付

| 序号 | 任务 |
|------|------|
| 6.1 | 模块内单元测试 |
| 6.2 | 模块间联调 |
| 6.3 | 跨进程联调（IPC 通信、心跳检测、崩溃恢复） |
| 6.4 | 整机功能测试 |
| 6.5 | 异常与稳定性测试 |
| 6.6 | 安装打包与安装流程验证 |
| 6.7 | 文档完善与验收交付 |

**第六阶段关键注意事项**：

- 产品交付不是只看功能可用，必须覆盖异常恢复、数据追踪、权限绕过、版本一致性和现场排查。
- 每个需要随安装包发布的 EXE/DLL 都必须进入打包阶段 `version_manifest.json` 或依赖清单，禁止安装包里出现来源不明或版本不匹配 DLL；但 MainExe 运行时 `logs/versionList` 只记录拆分模块清单，不记录第三方库。
- 安装包必须由 MyInstaller/Packaging 统一收集发布文件，禁止人工临时复制 DLL 形成不可追溯安装包。
- 安装流程必须验证安装目录层级、运行库依赖、快捷方式、旧版本处理、安装后首次启动和卸载残留。
- 权限阉割必须做绕过测试：隐藏按钮、直接调用服务、直接 IPC、替换插件清单都要验证。
- 数据安全必须做断电/崩溃/磁盘满/网络中断场景验证。
- 交付前必须输出：开发文档、部署说明、版本清单、测试记录、已知问题清单、回滚方案。

### 自动更新模块：MyUpdate.exe

MyUpdate.exe 是独立进程，存放位置与 MeyerScan.exe 同级。它不作为普通 DLL 加载，原因是更新过程需要关闭 MeyerScan.exe、覆盖程序文件、再重新启动 MeyerScan.exe；这类生命周期不能放进正在被覆盖的主程序内部。

| 模式 | 触发方式 | 流程 |
|------|----------|------|
| **检查更新** | MeyerScan.exe 中点击“检查更新”按钮 | MeyerScan.exe 生成 `myLocalUpdate.json/xml` → 启动 MyUpdate.exe → MyUpdate.exe 拉取 `MyCloudUpdate.json/xml` → 比对账号、设备、硬件、驱动、版本要求 → 弹出提示 → 用户确认后下载补丁包 → 关闭 MeyerScan.exe → 覆盖升级 → 重新启动 MeyerScan.exe |
| **主动更新** | 用户双击 MyUpdate.exe | MyUpdate.exe 自行采集本机信息或读取最近一次 `myLocalUpdate.json/xml` → 拉取云端更新配置 → 界面展示满足/不满足项 → 用户确认后执行同样的下载和覆盖流程 |

`myLocalUpdate.json/xml` 建议包含：本地软件版本、模块版本清单、系统版本、CPU/内存/显存、显卡驱动版本、登录账号、客户编号、设备编号、机型、当前安装路径、语言/区域、最近一次更新状态。

`MyCloudUpdate.json/xml` 建议包含：最新版本号、最低可升级版本、补丁包地址、补丁包哈希、显存大小要求、显卡驱动要求、内存要求、系统版本要求、账号白名单/黑名单、设备白名单/黑名单、客户范围、灰度比例、强制/可选升级标志、更新说明。

MyUpdate.exe 后续详细设计时再确定 UI、下载断点续传、回滚策略、签名校验和补丁包格式；当前阶段先纳入整体架构和模块拆分。

---

### 安装打包模块：MyInstaller / Packaging

安装打包暂作为独立发布交付模块纳入整体重构方案，后续真正实现该模块时再详细讨论安装器技术选型、脚本结构、界面细节和回滚策略。当前阶段先明确职责边界：

| 职责 | 当前规划 |
|------|----------|
| **生成安装包** | 收集 `MeyerScan.exe`、`MyUpdate.exe`、`ScanReconstructStudio.exe`、各插件 DLL、Qt 运行库、平台插件、SQL 驱动、默认配置、资源文件、帮助文档、版本清单，生成正式安装包 |
| **自定义安装界面** | 支持品牌/产品名、语言选择、许可说明、安装路径、组件选择、安装进度、完成页、启动选项等安装向导界面 |
| **自定义安装流程** | 支持安装前环境检查、旧版本检测、安装目录检查、组件选择、文件复制、快捷方式创建、安装后初始化、卸载入口、失败提示 |
| **文件夹层级** | 固化安装目录结构，区分 `bin/`、`plugins/`、`platforms/`、`sqldrivers/`、`config/`、`resources/`、`docs/`、`logs/`、`data/`、`updates/` 等目录用途 |
| **版本与依赖核对** | 将打包阶段 `version_manifest.json` 写入安装包，并在打包前核对 EXE/DLL/Qt 运行库版本一致性；运行时 `logs/versionList` 继续只记录拆分模块 |

建议安装后的目录层级先按以下方向规划，后续实现时再结合实际安装器调整：

```text
MeyerScan/
├── bin/                         # MeyerScan.exe、MyUpdate.exe、核心运行库
├── plugins/                     # 业务插件 DLL
├── ScanReconstructStudio/       # 扫描重建独立进程及其算法/显示依赖
├── platforms/                   # Qt platform 插件，如 qwindows.dll
├── sqldrivers/                  # 仅外部既有 QtSql 依赖需要时保留；Database 不依赖 Qt SQL 驱动
├── config/                      # 默认配置、插件清单、权限/功能配置模板
├── resources/                   # 图标、图片、样式、语言 qm 等资源
├── docs/                        # 帮助文档、version_manifest.json、发布说明
├── data/                        # 本地数据默认目录（可由配置迁移到用户指定路径）
├── logs/                        # 默认日志目录（可由配置迁移到用户指定路径）
└── updates/                     # 更新缓存、补丁临时目录
```

MyInstaller/Packaging 不负责业务建单、扫描采集、数据库 CRUD、权限判断或云端上传；它只负责发布文件组织和安装过程控制。

---

## 七、项目文件层级

```
MeyerScan_Project/
├── Source/
│   ├── Common/                  # 已有少量公共头；只放稳定、无业务语义的复用代码
│   ├── Logger/                  # MeyerScan_Logger.dll ✅
│   ├── Crypto/                  # MeyerScan_Crypto.dll
│   ├── ConfigCenter/            # MeyerScan_ConfigCenter.dll
│   ├── UIComponents/            # MeyerScan_UIComponents.dll
│   ├── Database/                # MeyerScan_Database.dll
│   ├── MyCaseOrderService/      # MeyerScan_CaseOrderService.dll；内部维护 DTO/JSON 合同
│   ├── MyOrderScanWorkspaceShell/ # MeyerScan_OrderScanWorkspaceShell.dll
│   ├── MySendUI/                # MeyerScan_SendUI.dll
│   ├── ScanSchemaService/       # MeyerScan_ScanSchemaService.dll
│   ├── OrderWorkflowService/    # MeyerScan_OrderWorkflowService.dll
│   ├── MyCaseUI/                # MeyerScan_CaseUI.dll 🟡 framework
│   ├── CaseUI/                  # legacy/planned naming reference
│   ├── MyHomeUI/                # MeyerScan_HomeUI.dll 🟡 framework
│   ├── HomeUI/                  # legacy/planned naming reference
│   ├── OrderCreateUI/           # MeyerScan_OrderCreateUI.dll
│   ├── ExternalLaunchAdapter/    # MeyerScan_ExternalLaunchAdapter.dll
│   ├── HisWorklistAdapter/       # MeyerScan_HisWorklistAdapter.dll
│   ├── Device/
│   │   ├── DeviceTransport/     # MeyerScan_DeviceTransport.dll
│   │   └── DeviceCmd/           # MeyerScan_DeviceCmd.dll
│   ├── Permission/              # MeyerScan_Permission.dll
│   ├── PermissionConfigUI/       # MeyerScan_PermissionConfigUI.dll
│   ├── UI_Modules/
│   │   ├── Login/               # MeyerScan_Login.dll
│   │   ├── SettingsUI/          # MeyerScan_SettingsUI.dll
│   │   ├── Calibration3DUI/     # MeyerScan_Calibration3DUI.dll
│   │   ├── CalibrationColorUI/  # MeyerScan_CalibrationColorUI.dll
│   │   ├── QRCodeAuthEntry/     # MeyerScan_QRCodeAuthEntry.dll
│   │   └── EngineeringSettings/ # MeyerScan_EngineeringSettings.dll
│   ├── ScanReconstruct/
│   │   ├── ScanDataIO/          # MeyerScan_ScanDataIO.dll
│   │   ├── ScanDataPreProcess/  # MeyerScan_ScanDataPreProcess.dll
│   │   ├── ScanWorkflowUI/      # MeyerScan_ScanWorkflowUI.dll
│   │   ├── DataProcessUI/       # MeyerScan_DataProcessUI.dll
│   │   ├── ScanCapture/         # 采集能力，后续源码子目录或 DLL
│   │   ├── ModelViewer/         # 三维显示能力，后续源码子目录或 DLL
│   │   ├── EditTools/           # 编辑工具子模块，后续优先 DLL 化
│   │   ├── ProcessingTools/     # 颈缘/测量/倒凹/咬合/底座工具，后续优先 DLL 化
│   │   └── ScanReconstructStudio/ # ScanReconstructStudio.exe
│   ├── DataExport/              # MeyerScan_DataExport.dll
│   ├── Statistics/              # MeyerScan_Statistics.dll
│   ├── MainExe/                 # MeyerScan.exe
│   └── MyUpdate/                # MyUpdate.exe（与 MeyerScan.exe 同级发布）
├── Installer/                   # MyInstaller/Packaging：安装包生成、自定义安装界面和安装流程
├── Tools/
├── Firmware/
├── Test/
├── Docs/
├── Build/
└── ThirdParty/
```

---

## 八、软件启动流程

```
软件启动
    │
    ▼
┌──────────────────────────┐
│   Logger.dll 初始化       │  ← 最先加载，零 Qt 依赖
│   Init(logDir, Info)      │     静态 CRT（/MT），不需 msvcr140.dll
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│   ConfigCenter.dll 加载   │  ← 读本地 JSON 配置文件（不依赖数据库）
│   LoadConfig(configPath)  │     获取: database.type, logger.logDir...
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│   数据库验证              │  ← DatabaseQtAdapter.dll -> Database.dll
│   （从 ConfigCenter 获取  │     从 ConfigCenter 拿连接参数
│     连接参数后连接/校验）   │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│   Permission.dll 校验     │  ← 权限门禁
│   CheckAccess("Startup") │     返回可访问功能列表
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│   登录模块               │  ← Login.dll
│   （账号/验证码/扫码）     │
└──────────────────────────┘
    │
    ▼
┌──────────────────────────┐
│   首页界面               │  ← HomeUI.dll
│   （四大入口）            │
└──────────────────────────┘
    │
    ├─────→ 创建订单 ──→ OrderScanWorkspaceShell.dll（Order/Scan/Process/Send）──→ OrderCreateUI.dll ──→ OrderWorkflowService.dll ──→ ScanReconstructStudio.exe
    ├─────→ 练习扫描 ──→ OrderScanWorkspaceShell.dll（Scan/Process）──→ 阶段性挂载 ScanWorkflowUI/DataProcessUI；后续切换为 ScanReconstructStudio.exe
    ├─────→ 案例管理 ──→ CaseUI.dll ──→ OrderWorkflowService.dll（加载订单决策）
    └─────→ 设置     ──→ SettingsUI.dll（必要时再进入 EngineeringSettings 高级维护入口）
```

---

## 九、2026-07-09 补充规则：QSS、资源、版本与扫描重建双形态

### 9.1 Qt 模块日志便捷层

依赖 Qt 的界面模块可以使用公共头文件 `Common/include/MeyerQtModuleUtils.h` 输出日志。调用方只需要传 `QString` 内容和可选操作名，模块名由工程里的 `MEYER_MODULE_NAME` 自动补充，避免每个 UI 模块重复维护 `moduleName` 字符串。

规则：

1. 每个模块仍然只缓存一份 `ILogger* m_logger`，生命周期内持续使用该变量写日志。
2. DLL 加载、工厂函数解析、页面切换、按钮点击、资源释放、模块 Shutdown 成功/失败都要写日志。
3. 日志目录始终为 `MeyerScan.exe` 同级 `logs/`，路径必须由 `QCoreApplication::applicationDirPath()` 或上层传入的 appDir 推导，禁止用 `QDir::currentPath()`。

### 9.2 QSS 与资源目录规则

所有界面控件样式必须通过 QSS 文件加载，业务源码中不得直接拼接样式字符串或调用 `setStyleSheet()`。唯一允许的源码入口是公共函数 `MeyerQtModule::ApplyModuleQss()`。

模块资源源码目录：

```text
ModuleName/
  Resources/
    icon/
    qss/
```

正式运行/发布目录：

```text
MeyerScan.exe
MeyerScan_UIResources.dll
Resources/
  license.lic
```

构建阶段由 `MyUIResources/tools/GenerateResourceManifest.ps1` 扫描各模块资源，使用 Qt `rcc -binary` 生成 RCC 数据并作为 Win32 `RCDATA` 嵌入 `MeyerScan_UIResources.dll`。公共加载器在第一个 UI 页面创建前动态注册资源，模块仍使用 `QFile`、`QPixmap`、`QImage`、`QIcon` 等 Qt 默认 API，通过 `:/MeyerScan/Modules/<ProjectName>/...` 访问。

现行优先级为：资源 DLL -> 源码树 `Resources` 开发降级 -> 旧安装目录散文件兼容。正式安装包只交付资源 DLL，不交付 UI PNG/QSS 散文件；源码调试不要求每次先复制资源。`Resources/license.lic`、配置文件、帮助文档等非 UI 数据不进入资源 DLL。

资源 DLL 化用于降低普通客户误改、误删单个图片/QSS 的概率，并减少发布目录文件数量；它不是加密。后续安装打包仍需校验 DLL 是否存在、文件版本、代码版本、哈希/数字签名，并提供安装修复能力。当前 Icon/QSS 不拆成两个 DLL，因为二者加载时机和升级版本一致，拆分只会增加清单和升级故障点；将来只有资源包显著变大或 OEM 裁剪必须独立发布时，才按业务域拆包。

### 9.3 全屏无边框与右上角按钮

首页、浏览、创建、练习均由 MainExe 的全屏无边框主窗口承载，不使用 Qt 原生标题栏和系统右上角按钮。页面内需要的最小化、关闭、设置、返回首页、上传等按钮必须是模块自绘控件，图标来自模块 `Resources/icon`，样式来自模块 QSS。

MainExe 只拥有顶层窗口和单内容区，不绘制所有页面共享的可见标题栏。HomeUI、CaseUI 和 OrderScanWorkspaceShell 各自绘制符合页面语义的顶部区域；页面只上报稳定动作 ID，最小化、关闭、返回首页和页面替换仍由 MainExe 执行。

当前规则：

1. 首页按参考图保留右上角校准/云端/帮助/最小化/关闭自绘按钮。
2. 浏览按参考图保留顶部订单/患者切换和右上角自绘工具按钮。
3. 创建和练习工作区右上角当前只保留最小化和关闭两个自绘按钮。
4. 用户可见文本源码仍使用 `tr("English source text")`，中文显示交给 qm 文件；不能为了多语言长度写 if/else 调整控件位置。

### 9.4 版本信息与版本清单

每个自研 DLL/EXE 都必须编译 `Version.rc`，文件详细信息字段至少包含：

`CompanyName`、`FileDescription`、`FileVersion`、`InternalName`、`OriginalFilename`、`ProductName`、`ProductVersion`、`LegalCopyright`。

启动版本清单只记录拆分出来的自研 DLL/EXE，不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库。`versionList` 同时记录：

1. `fileVersion`：来自 Windows `Version.rc` 文件资源。
2. `codeVersion`：来自统一导出函数 `GetMeyerModuleVersion()`。
3. `versionMatch`：用于检查文件版本和代码版本是否同步。

新增模块时优先维护 `MyMainExe/config/version_modules.json`，并同步 VS2015 PostBuild、CMake 复制规则和安装包脚本。

### 9.5 MyScanReconstructStudio 双形态

`MyScanReconstructStudio` 支持两种产物：

1. `MeyerScan_ScanReconstructStudio.dll`：可作为 DLL 嵌入 MeyerScan.exe，用于后续“创建/练习”内嵌扫描重建工作区或大客户定制。
2. `ScanReconstructStudio.exe`：可独立运行，作为独立练习进程，也便于后续 SDK/API 或第三方调用。

当前创建链路仍由 `OrderScanWorkspaceShell.dll` 统一承载 Order/Scan/Process/Send，并直接挂载 `ScanWorkflowUI.dll`、`DataProcessUI.dll`、`SendUI.dll`。是否把练习或创建中的 Scan/Process 替换为 `MeyerScan_ScanReconstructStudio.dll`，需要后续单独确认交互层级，避免出现双壳子嵌套。

稳定性规则：

1. 扫描/处理页面切换前必须调用离开模块的 `DeactivateAndRelease()`。
2. 不主动热卸载持有 VTK/OpenGL 资源的 DLL，降低退出阶段析构顺序风险。
3. 嵌入 DLL 形态下，窗口指针必须能识别父对象提前释放，避免二次 delete。

## 十、2026-07-10 glm52 建议复核与当前优先项

`glm52/01-04` 是评审输入，不是自动生效的架构规范。逐项结论记录在 `glm52/05-2026-07-10-建议复核与采纳结论.md`，当前优先级如下：

1. 优先补真实业务链路：建单保存/查询/打开订单、扫描方案和订单流程规则；不继续增加只含占位接口的空模块。
2. UIComponents 后续补 ScreenUtil/DpiUtil/LayoutRules，但页面仍由自身布局负责，不能重新引入按分辨率整体坐标缩放或按语言 if/else 调尺寸。
3. `OrderScanWorkspaceShell` 是工作台步骤导航的唯一所有者；OrderCreateUI、ScanWorkflowUI、DataProcessUI、SendUI 只提供内容页。
4. 当前不把 `Core.lib`、`CaseEntity.lib` 作为开发前置。稳定契约出现真实重复后再小范围抽取；患者/订单字段通过服务 DTO、版本化 JSON 和 schema migration 演进。
5. DLL 形态的 ScanReconstructStudio 使用进程内接口；独立 EXE 形态才需要 IPC。IPC 不阻塞当前创建/练习工作台链路。
6. 自动更新采用关闭主程序后替换文件，不要求 UI/VTK DLL 运行期热替换或热卸载。
7. `MyCaseManager` 继续作为只读历史 schema 参考并排除活跃构建、版本清单和安装包，确认迁移价值清零后再物理删除。

## 十一、2026-07-10 UI 资源 DLL、界面复现与脚本规范复核

1. 新增 `MyUIResources`，统一输出 `MeyerScan_UIResources.dll` 和 `UIResourcesTest.exe`；资源源码所有权不改变，业务模块仍只维护自己的 `Resources`。
2. MainExe v0.1.6 将资源 DLL 加入构建依赖、Release 复制和 `version_modules.json`，并接收浏览页云端/截图稳定动作 ID；运行时版本清单现为 24 个启动记录文件。
3. HomeUI v0.3.1 按参考首页重新校准背景、品牌、右上角工具按钮和四入口；完整背景按窗口宽高比等比覆盖，入口图标按历史整图的有效区域裁切，避免缩小整图导致图标过小。
4. CaseUI v0.3.1 恢复 Orders/Patients 页面导航，订单列表改为响应式卡片流；1920 宽度四列、1366 宽度三列，筛选栏和窗口动作保持在同一页面层级。
5. OrderCreateUI 经 2026-07-12 升级到 v0.5.2：左栏首行四个常用类型、第二行宽种植体按钮；前四种 hover/选中为局部彩色圆底，种植体为整行高亮；扫描流程预览固定高度，不因种植流程变长而缩小牙弓。
6. UI 测试宿主统一支持 `--capture-screenshot <png> --capture-size <WxH>`，用于固定 1366x768、1920x1080 和 2560x1440 截图验收。多分辨率适配继续使用 Layout、最小/最大尺寸、滚动区和伸缩因子，不恢复全界面坐标乘缩放系数，也不按语言写尺寸 if/else。
7. PowerShell 5.1 的编码、引号、退出码、路径、robocopy 和删除安全规范见 `PowerShell开发与自动化脚本规范.md`；仓库内可执行摘要见 `F:\MeyerScan\tools\README.md`。
8. 最终验证基线：VS2015 根方案和 CMake 根构建均通过；24 项模块/主链路测试全部返回 0；最新 versionList 为 24 项、0 缺失、0 文件/代码版本不一致、0 `codeVersionError`；首页、浏览、建单均完成 1920x1080 与 1366x768 截图复核。
9. `VersionManagerTest` 必须在 `test_runtime/VersionManagerTest` 隔离目录生成测试清单，禁止覆盖 MainExe 正式 `config/version_modules.json`；正式清单测试前后哈希必须保持一致。

## 十二、2026-07-12 建单治疗方案最终规则

1. 修复类型固定为全冠、缺失牙、嵌体、贴面、种植体五种；`inner_crown` 和 `bridge` 不属于当前修复类型。单颗牙图片序号固定为 `1/3/4/5/7`。
2. 牙位 mask 编码递增顺序固定为上颌 `11..18,21..28`、下颌 `31..38,41..48`；桥 mask 顺序与同颌相邻牙 key 一一对应。该规则来自实际 mask 和叠加 PNG 重叠验证，不按屏幕左右顺序猜测。
3. 任意两颗同颌直接相邻且已设置治疗方案的牙位显示空心桥连接点；点击后显示实心连接点。外部 `bridgeConnectors` 必须通过格式、相邻关系和两端牙位存在性校验。
4. 类型按钮普通态读取 `*_b.png`，hover/选中态读取 `*_h.png`；2560x1440 及以上读取 `*_2x.png` 源图，逻辑尺寸保持不变。
5. `TreatmentPlanResourceRules.h` 只放生产代码与测试共用的纯映射规则，不进入 DLL 公共 ABI；测试宿主不得直接链接 `ToothTreatmentPlanWidget` 等 DLL 内部 C++ 类。
6. 验证基线：VS2015 单模块和根方案通过；模块目录/根目录 OrderCreateUI smoke、UIResources smoke 通过；1366x768、1920x1080、2560x1440 无文本裁切、重叠或中间区域过度拉伸。

## 十三、2026-07-12 文档、权限、注释与集成验证收口

1. 四份核心重构文档的权威位置统一为 `F:\MeyerScan\Documents`，随源码提交 GitHub 并进入本地整体仓库；`D:\wj\重构文档` 只保留同名同步镜像。
2. Codex 本机配置使用 `danger-full-access`、`approval_policy = "never"`，并把 `F:\MeyerScan`、`F:\MeyerScan-Reposit` 标记为 trusted。该设置用于减少构建、测试、截图和受控文件操作的重复确认，不取消删除前路径边界校验和 Git 防误删规则。
3. 新增 `tools\CheckSourceCommentSafety.ps1`。中文 C/C++/PowerShell 源码必须使用 UTF-8 BOM，`.rc` 必须纯 ASCII；`//` 注释独占物理行且末尾禁止反斜杠，临时禁用代码使用带原因的 `#if 0`。
4. 历史称谓 `minMain` 不再对应独立项目，当前等价验证是 `MyMainExe/MeyerScan.exe --smoke-main`。2026-07-12 已同时验证根聚合输出和单模块输出，二者返回 0。
5. 根 `MeyerScan.exe` 已在最终资源 DLL 生成后再次强制重新链接，生成时间为 2026-07-12 17:00:15；`--smoke`、`--smoke-main`、外部 JSON 建单 smoke 均返回 0。最新 versionList 为 24 项、0 缺失、0 文件/代码版本不一致、0 代码版本读取错误。
6. OrderCreateUI v0.5.2 和 UIResources v0.1.3 已完成 VS2015 根方案 Rebuild、CMake 全量构建、模块/根 smoke 和三档截图复核；截图采用离屏固定画布并校验 PNG 真正为 1366x768、1920x1080、2560x1440，2K Scan Plan 最大高度为 1060px；测试显式依赖同批资源 DLL，禁止旧资源包遮盖当前 QSS。

## 十四、2026-07-12 全模块代码审查与失败合同

1. 模块可用状态不能只看 DLL 是否加载成功。动态模块必须依次检查“加载 DLL -> 解析工厂 -> 获取接口 -> Init -> 写入上下文 -> 创建页面”，任一步失败都要记录模块名和阶段，并停止使用该接口或进入文档允许的降级路径。
2. 所有返回 bool 的初始化和上下文接口都必须由调用方检查。测试项目遵守同一规则；Init/CreateWidget 失败时先清理已取得资源，再返回可区分阶段的非 0 退出码，禁止继续解引用空 QWidget。
3. JSON 上下文采用事务式更新：先在临时对象中完整解析和校验，成功后替换缓存并刷新 UI；失败返回 false、保留上一份有效状态。MainExe 收到拒绝后记录 `ContextRejected`，不能继续切换到目标步骤。
4. Qt 重页面生命周期固定为 `Init -> SetContext -> CreateWidget -> 宿主挂载 -> Activate -> DeactivateAndRelease -> 删除 QWidget -> Shutdown`。CreateWidget 不隐式 Activate；Scan/Process 目标页创建失败时工作台和 ScanReconstructStudio 不更新当前步骤。
5. Logger、UIComponents 和设置内校准模块的失败处理必须明确：Logger 可无日志降级；UIComponents 可回退本模块 Qt 控件/QSS；校准子模块失败只禁用对应校准页。任何降级都要清空半初始化接口，不能把可选依赖失败扩散成主流程崩溃。
6. 本轮边界复核未发现 UI 直接访问 Database、SendUI 实现真实发送业务、运行路径依赖 `QDir::currentPath()` 或业务源码直接调用 `setStyleSheet()`。样式调用仍只允许公共 `MeyerQtModuleUtils::ApplyModuleQss()` 入口。
7. 当前版本基线：MainExe v0.1.7、HomeUI/CaseUI v0.3.2、SettingsUI v0.2.1、OrderCreateUI v0.5.3、WorkspaceShell/ScanReconstructStudio v0.1.3、ScanWorkflowUI/DataProcessUI v0.2.3、SendUI v0.1.2、UIResources v0.1.3。
8. 验证基线：VS2015 根方案 Rebuild 和 CMake Release 全量构建通过；24 项自研测试/主链路及 MainExe 登录前 smoke 均返回 0；最新 versionList 为 24 项、0 缺失、0 版本不一致、0 `codeVersionError`；源码注释安全检查为 0 错误、0 警告。

## 十五、2026-07-13 生产数据与测试数据隔离

1. 正式 UI、MainExe 手工创建入口和生产服务不得内置患者、订单、医生、技工所、牙位或治疗方案示例值。无外部上下文时，建单页必须显示空白业务状态和可翻译的选择提示，不能把演示数据误保存为真实订单。
2. 练习模式允许生成脱离数据库的默认上下文，但患者号、订单号等可追踪字段必须使用 `PRACTICE_*` 前缀；测试宿主数据只存在于测试进程和测试数据库，不得进入生产模块默认值。
3. 外部拉起和恢复订单提供的 JSON 先写入候选对象，完成解析、字段类型和 schema 校验后再整体替换缓存。解析失败时返回 `false`、保留上一份有效上下文，并由宿主记录 `ContextRejected`。
4. OrderCreateUI smoke 必须覆盖“无上下文为空白”“有效上下文可显示”“随后写入非法 JSON 不覆盖缓存”“Shutdown 后可重新 Init”四类状态，避免生产默认值和生命周期回归。
5. 2026-07-13 验证基线：VS2015 根方案和 CMake Release 构建通过；MainExe `--smoke`、`--smoke-main`、`--smoke-external-order` 通过；`versionList_20260713_072234_645.json` 共 24 项，0 缺失、0 文件/代码版本不一致、0 `codeVersionError`。

---

> **文档版本**：v3.32（2026-07-13，补齐生产/测试数据隔离、空白建单初态和最新验证基线）
