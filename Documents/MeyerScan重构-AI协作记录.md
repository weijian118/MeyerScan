# MeyerScan 重构 AI 协作记录

> **文档职责**：只记录会影响后续开发的关键决策，不再复制每次命令、文件清单、构建输出和模块 CHANGELOG。
>
> **唯一维护位置**：`F:\MeyerScan\Documents`。历史全文通过 Git 追溯。

## 1. 记录规则

新增条目必须满足至少一项：

- 改变模块拆分、职责、依赖方向或公共接口。
- 改变全项目强制规则、构建/测试/发布方式。
- 推翻旧方案，后续开发容易误用旧口径。
- 明确重大功能链路、数据所有权或资源生命周期。

普通代码修复、注释补充、样式微调、单模块构建结果只写模块 CHANGELOG 和 Git 提交。全局当前状态写进度文档，不在本文重复。

每条关键决策只写：日期、问题、结论、影响范围。详细实现通过提交记录查询。

## 2. 当前关键决策

### 2026-07-14：设备传输模块命名、ABI 和职责

- 问题：初版 `MyDeviceManager` 名称过宽，混入命令语义、空传输占位、重复旧工程、硬编码 Eigen/OpenCV 路径，并存在采集线程和 CyAPI 异步资源风险。
- 结论：统一为 `MyDeviceTransport / MeyerScan_DeviceTransport.dll / DeviceTransportTest.exe`；当前只实现 Windows x64 CyAPI USB。
- 结论：公共接口统一为版本化 C ABI；模块负责原始命令/流、异步传输、组帧和底层错误，命令业务归 DeviceCmd，UI/重建算法不得进入本模块。
- 结论：采集参数在访问 CyAPI 前按公共 `MAX_*` 常量和 512 MiB 总预算校验；尺寸乘法使用 64 位中间值。`GetFrame` 固定非阻塞，无帧立即返回 `NotReady`。
- 结论：默认测试为无硬件 smoke；真实枚举、命令、流和采集必须显式选择，真实设备稳定性在后续联调验收。
- 验证：模块 VS2015 x64 Debug/Release、模块 CMake、总解决方案 Release、根 CMake 通过；复制到无仓库 helper 的临时目录后仍可独立构建；DeviceTransport smoke 30/30、根 CTest 25/25。
- 影响：ScanWorkflow/扫描编排后续只依赖 DeviceTransport 公共头；版本清单记录 DLL 的代码版本和文件版本，不记录测试及 CyAPI 第三方库。

### 2026-07-14：文档单一来源与内容收敛

- 问题：仓库 Documents、D 盘镜像、`_RefactorDocs` 和多份大文档重复，旧方案容易干扰后续开发。
- 结论：只维护 `F:\MeyerScan\Documents`；不再读取/同步 D 盘文档，不再生成 `_RefactorDocs`。
- 结论：任务总览管范围和完整模块清单；架构规范管边界和合同；进度只管当前状态；本文只管关键决策。
- 影响：`BackupToLocalRepository.ps1` 直接备份仓库 Documents；历史内容由 Git 保留。

### 2026-07-13：真实成熟度、数据隔离和统一 CTest

- 问题：页面能显示和单个 smoke 通过容易被误写成业务完成；测试可能污染正式配置/数据。
- 结论：使用“骨架可用/主链路可用/完成”三级状态；正式路径不得包含示例患者、订单或牙位。
- 结论：根 CMake 注册 24 项隔离 CTest，测试运行目录与正式配置、数据库、版本清单分离。
- 影响：新功能必须覆盖失败路径，并验证正式文件测试前后不变。

### 2026-07-12：统一 UI 资源和建单治疗方案

- 问题：icon/qss/qm 散落在客户目录容易遗失或被误改；建单治疗方案与现有产品交互差异较大。
- 结论：资源源码仍由各 UI 模块维护，构建时聚合进单一 `MeyerScan_UIResources.dll`，运行时使用 qrc 路径。
- 结论：建单修复类型固定为全冠、缺失牙、嵌体、贴面、种植体；FDI 牙位和桥通过 mask 与叠加资源实现。
- 影响：业务源码样式只从 QSS 加载；资源 DLL、模块 DLL 和版本清单作为一致批次发布。

### 2026-07-10：UI 所有权和扫描重建双形态

- 问题：MainExe、页面和工作台重复绘制标题/步骤区域；扫描模块既要嵌入又要独立运行。
- 结论：MainExe 只提供无边框单内容区；Home、Case、WorkspaceShell 各自拥有页面语义顶部；WorkspaceShell 是步骤导航唯一所有者。
- 结论：MyScanReconstructStudio 共用一套实现，同时产出 DLL 和 EXE；DLL 用于创建/练习嵌入，EXE 用于独立练习和定制。
- 影响：只有 EXE 形态使用 IPC；DLL 形态不重复建设 IPC。

### 2026-07-09：QSS、路径、版本和日志统一

- 问题：源码内联样式、开发机路径、版本来源不一致和关键操作日志不足。
- 结论：业务 UI 禁止直接 `setStyleSheet()`；可见文字使用 `tr("English source text")`；运行路径禁止 currentPath。
- 结论：每个自研 DLL/EXE 同时维护代码版本和 Version.rc 文件版本，MainExe 只按 manifest 记录自研模块。
- 结论：DLL 加载、页面切换、资源创建释放、权限/配置决策和客户操作均写结构化日志。

### 2026-07-07：扫描流程单一生产者与 SendUI 边界

- 问题：建单、Scan 和 Process 可能各自复制扫描流程规则；发送页容易直接实现导出/网络逻辑。
- 结论：OrderCreateUI 生成 `scanProcess`，MainExe/Workflow 转发，Scan/Process 只渲染 `steps`。
- 结论：SendUI 只展示上下文和上报 Export/Compress/Email/Upload；真实能力进入 DataExport/Network/服务层。

### 2026-07-06：扫描与数据处理分层

- 问题：ScanReconstructStudio 原工程职责过大，设备、采集、显示、编辑和分析混在一起。
- 结论：先拆 ScanWorkflowUI、DataProcessUI 和 ScanReconstructStudio 壳；后续设备、数据 IO、预处理、编辑、配准、测量和分析继续按依赖拆能力 DLL。
- 影响：UI 只负责交互和显示；离开阶段必须释放 QVTK、VTK、OpenGL、线程和模型资源。

### 2026-07-04：统一建单入口和第三方适配

- 问题：手工、第三方和 HIS 建单若各建一套 UI，会造成字段、校验和扫描方案分叉。
- 结论：三类入口共用 OrderCreateUI；External/HIS Adapter 只把来源字段归一化为标准上下文。
- 结论：第三方启动后用户直接看到建单工作台，不显示首页闪现；上下文必须记录第三方类型和来源版本。

### 2026-07-03：Database 去 Qt 和 Qt 适配层

- 问题：非界面 Database 使用 QtSql 增加底层依赖，UI 又需要 QString/QJson 便利性。
- 结论：Database 使用纯 C++/SQLite C API；新增 DatabaseQtAdapter 负责 Qt/C++ 类型和缓冲区转换。
- 标准链路：`UI -> RuntimeDataCenter/CaseOrderService -> DatabaseQtAdapter -> Database`。

### 2026-07-02：双构建、本地整体仓库和非界面 Qt 边界

- 结论：每个模块和测试宿主同时维护 VS2015 与 CMake；GitHub 外增加 `F:\MeyerScan-Reposit` 整体历史仓库。
- 结论：新增非界面模块能不用 Qt 就不用；需要 Qt 的模块把 UI 与业务实现分层，公共 ABI 保持稳定。
- 结论：本地仓库保存自研源码、工程和产物，排除第三方库、日志、现场数据库和 IDE 临时文件。

### 2026-06-30：运行时数据中心和易变化字段

- 问题：诊所、技工所、医生、账号、设备、患者、订单和云端诊所信息会扩展且被多模块读取。
- 结论：RuntimeDataCenter 保存进程内只读 JSON 快照；写操作和业务查询走 Service。
- 结论：易变化合同使用 `schemaVersion + stable keys + extensions`，不把字段固化进跨模块 Entity ABI。

### 2026-06-24：患者订单服务合并和 Qt 使用原则

- 问题：患者和订单在产品与数据库中紧密相连，拆 CaseService/OrderService 会增加事务和维护成本。
- 结论：合并为 CaseOrderService；医生、诊所、技工所等参考数据也由该服务管理。
- 结论：不刻意规避 Qt；Calibration3DUI、CalibrationColorUI、CaseOrderService 和 WorkspaceShell 可按需要使用 Qt，但必须守住边界。

### 2026-06-23：多分辨率、多语言、权限和资源释放

- 结论：不延续全界面绝对坐标乘分辨率系数；使用 Layout、sizePolicy、约束和有限 DPI 尺寸。
- 结论：禁止按语言写位置/尺寸 if/else；界面文字统一英文 tr source。
- 结论：Permission 的 visible/enabled 必须实际生效，关键动作不能只靠按钮隐藏。
- 结论：从案例管理进入扫描前释放 CaseUI；重页面离开必须释放大内存/显存资源。

### 2026-06-22：MainExe 启动顺序、日志和安装更新边界

- 结论：Logger 最早加载；随后配置/权限、数据库检查、登录、运行时数据和首页。
- 结论：页面切换由 MainExe/WorkspaceShell 编排，客户操作和切换过程记录日志。
- 结论：MyUpdate.exe 与 MeyerScan.exe 同级并独立覆盖更新；安装打包作为独立交付模块。

### 2026-06-17：细模块优先但拒绝过度工程化

- 结论：按功能边界、变化原因、资源和依赖拆分，不按理想 DLL 数量拆分。
- 结论：HomeUI 必须独立；加载订单规则进入 OrderWorkflowService；公共方法先归属具体模块，不建万能 Common。
- 结论：最终形态为主 EXE + 插件 DLL + 少量稳定静态库/公共头 + 必要独立进程。

## 3. 固定协作约束

- 先读 `Documents/README.md`、任务总览、架构规范、进度文档和目标模块 README/CHANGELOG。
- 不默认读取仓库外旧重构文档。
- 修改代码前先核对现有接口、版本、测试和工作树，不回退用户已有改动。
- 源码和测试补中文函数/关键实现注释；UI source text 用英文 tr。
- 修改模块行为时更新模块 CHANGELOG；只有全局决策变化才更新本文。
- 用户明确要求提交时，GitHub 和本地仓库提交日志使用中文且具体。

## 4. 历史查询

历史全文已进入 Git，不在当前文档保留重复副本：

```powershell
git -C F:\MeyerScan log -- Documents/MeyerScan重构-AI协作记录.md
git -C F:\MeyerScan show <commit>:Documents/MeyerScan重构-AI协作记录.md
git -C F:\MeyerScan log -- <module>/CHANGELOG.md
```

需要定位某次实现时，优先查询模块 CHANGELOG 和对应提交 diff；不要从旧文档复制代码或接口草案回现行方案。

---

> **文档版本**：v3.0（2026-07-14，从逐次对话流水账收敛为关键决策索引）
