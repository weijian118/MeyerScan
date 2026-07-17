# MeyerScan 重构开发进度跟踪

> **文档职责**：只记录当前版本、真实成熟度、未完成项、下一步和可复现验证。设计规则见任务总览和架构规范，历史过程见 Git 与模块 CHANGELOG。
>
> **唯一维护位置**：`F:\MeyerScan\Documents`。

## 1. 最新验证基线

验证日期：2026-07-16。

| 检查 | 结果 |
|---|---|
| CMake Release 全量构建 | 通过；最终采用非并行构建，避免多个工程争用同一输出文件 |
| 根 CTest | 27/27 通过，覆盖 25 个模块测试和 2 个 MainExe 集成 smoke；新增 DeviceCmd smoke |
| MainExe smoke | `--smoke`、`--smoke-main`、`--smoke-external-order` 已通过 |
| VS2015 根方案 | 最近一轮 Release x64 通过；仅保留既有外部登录头文件编码/声明警告 |
| 版本清单 | `versionList_20260716_112818_578.json`：27 项，0 缺失，0 文件/代码版本不一致，0 `codeVersionError`；DeviceTransport `1.1.0`、DeviceCmd `0.2.0` 均匹配 |
| 注释安全 | 0 错误、0 警告 |
| 文档备份脚本 | PowerShell 5.1 AST、BOM 和两次隔离幂等执行通过 |

标准快速回归：

```powershell
cmake --build F:\MeyerScan\build --config Release
ctest --test-dir F:\MeyerScan\build -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File F:\MeyerScan\tools\CheckSourceCommentSafety.ps1
git -C F:\MeyerScan diff --check
```

VS2015 与 CMake 会写入相同模块 `bin\Release`，不得并行构建。

## 2. 当前模块版本

| 模块 | 版本 | 当前成熟度 |
|---|---:|---|
| MainExe | 0.1.9 | 启动、登录、API 门禁、快照注入、新旧患者订单读模型合并、最小建单保存、导航和 smoke 已接通；真实业务编排仍在完善 |
| Logger | 1.1.1 | 基础能力可用；已提供 API 版本导出 |
| Database | 1.3.0 | SQLite 主链路可用；MySQL 原生 SDK 接入待完成 |
| DatabaseQtAdapter | 0.1.1 | 转换链路可用；已提供 API 版本导出 |
| DeviceTransport | 1.1.0 | VS2015/CMake 和 31 项无硬件 smoke 可用；增加统一 ABI 门禁和绝对路径日志加载；CyAPI 真实设备、长时间采集和拔插恢复待联调 |
| DeviceCmd | 0.2.0 | 按协议覆盖 48 组 A 类命令，新增相机/标定/曝光/设备信息/固件分包等固定 POD 接口；全量模拟 smoke 和真实 Transport 动态加载已通；多机型协议、MainExe 会话宿主、Flash 写入和实机命令待联调 |
| ConfigCenter | 0.1.1 | 读取骨架可用；迁移、通知、加密待完成 |
| Permission | 0.1.1 | visible/enabled 生效；六维权限和多层复核待完成 |
| RuntimeDataCenter | 0.1.1 | 本地/云端 JSON 快照骨架可用；由 MainExe 统一读取并注入 UI |
| CaseOrderService | 0.2.2 | 标准嵌套建单保存/查询、患者/订单轻量列表和最小 schema 可用；完整 CRUD、事务和迁移待完成 |
| ScanSchemaService | 0.1.0 | 扫描步骤规则服务和测试可用；规则已从 UI/MainExe 移出 |
| UIComponents | 0.4.1 | 常用控件工厂可用；DPI、公共弹窗、复杂表格待扩展 |
| UIResources | 0.1.4 | 统一资源 DLL 可用；已收录颜色校准 QSS、预览图和关闭按钮状态资源 |
| HomeUI | 0.3.3 | 页面和入口动作可用；只接收应用目录，不再带数据库语义 |
| CaseUI | 0.3.3 | 宿主快照列表和动作上报可用；真实 CRUD/Workflow 未闭环 |
| SettingsUI | 0.2.4 | 宿主快照页面、三维校准入口和可拖动颜色校准模态遮罩可用；Windows 透明窗口蒙层已修复；配置保存/刷新未闭环 |
| OrderCreateUI | 0.5.4 | 建单 UI、牙位/桥、完整上下文导出可用；字段校验仍需完善 |
| OrderScanWorkspaceShell | 0.1.4 | 创建/练习容器和步骤切换可用 |
| ExternalLaunchAdapter | 0.1.1 | CMD JSON 第三方建单归一化链路可用 |
| Calibration3DUI | 0.1.1 | UI/流程骨架；设备与计算未接入 |
| CalibrationColorUI | 0.2.0 | 参考软件弹窗、可拖动标题栏、预览资源、共享按钮和设置遮罩链路可用；设备取图与计算未接入 |
| ScanWorkflowUI | 0.2.4 | QVTK 页面、稳定 code 流程按钮和资源释放骨架可用 |
| DataProcessUI | 0.2.4 | QVTK 页面、稳定 code 处理入口和资源释放骨架可用 |
| SendUI | 0.1.3 | 页面和动作上报可用；导出/上传未实现 |
| ScanReconstructStudio | 0.1.4 | DLL/EXE 双形态壳、API 门禁和 Scan/Process 切换可用 |
| VersionManager | 0.1.0 | 历史骨架；当前能力内置 MainExe |

既有登录、网络和加解密 DLL 由外部项目提供，不以上表版本代表其实际版本。

## 3. 功能成熟度

| 链路 | 已完成 | 未闭环 |
|---|---|---|
| 启动与登录 | Logger 最早加载、数据库检查、等待页、登录信号、首页显示、单实例骨架 | LoginAdapter、完整失败回显、激活边界和正式登录状态合同 |
| 首页与导航 | Home/Case/Settings/Create/Practice 页面切换，工作台步骤切换 | 权限/配置最终注入、所有返回页刷新和异常恢复 |
| 患者/订单 | SQLite、Adapter、RuntimeDataCenter、CaseOrderService、OrderCreate Confirm/Next 最小保存链路；新表读模型与旧表快照按 ID 合并 | 搜索分页、编辑删除、正式事务/migration、并发和 Workflow |
| 建单 | 五种修复类型、FDI 牙位、桥、ScanSchemaService、完整上下文导出、第三方上下文 | 字段校验、跨表事务、HIS/Worklist |
| 设置 | 分类页面、来源页参数、两校准入口、只读参考数据 | ConfigCenter 上下文、Apply/Confirm/Restore、保存后刷新 |
| 权限 | JSON 读取、visible/enabled、首页/浏览示例 | 六维快照、PermissionConfigUI、服务/工作流/IPC 复核、绕过测试 |
| 扫描/处理 | DLL/EXE 壳、QVTK 占位、流程按钮、鼠标中心缩放、重资源释放；DeviceTransport + DeviceCmd 分层和无硬件最小采集链路 | DeviceSessionHost、真实硬件命令/采集联调、ScanDataIO、重建、真实模型、编辑/分析算法、异常恢复、独立进程 IPC |
| 校准 | 两个独立 UI DLL 骨架 | 设备采集、算法、结果保存和失败恢复 |
| 发送 | UI 展示和动作回调 | DataExport、压缩、邮件、云上传、重试和状态持久化 |
| 资源与样式 | UIResources DLL、模块 QSS、通用控件 | LanguageManager、Common qm、完整资源签名/修复 |
| 交付 | 运行时版本清单和本地整体仓库 | MyUpdate、安装器、发布清单、哈希/签名、升级回滚 |

页面可显示或 smoke 通过只表示框架可运行，不代表右侧未闭环业务已完成。

## 4. 当前优先任务

### P0：患者订单真实闭环

1. 新建 LoginAdapter，MainExe 不再直接理解外部登录结构。
2. 完善 CaseOrderService 的正式 schema、迁移、事务、错误码和完整 CRUD。
3. 完善 ScanSchemaService 的配置 schema/兼容策略，并把治疗方案持久化纳入患者订单事务。
4. 开发 OrderWorkflowService，统一创建、打开、继续扫描、处理和发送规则。
5. CaseUI 搜索/删除/打开全部改走 Service/Workflow；保存链路补字段校验、事务和失败恢复测试。

验收：可以创建真实订单、重启后查询、打开并进入正确工作台；任一步失败不产生半条订单或错误状态。

### P1：设置与权限闭环

1. MainExe 向 SettingsUI 注入版本化配置上下文。
2. 实现 Apply/Confirm/Restore，关闭后按来源页刷新。
3. Permission 构建六维不可变快照。
4. UI、MainExe、Service/Workflow 和独立进程分层复核关键动作。

验收：修改设置可持久化并恢复；隐藏、禁用和越权调用均有测试和日志。

### P2：扫描真实能力

1. 只为独立 ScanReconstructStudio EXE 建立最小版本化 IPC。
2. 在 MainExe/独立扫描宿主实现单一 DeviceSessionHost，把已开发 DeviceCmd -> DeviceTransport 接入设置、入口检查、校准和扫描；随后完成多机型真实设备联调。
3. 继续开发 ScanDataIO 和采集/重建算法；采集数据落盘、预处理和重建不得回填到 UI 或 DeviceCmd。
4. 按实际需求拆出编辑、手动配准、测量、颈缘、倒凹、咬合、底座等处理 DLL。
5. 增加重复 Scan/Process 切换、模型加载和异常退出稳定性测试。

### P3：发送和交付

1. DataExport/Network 与 SendUI 动作打通。
2. 完成 MyUpdate.exe。
3. 完成安装器、发布依赖清单、签名/哈希、安装修复和升级回滚。

## 5. 已规划但未开始

| 模块/能力 | 触发条件 |
|---|---|
| HisWorklistAdapter | 首个医院/HIS 接口和字段协议明确 |
| PermissionConfigUI / QRCodeAuthEntry | 授权规则导入和扫码流程明确 |
| Statistics | 稳定统计口径和调用页面明确 |
| EngineeringSettings | 高级设备/固件/诊断需求明确 |
| 多个处理工具 DLL | 对应算法接口和数据所有权明确 |
| MyUpdate / Installer | 主链路和发布目录稳定后 |

未满足触发条件前不创建空 DLL。

## 6. 测试清单

### 每次模块修改

- 单模块 VS2015 或 CMake Release 构建。
- 模块 Test.exe/smoke 的成功、失败和版本检查。
- 注释安全和 `git diff --check`。
- README/CHANGELOG 与版本同步。

### 跨模块修改

- 根 CMake 或 `MeyerScan_AllModules.sln` 构建。
- 26 项 CTest。
- MainExe `--smoke` 及受影响集成 smoke。
- 运行时 versionList：无缺失、无代码/文件版本不一致。
- 正式配置、数据库和 manifest 测试前后哈希不变。

### UI 修改

- 1366x768、1920x1080、2560x1440 截图。
- 检查文字裁切、控件重叠、动态内容抖动、全屏无边框、QSS 和 tr 规则。
- 反复进入/退出页面，检查 QWidget、线程、VTK、OpenGL 和模型资源释放。

### 发布前

- 干净目录/干净机器启动。
- 安装、升级、回滚、卸载和数据保留。
- 第三方依赖完整性和自研模块版本/签名。
- 长时间运行、异常 DLL/JSON/IPC、磁盘不足和断网测试。

## 7. 完成状态规则

- **骨架可用**：能构建、加载、显示或执行最小接口，有测试，但真实业务未闭环。
- **主链路可用**：与上下游真实联调，成功和失败均可验证，数据可恢复。
- **完成**：边界、测试、日志、版本、文档、异常恢复和发布验证全部满足。

进度文档默认使用以上三个状态，避免用模糊百分比制造完成错觉。

## 8. 最近决策

| 日期 | 决策 |
|---|---|
| 2026-07-15 | CaseUI/SettingsUI 改为宿主快照注入；扫描规则进入 ScanSchemaService；MainExe 动态 C++ 接口增加 API 门禁并接入最小建单保存 |
| 2026-07-14 | 重构文档只维护 `F:\MeyerScan\Documents`；移除 D 盘镜像和 `_RefactorDocs` |
| 2026-07-14 | 四份主文档去除重复历史和旧接口草案，现行规则由任务总览/架构规范维护 |
| 2026-07-13 | 根 CMake 注册 24 项隔离 CTest；正式数据与测试数据严格隔离 |
| 2026-07-12 | UIResources DLL、建单五类型/牙位/桥、多分辨率和注释安全规则完成复核 |
| 2026-07-10 | MainExe 无通用标题栏；WorkspaceShell 唯一步骤导航；扫描重建采用 DLL/EXE 双形态 |
| 2026-07-03 | Database 去 Qt，Qt 调用统一通过 DatabaseQtAdapter |

更细的代码变更查询模块 CHANGELOG 和 Git 历史，不再在本文件累积逐次对话记录。

---

> **文档版本**：v3.1（2026-07-15，记录快照注入、ABI 门禁、扫描规则服务和最小建单保存）
