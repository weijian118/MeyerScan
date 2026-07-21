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

### 2026-07-21：资源 DLL 独立覆盖合同

- 问题：客户旧版本可能只需定制图标，而主项目代码已继续演进；若资源编号、导出函数、qrc 前缀和版本只靠人工记忆，单独覆盖资源 DLL 容易在客户现场失配。
- 结论：资源 API、RCDATA `101`、清单 schema 和 qrc 前缀集中进公共合同头；Version.rc、C++、生成脚本和测试共同引用。资源 DLL 新增合同查询导出，新加载器初始化前严格校验，旧导出保持不变。
- 影响：UIResources 升级为 0.2.0。纯资源补丁允许独立发布，但必须从客户交付基线构建、保留旧 alias，并用客户原程序验证；业务 DLL 版本不因纯资源变化伪递增。

### 2026-07-21：设备版本快照和工作台生产模式配置

- 问题：颜色校准在完成连接、设备编号和型号判断后还必须读取下位机版本；mOS MyScan 有主控板和投图板，其他系列只有主控板，不能把投图板异常误判为所有机型故障。
- 结论：DeviceCmd 在型号确认后读取 `0x14/0x15` 主控板版本，只有 MyScan Profile 读取 `0x12/0x13` 投图板版本；版本和读取状态进入固定 POD、DeviceSessionHost 快照、SettingsUI/CalibrationColorUI 上下文、工作台 JSON 和日志。
- 影响：DeviceCmd ABI/schema 升级为 5；SettingsUI ABI/schema 为 7/5；CalibrationColorUI ABI/schema 为 6/5。生产工作台准入由 ConfigCenter 的 `device.practiceAllowProductionMode`、`device.orderCreateAllowProductionMode` 独立控制，默认 true/false。

### 2026-07-20：只有创建订单扫描要求正式设备编号

- 问题：新设备在生产调试阶段直到最后打包才写入设备编号，型号代码也可能尚未写入；若完全拒绝未写号设备，生产练习和校准流程无法运行，但正式订单扫描需要真实设备编号进行约束。
- 结论：DeviceCmd 继续如实检测并分别保存 reported/effective 身份，不理解创建、练习或校准模式。DeviceSessionHost 在工作流层应用准入策略：只有创建订单扫描使用 `RequireProgrammedDeviceNumber`；练习、颜色校准和后续三维校准使用 `AllowProductionCompatibilityIdentity`。
- 结论：生产设备已识别系列后可获得带 `CompatibilityDefault` 来源的默认编号/型号供练习复用；创建模式无真实编号时返回稳定状态 14，关闭设备会话、留在 Order，并禁止创建或激活 Scan/Process/Send。
- 结论：准入结果写入工作台 `deviceIdentity` JSON，reported/effective、来源、生产/兼容标志和产品身份必须成组传递。自动化 `--smoke-main` 可显式旁路物理 USB，但必须记录 `automationBypass=true`，正式配置没有该开关。
- 影响：MainExe 0.5.1、DeviceCmd 0.6.1；SettingsUI/CalibrationColorUI 继续保持 0.6.0。状态 14 只用于创建流程，颜色校准不因设备编号未写入而拦截，三维校准后续沿用相同规则。

### 2026-07-20：设备型号检测保留真实值、有效值和完整步骤证据

- 问题：旧设备、生产模式和不同下位机版本会让 D9/C7/CE 出现无回包、坏包、校验失败、未初始化或非法值；如果只保存一个最终编号/型号，兼容默认值会伪装成真实设备数据，后续权限、校准和诊断无法判断来源。
- 结论：固定检测顺序为连接/USB -> D4/D9 -> D9 校验失败时 C2/C7 -> CD/CE -> 产品目录。D9 只有求和校验失败表示未写设备编号并进入生产模式；无回包、普通坏包和合法帧中的非法编号分别返回明确失败状态。
- 结论：`MeyerDeviceDetectionRecord` 分别保存 `reportedDeviceNumber/reportedModelCode` 和 `effectiveDeviceNumber/effectiveModelCode`，同时保存值来源、生产模式、兼容标志、D9/C7/CE 状态和诊断文本。兼容值不得覆盖 reported 字段，也不得标成设备上报。
- 结论：生产模式 C7 无回包形成 mOS MyScan 候选，收到 C7 形成 mOS MyScan 5/6 候选；5/6 区分规则待定。CE 旧固件、坏包、校验/初始化/值异常可采用带来源的兼容值继续；C7 与 CE 或编号前缀与 CE 冲突必须拦截。
- 优化：流程图在“已写合法编号但 CE 不可用”分支统一写了 `62000020`。实现对已登记编号前缀选择同系列标准型号代码（20/27/53/55），只有未知前缀才回退 `62000020`；这样避免把已知 MyScan 5/5H 错套成 MyScan 3，同时仍以 CompatibilityDefault 标记，不能冒充精确产品。
- 影响：DeviceCmd 0.6.0/ABI 4、MainExe 0.5.0、SettingsUI/CalibrationColorUI 0.6.0 已贯通完整 POD。设置页一次提示有效编号、产品、型号代码、生产/兼容来源，颜色校准只消费快照，不解析协议。

### 2026-07-20：产品系列、具体产品和协议 Profile 分层

- 问题：设备编号前缀、型号代码、三/五/六代协议能力和具体销售型号曾混用；旧逻辑按型号代码首位判断 3/5/6，会把全部以 6 开头的已知代码错误识别为 MyScan 6。
- 结论：MyScan3/5/5H/6/6Wireless 只表示协议能力 Profile；产品系列单独表示 mOS MyScan、mOS MyScan 5、mOS MyScan 6；具体产品使用稳定 ID 和完整 8 位型号代码精确映射。
- 结论：D4/D9 读取 13 位设备编号，编号前缀只给系列候选；CD/CE 读取型号代码。旧有线与无线 0xCE 分支解析，未写设备编号继续识别，前缀/代码冲突返回 Conflict，未知时不猜测。
- 影响：DeviceCmd 0.5.0/ABI 3 新增 ProductCatalog 和产品身份 POD；MainExe 0.4.0、SettingsUI/CalibrationColorUI 0.5.0 只转发和展示识别结果，不解析协议。

### 2026-07-20：设备编号/型号代码回包集中解析与公共分级弹窗

- 问题：颜色校准需要先验证 `0xD4/0xD9` 设备编号，再读取 `0xCD/0xCE` 型号代码；若让 SettingsUI、校准 UI 各自解析回包，协议规则和错误处理会快速分散。
- 结论：所有命令回包的查找、长度/命令码校验、逐字节转换和型号映射统一留在纯 C++ DeviceCmd。内部可用 `std::string`/字节容器，跨 DLL 只传固定 POD、固定 UTF-8 数组和原始字节，不传 STL/Qt 容器。
- 结论：颜色校准固定顺序改为工作台门禁 -> 连接 -> USB3 -> D4/D9 设备编号 -> 显示编号 -> CD/CE 型号代码 -> 注入快照 -> 创建弹窗。旧有线 `0xCE` 前 8 字节只有完整代码命中中央目录时才映射具体产品；无线布局单独解析。
- 结论：普通分级弹窗归 UIComponents，并通过独立 C ABI 提供；单按钮分信息/成功/错误，双按钮分警告/高危。业务模块负责 `tr()`、日志和后续动作，公共 UI 只负责视觉和返回选择结果。
- 影响：该阶段完成 D4/D9 和公共弹窗基础链路；产品身份分层已由同日较新的 DeviceCmd 0.5.0 决策替代。实机已确认设备编号读取成功，CD/CE 仍超时。

### 2026-07-17：颜色校准设备预检链路

- 问题：颜色校准打开前必须排除创建/练习工作台占用设备，检查连接与 USB2/USB3，并读取设备信息和型号；任何 UI 自行打开 USB 都会破坏单会话边界。
- 结论：MainExe 落地 `DeviceSessionHost`，工作线程动态调用 DeviceCmd -> DeviceTransport；SettingsUI 只执行同步预检回调并传 POD，CalibrationColorUI 无有效快照拒绝创建。
- 结论：Cypress 有线机型共用自动枚举与速度检测；MyScan 6 Wireless 连接方法未开发时明确返回未支持。当时按无线协议预留区识别的初始口径，已被同日确认的“旧有线前 8 字节型号代码 + 无线授权布局分支解析”替代。
- 验证：当时 Transport、DeviceCmd、SettingsUI、CalibrationColorUI 和 MainExe smoke 均通过；实机确认 Cypress/USB3，CD 后 CE 超时。当时“直接 DeviceInfoReadFailed”行为已由 2026-07-20 DeviceCmd 0.6.0 决策替代为“保留 FirmwareTooOld 诊断并使用独立 compatibility effective 值”。

### 2026-07-16：设备命令层和单设备会话所有权

- 问题：设置、校准、扫描入口和扫描页面都需要设备信息/命令；若各模块直接加载 DeviceTransport，会形成多 USB 句柄、命令交叉、状态不一致和难以复现的释放问题。
- 结论：新增纯 C++ `MyDeviceCmd / MeyerScan_DeviceCmd.dll`，负责 A 类协议、型号能力目录、状态快照、灯光和采集启停；`MyDeviceTransport` 只保留原始字节/流和组帧。
- 结论：MainExe 嵌入形态或独立 ScanReconstructStudio 宿主各自只维护一个 DeviceSessionHost/DeviceCmd 句柄；UI 只提交动作并读取带 `validFields` 的快照，采集中禁止需要 A 类响应的查询。
- 结论：MyScan 6 Wireless 协议按 2025-08-08 文档核对；3/5/5H/6 先建立可扩展目录并明确待验证。测试模拟后端只证明软件链路，不代表硬件成功。
- 影响：设置设备信息、校准和扫描后续统一接入 DeviceSessionHost；任何页面直接包含 DeviceTransport 头或打开 USB 都视为架构偏移。

### 2026-07-16：按正式协议扩展设备命令覆盖

- 问题：初版 DeviceCmd 只实现机器码、版本、电池、设备信息、灯光和采集启停，协议中相机参数、标定、曝光、帧率和固件命令尚未形成稳定接口。
- 结论：依据 `美亚无线口内扫描仪通讯协议-20250808.pdf`，补齐协议表 48 组 A 类命令的命令码、响应码、固定长度检查和语义 API；大块数据使用调用方管理的固定 POD 结构，不把 STL/Qt 容器穿过 ABI。
- 结论：固化类统一检查 `0xFF/0x00` 状态，固件模块只提供擦除进度和分包烧写原语，不把固件文件读取、版本策略、断点续传和升级 UI 混入 DeviceCmd。
- 验证：模拟后端覆盖基础状态、相机参数、颜色矩阵、窗口位置、温度、帧率、两路相机标定、颜色标定、设备信息、曝光和固件分包；`DeviceCmdTest --smoke` 全部通过。
- 影响：SettingsUI、Calibration3DUI、CalibrationColorUI 和 ScanWorkflowUI 后续仍只接入 DeviceSessionHost，不得直接调用协议函数或创建自己的 USB 会话；真实设备和多机型验证仍是后续工作。

### 2026-07-16：新增设备命令代码中文注释复核

- 对 `MyDeviceCmd` 公共 ABI、协议编解码、动态 Transport 适配、模拟后端和测试程序补充函数级及关键代码中文注释。
- 注释重点说明固定 POD 跨 DLL、Windows 绝对路径加载、线程锁和异常边界、大小端转换、固定长度响应、资源释放顺序及测试断言目的。
- 验证：`CheckSourceCommentSafety.ps1` 为 0 错误/0 警告，MyDeviceCmd CMake Release 和 CTest `1/1` 通过。

### 2026-07-15：UI 数据注入、服务读模型和动态 ABI 门禁

- 问题：CaseUI/SettingsUI 自行连接 RuntimeDataCenter/Database 会让 UI、数据读取和基础设施生命周期耦合；CaseOrderService 写入新表后，旧表快照又无法显示新订单。
- 结论：业务 UI 只接收 MainExe 注入的版本化只读 JSON；CaseOrderService 提供患者/订单轻量列表查询，MainExe 按稳定 ID 与 RuntimeDataCenter 旧库快照合并，新数据优先且不硬写不稳定旧表。
- 结论：ScanSchemaService 是扫描步骤规则唯一生产者；OrderCreateUI 只采集配置，Scan/Process 只按稳定 `steps.code` 翻译和显示。
- 结论：所有返回自研 C++ 虚接口的动态 DLL 必须先通过 `GetMeyerModuleApiVersion()` 门禁，且使用应用目录绝对路径加载。
- 结论：建单 Confirm/Next 已接 CaseOrderService 最小保存链路；宿主补齐患者号、订单号、病例号、状态和创建时间，Next 仅在保存成功后进入扫描。
- 影响：CaseUI/SettingsUI 和测试宿主不再依赖数据库；患者/订单新旧 schema 迁移期间由宿主合并读模型，正式 migration/事务/CRUD 仍需继续完成。

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
- 结论：ScanSchemaService 根据 OrderCreateUI 收集的配置生成 `scanProcess`；MainExe/Workflow 只转发，Scan/Process 只消费稳定 `steps.code`。
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
- 标准写链路：`UI 动作 -> MainExe/Workflow -> CaseOrderService -> DatabaseQtAdapter -> Database`；只读页面通过 MainExe 注入快照，不直接持有数据服务。

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

> **文档版本**：v3.2（2026-07-21，补充下位机版本快照和生产模式配置决策）
