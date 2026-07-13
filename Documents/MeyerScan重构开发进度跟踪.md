# MeyerScan 重构开发进度跟踪

> **文档目的**：记录每次开发任务流程，标明当前任务和接下来的任务，便于 AI 和团队成员了解进度。
>
> **状态标记说明**：
> - 🔴 未开始
> - 🟡 进行中
> - 🟢 已完成
> - ⏸️ 暂缓（阻塞/依赖未就绪）
> - ⏭️ 跳过（已有模块，待后续集成）
>
> **权威文档位置（2026-07-12 起）**：`F:\MeyerScan\Documents\MeyerScan重构开发进度跟踪.md`。`D:\wj\重构文档` 下同名文件仅为同步镜像；本文只记录完成度、阻塞项和可复现验证证据，不重复展开架构设计。

---

## 2026-07-13 最新验证基线

- 四份核心文档已迁入 `F:\MeyerScan\Documents` 并作为权威源，D 盘同名文件作为同步镜像。
- 当前审查版本：MainExe v0.1.7、HomeUI/CaseUI v0.3.2、SettingsUI v0.2.2、OrderCreateUI v0.5.3、WorkspaceShell/ScanReconstructStudio v0.1.3、ScanWorkflowUI/DataProcessUI v0.2.3、SendUI v0.1.2、UIResources v0.1.3。
- 全模块复核补齐 Init/上下文/CreateWidget 返回值检查、JSON 事务式更新、CreateWidget/Activate 分离、全部流程禁用状态和可选依赖显式降级；近期模块公开接口、内部实现和测试宿主已补充中文实现技巧注释。
- 清理 OrderCreateUI、SendUI 和 MainExe 正式路径中的示例患者、订单、医生、技工所和牙位；手工创建恢复空白初态，练习数据使用明确的 `PRACTICE_*` 标识。
- VS2015 根 `MeyerScan_AllModules.sln` Release x64 和 CMake 全量 Release 构建通过；只保留外部登录头文件既有 C4819/C4091 警告。
- 根 `MeyerScan.exe --smoke`、`--smoke-main` 和 `--smoke-external-order` 均返回 0；OrderCreateUI smoke 已覆盖空白生产状态、有效/非法 JSON 事务更新和重新初始化。
- 自研模块测试覆盖 Logger、Database、DatabaseQtAdapter、ConfigCenter、Permission、VersionManager、RuntimeDataCenter、CaseOrderService、UIComponents、UIResources、两校准 UI、Home/Case/Settings/Order/Workspace/Scan/Process/Send、ExternalLaunchAdapter 和 ScanReconstructStudio，全部返回 0。
- 本轮最新 versionList 为 `versionList_20260713_222652_801.json`：schemaVersion=2、24 项、0 缺失、0 版本不一致、0 `codeVersionError`；SettingsUI 文件/代码版本为 0.2.2。源码注释安全脚本为 0 错误、0 警告；静态边界扫描未发现新增直接数据库访问、`currentPath` 或业务源码内联样式。

## 当前未完成闭环

| 功能链路 | 当前成熟度 | 尚缺内容 |
|----------|------------|----------|
| 首页/浏览/设置/建单导航 | 框架已接通 | 真实保存、编辑、删除、失败回显和权限服务端复核 |
| 患者/订单 | 最小服务与只读快照 | OrderCreateUI 保存到 CaseOrderService、正式迁移、并发/事务和完整 CRUD |
| 设置 | 页面与部分只读数据 | ConfigCenter 上下文注入、Apply/Confirm/Restore 持久化和来源页刷新 |
| 扫描/处理 | QVTK 页面和步骤切换 | 设备采集、重建算法、真实模型状态、编辑/分析算法和异常恢复 |
| 3D/颜色校准 | DLL/UI 占位框架 | 设备交互、采集、计算、结果保存和失败处理 |
| 发送 | UI 动作上报 | 导出、压缩、邮件、云上传、重试和状态服务 |
| 独立进程 | ScanReconstructStudio EXE/DLL 双形态 | MainExe 进程管理、版本化 IPC、状态同步和异常退出处理 |
| 交付 | 运行时版本清单 | 自动更新、安装包、自定义安装流程、签名/哈希和升级回滚 |

> 页面存在或 smoke 通过只表示相应框架可运行，不能把上表中的链路标记为业务完成。

## 统一自动测试

- 根 CMake 已登记 24 项 CTest：22 个模块测试，加 MainExe 内部导航和第三方建单两个集成 smoke；2026-07-13 实际执行结果为 24/24 通过，总耗时约 24 秒。
- 标准命令：`ctest --test-dir F:\MeyerScan\build -C Release --output-on-failure`。
- CTest 使用 `build/ctest_runtime/Release` 干净运行目录，统一串行并设置 120 秒超时；三档截图、真实数据库/设备/算法、长时间稳定性和安装包另行验收。
- CTest 之外单独执行 `MyMainExe\bin\Release\MeyerScan.exe --smoke`，登录前启动链路返回 0。

## 总进度概览

| 阶段 | 状态 | 模块数 | 完成/总数 | 开始日期 | 完成日期 |
|------|------|--------|-----------|----------|----------|
| 第一阶段：底座搭建 | 🟡 进行中 | 8 | 4/8 | 2026-06-12 | — |
| 第二阶段：核心支撑+案例UI | 🟡 进行中 | 9 | 6/9 | 2026-06-17 | — |
| 第三阶段：已有模块集成 | 🟡 进行中 | 9 | 2/9 | 2026-06-22 | — |
| 第四阶段：设备+校准+扫描IO | 🟡 进行中 | 5 | 2/5 | 2026-06-24 | — |
| 第五阶段：扫描重建独立进程 | 🟡 进行中 | 9 | 4/9 | 2026-07-06 | — |
| 第六阶段：联调测试与交付 | 🔴 未开始 | 6 | 0/6 | — | — |

> **阶段口径更新**（2026-06-23）：最终方案按“主 EXE + 静态库 + 插件 DLL + 独立进程”推进。HomeUI/CaseUI 框架已完成，因此第二阶段改为进行中；患者/订单强绑定，不再拆为 CaseService/OrderService，统一由 CaseOrderService 管理；ScanSchemaService、OrderWorkflowService、DataExport/Statistics 保持独立。

## 主链路路线图（轻量化团队）

| 里程碑 | 当前状态 | 阶段目标 | 验证标准 |
|--------|----------|----------|----------|
| M0 架构冻结与工程底座 | 🟡 进行中 | 主 EXE 壳、最小版本化合同、插件清单、构建模板统一 | 新模块可按模板创建、编译、提交；不依赖预建 Core/Entity 大包 |
| M1 基础设施闭环 | 🟡 进行中 | Logger、Database、DatabaseQtAdapter、ConfigCenter、运行时版本清单稳定 | 基础设施可被其他模块稳定调用；运行时版本清单当前由 MainExe 内置生成 |
| M2 病例/订单主链路 | 🟡 进行中 | CaseOrder/ScanSchema/Workflow 服务 + HomeUI/CaseUI/OrderCreateUI 最小流程 | 可创建订单、加载订单、进入扫描进程占位入口 |
| M3 权限、定制和设置闭环 | 🟡 进行中 | Permission、SettingsUI、PermissionConfigUI、EngineeringSettings、功能阉割闭环 | UI 隐藏 + Permission 拒绝 + Service/Workflow 复核可验证 |
| M4 设备、校准、数据 IO 集成 | 🔴 未开始 | DeviceTransport/DeviceCmd/Calibration/ScanDataIO 接入新架构 | 设备连接、校准、数据落盘可独立验证 |
| M5 扫描重建独立进程迁移 | 🟡 进行中 | ScanReconstructStudio 独立 EXE 壳、扫描阶段 UI DLL、数据处理阶段 UI DLL、IPC 状态同步、订单上下文传递、扫描/显示/处理工具分层 | 当前已验证扫描壳动态加载两个 UI DLL、阶段切换和资源释放；后续需完成主进程启动、IPC、真实设备/算法/处理进度同步 |
| M6 产品交付收口 | 🔴 未开始 | 全链路测试、异常恢复、权限绕过测试、版本清单、安装包、文档 | 能按版本清单复现、安装、运行、排查问题 |

> **执行原则**：不按时间倒逼架构，不为了赶阶段堆临时代码。每个里程碑只看主链路是否跑通、模块边界是否干净、后续维护是否容易。

---

## 第一阶段：底座搭建

> **目标**：搭建项目框架，开发日志系统和案例管理后台
> **依赖**：无
> **当前状态**：🟡 进行中

### 1.1 搭建项目框架 — 🟡 主程序壳框架已完成（v0.1.0）

- **模块**：MeyerScan.exe（主程序壳）
- **代码位置**：`f:\MeyerScan\MyMainExe\`
- **产出**：
  - [x] 创建 `MyMainExe/` 一级模块目录
  - [x] 创建 VS2015 解决方案和 vcxproj 工程，输出 `MeyerScan.exe`
  - [x] 主程序 EXE 框架：Qt 主窗口 + 模块编排壳（无业务逻辑）
  - [x] 启动顺序：Logger 初始化 → Database 健康检查 → Login → HomeUI → CaseUI
  - [x] Release 输出目录复制登录、首页、案例、数据库、日志和运行库依赖
  - [x] 新增 `--smoke` 和 `--smoke-main` 验证入口
  - [x] 接入登录返回参数，登录成功后进入 HomeUI
  - [x] 接入 HomeUI 入口回调，首页“浏览”进入 CaseUI
  - [x] 接入 CaseUI 操作回调，浏览模块“返回首页”切回 HomeUI
  - [x] 使用 MainExe 单内容区替换主页面：首页、浏览、等待页一次只挂载一个全屏页面，并按资源规则释放离开页面，降低闪现和不可见页面占用风险
  - [x] 记录登录状态、首页入口、浏览操作、工具栏导航和页面切换日志
  - [ ] 全局异常捕获
  - [x] Qt 5.6.3 路径配置；VTK/PCL/OpenCV 等扫描进程依赖后续在 ScanReconstructStudio 阶段配置
- **前置任务**：无
- **后续任务**：1.2 日志模块
- **验证结果**：
  - [x] VS2015 Release x64 构建通过
  - [x] `MeyerScan.exe --smoke` 返回 0
  - [x] `MeyerScan.exe --smoke-main` 返回 0
  - [x] 2026-06-22 追加验证：接入首页/浏览双向切换和操作日志后，`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main` 均返回 0
- **备注**：主程序 EXE 不承载业务 SQL、订单规则、扫描算法或设备协议；当前数据库健康检查只允许经 DatabaseQtAdapter 调用纯 C++ Database，正式业务访问必须走 RuntimeDataCenter / CaseOrderService / Workflow

### 1.2 开发日志模块 — 🟢 已完成（双构建环境验证通过）

- **模块**：MeyerScan_Logger.dll
- **代码位置**：`f:\MeyerScan\MyLogger\`
- **GitHub**：`https://github.com/weijian118/MeyerScan/tree/master/MyLogger`
- **功能要点**：
  - [x] 结构化日志格式（时间、级别、模块、操作、设备ID、案例ID、操作人、日志内容）
  - [x] 多进程安全写入（Windows Named Mutex `Global\MeyerScan_Logger_Mutex`）
  - [x] 日志文件轮转（默认每天主文件 `MeyerScan_YYYYMMDD.log`；超过 10 MiB 后生成 `_001`、`_002` 分卷）
  - [x] 运行时动态调整日志级别（`SetLogLevel` / `GetLogLevel`，原子操作）
  - [x] 不做敏感信息脱敏（按需求：调用方给什么就存什么）
  - [x] 线程/进程安全（原子级别过滤 + Windows 命名互斥量保护“选文件 + 写入”）
  - [x] UTF-8 with BOM 编码
  - [x] 同时支持 `const char*` / `std::string` / 头文件内联 `QString` 便利接口
  - [x] 日志宏封装（`MEYER_LOG_DEBUG/INFO/WARN/ERROR/FATAL`）
  - [x] 逐条同步写入：每条日志打开文件、追加一行、`FlushFileBuffers`、关闭句柄，后台可移动/删除/打包日志文件
  - [x] 模块级缓存输出：模块 Init 阶段缓存 `ILogger* m_logger`，后续在该变量生命周期内持续调用 `m_logger->Write(...)`
  - [x] CMake 构建系统（C++14, /MT 静态链接, Ninja + VS2015 双支持）
  - [x] 纯 C++ 测试程序（`LoggerTest.exe`，10 线程压力测试）
  - [x] 全部代码注释使用中文
- **双构建验证**（2026-06-15）：
  - [x] **VS2015**：双击 `.sln` → F7 编译 Debug/Release 均通过
  - [x] **VSCode**：`Ctrl+Shift+B` 弹出 Debug/Release 选择，MSBuild 编译均通过
  - [x] **LoggerTest**：8 项测试全部通过（含 5000 条多线程压力测试）
  - [x] **GitHub**：已推送到 `weijian118/MeyerScan` 仓库 `master` 分支
- **前置任务**：1.1 项目框架（Logger 已可独立编译，待集成到主解决方案）
- **后续任务**：1.4 Database 与 1.5 CaseOrderService 真实链路
- **备注**：
  - 2026-06-24 根据实际测试反馈升级为 `MeyerScan_Logger v1.1.0`，移除后台缓冲线程和 `LogBuffer.*`，改为逐条同步写入并关闭句柄。
  - 核心功能已实现，当前轮需要重新执行 VS2015 Release 和 `LoggerTest.exe` 验证。
  - 项目结构作为**后续模块的模板**：`.sln` + `.vcxproj` + `.vscode/tasks.json` + `CMakeLists.txt`
  - 源码 bug 已修复：Logger.h 补 `#include <string>`，Init(std::string) 补 level 参数，LoggerImpl.cpp 字符串拼接修复

### 1.3 定义病例/订单数据合同 — 🟡 随 CaseOrderService 演进

- **承载位置**：`MyCaseOrderService` 内部 DTO + 对外 UTF-8 JSON，不单独生成 CaseEntity.lib。
- **功能要点**：
  - [x] 患者/订单组合使用标准 JSON 边界和调用方缓冲区返回。
  - [x] 使用 `schemaVersion`、稳定字段 key 和 `extensions` 预留扩展能力。
  - [x] 患者/订单数据库语义统一归 CaseOrderService，不拆成两个服务。
  - [ ] 与正式版本化 migration、ScanSchemaService 和 OrderWorkflowService 合同对齐。
  - [ ] 为字段兼容、未知扩展字段保留和版本迁移补测试。
- **说明**：患者/订单字段高频变化，当前不采用固定结构体静态 ABI；稳定基础类型只有在多个模块出现真实重复后才按需抽取。
- **前置任务**：1.4 Database、1.5 CaseOrderService
- **后续任务**：1.7 ScanSchemaService、1.8 OrderWorkflowService

### 1.4 开发数据库模块 — 🟢 已完成（v1.2.0，当前默认 SQLite）

- **模块**：MeyerScan_Database.dll
- **代码位置**：`f:\MeyerScan\MyDatabase\`
- **功能要点**：
  - [x] 数据库连接管理（MySQL + SQLite 双支持，通过配置切换）
  - [x] SQL 执行引擎（ExecuteQuery、ExecuteUpdate、ExecuteScript）
  - [x] 事务管理（Begin/Commit/Rollback）
  - [x] 数据库备份功能（MySQL 原生备份待 SDK 接入 / SQLite 通过 Win32 文件复制实现）
  - [x] 线程安全（纯 C++ `std::mutex` 保护所有公共方法）
  - [x] JSON 配置文件解析
  - [x] 动态数据库类型切换
  - [x] 9 个单元测试用例，当前包含备份前置检查后共 21 个断言通过
- **v1.1.0 接口规范对齐**（2026-06-17）：
  - [x] 所有接口改为返回 Result\<T\> / VoidResult
  - [x] 引入 ErrorCode 枚举（临时定义，待迁移到 Core.lib）
  - [x] 集成 MeyerScan_Logger.dll（LoadLibrary 动态加载，无 CRT 冲突）
  - [x] 所有日志输出改用 Logger 模块（替代 qDebug）
  - [x] MySQL 密码硬编码添加 @todo 加密配置升级注释
  - [x] 备份路径硬编码添加 @todo 配置读取升级注释
  - [x] MEYER_MODULE_NAME 预处理器定义
  - [x] DatabaseTest 的 CRT 改为 /MD 匹配 Database.dll
  - [x] 所有注释使用中文
- **边界修正与验证**（2026-06-17）：
  - [x] 历史 Qt5Core/Qt5Sql 实现已在 2026-07-03 改为纯 C++ / sqlite3 C API；当前 Database 只做连接、SQL、事务、备份等基础设施能力
  - [x] 修复 VoidResult 非法构造函数、ILogger 局部 ABI 声明同步、SQL 错误消息悬空指针风险
  - [x] 修复 SetDatabaseType 持锁调用 Disconnect 的递归锁风险
  - [x] 源码保存为 UTF-8 with BOM，避免 VS2015 按代码页 936 误解析中文注释和字符串
  - [x] 补充 DatabaseTest 对 MeyerScan_Database 的解决方案依赖，支持 `/m` 并行构建
  - [x] Release x64 构建通过：0 warning / 0 error
  - [x] DatabaseTest.exe 运行通过：21 passed / 0 failed
  - [x] `db_config.json` 使用相对路径；Database 以配置文件所在目录解析 `mysql.dataDir` 和 `sqlitePath`
  - [x] DatabaseTest 生成独立测试配置并补齐 Qt/SQL 驱动运行依赖复制规则
  - [x] 2026-06-30：默认 `db_config.json` 切换为 SQLite；SQLite 连接前自动创建数据库文件父目录；DatabaseTest 测试配置也改为 SQLite 优先，避免依赖本机 MySQL 服务状态
- **前置任务**：1.2 Logger（Database 通过 LoadLibrary 动态加载 Logger.dll）
- **后续任务**：1.3 CaseEntity
- **备注**：采用方案 A（通用基础设施），待 ConfigCenter 就绪后改为从 ConfigCenter 获取数据库连接参数

### 1.4.1 开发运行时数据中心 — 🟡 骨架已完成（v0.1.0）

- **模块**：MeyerScan_RuntimeDataCenter.dll
- **代码位置**：`f:\MeyerScan\MyRuntimeDataCenter\`
- **功能要点**：
  - [x] 新增 VS2015 DLL 工程和 `RuntimeDataCenterTest.exe` 测试宿主
  - [x] 新增 `IRuntimeDataCenter` 接口：`Init()`、`ReloadAll()`、`ReloadDomain()`、`GetDomainJson()`、`UpdateCloudClinicJson()`、`Shutdown()`
  - [x] 本地快照 domain：`local.clinics`、`local.labs`、`local.software`、`local.doctors`、`local.settings`、`local.users`、`local.orders`、`local.patients`、`local.devices`
  - [x] 云端快照 domain：`cloud.clinicProfile`，当前由登录/云端同步模块拿到 JSON 后注入，不在本模块内联网请求
  - [x] 不暴露固定患者/订单/诊所 C++ struct 给 UI；对外返回 UTF-8 JSON 快照，调用方提供缓冲区，避免跨 DLL 内存释放问题
  - [x] 不允许调用方传 SQL 或表名；旧表名只在模块内部白名单维护
  - [x] MainExe 已在数据库连接后初始化 RuntimeDataCenter 并执行 `ReloadAll()`，失败按 Warning 记录，不阻断启动
  - [x] CaseUI 已通过 RuntimeDataCenter 读取 `local.patients` / `local.orders` 填充患者和订单表格
  - [x] SettingsUI 已通过 RuntimeDataCenter 读取 `local.doctors` / `local.clinics` / `local.labs` 填充设置页 Information 中的医生、诊所、技工所表格
  - [x] 快照读取增加有限缓冲区扩容重试，避免字段扩展后固定缓冲区不足；超过上限时必须转分页/服务查询
  - [x] 2026-07-01：测试宿主演示库补齐全部本地 domain 的最小表结构，MainExe smoke 日志可达到 `All runtime domains reloaded`
  - [ ] 后续结合正式 migration/schema 后，把旧表候选和字段映射升级为可版本化配置或服务内映射
  - [ ] 后续补充 SQLite 旧库迁移/业务 schema 初始化流程；RuntimeDataCenter 不负责执行旧 `mysql.sql`
- **边界说明**：
  - RuntimeDataCenter 是读模型/缓存中心，不做业务 CRUD、权限判断、云端请求或 UI 渲染。
  - 患者/订单保存、删除、编辑、状态变化、医生/诊所/技工所主数据维护仍归 `CaseOrderService.dll`。
  - 快照只适合轻量上下文；大批量订单、扫描数据、大文本和导出场景必须走分页/专用服务。
- **验证结果**：
  - [x] `MeyerScan_RuntimeDataCenter.sln` Release x64 构建通过，0 warning / 0 error
  - [x] `RuntimeDataCenterTest.exe` 返回 0，覆盖 SQLite 连接、本地快照读取和云端 JSON 注入
  - [x] `CaseUITest.exe --smoke` 和 `SettingsUITest.exe --smoke` 已验证数据库演示数据可通过 RuntimeDataCenter 显示到患者/订单表格及 Information 三张参考数据表
  - [x] 2026-07-01：`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、MainExe 单模块和根聚合目录 `MeyerScan.exe --smoke-main` 均返回 0；最新日志无本地 domain 缺表 Warning

### 1.5 开发病例订单服务 — 🟡 骨架已完成（v0.2.0）

- **模块**：MeyerScan_CaseOrderService.dll
- **代码位置**：`f:\MeyerScan\MyCaseOrderService\`
- **功能要点**：
  - [x] 新增 VS2015 DLL 工程，输出 `MeyerScan_CaseOrderService.dll`
  - [x] 统一患者/订单组合数据服务边界，替代原规划 `CaseService.dll` + `OrderService.dll`
  - [x] `EnsureSchema()` 创建轻量 `ms_patient_order` 与 `ms_reference_data` schema 占位
  - [x] `SavePatientOrderJson()` 按 `orderId` 保存患者/订单组合 JSON
  - [x] `GetPatientOrderJson()` 按订单 ID 读取患者/订单组合 JSON
  - [x] `ListReferenceDataJson()` 统一读取医生、诊所、技工所、操作人等参考数据
  - [x] `QueryJson()` 预留稳定查询名，当前支持 `patientOrder.byOrderId` 与 `referenceData.list`
  - [ ] 正式字段表、版本化 migration、DAO、权限复核和完整 CRUD
  - [x] `CaseOrderServiceTest.exe` 覆盖 schema、患者订单保存/读取、参考数据和稳定查询名
- **前置任务**：1.4 Database、DatabaseQtAdapter
- **后续任务**：1.7 ScanSchemaService、1.8 OrderWorkflowService、2.5 CaseUI、2.6 OrderCreateUI

### 1.6 独立订单服务 — ⏸️ 已合并取消

- **模块**：原规划 MeyerScan_OrderService.dll
- **决策**：患者和订单在口扫软件内部、数据库内部强绑定，不再单独开发 `OrderService.dll`。
- **归属**：订单 CRUD、状态、患者订单关联、医生/诊所/技工所相关主数据统一进入 `CaseOrderService.dll`；加载订单流程规则进入 `OrderWorkflowService.dll`。

### 1.7 开发扫描方案服务 — 🔴 未开始

- **模块**：MeyerScan_ScanSchemaService.dll
- **功能要点**：
  - [ ] 扫描方案 CRUD
  - [ ] 牙位、修复体类型、材料、齿色结构化保存
  - [ ] 扫描方案校验
  - [ ] 为 OrderCreateUI 和 ScanReconstructStudio 提供方案数据
- **前置任务**：1.4 Database、1.5 CaseOrderService 数据合同
- **后续任务**：2.6 OrderCreateUI、5.7 ScanReconstructStudio 壳和 5.8 真实流程集成

### 1.8 开发订单流程规则服务 — 🔴 未开始

- **模块**：MeyerScan_OrderWorkflowService.dll
- **功能要点**：
  - [ ] 统一判断新建、加载、继续扫描、进入处理、进入发送的流程动作
  - [ ] 输出建单可编辑状态、扫描启动模式、拒绝原因
  - [ ] 联动 Permission.dll 做入口和高价值动作复核
  - [ ] 不承担 UI 渲染、不承担数据库连接实现
- **前置任务**：1.4 Database、1.5 CaseOrderService、1.7 ScanSchemaService、2.2 Permission
- **后续任务**：2.1 ConfigCenter、2.5 CaseUI、2.6 OrderCreateUI、5.7 ScanReconstructStudio 壳和 5.8 真实流程集成

### 1.9 开发运行时版本清单能力 — 🟡 MainExe 内置，MyVersionManager 骨架保留（v0.1.0）

- **模块**：当前由 MeyerScan.exe 内置；`MeyerScan_VersionManager.dll` 作为历史骨架保留
- **代码位置**：`f:\MeyerScan\MyVersionManager\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案和 DLL 工程骨架
  - [x] 启动时扫描 `MeyerScan.exe` 同级目录下的软件相关 `exe` / `dll`
  - [x] 读取 Windows 文件版本信息
  - [x] 写出 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`
  - [x] MainExe 已在启动阶段内置生成版本清单并写日志，当前不再依赖 `MeyerScan_VersionManager.dll`
  - [x] 版本清单改为读取 `config/version_modules.json`，只记录清单中声明的 MeyerScan 拆分模块 EXE/DLL，不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库
  - [x] 2026-07-05：`version_modules.json` 升级为 schemaVersion=2，模块项包含 `file` 和可选 `versionFunction`；自研 DLL 统一导出 `GetMeyerModuleVersion()`，MainExe 运行时版本清单同时记录 Windows `Version.rc` 文件版本 `fileVersion`、模块代码版本 `codeVersion` 和 `versionMatch`
  - [x] 2026-07-05：版本清单文件名增加毫秒时间戳，避免同一秒内连续启动覆盖快照
  - [x] 历史 `MeyerScan_VersionManager.dll` 骨架也已改为 manifest 驱动，并同步输出 `fileVersion` / `codeVersion` / `versionMatch`，避免后续误用时恢复目录全量扫描或旧字段口径
  - [ ] 后续扩展算法 DLL、插件目录、文件哈希、签名校验和云端版本比对字段
- **前置任务**：1.1 项目框架、1.2 Logger
- **后续任务**：3.9 自动更新模块规划、6.6 安装打包模块
- **验证结果**：
  - [x] VS2015 Release x64 构建通过
  - [x] MainExe `--smoke-main` 生成 `logs/versionList/versionList_yyyyMMdd_HHmmss_zzz.json`

---

## 第二阶段：核心支撑 + 案例 UI

> **目标**：开发配置中心、权限核心、UI 组件库，并完成案例管理 UI 和工程设置
> **依赖**：第一阶段完成
> **当前状态**：🟡 进行中

### 2.1 开发配置中心 — 🟡 骨架已完成（v0.1.0）

- **模块**：MeyerScan_ConfigCenter.dll
- **代码位置**：`f:\MeyerScan\MyConfigCenter\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案和 DLL 工程骨架
  - [x] 从 `MeyerScan.exe` 同级 `config/runtime_config.json` 读取配置
  - [x] 首次运行自动生成默认配置文件
  - [x] 提供 `GetBool` / `GetInt` / `GetString` 轻量读取接口
  - [x] MainExe 已通过 ConfigCenter 读取 `database.type` 并控制 MySQL/SQLite 类型
  - [x] 运行路径由应用目录传入，不使用 `QDir::currentPath()`
  - [ ] 配置版本校验（启动时检查配置版本兼容性）
  - [ ] 配置自动迁移（旧版本配置 → 新版本格式）
  - [ ] 异常安全回滚（配置损坏时恢复到最近可用版本）
  - [ ] 配置加解密接口后续接入 Crypto
  - [ ] 所有正式模块禁止直接操作配置文件，必须调用本模块接口
- **前置任务**：1.1 项目框架
- **后续任务**：2.2 Permission、2.3 UIComponents
- **验证结果**：
  - [x] VS2015 Release x64 构建通过
  - [x] MainExe `--smoke` / `--smoke-main` 可生成并读取默认配置

### 2.2 开发权限模块 — 🟡 骨架已完成（v0.1.0）

- **模块**：MeyerScan_Permission.dll
- **代码位置**：`f:\MeyerScan\MyPermission\`
- **优化说明**：权限热路径必须保持纯粹，配置展示和扫码授权入口后续拆到 PermissionConfigUI.dll
- **功能要点**：
  - [x] 新增 VS2015 解决方案和 DLL 工程骨架
  - [x] 从 `MeyerScan.exe` 同级 `config/permission_rules.json` 读取权限规则
  - [x] 首次运行自动生成默认权限规则
  - [x] 提供 `IsFeatureVisible()` 和 `IsFeatureEnabled()` 轻量判断接口
  - [x] MainExe 已用 Permission 控制首页“设置”和浏览“返回首页”显隐
  - [ ] 加密权限规则文件导入/替换/备份/版本记录
  - [ ] 基础功能配置 + 大客户定制配置加载与合并
  - [ ] 六维权限校验（角色 + 客户 + 设备机型 + 设备序列号/加密狗 + 软件版本 + 时间 + 配置）
  - [ ] 输出"界面可见功能列表 + UI 样式规则"
  - [ ] 所有修改仅在软件重启后生效
  - [ ] 向 PermissionConfigUI.dll 提供只读授权状态和授权更新接口
- **前置任务**：2.1 ConfigCenter
- **后续任务**：2.4 PermissionConfigUI、2.5 CaseUI、2.8 EngineeringSettings
- **验证结果**：
  - [x] VS2015 Release x64 构建通过
  - [x] MainExe `--smoke-main` 可按默认权限创建首页和浏览页

### 2.3 开发共享 UI 组件 — 🟡 骨架增强（v0.4.0）

- **模块**：MeyerScan_UIComponents.dll
- **代码位置**：`f:\MeyerScan\MyUIComponents\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案和 DLL 工程骨架
  - [x] 提供 `ScaleX()` / `ScaleY()`，保留 1920x1080 作为图标、边距、间距等辅助尺寸基准
  - [x] 提供启动等待页、页面标题、字段标签、主按钮、次按钮、输入框、下拉框、日期框、多行文本框、基础表格工厂
  - [x] 新增标准按钮角色：Primary、Secondary、Text、Danger、Entry
  - [x] 新增标准按钮内容布局：TextOnly、IconOnly、IconLeftText、IconTopText
  - [x] 新增 `CreateButton()` / `CreateToolButton()` / `ApplyButtonStyle()` / `ApplyToolButtonStyle()`，支持创建标准按钮或给已有按钮套用统一样式
  - [x] MainExe 已使用 UIComponents 创建启动等待页
  - [x] HomeUI 首页入口按钮已接入 Entry 样式
  - [x] CaseUI 顶部返回/设置按钮和患者/订单工具栏按钮已接入 Primary / Secondary / Danger 样式
  - [x] 明确公共弹窗、Toast、确认框、等待页先归入 UIComponents，不单独拆弹窗模块
  - [x] 通用输入框、下拉框基础工厂
  - [x] 表单常用字段标签、日期框、多行文本框基础工厂
  - [x] 基础表格工厂和已有表格套用样式接口；表格数据、列含义、分页、排序和右键菜单仍归业务模块
  - [ ] 日历、复杂自定义表格等高级控件
  - [ ] 多主题/多背景切换支持
  - [ ] ScreenUtil / DpiUtil / LayoutRules 统一落地
  - [ ] Common qm、语言刷新通知和公共文案回退
  - [x] 跨 DLL 界面风格统一（第一步：HomeUI/CaseUI 标准按钮接入）
  - [ ] 为 OEM 定制预留样式接口
- **前置任务**：1.1 项目框架
- **后续任务**：被所有 UI 模块调用
- **验证结果**：
  - [x] VS2015 Release x64 构建通过
  - [x] MainExe 启动阶段可显示等待页
  - [x] HomeUITest / CaseUITest 可加载 UIComponents 并通过 smoke
  - [x] 2026-06-26 追加验证：UIComponents、HomeUI、CaseUI、MainExe Release x64 构建通过；`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`MeyerScan.exe --smoke-main` 均返回 0。MainExe 仍有既有登录模块头文件编码/typedef 警告，非本轮 UIComponents 接入问题
  - [x] 2026-06-26 文档/代码复查：HomeUI / CaseUI `ModuleInfo::Version` 与 `Version.rc` 升级到 v0.2.0，根 README 同步更新模块状态

### 2.3.1 开发统一 UI 资源模块 — 🟢 初版完成（v0.1.3）

- **模块**：MeyerScan_UIResources.dll
- **代码位置**：`F:\MeyerScan\MyUIResources\`
- **功能要点**：
  - [x] 资源源码仍由各 UI 模块自己的 `Resources` 维护，不复制第二份源文件
  - [x] Windows PowerShell 5.1 脚本自动扫描 PNG/QSS/SVG/ICO/JPG/BMP/GIF/QM 并生成确定性 qrc
  - [x] 使用 `rcc -binary` 生成 RCC 数据，以 Win32 `RCDATA` 嵌入 DLL
  - [x] 运行路径统一为 `:/MeyerScan/Modules/<ProjectName>/...`
  - [x] 公共加载器按“资源 DLL -> 源码树开发降级 -> 旧安装散文件兼容”定位资源
  - [x] MainExe 正式构建只复制资源 DLL，不再复制 UI PNG/QSS 散文件
  - [x] 已加入 `version_modules.json`，同时记录文件版本和代码版本
  - [x] `UIResourcesTest.exe` 覆盖注册、QSS/PNG 读取、版本导出和注销生命周期
  - [x] v0.1.3 已重新打包 OrderCreateUI v0.5.2 五种修复类型 b/h、1x/2x 图标和修正后的 hover QSS
  - [ ] 安装打包阶段增加哈希/数字签名校验和缺失修复；资源 DLL 化本身不视为加密
- **边界**：UIResources 只承载只读数据；UIComponents 继续负责控件工厂、角色和基础交互；业务页面继续归各 UI 模块。
- **验证结果**：
  - [x] VS2015/CMake 工程和独立测试宿主已建立
  - [x] 当前自动清单收集 608 个资源文件，使用 qrc + `rcc -binary` + Windows `RCDATA` 嵌入 DLL

### 2.4 开发权限配置 UI — 🔴 未开始

- **模块**：MeyerScan_PermissionConfigUI.dll
- **功能要点**：
  - [ ] 可视化展示各功能授权状态
  - [ ] 支持扫码方式设置/修改功能权限
  - [ ] 权限操作结果实时同步
  - [ ] 仅调用 Permission.dll，不保存权限规则核心数据
- **前置任务**：2.2 Permission、2.3 UIComponents
- **后续任务**：2.8 EngineeringSettings

### 2.5 开发案例管理 UI — 🟡 框架增强（v0.3.1）

- **模块**：MeyerScan_CaseUI.dll
- **代码位置**：`f:\MeyerScan\MyCaseUI\`
- **GitHub**：`https://github.com/weijian118/MeyerScan/tree/master/MyCaseUI`（已随主功能提交推送；当前仅备份规则修正小提交待网络恢复后补推）
- **功能要点**：
  - [x] Qt Widgets DLL 框架
  - [x] 患者管理 / 订单管理 Tab 框架（参考帮助文档浏览模块）
  - [x] 当前正式 DLL 不直连 Database；列表展示读取 RuntimeDataCenter 快照，测试宿主造数经 DatabaseQtAdapter 调用 Database
  - [x] 运行时加载 MeyerScan_Logger.dll 写日志
  - [x] PostBuild 复制 Qt 5.6.3 运行库、platforms/qwindows、sqldrivers/qsqlmysql/qsqlite，输出目录可独立启动
  - [x] 测试宿主启用 High DPI 属性，并按当前屏幕可用区域限制初始窗口大小、居中显示
  - [x] 当前界面文字已改为 `tr("English source text")`，为后续模块独立 qm 做准备
  - [x] 新增“返回首页”按钮，通过操作回调交给 MainExe 切换页面
  - [x] 患者/订单工具按钮、搜索回车、页签切换写入客户操作日志
  - [x] VS2015 Release x64 构建通过，0 warning / 0 error
  - [x] CaseUITest.exe `--smoke` 自动运行通过
  - [x] 2026-06-30 链路验证：`CaseUITest.exe --smoke` 会在空 SQLite 库中创建最小演示表和患者/订单/诊所/技工所/医生数据，并检查患者表、订单表均有数据行
  - [x] 2026-07-01 链路补强：测试演示库额外补齐软件信息、设置、账号、设备最小表，保证 RuntimeDataCenter 全部本地 domain 可加载；正式 CaseUI 仍不建表、不迁移、不写业务数据
  - [x] 2026-07-01 复核优化：CaseUI 只初始化 RuntimeDataCenter，不主动 `ReloadAll()`；MainExe 启动期负责全域刷新，CaseUI 读取患者/订单 domain 时按需懒加载
  - [x] 返回首页和设置入口已接入 MainExe + Permission 的 visible/enabled 下发和动作执行前复核
  - [x] 按参考浏览页恢复 Orders/Patients 页面导航，订单页改为响应式卡片流；1920 宽度四列、1366 宽度三列
  - [x] 筛选、日期范围、搜索、重置、新患者和视图按钮归入同一工具栏，订单状态转换为可读文本
  - [ ] 案例详情页
  - [ ] 发送界面（案例信息确认 + 导出 + 邮箱发送，对应当前创建模块的 2.4）
  - [x] 正式页面已移除对 Database 的直接业务访问，列表展示走 RuntimeDataCenter 快照；测试宿主造数才经 DatabaseQtAdapter
  - [ ] 搜索、CRUD、打开订单、发送等真实业务动作接入 CaseOrderService.dll、OrderWorkflowService.dll、DataExport.dll、NetworkHelper.dll
  - [ ] 扩展 Permission 展示规则到导入、导出、删除、新建、打开订单等更多动作
- **前置任务**：1.5 CaseOrderService、2.2 Permission、2.3 UIComponents
- **后续任务**：第三阶段集成

### 2.6 开发建单界面 — 🟡 初版增强（v0.5.2）

- **模块**：MeyerScan_OrderCreateUI.dll
- **代码位置**：`f:\MeyerScan\MyOrderCreateUI\`
- **功能要点**：
  - [x] 新增 VS2015 Qt Widgets DLL 工程，输出 `MeyerScan_OrderCreateUI.dll`
  - [x] 新增 `IOrderCreateUI` 接口和 `GetOrderCreateUI()` C ABI 工厂函数
  - [x] 初版采用单页工作台布局，把基本信息、扫描方案、治疗方案选择、订单明细和确认操作放在同一个界面内
  - [x] 治疗方案选择区已改为图片/mask 方案：上下颌牙弓等比缩放显示，点击坐标反算到 600x400 原始 mask，按像素值映射 FDI 牙位号
  - [x] 支持牙位叠加图、治疗类型图标按钮、桥连接点空心/实心状态和右侧明细刷新，并按每颗牙保存临时治疗类型
  - [x] 修复类型固定为全冠、缺失牙、嵌体、贴面、种植体五种，单颗牙叠加图序号固定为 `1/3/4/5/7`；`inner_crown` 和 `bridge` 已移出类型按钮
  - [x] 牙位 mask 顺序已按实际资源校准为上颌 `11..18,21..28`、下颌 `31..38,41..48`，桥 mask 同步校准，解决左右识别颠倒
  - [x] 任意同颌相邻且已设置治疗方案的牙位显示空心桥连接点，点击后显示实心连接点；外部桥数据校验格式、相邻关系和两端牙位存在性
  - [x] 桥记录按旧软件规则聚合：`16-17 + 17-18 -> 16-18`，跨中线 `11-12 + 11-21 -> 11-22`
  - [x] “Clear All” 放在上下颌之间，人工模式弹确认框，smoke 模式跳过确认框防止自动化阻塞
  - [x] 通过动作 ID 回调上报确认、取消、上一步、下一步、清空牙位、牙位变化和扫描流程变化
  - [x] 新增 `OrderCreateUITest.exe`，双击默认打开建单界面，`--smoke` 覆盖工厂函数、初始化、根控件、核心控件、牙位联动和动作回调
  - [x] VS2015 Release x64 构建通过，`OrderCreateUITest.exe` 返回 0
  - [x] 接入 MyUIComponents 的统一控件样式：通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格基础样式已复用共享 UI DLL
  - [x] 动态加载 UIComponents 时检查运行时版本；旧版 DLL 不满足新增表格接口时走本地降级，避免 vtable 不兼容风险
  - [x] 保留牙位按钮、扫描类型按钮等建单业务控件在本模块内部，避免 UIComponents 膨胀成业务控件库
  - [x] 新增上颌/下颌异性扫描杆、上颌/下颌扫描杆分段和咬合类型输入；生成 `scanProcess` JSON，供 MainExe 转发给 Scan/Process 页面
  - [x] `Segmented scanbody` 只控制第二扫描杆/第二异性扫描杆，普通扫描杆流程仍由对应颌是否存在 implant 牙位触发
  - [x] `OrderCreateUITest.exe --smoke` 覆盖扫描流程、五类型、叠加图序号、牙位/桥 mask 顺序、b/h hover、2K 阈值、空心桥候选、无效桥过滤、普通桥和跨中线桥断言
  - [x] 治疗方案资源已从历史 `bin/Release/icon/createModule/sacanPlan` 迁入 `MyOrderCreateUI/Resources/icon/createModule/sacanPlan`，正式构建统一编译进 `MeyerScan_UIResources.dll`
  - [x] 三栏改为不可折叠 `QSplitter`；左栏首行四个常用类型、第二行宽种植体按钮，中间分栏可伸缩但 Scan Plan 内容最大 980px 并居中
  - [x] 1366x768、1920x1080 和 2560x1440 使用同一套 Layout/QSS 截图验收；2K 及以上只切换 2x 图源，不按语言或分辨率写坐标分支
  - [x] 截图宿主使用固定离屏画布并校验真实 PNG 尺寸；Scan Plan 整体最大高度 1060px，高分辨率额外高度不再拉大上下颌间距
  - [x] 前四种类型 hover/选中只在图标区域显示彩色圆底和白色 h 图，种植体保留整行高亮；smoke 采样确认按钮矩形背景不变化
  - [x] 扫描流程预览固定单行高度并提供完整 tooltip；smoke 确认普通方案和种植方案下牙弓控件尺寸完全一致
  - [x] OrderCreateUITest 在 VS2015/CMake 中显式依赖同批 UIResources，避免输出目录旧资源 DLL 遮盖当前 QSS
  - [ ] 接入 RuntimeDataCenter / CaseOrderService / ScanSchemaService 加载医生、技工所、患者/订单和扫描方案真实数据
  - [ ] 由 OrderWorkflowService 决定建单完成后进入扫描、继续编辑、拒绝或其它流程
- **前置任务**：1.5 CaseOrderService、1.7 ScanSchemaService、2.3 UIComponents
- **后续任务**：5.7 ScanReconstructStudio 壳和 5.8 真实流程集成

### 2.7 开发首页 UI — 🟡 框架增强（v0.3.1）

- **模块**：MeyerScan_HomeUI.dll
- **代码位置**：`f:\MeyerScan\MyHomeUI\`
- **GitHub**：`https://github.com/weijian118/MeyerScan/tree/master/MyHomeUI`（已随主功能提交推送；当前仅备份规则修正小提交待网络恢复后补推）
- **功能要点**：
  - [x] Qt Widgets DLL 框架
  - [x] 创建、浏览、练习、设置四入口骨架（参考帮助文档首页）
  - [x] 当前正式 DLL 不直连 Database；首页只做入口展示，数据库健康检查由 MainExe 经 DatabaseQtAdapter 完成
  - [x] 运行时加载 MeyerScan_Logger.dll 写日志
  - [x] PostBuild 复制 Qt 5.6.3 运行库、platforms/qwindows、sqldrivers/qsqlmysql/qsqlite，输出目录可独立启动
  - [x] 测试宿主启用 High DPI 属性，并按当前屏幕可用区域限制初始窗口大小、居中显示
  - [x] 当前界面文字已改为 `tr("English source text")`，为后续模块独立 qm 做准备
  - [x] 新增入口回调接口，点击“浏览”可通知 MainExe 切换到 CaseUI
  - [x] 首页四个入口点击均写入客户操作日志
  - [x] VS2015 Release x64 构建通过，0 warning / 0 error
  - [x] HomeUITest.exe `--smoke` 自动运行通过
  - [x] 首页 Create / Browse / Practice / Settings 入口已接入 MainExe + Permission 的 visible/enabled 下发和动作执行前复核
  - [x] 按参考首页重新校准全屏背景、品牌、右上角按钮和四入口比例；背景保持比例覆盖，入口图标裁切有效区域后显示
  - [ ] 调用 OrderWorkflowService 决定创建、练习、加载订单的下一步动作
  - [ ] 正式入口不直接访问 Database，不直接访问 CaseOrder/ScanSchema 服务
- **前置任务**：2.2 Permission、2.3 UIComponents
- **后续任务**：整机联调

### 2.8 开发建单扫描工作台壳 — 🟡 初版集成完成（v0.1.2）

- **模块**：MeyerScan_OrderScanWorkspaceShell.dll
- **代码位置**：`f:\MeyerScan\MyOrderScanWorkspaceShell\`
- **功能要点**：
  - [x] 新增 VS2015 Qt Widgets DLL 工程，输出 `MeyerScan_OrderScanWorkspaceShell.dll`
  - [x] 新增 `IOrderScanWorkspaceShell` 接口，提供初始化、创建 widget、设置步骤、挂载步骤 widget、关闭接口
  - [x] 支持创建模式与练习模式：创建模式显示 Order / Scan / Process / Send；练习模式只显示 Scan / Process
  - [x] 顶部步骤条使用可点击 `QPushButton`，点击后切换内部 `QStackedWidget` 页面，并同步当前步骤高亮
  - [x] 顶部整合品牌、返回、唯一的步骤导航和右上角 `Minimize` / `Close`；通过稳定动作 ID 交给 MainExe 执行
  - [x] OrderCreateUI、ScanWorkflowUI、DataProcessUI 和 SendUI 只提供内容页，不复制工作台步骤条
  - [x] 提供步骤变化回调，MainExe 用它懒加载 ScanWorkflowUI / DataProcessUI，并在离开时释放隐藏页 QVTK/VTK/OpenGL 重资源
  - [x] 界面可见文字使用 `tr("English source text")`
  - [x] Release x64 构建通过，0 warning / 0 error
  - [x] 已接入 OrderCreateUI 页面
  - [x] 已阶段性接入 ScanWorkflowUI / DataProcessUI 页面，用于先跑通创建/练习工作台流程
  - [x] 已增加 `OrderScanWorkspaceShellTest.exe`，覆盖创建模式步骤点击、练习模式步骤裁剪和非法 step 防崩溃
  - [x] 已与 MainExe 页面切换、练习入口和资源释放流程联调
  - [ ] 后续真实扫描仍需通过 ScanReconstructStudio.exe 独立进程、IPC 和窗口/状态同步接入
- **前置任务**：2.3 UIComponents、2.6 OrderCreateUI
- **后续任务**：5.7 ScanReconstructStudio 壳和 5.8 真实流程集成
- **备注**：本模块是 Qt Widgets 工作台容器，可以使用 Qt 控件、布局、信号槽、QString/QMap 组织界面；业务保存、扫描采集和数据处理不进入本模块。当前 MainExe 直接挂载 ScanWorkflowUI / DataProcessUI 是阶段性集成方案，用于先打通创建/练习流程；后续真实扫描仍保持 ScanReconstructStudio.exe 独立进程边界，跨进程状态同步使用 IPC/POD/UTF-8 JSON。

### 2.9 开发工程/高级设置 — 🔴 未开始

- **模块**：MeyerScan_EngineeringSettings.dll
- **功能要点**：
  - [ ] 口扫/标定器设备相关参数显示与设置
  - [ ] 下位机升级功能
  - [ ] 参数设置（含扫描参数、数据处理参数等）
  - [ ] 集成 PermissionConfigUI.dll 的扫码验证入口
- **前置任务**：2.2 Permission、2.3 UIComponents、2.4 PermissionConfigUI
- **后续任务**：第四阶段集成设备相关

---

## 第三阶段：已有模块集成

> **目标**：将已开发完成的模块集成到新架构中，编写接口注释
> **依赖**：前两阶段框架就绪
> **当前状态**：🟡 进行中（登录测试宿主和 MainExe 接入已完成，ExternalLaunchAdapter 初版已完成；Crypto/Network/QRCode 仍待稳定适配，HIS/DataExport/Statistics 未开始）

### 3.1 集成文件加解密 — ⏭️ 跳过（已有）

- **模块**：MeyerScan_Crypto.dll
- **状态**：已开发完毕，待集成
- **集成工作**：
  - [ ] 编写对外接口头文件，注释清楚用法和适用场景
  - [ ] 在主程序插件清单中注册
  - [ ] 与其他模块的调用处预留接口声明

### 3.2 集成云端服务 — ⏭️ 跳过（已有）

- **模块**：MeyerScan_NetworkHelper.dll
- **状态**：已开发完毕，待集成
- **集成工作**：
  - [ ] 编写对外接口头文件，注释清楚用法和适用场景
  - [ ] 在主程序插件清单中注册
  - [ ] 与其他模块的调用处预留接口声明

### 3.3 集成登录模块 — 🟡 测试宿主和 MainExe 接入已完成（v0.1.0）

- **模块**：既有 `MeyerLoginWidget.dll`，测试宿主目录 `f:\MeyerScan\MyLogin\`
- **状态**：既有登录模块已通过 `MyLogin` 测试宿主和 `MyMainExe` 最小链路接入
- **集成工作**：
  - [x] 新增 `MeyerLoginTest.exe` 测试宿主，调用既有 `MeyerLoginWidget.dll`
  - [x] 使用 `D:\wj\My-wj\MyLogin\license.lic` 作为开发期复制源，运行时统一放到 `Resources/license.lic`
  - [x] 默认登录地址设置为 `https://myscan.meyerop.com/login`
  - [x] Release 输出目录复制登录 DLL、qm、许可文件和间接依赖闭包
  - [x] `MeyerLoginTest.exe --smoke` 返回 0
  - [x] MainExe 中可调用登录模块并在登录成功后进入 HomeUI/CaseUI 链路
  - [ ] 后续新增 `LoginAdapter`，隔离外部登录头文件编码/字段变化风险
  - [ ] 后续接入 ConfigCenter/Permission 后，将登录参数、语言、AppPath、URL 等改为配置驱动

### 3.4 集成扫码验证 — ⏭️ 跳过（已有）

- **模块**：MeyerScan_QRCodeAuthEntry.dll
- **状态**：已开发完毕，待集成
- **集成工作**：
  - [ ] 编写对外接口头文件，注释清楚用法和适用场景
  - [ ] 在主程序插件清单中注册
  - [ ] 与 EngineeringSettings 的调用处预留接口声明

### 3.5 开发第三方拉起适配 — 🟡 初版已完成

- **模块**：MeyerScan_ExternalLaunchAdapter.dll
- **代码位置**：`f:\MeyerScan\MyExternalLaunchAdapter\`
- **功能要点**：
  - [x] 支持命令行模拟第三方软件拉起 MeyerScan：`--external-order <json>` 和 `--external-order-type <type>`
  - [x] 接收患者/订单信息上下文，并输出标准建单 JSON
  - [x] 标准上下文包含 `source.thirdPartyType`、`thirdPartyName`、`sourceSystem`、`sourceVersion`
  - [x] MainExe 可后台准备首页创建入口并直接显示 OrderScanWorkspaceShell/OrderCreateUI，避免首页闪现
  - [x] `ExternalLaunchAdapterTest.exe` 和 MainExe `--smoke-external-order` 已覆盖初版链路
  - [ ] 后续按不同第三方类型补充字段映射、校验规则和错误提示
  - [ ] 正式接入 OrderWorkflowService、登录态/离线许可判断和真实保存/扫描流程
- **前置任务**：1.5 CaseOrderService、1.8 OrderWorkflowService、2.6 OrderCreateUI
- **后续任务**：主链路联调

### 3.6 开发 HIS/Worklist 适配 — 🔴 未开始

- **模块**：MeyerScan_HisWorklistAdapter.dll
- **功能要点**：
  - [ ] HIS/Worklist 患者信息查询
  - [ ] 下拉患者信息建单
  - [ ] 字段映射和连接状态展示
  - [ ] 适配公立医院/大型连锁差异化接口
- **前置任务**：2.6 OrderCreateUI、1.5/1.6 病例域服务
- **后续任务**：主链路联调

### 3.7 开发数据导入导出 — 🔴 未开始

- **模块**：MeyerScan_DataExport.dll
- **功能要点**：
  - [ ] 患者/订单/扫描数据导入导出
  - [ ] 打包和导出路径策略
  - [ ] 通过 Service 获取业务数据，不直接拼数据库语义
- **前置任务**：CaseOrderService、ScanDataIO
- **后续任务**：云端上传、发送界面

### 3.8 开发统计服务 — 🔴 未开始

- **模块**：MeyerScan_Statistics.dll
- **功能要点**：
  - [ ] 患者/订单/设备使用统计
  - [ ] 查询聚合和报表数据准备
  - [ ] 不修改业务状态
- **前置任务**：CaseOrderService、Database
- **后续任务**：CaseUI/EngineeringSettings 展示

### 3.9 自动更新模块规划 — 🔴 未开始

- **模块**：MyUpdate.exe（与 MeyerScan.exe 同级）
- **功能要点**：
  - [ ] MeyerScan.exe 点击“检查更新”时生成 `myLocalUpdate.json/xml` 并启动 MyUpdate.exe
  - [ ] 支持用户双击 MyUpdate.exe 主动检查更新
  - [ ] 拉取 `MyCloudUpdate.json/xml` 并比较版本、硬件、驱动、账号/设备白名单
  - [ ] 下载补丁包、关闭 MeyerScan.exe、覆盖升级、重新启动 MeyerScan.exe
- **前置任务**：ConfigCenter、NetworkHelper、版本清单规范
- **后续任务**：第六阶段打包/交付

---

## 第四阶段：设备 + 校准 + 扫描 IO

> **目标**：开发校准模块，集成设备相关模块，开发扫描数据 IO
> **依赖**：前两阶段框架就绪
> **当前状态**：🟡 进行中（三维校准 UI 和颜色校准 UI 骨架已完成；设备传输/命令为既有模块待集成，ScanDataIO 未开始）

### 4.1 开发三维校准 UI 模块 — 🟡 骨架已完成

- **模块**：MeyerScan_Calibration3DUI.dll
- **代码位置**：`f:\MeyerScan\MyCalibration3DUI\`
- **功能要点**：
  - [x] 三维校准 Qt Widgets 界面骨架
  - [x] 模块接口、版本资源、README/CHANGELOG
  - [x] 初始化日志路径由调用方传入，不使用当前工作目录
  - [ ] 标定器连接流程
  - [ ] 25 幅采集编排
  - [ ] 三维校准计算入口
  - [ ] 结果展示、错误码和日志
  - [ ] 调用算法 DLL、DeviceCmd、DeviceTransport
- **前置任务**：2.1 ConfigCenter、2.3 UIComponents、4.3 DeviceTransport、4.4 DeviceCmd
- **后续任务**：第五阶段扫描重建
- **备注**：本模块是 Qt 校准界面模块，可以使用 Qt Widgets、Qt Layout、信号槽和 QString；算法、设备重资源和跨进程同步通过清晰接口隔离。

### 4.2 开发颜色校准 UI 模块 — 🟡 骨架已完成

- **模块**：MeyerScan_CalibrationColorUI.dll
- **代码位置**：`f:\MeyerScan\MyCalibrationColorUI\`
- **功能要点**：
  - [x] 颜色校准 Qt Widgets 界面骨架
  - [x] 模块接口、版本资源、README/CHANGELOG
  - [x] 初始化日志路径由调用方传入，不使用当前工作目录
  - [ ] 颜色标定器流程
  - [ ] 颜色采集编排
  - [ ] 颜色校正参数生成入口
  - [ ] 结果展示、错误码和日志
  - [ ] 调用算法 DLL、DeviceCmd、DeviceTransport
- **前置任务**：2.1 ConfigCenter、2.3 UIComponents、4.3 DeviceTransport、4.4 DeviceCmd
- **后续任务**：第五阶段扫描重建
- **备注**：本模块是 Qt 校准界面模块，可以使用 Qt Widgets、Qt Layout、信号槽和 QString；算法、设备重资源和跨进程同步通过清晰接口隔离。

### 4.3 集成设备传输层 — ⏭️ 跳过（已有）

- **模块**：MeyerScan_DeviceTransport.dll
- **状态**：已开发完毕，待集成
- **集成工作**：
  - [ ] 编写对外接口头文件
  - [ ] 在主程序插件清单中注册
  - [ ] 包含下位机升级功能

### 4.4 集成设备命令层 — ⏭️ 跳过（已有）

- **模块**：MeyerScan_DeviceCmd.dll
- **状态**：已开发完毕，待集成
- **集成工作**：
  - [ ] 编写对外接口头文件
  - [ ] 在主程序插件清单中注册

### 4.5 开发扫描数据 IO — 🔴 未开始

- **模块**：MeyerScan_ScanDataIO.dll
- **功能要点**：
  - [ ] 扫描数据读写与本地存盘
  - [ ] 不同设备型号专属数据格式解析
  - [ ] 数据校验与完整性检查
- **前置任务**：1.1 项目框架
- **后续任务**：5.7 ScanReconstructStudio 壳和 5.8 真实流程集成

---

## 第五阶段：扫描重建独立进程

> **目标**：开发扫描重建独立 EXE 及相关模块
> **依赖**：前四阶段完成
> **当前状态**：🟡 进行中（2026-07-06 已完成独立壳和两个阶段 UI DLL 初版，真实 IPC/设备/算法未完成）

### 5.1 ScanReconstructStudio 独立进程 IPC — 🔴 未开始

- **模块**：先放在 ScanReconstructStudio IPC 子目录；出现第二个真实使用方后再评估独立小库。
- **优化说明**：DLL 嵌入形态使用进程内 C ABI/UTF-8 JSON，不需要 IPC；只有 `ScanReconstructStudio.exe` 独立进程形态使用本通信层，不并入通用 Core 大包。
- **功能要点**：
  - [ ] Qt 命名管道封装（`NamedPipeServer` / `NamedPipeClient`）
  - [ ] POD 消息头序列化/反序列化（`PodSerializer` / `PodDeserializer`）
  - [ ] UTF-8 JSON payload 或上下文文件路径传递患者/订单上下文
  - [ ] 状态检测协议（Ping/Pong 仅用于在线状态和日志，不把自动重启作为重点目标）
  - [ ] 命令/响应模型（`IpcCommand` / `IpcResponse`）
- **前置任务**：ScanReconstructStudio EXE 启动/退出边界、订单上下文 JSON 合同
- **后续任务**：5.7 ScanReconstructStudio 壳和 5.8 真实流程集成

### 5.2 开发图像预处理 DLL — 🔴 未开始

- **模块**：MeyerScan_ScanDataPreProcess.dll
- **功能要点**：
  - [ ] 数据解密
  - [ ] 数据镜像
  - [ ] 图像顺序调整/裁剪
  - [ ] 颜色校准预处理
  - [ ] AI 软组织消去
  - [ ] AI 手套色去除
- **前置任务**：4.4 ScanDataIO
- **后续任务**：5.7 ScanReconstructStudio 壳和 5.8 真实流程集成
- **备注**：便于扩展到不同设备机型

### 5.3 开发扫描阶段 UI — 🟡 初版增强完成（v0.2.3）

- **模块**：`MeyerScan_ScanWorkflowUI.dll`
- **代码位置**：`f:\MeyerScan\MyScanWorkflowUI\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案、DLL 工程、测试宿主、CMakeLists、README、CHANGELOG
  - [x] 提供扫描对象选择、右侧扫描工具、底部扫描控制、提示区和 QVTK 显示占位
  - [x] 提供稳定动作 ID 回调，后续由 ScanReconstructStudio 编排扫描阶段动作
  - [x] 提供 `DeactivateAndRelease()`，离开页面时释放 QVTKWidget、VTK renderer、OpenGL/显存等重资源
  - [x] 当前已被 MainExe 阶段性动态加载并挂入 `OrderScanWorkspaceShell` 的 Scan 步骤，用于创建/练习流程先跑通；页面最小尺寸已收敛到 960x600
  - [x] 顶部扫描流程按钮改为读取 session JSON 的 `scanProcess.steps`；没有流程时回退练习默认流程
  - [x] 顶部扫描流程按钮支持手型 hover、tooltip、点击切换当前扫描部位显示数据和选中态刷新
  - [x] QVTK 显示区滚轮缩放以鼠标位置为中心，并在范围内夹紧缩放值，避免越界后拉回
  - [x] `ScanWorkflowUITest.exe` 补充自定义 `scanProcess.steps` 渲染、tooltip、手型光标和点击回调断言
  - [ ] 接入真实设备连接、采集数据、算法重建结果刷新
- **前置任务**：ScanReconstructStudio 壳、VTK/QVTK/OpenCV 开发环境
- **后续任务**：5.5 扫描采集/三维显示能力、5.8 真实设备与算法集成
- **验证结果**：
  - [x] `MeyerScan_ScanWorkflowUI.sln` Release x64 构建通过
  - [x] `ScanWorkflowUITest.exe` 返回 0，验证 DLL 工厂、初始化、根控件、对象名、流程按钮交互和资源释放链路
  - [x] MainExe `--smoke-main` 覆盖创建工作台、练习工作台、Scan 步骤懒加载/释放和后续 Send 链路
  - [x] CMake 根聚合 Release 构建通过，使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器验证

### 5.4 开发数据处理阶段 UI — 🟡 初版增强完成（v0.2.3）

- **模块**：`MeyerScan_DataProcessUI.dll`
- **代码位置**：`f:\MeyerScan\MyDataProcessUI\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案、DLL 工程、测试宿主、CMakeLists、README、CHANGELOG
  - [x] 提供模型选择栏、右侧处理工具栏、独立 Process Hint 提示框、底部状态栏和 QVTK 显示占位
  - [x] 提供截图、编辑、颈缘、倒凹、色彩、测量等处理入口
  - [x] 提供稳定动作 ID 回调和 `DeactivateAndRelease()` 重资源释放接口
  - [x] 当前已被 MainExe 阶段性动态加载并挂入 `OrderScanWorkspaceShell` 的 Process 步骤，用于创建/练习流程先跑通；页面最小尺寸已收敛到 960x600
  - [x] 顶部处理流程按钮改为读取同一份 session JSON 的 `scanProcess.steps`，确保 Scan/Process 页面一致
  - [x] 顶部处理流程按钮支持手型 hover、tooltip、点击切换当前处理部位显示数据和选中态刷新
  - [x] Process 页不放扫描页底部中间的 `Start / Pause`，左下角提示框内容与 Scan 独立
  - [x] QVTK 显示区滚轮缩放以鼠标位置为中心，并在范围内夹紧缩放值，避免越界后拉回
  - [x] `DataProcessUITest.exe` 补充自定义 `scanProcess.steps` 渲染、tooltip、手型光标和点击回调断言
  - [ ] 接入真实编辑、测量、颈缘、倒凹、咬合、底座等处理 DLL
- **前置任务**：ScanReconstructStudio 壳、VTK/QVTK/OpenCV 开发环境
- **后续任务**：5.6 数据处理工具模块
- **验证结果**：
  - [x] `MeyerScan_DataProcessUI.sln` Release x64 构建通过
  - [x] `DataProcessUITest.exe` 返回 0，验证 DLL 工厂、初始化、根控件、对象名、流程按钮交互和资源释放链路
  - [x] MainExe `--smoke-main` 覆盖创建工作台、练习工作台、Process 步骤懒加载/释放和后续 Send 链路
  - [x] CMake 根聚合 Release 构建通过，使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器验证

### 5.5 开发扫描采集与三维显示能力 — 🔴 未开始

- **模块**：ScanReconstruct/ScanCapture（先作为源码子目录，必要时再 DLL 化）
- **功能要点**：
  - [ ] 上颌/下颌/咬合扫描流程
  - [ ] 开始、暂停、继续、完成、删除控制
  - [ ] 数据补扫、可续扫时间策略
  - [ ] 与 DeviceCmd.dll、ScanDataIO.dll 对接
- **前置任务**：4.2 DeviceTransport、4.3 DeviceCmd、4.4 ScanDataIO
- **后续任务**：5.8 ScanReconstructStudio 真实流程集成

### 5.6 开发数据处理工具子模块 — 🔴 未开始

- **模块**：ScanReconstruct/EditTools、ProcessingTools（可按工具再拆 DLL）
- **功能要点**：
  - [ ] 笔刷/圈选删除、翻转、数据锁定、挖孔
  - [ ] 颈缘线：手动/半自动、保存/导入 .xyz
  - [ ] 测量：距离、角度
  - [ ] 倒凹：手动/自动计算、梯度色标
  - [ ] 咬合分析：距离计算、开合、六视图、剖面线
  - [ ] 色彩模式与自动补洞标记
  - [ ] 底座：裁剪平面、自定义高度、添底、保存 STL
- **前置任务**：5.2 ScanDataPreProcess、5.3 ScanWorkflowUI、5.4 DataProcessUI、5.5 扫描采集与三维显示能力
- **后续任务**：5.8 ScanReconstructStudio 真实流程集成

### 5.7 开发扫描重建工作台壳 — 🟡 初版骨架已完成

- **模块**：`ScanReconstructStudio.exe`（独立 EXE，项目目录 `f:\MeyerScan\MyScanReconstructStudio\`）
- **功能要点**：
  - [x] 新增独立 EXE 壳，输出 `ScanReconstructStudio.exe`
  - [x] 通过 `QLibrary` 动态加载 `MeyerScan_ScanWorkflowUI.dll` 与 `MeyerScan_DataProcessUI.dll`
  - [x] 承载“扫描”和“数据处理”两个大阶段，并保持统一导航和阶段切换
  - [x] 阶段切换前调用离开模块的 `DeactivateAndRelease()`，释放 QVTK/VTK/OpenGL 等重资源
  - [ ] 根据案例管理传递的订单信息创建不同扫描流程
  - [ ] 编排采集数据 → 图像预处理 → 算法重建 → 结果展示
  - [ ] 集成 ScanCapture、ModelViewer、EditTools、ProcessingTools
  - [ ] 集成算法后处理 4 个 DLL
  - [ ] 集成 AI 模型
  - [ ] 订单上下文、扫描状态、处理进度同步
  - [ ] 异常退出后的日志记录和订单状态修复
- **前置任务**：5.1 IPC、5.2 ScanDataPreProcess、5.3 ScanWorkflowUI、5.4 DataProcessUI、5.5 扫描采集与三维显示能力、5.6 数据处理工具、1.5 CaseOrderService、1.7 ScanSchemaService
- **后续任务**：第六阶段联调
- **验证结果**：
  - [x] `MeyerScan_ScanReconstructStudio.sln` Release x64 构建通过
  - [x] `ScanReconstructStudio.exe --smoke` 返回 0，验证动态加载、页面创建、扫描/处理切换和资源释放链路
  - [x] CMake 根聚合 Release 构建通过，使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器验证
- **备注**：当前初版只是壳和 UI 阶段切换，不代表真实扫描、重建、处理算法或 IPC 已完成；练习入口当前已可通过 MainExe 进入 OrderScanWorkspaceShell 练习模式，后续真实扫描再切换为 ScanReconstructStudio.exe 独立进程承载。

### 5.7.1 开发发送 UI — 🟡 初版框架已完成（v0.1.0）

- **模块**：`MeyerScan_SendUI.dll`
- **代码位置**：`f:\MeyerScan\MySendUI\`
- **功能要点**：
  - [x] 新增 VS2015 解决方案、DLL 工程、测试宿主、CMakeLists、README、CHANGELOG 和 Version.rc
  - [x] 提供 `ISendUI` 接口，支持 `Init()`、`CreateWidget()`、`SetSessionContextJson()`、`SetActionCallback()`、`Shutdown()` 和版本查询
  - [x] 页面包含案例信息区、数据格式下拉占位、备注区、本地 Export / Compress、技工所 Email Send / Upload，以及 Previous / Finish 流程按钮
  - [x] 通用按钮、输入框、下拉框和字段标签优先通过 `MeyerScan_UIComponents.dll` 创建；缺失时使用本地降级样式
  - [x] 动作只通过稳定整数 ID 回调给 MainExe；真实导出、压缩、邮件发送、上传后续由 DataExport / Network / Workflow 等服务模块承接
  - [x] MainExe 已阶段性懒加载 SendUI，并在创建模式 Process 下一步后进入 Send 步骤；练习模式不进入 Send
- **前置任务**：OrderScanWorkspaceShell、OrderCreateUI、ScanWorkflowUI、DataProcessUI、MainExe 动态加载框架
- **后续任务**：DataExport、NetworkHelper、OrderWorkflowService 发送前规则复核和真实发送流程
- **验证结果**：
  - [x] `MeyerScan_SendUI.dll` 和 `SendUITest.exe` CMake/VS2015 Release 构建通过
  - [x] `SendUITest.exe` 返回 0，验证模块初始化、上下文字段填充和 Finish 按钮存在性
  - [x] `MeyerScan.exe --smoke-main` 返回 0，覆盖创建工作台 Scan → Process → Send 阶段性链路

### 5.8 集成算法后处理 DLL — 🔴 未开始

- **模块**：Algorithm/（4 个 DLL）
- **状态**：算法侧提供
- **集成工作**：
  - [ ] 将算法 DLL 放置于 ScanReconstructStudio/Algorithm/ 目录
  - [ ] 通过配置文件加载算法 DLL

---

## 第六阶段：联调测试与交付

> **目标**：逐步集成测试，修复问题，完善文档并验收交付
> **依赖**：前五阶段完成
> **当前状态**：🔴 未开始

| 序号 | 任务 | 状态 | 备注 |
|------|------|------|------|
| 6.1 | 模块内单元测试 | 🔴 | 各模块独立运行验证 |
| 6.2 | 模块间联调 | 🔴 | DLL 间调用验证 |
| 6.3 | 跨进程联调 | 🔴 | 主程序 ↔ ScanReconstructStudio IPC 通信、订单上下文、状态同步、异常记录 |
| 6.4 | 整机功能测试 | 🔴 | 病例管理/扫描重建/云端上传/设备联动 |
| 6.5 | 异常与稳定性测试 | 🔴 | 崩溃/网络中断/设备离线/操作审计/数据追溯/权限管控 |
| 6.6 | 安装打包模块 | 🔴 | MyInstaller/Packaging，生成安装包、自定义安装界面、自定义安装流程、安装目录层级、依赖和版本清单核对 |
| 6.7 | 文档与验收 | 🔴 | 用户手册/开发手册/维护手册 |

### 6.6 安装打包模块 — 🔴 未开始

- **模块**：MyInstaller / Packaging（发布交付模块，暂定名称）
- **功能要点**：
  - [ ] 收集 `MeyerScan.exe`、`MyUpdate.exe`、`ScanReconstructStudio.exe`、插件 DLL、Qt 运行库、Qt 插件、配置模板、资源、帮助文档和打包阶段 `version_manifest.json`
  - [ ] 生成正式安装包
  - [ ] 支持安装过程中的自定义界面显示：品牌、语言、许可、路径、组件、进度、完成页等
  - [ ] 支持自定义安装流程：安装前检查、旧版本检测、目录创建、文件复制、快捷方式、安装后初始化、卸载入口、失败提示
  - [ ] 固化安装后文件夹层级：`bin/`、`plugins/`、`ScanReconstructStudio/`、`platforms/`、`sqldrivers/`、`config/`、`resources/`、`docs/`、`data/`、`logs/`、`updates/`
  - [ ] 打包前核对 EXE/DLL/Qt 运行库版本，禁止来源不明文件进入安装包
- **前置任务**：版本清单规范、主 EXE、MyUpdate、ScanReconstructStudio、插件目录结构稳定
- **后续任务**：验收交付、自动更新补丁包生成策略
- **说明**：当前只纳入整体方案和模块拆分，安装器技术选型、脚本结构、安装 UI 细节和回滚策略后续实现时再详细设计。

---

## 当前任务

> **当前在做的任务**：🟡 UI/工程边界和失败合同复核完成，转入真实病例订单主链路 — 已确认 MainExe 无边框全屏单内容区、WorkspaceShell 唯一步骤导航、UIResources 统一资源 DLL、ScanReconstructStudio DLL/EXE 双形态、Init/上下文返回值检查和 24 文件版本清单；下一步不继续增加占位壳。
> **接下来的任务**：
> 1. 抽出 `LoginAdapter`，隔离既有登录头文件和参数结构变化风险
> 2. 完善 CaseOrderService / ScanSchemaService，让 OrderCreateUI 保存、CaseUI 搜索分页/删除/打开订单正式走 Service/Workflow；患者订单合同继续使用服务 DTO + 版本化 JSON
> 3. 开发 OrderWorkflowService，统一建单、加载订单、练习扫描和进入扫描重建的规则
> 4. 扩展 UIComponents 的 ScreenUtil、DpiUtil、LayoutRules、Common qm、公共弹窗和复杂表格能力，并增加 QSS/tr/path/version 自动检查
> 5. 只为 ScanReconstructStudio 独立 EXE 形态补最小 IPC，再接设备采集、算法重建和数据处理工具 DLL；DLL 嵌入形态不重复建设 IPC

---

## 通用约束（各阶段均适用）

| 约束 | 说明 | 适用阶段 |
|------|------|---------|
| **版本资源文件** | 每个开发中的 EXE/DLL 必须同步创建 `Version.rc` 并加入 vcxproj | 各阶段 |
| **版本号一致性** | `Version.rc` 的版本号必须与 `ModuleInfo::Version`、业务接口 `GetModuleVersion()` 和统一导出函数 `GetMeyerModuleVersion()` 返回值一致 | 各阶段 |
| **版本清单** | 正式构建/打包前生成 `version_manifest.json` 发布清单；MainExe 运行时继续读取 `version_modules.json` 并只写拆分模块到 `logs/versionList`，同时记录 `fileVersion`、`codeVersion`、`versionMatch` | 第六阶段 / MainExe |
| **运行路径** | 日志、配置、图标、qm、版本清单必须基于 `QCoreApplication::applicationDirPath()` 或 ConfigCenter 安装目录，禁止用 `QDir::currentPath()` | 各阶段 |
| **运行资源目录** | 登录离线许可放入 `Resources/license.lic`；UI PNG/QSS/qm 源码归各模块 `Resources`，正式发布编译进 `MeyerScan_UIResources.dll` 并通过 `:/MeyerScan/Modules/<ProjectName>/...` 访问；不在客户目录展开 UI 散文件 | 各阶段 |
| **脚本兼容性** | PowerShell 脚本兼容 Windows PowerShell 5.1，使用带 BOM UTF-8；复杂引号正则拆分；原生工具检查正确退出码；删除前验证路径边界；详见 `PowerShell开发与自动化脚本规范.md` | 各阶段 |
| **分辨率适配** | 不以 1920x1080 绝对坐标等比缩放作为主方案；页面必须使用 Qt Layout、sizePolicy、最小/最大宽度和文本自适应，缩放系数只用于图标、边距、间距等辅助尺寸 | 所有 UI 模块 |
| **多语言布局** | 禁止按语言写 if/else 调整控件位置和大小；翻译变长由布局、弹性空间、换行、省略号或 tooltip 消化；界面可见文字统一写 `tr("English source text")`，不在源码写中文 UI source text | 所有 UI 模块 |
| **共享 UI 控件** | 通用按钮、字段标签、输入框、下拉框、日期框、多行文本框、表格样式、公共弹窗、等待页等进入 UIComponents；按钮按角色和内容布局统一；公共虚接口新增方法只能追加到接口末尾；特殊/单模块业务控件留在自身模块 | 所有 UI 模块 |
| **设置占位路径** | SettingsUI 骨架期可使用 `QStandardPaths::DocumentsLocation` 生成用户目录占位路径，不允许在界面中显示开发机 `D:/`、`F:/MeyerScan` 等固定路径 | SettingsUI / 所有 UI 模块 |
| **资源释放** | 按资源重量决定缓存或释放：轻量页面可短期复用保证切换顺滑，重资源页面离开时必须释放 widget、暂停线程、释放显存/大内存；从案例管理进入扫描重建前必须释放 CaseUI widget，不能只隐藏 | 所有 UI/扫描模块 |
| **单实例** | MeyerScan.exe 同时只允许运行一个实例；重复启动只激活已显示主窗口，数据库检查或登录阶段不强制打断 | MainExe |
| **启动等待页** | 启动检查由 MainExe 编排，等待页由 UIComponents 提供展示，不承载检查逻辑 | MainExe/UIComponents |
| **运行期路径回退** | 运行参数不得回退到开发机绝对路径；测试宿主也必须从 exe 所在目录推导日志、配置、许可等路径 | 各阶段 |
| **全局基础设施生命周期** | UI 模块只借用进程级 Logger；正式 UI 不直接访问 Database，也不做数据库健康检查。Database 由 MainExe 通过 DatabaseQtAdapter 统一初始化和收尾，测试宿主可在自身进程内经 Adapter 准备演示数据 | UI 模块/MainExe/测试宿主 |
| **工程一致性** | 新模块必须补齐 `MEYER_MODULE_NAME`、规范 `Version.rc`、中文 README/CHANGELOG、测试入口和接口债务 TODO | 各阶段 |
| **模块信息一致性** | 模块内部维护 `ModuleInfo::Name` / `ModuleInfo::Version` 或等价结构；日志名、`MEYER_MODULE_NAME`、`Version.rc`、`GetModuleVersion()`、`GetMeyerModuleVersion()` 必须一致 | 各阶段 |
| **自研 DLL 动态加载** | MainExe 对自研功能/支撑 DLL 优先使用 `QLibrary + extern "C" GetXxx()` 动态加载；主程序只保留接口头文件，不链接这些模块 import lib | MainExe / 插件 DLL |
| **CMake 双构建入口** | 每个模块和测试宿主必须有 `CMakeLists.txt`；根目录 `F:\MeyerScan\CMakeLists.txt` 聚合当前模块；VS2015 `.sln/.vcxproj` 同步保留 | 各阶段 |
| **本地整体备份** | GitHub 提交之外，必须通过 `tools/BackupToLocalRepository.ps1` 把除第三方库外的全部模块源码、测试项目、工程文件、CMake、文档、配置模板和自研 DLL/EXE/LIB 备份到 `F:\MeyerScan-Reposit`；备份提交日志使用中文且写清内容。Qt/VC/UCRT/OpenSSL/AWS/MySQL/SQLiteStudio/SQL 驱动、日志、数据库现场文件、IDE 临时文件必须过滤 | 各阶段 |
| **非界面模块 Qt 边界** | 新增非界面模块优先不用 Qt；已有 Qt 非界面模块把 Qt 限制在 `.cpp` 私有实现，公共 ABI 不暴露 Qt 类型 | 基础设施/服务模块 |
| **扫描重建处理分层** | ScanReconstructStudio.exe 只做 UI/交互/流程编排；编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等数据处理能力优先拆 DLL 或独立库 | 第五阶段 |

## 阶段检查项（每阶段结束前必须检查）

| 检查项 | 要求 | 目的 |
|--------|------|------|
| **边界检查** | 模块职责、禁止事项、依赖方向与架构文档一致 | 防止后期维护时出现隐形耦合 |
| **构建检查** | VS2015 Release x64、VSCode MSBuild 至少通过一种自动命令验证；正式阶段两者都要通过 | 保证团队成员可在不同环境继续开发 |
| **CMake 检查** | 模块目录和根目录 CMakeLists 存在；有 CMake 环境时需执行 configure/build；没有 CMake 环境时需记录无法运行原因 | 保证 VSCode/CMake 路线不退化成摆设 |
| **测试入口** | DLL 必须有测试宿主、smoke 或最小验证程序；独立 EXE 必须有启动/退出/异常路径测试 | 降低碎片化开发调试成本 |
| **日志检查** | 客户每一步可见操作和关键成功/失败路径写结构化日志，模块名、operation/actionId/pageName 明确；日志字段 key、内部错误 key 可保持稳定英文 | 便于开发调试、程序处理、多语言展示和现场问题复盘 |
| **权限检查** | 高价值动作必须可被 Permission/Workflow/Service/IPC 至少两层复核 | 防止只靠 UI 隐藏导致绕过 |
| **版本检查** | EXE/DLL 包含 Version.rc，版本号与 GetModuleVersion 一致 | 防止安装包和现场环境版本混乱 |
| **工程配置检查** | `CompanyName`、`ProductName`、`FileDescription`、`FILEOS/FILETYPE/FILEFLAGS`、`MEYER_MODULE_NAME` 与规范一致 | 防止文件属性、日志模块名和版本清单长期漂移 |
| **文档检查** | 接口、README、进度、AI 协作记录同步更新 | 保证后续人工阅读和接手成本低 |
| **恢复检查** | 配置、数据库迁移、插件替换、扫描进程异常均要有回滚或降级策略 | 保证产品异常后可恢复、可定位 |

---

## 任务变更记录

| 日期 | 变更内容 | 原因 |
|------|----------|------|
| 2026-06-11 | 创建进度跟踪文档 | 重构启动，文档准备阶段 |
| 2026-06-12 | CaseEntity 改为 .lib；Permission 合并 PermissionUI；IPC 归入 Core.lib | 基于优化建议，减少 DLL 数量，降低管理成本 |
| 2026-06-12 | 第二阶段模块从 6 个减为 5 个（合并 PermissionUI） | 架构优化 |
| 2026-06-12 | 文档合并：任务总纲 + 优化建议 → 统一为"总体方案与详细设计"；Logger 设计调整（永久保留/不脱敏/QString+std::string双接口） | 减少文档维护成本；匹配实际需求 |
| 2026-06-12 | Logger.dll 核心实现完成（13 个文件，含测试程序，全部中文注释） | 第一阶段 1.2 日志模块已可独立编译运行 |
| 2026-06-15 | Logger.dll 双构建环境搭建完成（VS2015 + VSCode MSBuild）+ GitHub 推送；新增构建约束章节到方案文档 | 1.2 日志模块成为后续模块的项目结构模板；源码编码/编译问题修复 |
| 2026-06-15 | GitHub 仓库创建：`weijian118/MeyerScan`，`MyLogger/` 作为第一个子模块推送 | 仓库结构 `MeyerScan/ModuleName/` 支持后续多模块并行开发 |
| 2026-06-16 | 创建《架构设计与接口规范》文档；生成 10 个模块的接口头文件（ErrorCode.h、Result.h、IpcCommand.h、CaseEntity.h、ConfigCenter.h、Crypto.h、Permission.h、CaseManager.h、DeviceCmd.h、ScanDataIO.h、NetworkHelper.h） | 定义模块边界、接口规范、依赖关系、调用时序、注释规范、开发规约 |
| 2026-06-16 | 新增 Database.dll、HomeUI.dll、OrderCreateUI.dll 模块；新增首页和建单分离方案；当时业务理解修正为“患者+订单统一管理，扫描重建保持整体”。该“保持整体”是历史口径，2026-07-06 起扫描重建已改为独立壳 + 扫描阶段 UI DLL + 数据处理阶段 UI DLL | 基于当时用户反馈优化架构设计；保留历史时间线，同时以后续新口径为准 |
| 2026-06-16 | 文档合并：任务总纲+方案优化建议+架构优化方案+问答记录 → 合并为《重构任务总览》和《架构设计与接口规范》 | 减少文档冗余，便于维护 |
| 2026-06-16 | 修复 MyLogger 代码问题：Logger.h 中 QString Init() 重载缺少 level 参数 | 代码bug修复 |
| 2026-06-17 | **Database.dll v1.1.0 接口规范对齐**：所有接口改为 Result\<T\>/VoidResult；集成 Logger（动态加载）；CRT 规则文档化（/MT vs /MD）；MEYER_MODULE_NAME 预处理器定义；DatabaseTest CRT 改为 /MD | 对齐架构规范，统一返回值风格和日志规范 |
| 2026-06-17 | **模块职责调整**：EngineeringSettings 移除数据库切换和功能开关控制（分别移入病例域服务和 Permission）；Permission 内部冷热路径分离 | 精简职责，降低耦合 |
| 2026-06-17 | **新增开发约束**：CRT 链接规则（无 Qt 依赖用 /MT，有 Qt 依赖用 /MD）；注释政策（源码中文，日志 key 稳定，UI 用 tr）；Database 角色明确为通用基础设施（方案 A） | 补充架构文档遗漏内容 |
| 2026-06-17 | **新增版本资源规范**：每个 EXE/DLL 必须包含 Version.rc；打包时生成 version_manifest.json 版本清单；版本号一致性规则 | 标准化模块版本管理，便于售后排查和追溯 |
| 2026-06-17 | **细模块优先调整（历史口径）**：当时曾将病例域从单一 CaseManager 拆为 CaseService、OrderService、ScanSchemaService、OrderWorkflowService、DataExport、Statistics；PermissionConfigUI 独立；ScanReconstructStudio 内部按采集/显示/编辑/处理工具拆分 | 后续在 2026-06-23/24 修正：患者/订单强绑定，CaseService + OrderService 合并为 CaseOrderService |
| 2026-06-17 | **MyLogger/MyDatabase 审查修正**：Logger 增加 GetModuleVersion 和 Version.rc；Database 保留 Qt5Sql 实现并明确职责边界；修复 Database 动态加载 Logger 空路径 Init、SetDatabaseType 死锁风险、SQL 错误消息悬空指针风险；日志内容改为英文 | 对齐“管边界，不纠结 Qt”的设计原则，并修复实际运行风险 |
| 2026-06-17 | **MyDatabase 构建与测试收口**：修复 VoidResult 非法构造、VS2015 UTF-8 BOM 编码问题、DatabaseTest 解决方案依赖；Release x64 构建 0 warning/0 error；DatabaseTest 19 passed/0 failed | 代码状态从“已修正”推进到“已构建并运行验证” |
| 2026-06-17 | **重构方案再梳理**：明确 HomeUI 只做入口 UI；新增 OrderWorkflowService 承接新建/加载/继续扫描/发送规则；收窄 Core.lib 公共方法边界；补充六维权限流程、规则合并优先级、功能阉割三层闭环 | 防止 UI 规则膨胀、公共模块变大杂烩、只隐藏按钮导致权限绕过 |
| 2026-06-17 | **HomeUI/CaseUI 框架创建（历史口径）**：新增 `F:\MeyerScan\MyHomeUI` 和 `F:\MeyerScan\MyCaseUI`；Qt Widgets DLL + 测试宿主；当时曾运行时加载 Logger 并链接 Database；VS2015 Release x64 构建通过；smoke 自动运行通过。2026-07-03 起正式 UI 已移除 Database 直连 | 该行记录框架创建历史；当前 UI 模块加载和业务数据读取以 RuntimeDataCenter / Service / DatabaseQtAdapter 新链路为准 |
| 2026-06-17 | **HomeUI/CaseUI Qt 运行库收口**：两个 UI 项目 PostBuild 显式复制 Qt 5.6.3 的核心 DLL、平台插件和 SQL 驱动；确认输出目录 plugins 布局干净，并重新通过 HomeUITest/CaseUITest smoke | UI 必须使用 Qt 界面且能独立启动；运行库必须匹配编译版本，避免混用 SQLiteStudio 目录中的 Qt 5.7 DLL |
| 2026-06-17 | **GitHub 模块目录规则确认**：`MyHomeUI/`、`MyCaseUI/` 与 `MyLogger/`、`MyDatabase/` 同级，均作为 `weijian118/MeyerScan` 仓库下一级模块目录提交和记录；本地提交 `fe88c44` 已包含首页和案例管理模块，远端推送受网络重置影响待重试 | 统一模块存放和追踪规则，保证后续每个模块都能独立维护、独立记录、按目录审查 |
| 2026-06-18 | **最终方案再梳理口径修正**：不再围绕医疗字样和开发周期展开，聚焦产品稳定、代码实现、模块边界、调试成本和轻量化团队维护策略 | 在“模块足够小、代码量少、人工可维护”和“拒绝过度工程化、低调试成本”之间建立可执行标准 |
| 2026-06-18 | **HomeUI/CaseUI 适配与国际化预埋**：检查确认 `MyCaseManager\mysql.sql` 未被新模块使用，仅作为旧 schema 参考；HomeUI/CaseUI 测试宿主增加 High DPI 和当前屏幕居中逻辑；界面文字完成国际化预埋；Release x64 重新构建并 smoke 通过 | 明确数据库迁移不能硬编码旧脚本；多个 UI 模块的多屏/DPI 与多语言策略必须统一，后续迁入 UIComponents/LanguageManager |
| 2026-06-23 | **UI 文案 source text 规则补充**：HomeUI、CaseUI、MainExe 可见文字统一改为 `tr("English source text")`；需求写中文按钮名时，源码仍写英文源文案，中文显示交给模块 `.qm` | 避免中文 UI 文案散落源码，统一翻译上下文和多语言维护方式 |
| 2026-06-18 | **数据库调用边界补充**：确认 MyHomeUI/MyCaseUI 当前直连 MyDatabase 只用于 Init/Connect/smoke 健康检查；正式业务数据访问必须走 CaseOrderService/ScanSchemaService/OrderWorkflowService；补充缺口：ConfigCenter 连接配置、版本化 migration、Service 内部 DAO、schema 版本表和 CRUD/迁移测试 | 防止临时框架验证代码被误用为正式 UI 直连数据库模式 |
| 2026-06-22 | **功能清单与模块拆分补充（已修正）**：补充云端账号离线/联网登录、订单上传口扫云、第三方软件拉起建单、HIS/Worklist 建单、MyUpdate.exe 自动更新；当时记录的 Calibration3D.dll / ColorCalibration.dll 后续修正为 Calibration3DUI.dll / CalibrationColorUI.dll；弱化扫描进程自动重启，重点改为双 EXE 状态同步；CaseEntity 改为暂定契约层并记录字段高频变化风险；新增每模块 CHANGELOG 规则 | 补齐产品功能清单和模块拆分结果，降低后续扩展和人工维护成本 |
| 2026-06-24 | **Qt 默认使用与模块口径修正（历史口径，2026-07-02 已收紧）**：当时明确 CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、Database 和各 UI 模块都可以优先使用 Qt 默认能力；患者/订单强绑定统一走 CaseOrderService；校准拆为两个 Qt UI+流程+计算入口模块；新增 OrderScanWorkspaceShell 进度项。2026-07-02 评审后，非界面模块改为“能不用 Qt 就不用 Qt，已有 Qt 实现只作为内部细节”。 | 记录历史决策来源，同时以 2026-07-02 新评审规则作为后续开发准则 |
| 2026-06-24 | **校准 UI 骨架落地**：新增 MyCalibration3DUI 与 MyCalibrationColorUI 两个 VS2015/Qt Widgets DLL 骨架，补齐接口、版本资源、日志入口、README 和 CHANGELOG；真实标定器连接、采集编排、算法 DLL 和设备 DLL 接入后续继续开发 | 先固定三维校准与颜色校准两个独立维护边界，避免校准流程混入 MainExe 或 EngineeringSettings |
| 2026-06-22 | **记录语言规范补充**：源码注释、模块 `CHANGELOG.md`、GitHub commit message 统一使用中文；日志字段 key、内部错误 key、翻译 key 可保持稳定英文 | 防止后续提交和模块记录语言反复偏离，方便人工维护和审阅 |
| 2026-06-22 | **安装打包模块纳入方案**：新增 MyInstaller/Packaging 作为发布交付模块，负责安装包生成、自定义安装界面、自定义安装流程、安装目录层级、依赖收集和版本清单核对 | 把交付过程模块化，避免后期人工复制文件、目录混乱和安装包不可追溯 |
| 2026-06-22 | **MyLogin 与 MyMainExe 框架落地**：新增 `MyLogin/` 登录测试宿主和 `MyMainExe/` 主程序入口；MainExe 当前按 Logger → Database 健康检查 → Login → HomeUI → CaseUI 跑通最小链路；补齐登录模块间接依赖闭包；VS2015 Release x64 构建通过，三个 smoke 均返回 0 | 先把新架构主入口和既有登录模块接通，形成可运行集成骨架，为后续 ConfigCenter、Permission、Service/Workflow 接入做准备 |
| 2026-06-22 | **MyMainExe/minMain 复测与架构复核**：重新构建 `MeyerScan_MainExe.sln` 和 `MeyerScan_MyLogin.sln`，Release x64 均通过；`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerLoginTest.exe --smoke` 均返回 0；递归依赖检查非 API Set DLL 缺失为空 | 确认当前主入口仍符合“薄主 EXE + 模块编排”定位，下一步优先补 `LoginAdapter` 和配置驱动 |
| 2026-06-22 | **首页/浏览双向切换与操作日志补齐（历史旧实现）**：HomeUI 新增入口回调，首页“浏览”进入 CaseUI；CaseUI 新增“返回首页”按钮和操作回调；当时 MainExe 曾用 `QStackedWidget` 切换页面并记录 `PageSwitch`；补齐首页入口、浏览工具按钮、搜索、页签切换等客户操作日志；`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`CaseUITest.exe --smoke` 均返回 0。注意：该 `QStackedWidget` 并列页面方案已在 2026-06-25 废弃，后续开发必须以“MainExe 单内容区全屏替换 + 离开页面按资源规则释放”为准。 | 记录历史实现来源，同时明确旧页面方案不得继续沿用，避免后续开发误把首页和浏览当成并列兄弟页面 |
| 2026-06-23 | **多分辨率、多语言、路径、共享 UI、配置/权限、版本清单、等待页和单实例规则落地**：新增 MyConfigCenter、MyPermission、MyUIComponents、MyVersionManager 四个模块骨架并接入 MainExe；MainExe 基于应用目录管理 logs/config/versionList，启动时生成版本清单，等待页由 UIComponents 提供，单实例由主 EXE 控制；HomeUI/CaseUI 增加权限显隐入口；主页面改为按需创建并释放非当前轻量页面 widget | 回应当前阶段“模块多可以，但边界必须清楚、代码量小、低耦合、易人工维护”的要求，同时避免旧的绝对坐标缩放、currentPath、UI 互相切换和长期占用资源问题继续扩散 |
| 2026-06-23 | **复查补漏与边界收紧（历史口径）**：删除 MainExe 数据库配置开发路径回退；登录许可运行参数当时改为应用目录 `license.lic`，2026-06-25 又统一迁入 `Resources/license.lic`；HomeUI/CaseUI 测试宿主改为按 exe 所在目录推导路径；当时约束 UI 模块 Shutdown 不关闭进程级 Logger/Database；首页“设置”和浏览“返回首页”改为 ConfigCenter + Permission 同时允许才显示；Database `mysqlDataDir` 从 JSON 配置读取；UIComponents 等待页进度条按 DPI 辅助缩放。2026-07-03 起正式 UI 不再借用 Database | 对用户提出的 9 类点做第二轮检查，减少路径、生命周期、显隐控制和资源释放方面的遗漏 |
| 2026-06-23 | **复查补漏第三轮（历史口径）**：Database 配置相对路径按 `db_config.json` 所在目录解析，测试宿主生成独立测试配置并补齐当时 Qt/SQL 驱动复制；当时 HomeUI/CaseUI 曾优先借用已连接 Database；UIComponents 增加次按钮、输入框、下拉框基础工厂；重新构建并验证 DatabaseTest 21 passed / 0 failed、各 smoke 返回 0。2026-07-03 起正式 UI 已移除该借用链路 | 进一步消除运行路径、测试配置、基础设施生命周期和共享 UI 边界上的歧义；当前以 Database 纯 C++ + DatabaseQtAdapter 为准 |
| 2026-06-23 | **扫描前资源释放规则补充**：MainExe 预留扫描前准备流程；后续从案例管理进入 ScanReconstructStudio 前，必须先切到等待页并释放 CaseUI widget，不能只隐藏浏览模块；MainExe Release x64 构建通过，`--smoke-main`、`--smoke` 返回 0 | 保证案例管理的表格、缩略图、查询结果等资源不会长期占用内存/显存，为扫描重建让出资源 |
| 2026-06-23 | **全模块交互复测与 MainExe smoke 增强**：CaseUI `Open` 操作接入 MainExe 扫描前准备流程；`--smoke-main` 自动覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前释放 CaseUI；单实例激活改为登录完成且主窗口可见后才响应；MainExe Release x64 构建通过，`--smoke-main`、`--smoke` 返回 0 | 把“页面切换、资源释放、日志记录、单实例边界”纳入自动回归，避免只验证能启动而漏掉模块交互问题 |
| 2026-06-23 | **拆分模块总清单补充**：在架构规范 `2.4.1` 增加正式模块清单，逐项记录模块中文名、项目名/目录、DLL/EXE/LIB 名、形态、功能详情和边界注意事项；任务总览同步增加清单维护规则 | 让后续人工维护能直接查清“功能归属、项目命名、产物命名、模块边界”，减少模块命名漂移和职责重复 |
| 2026-06-23 | **任务总览同步完整模块清单**：`MeyerScan重构任务总览.md` 新增 `3.6 拆分模块总清单`，内容与架构规范清单保持一致，不再只保留索引和维护规则 | 用户阅读任务总览时即可直接看到模块中文名、项目名、产物名、形态、功能详情和边界说明 |
| 2026-06-24 | **MainExe 与配置/注释规则收口**：修复登录窗口显示后等待页残留；Logger 提前到 MainWindow 构造阶段初始化并缓存 `ILogger*`；Release 改为 Windows 子系统隐藏 CMD；运行时版本清单逻辑并入 MainExe；ConfigCenter 自动迁移清理旧 `startup.showWaitPage/singleInstance`；补充“每个函数必须中文注释、关键代码必须注释、测试项目同样适用、源码 UTF-8 BOM”规则；移除 VS2015 不兼容/无必要的 `LanguageStandard` 标签；补注释后将 UIComponents 等源码转为 UTF-8 with BOM 并重新验证 VS2015 构建 | 让启动链路更稳定、运行配置更清晰、VS2015 打开项目更稳，并降低后续人工阅读维护成本 |
| 2026-06-24 | **全模块注释覆盖复查**：复查 `F:\MeyerScan` 自研 `.h/.cpp`，补齐 MyLogin、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell、HomeUI、CaseUI、UIComponents、CaseOrderService、Database 的公共接口、实现函数、测试宿主和关键路径注释；模块 `CHANGELOG.md` 同步记录；第三方 MySQL SDK 和外部登录头文件不纳入自研注释修改范围 | 将“每个函数中文注释、关键代码说明原因、测试项目同样注释”的规则从文档落实到已开发模块，降低后续人工维护和新成员阅读成本 |
| 2026-06-24 | **函数体内部注释补强**：根据用户反馈继续补充函数体内部中文注释，不只保留函数头说明；重点补齐 HomeUI、CaseUI、CaseOrderService、Database、MainExe、测试宿主、Logger 和 OrderScanWorkspaceShell 中的关键判断、路径推导、Qt 父子关系、跨模块调用、失败分支、事务/锁、页面释放和日志写入说明；增加扫描检查，确认自研源码中未发现“大函数体无内部注释”或“8 行以上函数体内部注释少于 2 条”的候选 | 让注释真正服务人工阅读和维护，避免只满足形式上的函数头注释而仍需要读者猜生命周期、边界和设计原因 |
| 2026-06-24 | **MyLogger 按实际测试反馈重构**：日志文件规则改为每天默认 `MeyerScan_YYYYMMDD.log`，超过 10 MiB 才生成 `_001/_002` 分卷；日志格式省略空 `Dev/Case/Op` 字段，不再固定宽度补空格；删除后台缓冲线程和 `LogBuffer.*`，每条日志同步打开、追加、`FlushFileBuffers`、关闭句柄；明确模块 Init 阶段缓存 `ILogger* m_logger`，后续在该变量生命周期内持续 `m_logger->Write(...)`；Qt 模块可通过头文件便利层直接输出 `QString`，Logger.dll 本体仍不依赖 Qt；无真实操作员上下文的模块改为空字符串，不再写 `[Op:System]` | 更贴近现场维护和人工阅读需求：同一日志文件按调用顺序写入、单条写完即可移动/删除日志文件、格式更清爽、调用方式更短，同时保持基础设施模块轻量边界 |
| 2026-06-25 | **glm52 建议复核与工程一致性收口**：读取 `glm52` 下两份建议文档后，立即采纳低风险工程配置建议：统一各模块 `Version.rc` 的 `CompanyName`、`ProductName`、`FileDescription` 和命名常量；补齐缺失 `MEYER_MODULE_NAME`；`CaseOrderServiceResult` 增加 `IsSuccess()` / `IsError()`；ConfigCenter/Permission 头文件补充 Core.lib 后接口迁移 TODO；架构规范和任务总览新增工程一致性验收清单 | 接受能提升维护性且不破坏当前 ABI/主链路的建议；涉及 Core.lib、ErrorCode/Result 迁移、Logger 导出宏改名、UI 去 Database 直连等调用链变更先记录为后续集中迁移，避免当前阶段因形式统一导致已跑通模块不稳定 |
| 2026-06-25 | **MainExe/权限/版本清单/VS2015 聚合方案收口**：版本清单改为读取 `config/version_modules.json`，只记录拆分模块 EXE/DLL；新增 `version_modules.md`、`runtime_config.md`、`permission_rules.md` 说明文件；HomeUI/CaseUI 增加 `enabled` 接口并由 MainExe 下发和二次复核；MainExe 从 `QStackedWidget` 并列页面改为单内容区全屏替换；登录许可运行路径迁入 `Resources/license.lic`；新增 `F:\MeyerScan\MeyerScan_AllModules.sln` 聚合已拆分模块工程；MainExe/HomeUI/CaseUI 落地 `ModuleInfo` 规则 | 让版本清单可扩展且不混入第三方库，让权限 `enabled` 真正生效，让首页/浏览层级关系更符合入口流程，降低 VS2015 人工维护成本 |
| 2026-06-25 | **版本清单、日志字段和模块信息二次复查**：历史 `MyVersionManager` 也改为读取 `config/version_modules.json`，防止后续恢复独立 DLL 时误扫第三方库；Logger 日志正文标记统一为 `[Content:]`，并改用 UTF-8 转 UTF-16 后调用 Windows 宽字符文件 API，支持中文安装路径；Database、ConfigCenter、Permission、UIComponents、CaseOrderService、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell、Logger 等模块补齐 `ModuleInfo` 信息来源 | 把用户 13 点要求从 MainExe/HomeUI/CaseUI 扩展到已开发模块整体，减少“某个历史骨架以后被误用”的隐患 |
| 2026-06-25 | **13 点要求复测收口**：补充 `runtime_config.json`、`permission_rules.json` 默认模板和说明文件复制；MainExe PostBuild 补齐 CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI DLL；修正 DatabaseTest `libmysql.dll` 复制来源；LoggerTest 清理旧测试日志后再验证格式；单模块、聚合方案和 smoke 全部通过，MainExe 最新 versionList 只记录拆分模块且全部存在 | 把清单驱动、权限 enabled、Release 依赖闭包、VS2015 聚合打开和日志格式验证落到可运行结果 |
| 2026-06-26 | **UIComponents 标准控件样式第一步落地**：读取 `glm52` 代码修改记录和建议文档后，将通用控件样式收口到 MyUIComponents；新增按钮角色 Primary/Secondary/Text/Danger/Entry 和内容布局 TextOnly/IconOnly/IconLeftText/IconTopText；HomeUI 首页入口按钮接入 Entry 样式，CaseUI 顶部按钮和患者/订单工具栏按钮接入标准按钮样式；特殊/单模块控件仍留在自身模块，避免 UIComponents 膨胀成页面库；UIComponents/HomeUI/CaseUI/MainExe Release x64 构建通过，三个 smoke 均返回 0 | 回应“界面控件样式如何统一管理”的问题：共享样式统一，但业务动作、权限、页面结构仍留在各自模块 |
| 2026-06-26 | **文档与代码口径复查**：梳理 `D:\wj\重构文档` 和各模块 README/CHANGELOG，修正根 README 旧模块状态；HomeUI/CaseUI 版本从 v0.1.0 升为 v0.2.0，与 UIComponents 接入保持一致；SettingsUI General 页面占位路径改为基于 `QStandardPaths::DocumentsLocation` 派生，不再显示 `D:/SOFTSCANDATA` / `D:/SCANDATA` | 消除当前规范和代码/文档之间的小漂移，避免后续开发继续沿用旧路径、旧版本和旧模块清单 |
| 2026-06-26 | **日志格式契约和设置入口复查**：修正 Logger 实际输出中残留的历史 `[Txt:]` 标签，恢复为全局规范要求的 `[Content:]`；公开头文件示例改为当前 `MeyerScan_CaseOrderService` 命名和空操作员上下文写法；任务总览和架构规范中的当前启动/入口流程改为首页/浏览进入 `SettingsUI.dll`，工程设置继续作为高级维护规划入口 | 保证文档、代码、测试对日志字段和设置模块入口使用同一套当前口径，避免后续开发按旧示例继续扩散 |
| 2026-06-26 | **MyCaseManager 历史目录边界补文档**：为旧版 `MyCaseManager` 目录新增 README/CHANGELOG，明确其只作为旧数据库、旧 schema、SQLite 样例和 MySQL 目录参考；当前案例管理 UI 归 `MyCaseUI`，患者/订单/参考数据服务归 `MyCaseOrderService` | 防止后续维护人员把旧目录误当成当前重构模块入口，避免新代码继续依赖旧数据库包 |
| 2026-06-30 | **SQLite 默认链路与 RuntimeDataCenter 落地**：`MyDatabase` 默认配置切换为 SQLite，SQLite 首次连接自动创建 Data 目录；新增 `MyRuntimeDataCenter`，将本地诊所/技工所/软件/医生/设置/账号/患者/订单/设备和云端诊所信息包装成运行时 JSON 快照；MainExe 启动后初始化该模块，CaseUI 通过 `local.patients` / `local.orders` 读取列表；自研 Qt DLL 和插件复制来源统一到编译所用 Qt 5.6.3 目录，避免输出目录混用 Qt 5.6.2 / 5.6.3 | 用 JSON 快照吸收高频字段变化，避免 UI/主程序频繁改 ABI；同时让 VS2015 聚合构建后的根 `bin\Release\MeyerScan.exe --smoke-main` 与 MainExe 单模块输出行为一致 |
| 2026-06-30 | **数据库到 UI 链路测试补强**：CaseUITest 和 SettingsUITest 在空 SQLite 库中准备最小演示数据；CaseUI 验证患者/订单表格均有数据；SettingsUI Information 验证医生、诊所、技工所三张表均有数据；修复 SettingsUI 解析 RuntimeDataCenter JSON 时把整块预分配缓冲区交给 `QJsonDocument` 导致 invalid JSON 的问题 | 把“能启动窗口”升级为“数据库 -> RuntimeDataCenter -> UI 表格”真实链路验证，同时保持正式 UI 不建表、不迁移、不写业务数据 |
| 2026-07-01 | **RuntimeDataCenter 全域演示库补齐**：CaseUITest、SettingsUITest 的 SQLite 演示库补齐软件信息、设置、账号、设备最小表；RuntimeDataCenterTest 的设备表改为优先读取表 `device_info_tbl2`；MainExe 单模块和根聚合目录 smoke 日志均显示 `All runtime domains reloaded` | 测试宿主可以造最小演示数据，但正式 UI 仍不承担 schema 初始化；集成测试日志应尽量排除预期缺表噪声，方便发现真实问题 |
| 2026-07-01 | **需求与代码复核**：再次检查“CaseUI 显示患者/订单、SettingsUI 显示医生/诊所/技工所、空库测试宿主造演示数据”链路；确认正式 UI 源码没有建表/插入/删除等业务写入，写演示数据仅存在测试宿主；当时调整为先完成数据库健康检查再初始化 RuntimeDataCenter，2026-07-03 起该健康检查由测试宿主/MainExe 经 DatabaseQtAdapter 完成；`MeyerScan_AllModules.sln` Release x64 0 warning / 0 error，RuntimeDataCenterTest、CaseUITest、SettingsUITest、MainExe 单模块和根聚合 smoke 均返回 0 | 保持 UI 只读展示、数据写入归服务/迁移层的边界，同时让代码注释和当前 RuntimeDataCenter 方案一致，避免后续开发按旧 CaseOrderService 直填列表注释误走回头路 |
| 2026-07-01 | **重构规则与代码偏移复核**：按模块清单、路径规则、多语言规则、数据库边界、权限 visible/enabled、资源释放、版本清单和文档同步要求检查 `D:\wj\重构文档` 与 `F:\MeyerScan`；代码侧未发现正式 UI 写业务库、运行路径使用 `currentPath`、首页/浏览并列缓存、权限 enabled 不生效等偏移；优化 CaseUI/SettingsUI，不再在 UI 模块中主动 `RuntimeDataCenter.ReloadAll()`，改为 MainExe 启动期全域刷新 + UI 按需懒加载 domain；修正 LoggerTest 测试日志目录，不再写死 `F:\MeyerScan` 开发机路径；同步修正 HomeUI/CaseUI 进度状态为 v0.2.0，更新当前任务描述 | 减少 UI 模块对全局数据刷新的重复职责，让模块行为更小、更独立；同时修正文档状态漂移和测试宿主路径偏移，避免后续开发继续按旧版本/旧任务理解当前架构 |
| 2026-07-01 | **全模块实现技巧型注释复查**：按用户“每行代码尽可能注释，并解释代码实现技巧”的要求，继续补强 ConfigCenter、Permission、UIComponents、CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、VersionManager、HomeUI、CaseUI/SettingsUI/RuntimeDataCenter 测试宿主、DatabaseTest、LoggerTest、MyLogin 登录宿主等自研源码；重点解释 Qt Layout/父子对象、`deleteLater()`、信号槽、`QLibrary` 动态加载、JSON 解析、`QByteArray::constData()` 生命周期、调用方缓冲区、C ABI、RAII 锁、Windows API、事件循环、测试造数据和正式 UI 边界 | 注释标准从“函数级说明”提升到“实现机制可读”：维护者不仅知道函数做什么，还能理解代码如何实现、为什么这样写、哪些地方不能误改 |
| 2026-07-02 | **实现技巧型注释二次补强**：继续按低注释密度和关键链路复查所有自研源码，重点补强 `SettingsUIImpl.cpp`、`RuntimeDataCenterImpl.cpp`、`MainWindow.cpp`、`SettingsUITest.exe`、`RuntimeDataCenterTest.exe`、`CaseUITest.exe` 中的函数体内部注释；补充 QSS/objectName、Qt Layout 伸缩、设置模块内部 `QStackedWidget` 与 MainExe 业务页替换的区别、校准 DLL 懒加载、domain JSON 快照、旧表兼容、调用方缓冲区、版本 manifest、页面释放和测试宿主造数边界说明 | 对 2026-07-01 注释规则做落地复查，不改变业务逻辑；让核心 UI/数据链路和测试宿主读起来更像“实现说明”，而不是只靠函数名猜代码技巧 |
| 2026-07-02 | **评审后工程规则收口**：新增根目录 CMake 聚合工程和各模块/测试宿主 CMakeLists；新增 `tools/BackupToLocalRepository.ps1`，用于把除第三方库外的全模块源码、测试项目、工程文件、CMake、文档、配置模板和自研 DLL/EXE/LIB 整体备份到 `F:\MeyerScan-Reposit`；备份脚本已收紧过滤规则，排除 Qt/VC/UCRT/OpenSSL/AWS/MySQL/SQLiteStudio/SQL 驱动、日志、数据库现场文件和 IDE 临时文件；文档修正非界面模块 Qt 边界和扫描重建数据处理 DLL/库拆分规则；各模块 README/CHANGELOG 同步记录 | 满足组内评审要求：GitHub + 本地仓库双保障，VSCode + VS2015 双构建入口，非界面模块降低 Qt 耦合，扫描重建 UI/交互与数据处理分离 |
| 2026-07-02 | **本地仓库备份验证**：已重新生成 `F:\MeyerScan-Reposit`，首个完整备份提交为 `09d2f1b01baa22000132de0b7ac1d91f20bcc241`；后续补充备份可能继续产生新提交，最新提交以 `git -C F:\MeyerScan-Reposit log -1 --oneline` 为准。使用 `git ls-files` 检查，提交内容中未发现 Qt/VC/UCRT/OpenSSL/AWS/MySQL/SQLite 第三方 DLL，未发现 `.db/.sqlite/.log/.frm/.MYD/.MYI` 现场文件，未发现旧接口 token 文本；重构文档快照仅保留 `.md` | 确认本地仓库符合“只备份自研和必要工程文件，不混入第三方依赖和运行现场”的新规则 |
| 2026-07-02 | **评审后规则验证结果**：`git diff --check` 无空白错误；当时当前机器未找到 `cmake.exe`，CMake configure/build 未运行；`MeyerScan_AllModules.sln` Release x64 通过，0 warning / 0 error；`LoggerTest.exe`、`DatabaseTest.exe`、`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`MyMainExe\bin\Release\MeyerScan.exe --smoke-main`、`F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0 | 该条记录为 2026-07-02 当时状态；CMake 环境已在 2026-07-06 安装并用 VS2015 x64 生成器完成根聚合 Release 构建补验证 |
| 2026-07-03 | **Database 去 Qt 与 DatabaseQtAdapter 主链路收口**：MyDatabase 改为纯 C++ / sqlite3 C API，不再链接 QtCore/QtSql；新增 `MyDatabaseQtAdapter`，固定依赖方向 `Qt UI/Service -> DatabaseQtAdapter -> Database`；MainExe、RuntimeDataCenter、CaseOrderService 和 CaseUI/SettingsUI 测试宿主已改走 Adapter；HomeUI 正式 DLL 不再接入 Database，CaseUI/SettingsUI 正式 UI 继续只读 RuntimeDataCenter 快照 | 当前数据库主链路从“Qt 模块散落转换/部分旧直连”收口为一个 Adapter 边界，后续字段扩展和去 Qt 化更容易；旧 QtSql 和 UI 框架期 Database 健康检查记录仅作为历史追溯 |
| 2026-07-03 | **SQLite x64 运行时与单模块输出复核**：发现旧 SQLiteStudio 目录内 `sqlite3.dll` 为 32 位，导致 x64 Database 动态加载失败但旧 DatabaseTest 仍跳过；已统一 VS2015 PostBuild 和 CMake 复制来源为 `ThirdParty\SQLite\win-x64\sqlite3.dll`，并补充目录 README。DatabaseTest 现在固定 SQLite 连接失败即失败，不再用“MySQL 可能未运行”跳过。已分别重建 MyDatabase、MyDatabaseQtAdapter、RuntimeDataCenter、CaseUI、SettingsUI、MainExe 单模块方案和根聚合方案，关键输出目录 `sqlite3.dll` 均为 x64；`DatabaseTest.exe` 24 passed / 0 failed，RuntimeDataCenterTest、CaseUITest、SettingsUITest、MyMainExe smoke、根目录 MeyerScan smoke 均返回 0 | 解决“聚合目录能跑、单模块目录缺依赖或拿到旧 32 位 DLL”的隐患；后续新增依赖必须同时验证单模块 `.sln` 与根聚合 `.sln` 输出目录 |
| 2026-07-03 | **提交前全模块复核**：提交前重新检查 Git 状态、旧 SQLiteStudio 路径、Database Qt/QtSql 依赖、第三方 DLL 跟踪、运行路径 QDir::currentPath()、UI/Service 是否绕过 DatabaseQtAdapter、权限 enabled/visible 生效路径和各模块 CMake/VS2015 工程入口；清理 CaseOrderServiceImpl.cpp 中残留的 QSqlQuery 文字说明，改为后续由 Database/DAO 层提供参数绑定能力 | 本轮复核未发现实质链路偏移：Database 仍为纯 C++，Qt 调用方仍经 DatabaseQtAdapter，正式 UI 仍只读 RuntimeDataCenter；MeyerScan_AllModules.sln Release x64 构建通过，DatabaseTest 24/24，通过 LoggerTest、RuntimeDataCenterTest、HomeUITest/CaseUITest/SettingsUITest smoke 和根目录 MeyerScan smoke |
| 2026-07-03 | **活跃模块测试宿主补齐与根方案回归**：检查 `F:\MeyerScan` 当前活跃自研模块，补齐 DatabaseQtAdapter、CaseOrderService、ConfigCenter、Permission、UIComponents、VersionManager、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell 的最小测试宿主、VS2015 测试项目和 CMake 测试目标；将 Logger、Database、HomeUI、CaseUI、SettingsUI、RuntimeDataCenter 等既有测试项目也加入 `MeyerScan_AllModules.sln`；修复 CaseUITest/SettingsUITest 在根输出目录运行时的仓库根路径推导问题；CaseUITest/SettingsUITest 改为在输出目录生成各自独立测试配置和独立 SQLite 文件，避免多个测试共用 `config/db_config.json` 和旧表结构互相污染 | 当前活跃自研模块均具备测试入口；根方案 Release x64 构建通过；LoggerTest、DatabaseTest、ConfigCenterTest、PermissionTest、VersionManagerTest、DatabaseQtAdapterTest、CaseOrderServiceTest、RuntimeDataCenterTest、UIComponentsTest、HomeUITest/CaseUITest/SettingsUITest smoke、Calibration3DUITest、CalibrationColorUITest、OrderScanWorkspaceShellTest 和 `MeyerScan.exe --smoke-main` 均返回 0；CMake 声明已补齐，并已在 2026-07-06 使用 VS2015 x64 生成器完成根聚合 Release 构建补验证 |
| 2026-07-04 | **MyOrderCreateUI 初版建单界面落地**：新增 `F:\MeyerScan\MyOrderCreateUI`，输出 `MeyerScan_OrderCreateUI.dll` 和 `OrderCreateUITest.exe`；采用单页建单工作台布局，左侧基本信息、中间扫描类型和 FDI 牙位选择、右侧订单摘要/牙位明细/标信息/确认操作；新增动作 ID 回调，暂不写数据库；根 CMake 和 `MeyerScan_AllModules.sln` 已接入，MainExe 版本清单和 PostBuild 复制已补齐 `MeyerScan_OrderCreateUI.dll`；测试宿主调整为双击默认显示界面，`--smoke` 才自动测试退出。 | `MeyerScan_OrderCreateUI.sln` Release x64 构建通过；`OrderCreateUITest.exe --smoke` 返回 0；后续需接入 MyUIComponents、RuntimeDataCenter、CaseOrderService、ScanSchemaService 和 OrderScanWorkspaceShell。 |
| 2026-07-04 | **第三方拉起建单链路初版接入**：新增 `MyExternalLaunchAdapter`，输出 `MeyerScan_ExternalLaunchAdapter.dll` 和 `ExternalLaunchAdapterTest.exe`；标准建单上下文增加 `source.thirdPartyType`、`thirdPartyName`、`sourceSystem`、`sourceVersion`；MainExe 支持 `--external-order <json> --external-order-type <type>`，单实例 IPC 可转发外部订单消息；首页“Create”入口进入 `OrderScanWorkspaceShell`，并把 `OrderCreateUI` 挂到建单步骤；外部拉起时后台准备 HomeUI 创建入口并复核 `order.create` 的 `visible/enabled`，客户视觉上只看到工作台/建单页，不看到首页闪现。 | `MyExternalLaunchAdapter`、`MyOrderCreateUI`、`MyMainExe` 单模块 Release x64 构建通过；根 `MeyerScan_AllModules.sln` Release x64 构建通过；`ExternalLaunchAdapterTest.exe`、`OrderCreateUITest.exe --smoke`、`MeyerScan.exe --smoke-external-order --external-order ... --external-order-type cmd_demo` 在单模块输出和根输出目录均返回 0；MainExe 构建仍只有外部登录头文件既有 C4819/C4091 警告。 |
| 2026-07-04 | **全模块偏移复查与测试回归**：按开发 md 规则复查所有活跃自研模块和今日新增模块。确认模块文件结构齐全（README、CHANGELOG、CMakeLists、VS2015 工程、Version.rc、测试宿主或 smoke 入口）；`MyCaseManager` 仅作为旧参考目录，不作为活跃模块；修正任务总览中“创建订单”旧链路为 `OrderScanWorkspaceShell -> OrderCreateUI -> OrderWorkflowService`；ExternalLaunchAdapter 测试样例路径已采用从 exe 目录向上搜索的可移植方式，不绑定 `F:\MeyerScan`。 | 根 `MeyerScan_AllModules.sln` Release x64 构建通过；19 个测试/主流程 smoke 全部返回 0：Logger、Database、ConfigCenter、Permission、VersionManager、DatabaseQtAdapter、CaseOrderService、RuntimeDataCenter、UIComponents、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell、OrderCreateUI、ExternalLaunchAdapter、HomeUI、CaseUI、SettingsUI、MainExe 主链路和外部建单链路。静态扫描未发现实际 `QDir::currentPath()` 路径依赖、Database Qt/QtSql 实依赖、`tr("中文")` 可见文案或运行期开发机硬编码；`git diff --check` 仅有 LF/CRLF 提示；CMake 根聚合 Release 构建已在 2026-07-06 补验证通过。 |
| 2026-07-05 | **MyOrderCreateUI 样式优化与共享 UI 接入**：UIComponents 升级到 v0.4.0，新增 `CreateFieldLabel()`、`CreateDateEdit()`、`CreateTextEdit()`、`CreateTableWidget()`、`ApplyTableStyle()`；OrderCreateUI 升级到 v0.2.1，页面视觉调整为更清爽的三栏工作台，通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格基础样式改为动态加载 UIComponents，牙位按钮和扫描类型按钮保留在建单模块内部；OrderCreateUI 不再链接 `MeyerScan_UIComponents.lib`，只保留头文件依赖和 DLL 复制，运行时 `QLibrary` 加载失败或 UIComponents 版本过旧则走本地降级样式。 | `MeyerScan_UIComponents.sln`、`MeyerScan_OrderCreateUI.sln` 和根 `MeyerScan_AllModules.sln` Release x64 构建通过；模块输出和根输出目录的 `UIComponentsTest.exe`、`OrderCreateUITest.exe --smoke` 均返回 0；根输出目录 `MeyerScan.exe --smoke-main` 和 `MeyerScan.exe --smoke-external-order ... --external-order-type cmd_demo` 返回 0。 |
| 2026-07-05 | **MainExe 自研模块动态加载与版本双来源清单**：MainExe 对 Logger、ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter 改为 `QLibrary + extern "C" GetXxx()` 动态加载，不再链接这些自研模块 import lib；`version_modules.json` 升级为 `file + versionFunction` 清单，自研 DLL 统一导出 `GetMeyerModuleVersion()` 供版本清单读取代码版本，运行时 `versionList` 同时记录 `fileVersion`、`codeVersion`、`versionMatch` 和错误信息；版本清单文件名升级为 `versionList_yyyyMMdd_HHmmss_zzz.json`；DatabaseQtAdapter 补充 `IDatabaseQtAdapter` 和自身 `GetModuleVersion()`；历史 VersionManager 骨架同步新清单口径；MyDatabaseQtAdapter standalone `.sln` 清理 stale GUID 依赖；MainExe 单模块 PostBuild 增加根聚合输出自研 DLL 兜底覆盖，避免旧 sibling DLL 造成单模块目录版本清单读取失败。 | `MyDatabaseQtAdapter\MeyerScan_DatabaseQtAdapter.sln`、`MeyerScan_MainExe.sln`、根 `MeyerScan_AllModules.sln` Release x64 构建通过；根输出目录 `MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke-external-order ... --external-order-type cmd_demo`、单模块输出目录 `MeyerScan.exe --smoke-main` 均正常返回；当时清单覆盖 18 个已声明模块；2026-07-06 加入扫描三件套后，最新 `versionList` 已扩展为 schemaVersion=2、21 个模块、无缺失、无 `codeVersionError`，自研模块 `versionMatch=true`。 |
| 2026-07-06 | **扫描重建独立进程三件套初版落地**：新增 `MyScanWorkflowUI`、`MyDataProcessUI`、`MyScanReconstructStudio` 三个模块；`ScanReconstructStudio.exe` 作为独立壳动态加载 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll`，承载“扫描”和“数据处理”两个大阶段；两个 UI DLL 均提供 QVTK/VTK 显示占位、动作回调和 `DeactivateAndRelease()` 重资源释放接口；扫描相关 VS2015/CMake 第三方路径统一收口到 `MeyerScanScanThirdParty.props/.cmake`，优先使用 `QT_ROOT`、`VTK_ROOT`、`VTK_HEADERS_ROOT`、`OPENCV_ROOT` 环境变量。 | 三个单模块 VS2015 `Release|x64` 构建通过；`ScanWorkflowUITest.exe`、`DataProcessUITest.exe`、`ScanReconstructStudio.exe --smoke` 均返回 0；根工程、MainExe 版本清单和 PostBuild 已接入三者；当前只是壳和 UI 阶段切换初版，真实 IPC、设备连接、扫描算法、后处理算法和 OrderScanWorkspaceShell 启动扫描进程链路仍未完成。 |
| 2026-07-06 | **CMake 安装、版本清单与旧口径清理**：安装 CMake 3.31.6 到 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe`，并写入用户 PATH；使用 `Visual Studio 14 2015 Win64` 生成器完成根聚合 CMake `Release` 配置和构建；修复扫描 UI CMake include 顺序，优先使用 `VTK_HEADERS_ROOT` 中可编译的 `QVTKWidget.h`；补齐 CaseOrderServiceTest 的 DatabaseQtAdapter 依赖；MainExe CMake/VS2015 输出目录补齐配置文件和 VTK/OpenCV 运行库复制；运行时 versionList 扩展到 21 个声明模块，扫描三件套均记录 `fileVersion`、`codeVersion`、`versionMatch=true`，且不混入 Qt/VTK/OpenCV 第三方库；清理 CaseUI/Database 中误导后续开发的旧注释。 | `F:\MeyerScan\build\cmake-vs2015-x64` 根聚合 `Release` 构建通过；`MeyerScan.exe --smoke-main`、`ScanWorkflowUITest.exe`、`DataProcessUITest.exe`、`ScanReconstructStudio.exe --smoke` 均返回 0；最新验证文件 `versionList_20260706_154131_254.json` 为 schemaVersion=2、21 个模块、无缺失、无 `codeVersionError`、无版本不一致、第三方库计数为 0。 |
| 2026-07-08 | **MyOrderCreateUI 治疗方案选择优化**：建单治疗方案区按当前软件视频复刻，新增 `ToothTreatmentPlanWidget`，使用上下颌牙弓图、mask 像素命中、牙位叠加图和桥连接点叠加图；布局继续校准为左侧治疗类型卡片、中间大牙弓主交互、右侧明细卡；治疗类型改为图标按钮并保留当前类型摘要；“Clear All” 放到上下颌之间并增加确认框；外部 `bridgeConnectors` 增加两端牙位 bridge 类型校验和反向 key 归一化；模块资源规则落地为 `模块/Resources -> 运行目录 Resources/Modules/<ProjectName>`。 | `MeyerScan_OrderCreateUI.dll` 和 `OrderCreateUITest.exe` Release 构建通过；`OrderCreateUITest.exe --smoke` 返回 0，覆盖治疗方案控件、非 bridge 脏桥连接点过滤、反向桥 key 归一化、普通桥区间 `16-18`、跨中线桥区间 `11-22` 和扫描流程 JSON；`OrderCreateUITest.exe --capture-screenshot <png>` 可固定 1920x1080 生成截图用于视频帧对齐。 |
| 2026-07-06 | **OrderScanWorkspaceShell 步骤切换缺陷修复**：用户反馈工作台内 Order / Scan / Process 页面点击无法切换。定位原因是顶部步骤条使用 `QLabel` 仅展示文字，没有点击信号；已改为 `QPushButton`，点击按钮后由壳子内部调用 `SetStep()` 切换 `QStackedWidget` 页面，并同步当前步骤按钮高亮。 | `OrderScanWorkspaceShellTest.exe` 已从直接调用 `SetStep()` 升级为查找真实步骤按钮并执行 `click()`，验证 Scan 按钮可切到扫描页、Process 按钮可切到处理占位页；CMake 目标 `OrderScanWorkspaceShellTest` 构建通过，测试返回 0；`MeyerScan.exe --smoke-main` 和 `--smoke-external-order` 返回 0。 |
| 2026-07-07 | **建单扫描流程创建链路接入**：OrderCreateUI 升级到 v0.3.0，新增上颌/下颌异性扫描杆、上颌/下颌扫描杆分段和咬合类型输入；生成 `scanProcess` JSON（`schemaVersion/source/config/steps`），并通过 `OrderCreateActionScanProcessChanged` 通知 MainExe；MainExe 升级到 v0.1.2，创建模式进入 Scan/Process 前读取并转发建单流程，练习模式固定默认流程；ScanWorkflowUI/DataProcessUI 升级到 v0.2.0，统一从 `scanProcess.steps` 渲染顶部按钮。 | CMake 根聚合 `MeyerScan` Release 构建通过；`OrderCreateUITest.exe --smoke`、`ScanWorkflowUITest.exe`、`DataProcessUITest.exe`、`MeyerScan.exe --smoke-main` 均返回 0；根方案仍只有外部登录头文件既有 C4819/C4091 警告。 |
| 2026-07-07 | **Scan/Process 流程按钮交互、鼠标中心缩放和 SendUI 初版接入**：ScanWorkflowUI/DataProcessUI 升级到 v0.2.1，流程按钮增加手型 hover、tooltip、点击切换当前部位显示数据和选中态；两页 QVTK 滚轮缩放改为以鼠标位置为中心并在范围内夹紧；DataProcessUI 新增独立 Process Hint，且不加入 Scan 页的 Start/Pause；新增 MySendUI，输出 `MeyerScan_SendUI.dll` 和 `SendUITest.exe`，MainExe 升级到 v0.1.3 并在创建模式 Process 下一步后进入 Send 步骤。 | `cmake --build build --config Release --target MeyerScan SendUITest ScanWorkflowUITest DataProcessUITest` 通过；`SendUITest.exe`、`ScanWorkflowUITest.exe`、`DataProcessUITest.exe` 均返回 0；`MeyerScan.exe --smoke-main` 返回 0；修复 CMake 构建 SendUITest 时漏拷贝 `MeyerScan_Logger.dll` 的运行依赖问题。 |
| 2026-07-09 | **QSS/Resources、版本详细信息、扫描重建双形态与关键日志补强**：新增公共 `MeyerQtModuleUtils.h`，Qt 模块日志可直接传 `QString` 且模块名由 `MEYER_MODULE_NAME` 自动填充；各 UI 模块样式统一从 `Resources/qss` 加载，源码仅允许公共 `ApplyModuleQss()` 调用 `setStyleSheet()`；MainExe VS2015/CMake 发布目录复制各模块完整 `Resources`；`Version.rc` 统一补齐 `LegalCopyright`；版本清单默认扩展到 23 个启动记录文件，包含 `MeyerScan_ScanReconstructStudio.dll` 和 `MeyerScan_SendUI.dll`；`MyScanReconstructStudio` 增加 DLL 形态 VS2015 工程并加入 sln，独立 EXE 和 DLL 均纳入版本/资源/构建规则；创建/练习壳右上角改为自绘最小化/关闭图标按钮；MainExe 增加扫描/处理/发送页面释放前后日志。 | 已完成静态规则扫描：源码实际 `setStyleSheet()` 仅剩公共入口，未发现运行路径依赖 `QDir::currentPath()`；各 UI 模块均有 `Resources/qss`；所有 `Version.rc` 均包含文件说明、文件版本、产品名、产品版本、原始文件名和版权字段。后续仍需按本轮最终代码重新执行 CMake/VS2015 构建和 smoke 验证。 |
| 2026-07-10 | **glm52 建议复核、UI 所有权修正和全链路回归**：逐项复核 01-04 建议并新增 05 采纳结论；废止 MainExe 通用可见标题栏和 OrderCreateUI 重复步骤条，改为 MainExe 无边框全屏单内容区、HomeUI/CaseUI/WorkspaceShell 页面语义顶部、WorkspaceShell 唯一步骤导航；MainExe/HomeUI/CaseUI/OrderCreateUI/WorkspaceShell/ScanReconstructStudio 分别升级到 v0.1.4/v0.2.1/v0.2.1/v0.4.1/v0.1.2/v0.1.1；修正 Core/CaseEntity 为按需候选和服务 DTO/版本化 JSON 口径；同步模块 README/CHANGELOG 和代码 TODO。 | VS2015 根 `MeyerScan_AllModules.sln` Release x64 通过；CMake 根 `build` Release 重新配置并构建通过，仅保留外部登录头文件既有 C4819/C4091 警告；Database 24/24、ConfigCenter、Permission、CaseOrderService、UIComponents、HomeUI、CaseUI、OrderCreateUI、WorkspaceShell、ScanReconstructStudio、MeyerScan smoke 全部返回 0；最新 versionList 为 23 模块、0 缺失、0 版本不一致、0 codeVersionError。 |
| 2026-07-10 | **本地备份历史排除内容清理修复**：发现 `robocopy /MIR` 配合 `/XD`、`/XF` 只会阻止排除项继续复制，不会删除目标仓库中早期已存在的第三方文件；在 `BackupToLocalRepository.ps1` 增加目标根路径边界校验、排除目录深度优先删除和排除文件通配符清理，并将脚本固定为带 BOM 的 UTF-8，避免 Windows PowerShell 5.1 按 ANSI 误解析中文注释后吞并代码行。 | 首次修复运行清理 33 个历史排除目录和 889 个排除文件，本地仓库提交 `50009af` 删除 355 个此前已跟踪的第三方文件；复查命中数为 0；第二次完整运行目录/文件命中均为 0 且无新提交，证明脚本可重复执行。 |
| 2026-07-10 | **统一资源 DLL、首页/浏览/建单最终复核与版本测试隔离**：新增 MyUIResources v0.1.1，自动聚合 608 个 UI 资源；MainExe/HomeUI/CaseUI/OrderCreateUI 升级到 v0.1.6/v0.3.1/v0.3.1/v0.5.0；修复 VersionManagerTest 覆盖根 Release 正式清单的问题；修复 Scan/Process/Scan 往返时 QVTK/VTK 释放顺序和延迟析构重入风险。 | VS2015 根方案、CMake 根构建通过；24 项模块/主链路测试全部返回 0；正式 manifest 在 VersionManagerTest 前后 SHA-256 不变；最新 versionList 为 24 项、0 缺失、0 版本不一致、0 codeVersionError；首页、浏览、建单 1920x1080 与 1366x768 共 6 张截图复核通过。 |
| 2026-07-12 | **OrderCreateUI 五类型、牙位/桥映射和多分辨率修正**：OrderCreateUI 升级到 v0.5.1，修复类型收口为全冠/缺失牙/嵌体/贴面/种植体，图片序号为 `1/3/4/5/7`；按实际 mask 校正上下颌牙位和桥顺序；相邻已选牙显示空心桥点，点击后变实心；类型按钮按 b/h 和 1x/2x 资源切换；中间 Scan Plan 最大 980px 居中。UIResources 升级到 v0.1.2。 | VS2015 单模块和根方案构建通过；模块目录与根目录 OrderCreateUI smoke、UIResources smoke 返回 0；1366x768、1920x1080、2560x1440 截图复核无文字裁切、控件重叠或中栏过度拉伸。 |
| 2026-07-12 | **文档权威源、注释安全、建单 hover/布局与 MainExe 强制重建**：四文档迁入仓库 Documents；新增注释换行/BOM 检查；OrderCreateUI v0.5.2 对 h 图合成局部圆底、固定扫描流程预览高度、限制 Scan Plan 最大高度并锁定资源 DLL 批次；UIResources v0.1.3；配置根/本地仓库为 trusted。 | VS2015 根方案 Rebuild、CMake 全量 Release 通过；根和单模块 MainExe smoke-main、根 startup/external smoke、OrderCreateUI/UIResources smoke 均返回 0；三张 PNG 实际尺寸为 1366x768/1920x1080/2560x1440；versionList 24 项 0 缺失/不一致/代码错误；根 EXE 已于 7 月 12 日重新生成。 |
| 2026-07-12 | **全模块代码、注释和架构偏移复核**：近期 Scan/Process/Send/ScanReconstructStudio、工作台、建单、首页、浏览、设置和 MainExe 补齐中文实现注释；统一检查 Init/上下文/CreateWidget 返回值；非法 JSON 保留上一份有效上下文；重页面改为宿主挂载后显式 Activate；可选依赖失败清空接口并按模块降级。 | VS2015 根 Rebuild 与 CMake Release 全量构建通过；24 项自研测试/集成链路及 MainExe 登录前 smoke 返回 0；最新 versionList 24 项 0 缺失/不一致/代码错误；注释安全 0 错误 0 警告；未发现 UI 直连 Database、SendUI 越界实现业务、currentPath 或业务源码直接 setStyleSheet。 |
| 2026-07-13 | **生产/测试数据隔离复核**：OrderCreateUI、SendUI 和 MainExe 清理正式路径中的示例患者、订单、医生、技工所和牙位；手工创建使用空白上下文，练习数据统一使用 `PRACTICE_*`；OrderCreateUI 在候选 JSON 完整解析成功后才提交缓存，非法 JSON 保留上一份有效状态。 | VS2015 根 Release x64 与 CMake Release 构建通过；OrderCreateUI 增强 smoke、MainExe 三类 smoke 返回 0；`versionList_20260713_072234_645.json` 为 24 项、0 缺失、0 文件/代码版本不一致、0 `codeVersionError`。 |
