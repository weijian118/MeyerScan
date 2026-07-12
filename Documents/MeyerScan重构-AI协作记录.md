# MeyerScan 重构 — AI 协作记录

> **文档目的**：记录项目重构过程中与 AI 的沟通要求、AI 回复建议、采纳的代码设计方案、功能新增和修改项，便于追溯。
>
> **记录原则**：仅记录与重构相关的沟通内容，且以采纳 AI 建议的决策为主。按时间倒序排列。历史条目用于追溯当时状态；当前开发约束以最新日期条目、`MeyerScan重构任务总览.md` 和 `MeyerScan架构设计与接口规范.md` 为准。
>
> **权威文档位置（2026-07-12 起）**：`F:\MeyerScan\Documents\MeyerScan重构-AI协作记录.md`。`D:\wj\重构文档` 下同名文件仅为同步镜像；新增记录必须写清用户要求、采纳结论、代码范围、验证命令和未完成项。

---

## 2026-07-13 - 生产测试数据隔离与最终复核

### 用户要求

用户要求继续检查近期新增模块注释，并依据重构方案复核全部模块的职责、依赖和交互是否偏移。

### 代码处理

1. 在上一轮全模块失败路径和中文实现注释复核基础上，继续搜索生产代码中的固定患者、订单、医生、技工所、牙位和治疗方案示例值。
2. OrderCreateUI 无上下文时不再预填固定患者号、姓名、年龄、生日、医生、技工所、订单号和示例牙位；医生、技工所和生日仅显示 `tr()` 包裹的选择提示。
3. `SetOrderContextJson()` 先解析局部候选 JSON，成功后再替换 pending context；非法 JSON 返回 `false` 并保留上一份有效上下文。smoke 新增空白生产状态、非法 JSON 不污染缓存、Shutdown 后重新 Init 等断言。
4. MainExe 手工创建改为空白患者/订单上下文，字段统一为 `patientId` / `orderId` 并补充标准 `source`；练习模式保留独立默认数据，但统一使用 `PRACTICE_*` 标识。SendUI 同步删除生产界面的测试患者、默认医生和 `LOCAL_ORDER`。
5. MainExe 保持 v0.1.7，OrderCreateUI 升级到 v0.5.3；CMake、代码版本和 Windows `Version.rc` 已一致。

### 架构结论

- 未发现 UI 模块直接访问 Database；CaseUI/SettingsUI 继续只读 RuntimeDataCenter，SendUI 继续只上报动作而不实现网络、压缩、上传或真实导出。
- WorkspaceShell 仍是 Order/Scan/Process/Send 唯一步骤导航所有者；Scan/Process 离开时释放 QVTK/VTK/OpenGL，MainExe 进入扫描前释放 CaseUI。
- 未发现运行路径使用 `QDir::currentPath()`；业务源码样式仍只通过 `MeyerQtModuleUtils::ApplyModuleQss()` 公共入口加载。
- 当前没有新增模块边界偏移。下一步应优先完成真实建单保存/查询、ScanSchema/Workflow 服务和设备算法链路。

### 验证结果

- VS2015 根 `MeyerScan_AllModules.sln` Release x64 与 `cmake --build F:\MeyerScan\build --config Release --parallel 4` 均通过；仅保留外部登录头文件原有 C4819/C4091 警告。
- OrderCreateUI 增强 smoke、MainExe `--smoke`、`--smoke-main`、`--smoke-external-order` 均返回 0。
- 最新 `versionList_20260713_072234_645.json` 共 24 项，0 缺失、0 文件/代码版本不一致、0 `codeVersionError`；MainExe 为文件/代码版本 0.1.7，OrderCreateUI 为 0.5.3。
- 本轮代码、模块文档和四份权威文档在验证通过后统一提交 GitHub，并通过整体备份脚本同步到 `F:\MeyerScan-Reposit` 本地仓库；提交信息使用中文并详细描述变更和验证结果。

---

## 2026-07-12 - 全模块代码注释、失败路径与架构偏移复核

### 用户要求

用户要求检查全部模块代码，重点补充近期新增模块的中文注释，并对照重构方案检查职责、依赖和交互是否发生偏移。

### 代码处理

1. 重点补充 SendUI、ScanWorkflowUI、DataProcessUI、ScanReconstructStudio、OrderCreateUI、WorkspaceShell、UIResources 及其测试宿主的函数说明和函数内部“如何实现”注释；同步补充 MainExe、HomeUI、CaseUI、SettingsUI 的失败路径说明。
2. Send/Scan/Process 的非法 JSON 改为返回 false 并保留上一份有效上下文；测试先写有效上下文再写非法 JSON，验证 UI 字段/流程没有被破坏。
3. Scan/Process 的 CreateWidget 不再隐式 Activate，由 MainExe 或 ScanReconstructStudio 完成挂载后显式激活；当前步骤禁用时选择首个 enabled 步骤，全部禁用时保持无活动步骤并清理旧显示数据。
4. MainExe 检查 ConfigCenter、Permission、UIComponents、RuntimeDataCenter、各 UI、WorkspaceShell 和 ExternalLaunchAdapter 的 Init/上下文返回值；目标页创建失败时不推进步骤，刷新上下文失败记录 `ContextRejected`。
5. SettingsUI 检查两个校准子模块 Init；失败时 Shutdown、清空接口并只降级对应校准能力。Home/Case/Order 的 UIComponents 初始化失败、Workspace/Order/Scan/Process/Send/Studio 的 Logger 初始化失败均清空半初始化接口并走规定降级。
6. SendUI 删除测试患者、默认医生和 `LOCAL_ORDER` 生产伪数据，新增末尾动作 `SendUIActionDataFormatChanged = 7`；程序填充使用信号阻断，真实客户选择才上报。真实导出、压缩、邮件和上传仍未进入 UI 模块。
7. HomeUITest/CaseUITest 补齐 Init/CreateWidget 空值保护；ScanReconstructStudio 的 `SwitchToStep()` 返回 bool，子模块 Init/上下文/页面创建失败向 Initialize/RunSmoke 传播。

### 架构复核结论

- 未发现近期 UI 模块直接访问 MyDatabase、配置文件或权限文件；CaseUI/SettingsUI 继续只读 RuntimeDataCenter，写操作仍归服务层。
- 未发现 SendUI 内实现网络、压缩、邮件、上传或真实导出；页面只展示上下文并上报稳定动作 ID。
- 源码实际 `setStyleSheet()` 仍只有 `Common/include/MeyerQtModuleUtils.h` 公共入口；未发现运行路径使用 `QDir::currentPath()`。
- WorkspaceShell 继续只拥有唯一步骤导航和页面容器；Order/Scan/Process/Send 只提供内容页。Scan/Process 离开时释放 QVTK/VTK/OpenGL，MainExe 进入扫描前释放 CaseUI。
- 当前拆分无新增架构偏移。下一优先项仍是真实患者/订单保存查询、ScanSchema/Workflow 服务和扫描设备/算法链路，不继续增加只有占位接口的模块。

### 版本与验证

- 版本同步为 MainExe v0.1.7、HomeUI/CaseUI v0.3.2、SettingsUI v0.2.1、OrderCreateUI v0.5.3、WorkspaceShell/ScanReconstructStudio v0.1.3、ScanWorkflowUI/DataProcessUI v0.2.3、SendUI v0.1.2；UIResources 因资源内容未变保持 v0.1.3。
- VS2015 根 `MeyerScan_AllModules.sln /t:Rebuild` 和 `cmake --build F:\MeyerScan\build --config Release --parallel 4` 均通过；只保留外部登录模块头文件既有 C4819/C4091 警告。
- 24 项自研测试/集成链路及 `MeyerScan.exe --smoke` 全部返回 0；`--smoke-main` 日志覆盖 Home -> Practice -> Scan -> Process -> Home -> Case -> 释放 Case，外部 JSON smoke 覆盖 ExternalLaunchAdapter -> 后台首页创建入口 -> WorkspaceShell/OrderCreateUI。
- 最后一轮源码调整后重新执行 VS2015/CMake 增量构建、五个受影响模块测试和 MainExe 集成 smoke；最新清单 `versionList_20260712_184237_111.json` 共 24 项，0 缺失、0 文件/代码版本不一致、0 `codeVersionError`。`CheckSourceCommentSafety.ps1` 结果为 0 错误、0 警告，`git diff --check` 无空白错误。
- 本轮按用户当前要求只修改代码和文档，未提交 GitHub 或本地仓库。

---

## 2026-07-12 - 文档迁移、注释安全、治疗类型 hover 与 MainExe 重验证

### 用户要求

用户明确授权 `F:\MeyerScan`、`D:\wj\重构文档` 下受控开发操作并允许调整 Codex 配置；要求建立防止中文注释换行破坏代码的规则；把 D 盘四份核心文档整理并迁入 `F:\MeyerScan\Documents`；核实最近两次变更是否经过 minMain；修复治疗类型 hover 和选择种植体后牙弓略微缩小。

### 采纳与实现

1. `config.toml` 改为 `approval_policy = "never"`，并把 `F:\MeyerScan`、`F:\MeyerScan-Reposit` 标记为 trusted；仍保留递归删除前路径边界校验和不覆盖用户未提交修改的规则。
2. 四份核心文档统一复制到仓库 `Documents` 并建立 `Documents/README.md`：任务总览管范围，架构规范管边界，进度文档管状态，AI 记录管时间线。D 盘同名文件改为同步镜像。
3. 新增 `tools/CheckSourceCommentSafety.ps1`，检查中文源码 UTF-8 BOM、`.rc` 纯 ASCII、`//` 注释末尾反斜杠和疑似注释/代码粘连。清理 Logger 宏示例中真实存在的注释行尾续行符；全仓最终为 0 错误、0 警告。
4. OrderCreateUI 升级到 v0.5.2：按钮增加稳定 `treatmentCode` 属性；前四种 h PNG 的白色图形叠加到从 b PNG 自动提取主色的局部圆底上，QSS 不再给整个按钮绘制绿色矩形；种植体保留独立宽按钮整行状态。
5. 扫描流程预览固定为单行高度并把完整内容放入 tooltip，避免种植体增加扫描杆步骤后 QLabel 换行增高、向上挤压牙弓。
6. smoke 增加 hover 图切换、按钮背景像素、QSS 类型属性和种植体前后牙弓尺寸不变断言；截图宿主增加 `--capture-hover-type`。
7. 首次 smoke 暴露单模块测试加载了旧 UIResources DLL。已把 UIResources 加入 OrderCreateUITest 的 VS2015/CMake 构建依赖和 PostBuild 复制，根方案测试依赖同步补齐。UIResources 升级到 v0.1.3。
8. 历史 `minMain` 已由 `MyMainExe/MeyerScan.exe --smoke-main` 取代，没有独立 minMain 项目。此次对根聚合输出和单模块输出都重新执行该验证。
9. 首次三档截图的实际文件尺寸都被桌面限制为 1028x749，不能作为多分辨率证据。截图宿主改为 `WA_DontShowOnScreen + setFixedSize` 离屏画布，并校验目标/实际尺寸；2560x1440 的真实截图进一步暴露纵向过空，已将 Scan Plan 最大高度限制为 1060px 并垂直居中。
10. 最终验证时曾并行启动 VS2015 根构建和 CMake UIResources 目标，两者争用同一 `MyUIResources\bin\Release` 并触发 LNK1104。已改为串行构建并使用 `/nodeReuse:false` 复验通过，同时把该限制写入 PowerShell/构建规范。

### 最终验证

- `MeyerScan_OrderCreateUI.sln` Rebuild、根 `MeyerScan_AllModules.sln /t:Rebuild` 和 CMake 全量 Release 构建通过；仅有外部登录模块头文件既有 C4819/C4091 警告。
- 根/模块 `OrderCreateUITest.exe --smoke`、`UIResourcesTest.exe --smoke`、根 `MeyerScan.exe --smoke`、根/单模块 `MeyerScan.exe --smoke-main`、外部 JSON 建单 smoke 均返回 0。
- 三张 PNG 已读取文件头确认实际为 1366x768、1920x1080、2560x1440；Missing Tooth/Implant hover 通过人工检查，普通类型没有矩形背景，1x/2x h 图均可见，牙弓未因种植体缩小，2K 上下颌间距不再随整屏高度拉大。
- 本轮三档截图和视频对照 contact sheet 保存在 `D:\wj\重构文档\验收截图\2026-07-12_OrderCreateUI`，作为不进入源码仓库的人工视觉证据。
- 根 `MeyerScan.exe` 在最终资源 DLL 之后再次 Rebuild，生成时间为 2026-07-12 17:00:15。本轮最终最新 versionList 共 24 项，0 缺失、0 版本不一致、0 代码版本错误；OrderCreateUI 文件/代码版本为 0.5.2，UIResources 为 0.1.3。清单文件名按启动时间生成，以 `logs/versionList` 中时间最新者为准。

---

## 2026-07-12 - OrderCreateUI 五种修复类型与牙位/桥交互修正

### 用户要求

用户提供当前软件“扫描方案设置”截图，要求治疗方案只保留全冠、缺失牙、嵌体、贴面、种植体五种修复类型，按指定 `1/3/4/5/7` 单牙图片序号显示；修复左右牙位识别颠倒；相邻已选牙显示桥连接点；缩小过空的 Scan Plan 区域；按钮按 b/h 和 1x/2x 图片切换，并提交 GitHub 与本地仓库。

### 代码调整

1. `MyOrderCreateUI` 升级到 v0.5.1，移除 `inner_crown` 和 `bridge` 类型按钮。类型编码固定为 `crown/missing/inlay/veneer/implant`，叠加图序号固定为 `1/3/4/5/7`。
2. 通过实际 mask 与单牙 PNG 的像素重叠关系校正映射：上颌灰度递增对应 `11..18,21..28`，下颌对应 `31..38,41..48`；两颌桥 mask 前半段顺序同步修正。
3. bridge 改为相邻牙连接状态，不再当修复类型。任意两颗同颌直接相邻且均已设置治疗方案时显示空心连接点，点击后显示实心连接点；外部 `bridgeConnectors` 必须通过格式、相邻关系和两端牙位存在性校验。
4. 治疗类型按钮普通态使用 `*_b.png`，hover/选中态使用 `*_h.png`；屏幕达到 2560x1440 档位时加载 `*_2x.png` 源图。高亮图为白色，因此 QSS 使用深绿色高亮背景保证对比度。
5. 多分辨率布局继续使用 Qt Layout：左栏最小 380px、类型按钮最小 82px；中间分栏继续伸缩，但 Scan Plan 内容最大 980px 并居中，避免 2K/4K 横向拉空。
6. 新增内部纯规则头 `TreatmentPlanResourceRules.h`，生产代码与测试共用同一份映射。测试不直接链接 `ToothTreatmentPlanWidget` / `OrderCreateUIImpl` 等 DLL 内部 C++ 类，桥候选通过 QWidget 只读动态属性检查，不扩大公共 ABI。
7. `MyUIResources` 升级到 v0.1.2，重新打包建单 QSS 及五类型 b/h、1x/2x 资源。

### 验证结果

- `MeyerScan_OrderCreateUI.sln` 和根 `MeyerScan_AllModules.sln` 的 VS2015 Release x64 构建通过。
- 模块输出和根输出 `OrderCreateUITest.exe --smoke` 返回 0；新增断言覆盖五类型、`1/3/4/5/7`、牙位/桥 mask 顺序、hover 图、2K 阈值、桥候选和无效桥过滤。
- `UIResourcesTest.exe --smoke` 返回 0，资源清单保持 608 项。
- 1366x768、1920x1080、2560x1440 截图人工检查通过；低分辨率无 `Missing Tooth` 裁切，2K 中栏内容不过度拉伸。

---

## 2026-07-10 - UI 资源 DLL、三处核心界面复现与全量复核

### 用户要求

用户要求评估将 icon、QSS 等资源编译进 DLL 的可行性和架构兼容方式，整理 PowerShell 常见交互问题，检查全部模块代码和文档，并再次按历史参考界面优化首页、浏览和建单页面。

### 采纳方案

1. 新增 `MyUIResources`，输出 `MeyerScan_UIResources.dll`。各模块继续维护自己的 `Resources` 源文件，构建时统一生成 qrc，经 `rcc -binary` 生成 RCC 数据，再作为 Windows `RCDATA` 嵌入 DLL。
2. 统一 Qt 资源路径为 `:/MeyerScan/Modules/<ProjectName>/...`；MainExe 在首个 UI 页面创建前注册资源，并以 `QLibrary::PreventUnloadHint` 保持资源 DLL 在进程生命周期内有效。
3. 正式安装包只交付资源 DLL，不展开 UI PNG/QSS 散文件；源码树资源和旧安装目录散文件仅用于开发/历史兼容降级。资源 DLL 能降低普通误改和误删概率，但不是加密，安装包仍要增加哈希、签名和修复能力。
4. 当前不拆分 Icon.dll 与 Qss.dll。两类资源加载时机、版本和升级原子性一致，拆分只会增加加载与发布故障点；仅在资源体积或 OEM 裁剪出现真实需求后按业务域拆包。

### 代码与界面调整

1. HomeUI v0.3.1：完整背景按比例绘制，四入口使用响应式 Layout，中英组合 Logo 和校准/云端/帮助/最小化/关闭入口保持清晰。
2. CaseUI v0.3.1：恢复 Orders/Patients 页面导航，订单改为响应式卡片流；1920 宽四列、1366 宽三列，顶部补齐云端、截图、设置、首页、最小化、关闭动作。
3. OrderCreateUI v0.5.0：三栏改用不可折叠 QSplitter，中间牙弓获得主要伸缩空间；治疗类型三列布局容纳长翻译，窄屏流程动作两行两列，左侧长表单使用滚动区。
4. 修复 ScanWorkflowUI/DataProcessUI 的 QVTK/VTK 释放顺序和延迟析构重入问题，ScanReconstructStudio 的 Scan -> Process -> Scan 二次创建不再访问冲突。
5. 修复 `VersionManagerTest` 覆盖 MainExe 正式 `version_modules.json` 的问题，测试改在 `test_runtime/VersionManagerTest` 使用隔离配置、模块副本和日志目录。

### PowerShell 约束

新增 `PowerShell开发与自动化脚本规范.md`，并同步 `F:\MeyerScan\tools\README.md`。规则覆盖 Windows PowerShell 5.1 语法、UTF-8 BOM、`.rc` ASCII、`$LASTEXITCODE`、rg/robocopy 特殊退出码、GUI 进程退出码、`-LiteralPath`、删除边界、幂等、Markdown 显式 UTF-8 读取和大仓库递归枚举限制。

### 最终验证

- VS2015 根 `MeyerScan_AllModules.sln` Release x64 构建通过。
- CMake 根 `F:\MeyerScan\build` Release 构建通过。
- 24 项模块/主链路测试全部返回 0，包括 UIResources、登录宿主、ScanReconstructStudio 和 MeyerScan `--smoke-main`。
- 资源清单自动收集 608 个资源；最新 versionList 为 24 项、0 缺失、0 版本不一致、0 `codeVersionError`。
- VersionManagerTest 前后正式清单 SHA-256 不变。
- 首页、浏览、建单完成 1920x1080 与 1366x768 共 6 张截图复核，无控件重叠或不可操作区域。

---

## 2026-07-10 - glm52 建议复核、UI 归属修正与全模块回归

### 用户要求

用户要求继续上一轮未完成任务，审阅 `glm52` 下 01-04 四份重构建议和另一模型编写的 `2026-07-09_UI优化-统一标题栏-步骤条.md`，对照当前代码、模块拆分和全部重构文档判断哪些建议应采纳，并完成必要优化。

### 评审结论

1. 采纳服务层补齐、Qt Layout/DPI 统一、路径可移植性、版本一致性检查和按风险增强测试等建议。
2. 调整采纳公共契约：当前不把 `Core.lib` 作为前置，只有稳定类型出现真实跨模块重复后才抽取最小公共头/静态库。
3. 不采纳固定字段 `CaseEntity.lib`：患者/订单字段高频变化，继续由 CaseOrderService DTO、UTF-8 JSON、`schemaVersion`、`extensions` 和数据库 migration 承接。
4. IPC 只服务 ScanReconstructStudio 独立 EXE 形态，不阻塞 DLL 嵌入链路，也不并入通用 Core 大包。
5. 不采纳自研 UI/VTK DLL 运行期热替换；自动更新在 MeyerScan.exe 退出后覆盖文件。
6. `MyCaseManager` 继续作为只读历史 schema 参考，排除活跃构建、版本清单和安装包，确认无迁移价值后再删除。

### UI 和代码修正

1. 废止 MainExe 绘制所有页面通用 36px 可见标题栏的方案。MainExe 只保留 `Qt::FramelessWindowHint + showFullScreen()`、单内容区和窗口动作执行。
2. HomeUI、CaseUI、OrderScanWorkspaceShell 各自绘制符合页面语义的品牌/顶部区域，并用稳定动作 ID 把最小化、关闭、返回等动作上报 MainExe。
3. Order/Scan/Process/Send 步骤导航唯一归 OrderScanWorkspaceShell；OrderCreateUI 删除第二套步骤条和 `OrderCreateActionStepClicked`。
4. OrderCreateUI 源码局部样式全部迁入 `Resources/qss/order_create.qss`；源码中的 `setStyleSheet()` 只剩公共 QSS 加载入口。
5. ScanReconstructStudio 补齐 `MeyerScan_ScanReconstructStudio.dll`，与独立 EXE 共用实现并分别维护文件版本；嵌入外层工作台时必须避免双壳/双导航。
6. 修正 ConfigCenter、Permission、Database、CaseOrderService 代码注释中“等待 Core.lib 落地”的旧 TODO，不改变现有 ABI 和业务逻辑。
7. 新增 `glm52/05-2026-07-10-建议复核与采纳结论.md`，重写 7 月 9 日 UI 记录，并同步四份核心文档及相关模块 README/CHANGELOG。

### 验证结果

- VS2015 根 `MeyerScan_AllModules.sln` Release x64 构建通过。
- CMake 根 `F:\MeyerScan\build` Release 重新配置并构建通过；仅保留外部既有登录头文件 C4819/C4091 警告。
- DatabaseTest 24/24；ConfigCenterTest、PermissionTest、CaseOrderServiceTest、UIComponentsTest 全部通过。
- `HomeUITest --smoke`、`CaseUITest --smoke`、`OrderCreateUITest --smoke`、`OrderScanWorkspaceShellTest`、`ScanReconstructStudio --smoke`、`MeyerScan --smoke-main` 均返回 0。
- 最新运行时版本清单包含 23 个自研 EXE/DLL，缺失数、版本不一致数和 `codeVersionError` 均为 0。

### 本地备份清理补充

1. 复核发现 `F:\MeyerScan-Reposit` 仍遗留大量 Qt、VTK、OpenCV、VC/UCRT 等第三方 DLL。根因不是 `/XF` 规则缺失，而是 `robocopy /MIR` 不会删除目标中已被 `/XD`、`/XF` 排除的历史文件。
2. `BackupToLocalRepository.ps1` 新增镜像后主动清理：先按深度删除排除目录，再按与 `/XF` 相同的文件名通配符删除排除文件；所有删除操作先验证绝对路径位于备份根目录内。
3. 脚本改为带 BOM 的 UTF-8。Windows PowerShell 5.1 会把无 BOM UTF-8 按系统 ANSI 代码页读取，中文注释可能导致后续代码行无法按预期解析；该编码要求必须长期保留。
4. 首次执行命中 33 个历史目录和 889 个历史文件，本地仓库提交 `50009af` 清除了 355 个已被 Git 跟踪的第三方文件；完整规则复查结果为 0 个遗留命中。
5. 再次执行完整备份时，目录和文件清理计数均为 0，Git 无变化可提交，确认脚本具备幂等性。

## 2026-07-07 - Scan/Process 流程按钮交互、鼠标中心缩放与 SendUI 初版接入

### 用户要求

用户要求 Scan 和 Process 页顶部扫描流程按钮 hover 时显示手型并提供 tooltip；点击流程按钮后按扫描部位切换界面数据显示；Process 左下角提示框与 Scan 不同，且 Process 不放底部中间的 Start/Pause 扫描控制；VTK 视图滚轮缩放要以鼠标所在位置为中心，缩放范围受控，不能先越界再“拉回”；同时根据截图启动开发“发送”模块。

### 本轮处理

1. `MyScanWorkflowUI` 升级为 v0.2.1：顶部 `scanProcess.steps` 按钮设置 `Qt::PointingHandCursor`、tooltip、checkable 选中态；点击按钮后更新当前扫描部位状态、刷新占位数据显示并上报动作。
2. `MyDataProcessUI` 升级为 v0.2.1：顶部处理流程按钮与 Scan 使用同一份 `scanProcess.steps`，同样支持手型 hover、tooltip、点击切换；新增独立 `Process Hint` 提示框；底部仅保留 Previous / 状态 / Next，不加入 Scan 页的 Start/Pause。
3. Scan / Process 的 QVTK 显示区新增轻量派生控件，只接管滚轮缩放：先按当前缩放值计算目标缩放并夹紧到范围内，再按鼠标位置读缩放前后世界坐标，平移相机位置和焦点，使鼠标指向的世界点尽量保持在同一屏幕位置。
4. 修复 Scan / Process 自定义 ViewerWidget 放在匿名命名空间导致 VS2015 与头文件前置声明类型不一致的编译问题。
5. 新增 `MySendUI` 模块，输出 `MeyerScan_SendUI.dll` 和 `SendUITest.exe`；提供案例信息区、发送动作区、Previous / Finish 流程按钮和 `ISendUI` 动作回调。模块只做 UI 和动作上报，不做真实导出、压缩、邮件发送或上传。
6. MainExe 升级为 v0.1.3：动态加载 `MeyerScan_SendUI.dll`，创建模式 DataProcess 的 Next 进入 Send 步骤；进入 Send 前释放 ScanWorkflowUI / DataProcessUI 重资源；`version_modules.json`、VS2015/CMake 和根聚合方案纳入 SendUI。
7. 修复 CMake 构建的 `SendUITest.exe` 输出目录缺少 `MeyerScan_Logger.dll` 导致无输出退出的问题；CMake 现在同步复制 Logger 和 UIComponents 运行依赖。
8. 更新总览、架构规范、进度跟踪、模块 README/CHANGELOG，明确 SendUI 的 UI 边界、Scan/Process 按钮和缩放交互要求。

### 验证结果

- `cmake --build build --config Release --target MeyerScan SendUITest ScanWorkflowUITest DataProcessUITest` 构建通过。
- `SendUITest.exe` 返回 0，覆盖模块初始化、上下文字段填充和 Finish 按钮存在性。
- `ScanWorkflowUITest.exe` 返回 0，覆盖流程按钮渲染、tooltip、手型光标和点击回调。
- `DataProcessUITest.exe` 返回 0，覆盖流程按钮渲染、tooltip、手型光标和点击回调。
- `MeyerScan.exe --smoke-main` 返回 0，覆盖 MainExe 创建工作台 Scan → Process → Send 的阶段性链路。

## 2026-07-07 - 创建/练习工作台集成、Scan/Process 切换与分辨率收敛

### 用户要求

用户反馈 Order、Scan、Process 页面切换按钮无法切换；要求 Order 界面分辨率适配更完善；创建和练习模块右上角只保留最小化和关闭；要求启动练习模块，练习模块只包含 Scan 和 Process，并可借用创建模块工作台壳，订单信息暂用默认值。

### 本轮处理

1. `OrderScanWorkspaceShell` 顶部步骤条从只展示文字的 `QLabel` 改为可点击 `QPushButton`，点击后由壳子内部调用 `SetStep()` 切换 `QStackedWidget` 当前页，并同步当前步骤高亮。
2. `OrderScanWorkspaceShell` 增加工作台模式：创建模式显示 Order / Scan / Process / Send；练习模式只显示 Scan / Process；右上角壳按钮只保留 `Minimize` / `Close`，动作通过回调交给 MainExe。
3. MainExe 首页 Practice 入口已接入练习工作台；创建工作台和练习工作台共用 `OrderScanWorkspaceShell`，练习模式使用默认订单上下文。
4. MainExe 阶段性动态加载 `MeyerScan_ScanWorkflowUI.dll` 与 `MeyerScan_DataProcessUI.dll`，挂入工作台 Scan / Process 步骤；切换离开时释放 QVTK/VTK/OpenGL 重资源，并用轻量占位页替换旧步骤。
5. 明确阶段性边界：MainExe 直接挂载 ScanWorkflowUI/DataProcessUI 只用于先跑通创建/练习流程；后续真实扫描仍以 `ScanReconstructStudio.exe` 独立进程作为最终边界。
6. OrderCreateUI 低分辨率适配收敛：根界面最小尺寸降为 960x600，布局边距、三栏宽度、类型按钮、牙位按钮、表格和备注框高度均做收敛；分辨率适配继续采用 Qt Layout、滚动区、伸缩策略和多语言换行，不恢复 1920x1080 等比硬缩放。
7. ScanWorkflowUI / DataProcessUI 根最小尺寸也降为 960x600，便于嵌入工作台壳。
8. 同步升级本轮涉及模块版本：MainExe v0.1.1、OrderScanWorkspaceShell v0.1.1、OrderCreateUI v0.2.2、ScanWorkflowUI v0.1.1、DataProcessUI v0.1.1，代码版本、CMake `project(VERSION)` 和 `Version.rc` 文件版本保持一致。
9. 更新任务总览、架构规范、开发进度跟踪和模块 README/CHANGELOG，记录练习模式、工作台右上角按钮要求、分辨率策略和重资源释放规则。

### 验证结果

- `F:\Tools\CMakePython\cmake\data\bin\cmake.exe --build F:\MeyerScan\build\cmake-vs2015-x64 --config Release --target MeyerScan -- /m` 构建通过；仅有外部既有登录模块头文件 C4819/C4091 警告。
- `OrderScanWorkspaceShellTest.exe` 返回 0，覆盖创建模式步骤点击和练习模式只显示 Scan/Process。
- `OrderCreateUITest.exe --smoke` 返回 0，覆盖建单上下文填充、牙位联动和动作回调。
- `ScanWorkflowUITest.exe`、`DataProcessUITest.exe` 返回 0。
- 单模块输出目录 `F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main` 和根输出目录 `F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0。
- 最新版本清单 `versionList_20260707_073811_244.json` 中 MainExe、OrderCreateUI、OrderScanWorkspaceShell、ScanWorkflowUI、DataProcessUI 的 `fileVersion` / `codeVersion` 均为本轮新版本且 `versionMatch=true`。

---

## 2026-07-06 - CMake 安装、版本清单 21 模块验证与旧口径清理

### 用户要求

用户要求版本记录时继续记录各模块代码版本、DLL/EXE 文件版本等信息；安装 CMake；梳理 `F:\MeyerScan` 下各模块文档和 `D:\wj\重构文档` 下 Markdown 文档，适当清理旧版本/旧方案痕迹。

### 本轮处理

1. 安装 CMake 3.31.6 到 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe`，并写入用户 PATH；当前 Codex 进程未自动刷新 PATH 时使用绝对路径验证。
2. 使用 `Visual Studio 14 2015 Win64` 生成器完成根聚合 CMake `Release` 配置和构建。
3. 修复扫描相关 CMake include 顺序：`MeyerScanScanThirdParty.cmake` 优先使用 `VTK_HEADERS_ROOT/include/vtk-8.0` 中可编译的 `QVTKWidget.h`，避免误读 VTK 安装目录中的生成/占位头。
4. 补齐 `CaseOrderServiceTest` 对 `MyDatabaseQtAdapter` 的 CMake 依赖，保证测试目标能在根聚合 CMake 中独立编译。
5. MainExe CMake PostBuild 补齐配置文件复制；MainExe VS2015/CMake 输出目录补齐扫描 UI 所需 VTK/OpenCV 运行库复制。第三方运行库只作为运行依赖复制，不进入运行时 `logs/versionList`。
6. 重新运行 `MeyerScan.exe --smoke-main`，最新运行时版本清单为 schemaVersion=2、21 个模块；`ScanReconstructStudio.exe`、`MeyerScan_ScanWorkflowUI.dll`、`MeyerScan_DataProcessUI.dll` 均记录 `fileVersion`、`codeVersion`，且 `versionMatch=true`；未混入 Qt/VTK/OpenCV 第三方库。
7. 清理误导性旧注释：CaseUI 不再描述为做数据库健康检查；Database 头文件不再暗示 Permission/UI 可直接执行 SQL，明确 Database 只供 Adapter、领域服务、迁移工具、统计/导出等数据访问边界使用。
8. 更新根 README、扫描三件套 README/CHANGELOG、MainExe CHANGELOG、任务总览、架构规范和进度跟踪文档，移除当前规范中的“CMake 未安装/待验证”旧口径。

### 验证结果

- `F:\Tools\CMakePython\cmake\data\bin\cmake.exe --version` 返回 CMake 3.31.6。
- `cmake -S F:\MeyerScan -B F:\MeyerScan\build\cmake-vs2015-x64 -G "Visual Studio 14 2015 Win64"` 配置成功。
- `cmake --build F:\MeyerScan\build\cmake-vs2015-x64 --config Release` 构建成功。
- `MeyerScan.exe --smoke-main`、`ScanWorkflowUITest.exe`、`DataProcessUITest.exe`、`ScanReconstructStudio.exe --smoke` 均返回 0。
- 最新验证文件 `versionList_20260706_154131_254.json`：schemaVersion=2、21 个模块、无 `codeVersionError`、无缺失模块、无版本不一致，第三方库计数为 0。

---

## 2026-07-05 - MainExe 动态加载自研 DLL 与版本清单双版本记录

### 用户要求

用户询问是否可以把部分静态加载（dll + lib）的模块改为动态加载 DLL（仅 DLL），并指出 DLL 文件详细信息中没有版本号，需要重新梳理版本管理方案。要求运行时生成模块版本清单时同时记录文件版本和代码版本。

### 本轮处理

1. MainExe 对 Logger、ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter 改为 `QLibrary + extern "C" GetXxx()` 运行时动态加载；VS2015 工程不再链接这些自研模块的 import lib。
2. 保留 Qt、Windows `Version.lib`、既有外部登录模块 `MeyerLoginWidget.lib` 的当前链接方式。动态加载不是“一刀切删除所有 .lib”，而是优先用于需要降低 MainExe 耦合、便于替换或缺失降级的自研模块。
3. `version_modules.json` 升级为 schemaVersion=2，模块项从字符串扩展为 `{ "file": "...", "versionFunction": "GetMeyerModuleVersion" }`；`versionFunction` 为空的模块只记录文件版本。旧 `factory` 字段仅保留兼容读取，不再作为新增模块推荐写法。
4. 各自研 DLL 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，该函数只返回 `ModuleInfo::Version`，不创建业务接口对象；MainExe 生成 `logs/versionList/versionList_*.json` 时同时记录 `fileVersion`、`codeVersion`、`versionMatch` 和 `codeVersionError`。`fileVersion` 来自 Windows `Version.rc` 版本资源，`codeVersion` 来自统一版本函数。
5. `MyDatabaseQtAdapter` 新增 `IDatabaseQtAdapter` 纯虚接口和自身 `GetModuleVersion()`，MainExe 可动态加载 Adapter 后按接口使用，同时保留既有 `GetDatabaseQtAdapter()` 返回具体类指针，避免影响当前测试宿主和服务模块源码。
6. 文档补充规则：DLL “详细信息”页没有版本号时，应检查 `src/Version.rc` 是否编入工程；`ModuleInfo::Version`、业务接口 `GetModuleVersion()` 和统一版本函数 `GetMeyerModuleVersion()` 只负责运行时代码版本，不会让 Windows 文件属性自动显示版本。
7. 历史 `MyVersionManager` 骨架同步新规则：读取同一个 `version_modules.json`，输出 schemaVersion=2，并记录 `fileVersion`、`codeVersion`、`versionMatch`。

### 验证结果

- `MyDatabaseQtAdapter\MeyerScan_DatabaseQtAdapter.sln` Release x64 构建通过，并清理了测试项目中指向不存在工程的 stale GUID 依赖，避免 VS2015 打开/构建时卡住。
- `MeyerScan_MainExe.sln` 和根 `MeyerScan_AllModules.sln` Release x64 构建通过，仍只有外部登录头文件既有 C4819/C4091 警告。
- 根输出目录 `MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke-external-order --external-order ... --external-order-type cmd_demo`、单模块输出目录 `MeyerScan.exe --smoke-main` 均正常返回。
- 当时根输出目录和单模块输出目录的 `versionList_yyyyMMdd_HHmmss_zzz.json` 均为 schemaVersion=2、18 个模块；所有声明模块存在，无 `codeVersionError`；有代码版本来源的自研模块 `versionMatch=true`。2026-07-06 加入扫描三件套后，当前运行时版本清单已扩展为 21 个模块。
- 发现并修复单模块输出目录曾因旧 `MeyerScan_RuntimeDataCenter.dll` 与新 `MeyerScan_DatabaseQtAdapter.dll` 不匹配导致 `codeVersion` 读取失败的问题：MainExe VS2015 PostBuild 现在会在单模块输出完成后，从根聚合输出目录存在的 `MeyerScan_*.dll` 强制覆盖一次；CMake 入口也通过 target 依赖和 POST_BUILD 复制自研 DLL，保证 VSCode/CMake 构建结果具备运行依赖。

---

## 2026-07-08 - MyOrderCreateUI 治疗方案选择与资源目录规则

> 本节记录当时的阶段性方案。其中“bridge 作为修复类型”和“资源复制为散文件”已分别在 2026-07-12、2026-07-10 被后续方案废止；当前规则以最新条目和四份核心文档为准。

### 用户要求

用户要求优化 `MyOrderCreateUI` 的治疗方案选择功能，尽量和当前软件视频逐帧对齐，并明确整个重构软件各模块 icon、图片、mask 等资源应如何放置和复制。

### 本轮处理

1. `MyOrderCreateUI` 升级到 v0.4.0，新增 `ToothTreatmentPlanWidget`，负责上下颌牙弓图片绘制、mask 像素命中、牙位叠加图绘制、桥连接点绘制、hover 光标和 tooltip。
2. 治疗方案选择不再用简化牙位按钮矩阵，改为 `maxilla.png` / `mandible.png` 显示，点击坐标反算到 600x400 原始 mask 后映射 FDI 牙位号。
3. 治疗类型按钮改为图标在上、文字在下，图标资源使用 `sacanPlan/button` 下普通态/高亮态图片；多语言通过 Qt 布局和 tooltip 处理，不按语言写坐标 if/else。
4. 支持桥连接点：相邻两颗牙均为 `bridge` 时显示空心连接点，点击后显示实心连接点；桥记录聚合覆盖 `16-17 + 17-18 -> 16-18` 和跨中线 `11-12 + 11-21 -> 11-22`。
5. “Clear All” 按钮迁移到上下颌之间，人工模式弹确认框，smoke 模式通过 `QApplication` 动态属性跳过确认框，避免自动化测试阻塞。
6. 资源目录规则落地：模块私有资源先放各模块源码目录 `Resources/`，构建时复制到运行总目录 `Resources/Modules/<ProjectName>/...`；多个模块共用资源后续放 `Resources/Common` 或由 UIComponents 管理；运行时禁止使用 `QDir::currentPath()`。
7. `MyOrderCreateUI` CMake 和 VS2015 PostBuild 均会复制治疗方案资源到 `Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan`；MainExe 构建也补齐该资源复制。
8. 继续按视频关键帧校准布局：治疗类型面板从中间牙弓左侧迁移到整体左栏上方，左栏改为“治疗类型卡片 + 基本信息”，中间区域只保留上下颌牙弓、清空按钮和扫描流程输入，右侧保留明细/标信息/操作按钮。
9. 清理旧牙位按钮矩阵残留代码，牙位选择只保留 `ToothTreatmentPlanWidget + mask 命中 + OrderCreateUIImpl 状态刷新` 一条链路。
10. 外部 `scanPlan.bridgeConnectors` 增加防御性校验：格式必须正确，且两端牙位都必须为 `bridge` 类型，才允许进入桥记录和扫描流程 JSON；`18-17` 这类反向 key 会归一化为 `17-18`，避免第三方方向差异造成无效桥。
11. `OrderCreateUITest.exe` 新增 `--capture-screenshot <png>`，固定 1920x1080 保存模块截图，用于和 `D:\wj\OrderTreatmentPlan\治疗方案选择.mp4` 提取帧逐帧人工核对；当前截图路径为 `C:\Users\02241wj\AppData\Local\Temp\OrderCreateUITest_treatment_plan_latest.png`。

### 验证结果

- `cmake --build F:\MeyerScan\build --config Release --target MeyerScan_OrderCreateUI OrderCreateUITest` 构建通过。
- `F:\MeyerScan\MyOrderCreateUI\bin\Release\OrderCreateUITest.exe --smoke` 返回 0。
- `F:\MeyerScan\MyOrderCreateUI\bin\Release\OrderCreateUITest.exe --capture-screenshot C:\Users\02241wj\AppData\Local\Temp\OrderCreateUITest_treatment_plan_latest.png` 返回 0。
- 已截图核对治疗方案区域：左侧治疗类型卡片、中间牙弓、右侧明细卡结构已接近视频帧；清空按钮位于上下颌之间，治疗类型为图标按钮，牙弓区域按等比缩放显示。

---

## 2026-07-05 - MyOrderCreateUI 样式优化与共享 UI 接入

### 用户要求

用户指出 `MyOrderCreateUI` 初版样式太丑，要求优化界面，并让某些通用控件借助共享 UI DLL 实现。

### 本轮处理

1. `MyUIComponents` 升级到 v0.4.0，新增 `CreateFieldLabel()`、`CreateDateEdit()`、`CreateTextEdit()`、`CreateTableWidget()` 和 `ApplyTableStyle()`，并让输入框、下拉框、日期框、多行文本框复用统一输入控件 QSS，让基础表格复用统一表头、边框、隔行色和默认只读/整行选择规则。
2. 新增接口追加在 `IUIComponents` 末尾，不插入旧虚函数中间，避免破坏旧模块已编译代码的 vtable 顺序。
3. `MyOrderCreateUI` 升级到 v0.2.1，建单页面调整为更清爽的三栏工作台风格：顶部标题说明区、左侧基础信息、中间牙位/扫描方案、右侧摘要/明细/操作。
4. 建单页面中的通用按钮、字段标签、输入框、下拉框、日期框、多行备注框和已选牙位表格基础样式改为优先通过 `MeyerScan_UIComponents.dll` 创建。
5. 牙位按钮、扫描类型按钮、牙位状态联动仍留在 `MyOrderCreateUI`，不进入共享 UI DLL，避免 UIComponents 膨胀成业务控件库。
6. `MyOrderCreateUI` 不再链接 `MeyerScan_UIComponents.lib`，只保留头文件依赖、VS2015/CMake 构建顺序和 DLL 复制；运行时通过 `QLibrary` 动态加载 UIComponents，缺失或版本低于 v0.4.0 时走本地降级样式，避免旧 DLL 不包含新增虚接口导致 vtable 风险。
7. 同步更新 `MyUIComponents`、`MyOrderCreateUI` 的 README/CHANGELOG，以及任务总览、架构规范、开发进度跟踪中的共享 UI 规则。

### 验证结果

`MeyerScan_UIComponents.sln`、`MeyerScan_OrderCreateUI.sln` 和根 `MeyerScan_AllModules.sln` Release x64 构建通过；模块输出和根输出目录的 `UIComponentsTest.exe`、`OrderCreateUITest.exe --smoke` 返回 0；根输出目录 `MeyerScan.exe --smoke-main` 和 `MeyerScan.exe --smoke-external-order ... --external-order-type cmd_demo` 返回 0；`git diff --check -- MyUIComponents MyOrderCreateUI README.md` 仅有 LF/CRLF 提示。根方案仍有外部登录头文件既有 C4819/C4091 警告，非本轮修改引入。

---

## 2026-07-03 - 活跃模块测试宿主补齐与根方案回归

### 用户要求

用户要求检查各模块，需要测试项目的要补充开发。

### 本轮处理

1. 检查 `F:\MeyerScan` 当前活跃自研模块，确认模块必须具备 `test/main.cpp`、VS2015 测试 `.vcxproj`、模块 `.sln` 测试入口和同模块 `CMakeLists.txt` 测试目标。
2. 补齐 `MyDatabaseQtAdapter`、`MyCaseOrderService`、`MyConfigCenter`、`MyPermission`、`MyUIComponents`、`MyVersionManager`、`MyCalibration3DUI`、`MyCalibrationColorUI`、`MyOrderScanWorkspaceShell` 的最小测试宿主。
3. 将 Logger、Database、HomeUI、CaseUI、SettingsUI、RuntimeDataCenter 等既有测试项目同步纳入根 `MeyerScan_AllModules.sln`。
4. 修复 `CaseUITest.exe --smoke` 和 `SettingsUITest.exe --smoke` 在根输出目录 `F:\MeyerScan\bin\Release` 运行时的路径推导问题：测试宿主现在从 EXE 所在目录向上查找 `MeyerScan_AllModules.sln` 作为仓库根，兼容单模块输出和根聚合输出。
5. CaseUITest/SettingsUITest 不再复用公共 `config/db_config.json`，而是在输出目录下生成各自的 `config/CaseUITest/db_config.json`、`config/SettingsUITest/db_config.json` 和独立 SQLite 测试库，避免多个测试共享旧表结构后互相污染。
6. 文档同步明确：DLL 模块优先提供独立 `*Test.exe`；主 EXE 用 `MeyerScan.exe --smoke-main` 作为自动测试入口；外部既有登录 DLL 的交互测试宿主独立维护，不强制进入根方案自动回归。

### 验证结果

`MeyerScan_AllModules.sln` Release x64 构建通过。已运行并通过：`LoggerTest.exe`、`DatabaseTest.exe`、`ConfigCenterTest.exe`、`PermissionTest.exe`、`VersionManagerTest.exe`、`DatabaseQtAdapterTest.exe`、`CaseOrderServiceTest.exe`、`RuntimeDataCenterTest.exe`、`UIComponentsTest.exe`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`Calibration3DUITest.exe`、`CalibrationColorUITest.exe`、`OrderScanWorkspaceShellTest.exe`、`MeyerScan.exe --smoke-main`。当时 PATH 未找到 `cmake.exe`，只完成 CMake 声明完整性检查；该构建验证已在 2026-07-06 使用 CMake 3.31.6 和 VS2015 x64 生成器补齐。

## 2026-07-03 - 提交前全模块复核与仓库同步

### 用户要求

用户要求把所有模块代码和相关文件再过一遍，确认前期修改意见已经落实，并提交 GitHub 与 `F:\MeyerScan-Reposit` 本地仓库。

### 本轮复核结论

1. 代码仓库与本地备份仓库开始复核时均为干净状态，分别停在“数据库去 Qt 与 SQLite x64 运行时收口”的最新提交。
2. 重新检查旧 SQLiteStudio 路径、Database Qt/QtSql 依赖、第三方 SQLite DLL 跟踪状态、`QDir::currentPath()`、UI 可见文案 `tr("English source text")`、CMake/VS2015 工程入口、权限 `visible/enabled` 生效链路和 UI/Service 是否绕过 DatabaseQtAdapter。
3. 未发现 Database 重新引入 Qt、正式 HomeUI/CaseUI/SettingsUI 直连 Database、第三方 `sqlite3.dll` 被 Git 跟踪、或运行路径依赖当前工作目录等实质偏移。
4. 发现并清理 `CaseOrderServiceImpl.cpp` 中残留的 `QSqlQuery` 文字说明，改为“后续应在 Database/DAO 层提供参数绑定能力”，避免后续维护者按旧 QtSql 思路回退。

### 验证结果

`MeyerScan_AllModules.sln` Release x64 构建通过；`DatabaseTest.exe` 24 passed / 0 failed；`LoggerTest.exe`、`RuntimeDataCenterTest.exe`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0。登录测试宿主未作为自动冒烟执行，因为它会打开外部既有登录界面，适合作为人工集成验证。

## 2026-07-03 - Database 去 Qt 与 DatabaseQtAdapter 主链路收口

### 本轮继续处理结果

继续任务时复测发现 `DatabaseTest.exe` 虽返回 0，但 SQLite 实际没有连接成功；根因是工程从旧 `MyCaseManager\SQLite\sqlitestudio311\SQLiteStudio` 目录复制的 `sqlite3.dll` 为 32 位，而当前工程按 x64 编译。已将 SQLite 运行时统一为 `F:\MeyerScan\ThirdParty\SQLite\win-x64\sqlite3.dll`，VS2015 PostBuild 和 CMake 公共规则均从该目录复制；第三方 DLL 本体不提交仓库，只提交目录 README 说明。`DatabaseTest` 已改为 SQLite 连接失败即失败，并输出具体底层错误，不再用“MySQL 可能未运行”跳过。

已按单模块和根聚合两类运行目录重新验证：`MyDatabase`、`MyDatabaseQtAdapter`、`MyRuntimeDataCenter`、`MyCaseUI`、`MySettingsUI`、`MyMainExe` 单模块方案和 `MeyerScan_AllModules.sln` Release x64 均可构建；关键输出目录内 `sqlite3.dll` 均为 x64；`DatabaseTest.exe` 24 passed / 0 failed，`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`MyMainExe\bin\Release\MeyerScan.exe --smoke-main`、`F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0。外部登录 SDK 头文件仍有 VS2015 编码/typedef 警告，属于外部既有模块，不影响本轮自研链路。

### 用户要求

用户要求按 `MyCaseUI / MySettingsUI -> RuntimeDataCenter / CaseOrderService -> MyDatabaseQtAdapter -> MyDatabase` 链路修改代码和文档，Database 连接/查询不再使用 Qt；Qt UI/Service 模块如需 `QString`、`QJsonDocument`、SQL 文本转换和查询结果解析，统一通过 `MyDatabaseQtAdapter` 作为中介，便于后续去 Qt 化和降低模块耦合。

### 当前设计结论

1. `MyDatabase` 是纯 C++ 基础设施模块，不再链接 QtCore/QtSql，也不包含 `QString`、`QSqlDatabase`、`QSqlQuery`、`QMutex` 等 Qt 类型。
2. SQLite 由 `MyDatabase` 运行时动态加载 `sqlite3.dll` 并调用 SQLite C API；MySQL 原生链路保留配置和枚举，待 MySQL C API SDK 进入工程后接入。
3. `MyDatabaseQtAdapter` 是 Qt 调用方进入 Database 的唯一类型转换层，依赖方向固定为 `Qt UI/Service -> DatabaseQtAdapter -> Database`，Database 不反向依赖 Adapter。
4. `MyMainExe` 的数据库健康检查、`MyRuntimeDataCenter` 的只读快照加载、`MyCaseOrderService` 的服务查询，以及 CaseUI/SettingsUI 测试宿主造演示数据，均应经 `DatabaseQtAdapter` 访问 Database。
5. 正式 `MyHomeUI` 不访问 Database；正式 `MyCaseUI`、`MySettingsUI` 读取 RuntimeDataCenter 快照，不直连 Database，不建表、不迁移、不写业务数据。
6. 历史记录中关于 QtSql、UI 框架期直连 Database、HomeUI/CaseUI 借用 Database 健康检查的内容仅用于追溯；当前开发必须以上述新链路为准。

### 代码与工程调整

| 模块 | 调整 |
|------|------|
| MyDatabase | 改为纯 C++ / sqlite3 C API；VS2015 与 CMake 工程移除 Qt 依赖并使用 `/MT`；版本更新到 v1.3.0。 |
| MyDatabaseQtAdapter | 新增模块，输出 `MeyerScan_DatabaseQtAdapter.dll`，提供 `EnsureConnected()`、`ExecuteUpdate()`、`ExecuteQueryJson()`、`ExecuteQueryJsonDocument()`、`RowsFromTableJson()`、`EscapeSqlText()` 等 Qt 便利接口。 |
| MyMainExe | 启动期数据库健康检查改为通过 DatabaseQtAdapter 完成，MainExe 不再包含 `Database.h`；运行时版本清单加入 Adapter。 |
| MyRuntimeDataCenter | 通过 DatabaseQtAdapter 查询本地表并包装 domain JSON 快照，不直接访问 Database。 |
| MyCaseOrderService | 通过 DatabaseQtAdapter 访问底层数据库，继续保持服务层边界。 |
| MyCaseUI / MySettingsUI | 正式 UI 不直连 Database；测试宿主造最小演示数据时才经 Adapter 写入 SQLite。 |

### 文档同步

已同步 `MeyerScan重构任务总览.md`、`MeyerScan架构设计与接口规范.md`、`MeyerScan重构开发进度跟踪.md` 和各模块 README/CHANGELOG。后续若继续扩展数据库查询能力，应优先判断是“类型转换”还是“业务语义”：前者放 Adapter，后者放 RuntimeDataCenter / CaseOrderService / 对应领域服务。

## 2026-07-01 - 重构规则与代码偏移复核

### 复核范围

本轮按模块清单、路径规则、多语言规则、数据库边界、权限 `visible/enabled`、页面资源释放、版本清单、RuntimeDataCenter 读写边界和文档同步要求，复核 `D:\wj\重构文档` 下核心 md 文档与 `F:\MeyerScan` 下已开发模块代码。

### 发现与处理

1. 未发现正式 UI 源码使用 `QDir::currentPath()` 推导运行资源路径；只剩注释中的禁止说明。
2. 未发现正式 HomeUI / CaseUI / SettingsUI 源码执行建表、插入、删除等业务写库逻辑；造 SQLite 演示数据仅存在测试宿主。
3. 未发现中文 UI source text 直接写入 `tr()` 或 `setText()`；当前可见文案继续遵守 `tr("English source text")`。
4. MainExe 当前仍是单内容区全屏替换页面，未回退到首页/浏览并列 `QStackedWidget`；从案例管理进入扫描前会释放 CaseUI。
5. 权限 `visible/enabled` 已由 MainExe 合并 ConfigCenter / Permission 后下发到 HomeUI / CaseUI，并在动作执行入口二次复核。
6. `CaseUI` 和 `SettingsUI` 原先在 UI 模块中主动刷新 RuntimeDataCenter 全域缓存，职责偏重；已调整为 MainExe 启动期执行 `ReloadAll()`，UI 模块只初始化 RuntimeDataCenter，并在读取自己需要的 domain 时按需懒加载。
7. `LoggerTest.exe` 原先写死测试日志目录 `F:\MeyerScan\MyLogger\test\logs`，与“测试宿主也不得依赖开发机绝对路径”规则偏移；已改为从测试 exe 所在目录推导同级 `logs`。

### 代码与文档同步

| 模块 / 文档 | 调整 |
|-------------|------|
| MyCaseUI | 调整 RuntimeDataCenter 初始化职责；README / CHANGELOG 记录 MainExe 全域刷新 + UI 懒加载 domain 的边界。 |
| MySettingsUI | 调整 RuntimeDataCenter 初始化职责；README / CHANGELOG 记录 Information 页面只读快照边界。 |
| MyLogger | `LoggerTest.exe` 测试日志目录改为基于 exe 所在目录推导；README / CHANGELOG 记录测试宿主路径规则。 |
| 全局进度文档 | 记录本轮偏移复核结论和已修正事项，避免后续开发继续沿用旧调用方式。 |

### 验证结果

| 项目 | 结果 |
|------|------|
| `F:\MeyerScan\MeyerScan_AllModules.sln` Release x64 | 通过，0 warning / 0 error |
| `F:\MeyerScan\MyLogger\LoggerTest.vcxproj` Release x64 | 通过，0 warning / 0 error |
| `LoggerTest.exe` | 返回 0，日志写入 `F:\MeyerScan\MyLogger\bin\Release\logs` |
| `RuntimeDataCenterTest.exe` | 返回 0 |
| `CaseUITest.exe --smoke` | 返回 0 |
| `SettingsUITest.exe --smoke` | 返回 0 |
| `F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main` | 返回 0 |
| `F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` | 返回 0 |

### 当前结论

当前方案总体仍符合“模块小、职责清晰、低耦合、便于人工维护”的目标。需要继续关注的不是是否继续拆模块，而是防止 UI 模块、RuntimeDataCenter、Database 和 Service 之间的职责互相侵入：UI 做展示与动作上报，RuntimeDataCenter 做只读快照，CaseOrderService 做业务读写，Database 只做基础 SQL/连接。

## 2026-06-30 - SQLite 默认链路与 RuntimeDataCenter 运行时快照模块落地

### 用户要求

用户要求在 `F:\MeyerScan` 下切换到 SQLite 数据库，并开始开发“本地数据库信息读取后在内存中的存储”和“云端相关信息在内存中的存储”能力。涉及本地诊所、技工所、软件信息、医生、设置、账号、订单、患者、设备信息，以及云端诊所登录信息、诊所基本信息、云端合作技工所、云端设备信息等。用户强调这些字段后续会频繁扩展，需要考虑用一个或两个 DLL，或其它方式封装，便于其它模块读取/传递，同时尝试接入 MyCaseManager/CaseUI 等模块。

### 设计结论

1. 默认运行链路切换为 SQLite，但保留 MySQL 能力；数据库类型由 `db_config.json` 和 ConfigCenter 的 `database.type` 同步表达。
2. 新增 `MyRuntimeDataCenter` 模块，输出 `MeyerScan_RuntimeDataCenter.dll`。它是运行时读模型/缓存中心，不是业务 CRUD 服务。
3. 高频变化字段不暴露为固定 C++ struct 给 UI 和 MainExe。固定 struct 看似清晰，但字段增删会牵动 ABI、头文件、UI、主程序和测试，后期维护成本会升高。
4. RuntimeDataCenter 对外以稳定 domain 返回 UTF-8 JSON 快照，例如 `local.patients`、`local.orders`、`local.clinics`、`cloud.clinicProfile`。调用方只读自己需要的字段，字段新增时不需要改接口。
5. `CaseOrderService.dll` 继续承担患者/订单保存、删除、编辑、状态变化、主数据维护和权限复核。RuntimeDataCenter 只负责运行时快照和上下文读取，不替代 CaseOrderService。
6. UI/主程序不传 SQL、不传旧表名。旧表名只在 RuntimeDataCenter 内部白名单维护，避免它退化成任意查询通道。
7. RuntimeDataCenter 是有容量上限的只读快照，不是无限大表缓存；超过上限的数据要转 CaseOrderService 分页查询、DataExport 或专用服务。
8. SQLite 默认链路不等于完成旧库迁移；旧 `mysql.sql` 不由 Database/RuntimeDataCenter 自动执行，后续 migration/adapter 另行设计。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyDatabase | `config/db_config.json` 默认改为 `sqlite`；SQLite 连接前自动创建数据库文件父目录；DatabaseTest 运行配置改为 SQLite 优先，并按数据库类型选择列出表 SQL。 |
| MyConfigCenter | `config/runtime_config.json` 中 `database.type` 改为 `sqlite`。 |
| MyRuntimeDataCenter | 新增 VS2015 DLL 工程、测试宿主、README、CHANGELOG；提供 `IRuntimeDataCenter` 接口；支持 `ReloadAll()`、`ReloadDomain()`、`GetDomainJson()`、`UpdateCloudClinicJson()`。 |
| MyMainExe | 链接并复制 `MeyerScan_RuntimeDataCenter.dll`；启动数据库连接后初始化 RuntimeDataCenter 并 `ReloadAll()`；`version_modules.json` 加入该 DLL。 |
| MyCaseUI | 动态加载 `MeyerScan_RuntimeDataCenter.dll`；患者页读取 `local.patients`，订单页读取 `local.orders`，只填充界面表格，不直接拼业务 SQL；读取缓冲采用有限扩容重试，避免字段扩展后误显示空列表。 |
| 工程文件 | 已开发自研 Qt 模块 Release PostBuild 的 Qt DLL/插件复制来源统一为编译所用 `C:\Qt\Qt5.6.3\5.6.3\msvc2015_64`，避免输出目录混用 Qt 5.6.2 / 5.6.3 导致 `DatabaseTest.exe` 或 `MeyerScan.exe --smoke-main` 异常退出；外部登录相关既有 DLL 仍从已安装软件目录复制。 |
| 聚合方案 | `MeyerScan_AllModules.sln` 加入 `MyRuntimeDataCenter`。 |

### 文档同步

- `MeyerScan重构任务总览.md`：拆分模块总清单新增“运行时数据中心”；患者/订单与主数据读写口径改为“RuntimeDataCenter 读快照 + CaseOrderService 做 CRUD”。
- `MeyerScan架构设计与接口规范.md`：2.1 分层架构加入 RuntimeDataCenter；2.2 职责表补充 RuntimeDataCenter；新增“运行时数据与业务服务边界”说明，明确 JSON 快照、domain、schemaVersion 和 CaseOrderService 边界。
- `MeyerScan重构开发进度跟踪.md`：新增 `1.4.1 开发运行时数据中心`，记录构建、接口和验证结果。
- `MyRuntimeDataCenter`、`MyMainExe`、`MyCaseUI`、`MyDatabase`、`MyConfigCenter`、`MyCaseManager` 的 README/CHANGELOG 同步记录相关边界。

### 验证结果

| 项目 | 结果 |
|------|------|
| `F:\MeyerScan\MeyerScan_AllModules.sln` Release x64 | 通过，0 warning / 0 error |
| `RuntimeDataCenterTest.exe` | 返回 0，覆盖 SQLite、本地快照、云端诊所 JSON 注入 |
| `DatabaseTest.exe` | 返回 0，21 passed / 0 failed |
| `CaseUITest.exe --smoke` | 返回 0 |
| `F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main` | 返回 0 |
| `F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` | 修复 Qt 运行库复制来源后返回 0，并生成 `logs/versionList` |

### 当前结论

RuntimeDataCenter 的引入是为了降低字段变化对 UI/主程序 ABI 的冲击，不是为了新增一个“万能数据模块”。后续开发应继续保持三层边界：Database 做基础 SQL/连接，RuntimeDataCenter 做只读快照，CaseOrderService 做患者/订单和主数据的业务读写。

### 2026-06-30 复测修正

复测 `DatabaseTest.exe` 时发现输出目录中 `Qt5Core.dll` 为 Qt 5.6.3、`Qt5Sql.dll` 为 Qt 5.6.2，导致测试宿主在调用 `GetDatabase()` 期间异常退出。最终修正为：所有自研 Qt 模块的 Qt DLL、platforms 和 sqldrivers 插件都从编译所用 Qt 5.6.3 目录复制，并去掉 Qt 运行库复制命令中的 `/D`，避免旧 DLL 因时间戳未被覆盖。`DatabaseTest.exe` 入口同时创建 `QCoreApplication`，符合 Database 依赖 Qt Core/Sql 的实际运行条件。修正后 `DatabaseTest.exe` 默认 SQLite 链路返回 0，当前断言数为 23 passed / 0 failed。

### 2026-06-30 链路测试补充

用户要求用当前需求测试链路：`MyCaseUI` 显示数据库中的患者/订单信息；设置模块对应页面显示本地诊所、技工所和医生信息；若数据库无数据，则造几条数据放入数据库。

本轮处理如下：

| 模块 | 处理 |
|------|------|
| MySettingsUI | Information 页面接入 `MeyerScan_RuntimeDataCenter.dll`，医生读取 `local.doctors`，诊所读取 `local.clinics`，技工所读取 `local.labs`，不再使用硬编码占位行。 |
| MySettingsUI | 修复 JSON 缓冲区解析问题：RuntimeDataCenter 写入的是以 `\0` 结尾的 UTF-8 JSON，调用方必须按真实字符串解析，不能把整块预分配缓冲区直接交给 `QJsonDocument`。 |
| MySettingsUI 工程 | 补齐 RuntimeDataCenter、Database、QtSql、qsqlite/qsqlmysql 和 `db_config.json` 的 VS2015 工程依赖与 Release 复制。 |
| SettingsUITest | 启动时按发布目录 `config/db_config.json` 或仓库 `MyDatabase/config/db_config.json` 连接 SQLite；空库时创建最小旧表并写入医生、诊所、技工所、患者、订单各一条演示数据；`--smoke` 检查 Information 三张表均有数据行。 |
| CaseUITest | 启动时准备同样的最小演示数据；`--smoke` 从“窗口能创建”升级为检查患者表和订单表均有数据行。 |

验证结果：

| 项目 | 结果 |
|------|------|
| `F:\MeyerScan\MySettingsUI\MeyerScan_SettingsUI.sln` Release x64 | 通过，0 warning / 0 error |
| `F:\MeyerScan\MyCaseUI\MeyerScan_CaseUI.sln` Release x64 | 通过，0 warning / 0 error |
| `F:\MeyerScan\MySettingsUI\bin\Release\SettingsUITest.exe --smoke` | 返回 0 |
| `F:\MeyerScan\MyCaseUI\bin\Release\CaseUITest.exe --smoke` | 返回 0 |
| SQLite 数据 | `patient_tbl2`、`order_tbl2`、`clinic_tbl`、`lab_tbl2`、`dentist_tbl` 均已有演示数据行 |

结论：本次链路测试只允许测试宿主准备演示 schema 和演示数据；正式 UI DLL 仍不负责建表、迁移或写业务库。正式新增、编辑、删除、保存和主数据维护继续归 `CaseOrderService` 或后续专门设置服务。

---

## 2026-06-26 - UIComponents 标准控件样式管理落地

### 用户要求

用户要求读取 `D:\wj\重构文档\glm52` 下的代码修改记录和建议文档，并思考“界面控件的样式如何统一管理”：例如 `QPushButton` 存在纯文字、纯图标、文字加图标、上下图文、高亮按钮、普通按钮等多种样式，多个 UI 模块都能用上的控件是否应由 `MyUIComponents` 统一管理，特殊控件是否留在自身模块。

### 设计结论

1. `MyUIComponents` 是正确归属，但边界必须克制：它只管理通用控件、通用样式、尺寸、DPI/Layout 辅助和多语言友好策略。
2. `MyUIComponents` 不读取权限、不访问数据库、不决定页面切换、不绑定按钮 clicked 行为、不保存业务状态。
3. 业务模块负责页面结构、按钮文案、动作 ID、权限显隐/启用态、日志和业务回调。
4. 多个模块复用的按钮、输入框、下拉框、表格样式、公共弹窗和等待页进入 UIComponents；只在单个模块出现的截图还原控件、复杂设置页控件或单页面专用组合控件留在自身模块。
5. 是否公共化的判断标准是“至少两个模块复用，且不包含业务语义”，不是“看起来像控件就抽公共模块”。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyUIComponents | 版本升为 v0.2.0；新增 `MeyerButtonRole`：Primary、Secondary、Text、Danger、Entry；新增 `MeyerButtonContentLayout`：TextOnly、IconOnly、IconLeftText、IconTopText。 |
| MyUIComponents | 新增 `CreateButton()`、`CreateToolButton()`、`ApplyButtonStyle()`、`ApplyToolButtonStyle()`；保留 `CreatePrimaryButton()` / `CreateSecondaryButton()` 并转发到新工厂。 |
| MyHomeUI | 首页四个入口按钮优先使用 UIComponents 的 Entry 标准样式；UIComponents 不可用时降级为本地按钮样式。 |
| MyCaseUI | 顶部返回/设置按钮使用 Secondary；患者/订单工具栏按钮按操作类型使用 Secondary / Primary / Danger。 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：补充 UIComponents 控件工厂边界、按钮角色/内容布局、特殊控件归属规则。
- `MeyerScan重构任务总览.md`：补充共享 UI 组件模块规则、第二阶段任务状态和特殊控件公共化判断标准。
- `MeyerScan重构开发进度跟踪.md`：将 UIComponents 状态更新为 v0.2.0 骨架增强，记录 HomeUI/CaseUI 已接入标准按钮样式。
- `glm52/glm52代码修改记录.md`：记录本次读取 glm52 后对 UIComponents 的采纳和落地。
- `MyUIComponents`、`MyHomeUI`、`MyCaseUI` 的 README / CHANGELOG 同步记录本次变更。

### 当前结论

共享 UI 样式需要统一，但不能把 UIComponents 做成“页面库”或“业务组件库”。当前先从按钮角色和内容布局落地，后续再逐步扩展表格、公共弹窗、主题、ScreenUtil/DpiUtil/LayoutRules 和 Common qm。

---

## 2026-06-26 - 文档与代码口径复查

### 用户要求

用户要求重新梳理 `D:\wj\重构文档` 下 md 文档和 `F:\MeyerScan` 各模块 md 文档，确保模块拆分、功能分割、开发要求符合当前要求，并根据修改内容检查已开发模块代码。

### 复查处理

| 项目 | 处理 |
|------|------|
| 根 README | 重写为当前模块清单和关键开发规则摘要，移除 HomeUI/CaseUI v0.1.0、Logger 后台线程等旧口径。 |
| HomeUI / CaseUI | 因已接入 UIComponents 标准按钮样式，`ModuleInfo::Version` 和 `Version.rc` 同步升级为 v0.2.0。 |
| SettingsUI | General 页面订单存储路径和打包路径不再显示 `D:/SOFTSCANDATA` / `D:/SCANDATA`，骨架期改为基于 `QStandardPaths::DocumentsLocation` 派生的用户目录占位路径。 |
| SettingsUI 文档 | CHANGELOG 和 README 记录占位路径规则：正式阶段仍由 ConfigCenter 读取和持久化设置。 |
| 全局进度文档 | 补充本次复查记录和设置占位路径约束。 |

### 结论

历史记录中带有“历史口径/已修正”的旧方案继续保留，用于追溯；当前规范和当前代码不能错的内容已修正。后续仍要优先推进 Core.lib、CaseOrderService/ScanSchemaService/OrderWorkflowService，让 HomeUI/CaseUI 的框架期 Database 健康检查逐步退出正式业务链路。

---

## 2026-06-15 — Logger.dll 双构建环境搭建 + GitHub 仓库推送

### 用户要求

1. 把 MyLogger 项目变成 VS2015 和 VSCode 都可直接编译的项目
2. VS2015 已安装但缺少 C++ 编译工具，需安装
3. VSCode 需能直接编译 Debug 和 Release 版本
4. 代码推送到 GitHub（`https://github.com/weijian118/MeyerScan.git`）
5. GitHub 仓库按模块文件夹分类（`MeyerScan/MyLogger/`），后续其他模块放同级目录

### AI 实施内容

#### 1. VS2015 C++ 工具安装

- **问题**：本机 VS2015 Community 只装了 IDE 外壳，没有 C++ 编译器（`cl.exe`、`link.exe` 均缺失）
- **解决**：运行 `C:\ProgramData\Package Cache\{...}\vs_community.exe /Modify`，安装 Visual C++ 公共工具 + Windows 8.1 SDK

#### 2. VS2015 项目文件创建

| 文件 | 说明 |
|------|------|
| `MeyerScan_Logger.sln` | VS2015 解决方案，含 DLL + EXE 两个项目 |
| `MeyerScan_Logger.vcxproj` | DLL 项目，v140 工具集，/MT 运行时，输出 `bin/Debug/MeyerScan_Logger.dll` |
| `MeyerScan_Logger.vcxproj.filters` | Solution Explorer 虚拟文件夹 |
| `LoggerTest.vcxproj` | 测试 EXE 项目，控制台应用，引用 DLL 项目 |
| `LoggerTest.vcxproj.filters` | 测试项目筛选器 |

#### 3. VSCode tasks.json 配置

- `Ctrl+Shift+B` 弹出 Debug/Release 选择
- 使用 MSBuild 直接编译 `.sln`，**无需 CMake 或 Ninja**
- MSBuild 路径：`C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe`
- Debug 和 Release 两个独立任务

#### 4. 编码与代码问题修复

| 问题 | 修复方式 |
|------|----------|
| **UTF-8 源文件被 MSVC 按 GBK(936) 误解析** | 全部 13 个源文件转为 UTF-8 with BOM（VS2015 RTM 不支持 `/utf-8` 编译选项） |
| **Logger.h 缺少 `#include <string>`** | 补入（内联的 `std::string` 重载方法依赖此头文件） |
| **Init(std::string) 重载漏掉 level 参数** | 原代码 `Init(logDir.c_str())` → 修正为 `Init(logDir.c_str(), level)` |
| **LoggerImpl.cpp 临时 std::string 无法隐式转 const char\*** | 用 `.append().c_str()` 替代 `+` 运算符 |

#### 5. Git 与 GitHub

- **Git 安装**：通过 winget 安装 `Git.Git` 2.54.0
- **GitHub CLI 安装**：通过 winget 安装 `GitHub.cli` 2.94.0
- **认证**：Personal Access Token (classic)，scopes: `repo`
- **仓库结构**：
  ```
  MeyerScan/                      ← Git 根目录（也是 GitHub 仓库根）
  ├── .gitignore                  ← 全局忽略规则
  ├── MyLogger/                   ← 日志模块（所有源文件在此子目录下）
  │   ├── include/Logger.h
  │   ├── src/                   5 个 .cpp + 5 个 .h
  │   ├── test/main.cpp
  │   ├── MeyerScan_Logger.sln
  │   ├── *.vcxproj*
  │   ├── CMakeLists.txt
  │   └── .vscode/
  └── MyNetworkHelper/            ← 后续模块（空目录，占位）
  ```
- **首版提交**：`master` 分支，24 个文件，2512 行

#### 6. 构建验证

| 构建方式 | 结果 |
|----------|------|
| VS2015 编译 Debug | ✅ `MeyerScan_Logger.dll` + `LoggerTest.exe` |
| VS2015 编译 Release | ✅ |
| VSCode `Ctrl+Shift+B` Debug | ✅ |
| LoggerTest 运行（8 项测试） | ✅ 全部通过（含 5000 条多线程压力测试） |

### 采纳决策

以上所有实施方案已采纳并验证通过。后续模块参照 MyLogger 的项目结构模板创建。

### 影响范围

- `f:\MeyerScan\MyLogger\`：新增 `.sln`、`.vcxproj`、`.vcxproj.filters` 文件，修改 Logger.h/LoggerImpl.cpp 源码 bug
- `f:\MeyerScan\.gitignore`：新建根级 gitignore
- `D:\wj\重构文档\`：更新 3 个 md 文档（方案优化建议 / AI协作记录 / 进度跟踪）
- 文档版本：方案优化建议 v2.1，进度跟踪更新 Logger 状态，AI 协作记录新增本条

---

## 2026-06-12（第三次） — Logger.dll 完整实现 + 中文注释翻译

### 用户要求

1. 按照设计方案完整实现 MeyerScan_Logger.dll 模块
2. 所有代码注释使用中文，不使用英文

### AI 实现内容

#### Logger.dll 核心实现（历史记录，2026-06-24 已按现场反馈调整）

| 文件 | 说明 |
|------|------|
| `include/Logger.h` | 唯一公开头文件，定义 ILogger 接口、LogLevel 枚举、MEYER_LOG_* 宏 |
| `src/LogFormat.h/.cpp` | 日志格式化：时间戳 + 级别 + 模块 + 操作 + 设备ID + 病例ID + 操作员 + 内容 |
| `src/LogBuffer.h/.cpp` | 历史实现曾用于后台缓冲；2026-06-24 已删除，不再参与当前日志模块 |
| `src/LogRotation.h/.cpp` | 文件轮转：每天主文件 `MeyerScan_YYYYMMDD.log`，超过 10 MiB 后生成 `_001/_002` 分卷 |
| `src/LogWriter.h/.cpp` | 文件 I/O + 跨进程 Named Mutex（`Global\MeyerScan_Logger_Mutex`），每条日志打开/写入/刷盘/关闭句柄 |
| `src/LoggerImpl.h/.cpp` | 单例实现 + 同步逐条写入编排，不再使用后台刷新线程 |
| `test/main.cpp` | 纯 C++ 测试程序（无 Qt 依赖），覆盖全部 API |
| `CMakeLists.txt` | CMake 构建配置，C++14，/MT 静态链接运行时，支持 Ninja + VS2015 |

#### 设计决策实现细节

| 决策项 | 实现方式 |
|--------|----------|
| **DLL 边界 ABI** | 纯虚接口 `ILogger`，Write() 参数全部 `const char*`，QString/std::string 重载为内联函数编译到调用方 |
| **多进程安全** | Windows Named Mutex `Global\MeyerScan_Logger_Mutex`，500ms 超时，失败回退 `Local\`，构造失败则无保护写入 |
| **轮转规则** | 日期变更（Today() ≠ m_currentDate）→ 重置为当天主文件；主文件超过 10 MiB 后递增尾部序号 `_001/_002` |
| **刷新策略** | 每条日志同步写入：打开文件、追加一行、`FlushFileBuffers`、关闭句柄；后台可以移动/删除/打包日志文件 |
| **单例** | C++11 静态局部变量（Magic Statics），线程安全，`extern "C"` 导出 `GetLogger()` |
| **日志格式** | `[2026-06-24 08:42:14.893] [INFO] [Module] [Operation] [Dev:SN] [Case:ID] [Op:Name] content`；空 `Dev/Case/Op` 直接省略 |
| **空字段处理** | nullptr/空字符串 → "-"（单短横线），content 可为空字符串 |
| **行截断** | 2048 字节缓冲区，超出截断（snprintf 返回值处理） |
| **stdio 缓冲** | `setvbuf _IONBF` 禁用，保证行级原子性 |
| **目录创建** | `SHCreateDirectoryExW` 递归创建父目录 |
| **日志保留** | 永久保留，不自动删除（与第二次需求一致） |
| **敏感信息** | 不做脱敏，调用方给什么就存什么（与第二次需求一致） |

#### 测试覆盖

| 测试项 | 说明 |
|--------|------|
| 单例获取 | GetLogger() 非空验证 |
| 初始化 | Init() 创建目录+打开文件 |
| 五级别写入 | Debug/Info/Warning/Error/Fatal 各一条 |
| 级别过滤 | SetLogLevel(Warning) → Debug/Info 被抑制，Error 正常 |
| 多线程压力 | 10 线程 × 500 条 = 5000 条，无崩溃无交错 |
| Error 立即刷新 | Error 级别唤醒后台线程（100ms 内） |
| 优雅关闭 | Shutdown() → 排空 → 关文件 → join 线程 |

#### CMake 构建配置

- C++14 标准，MSVC 扩展关闭
- `/MT` 静态链接运行时（Logger 作为第一个加载模块不依赖 msvcr140.dll）
- `MEYER_LOGGER_EXPORTS` 私有定义，控制 dllexport/dllimport
- 可选 `BUILD_TESTS` 开关，测试 exe 输出到 bin/ 目录
- 支持 Ninja（VS Code 日常开发）和 VS2015 解决方案两种生成器

### 影响范围

- `f:\MeyerScan\MyLogger\`：完整创建 13 个文件，Logger.dll 功能已全部实现
- 全部注释为中文，代码逻辑和标识符不变
- Logger 模块可作为第一阶段 1.2 的基础，后续根据集成需要微调

---

## 2026-06-12（第二次） — 文档合并 + Logger 需求调整

### 用户要求

1. 将 `MeyerScan重构方案优化建议.md` 和 `MeyerScan重构任务总纲.md` 合并为一个文档
2. 修改日志模块设计：
   - 日志文件永久保留，不需要自动删除（可预留功能但暂不启用）
   - 敏感信息不需要脱敏，调用方给什么就存什么
   - 写入接口需要同时支持 `QString` 和 `std::string`（有的模块不依赖 Qt）

### AI 建议与采纳

#### 文档合并方案

将任务总纲中独有的内容（任务背景/功能需求/原则/目标/约束/开发顺序/文件层级）插入优化建议文档，并重新编排为 10 章的统一文档：

| 章节 | 内容 | 来源 |
|------|------|------|
| 一 | 任务背景 | 任务总纲 |
| 二 | 功能需求说明 | 任务总纲 |
| 三 | 重构原则 | 任务总纲 |
| 四 | 预期目标 | 任务总纲 |
| 五 | 开发约束 | 任务总纲 |
| 六 | 架构优化建议 | 优化建议 |
| 七 | 开发顺序 | 合并（取任务总纲的完整6阶段） |
| 八 | 项目文件层级 | 任务总纲 |
| 九 | 各模块功能详细设计 | 优化建议 |
| 十 | 开发环境方案 | 优化建议 |

原 `MeyerScan重构任务总纲.md` 改为指向合并文档的引用文件，不再独立维护。

#### Logger 设计修改

1. **日志永久保留**：
   - 删除了"30天自动清理"策略
   - 保留策略改为"永久保留，不自动删除"
   - 预留 `SetRetentionDays(int days)` 接口，`retentionDays=0` 表示永久保留
   - ConfigCenter 配置中 `logger.retentionDays` 默认值为 0
   - 定时清理逻辑代码可写好但不启用

2. **不做敏感信息脱敏**：
   - 删除了整个"敏感信息脱敏"设计小节
   - 日志格式说明中明确"原样存储不做脱敏"
   - Logger 核心职责中移除"敏感信息自动脱敏"
   - Crypto 使用场景表中移除"Logger.dll 敏感日志内容加密存储"

3. **接口支持 QString 和 std::string**：
   - 主接口使用 `const char*`（UTF-8）作为 DLL 边界的纯虚函数，确保 ABI 安全
   - 在 ILogger 类中提供内联的 `QString` 重载（`toUtf8().constData()` 转发）
   - 在 ILogger 类中提供内联的 `std::string` 重载（`.c_str()` 转发）
   - Init 方法同样提供三种重载
   - Qt 模块用 QString 版本，非 Qt 模块用 std::string 或 const char* 版本

### 影响范围

- `MeyerScan重构方案优化建议.md`：完整重写为 10 章合并文档，Logger 模块全部按要求修改
- `MeyerScan重构任务总纲.md`：改为引用文件，指向合并文档
- 后续只需维护一份权威文档

---

## 2026-06-12（第一次） — 文档优化 + 模块详细设计 + 开发环境方案

### 用户要求

1. 读取 `0228-口扫软件重构方案概述.docx` 和 `MeyerScan重构方案优化建议.md`
2. 基于读取内容，修改 `MeyerScan重构开发进度跟踪.md` 和 `MeyerScan重构任务总纲.md`
3. 在 `MeyerScan重构方案优化建议.md` 中增加：
   - 每个 DLL 的功能详细描述
   - 日志模块如何在其他模块中共同使用，并安全保存到同一个日志文件中
   - 日志文件生成规则
   - 扫描重建模块是否需要单独的日志文件
   - 模块详细设计
4. 增加开发环境内容：
   - 现有环境：Qt 5.6.3, VTK 8.0, VS2015, OpenCV 3.3.0, PCL 1.8
   - 新需求：VS Code + CMake + C++ Extension 生成各模块 exe 和 dll
   - 约束：模块项目也要能直接在 VS2015 中打开和编译
   - 可行性分析及开发方案
5. 在 `D:\wj\重构文档\` 下生成记录文档（本文档），用于追溯 AI 协作内容

### AI 建议摘要

#### 2.1 进度跟踪文档修改

- **CaseEntity**：从 `MeyerScan_CaseEntity.dll` 改为 `MeyerScan_CaseEntity.lib`（静态库），理由：纯数据结构定义，无运行时逻辑，DLL 化增加不必要的导出符号管理
- **Permission 合并**：删除独立的 `MeyerScan_PermissionUI.dll`（2.5），将其功能合并到 `MeyerScan_Permission.dll`（2.2），理由：PermissionUI 功能简单、代码量小，仅供 EngineeringSettings 调用
- **IPC 归入 Core.lib**：5.1 IPC 通信层明确标注归入 `MeyerScan_Core.lib`，作为公共工具/数据契约的一部分
- **第二阶段模块数**：从 6 个减为 5 个
- **EngineeringSettings 依赖调整**：前置依赖从 `2.2 + 2.3 + 2.5` 改为 `2.2 + 2.3`

#### 2.2 任务总纲修改

- 开发顺序表中 CaseEntity 标注为 `.lib`
- 删除 PermissionUI 条目，Permission 标注"合并原 PermissionUI"
- IPC 条目标注"归入 Core.lib"
- 项目文件层级调整：`Permission/` 不再有子目录；`IPC/` 移入 `Core/` 下；`CaseEntity/` 标注为静态库

#### 2.3 各模块功能详细设计（新增第七章，历史口径）

为 19 个模块编写了详细设计文档，包括。下表为当时口径，后续已修正：患者/订单统一为 CaseOrderService，校准统一为 Calibration3DUI / CalibrationColorUI 两个 Qt UI+流程+计算入口模块。

| 模块 | 设计要点 |
|------|----------|
| **Logger.dll** | 结构化日志格式、Named Mutex 多进程安全、按天+按大小双规则轮转、敏感信息脱敏、日志宏封装、ScanReconstructStudio 使用同一 Logger.dll 写入同一日志目录 |
| **Core.lib** | 数据契约 + 公共工具 + IPC（命名管道+POD序列化+心跳协议） |
| **ConfigCenter.dll** | 全局唯一配置入口、JSON 格式、版本校验/自动迁移/安全回滚（第二阶段）、变更回调 |
| **Crypto.dll** | AES-256-CBC、密钥轮换、文件加解密、被 Logger/Permission/Config/CaseManager/NetworkHelper/ScanDataIO 调用 |
| **UIComponents.dll** | 9 个通用控件（MeyerButton/Dialog/Toast/Table/Calendar/Loading/HelpButton/RestartReminder）+ ThemeManager |
| **CaseEntity.lib** | CaseInfo + OrderInfo 结构体定义，纯数据载体，预留 extData 扩展字段 |
| **CaseManager.dll** | Service 层（ICaseService）+ DAO 层（ICaseDao→SqliteDao/MysqlDao），数据库切换机制，第一阶段临时配置方案 |
| **CaseUI.dll** | 4 个页面（CaseListPage/DetailPage/CreateDialog/SendDialog），纯 UI 零业务逻辑 |
| **Permission.dll** | 六维校验算法（角色+客户+机型+版本+时间+配置），所有修改重启生效，含扫码配置界面 |
| **DeviceTransport.dll + DeviceCmd.dll** | 传输层/命令层分离设计，通信方式变更不影响命令层 |
| **Calibration.dll** | 三维校准（25幅采集流程）+ 颜色校准流程 |
| **ScanDataIO.dll** | 帧数据保存/读取/完整性校验 |
| **ScanDataPreProcess.dll** | 预处理流水线（解密→镜像→裁剪→颜色校准→AI消去），独立 DLL 便于多机型适配 |
| **ScanReconstructStudio.exe** | 独立进程、扫描状态机、IPC 通信、崩溃自恢复（心跳 3 秒 + 10 秒超时重启） |
| **NetworkHelper.dll** | 云端上传/下载/同步/邮件发送 |

##### 日志模块关键设计决策

| 决策项 | 结论 | 理由 |
|--------|------|------|
| **多进程安全方式** | Windows Named Mutex（`Global\MeyerScan_Logger_Mutex`） | 内核对象，进程崩溃自动释放不死锁 |
| **日志文件共享** | 所有进程（含 ScanReconstructStudio）写入**同一目录同一文件** | 便于排查跨进程问题，日志中通过 Module 字段区分来源 |
| **ScanReconstructStudio 日志** | 使用同一 Logger.dll，**不需要单独日志文件** | 统一日志目录便于售后一次性打包 |
| **轮转规则** | 按天（00:00）+ 按大小（10MB），双规则 | 覆盖常见场景 |
| **命名格式** | `MeyerScan_YYYYMMDD.log`；超过大小后 `MeyerScan_YYYYMMDD_NNN.log` | 默认每天一个主文件；NNN 三位序号，从 001 开始 |
| **保留策略** | 30 天自动清理 | 启动时 + 每天 03:00 检查 |
| **缓冲区策略** | Error/Fatal 立即刷盘，Info 5 秒或 100 条批量刷盘 | 平衡性能与可靠性 |
| **敏感脱敏** | Logger 内部自动处理（正则匹配+替换） | 调用方无感知，不遗漏 |

#### 2.4 开发环境方案（新增第八章）

##### 可行性结论：**完全可行**

核心原理：CMake 作为构建描述中间层，分别生成 Ninja 构建文件（VS Code）和 VS2015 `.sln` 解决方案文件，两者共享同一套 MSVC 14.0 编译器工具链，ABI 完全一致。

##### 关键配置

- **VS Code 开发**：安装 C/C++ 扩展 + CMake Tools 扩展，使用 Ninja 生成器 + MSVC 14.0 工具链
- **VS2015 开发**：执行 `cmake --preset vs2015-debug` → 生成 `Build/Debug/MeyerScan.sln` → 在 VS2015 中打开
- **统一构建配置**：通过 `CMakePresets.json` 管理 Debug/Release、Ninja/VS2015 多种 preset
- **C++ 标准**：C++14（VS2015 最高支持）
- **Qt 集成**：`CMAKE_AUTOMOC/AUTOUIC/AUTORCC` + `find_package(Qt5)`
- **兼容注意事项**：字符集 Unicode、不使用 C++17 特性、Ninja 需从 VS2015 命令行启动 VS Code

##### 提供给团队的 CMake 模板

提供了：
- 顶层 `CMakeLists.txt` 模板
- DLL 模块 `CMakeLists.txt` 模板（含 MODULE_NAME 宏定义）
- EXE 模块 `CMakeLists.txt` 模板
- 静态库 `CMakeLists.txt` 模板
- `.vscode/settings.json`、`launch.json`、`tasks.json` 推荐配置
- `CMakePresets.json` 完整示例（4 个 preset）

### 采纳决策

以上所有建议均已采纳，具体修改位置：

| 修改文档 | 修改内容 |
|----------|----------|
| `MeyerScan重构开发进度跟踪.md` | CaseEntity→lib、Permission 合并、IPC→Core.lib、模块数调整、任务变更记录更新 |
| `MeyerScan重构任务总纲.md` | 开发顺序表、项目文件层级、模块标注更新 |
| `MeyerScan重构方案优化建议.md` | 新增第七章（19 个模块详细设计）、新增第八章（开发环境方案含 VS Code + CMake + VS2015） |
| `MeyerScan重构-AI协作记录.md`（本文档） | 新增，记录本次及后续 AI 协作内容 |

### 影响范围

- DLL 总数：从 18 个降至 **16 个**（优化建议采纳后）
- 开发环境：新增 VS Code + CMake 工作流，保留 VS2015 兼容性
- 文档体系：从 3 个核心文档扩展为 4 个（新增 AI 协作记录）

---

## 2026-06-16 — 架构设计与接口规范文档生成

### 用户要求

1. 读取 D:\wj\重构文档下的几个 md 文件，提出改进意见
2. 定义模块的边界划分、模块解耦方案
3. 定义接口规范、头文件定义
4. 绘制整体架构图和模块依赖关系图
5. 定义模块间入参/出参、函数接口
6. 编写设计思路、注释规范、开发规约
7. 输出架构说明 + .h 接口声明（C++风格）
8. 更新现有 md 文档

### AI 实施内容

#### 1. 文档分析改进意见

| 问题 | 改进建议 |
|------|----------|
| 文档内容过于分散 | 整合为 3 个核心文档 |
| 接口定义不够具体 | 输出完整的头文件声明 |
| 缺少模块边界明确定义 | 定义职责边界、接口边界、依赖边界 |
| 缺少调用时序图 | 补充启动流程、病例创建、扫描流程时序图 |
| 注释规范未落地 | 给出具体的注释模板 |
| 开发规约不够具体 | 细化命名规范、代码风格、错误处理规范 |

#### 2. 创建《架构设计与接口规范》文档

文件路径：`D:\wj\重构文档\MeyerScan架构设计与接口规范.md`

包含以下章节：
- 一、现有文档改进意见
- 二、模块边界划分与解耦原则（分层架构、职责边界、解耦约束）
- 三、整体架构图（系统架构总览、模块依赖关系图）
- 四、模块间入参/出参规范（跨 DLL/跨进程数据传递规范）
- 五、公共类型定义（ErrorCode、Result、IpcCommand）
- 六、模块接口头文件定义（Core、ConfigCenter、Crypto、Permission、CaseEntity、CaseManager、DeviceCmd、ScanDataIO、NetworkHelper）
- 七、调用时序图（病例创建、扫描流程、心跳检测）
- 八、注释规范（文件头、类、函数、枚举、结构体模板）
- 九、开发规约（命名规范、代码风格、错误处理、日志规范、禁止事项）
- 十、后续工作

#### 3. 生成接口头文件（11 个文件）

目录：`D:\wj\重构文档\接口规范\`

| 文件名 | 说明 |
|--------|------|
| `ErrorCode.h` | 统一错误码枚举，按模块分类（0xxxx-9xxxx） |
| `Result.h` | 泛型结果包装 `Result<T>` 和 `VoidResult` |
| `IpcCommand.h` | IPC 命令/响应枚举、超时配置、扫描状态 |
| `CaseEntity.h` | CaseInfo、OrderInfo、ScanSchemaInfo、DeviceInfo 结构体 |
| `ConfigCenter.h` | IConfigCenter 接口（配置读写唯一入口） |
| `Crypto.h` | ICrypto 接口（AES-256 加解密、密钥管理） |
| `Permission.h` | IPermission 接口（六维权限校验、扫码配置） |
| `CaseManager.h` | ICaseService 接口（病例 CRUD、数据库切换） |
| `DeviceCmd.h` | IDeviceCmd 接口（设备命令、扫描控制、帧获取） |
| `ScanDataIO.h` | IScanDataIO 接口（帧数据保存/加载、完整性校验） |
| `NetworkHelper.h` | INetworkHelper 接口（云端上传/下载、邮件发送） |

#### 4. 接口设计要点

| 设计要点 | 说明 |
|----------|------|
| **DLL 边界安全** | 所有接口使用 `const char*`（UTF-8）作为参数，避免 std::string/QString ABI 问题 |
| **POD 结构体** | 跨进程传递的结构体含 version 字段 + 预留字段，禁用指针/QObject |
| **工厂函数** | 每个模块提供 `extern "C"` 导出的 `GetXxx()` 工厂函数 |
| **错误码分类** | 0xxxx 通用，1xxxx 数据库，2xxxx 设备，3xxxx 网络...便于定位问题来源 |
| **Result 包装** | Service 层接口统一返回 `Result<T>`，调用方必须检查 IsSuccess() |

#### 5. 更新现有文档

- `MeyerScan重构方案优化建议.md`：新增第十一章引用架构设计文档
- `MeyerScan重构开发进度跟踪.md`：任务变更记录新增 2026-06-16 条目
- `MeyerScan重构-AI协作记录.md`：新增本条记录

### 影响范围

- 新增文档 1 个：《架构设计与接口规范》（约 180KB）
- 新增接口头文件 11 个：`接口规范/*.h`
- 更新文档 3 个：方案优化建议、进度跟踪、AI协作记录

---

## 2026-06-16 — 架构优化方案修正与新增设计决策

### 用户反馈

1. **患者与订单是关联业务**：患者下有多个订单，他们的 CRUD 和搜索应该归为一类
2. **扫描方案是订单的一部分**：不是独立业务实体，不需要单独的 DLL
3. **扫描重建模块保持整体**：暂不拆分，后续再决定
4. **建单功能与扫描重建分离**：建单作为 DLL，扫描重建作为 EXE，两者分开开发和维护

### AI 实施内容

#### 1. 业务理解修正

| 修正项 | 原理解（错误） | 修正后理解 |
|--------|--------------|-----------|
| 患者与订单 | 分离为两个独立模块 | **关联业务，统一管理** |
| 扫描方案 | 独立的业务实体 | **订单的一部分** |
| CaseManager 拆分 | 拆分为 6 个模块 | **拆分为 4 个模块** |
| ScanReconstructStudio.exe | 拆分为多个模块 | **保持整体** |

#### 2. 新增首页模块设计决策

**问题**：首页是否独立为 DLL？

**分析**：
| 选项 | 好处 | 弊端 |
|------|------|------|
| 独立 HomeUI.dll | 可独立升级、定制化替换 | 增加 DLL 管理成本 |
| 放入主程序 EXE | 无额外 DLL | 定制需重编译主程序 |

**推荐**：首页独立为 `HomeUI.dll`

理由：首页定制化需求高，独立 DLL 便于快速替换。

#### 3. 建单与扫描重建分离方案

```
重构后架构：
┌─────────────────────────────────────────────────────────────────┐
│                        MeyerScan.exe（主程序壳）                 │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │              建单页面（嵌入区域 A）                           │ │
│  │              OrderCreation.dll                              │ │
│  └────────────────────────────────────────────────────────────┘ │
│                          │ 传递订单数据（IPC/文件）               │
│                          ▼                                      │ │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │              扫描重建区域（嵌入区域 B）                        │ │
│  │              ScanReconstructStudio.exe                      │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

**技术方案**：
- OrderCreation.dll：返回 QWidget，嵌入主程序布局
- ScanReconstructStudio.exe：通过 `QWindow::fromWinId()` 嵌入
- IPC 通信：Qt 命名管道传递订单数据

**三种进入模式**：
| 模式 | 建单页面 | 扫描重建 | 数据来源 |
|------|---------|---------|---------|
| 创建订单 | ✅ 显示 | ✅ 显示 | 用户新建 |
| 案例管理加载 | ✅ 显示 | ✅ 显示 | 数据库加载 |
| 练习扫描 | ❌ 隐藏 | ✅ 显示 | 默认参数 |

**界面一致性保证**：
- 统一样式表（全局 QSS）
- Qt 窗口嵌入技术
- 无缝切换

#### 4. 修正后的模块列表

- DLL 总计：19 个（+3）
- EXE 总计：2 个（主程序 + 扫描重建）
- 静态库总计：2 个

#### 5. 更新的文档

- `MeyerScan架构优化方案-修正版.md`：新增第八至十一节

### 影响范围

- 新增模块设计决策说明
- 确认建单 DLL + 扫描重建 EXE 分离方案可行

---

## 2026-06-16 — 文档合并整理 + MyLogger 代码审查修复

### 用户要求

1. 把架构优化方案-修正版.md 的结果记录在相关 md 文件中
2. D:\wj\重构文档下 md 文件有好几个，进行内容合并和分类
3. 设想的合并结果：重构任务总览、AI协作记录、进度跟踪、架构详细设计与接口规范与模块拆分细节
4. 合并后删除多余的 md 文件
5. 审查 F:\MeyerScan\MyLogger 下的日志模块代码，只修改代码部分，项目属性不动
6. 修改同步到相关 md 文档

### AI 实施内容

#### 1. 文档合并

**合并前**：8 个 md 文档

| 文档 | 内容 |
|------|------|
| MeyerScan重构任务总纲.md | 任务背景、原则、约束 |
| MeyerScan重构方案优化建议.md | 架构优化、模块详细设计 |
| MeyerScan架构优化方案-细粒度拆分.md | 模块拆分方案 |
| MeyerScan架构设计问答记录.md | 设计问答 |
| MeyerScan架构优化方案-修正版.md | 修正后的拆分方案 |
| MeyerScan架构设计与接口规范.md | 接口规范 |
| MeyerScan重构开发进度跟踪.md | 进度跟踪 |
| MeyerScan重构-AI协作记录.md | AI协作记录 |

**合并后**：4 个 md 文档

| 文档 | 职责 |
|------|------|
| **MeyerScan重构任务总览.md** | 任务背景、功能需求、重构原则、预期目标、开发约束、开发顺序、项目文件层级、启动流程 |
| **MeyerScan架构设计与接口规范.md** | 模块边界划分、分层架构、接口规范、依赖关系、调用时序、注释规范、开发规约、模块拆分方案、首页与建单分离方案 |
| **MeyerScan重构开发进度跟踪.md** | 各阶段进度、任务状态、变更记录 |
| **MeyerScan重构-AI协作记录.md** | AI协作历史记录 |

#### 2. 删除的冗余文件

- `MeyerScan重构任务总纲.md`
- `MeyerScan重构方案优化建议.md`
- `MeyerScan架构优化方案-细粒度拆分.md`
- `MeyerScan架构设计问答记录.md`
- `MeyerScan架构优化方案-修正版.md`

#### 3. MyLogger 代码审查与修复

**发现的问题**：

| 文件 | 行号 | 问题 | 修复 |
|------|------|------|------|
| `include/Logger.h` | 221-225 | `QString Init()` 重载缺少 `level` 参数 | 添加 `level` 参数，修正为 `Init(logDir.toUtf8().constData(), level)` |

**代码质量评估**：

| 方面 | 评估 | 说明 |
|------|------|------|
| 架构设计 | ✅ 已更新 | 早期采用单例模式、后台刷新线程、跨进程互斥量、零 Qt 依赖；2026-06-24 根据实际测试反馈改为逐条同步写入、刷盘并关闭句柄 |
| 线程安全 | ✅ 已更新 | 使用 `std::atomic` 做级别过滤，使用 Windows 命名互斥量保护“选文件 + 写入”；不再使用 `std::condition_variable` 后台缓冲线程 |
| 跨进程安全 | ✅ 优秀 | 使用 `Global\MeyerScan_Logger_Mutex` 命名互斥量 |
| DLL边界安全 | ✅ 优秀 | 主要接口使用 `const char*`，内联重载支持 `std::string`/`QString` |
| 文件轮转 | ✅ 优秀 | 按日期+按大小双规则轮转 |
| 代码注释 | ✅ 优秀 | 中文注释详尽，设计决策清晰 |

### 2026-06-24 追加修正：日志模块按现场测试反馈重构

用户反馈旧日志实现与预期差距较大，重点问题包括：每天文件数量规则不直观、空字段占位观感差、长期持有句柄导致后台不方便移动/删除日志文件、调用方式偏繁琐、Qt 模块希望能直接输出 `QString`。

本轮处理结果：

1. **文件规则调整**：默认每天写入 `MeyerScan_YYYYMMDD.log`；只有当天文件达到大小上限后才生成 `MeyerScan_YYYYMMDD_001.log`、`_002.log` 等尾部序号文件。
2. **内容格式调整**：`deviceId`、`caseId`、`operator` 为空时不再输出 `[Dev:-]`、`[Case:-]`、`[Op:-]`，日志级别也不再固定宽度补空格。
3. **写入策略调整**：删除后台缓冲线程和 `LogBuffer.*` 构建引用；每条日志在命名互斥量保护下完成“选择文件 -> 打开 -> 追加一行 -> `FlushFileBuffers` -> 关闭句柄”。
4. **调用方式优化**：保留 `ILogger::Write(...)` 兼容接口；根据用户澄清，“连续输出”指每个模块 Init 阶段缓存一份 `ILogger* m_logger`，后续在该变量生命周期内持续通过 `m_logger->Write(...)` 输出日志，不是新增流式 `<<` 封装。
5. **Qt 使用边界**：`Logger.dll` 本体仍保持无 Qt 依赖，确保最早加载和非 Qt 模块可用；Qt 模块通过 `Logger.h` 的内联便利层直接使用 `QString`。
6. **其它模块调用修正**：没有真实操作员上下文的模块传空字符串，不再为了占位传 `"System"`，避免日志中出现无意义 `[Op:System]`。

### 影响范围

- 文档：8 个 → 4 个
- 代码修复：1 处（Logger.h 第 221-225 行）
- 更新的文档：`MeyerScan重构开发进度跟踪.md`（新增修复记录）

---

> **文档维护**：每次用户提出重构相关要求并采纳 AI 建议后，在本文件末尾追加一条记录。AI 回复内容以概括形式记录，保留关键设计决策和代码模板。

> **初始创建**：2026-06-12

---

## 2026-06-17 — Database.dll v1.1.0 接口规范对齐 + 代码审查修复

### 用户要求

1. 读取 MyLogger 和 MyDatabase 项目，审查需要修改的地方
2. MyDatabase 引入日志模块（MeyerScan_Logger.dll）
3. 所有修改记录到 4 个 MD 文档
4. 抽离新的原则/约束（CRT 链接规则等）到相关 MD 文档

### AI 审查发现

#### 1. 关键问题：CRT 链接冲突

| 项目 | Debug CRT | Release CRT |
|------|-----------|-------------|
| Logger.dll | `/MTd`（静态） | `/MT`（静态） |
| Database.dll | `/MDd`（动态） | `/MD`（动态） |
| DatabaseTest.exe | `/MTd`（静态）→ **改为 `/MDd`** | `/MT`（静态）→ **改为 `/MD`** |

**问题**：Logger 用 `/MT`（静态 CRT，零 Qt 依赖可最先加载），Database 用 `/MD`（动态 CRT，Qt 需要）。
**解决**：Database 不链接 Logger.lib，改用 `LoadLibrary` + `GetProcAddress` 运行时动态加载。详见架构文档"CRT 链接规则"。

#### 2. Database.dll 接口规范对齐

| # | 问题 | 修改方式 |
|---|------|----------|
| 1 | **返回值不规范**（返回 bool/DbResult） | 改为返回 Result\<T\>/VoidResult，在 Database.h 中临时定义 ErrorCode 和 Result 模板 |
| 2 | **日志用 qDebug()** | 替换为动态加载的 Logger 实例，LogError/LogInfo 方法写入结构化日志 |
| 3 | **缺 MEYER_MODULE_NAME** | vcxproj 添加 `MEYER_MODULE_NAME="MeyerScan_Database"` |
| 4 | **DatabaseTest CRT 不匹配** | 从 `/MT` 改为 `/MD`，匹配 Database.dll |
| 5 | **MySQL 密码硬编码** | 保留，添加 @todo 加密配置升级注释 |
| 6 | **MySQL 备份路径硬编码** | 保留，添加 @todo 配置读取注释 |

#### 3. 新原则/约束记录

| 约束 | 内容 |
|------|------|
| **CRT 链接规则** | 无 Qt 依赖的基础模块用 `/MT`（静态 CRT）；有 Qt 依赖的模块用 `/MD`（动态 CRT）；跨 CRT 边界仅传递 `const char*`/POD |
| **注释政策** | 源码注释用中文；日志字符串用英文；UI 显示文字用 `tr()` 包裹，tr 内用英文 |
| **Database 角色** | 通用基础设施（方案 A）—— 裸 SQL 执行，无业务逻辑 |
| **ModuleVersion** | 使用简单字符串格式，不返回 ModuleVersion 结构体 |

#### 4. 其他架构调整

| 调整 | 内容 |
|------|------|
| **EngineeringSettings** | 职责精简：移除"数据库类型切换"（移入 CaseManager），移除"功能开关控制"（移入 Permission） |
| **Permission** | 冷热路径分离：CheckAccess（热，纯内存）与 LoadRuleFile（冷，有文件 IO）内部隔离 |

### AI 实施内容

#### 1. 代码修改

| 文件 | 修改内容 |
|------|----------|
| `MyDatabase/include/Database.h` | 新增 ErrorCode/Result\<T\>/VoidResult 定义；所有方法改为返回 Result/VoidResult；保留中文注释 |
| `MyDatabase/src/DatabaseImpl.h` | 新增 GetLogger() 静态动态加载机制；添加 LogInfo/LogError 辅助方法；修改方法签名对齐新接口 |
| `MyDatabase/src/DatabaseImpl.cpp` | 全部实现改为返回 Result/VoidResult；qDebug→Logger；密码/路径硬编码加 @todo 注释 |
| `MyDatabase/test/main.cpp` | TEST_ASSERT 适配 Result 接口；输出改用 fprintf（不依赖 Logger） |
| `MyDatabase/MeyerScan_Database.vcxproj` | Debug/Release 均添加 `MEYER_MODULE_NAME` 和 Logger include 路径 |
| `MyDatabase/DatabaseTest.vcxproj` | CRT 从 `/MT` 改为 `/MD`；添加 Logger include 路径 |
| `MyDatabase/README.md` | 更新 API 示例（Result 风格）、版本历史、接口说明 |

#### 2. 文档修改

| 文档 | 修改内容 |
|------|----------|
| **任务总览** | 新增 CRT 链接规则和工程设置调整说明 |
| **架构设计与接口规范** | Database 职责边界更新；EngineeringSettings/Permission 调整；注释政策更新；后续工作状态更新；新增 10.3 设计决策记录 |
| **进度跟踪** | Database 1.4 状态从"未开始"更新为"已完成 v1.1.0"；阶段总进度从 1/4 改为 2/4；任务变更记录新增 3 条 |
| **AI协作记录** | 新增本条记录 |

### 影响范围

- `f:\MeyerScan\MyDatabase\`：4 个源码文件 + 2 个 vcxproj + README.md 更新
- `D:\wj\重构文档\`：4 个 MD 文档全部更新
- 新增约束：CRT 链接规则、注释政策、ModuleVersion 策略
- Logger 集成方式：运行时 LoadLibrary（非编译时链接）

---

## 2026-06-17 — 两份架构文档补充审查与矛盾修复

### 用户要求

详细通读《任务总览》和《架构设计与接口规范》两份文档，检查遗漏和不一致之处，完成后更新 AI协作记录。

### AI 审查发现

通读两份文档后，发现 8 处不一致/矛盾问题：

| # | 问题 | 位置 | 严重度 |
|---|------|------|--------|
| 1 | 构建约束写死 `/MT`，与新增的 CRT 链接规则矛盾 | 任务总览 §5.3 | 🔴 |
| 2 | 启动流程只有 3 步（DB→Login→Home），缺少 Logger/ConfigCenter/Permission 加载阶段 | 任务总览 §8 | 🔴 |
| 3 | "禁止直接操作数据库"约束与 Database 通用基础设施角色（方案 A）矛盾 | 架构文档 §2.3 | 🔴 |
| 4 | §11.4 Database 接口模板仍然是旧的 bool 风格 | 架构文档 §11.4 | 🔴 |
| 5 | 十一/十二节引用了已删除的 md 文件（死链） | 架构文档 §11/§12 | 🟡 |
| 6 | §11 CaseManager 拆分描述（6 个模块）与实际情况（保持整体）不符 | 架构文档 §11 | 🟡 |
| 7 | ModuleVersion 定义为结构体，但实际已决定使用字符串策略 | 架构文档 §6.1 | 🟡 |
| 8 | 两份文档的版本号均未更新 | 文档末尾 | 🟡 |

### AI 修复内容

| 文档 | 版本 | 修复项 |
|------|------|--------|
| **任务总览** | v3.0 → v3.1 | ① §5.3 构建约束从"统一 /MT"改为区分对待 ② §8 启动流程补充为完整 6 步（Logger→Config→DB→Permission→Login→Home） |
| **架构设计与接口规范** | v1.2 → v1.3 | ① §2.3 "禁止直接操作数据库"→"禁止**业务层**直接操作数据库" ② §6.1 ModuleVersion 改为字符串 typedef ③ §11.4 Database 接口模板改为 Result\<T\> 风格 ④ §11/§12 移除已删除文件的引用，改为说明注释 ⑤ §11.2/§11.3 加说明标注实际决策与优化建议的差异 |

### 核心逻辑修正

| 修正项 | 修正前 | 修正后 |
|--------|--------|--------|
| **CRT 策略一致性** | §5.3 写死 `/MT`；§5.2 新增 CRT 规则允许 `/MD` → 矛盾 | 统一：有 Qt 依赖用 `/MD`，无 Qt 依赖用 `/MT` |
| **数据库访问约束** | 禁止所有模块直接操作数据库 | 禁止**业务层**直接操作数据库，但 Database.dll 作为通用基础设施可直接被调用 |
| **启动流程完整性** | 3 步遗漏关键阶段 | 6 步覆盖全部核心模块 |
| **ModuleVersion 策略** | 结构体定义（不实用） | 字符串 typedef（与实现一致） |
| **CaseManager 拆分** | §11 说拆 6 个模块（过时） | 加说明：实际不拆分，保持整体 |
| **文档引用** | 指向已删除的文件（死链） | 移除引用，改为说明注释 |

### 影响范围

- `D:\wj\重构文档\`：两份架构文档各更新，AI协作记录新增本条
- 修正 8 处文档矛盾，确保文档内部逻辑一致

---

## 2026-06-17 — 新增模块版本资源规范

### 用户要求

每个 EXE/DLL 需要嵌入 Windows 版本信息（文件属性→详细信息），打包时生成各模块版本清单，规则记录到对应 md 文件。

### AI 实施内容

#### 1. 版本资源文件（Version.rc）

创建 Windows 版本资源文件模板，以 Database.dll 为例：

| 文件 | 内容 |
|------|------|
| `MyDatabase/src/Version.rc` | Windows `VS_VERSION_INFO` 资源，含 CompanyName、FileVersion、ProductName 等字段 |
| `MyDatabase/MeyerScan_Database.vcxproj` | 添加 `<ResourceCompile Include="src\Version.rc" />` |
| `MyDatabase/MeyerScan_Database.vcxproj.filters` | 添加 Resource Files 筛选器 |

#### 2. 文档更新

| 文档 | 新增内容 |
|------|----------|
| **任务总览** | §5.3 版本资源文件约束；§5.4 版本清单规范（格式 JSON、命名约定、生成时机） |
| **架构设计与接口规范** | §9.6 版本资源规范（Version.rc 模板、规范细则表格、版本清单 JSON 示例、版本升级流程） |
| **进度跟踪** | 通用约束表（版本资源/版本号一致性/版本清单）；变更记录新增条目 |
| **AI协作记录** | 本条记录 |

#### 3. 版本资源规范要点

| 规则 | 内容 |
|------|------|
| **必填字段** | CompanyName、FileDescription、FileVersion、InternalName、OriginalFilename、ProductName、ProductVersion |
| **版本号一致性** | Version.rc 的 FILEVERSION 必须与 GetModuleVersion() 返回的字符串版本号一致 |
| **FILETYPE** | DLL 用 `VFT_DLL`，EXE 用 `VFT_APP` |
| **版本清单** | 打包时生成 `version_manifest.json`，列所有模块版本 |
| **SemVer** | 版本号遵循语义化版本：主版本.次版本.修订号 |

### 影响范围

- `f:\MeyerScan\MyDatabase\src\Version.rc`：新建
- `f:\MeyerScan\MyDatabase\MeyerScan_Database.vcxproj` / `.filters`：新增 Version.rc 编译项
- `D:\wj\重构文档\`：4 个 MD 文档全部更新
- 后续每个新建模块都需参照此模板创建 Version.rc

---

## 2026-06-17 — 细模块优先架构调整

### 用户要求

先阅读 `helpCenter` 理解当前软件功能，再阅读重构相关 Markdown，判断现有设计是否满足“后期人工维护和扩展方便、模块尽可能细、职责独立、功能简单、代码量少”的目标；如不满足，说明是否可以修改、如何修改、是否有好处。

### AI 判断

现有方案已经完成了基础设施、设备、校准、网络、登录、扫描进程隔离等关键拆分，但以下模块仍偏粗：

| 模块 | 问题 |
|------|------|
| CaseManager.dll | 同时承载患者/病例、订单、扫描方案、导入导出、数据库切换，容易继续膨胀 |
| CaseUI.dll | 浏览、详情、发送等页面职责混在一起，需要至少按页面子模块组织 |
| Permission.dll | 权限热路径和配置展示/扫码入口混合，容易引入 UI 依赖 |
| EngineeringSettings.dll | 设置入口容易变成“所有设置都放这里”的大模块 |
| ScanReconstructStudio.exe | 独立进程是正确的，但内部仍需按采集、显示、编辑、数据处理工具拆分 |

### 修改内容

| 文档 | 修改点 |
|------|--------|
| MeyerScan架构设计与接口规范.md | 模块职责表改为 CaseService / OrderService / ScanSchemaService / DataExport / Statistics；PermissionConfigUI 独立；ScanReconstructStudio 内部子模块化；病例域接口示例从单一大接口改成多个小接口；文档版本更新为 v1.5 |
| MeyerScan重构任务总览.md | 开发顺序新增病例服务、订单服务、扫描方案服务、权限配置 UI、数据导入导出、统计服务；扫描重建阶段新增 ScanCapture / ModelViewer / EditTools / ProcessingTools；项目文件层级同步更新；文档版本更新为 v3.2 |
| MeyerScan重构开发进度跟踪.md | 第二阶段和第五阶段任务拆细；修正 EngineeringSettings 旧职责；新增“细模块优先调整”变更记录 |

### 新原则

| 原则 | 说明 |
|------|------|
| 职责先拆细 | 先按业务职责拆分清楚，避免单个模块继续变大 |
| 物理 DLL 适度 | 需要独立升级、替换交付、多人并行开发的模块做 DLL；纯内部工具可先做源码子目录或静态库 |
| UI 不承载业务 | 页面只做展示和交互，业务操作调用对应 Service |
| 扫描进程保持独立 | 继续保留崩溃隔离，但进程内部按工具拆分，降低维护复杂度 |

### 预期收益

- 后期人工维护时能按模块定位问题，减少读大文件和跨功能修改。
- 订单、扫描方案、导入导出、统计等功能可以独立开发和测试。
- 权限判断核心不被 UI 依赖污染，热路径更稳定。
- 扫描数据处理工具可以逐个开发、逐个验证，降低 ScanReconstructStudio 的复杂度。
- 代价是 DLL/工程数量增加，接口和版本清单管理要求更高，需要严格执行 Version.rc、GetModuleVersion 和 plugin_manifest 规范。

---

## 2026-06-17 — MyLogger/MyDatabase 审查与边界修正

### 用户要求

读取 `F:\MeyerScan\MyLogger` 和 `F:\MeyerScan\MyDatabase`，判断是否符合刚刚的重构规划；可自行修改代码，并将相关记录更新到 `D:\wj\重构文档` 下 4 个 Markdown。用户进一步确认：`Database` 不必去 Qt，使用 Qt 连接 QMySQL/QSQLite 更方便；当前更需要管的是边界，不是“要不要 Qt”。

### 设计结论

| 结论 | 说明 |
|------|------|
| Database 保留 Qt | `QSqlDatabase` / `QSqlQuery` 对 MySQL 和 SQLite 的封装成熟，符合当前 Qt 技术栈 |
| 架构风险在边界 | Database 依赖 Qt 不影响分层；真正要禁止的是业务逻辑、UI、权限判断进入 Database |
| Logger 仍保持零 Qt | Logger 是最先加载的基础设施模块，应继续无 Qt 依赖、/MT 编译 |

### 代码修改

| 项目 | 修改 |
|------|------|
| MyLogger | `ILogger` 增加 `GetModuleVersion()`；`LoggerImpl` 实现版本返回；新增 `src/Version.rc`；VS2015 `.vcxproj/.filters` 和 CMake 均加入 Version.rc；示例中的 `CaseManager` 改为 `CaseService`；修正 CMake 中关于 CRT 的旧注释 |
| MyDatabase | `DatabaseImpl::GetLogger()` 不再用空路径调用 `Init()`，只获取主程序已初始化的 Logger 实例；`SetDatabaseType()` 不再持锁调用 `Disconnect()`，避免递归锁死锁；SQL 错误返回不再使用临时 Qt 字节数组指针；修复 `VoidResult` 非法构造函数；同步本地 `ILogger` ABI 声明；源码转为 UTF-8 with BOM；补充 `DatabaseTest -> MeyerScan_Database` 解决方案依赖；日志内容改为英文；默认 SQLite 路径从 `MyCaseManager` 改到 `MyDatabase`；README 和 `Database.h` 明确“允许 Qt SQL，但禁止业务逻辑” |

### 验证结果

| 项目 | 结果 |
|------|------|
| MyLogger Release x64 | MSBuild 通过，0 warning / 0 error，输出 `F:\MeyerScan\MyLogger\bin\Release\MeyerScan_Logger.dll` |
| MyDatabase Release x64 | MSBuild 通过，0 warning / 0 error，输出 `F:\MeyerScan\MyDatabase\bin\Release\MeyerScan_Database.dll` 和 `DatabaseTest.exe` |
| DatabaseTest.exe | 运行通过，19 passed / 0 failed；备份源目录不存在仅作为环境提示，未计入失败 |

### 文档修改

| 文档 | 修改 |
|------|------|
| MeyerScan架构设计与接口规范.md | Database 职责表增加允许 Qt5Sql/QSqlDatabase 内部实现；新增 Database Qt 依赖设计决策；补充 UTF-8 with BOM 编码约束；版本更新到 v1.7 |
| MeyerScan重构任务总览.md | 技术约束增加“Database 保留 Qt SQL 实现，重点约束职责边界”；补充构建/编码验证结论；版本更新到 v3.4 |
| MeyerScan重构开发进度跟踪.md | 任务变更记录新增 MyLogger/MyDatabase 审查修正，以及 MyDatabase 构建与测试收口记录 |
| MeyerScan重构-AI协作记录.md | 本条记录 |

### 后续注意

- Database 可以使用 Qt5Core/Qt5Sql，但不得成为病例/订单/权限/设置的业务入口。
- 上层 `CaseService`、`OrderService`、`ScanSchemaService` 等 Service 决定业务语义，Database 只执行数据库基础能力。
- 主程序必须在 Database 写日志前初始化 Logger，否则 Database 动态加载 Logger 后只会静默跳过日志，不主动创建日志目录。

## 2026-06-17 — 重构方案再梳理：模块细化、首页/加载订单规则、六维权限闭环

### 用户要求

重新梳理重构方案，尽可能功能隔离、功能归类、每个模块代码量少、易维护；重点评估首页模块、加载订单规则模块、公共方法模块的必要性；梳理六维权限控制流程、定制客户、功能阉割方案是否合理、是否存在漏洞、是否达到预期目的。

### 评审结论

| 议题 | 结论 | 原因 |
|------|------|------|
| HomeUI.dll | 保留，但只做入口 UI 和 OEM 展示定制 | 首页变化频繁，独立 DLL 有价值；但不能承载订单/权限/扫描规则 |
| 加载订单规则 | 新增 `OrderWorkflowService.dll` | 新建、加载、继续扫描、进入处理、进入发送的规则会被 HomeUI、CaseUI、OrderCreateUI、ScanReconstructStudio 复用，放 UI 中会膨胀和重复 |
| 公共方法模块 | Core.lib 收窄 | 只放 ErrorCode/Result、POD 契约、FeatureId、IPC 命令、版本结构和少量无状态工具；禁止放业务规则 |
| 六维权限 | 需要全链路门禁 | UI 隐藏不是安全边界，Permission、Workflow、Service、IPC 接收端都要复核 |
| 功能阉割 | 分强/中/弱三层 | 高价值功能应“模块不交付/不加载 + Permission 拒绝 + Service/IPC 拒绝”；普通定制走配置和 UI |

### 关键设计调整

1. 新增 `OrderWorkflowService.dll`，统一输出 `OrderWorkflowDecision`：拒绝、显示建单、开始扫描、继续扫描、进入处理、进入发送。
2. `OrderCreateUI.dll` 只做基本信息和扫描方案表单展示，不决定加载订单规则。
3. `HomeUI.dll` 只显示入口，并调用 Permission 显示规则和 Workflow 决策。
4. `PermissionCheckContext` 补充设备序列号/加密狗 ID、配置方案 ID；避免只按机型授权导致同型号设备误授权。
5. 权限规则合并优先级明确为：显式 Deny 最高、授权文件 Allow、客户定制规则、基础白名单、未声明默认 Deny。
6. 权限更新时间回拨需要降级：允许基础浏览和安全退出，禁止新建、扫描、导出、授权更新等高价值动作。

### 文档修改

| 文档 | 修改 |
|------|------|
| MeyerScan架构设计与接口规范.md | 增加细模块归类与拆分判定；新增 OrderWorkflowService 接口；重写首页/建单/加载订单规则章节；补充六维权限流程和功能阉割闭环；版本更新到 v1.8 |
| MeyerScan重构任务总览.md | 技术约束补充 Core.lib 边界、首页边界、OrderWorkflowService、功能阉割闭环；阶段任务新增 1.8；版本更新到 v3.5 |
| MeyerScan重构开发进度跟踪.md | 新增 OrderWorkflowService 任务；更新 Permission 和 HomeUI 任务边界；新增本次重构方案再梳理记录 |

## 2026-06-17 — 创建 HomeUI / CaseUI Qt 框架并跑通 Logger + Database

### 用户要求

在 `F:\MeyerScan\MyLogger` 和 `F:\MeyerScan\MyDatabase` 同级目录创建首页和案例管理项目；两个项目都使用 Qt 并有界面 UI；先写框架，保证 VSCode 和 VS2015 能编译通过，能调用数据库和日志模块，先跑通流程；参考帮助文档中的首页和浏览模块图文描述；完成后更新文档并提交代码到 GitHub。

### 新增项目

| 项目 | 输出 | 说明 |
|------|------|------|
| `F:\MeyerScan\MyHomeUI` | `MeyerScan_HomeUI.dll` + `HomeUITest.exe` | 首页四入口框架：Create / Browse / Practice / Settings |
| `F:\MeyerScan\MyCaseUI` | `MeyerScan_CaseUI.dll` + `CaseUITest.exe` | 案例管理框架：Patients / Orders 两个 Tab |

### 技术实现

- 两个模块均为 Qt Widgets DLL，使用 `/MD`，符合 Qt 模块 CRT 规则。
- 不使用 `.ui` 和 `Q_OBJECT`，先用纯 C++ 构建界面，降低 VS2015/moc 配置复杂度。
- 通过导入库链接 `MeyerScan_Database.lib`，运行时复制 `MeyerScan_Database.dll`。
- 通过 `QLibrary` 动态加载 `MeyerScan_Logger.dll`，避免跨 CRT 链接 Logger.lib。
- 测试宿主支持 `--smoke` 参数，创建界面后自动退出，便于自动验证。
- PostBuild 复制 Database、Logger、libmysql 和 sqldrivers 依赖到模块输出目录。

### 验证结果

| 项目 | 验证 |
|------|------|
| MyHomeUI | VS2015 Release x64 构建通过，0 warning / 0 error；`HomeUITest.exe --smoke` 运行通过 |
| MyCaseUI | VS2015 Release x64 构建通过，0 warning / 0 error；`CaseUITest.exe --smoke` 运行通过 |

### 后续注意

- HomeUI 仍只做入口 UI，后续通过 Permission 控制入口显示，通过 OrderWorkflowService 决定下一步动作。
- CaseUI 只做展示框架，后续 CRUD、搜索、导入导出、发送、加载订单规则分别接入 CaseService / OrderService / DataExport / NetworkHelper / OrderWorkflowService。
- 当前先用英文界面占位，后续统一走 `tr()` 和 UI 文案规范。

## 2026-06-17 — HomeUI / CaseUI Qt 运行库收口

### 用户要求

用户确认首页和案例管理仍然必须引入 Qt 界面；文档同级目录附近存在 Qt DLL 文件夹，需要处理 UI 模块运行库问题。

### 处理结论

| 项 | 结论 |
|------|------|
| Qt UI 方向 | HomeUI 和 CaseUI 继续作为 Qt Widgets UI 模块，不退回无界面或控制台框架 |
| 运行库版本 | 项目当前按 `C:\Qt\Qt5.6.3\5.6.3\msvc2015_64` 编译，输出目录必须复制同版本 Qt 5.6.3 运行库 |
| 附近 Qt DLL | `D:\wj\My-wj\SQLite\sqlitestudio311\SQLiteStudio` 中存在 Qt 5.7 DLL，只能作为环境线索，不能混入 Qt 5.6.3 编译产物 |
| 插件布局 | `platforms` 只放 `qwindows.dll`；`sqldrivers` 只放 `qsqlmysql.dll` 和 `qsqlite.dll`，避免把 Qt 核心 DLL 误放进插件目录 |

### 代码修改

| 项目 | 修改 |
|------|------|
| `F:\MeyerScan\MyHomeUI\MeyerScan_HomeUI.vcxproj` | PostBuild 显式复制 Qt5Core/Qt5Gui/Qt5Widgets/Qt5Sql、platforms/qwindows、sqldrivers/qsqlmysql/qsqlite，同时复制 Database、Logger、libmysql |
| `F:\MeyerScan\MyCaseUI\MeyerScan_CaseUI.vcxproj` | 同步 HomeUI 的 Qt 5.6.3 运行库复制规则 |

### 验证结果

| 项目 | 结果 |
|------|------|
| MyHomeUI Release x64 | MSBuild 通过，0 warning / 0 error；输出目录包含 Qt 5.6.3 DLL、platforms/qwindows、sqldrivers/qsqlmysql/qsqlite；`HomeUITest.exe --smoke` 通过 |
| MyCaseUI Release x64 | MSBuild 通过，0 warning / 0 error；输出目录包含 Qt 5.6.3 DLL、platforms/qwindows、sqldrivers/qsqlmysql/qsqlite；`CaseUITest.exe --smoke` 通过 |

### 后续注意

- 后续打包脚本必须沿用“编译 Qt 版本 = 发布 Qt 运行库版本”的规则。
- 文档附近的 SQLiteStudio Qt 5.7 DLL 不应作为当前 MeyerScan UI 模块的发布依赖。
- HomeUI/CaseUI 仍只负责界面展示和入口状态，业务规则继续下沉到 Permission、OrderWorkflowService、CaseService/OrderService 等模块。

## 2026-06-17 — GitHub 模块目录规则确认

### 用户要求

用户明确要求：首页模块和案例管理模块也要提交到 GitHub；它们在仓库中的位置应与日志模块、数据库模块一致，都位于 `weijian118/MeyerScan` 下，并且每个模块以一个文件夹形式存放、提交和记录。该规则需要同步更新到对应 Markdown 文档。

### 规则确认

| 项 | 规则 |
|------|------|
| Git 根目录 | `F:\MeyerScan` |
| GitHub 仓库 | `https://github.com/weijian118/MeyerScan.git` |
| 模块目录 | 每个模块使用仓库根目录下的一级文件夹 |
| 已有示例 | `MyLogger/`、`MyDatabase/` |
| 本次模块 | `MyHomeUI/`、`MyCaseUI/` 与 `MyLogger/`、`MyDatabase/` 同级 |
| 提交范围 | 只提交对应模块目录和必要根级文件，不混入临时目录、压缩包、构建产物或无关实验文件 |

### 当前 Git 状态

| 项 | 状态 |
|------|------|
| 本地提交 | `fe88c44 Add HomeUI and CaseUI Qt shells` |
| 提交内容 | 已包含 `MyHomeUI/`、`MyCaseUI/` 以及本轮前置的 Logger/Database 规范修正 |
| 远端状态 | 本地 `master` 领先 `origin/master` 1 个提交 |
| 推送结果 | GitHub HTTPS 推送时连接被重置：`Recv failure: Connection was reset`；待网络恢复后重试 |

### 文档同步

- `MeyerScan重构任务总览.md`：补充 GitHub 仓库规范，明确 `F:\MeyerScan` 为 Git 根目录，每个模块使用一级目录提交。
- `MeyerScan架构设计与接口规范.md`：设计决策表补充 GitHub 模块目录规则。
- `MeyerScan重构开发进度跟踪.md`：为 MyHomeUI/MyCaseUI 增加 GitHub 目标路径和推送状态记录。
- `MeyerScan重构-AI协作记录.md`：记录本次规则确认。

## 2026-06-18 — 最终方案梳理口径修正：聚焦产品和代码实现

### 用户要求

用户明确修正方向：不要围绕“医疗”字样做额外推导，也不要写开发周期相关内容；只考虑产品和代码实现层面，可以考虑 3 人小团队如何长期维护。需要把这些思考结果更新到对应 Markdown 文档。

### 修正后的核心判断

| 议题 | 结论 |
|------|------|
| 架构口径 | 继续采用主 EXE + 静态库 + 插件 DLL + 独立进程 |
| 关注重点 | 产品稳定、数据可靠、异常可恢复、问题可排查、模块可维护 |
| 不再展开 | 不再从“医疗级”字样推导额外流程；不再给开发周期估算 |
| 小团队维护 | 3 人团队更需要小模块、少接口、单向依赖、可独立验证，而不是更多框架和更多 DLL |
| 模块拆分 | 模块小有利于人工维护，但前提是边界正确；否则小文件和小 DLL 也会增加调试成本 |

### 文档修改

| 文档 | 修改 |
|------|------|
| `MeyerScan重构任务总览.md` | 将“医疗级合规与安全”改为“产品稳定性与数据安全”；删除开发周期预估；新增 3 人小团队维护策略和代码量建议；版本更新到 v3.10 |
| `MeyerScan架构设计与接口规范.md` | 保留轻量化拆分判定，版本说明改为产品和代码实现口径；版本更新到 v1.13 |
| `MeyerScan重构开发进度跟踪.md` | 将“周期路线图”改为“主链路路线图”；将“阶段门禁”改为“阶段检查项”；去掉医疗级和周期表述；新增 2026-06-18 变更记录 |
| `MeyerScan重构-AI协作记录.md` | 追加本条记录 |

### 后续执行标准

- 每个模块优先保证能独立编译、独立 smoke、独立阅读。
- UI 不放业务；公共层不放业务；Database 不放业务语义。
- 每次只改一个模块或一条调用链，避免多个半成品互相牵制。
- 文档记录的是实际开发规则，不做过重流程，不追求形式上的完整。

## 2026-06-18 — UI 多屏/DPI、多语言与旧 mysql.sql 边界检查

### 用户要求

用户追问：`F:\MeyerScan\MyCaseManager\mysql.sql` 是否已经用上；多显示器和分辨率适配是否考虑；多个模块都有界面时如何规划、同步、适配多分辨率和多语言，每个界面模块是否维护自己的多个 qm。用户要求把今天结果记录到相关 Markdown，并检查已开发模块代码，修改完不用提交 GitHub。

### 检查结论

| 项 | 结论 |
|------|------|
| `mysql.sql` 使用状态 | 当前 MyDatabase/MyHomeUI/MyCaseUI 均未读取或执行 `F:\MeyerScan\MyCaseManager\mysql.sql` |
| 旧脚本定位 | 旧 `mysql.sql` 只作为历史 schema 参考，不能由 UI 或 Database 硬编码自动加载 |
| 数据库迁移边界 | Database.dll 只负责连接、事务和裸 SQL 执行；建表/迁移脚本由 ConfigCenter 或病例域服务按版本选择后调用 Database 执行 |
| 多屏/DPI | 主 EXE 和测试宿主负责在 QApplication 前设置 High DPI；UIComponents 后续统一提供 ScreenUtil/DpiUtil/LayoutRules |
| 多语言 | 采用 Common qm + 各 UI 模块独立 qm；LanguageManager 统一加载、切换、回退 |
| 业务返回值 | Service/Workflow/Permission 返回错误码、原因码、功能 ID，不返回已翻译文案 |

### 代码修改

| 文件 | 修改 |
|------|------|
| `F:\MeyerScan\MyHomeUI\test\main.cpp` | 在 QApplication 创建前设置 `Qt::AA_EnableHighDpiScaling` 和 `Qt::AA_UseHighDpiPixmaps`；新增按当前屏幕可用区域限制大小并居中的启动逻辑 |
| `F:\MeyerScan\MyCaseUI\test\main.cpp` | 同步 HomeUI 测试宿主的 High DPI 与多屏初始窗口逻辑 |
| `F:\MeyerScan\MyHomeUI\src\HomeUIImpl.cpp` | 将当前界面显示文字纳入 Qt 翻译机制 |
| `F:\MeyerScan\MyCaseUI\src\CaseUIImpl.cpp` | 将标题、Tab、按钮、搜索占位符和表头文字纳入 Qt 翻译机制 |

### 验证结果

| 项目 | 结果 |
|------|------|
| MyHomeUI Release x64 | MSBuild 通过，0 warning / 0 error；`HomeUITest.exe --smoke` 通过 |
| MyCaseUI Release x64 | MSBuild 通过，0 warning / 0 error；`CaseUITest.exe --smoke` 通过 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：补充旧 mysql.sql 定位、数据库迁移职责、UI 全局适配、多语言 qm 策略和 LanguageManager 边界。
- `MeyerScan重构任务总览.md`：补充技术约束，明确旧脚本不自动接入、多屏/DPI 统一适配、多语言按模块维护。
- `MeyerScan重构开发进度跟踪.md`：补充 HomeUI/CaseUI 当前代码状态和 2026-06-18 变更记录。
- `MeyerScan重构-AI协作记录.md`：记录本次检查、修改和验证结果。

### 后续注意

- 当前 HomeUI/CaseUI 的多屏逻辑仍在测试宿主内，只是为了先跑通；正式主 EXE 建好后，应迁入 UIComponents 并由主 EXE 统一调用。
- 后续创建 `UIComponents.dll` 时，应优先实现屏幕/DPI/主题/语言管理，而不是先做复杂控件。
- 新建 CaseService/OrderService 前，需要先规划版本化 schema/migration 目录，避免继续依赖旧 `mysql.sql`。

## 2026-06-18 — 数据库相关模块缺口与 UI 直连边界确认

### 用户问题

用户询问：数据库相关模块还缺什么；`MyCaseUI` 或 `MyHomeUI` 使用数据库功能时，是否直接对接 `MyDatabase`。

### 结论

当前代码中，`MyHomeUI` 和 `MyCaseUI` 确实直接链接并调用 `MeyerScan_Database.dll`，但只用于框架期验证：

```text
HomeUI/CaseUI test host
  -> HomeUI/CaseUI
    -> GetDatabase()
    -> Init()
    -> Connect()
```

这只是确认 DLL、Qt SQL 驱动、配置文件和数据库连接链路能跑通，不代表正式业务架构。正式业务链路必须是：

```text
HomeUI / CaseUI
  -> OrderWorkflowService / CaseService / OrderService / ScanSchemaService
    -> DAO / Repository
      -> MyDatabase
        -> MySQL / SQLite
```

### 数据库相关缺口（历史口径，后续已修正）

> 后续修正：患者/病例数据服务与订单数据服务不再拆成 CaseService.dll / OrderService.dll，统一归 `CaseOrderService.dll`；加载订单规则仍归 `OrderWorkflowService.dll`。

| 缺口 | 归属 | 说明 |
|------|------|------|
| 数据库配置来源 | ConfigCenter.dll | 数据库类型、路径、MySQL 地址、账号等不能长期由 UI 测试宿主硬编码 |
| 建表/迁移 | ConfigCenter + 病例域服务 | 版本化 migration，不自动执行旧 `mysql.sql` |
| 患者/病例数据服务 | CaseService.dll | 患者 CRUD、查询、状态管理 |
| 订单数据服务 | OrderService.dll | 订单 CRUD、订单状态、患者订单关联 |
| 扫描方案服务 | ScanSchemaService.dll | 牙位、修复体、材料、齿色等结构化保存 |
| 加载订单规则 | OrderWorkflowService.dll | 新建、加载、继续扫描、发送前判断 |
| DAO/Repository | 各 Service 内部 | 封装 SQL 细节，UI 和 Workflow 不碰 SQL |
| schema 版本表 | 数据库 schema | 记录当前数据库版本，支持升级判断 |
| MySQL/SQLite 差异脚本 | migration 目录 | 两种数据库分别维护脚本，不靠临时拼接兼容 |
| 数据错误码映射 | Core/ErrorCode + Service | SQL 错误转成业务可理解的错误码 |
| CRUD/迁移测试 | 各 Service 测试 | 不只测连接，还要测建表、升级、CRUD、事务 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：收紧 Database 角色描述，明确 UI/Workflow 不得绕过 Service/DAO 直接执行业务 SQL；新增 UI 直连 Database 边界。
- `MeyerScan重构任务总览.md`：技术约束中补充 UI 不直连业务数据库；第一阶段注意事项中补充数据库主链路缺口。
- `MeyerScan重构开发进度跟踪.md`：将 HomeUI/CaseUI 的数据库连接说明改为“临时健康检查和 smoke 验证”，新增正式页面改走 Service/Workflow 的待办。
- `MeyerScan重构-AI协作记录.md`：记录本次问答、缺口清单和最终调用链路。

### 后续执行标准

- `HomeUI` 正式功能不直接调用 Database，只调用 Permission 和 OrderWorkflowService。
- `CaseUI` 正式功能不直接调用 Database，列表、搜索、删除、打开订单、导入导出都走 Service/Workflow/DataExport。
- `Database` 可以继续保留 `ExecuteSql` / `ExecuteScript` 等底层能力，但调用业务 SQL 的代码应收敛在 Service 内部 DAO。

## 2026-06-22 — 功能清单、模块拆分与已完成模块维护规范补充

### 用户要求

用户指出《重构任务总览》存在功能清单和模块拆分缺口，需要补充：云端账号登录（离线/联网）、订单上传口扫云、第三方软件（美亚美牙）拉起本地口扫软件并自动建单、HIS/Worklist 建单、MyUpdate.exe 自动更新、弱化扫描进程看门狗自动重启、重新评估 CaseEntity.lib 形态、每个模块目录增加修改记录、GitHub 英文提交信息更详细、去掉具体“3 人”表述、增加整体模块清单和扩展性目标示例、三维校准和颜色校准拆成独立 DLL、明确 Qt 模块间 IPC 是否能用 QString/QJson 传复杂订单信息。用户还要求检查 `F:\MeyerScan` 下已完成四个模块：MyCaseUI、MyDatabase、MyHomeUI、MyLogger，并做对应优化。

### 文档结论

| 议题 | 结论 |
|------|------|
| 核心功能清单 | 补充云端账号离线/联网登录、订单上传口扫云、第三方拉起建单、HIS/Worklist、自动更新 |
| 建单模块 | OrderCreateUI 独立是必要的，第三方建单、HIS/Worklist、手工建单都应复用它 |
| 自动更新 | 新增 MyUpdate.exe，和 MeyerScan.exe 同级，以独立进程完成策略比对、补丁下载、关闭主程序、覆盖升级、重启主程序 |
| 看门狗 | 弱化“超时自动重启”，重点改为 MeyerScan.exe 与 ScanReconstructStudio.exe 的订单上下文、状态、进度同步和异常记录 |
| CaseEntity | 暂按 `.lib`/契约层规划，但字段高频变化，必须使用 version、extensions、字段 key、schema 映射；必要时改为头文件契约或 Service DTO |
| 模块记录 | 每个模块目录必须有 `CHANGELOG.md`，按时间记录修改内容、验证结果和注意事项 |
| GitHub 提交 | 最新规范已改为提交信息统一用中文，并且要描述具体变更，避免“更新/修复/修改”这类空泛信息 |
| 团队表述 | 去掉具体“3 人”角色描述，改为轻量化团队、分模块开发、模块内测试、统一集成/联调 |
| 模块清单 | 在架构规范中新增整体模块清单：功能简述、拆分原因、功能边界 |
| 扩展示例 | 补充第三方建单、HIS/Worklist、OEM/海外版、云端升级策略如何通过模块协作完成 |
| 校准 | 三维校准拆为 Calibration3D.dll，颜色校准拆为 ColorCalibration.dll，校准入口只做协调/UI |
| IPC 复杂信息 | 可以用 JSON 表达患者/订单上下文，但跨进程传 UTF-8 JSON 字节或上下文文件路径，不传 QString/QJsonObject 对象本身 |

### 代码侧检查结论

| 模块 | 结论 |
|------|------|
| MyLogger | 结构和边界基本符合规划；补模块内 CHANGELOG 即可 |
| MyDatabase | 边界基本符合规划；硬编码 MySQL 凭据和备份路径仍是已记录 TODO，等 ConfigCenter 接入再改，不做半套配置 |
| MyHomeUI | 当前 Database 调用仅为健康检查；README 需继续强调临时性质；补 CHANGELOG |
| MyCaseUI | 当前 Database 调用仅为健康检查；README 需继续强调临时性质；补 CHANGELOG |

### 文档同步

- `MeyerScan重构任务总览.md`：补充功能清单、扩展性目标示例、轻量化团队表述、MyUpdate.exe 流程、CaseEntity 风险、校准拆分、项目目录。
- `MeyerScan架构设计与接口规范.md`：补充模块职责表、整体模块清单、IPC JSON 上下文规范、状态同步时序、近期决策记录。
- `MeyerScan重构开发进度跟踪.md`：更新路线图、CaseEntity、第三方/HIS/MyUpdate 任务、校准拆分、IPC/扫描进程任务、任务变更记录。
- `MeyerScan重构-AI协作记录.md`：记录本次补充和代码检查结论。

## 2026-06-22 - 四个已完成模块代码优化补充

### 背景

在补充核心功能清单、MyUpdate、校准拆分、IPC JSON 上下文规则和轻量化团队维护口径后，继续检查 `F:\MeyerScan` 下四个已完成模块：`MyLogger`、`MyDatabase`、`MyHomeUI`、`MyCaseUI`。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyLogger | `Write()` 在 Logger 未初始化或已经 `Shutdown()` 后直接丢弃日志，避免模块退出阶段迟到日志重新写入已关闭会话 |
| MyDatabase | `Disconnect()` 和数据库类型切换时显式 `QSqlDatabase::removeDatabase(connectionName)`，避免 Qt SQL 连接留在全局连接池导致插件/进程退出顺序风险 |
| MyHomeUI | smoke 宿主传入 logDir 时由 HomeUI 初始化/关闭本地 Logger 会话；Shutdown 顺序改为先关闭 Database，再关闭 Logger |
| MyCaseUI | 与 HomeUI 保持相同的 Logger/Database 生命周期规则 |

### 验证结果

- `MeyerScan_Logger.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan_Database.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan_HomeUI.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan_CaseUI.sln` Release x64 构建通过，0 warning / 0 error。
- `HomeUITest.exe --smoke` 退出码为 0。
- `CaseUITest.exe --smoke` 退出码为 0。
- `DatabaseTest.exe` 退出码为 0；既有注意项仍存在：MySQL 备份源路径当前硬编码，若本机不存在 `F:\MeyerScan\MyDatabase\MySQL\data\mscan\`，robocopy 会输出路径错误。该问题保留到 ConfigCenter/备份路径配置阶段处理。

## 2026-06-22 - 记录语言规范纠偏

### 用户要求

用户明确要求：GitHub 提交日志也要使用中文；源码注释、模块 `CHANGELOG.md`、GitHub commit message 都要写入相关文档形成固定规范，避免后续再次遗忘。

### 最新规范

| 项目 | 规范 |
|------|------|
| 源码注释 | 统一使用中文，说明设计意图、边界、注意事项和关键实现原因 |
| 模块变更记录 | 每个模块目录维护中文 `CHANGELOG.md`，按日期记录修改内容、验证结果和注意事项 |
| GitHub 提交日志 | commit message 统一使用中文，描述具体变更内容和影响范围，避免只写“更新”“修复”“修改” |
| 可保留英文的内容 | 日志字段 key、内部错误 key、翻译 key、协议字段名等需要程序稳定识别的内容可保持英文 |

### 已同步文档

- `MeyerScan重构任务总览.md`：更新 GitHub 仓库规范、模块记录规则、注释与记录语言规则。
- `MeyerScan架构设计与接口规范.md`：更新代码风格和近期设计决策记录。
- `MeyerScan重构开发进度跟踪.md`：新增记录语言规范补充条目。
- `MeyerScan重构-AI协作记录.md`：记录本次纠偏，明确最新规范覆盖旧的英文提交口径。

## 2026-06-22 - 安装打包模块纳入整体方案

### 用户要求

用户要求在重构方案中增加打包环节，包括生成安装包、安装过程中的自定义界面显示、自定义安装流程、文件夹层级等。该内容当前先作为一个模块纳入整体方案，后续真正实现该模块时再详细设计。

### 规划结论

| 项目 | 结论 |
|------|------|
| 模块名称 | 暂定 `MyInstaller / Packaging` |
| 模块定位 | 发布交付模块，不作为 MeyerScan.exe 运行时插件 |
| 主要职责 | 生成安装包、自定义安装界面、自定义安装流程、安装目录层级、依赖收集、版本清单写入和核对 |
| 禁止事项 | 不做业务建单、扫描采集、数据库 CRUD、权限判断或云端上传 |
| 后续细化 | 安装器技术选型、脚本结构、安装 UI 细节、回滚策略、补丁包格式等后续实现时再详细设计 |

### 建议安装目录层级

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

### 已同步文档

- `MeyerScan重构任务总览.md`：补充核心功能清单、架构形态、技术约束、第六阶段任务、安装打包模块说明和项目目录。
- `MeyerScan架构设计与接口规范.md`：补充模块职责、模块归类、整体模块清单、安装打包规范和近期设计决策。
- `MeyerScan重构开发进度跟踪.md`：新增第六阶段安装打包模块任务和任务变更记录。
- `MeyerScan重构-AI协作记录.md`：记录本次用户要求、规划结论和同步范围。

---

## 2026-06-22 - MyLogin 测试宿主与 MainExe 最小链路集成

### 用户要求

1. 在 `F:\MeyerScan` 下新增 `MyLogin` 文件夹，调用既有 `D:\wj\My-wj\MyLogin\lib\MeyerLoginWidget.dll/.lib`，正确显示登录界面。
2. 登录参数先使用离线许可 `D:\wj\My-wj\MyLogin\license.lic`、缩放系数 `1.0`、语言索引简体中文、登录地址 `https://myscan.meyerop.com/login`。
3. 在登录模块接通后开发 `MyMainExe`，输出 `MeyerScan.exe`，流程为日志 → 数据库检查 → 登录 → 首页 → 案例管理。
4. Release 目录需要能完整运行，相关 DLL、Qt 文件和已有模块依赖要复制到模块输出目录。

### 实施内容

| 模块 | 内容 |
|------|------|
| `MyLogin/` | 新增 VS2015/VSCode 工程，输出 `MeyerLoginTest.exe`，用于独立验证既有登录 DLL |
| `MyMainExe/` | 新增主程序入口工程，输出 `MeyerScan.exe`，作为新架构主 EXE 壳 |
| 登录参数 | 使用离线许可、默认缩放 1.0、简体中文、口扫云登录地址 |
| MainExe 链路 | 初始化 Logger；调用 Database 读取 `config/db_config.json` 做健康检查；调用 `MeyerLoginWidget.dll`；登录成功后加载 HomeUI/CaseUI |
| 验证参数 | `MyLogin` 增加 `--smoke`；`MyMainExe` 增加 `--smoke` 和 `--smoke-main` |
| Release 依赖 | 补齐 Qt、平台插件、SQL 驱动、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi、登录 qm、许可文件和已有模块 DLL |

### 验证结果

| 命令 | 结果 |
|------|------|
| `MSBuild F:\MeyerScan\MyLogin\MeyerScan_MyLogin.sln /p:Configuration=Release /p:Platform=x64` | 通过 |
| `MSBuild F:\MeyerScan\MyMainExe\MeyerScan_MainExe.sln /p:Configuration=Release /p:Platform=x64` | 通过 |
| `F:\MeyerScan\MyLogin\bin\Release\MeyerLoginTest.exe --smoke` | 返回 0 |
| `F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke` | 返回 0 |
| `F:\MeyerScan\MyMainExe\bin\Release\MeyerScan.exe --smoke-main` | 返回 0 |

### 设计决策

1. MainExe 只做启动、模块编排和窗口容器，不写业务 SQL、订单规则、扫描算法或设备协议。
2. MainExe 直接调用 Database 仅用于启动健康检查；正式患者/订单/扫描方案功能必须走 CaseService、OrderService、ScanSchemaService、OrderWorkflowService。
3. 当时 HomeUI 尚未暴露“案例管理/浏览”按钮事件，MainExe 暂用工具栏切换 CaseUI；后续已在同日补齐 HomeUI 入口事件和 CaseUI 返回首页事件。
4. 既有登录头文件存在 VS2015 代码页/声明警告，当前只读取稳定的 `LoginReturnParameters.currentStatus`。后续建议新增 `LoginAdapter`，隔离外部头文件编码、字段和枚举变化。
5. 安装打包模块后续必须按依赖闭包收集运行文件，不能只复制直接依赖 DLL。

### 已同步文档

- `MeyerScan重构任务总览.md`：补充当前 MainExe 集成落地状态。
- `MeyerScan架构设计与接口规范.md`：补充 MyLogin、MainExe、LoginAdapter、Release 依赖闭包决策。
- `MeyerScan重构开发进度跟踪.md`：更新 1.1 主程序壳、3.3 登录集成状态和验证结果。
- 模块内 `README.md` / `CHANGELOG.md`：记录构建、运行验证、依赖和已知风险。

---

## 2026-06-22 - MyMainExe/minMain 重新验证与整体方案复核

### 用户要求

用户要求把 `minMain` 项目重新验证测试，并根据重构文档整体考虑是否符合当前方案。经本地目录检索，`F:\MeyerScan` 下没有单独名为 `minMain` 的项目；本次按当前主入口模块 `MyMainExe` / `MeyerScan.exe` 进行复测。

### 复测结果

| 验证项 | 结果 |
|------|------|
| `MeyerScan_MainExe.sln` Release x64 构建 | 通过，0 warning / 0 error |
| `MeyerScan_MyLogin.sln` Release x64 构建 | 通过，0 warning / 0 error |
| `MeyerScan.exe --smoke` | 返回 0 |
| `MeyerScan.exe --smoke-main` | 返回 0 |
| `MeyerLoginTest.exe --smoke` | 返回 0 |
| MainExe Release 依赖闭包 | 非 API Set DLL 缺失为空，当前目录共 50 个 DLL |
| 日志检查 | 可看到 MainExe 与 Database 的 Shutdown/Disconnect 记录 |

### 架构复核结论

1. 当前 MainExe 仍符合“薄主 EXE”定位：只做 Logger 初始化、Database 健康检查、Login、HomeUI、CaseUI 编排。
2. MainExe 未加入业务 SQL、订单规则、扫描算法或设备协议，符合重构文档中“主 EXE 只做壳和编排”的边界要求。
3. 当前硬编码项仍属于框架期临时实现：登录 URL、语言、许可路径、数据库配置回退路径。后续应迁入 ConfigCenter。
4. 既有登录头文件仍直接暴露给 MainExe，后续建议新增 `LoginAdapter`，把参数构造、状态转换、头文件编码风险隔离起来。
5. 当时 HomeUI 尚未暴露“浏览/案例管理”入口事件，MainExe 暂用工具栏切换 CaseUI；后续已在同日追加 HomeUI 入口回调和 CaseUI 返回首页回调。

### 已同步文档

- 更新 `MyMainExe/CHANGELOG.md`，记录本次复测结果和架构复核结论。
- 更新 `MeyerScan重构任务总览.md`，补充 MainExe 复测记录。
- 更新 `MeyerScan架构设计与接口规范.md`，补充 MyMainExe 复测结论。
- 更新 `MeyerScan重构开发进度跟踪.md`，新增本次任务变更记录。

---

## 2026-06-22 - 首页与浏览模块双向切换、丝滑切页和客户操作日志补齐

### 用户要求

用户反馈首页已经能显示，但点击“浏览”按钮没有反应；随后要求在浏览模块中增加“返回首页”按钮，并明确所有界面切换过程要丝滑、不能闪现。同时要求在关键步骤增加日志，最终做到客户每一步操作都输出日志，并同步更新总文档和模块内部文档。

### 实施内容

| 模块 | 调整 |
|------|------|
| MyHomeUI | 新增 `SetEntryCallback()` 入口事件接口；首页创建、浏览、练习、设置按钮点击时写 `EntryClicked` 日志；“浏览”入口上报 `HomeEntryBrowse` 给 MainExe |
| MyCaseUI | 新增 `SetActionCallback()` 操作事件接口；浏览模块顶部新增“返回首页”按钮；患者/订单工具按钮、搜索回车、页签切换都写 `UserAction` 日志 |
| MyMainExe | 接收登录返回参数，登录成功后进入首页；接收 HomeUI 入口事件和 CaseUI 操作事件；历史初版曾用预创建 widget + `QStackedWidget` 切换 HomeUI/CaseUI；2026-06-25 已改为单内容区替换全屏页面；记录登录状态、首页入口、浏览操作和 `PageSwitch` 日志 |

### 设计结论

1. HomeUI、CaseUI、OrderCreateUI 等 UI DLL 只上报入口或操作 ID，不直接切换其他 UI 模块。
2. MeyerScan.exe 作为主壳统一管理主页面切换。历史初版采用 `QStackedWidget` 预创建并复用页面；2026-06-25 已改为单内容区替换当前全屏页面，首页和浏览不再是并列兄弟页。
3. 后续如果需要淡入淡出等视觉过渡，应集中放在 MainExe 或 UIComponents，不在各 UI 模块内分别实现。
4. 客户每一步可见操作必须写结构化日志；UI 模块记录模块内操作，MainExe 记录跨模块导航和页面切换，Service 后续记录业务成功/失败和状态变化。
5. 日志字段 key、operation、actionId 可保持稳定英文，源码注释、模块 CHANGELOG、GitHub commit message 仍按既定规范使用中文。

### 验证结果

| 验证项 | 结果 |
|------|------|
| `MeyerScan_CaseUI.sln` Release x64 构建 | 通过，0 warning / 0 error |
| `MeyerScan_MainExe.sln` Release x64 构建 | 通过，6 个已知外部登录头文件警告 / 0 error |
| `MeyerScan.exe --smoke-main` | 返回 0 |
| `MeyerScan.exe --smoke` | 返回 0 |
| `CaseUITest.exe --smoke` | 返回 0 |

### 已同步文档

- 更新 `MeyerScan重构任务总览.md`：补充主页面无闪现切换、客户操作日志必记、当前 MainExe/HomeUI/CaseUI 双向切换落地状态。
- 更新 `MeyerScan架构设计与接口规范.md`：补充 UI 模块只发事件 ID、MainExe 集中切页、当时的 `QStackedWidget` 复用页面方案、日志规范和近期设计决策；该页面方案已在 2026-06-25 被单内容区替换方案取代。
- 更新 `MeyerScan重构开发进度跟踪.md`：更新 MainExe、HomeUI、CaseUI 已完成项和任务变更记录。
- 更新 `MyMainExe`、`MyHomeUI`、`MyCaseUI` 的 `README.md` / `CHANGELOG.md`。

---

## 2026-06-23 - 多分辨率、多语言、运行路径、共享 UI、配置权限、版本清单、等待页和单实例规则落地

### 用户要求

用户提出 9 类约束和开发诉求：重新评估 1920x1080 等比缩放方案和多语言长度差异；日志、图标、资源等路径不得使用 `QDir::currentPath()`；当前是否应开发共享 UI 组件模块及弹窗归属；是否应开发配置中心和权限模块并控制首页/浏览入口显隐、数据库类型切换；建单模块和扫描重建模块是否需要统一壳子保持界面一致；不可见模块需要析构释放资源；MeyerScan.exe 启动时需要生成 EXE/DLL 版本清单；启动检查需要等待页；MeyerScan.exe 只允许运行一个实例；整体仍坚持模块边界清晰、功能分割彻底、代码量小和易人工维护。

### 设计结论

| 议题 | 结论 |
|------|------|
| 多分辨率 | 不继续把 1920x1080 绝对坐标等比缩放作为主布局方案；1920x1080 只作为设计稿和图标/边距/间距参考。页面必须用 Qt Layout、sizePolicy、最小/最大宽度和文本自适应 |
| 多语言 | 禁止按语言写 if/else 调整控件位置和大小；翻译变长由布局、弹性空间、换行、省略号或 tooltip 消化 |
| 运行路径 | 日志、配置、图标、qm、版本清单都以 `QCoreApplication::applicationDirPath()` 或 ConfigCenter 的安装目录为基准，禁止用 `QDir::currentPath()` |
| 共享 UI | 当前阶段可以开发 UIComponents；普通按钮、标签、输入框、下拉框、表格样式、公共弹窗、等待页、DPI/Layout 规则先归入 UIComponents |
| 弹窗模块 | 暂不单独拆弹窗 DLL，除非后续形成独立业务流程或独立发布价值 |
| 配置与权限 | 可以先做 ConfigCenter/Permission 轻量骨架，先打通数据库类型、首页“设置”、浏览“返回首页”等流程开关；配置加解密接口后续接 Crypto |
| 建单与扫描一致性 | 规划 OrderScanWorkspaceShell/工作台壳，统一顶部步骤、导航、状态栏和页面容器；壳只做容器和导航，不做建单保存或扫描算法 |
| 资源释放 | 按资源重量决定缓存或释放：轻量页面可短期复用保证切换体验，重资源页面离开时必须释放 widget、暂停线程、释放显存/大内存 |
| 版本清单 | 当时规划 VersionManager 独立生成 `logs/versionList/versionList_时间戳.json`；2026-06-24 已并入 MainExe，2026-06-25 又统一为读取 `config/version_modules.json` 的清单驱动方式 |
| 等待页 | 启动检查由 MainExe 编排，等待页由 UIComponents 创建，只展示状态，不承载检查逻辑 |
| 单实例 | MainExe 使用单实例控制；重复启动只激活已显示主窗口，数据库检查或登录阶段可忽略激活请求 |

### 代码落地

| 模块 | 调整 |
|------|------|
| MyConfigCenter | 新增配置中心 DLL 骨架，读取/生成 `config/runtime_config.json`，提供 `GetBool` / `GetInt` / `GetString` |
| MyPermission | 新增权限 DLL 骨架，读取/生成 `config/permission_rules.json`，提供 `IsFeatureVisible()` / `IsFeatureEnabled()` |
| MyUIComponents | 新增共享 UI DLL 骨架，提供缩放系数、等待页、页面标题、主按钮工厂 |
| MyVersionManager | 新增版本清单 DLL 骨架；历史初版扫描应用目录 EXE/DLL，2026-06-25 已改为读取 `config/version_modules.json` |
| MyHomeUI | 新增 `SetEntryVisible()`，MainExe/Permission 可控制首页入口显隐，当前用于“设置”入口 |
| MyCaseUI | 新增 `SetActionVisible()`，MainExe/Permission 可控制浏览模块操作显隐，当前用于“返回首页”按钮 |
| MyMainExe | 接入 ConfigCenter、Permission、UIComponents、VersionManager；启动等待页、单实例、版本清单、配置驱动数据库类型、权限显隐均已接入；页面改为按需创建并释放非当前 widget |

### 资源生命周期说明

MainExe 当前只释放首页、浏览页、等待页的 widget，不在普通切换时调用 HomeUI/CaseUI 的 `Shutdown()`，原因是当前 UI 模块仍借用进程级 Logger/Database 单例，贸然 Shutdown 会影响 MainExe 和其他模块。后续应把 UI 模块的 Logger/Database 生命周期进一步改成“只借用、不关闭全局基础设施”，正式业务数据访问也迁到 Service/Workflow 后，再细化每个 UI 模块的完整释放协议。

### 验证结果

- `MeyerScan_ConfigCenter.sln` Release x64 构建通过。
- `MeyerScan_Permission.sln` Release x64 构建通过。
- `MeyerScan_UIComponents.sln` Release x64 构建通过。
- `MeyerScan_VersionManager.sln` Release x64 构建通过。
- `MeyerScan_HomeUI.sln` Release x64 构建通过。
- `MeyerScan_CaseUI.sln` Release x64 构建通过。
- `MeyerScan_MainExe.sln` Release x64 构建通过，仅保留既有外部登录头文件编码/声明警告。
- `MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke` 均返回 0。

### 已同步文档

- `MeyerScan重构任务总览.md`：补充多分辨率、多语言、运行路径、共享 UI、配置/权限、版本清单、等待页、单实例、资源释放和建单/扫描统一壳规则。
- `MeyerScan架构设计与接口规范.md`：补充禁止项、模块清单、UI 全局适配、运行路径、版本清单、启动准备和近期设计决策。
- `MeyerScan重构开发进度跟踪.md`：更新 ConfigCenter、Permission、UIComponents、VersionManager、MainExe、HomeUI、CaseUI 状态和任务变更记录。
- `MyConfigCenter`、`MyPermission`、`MyUIComponents`、`MyVersionManager`、`MyMainExe`、`MyHomeUI`、`MyCaseUI` 模块内 README/CHANGELOG 已记录本次变更。

### 复查补漏（同日追加）

按用户要求对“刚刚提的点”再次逐项检查后，追加修正如下：

| 模块 | 修正 |
|------|------|
| MyMainExe | 删除数据库配置开发路径回退，只读取运行目录 `config/db_config.json`；登录许可运行参数改为应用目录同级 `license.lic`；页面 widget 释放后再次进入不重复初始化 HomeUI/CaseUI；首页“设置”和浏览“返回首页”显隐改为 ConfigCenter 与 Permission 同时允许才显示 |
| MyHomeUI / MyCaseUI | 测试宿主不再硬编码 `F:/MeyerScan/...`，改为按 exe 所在目录推导日志目录和 MyDatabase 配置路径；模块 `Shutdown()` 不再关闭进程级 Logger/Database 单例，只 Flush 日志并释放自身引用 |
| MyCaseUI | 返回按钮源文案改为英文 `Back Home`，中文显示后续由模块 qm 提供，避免源码文案混用语言 |
| MyLogin | 登录测试宿主运行参数中的离线许可路径改为程序同级 `license.lic`，由 PostBuild 复制，避免运行时依赖开发目录 |
| MyDatabase | DatabaseTest 的日志、备份目标、配置路径改为按测试程序所在目录推导；`DbConfig` 增加 `mysqlDataDir`，MySQL 备份源目录从 `config/db_config.json` 的 `mysql.dataDir` 读取 |
| MyUIComponents | 等待页进度条高度按 `ScaleY()` 辅助缩放，避免高 DPI 下过细 |

本次复查后仍保留的已知事项：vcxproj/PostBuild 中仍会引用 `D:\wj\My-wj\MyLogin` 和 `C:\Program Files (x86)\MeyerScan` 作为既有模块和依赖的开发来源；这是构建期依赖来源，不是运行时资源路径。MyDatabase 的 MySQL 账号密码仍是已记录 TODO，后续随 ConfigCenter/Crypto 接入处理。

### 复查补漏（第三轮）

继续按“路径、生命周期、配置/权限、共享 UI、测试可运行”逐项复查后，追加修正如下：

| 模块 | 修正 |
|------|------|
| MyDatabase | `db_config.json` 改为相对路径；Database 以 `db_config.json` 所在目录解析 `mysql.dataDir` 和 `sqlitePath`，不回退开发机绝对路径或当前工作目录 |
| MyDatabase | DatabaseTest 补齐 Qt5Core/Qt5Sql、SQL 驱动和 `libmysql.dll` 复制规则；测试宿主在 `bin/Release/config` 生成独立测试配置，避免测试配置和正式发布配置混用 |
| MyHomeUI / MyCaseUI | 初始化时优先检查进程级 Database 是否已连接；MainExe 已完成数据库健康检查时，UI 模块只借用现有连接，不重复 Init/Connect |
| MyUIComponents | 增加次按钮、输入框、下拉框基础控件工厂；控件只统一尺寸、样式和多语言适配基础，不承载点击行为、配置读取或权限判断 |
| 文档 | 同步更新四个总文档和 MyDatabase/MyHomeUI/MyCaseUI/MyUIComponents 模块 README/CHANGELOG |

验证结果：

- `MeyerScan_Database.sln` Release x64 构建通过，0 warning / 0 error。
- `DatabaseTest.exe` 返回 0，当前 21 passed / 0 failed，备份用例已实际执行成功。
- `MeyerScan_UIComponents.sln`、`MeyerScan_HomeUI.sln`、`MeyerScan_CaseUI.sln`、`MeyerScan_MainExe.sln` Release x64 构建通过；MainExe 仍仅保留既有外部登录头文件警告。
- `MeyerLoginTest.exe --smoke`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0。

### 资源释放规则补充（同日）

用户补充要求：后续打开扫描重建模块时，需要把案例管理模块资源释放出来，不能只隐藏案例管理界面。

处理结果：

| 文件 | 调整 |
|------|------|
| `F:\MeyerScan\MyMainExe\src\MainWindow.cpp` | 新增 `PrepareForScanReconstruct()`，用于后续启动 `ScanReconstructStudio.exe` 前先切换到等待页、释放 CaseUI widget，并处理 Qt 延迟删除事件 |
| `F:\MeyerScan\MyMainExe\README.md` / `CHANGELOG.md` | 记录扫描前必须释放 CaseUI 的 MainExe 规则 |
| `F:\MeyerScan\MyCaseUI\README.md` / `CHANGELOG.md` | 记录 CaseUI 必须支持被 MainExe 释放并重建，不保留必须跨页面存活的大缓存 |
| 四个总文档 | 同步补充“从案例管理进入扫描重建前必须释放 CaseUI widget，不能只 hide”的资源生命周期约束 |

后续接入 OrderWorkflowService 和 ScanReconstructStudio 时，所有进入扫描重建的入口都必须先走 MainExe 的扫描前准备流程，再启动或激活扫描重建进程。

验证结果：

- `MeyerScan_MainExe.sln` Release x64 构建通过，仅保留既有外部登录头文件编码/声明警告。
- `MeyerScan.exe --smoke-main` 返回 0。
- `MeyerScan.exe --smoke` 返回 0。

## 2026-06-23 - UI 文案 `tr("English source text")` 规则收口

### 用户要求

用户明确要求：界面显示内容都用 `tr()` 包含，`tr()` 里面统一写英文；即使用户需求说的是“回到首页”按钮，源码也应写英文源文案。

### 规则更新

| 规则 | 说明 |
|------|------|
| UI 可见文案 | 统一写 `tr("English source text")` |
| 中文显示 | 通过模块 `.qm` 翻译文件提供 |
| 禁止事项 | 源码中不写中文 UI source text；不再散用 `QApplication::translate()` |
| 示例 | 需求说“回到首页”，源码写 `tr("Back Home")` |
| UIComponents | 控件工厂只接收调用方 `tr()` 后的文本，不在控件工厂内部写中文 source text |

### 代码调整

| 模块 | 调整 |
|------|------|
| MyHomeUI | `HomeUIImpl` 增加 `Q_DECLARE_TR_FUNCTIONS(HomeUI)`；首页标题、入口、说明、状态标签改为 `tr("English source text")` |
| MyCaseUI | `CaseUIImpl` 增加 `Q_DECLARE_TR_FUNCTIONS(CaseUI)`；返回首页、页签、按钮、搜索占位符、表头改为 `tr("English source text")` |
| MyMainExe | `MainWindow` 固定 `MainExe` 翻译上下文；工具栏、状态栏、等待页标题和状态文案改为 `tr("English source text")` |
| MyUIComponents | 文档明确控件工厂只负责样式和显示，文案由调用方 `tr()` 后传入 |

### 文档调整

- `MeyerScan重构任务总览.md`：补充 UI source text 必须英文、中文由 `.qm` 提供。
- `MeyerScan架构设计与接口规范.md`：补充 `tr("English source text")` 规则、禁止中文 UI source text 和分散 `QApplication::translate()`。
- `MeyerScan重构开发进度跟踪.md`：更新 HomeUI/CaseUI 国际化状态和任务变更记录。
- 模块 README/CHANGELOG：同步记录 HomeUI、CaseUI、MainExe、UIComponents 的文案规则。

### 验证结果

- 代码扫描：`MyHomeUI`、`MyCaseUI`、`MyUIComponents`、`MyMainExe` 的 `.h/.cpp` 中未发现 `QApplication::translate()`，未发现中文 UI source text。
- `MeyerScan_HomeUI.sln` Release x64 构建通过，0 warning / 0 error；`HomeUITest.exe --smoke` 返回 0。
- `MeyerScan_CaseUI.sln` Release x64 构建通过，0 warning / 0 error；`CaseUITest.exe --smoke` 返回 0。
- `MeyerScan_UIComponents.sln` Release x64 构建通过，0 warning / 0 error。
- `MeyerScan_MainExe.sln` Release x64 构建通过；仅保留既有登录头文件编码/声明警告；`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0。

## 2026-06-23 - 全模块交互复测与 MainExe smoke 增强

### 复查结论

本轮按模块交互链路复查：MainExe 仍只做启动、基础设施编排和页面容器；HomeUI/CaseUI 只上报入口或操作 ID；ConfigCenter 与 Permission 只由 MainExe 合并后下发最终显隐结果；Logger、Database 由 MainExe 统一管理生命周期，UI 模块只借用。

### 代码调整

| 文件 | 调整 |
|------|------|
| `F:\MeyerScan\MyMainExe\src\MainWindow.cpp` | CaseUI `Open` 操作接入 `PrepareForScanReconstruct()`，先切等待页、释放 CaseUI widget、处理延迟删除事件，后续再接入扫描重建进程 |
| `F:\MeyerScan\MyMainExe\src\MainWindow.cpp` | `--smoke-main` 自动覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前资源释放链路 |
| `F:\MeyerScan\MyMainExe\test\main.cpp` | 单实例激活改为登录完成且主窗口可见后才响应，数据库检查或登录阶段不抢占前台 |

### 验证结果

- `MeyerScan_MainExe.sln` Release x64 构建通过，0 error，仅保留既有外部登录头文件编码/声明警告。
- `MeyerScan.exe --smoke-main` 返回 0；最新日志包含 `PageCreate`、`PageSwitch`、`PageRelease`、`PrepareScanReconstruct`、`PrepareScanReconstructDone`。
- `MeyerScan.exe --smoke` 返回 0；登录模块仍可被正式启动链路拉起。
- 启动后生成 `logs/versionList/versionList_yyyyMMdd_HHmmss.json`，版本清单包含 MainExe、HomeUI、CaseUI、ConfigCenter、Permission、UIComponents、VersionManager、Database、Logger、LoginWidget 和 Qt 运行库。

## 2026-06-23 - 拆分模块总清单补充

### 用户要求

用户要求在重构文档合适位置增加拆分模块清单，清单内必须包含模块中文名、项目名、DLL/EXE 名和各模块功能详情。

### 文档调整

| 文档 | 调整 |
|------|------|
| `MeyerScan架构设计与接口规范.md` | 将 `2.4.1 整体模块清单` 扩展为正式 `拆分模块总清单（中文名 / 项目名 / 产物名 / 功能详情）`，表格字段包含模块中文名、项目名/目录、DLL/EXE/LIB 名、形态、功能详情、边界与注意事项 |
| `MeyerScan重构任务总览.md` | 在最终架构原则中增加模块总清单维护规则，要求新增、删除、合并、改名模块时同步更新正式清单、进度文档和模块内 README/CHANGELOG |
| `MeyerScan重构开发进度跟踪.md` | 在变更记录中记录本次清单补充 |

### 设计说明

清单同时覆盖已落地模块和规划模块。已落地模块按当前 `F:\MeyerScan` 一级目录记录，规划模块按建议项目名记录，后续创建项目时优先沿用清单命名，避免项目名、产物名和功能边界在多人维护时漂移。

### 追加同步

用户进一步要求：拆分模块总清单在 `MeyerScan重构任务总览.md` 中也需要有完整内容，不能只放索引。

处理结果：

- `MeyerScan重构任务总览.md` 新增 `3.6 拆分模块总清单（中文名 / 项目名 / 产物名 / 功能详情）`，内容与架构规范清单保持一致。
- 任务总览中的清单维护规则更新为：总览 `3.6` 和架构规范 `2.4.1` 两处都属于正式清单位置，后续必须同步维护。

## 2026-06-24 - Qt 默认使用规则与病例订单服务口径收敛

### 用户要求

用户明确要求：能用 Qt 默认能力的地方尽量用 Qt，不要刻意规避；`CaseOrderService.dll`、`Calibration3DUI.dll`、`CalibrationColorUI.dll`、`OrderScanWorkspaceShell.dll` 都可以使用 Qt。

### 规则更新

| 规则 | 说明 |
|------|------|
| Qt 默认使用 | MeyerScan 重构基于 Qt 5.6.3，业务/UI 模块可优先使用 Qt Core、Qt JSON、Qt SQL、Qt Widgets、信号槽和布局系统 |
| 边界重点 | 架构控制的是模块职责、对象所有权、跨进程边界和长期 ABI 稳定性，不是去 Qt 化 |
| 可用模块 | CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、Database、各 UI 模块都可以用 Qt |
| 需收敛场景 | 跨进程 IPC、第三方 C ABI、长期兼容插件接口、不确定调用方运行库的边界，仍优先使用 UTF-8/POD/调用方缓冲区 |

### 同步调整

- `MeyerScan架构设计与接口规范.md`：补充 Qt 默认使用原则；调整跨 DLL/跨进程传递表述；明确同一进程同一 Qt/VS2015 环境内可使用 `QString/QJsonObject/QVariantMap`。
- `MeyerScan重构任务总览.md`：补充 Qt 默认使用原则和 CaseOrderService/Database 可用 Qt 的规则。
- `MeyerScan重构开发进度跟踪.md`：将校准模块改为 `Calibration3DUI.dll` 与 `CalibrationColorUI.dll` 两个 Qt UI 模块；新增 `OrderScanWorkspaceShell` 骨架进度。
- `MyCaseOrderService` / `MyOrderScanWorkspaceShell` / `MyDatabase` 模块 README 和 CHANGELOG：补充“不刻意规避 Qt”的模块规则。

### 代码状态

- `MyCaseOrderService` 当前内部已使用 `QString`、`QJsonDocument/QJsonObject`、`QDateTime` 等 Qt 能力处理 JSON、字段和日志内容，符合新规则。
- `MyOrderScanWorkspaceShell` 当前使用 Qt Widgets、`QStackedWidget`、布局、`QString` 和 `QMap`，符合新规则。
- 对外 `const char*` / 调用方缓冲区接口保留，原因是当前作为稳定插件 ABI 和未来跨进程/第三方边界的安全默认，不代表模块内部要规避 Qt。

### 追加落地

- 新增 `F:\MeyerScan\MyCalibration3DUI`，输出 `MeyerScan_Calibration3DUI.dll`，当前为 Qt Widgets 最小骨架，包含 `ICalibration3DUI`、创建页面、日志、版本资源、README 和 CHANGELOG。
- 新增 `F:\MeyerScan\MyCalibrationColorUI`，输出 `MeyerScan_CalibrationColorUI.dll`，当前为 Qt Widgets 最小骨架，包含 `ICalibrationColorUI`、创建页面、日志、版本资源、README 和 CHANGELOG。
- 两个校准模块均遵守：UI 文案使用 `tr("English source text")`，日志路径由调用方基于安装目录传入，不使用当前工作目录；真实标定器连接、采集编排、算法 DLL 和设备 DLL 后续在对应模块内部接入。
- `MyOrderScanWorkspaceShell` 替换步骤页面时释放旧页面，避免后续挂载真实建单/扫描页面时残留占位资源。

### 验证结果

| 验证项 | 结果 |
|--------|------|
| `MeyerScan_Database.sln` Release x64 | 通过，0 warning / 0 error |
| `DatabaseTest.exe` | 23 passed / 0 failed |
| `MeyerScan_CaseOrderService.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_OrderScanWorkspaceShell.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_Calibration3DUI.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_CalibrationColorUI.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_Logger.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_ConfigCenter.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_Permission.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_UIComponents.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_HomeUI.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_CaseUI.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_VersionManager.sln` Release x64 | 通过，0 warning / 0 error |
| `MeyerScan_MainExe.sln` Release x64 | 通过，3 warnings / 0 errors；警告来自既有登录头文件 `MeyerLoginWidget.h` / `globalLoginValue.h` 的代码页和 typedef 声明问题 |

## 2026-06-24 - MainExe 启动链路、配置边界和注释规范收口

### 用户要求

用户继续提出 10 点收口要求：登录窗口显示后等待页没有关闭；代码注释覆盖率不足；`MeyerScan.exe` 启动有 CMD 弹窗；`MyVersionManager` 可集成进 MainExe；VS2015 加载项目报错；Logger 应一开始初始化；日志接口应缓存模块级变量；`showWaitPage` / `singleInstance` 不应配置化；需要解释 `permission_rules.json` 与 `runtime_config.json` 关系；回顾文档和代码边界。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyMainExe | `MainWindow` 构造阶段提前调用 `InitLoggerEarly()`，缓存 `ILogger* m_logger`；登录窗口显示前释放等待页并隐藏 MainWindow；Release 工程改为 Windows 子系统并指定 `mainCRTStartup`，隐藏 CMD；版本清单生成逻辑并入 MainExe，输出仍为 `logs/versionList/versionList_yyyyMMdd_HHmmss.json`；移除 MainExe 对 `MeyerScan_VersionManager.dll` 的依赖 |
| MyConfigCenter | 默认配置不再写 `startup.showWaitPage` / `startup.singleInstance`；初始化时若发现旧配置残留 `startup` 段，会自动迁移清理并写回；README 补充 `runtime_config.json` 与 `permission_rules.json` 关系 |
| MyPermission | README 补充 `visible` / `enabled` 字段含义：`visible` 控制是否显示入口，`enabled` 控制是否允许执行；说明 ConfigCenter 给默认策略，Permission 给授权结果，MainExe 合并后下发 UI |
| MyVersionManager | README 和 CHANGELOG 标注当前阶段为历史骨架，后续版本管理复杂化时再恢复独立 DLL |
| HomeUI / CaseUI / UIComponents / CaseOrderService | 补充函数级中文注释和关键边界注释，强调 UI 只上报入口/动作 ID，服务层不做 UI/扫描/流程决策，共享 UI 不承载业务行为 |
| VS2015 工程 | 批量移除 `LanguageStandard` 标签，避免 VS2015/v140 项目系统打开项目时报错；本轮修改过的 C++ 源码/头文件保存为 UTF-8 with BOM |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：补充强制注释规则，每个函数必须有中文注释，关键代码和测试项目同样必须注释；更新运行时版本清单当前由 MainExe 内置生成；明确等待页和单实例是固定流程，不写入 `runtime_config.json`；补充配置和权限共同控制 UI 显隐的关系。
- `MeyerScan重构任务总览.md`：同步模块清单中的版本清单能力口径；在开发约束中补充函数级中文注释、关键代码注释、测试代码注释和 UTF-8 BOM 规则。
- `MeyerScan重构开发进度跟踪.md`：更新版本清单能力状态、当前任务描述和 2026-06-24 变更记录。
- 模块 README/CHANGELOG：同步记录 MainExe、ConfigCenter、Permission、VersionManager、HomeUI、CaseUI、UIComponents、CaseOrderService 的本轮收口。

### 验证结果

- `MeyerScan_MainExe.sln` Release x64 构建通过，0 error；保留 3 个既有外部登录头文件警告。
- `MeyerScan.exe --smoke` 返回 0。
- `MeyerScan.exe --smoke-main` 返回 0。
- 启动后生成 `logs/versionList/versionList_yyyyMMdd_HHmmss.json`。

### 当前结论

`runtime_config.json` 是产品/客户默认策略，`permission_rules.json` 是授权结果。UI 显隐由 MainExe 合并两者后下发，安全边界仍要靠 Workflow、Service、IPC 接收端继续复核。等待页和单实例属于 MainExe 固定启动流程，不再允许配置关闭。当前版本清单能力先内置在 MainExe，减少运行时 DLL；后续复杂化再恢复独立 VersionManager。

## 2026-06-24 - 全模块注释覆盖复查

### 用户要求

用户明确要求：每个模块都要增加详细中文注释，每个函数都要有注释，关键代码也要有注释，测试项目同样适用，默认代码阅读者按初学者考虑。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyLogin | 为 `LoginHost` 头文件、实现文件和 `MeyerLoginTest` 入口补充中文注释，说明登录 DLL 参数组装、信号返回、离线许可路径和 `--smoke` 自动退出用途 |
| MyCalibration3DUI / MyCalibrationColorUI | 补充公共接口、实现类、界面创建、日志、Shutdown 生命周期注释，明确 MainExe 只嵌入 QWidget，不参与校准内部计算和设备接入 |
| MyOrderScanWorkspaceShell | 补充工作区壳子公共接口、步骤枚举、页面挂载、步骤切换、旧页面释放和日志注释，明确壳子只管理容器，不实现建单/扫描/发送业务 |
| MyHomeUI / MyCaseUI | 补充公共接口和测试宿主注释，说明入口/动作 ID、显隐控制、路径推导、冒烟测试和 UI 不直接做业务 SQL 的边界 |
| MyUIComponents | 补充共享 UI 公共接口注释，说明控件工厂、等待页、辅助缩放系数和不承载业务行为的边界 |
| MyCaseOrderService | 补充公共接口注释，说明患者/订单组合 JSON、参考数据、稳定查询入口和调用方缓冲区返回约束 |
| MyDatabase | 补充 `Result<T>`、`VoidResult`、错误码辅助函数和 `DatabaseTest` 路径辅助函数注释，测试项目与公共结构同样按函数级注释处理 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：在强制注释规则中补充第三方源码例外和本轮落地范围。
- `MeyerScan重构任务总览.md`：补充注释覆盖落地要求，强调公共接口、实现、测试宿主、smoke 入口同步补注释。
- `MeyerScan重构开发进度跟踪.md`：新增全模块注释覆盖复查记录。
- 模块 `CHANGELOG.md`：同步记录 MyLogin、MyCalibration3DUI、MyCalibrationColorUI、MyOrderScanWorkspaceShell、MyHomeUI、MyCaseUI、MyUIComponents、MyCaseOrderService、MyDatabase 的本轮注释补充。

### 范围说明

本轮注释规则覆盖自研源码和测试宿主。`MySQL/include`、`MyCaseManager/MySQL/include`、外部登录模块头文件等第三方或既有外部依赖不强制改中文注释，避免破坏后续升级和对照原厂 SDK。

## 2026-06-24 - 函数体内部注释补强

### 用户要求

用户指出上一轮主要补了函数级注释，但函数内部关键代码注释仍然不足。要求把代码阅读者当作初学者，不只解释函数用途，也要在函数体内部解释关键判断、路径推导、资源生命周期、失败分支和跨模块边界。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyHomeUI | 在初始化、日志加载、数据库借用、入口显隐、页面创建、按钮回调、Shutdown 和测试宿主路径推导中补充函数体内部注释，强调首页只上报入口 ID，不直接承载建单/扫描/权限核心判断 |
| MyCaseUI | 在日志/数据库借用、返回首页按钮显隐、Tab 切换、患者/订单工具栏、搜索触发、表格占位和测试宿主中补充内部注释，强调 UI 不直接拼业务 SQL |
| MyCaseOrderService | 在固定缓冲区复制、初始化、schema 创建、JSON 保存/读取、参考数据白名单、QueryJson 分发、SQL 转义和 CopyToBuffer 中补充内部注释，说明骨架期约束和后续 DAO/migration 方向 |
| MyDatabase | 在配置路径解析、Qt SQL 连接生命周期、查询 JSON 转换、备份、事务、数据库类型切换、日志动态加载和 DatabaseTest 备份/路径函数中补充内部注释 |
| MyMainExe | 在启动流程、等待页隐藏、登录参数、基础设施初始化、配置/权限合并、页面创建/切换/释放、扫描前释放 CaseUI、版本清单和单实例测试入口中补充内部注释 |
| MyLogger / MyOrderScanWorkspaceShell | 补充日志轮转、LogWriter 关闭、工作区步骤标题映射等短函数的内部注释，避免只靠函数头说明 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：强制注释规则新增“函数体内部也必须注释”，明确不能只写函数头；关键判断、路径、资源、Qt 所有权、跨模块调用、失败分支、线程/事务边界都要在代码附近说明。
- `MeyerScan重构任务总览.md`：开发约束新增“函数体内部注释强制规则”。
- `MeyerScan重构开发进度跟踪.md`：新增函数体内部注释补强记录。
- 模块 `CHANGELOG.md`：补充本轮函数体内部注释补强记录。

### 检查方式

对 `F:\MeyerScan` 下自研 `.h/.cpp` 执行启发式扫描，排除第三方 MySQL SDK 和生成目录。检查结果：未发现“大函数体完全无内部注释”的候选；进一步检查 8 行以上函数体，未发现内部注释少于 2 条的候选。该扫描不能替代人工代码审查，但可作为当前注释覆盖的底线检查。

## 2026-06-25 - glm52 建议复核与采纳记录

### 用户要求

用户要求读取 `D:\wj\重构文档\glm52` 下的两份建议文档，判断哪些建议适合当前重构方案采纳，哪些应按方向自行优化；随后同步更新全局 MD 文档、模块代码和模块内部文档。

### 采纳判断

| 建议类型 | 本轮处理 | 原因 |
|----------|----------|------|
| `Version.rc` 公司名、产品名、文件描述和命名常量统一 | 立即采纳并修改代码 | 纯工程配置变更，不影响 ABI 和运行逻辑，但能提升文件属性、版本清单和现场排查一致性 |
| 缺失 `MEYER_MODULE_NAME` | 立即采纳并修改工程 | Logger 便捷宏依赖该宏输出模块名，缺失会导致日志模块名退化为 `Unknown` |
| `CaseOrderServiceResult` 增加 `IsSuccess()` / `IsError()` | 立即采纳 | 不改变结构体布局的字段含义和现有调用方式，能统一调用风格，后续迁移 Core.lib 更顺 |
| ConfigCenter / Permission 从 `bool` 升级到 `ErrorCode` / `VoidResult` | 本轮只写 TODO，不改签名 | 会影响 MainExe 和调用方接口，且 Core.lib 尚未落地，当前强改会增加联调风险 |
| CaseOrderService 迁移到统一 `Result<T>` | 本轮只记录方向 | 依赖 Core.lib 的 `Result/VoidResult`，应与 Core.lib 一起集中迁移 |
| Logger 导出宏从 `MEYER_LOGGER_API` 改为 `MEYERSCAN_LOGGER_API` | 暂缓到主版本或集中 ABI 收口 | 改宏本身不复杂，但会牵动导出定义、工程宏和调用方包含习惯；当前 Logger 刚按用户反馈重构，优先保持稳定 |
| HomeUI / CaseUI 移除 Database 直连 | 暂缓 | 当前只用于框架期健康检查；正式移除要等 CaseOrderService/Workflow 提供健康检查和业务查询入口 |
| MainExe 登录依赖硬编码路径改统一 deps 目录 | 暂缓记录 | 当前属于开发期 PostBuild 依赖来源，运行时不回退开发路径；正式打包模块设计时统一收口 |
| 创建 Core.lib | 作为后续前置任务记录 | 方向正确，但本轮目标是建议筛选和低风险一致性修复，不展开新的公共库项目 |

### 代码调整

| 模块 | 调整 |
|------|------|
| HomeUI / CaseUI / ConfigCenter / Permission / UIComponents / CaseOrderService / Calibration3DUI / CalibrationColorUI / OrderScanWorkspaceShell / VersionManager / MainExe / LoginTest | 统一 `Version.rc`：公司名固定为 `Hefei Meyer Optoelectronic Technology Co., Ltd.`，产品名固定为 `MeyerScan Digital Dental Scanner`，文件描述使用模块化英文描述，`FILEOS/FILETYPE/FILEFLAGS` 使用 Windows SDK 命名常量 |
| ConfigCenter / Permission / UIComponents / CaseOrderService / Calibration3DUI / CalibrationColorUI / OrderScanWorkspaceShell / VersionManager / MainExe / LoginTest / Logger | 补齐 `MEYER_MODULE_NAME` 预处理器定义，保证日志宏能输出正确模块名 |
| CaseOrderService | `CaseOrderServiceResult` 增加 `IsSuccess()` 和 `IsError()` 辅助方法，调用方后续不需要反复手写 `errorCode == 0` |
| ConfigCenter / Permission | 在 `Init()` 接口注释中补充 Core.lib 后迁移到 `ErrorCode/VoidResult` 的 TODO，明确当前 `bool` 是骨架期过渡 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：新增工程配置命名与 `MEYER_MODULE_NAME` 规则；增强 `Version.rc` 规范；在近期设计决策中记录 glm52 建议采纳原则、骨架接口迁移规则和新模块工程验收清单。
- `MeyerScan重构任务总览.md`：构建约束新增版本资源字段统一、模块名宏必填；碎片化开发规则新增“新模块工程验收清单”。
- `MeyerScan重构开发进度跟踪.md`：通用约束和阶段检查项新增工程一致性检查；任务变更记录新增本轮 glm52 建议复核。
- 各修改模块 `CHANGELOG.md`：按中文记录本轮版本资源、模块名宏和接口辅助方法/TODO 调整。

### 当前结论

glm52 建议整体方向是有价值的，但执行顺序要服务当前架构稳定性。工程一致性问题应立即修；接口签名、公共 Result、导出宏、UI 去 Database 直连等属于调用链和 ABI 迁移，应等 Core.lib、CaseOrderService、Workflow 等基础能力齐备后集中处理。

## 2026-06-25 - MainExe、权限 enabled、版本清单 manifest 与 VS2015 聚合方案收口

### 用户要求

用户提出 13 点继续优化：版本清单只记录拆分模块 DLL/EXE，并考虑清单文件；VS2015 一次打开所有模块工程；`permission_rules.json` 的 `enabled` 必须生效；首页和浏览不能作为并列 `QStackedWidget` 页面；Qt 模块使用 Logger 的 Qt 接口；日志字段带分类标记；清理不该出现在 Logger Release/config 的数据库配置；`license.lic` 迁入 `Resources`；ConfigCenter/HomeUI VS2015 打开异常需判断；JSON 配置不写注释，另写 md；按钮权限设置归类；解释 MainExe 下发权限状态的设计原因；模块内部维护模块名/版本等信息。

### 2026-06-25 追加纠偏

1. 运行时 `logs/versionList` 与安装包 `version_manifest.json` 是两个不同清单。前者由 MainExe 读取 `config/version_modules.json` 生成，只记录 MeyerScan 拆分模块 EXE/DLL；后者由 MyInstaller/Packaging 在发布打包阶段生成，可覆盖 Qt 运行库、VC/UCRT、SQL 驱动、平台插件等安装依赖。历史记录中如提到运行时 versionList 包含 Qt 运行库，以本条为准。
2. `MyConfigCenter` 新增无注释 `config/runtime_config.json` 默认模板，`MyPermission` 新增无注释 `config/permission_rules.json` 默认模板；字段说明继续写在同级 `.md`，VS2015 PostBuild 复制 JSON 和 md 到 Release `config/`。
3. `MyMainExe` Release PostBuild 同步复制 ConfigCenter/Permission 的 JSON 模板和说明文件，避免运行时只依赖首次启动自动生成默认配置。
4. 登录离线许可的最终运行路径为 `Resources/license.lic`；历史记录中提到同级 `license.lic` 是迁移前过渡状态。
5. MainExe Release PostBuild 已补齐 `MeyerScan_CaseOrderService.dll`、`MeyerScan_OrderScanWorkspaceShell.dll`、`MeyerScan_Calibration3DUI.dll`、`MeyerScan_CalibrationColorUI.dll`，保证 `version_modules.json` 中声明的已开发拆分模块能进入运行目录并被 versionList 记录为 `exists=true`。
6. 复测结果：Logger、Database、ConfigCenter、Permission、UIComponents、HomeUI、CaseUI、CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、VersionManager、MainExe 单模块 Release x64 构建通过；`F:\MeyerScan\MeyerScan_AllModules.sln` Release x64 构建通过；`LoggerTest.exe`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0。聚合全量重新编译时仅见外部登录头文件编码/typedef 既有警告，非本轮工程结构错误。
7. 当时 MainExe 运行时 `versionList_20260625_074259.json` 只包含 `version_modules.json` 中声明的拆分模块，未出现 Qt、OpenSSL、AWS、VC/UCRT、SQL 驱动、libmysql、libcurl、zlib 等第三方库；清单内当时已开发模块均为 `exists=true`。当前规则已扩展为 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库均不进入运行时 `logs/versionList`。

### 代码调整

| 模块 | 调整 |
|------|------|
| MyMainExe | 版本清单改为读取 `config/version_modules.json`，只记录清单中的拆分模块 EXE/DLL；缺失模块也写入 `exists=false`，便于发现安装包漏复制；新增 `config/version_modules.md` 字段说明。 |
| MyMainExe | 页面容器从 `QStackedWidget` 并列页面改为单内容区替换页面：一次只挂载一个全屏页面；首页进入浏览、浏览返回首页、扫描前等待页都按“替换当前页面 + 释放离开页面资源”处理。 |
| MyMainExe | `BuildLoginParameters()` 改为读取 `Resources/license.lic`；PostBuild 同步复制离线许可到 `Resources`。 |
| MyMainExe | 权限设置归类为 `ApplyHomeEntryRules()`、`ApplyCaseActionRules()`、`IsFeatureVisible()`、`IsFeatureEnabled()`、`HomeEntryFeatureId()`、`CaseActionFeatureId()`；MainExe 下发 `visible/enabled`，收到回调后仍按 `enabled` 二次复核。 |
| MyHomeUI | 公共接口新增 `SetEntryEnabled()`；按钮创建时同时应用 `visible` 与 `enabled`；日志模块名统一为 `MeyerScan_HomeUI`；Qt 日志调用改用 Logger 的 `QString` 便捷接口。 |
| MyCaseUI | 公共接口新增 `SetActionEnabled()`；当前先落地“返回首页”按钮启用态；日志模块名统一为 `MeyerScan_CaseUI`；Qt 日志调用改用 Logger 的 `QString` 便捷接口。 |
| MyConfigCenter | 新增 `config/runtime_config.md`，说明 `runtime_config.json` 字段含义和与权限文件关系；JSON 内部不写注释。 |
| MyPermission | 新增 `config/permission_rules.md`，说明 `visible`、`enabled`、featureId 写法和复核要求；`enabled` 已由 MainExe/UI 接入，不再只是预留字段。 |
| MyLogger | README 更新日志格式为 `[Mod:] [Op:] [Content:]`，并说明 Qt 模块 `QString` 重载是正式便捷接口，但跨 DLL ABI 仍为 UTF-8 `const char*`。 |
| MyMainExe/MyLogin | 登录测试宿主也改为 `Resources/license.lic`，与正式 MainExe 规则一致。 |
| F:\MeyerScan | 新增 `MeyerScan_AllModules.sln`，用于 VS2015 一次打开当前已拆分模块工程；外部既有登录 DLL 不纳入聚合方案。 |
| MyMainExe/MyHomeUI/MyCaseUI | 落地 `ModuleInfo` 模式：`Name` 用于日志模块名，`Version` 用于版本清单和 `GetModuleVersion()`；后续模块按同一规则补齐。 |

### 二次复查追加

在继续检查代码后，本轮又补充了三类收口：

| 模块 | 追加处理 |
|------|----------|
| MyLogger | 日志正文分类标记由 `[Txt:]` 调整为 `[Content:]`，固定字段为 `[Mod:] [Op:] [Content:]`，可选字段为 `[Dev:] [Case:] [Opr:]`；路径写入改为 UTF-8 转 UTF-16 后调用 `CreateFileW` / `SHCreateDirectoryExW`，支持中文安装路径。 |
| MyVersionManager | 虽然当前版本清单逻辑已并入 MainExe，但历史骨架也改为读取 `config/version_modules.json`，只记录 MeyerScan 拆分模块，不再扫描运行目录全部 EXE/DLL。 |
| 模块信息 | 将 `ModuleInfo::Name` / `ModuleInfo::Version` 从 MainExe/HomeUI/CaseUI 扩展到 Logger、Database、ConfigCenter、Permission、UIComponents、CaseOrderService、OrderScanWorkspaceShell、Calibration3DUI、CalibrationColorUI、VersionManager 等已开发模块。 |

### 设计结论

1. 版本清单不应扫描运行目录全部 DLL。运行目录里有 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库，当前改为清单驱动，只记录拆分出来的 MeyerScan 模块，新增模块时维护 `config/version_modules.json`。
2. 首页和浏览不是并列页面。浏览入口在首页内，所以 MainExe 采用单内容区替换当前全屏页面，而不是把 Home/Case 放在同一个 `QStackedWidget` 中长期作为兄弟页面。
3. `enabled` 必须在两层生效：UI 层设置按钮禁用态，MainExe 收到回调后继续二次复核。后续 Workflow、Service、IPC 接收端仍要复核，不能只靠 UI。
4. HomeUI/CaseUI 不直接读取 Permission/Config 是有意设计：MainExe 负责合并产品配置和授权结果，UI 模块只做渲染和上报，避免多个 UI 重复读文件、重复合并规则、产生循环依赖。UI 自己读取技术上可行，但会让权限口径分散。
5. VS2015 报图中 Visual Assist `WholeTomatoSoftware.VisualAssist.VaManagedPkg` 加载失败属于 VS 插件问题，不是项目文件本身错误；但本轮仍新增聚合 sln 降低多模块打开成本。
6. 版本信息以 `Version.rc` 为文件属性权威源，代码中的 `ModuleInfo::Version` / `GetModuleVersion()` 必须同步，不允许保留不一致版本。

### 文档同步

- `MeyerScan重构任务总览.md`：更新单内容区切换、版本清单 manifest、配置说明 md、`enabled` 生效、Resources 目录、VS2015 聚合方案和模块信息规则。
- `MeyerScan架构设计与接口规范.md`：更新解耦约束、运行路径与启动准备规则、设计决策表和页面切换时序描述。
- `MeyerScan重构开发进度跟踪.md`：更新 MainExe 当前能力、运行资源目录、模块信息一致性和 2026-06-25 变更记录。
- 各模块 `README.md` / `CHANGELOG.md`：同步记录 MainExe、HomeUI、CaseUI、ConfigCenter、Permission、Logger、MyLogin 的本轮改动。

### 2026-06-25 文档一致性复查补充

用户追问今天方案是否已经同步到对应 md，避免后续开发继续使用旧方案。复查结论：

1. `MeyerScan重构任务总览.md`、`MeyerScan架构设计与接口规范.md`、`MeyerScan重构开发进度跟踪.md`、模块 README/CHANGELOG 中已记录最终口径：MainExe 单内容区全屏替换、`version_modules.json` 清单驱动、`permission_rules.json` 的 `enabled` 生效、`Resources/license.lic`、JSON 字段说明写同级 md、模块信息一致性和 VS2015 聚合解决方案。
2. 发现 `MeyerScan重构开发进度跟踪.md` 中 2026-06-22 历史记录仍写“MainExe 统一用 `QStackedWidget` 切换页面”。该记录已改为“历史旧实现”，并明确 2026-06-25 起该方案废弃，后续必须以“MainExe 单内容区全屏替换 + 离开页面按资源规则释放”为准。
3. 用户反馈 VS2015 打开 `F:\MeyerScan\MeyerScan_AllModules.sln` 弹出 `WholeTomatoSoftware.VisualAssist.VaManagedPkg` 错误。复查 `.sln/.vcxproj` 未引用 Visual Assist；MSBuild `ValidateSolutionConfiguration` 成功，判断为 Visual Assist 插件加载问题，不是 MeyerScan 聚合工程文件错误。
4. 当前工作区额外发现 `F:\MeyerScan\MyLogger\src\LogFormat.cpp` 有未提交改动，把最终文档要求的 `[Content:]` 日志正文标记改回了 `[Txt:]`。该改动与当前文档口径不一致，后续提交前必须确认：若继续执行今天最终方案，应恢复为 `[Content:]`；若用户另行确认改回 `[Txt:]`，则需要同步修改所有日志规范文档和测试断言。

### 2026-06-26 继续复查补充

1. 已按当前最终方案修复 `MyLogger/src/LogFormat.cpp`，Logger 正式输出恢复为 `[Content:]`；`[Txt:]` 仅保留在历史记录和测试反向断言中。
2. `MyLogger/include/Logger.h` 示例同步调整：模块名使用 `MeyerScan_CaseOrderService` 风格，无真实操作员上下文时传空字符串，不再示例传 `"System"`。
3. `MeyerScan重构任务总览.md` 和 `MeyerScan架构设计与接口规范.md` 的当前流程说明补齐 `SettingsUI.dll`：首页/浏览打开设置走 SettingsUI，工程设置保留为后续高级维护入口。

## 2026-07-01 - 全模块实现技巧型注释复查

### 用户要求

用户要求再次对所有模块（包括测试程序）补充代码注释。重点不只是解释函数/代码“做什么”，还要解释“代码是如何实现的”，例如 Qt、C++、Windows API、JSON、跨 DLL 调用、缓冲区、事件循环等实现技巧；函数体内部尽可能逐行或按关键代码块补注释，把代码阅读者按初学维护者考虑。

### 代码调整

| 模块 | 本轮补充重点 |
|------|--------------|
| ConfigCenter | 补充 `QString::fromUtf8`、`QDir::mkpath/filePath`、`QJsonDocument::fromJson`、点号 key 解析、调用方缓冲区、默认配置生成和旧 `startup` 段迁移写回的实现技巧说明。 |
| Permission | 补充授权文件路径、JSON 严格 bool 判断、`visible` / `enabled` 区别、默认全开放规则和不覆盖已有授权文件的原因。 |
| UIComponents | 补充 Qt Layout、多分辨率辅助缩放、`QSizePolicy`、QSS 统一样式、按钮角色/内容布局、等待页无限进度条、图标加载和多语言文本变长适配说明。 |
| CaseOrderService | 补充 C ABI 固定结构体返回、`QByteArray::constData()` 生命周期、JSON compact 存储、调用方缓冲区、静态数组脚本计数、SQL 白名单和骨架期单引号转义限制说明。 |
| OrderScanWorkspaceShell | 补充 `QStackedWidget` 页面切换、步骤页面挂载、旧页面 `deleteLater()`、Qt 父子对象和弱引用清空说明。 |
| Calibration3DUI / CalibrationColorUI | 补充根页面父子关系、布局容器、占位区、Start/Cancel 占位按钮、多语言 `tr()`、日志 UTF-8 ABI 和未来算法/设备资源释放边界说明。 |
| VersionManager | 补充 manifest 驱动版本清单、为什么不扫描第三方库、`QFileInfo`、时间戳输出、Windows 版本资源 API、`HIWORD/LOWORD` 版本号拆分和成员 `QByteArray` 返回路径生命周期说明。 |
| HomeUI / MyLogin | 补充首页入口回调 C ABI 设计、`QLibrary` 动态加载、UIComponents 可选降级、按钮 lambda 捕获、登录模块信号槽、登录参数 `QString` 不能 `memset`、`QTimer::singleShot(0)` 延迟退出原因。 |
| CaseUITest / SettingsUITest / RuntimeDataCenterTest | 补充测试宿主造最小演示数据的边界、正式 UI 不建表/插表、`findChildren<QTableWidget*>` 冒烟检查、路径从 `applicationDirPath()` 推导、事件循环等待后再检查 UI 数据的原因。 |
| DatabaseTest / LoggerTest | 补充 VS2015 不用 `std::filesystem` 的路径字符串处理、`GetModuleFileNameA`、`CreateDirectoryA`、测试日志兜底、`std::thread::join`、`std::rename` 验证文件句柄关闭等实现技巧。 |

### 文档同步

- `MeyerScan架构设计与接口规范.md`：强制注释规则新增“必须解释代码实现技巧”和“接近逐行解释但避免噪声”，并把注释内容深度写入近期设计决策；文档版本更新到 v1.17。
- `MeyerScan重构任务总览.md`：开发约束补充实现技巧型注释和接近逐行解释要求；文档版本更新到 v3.15。
- `MeyerScan重构开发进度跟踪.md`：新增 2026-07-01 全模块实现技巧型注释复查记录。
- 各模块 `CHANGELOG.md`：本轮涉及模块补充“实现技巧型注释复查”记录，强调本轮只补注释和文档，不改变业务逻辑。

### 当前约束

1. 注释不是越多越好，而是要让维护者理解代码机制；简单赋值、直接 return 不机械堆注释，但复杂函数内部要覆盖关键语句和连续代码块。
2. 测试宿主必须解释为什么可以造数据、为什么正式 UI 不能造数据，以及 smoke 如何判断链路通过。
3. 第三方 SDK、外部登录模块头文件仍不强制改注释，自研适配层和调用层必须解释第三方调用方式和生命周期。

## 2026-07-02 - 实现技巧型注释二次补强

### 用户要求

用户要求继续未完成任务，对所有模块（包括测试程序）再做一轮代码注释，函数体内部每行代码也尽可能有注释；除说明功能之外，还要说明这些功能通过什么技术实现，特别是维护者能理解 Qt、C++、跨 DLL、JSON、缓冲区、事件循环和测试宿主造数等“代码实现技巧”。

### 本轮处理

- 复查 `F:\MeyerScan` 自研 `.h/.cpp` 注释密度和连续无注释代码块，优先处理低注释密度且属于核心链路的文件。
- 补强 `MySettingsUI/src/SettingsUIImpl.cpp`：说明 QSS/objectName 精确选择器、Qt Layout 伸缩、设置模块内部 `QStackedWidget` 分类页、RuntimeDataCenter 动态加载、校准 DLL 懒加载、校准 wrapper、`deleteLater()`、`QTableWidgetItem` 所有权和多语言/多分辨率布局机制。
- 补强 `MyRuntimeDataCenter/src/RuntimeDataCenterImpl.cpp`：说明 domain 独立刷新、云端空快照、旧表候选表、JSON envelope、调用方缓冲区、有限扩容重试、订单摘要字段裁剪和快照一次性替换机制。
- 补强 `MyMainExe/src/MainWindow.cpp`：说明页面创建失败降级、UI 模块初始化边界、单内容区页面释放、`deleteLater()` 延迟析构、Layout stretch、配置/权限 visible/enabled 合并、版本 manifest 顺序、Windows 文件版本资源和 Logger 早期初始化。
- 补强 `MySettingsUI/test/main.cpp`、`MyRuntimeDataCenter/test/main.cpp`、`MyCaseUI/test/main.cpp`：说明测试宿主为什么可以准备最小 SQLite 旧表、正式 UI 为什么不能建表/插表、每张旧表对应哪个 RuntimeDataCenter domain、`REPLACE`/`DELETE+INSERT` 的重复运行策略、`findChildren<QTableWidget*>` 冒烟检查和事件循环等待原因。

### 文档同步

- `MeyerScan架构设计与接口规范.md`：注释规范补充 2026-07-02 二次补强范围，文档版本更新到 v1.18。
- `MeyerScan重构任务总览.md`：开发约束补充 2026-07-02 起核心 UI/服务链路和测试宿主需优先解释的实现技巧，文档版本更新到 v3.16。
- `MeyerScan重构开发进度跟踪.md`：新增 2026-07-02 实现技巧型注释二次补强记录。
- `MySettingsUI`、`MyRuntimeDataCenter`、`MyCaseUI`、`MyMainExe` 的 `CHANGELOG.md`：新增 2026-07-02 中文变更记录。

### 验证结果

- `git diff --check`：未发现空白错误，仅有 Git 工作区 LF/CRLF 提示。
- 运行期路径复查：未发现实际代码调用 `QDir::currentPath()` 推导资源路径，命中项均为规范说明或注释。
- UI 文案复查：未发现 `tr("中文")` 形式的可见 UI 文案，仍遵守 `tr("English source text")` 规则。
- 编码复查：本轮重点修改的 C++ 文件均为 UTF-8 with BOM，避免 VS2015 误解析中文注释。
- 聚合构建：`MeyerScan_AllModules.sln` Release x64 构建通过，0 error；仍有 3 个既有外部登录头文件警告（C4819/C4091），不是本轮自研代码问题。
- 冒烟/测试：`LoggerTest.exe`、`DatabaseTest.exe`、`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`MyMainExe\bin\Release\MeyerScan.exe --smoke-main`、`F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0。

## 2026-07-02 - 组内评审后工程规则收口

### 用户要求

组内评审后要求对重构方案和代码做四类调整：

1. GitHub 之外增加本地仓库/备份，位置为 `F:\MeyerScan-Reposit`；除 Qt、VTK、OpenCV、PCL 等第三方库外，所有源码、测试项目、VS2015 工程、DLL/LIB、CMakeLists、文档和配置都要整体备份，提交日志使用中文且详细。
2. 每个模块和测试项目都必须能被 VSCode 和 VS2015 编译；每个模块和测试项目都要有 `CMakeLists.txt`，便于移植到其他电脑。
3. 非界面模块能不用 Qt 就不用 Qt；需要 Qt 的 UI 模块要尽量保持界面代码和业务代码分离，便于后续去 Qt 化。
4. 后续扫描重建模块的数据处理、编辑等能力要拆分 DLL，保持界面、交互和业务/算法处理分离。

### 本轮处理

- 新增根目录 `F:\MeyerScan\CMakeLists.txt` 和公共 `cmake/MeyerScanCommon.cmake`，作为 VSCode/CMake Tools 聚合入口。
- 为缺失的模块补齐 `CMakeLists.txt`：Database、ConfigCenter、Permission、UIComponents、CaseOrderService、RuntimeDataCenter、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell、VersionManager、SettingsUI、MainExe、MyLogin 测试宿主、CaseManager 参考目录。
- 更新 HomeUI、CaseUI、Logger 的既有 CMake，使其复用公共输出路径、Qt 路径、CRT 和兄弟模块链接规则。
- 新增 `tools/BackupToLocalRepository.ps1` 和 `tools/LocalBackup.gitignore`，用于把全模块工程快照备份到 `F:\MeyerScan-Reposit`，并额外保存 `D:\wj\重构文档` 的 Markdown 快照到 `_RefactorDocs`。
- 文档修正旧口径：非界面模块不再默认鼓励使用 Qt；已有 Qt 非界面模块保持公共 ABI 不暴露 Qt 类型，把 Qt 限制在 `.cpp` 私有实现内，后续可替换内部实现。
- 扫描重建方案修正：`ScanReconstructStudio.exe` 负责 UI/交互/工作台编排；编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等数据处理能力优先拆成 DLL 或独立库。
- 根 README、各模块 README/CHANGELOG、四个全局 Markdown 同步记录本地仓库、CMake 双入口、非界面 Qt 边界和扫描重建分层规则。

### 待验证/注意事项

- 当前机器未在 PATH 中找到 `cmake.exe`，CMake 文件先做静态一致性检查；实际 configure/build 需要安装 CMake 或把 CMake 加入 PATH 后再跑。
- VS2015/MSBuild 仍作为当前可执行验证入口；本轮已执行 `MeyerScan_AllModules.sln` Release x64，结果为 0 warning / 0 error。
- 冒烟/测试结果：`LoggerTest.exe`、`DatabaseTest.exe`、`RuntimeDataCenterTest.exe`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`MyMainExe\bin\Release\MeyerScan.exe --smoke-main`、`F:\MeyerScan\bin\Release\MeyerScan.exe --smoke-main` 均返回 0。
- `git diff --check` 未发现空白错误，仅有 Git 换行提示。
- Database、ConfigCenter、Permission、CaseOrderService、RuntimeDataCenter 目前仍有 Qt 内部实现，不在本轮大改；这是受控过渡，不是最终鼓励非界面模块长期依赖 Qt。

### 本地备份脚本修正与验证

- `BackupToLocalRepository.ps1` 改为脚本内部不写中文默认路径/中文默认提交信息，避免 Windows PowerShell 5.1 在无 BOM UTF-8 脚本中把中文误按 ANSI 解析；中文提交日志改为调用脚本时通过 `-CommitMessage` 传入。
- 修复脚本函数参数名使用 `$Args` 导致 `robocopy` 过滤参数未生效的问题；改为 `$CopyArgs` 后，`/MIR`、`/XD`、`/XF` 等参数能正确传递。
- 收紧本地备份过滤：排除 `.git/.vs/obj/build/logs/plugins/MySQL/SQLite/backup/CMakeFiles`，排除 Qt、VC/UCRT、OpenSSL、AWS、MySQL/SQLiteStudio/SQL 驱动等第三方运行文件，排除 `.db/.sqlite/.log/.frm/.MYD/.MYI` 等现场数据。
- `LocalBackup.gitignore` 同步作为二次兜底：本地仓库可以保留自研 DLL/EXE/LIB，但不允许提交第三方运行库、日志、数据库现场文件、IDE 私有文件和链接/增量编译中间文件。
- 已删除错误生成的 `F:\MeyerScan-Reposit` 并重新生成干净本地仓库；首个完整备份提交为 `09d2f1b01baa22000132de0b7ac1d91f20bcc241`，提交日志为“本地完整备份：落实评审要求，补齐全模块CMake和本地仓库规则”。后续补充备份会继续产生新提交，最新提交以 `git -C F:\MeyerScan-Reposit log -1 --oneline` 为准，避免文档记录提交号后再次备份形成循环更新。
- 用 `git ls-files` 检查本地仓库提交内容：未发现 Qt/VC/UCRT/OpenSSL/AWS/MySQL/SQLite 第三方 DLL，未发现 `.db/.sqlite/.log/.frm/.MYD/.MYI` 现场文件，未发现旧接口 token 文本；`_RefactorDocs` 仅提交 Markdown 快照。

## 2026-07-04 - MyOrderCreateUI 初版建单界面

### 用户要求

用户提供当前建单界面截图，要求新建 `MyOrderCreateUI`，把建单主要内容放在一个界面内，形成初版。

### 本轮处理

- 新增 `F:\MeyerScan\MyOrderCreateUI` 模块，包含 `include`、`src`、`test`、VS2015 `.sln/.vcxproj`、`CMakeLists.txt`、`README.md`、中文 `CHANGELOG.md` 和 `Version.rc`。
- 新增 `IOrderCreateUI` 接口和 `GetOrderCreateUI()` C ABI 工厂函数，提供 `Init`、`CreateWidget`、`SetActionCallback`、`GetModuleVersion`、`Shutdown`。
- 初版界面采用单页工作台布局：左侧基本信息，中间扫描类型和 FDI 牙位选择，右侧患者/订单摘要、牙位明细、标信息占位、备注和确认操作。
- 牙位选择只维护 UI 临时状态，不直接写数据库；每颗牙保存自己的扫描类型，避免切换当前工具类型后误改旧明细。
- 用户动作通过稳定动作 ID 上报：确认、取消、上一步、下一步、清空牙位和牙位变化；后续由 MainExe / OrderScanWorkspaceShell / Workflow 决定流程。
- 新增 `OrderCreateUITest.exe`，双击默认打开建单界面，`--smoke` 验证工厂函数、初始化、根控件、核心控件、牙位联动、清空和确认回调。
- 根 `CMakeLists.txt` 和 `MeyerScan_AllModules.sln` 接入 `MeyerScan_OrderCreateUI` 与 `OrderCreateUITest`。
- MainExe 版本清单 `version_modules.json`、内置默认清单和 VS2015 PostBuild 已补齐 `MeyerScan_OrderCreateUI.dll`，避免运行版本清单遗漏新模块。

### 验证结果

- `MeyerScan_OrderCreateUI.sln` Release x64 构建通过。
- `F:\MeyerScan\MyOrderCreateUI\bin\Release\OrderCreateUITest.exe --smoke` 返回 0。
- 本机仍未确认 CMake 可用，CMake 文件已补齐，实际 CMake 构建需后续安装/配置 CMake 后验证。

## 2026-07-04 - 第三方拉起建单与工作台集成链路

### 用户要求

用户要求继续梳理 MyOrderCreateUI 和 OrderScanWorkspaceShell：从首页点击“创建”进入 `MeyerScan_OrderScanWorkspaceShell`；为了开发 `MeyerScan_ExternalLaunchAdapter`，可以先模拟从 cmd 传入 JSON 文件路径打开 `MeyerScan.exe`，读取 JSON 后自动进入外部拉起适配模块，并把患者/订单信息填充到 `MyOrderCreateUI`。随后补充要求：标准上下文必须增加第三方类型记录参数，可能有多个第三方且需求不同；第三方调用应自动打开 MeyerScan.exe，打开首页、进入创建模块，但客户视觉上只能看到 OrderScanWorkspaceShell/OrderCreateUI，不能看到首页和进入创建模块的动作；同时执行前述三件事并回顾开发 md 文档按规范落实。

### 本轮处理

- `MyOrderCreateUI` 增加 `SetOrderContextJson(const char*)`，支持从标准 JSON 上下文填充患者基本信息、订单信息、医生/技工所、交付日期和扫描方案牙位。
- 标准建单上下文固定为 `source / patient / order / scanPlan`，其中 `source.thirdPartyType`、`thirdPartyName`、`sourceSystem`、`sourceVersion` 用于记录第三方来源和后续映射规则。
- 新增 `F:\MeyerScan\MyExternalLaunchAdapter`，输出 `MeyerScan_ExternalLaunchAdapter.dll` 和 `ExternalLaunchAdapterTest.exe`；公共 ABI 使用 `const char*`、POD 结构体和调用方缓冲区，Qt JSON 只作为内部实现。
- MainExe 新增命令行入口：`--external-order <json>` 和可选 `--external-order-type <type>`；新增 `--smoke-external-order` 自动测试入口。
- MainExe 单实例 IPC 从简单 activate 扩展为 JSON 消息，第二个进程带外部订单参数启动时，可把 JSON 路径和第三方类型转发给已登录完成的主实例。
- 首页“Create”入口现在进入 `OrderScanWorkspaceShell`，MainExe 创建 `OrderCreateUI` 并挂载到 `WorkspaceStepOrderCreate`。
- 第三方拉起时 MainExe 先后台准备 HomeUI 的 Create 入口并复核 `order.create` 的 `visible/enabled`，随后直接显示 OrderScanWorkspaceShell/OrderCreateUI；首页不挂到内容区，客户视觉上不会看到首页闪现或自动点击创建过程。
- ExternalLaunchAdapter 新源码和测试宿主已补中文实现技巧注释，并保存为 UTF-8 BOM，避免 VS2015 按代码页 936 误读中文注释。

### 当前边界

1. ExternalLaunchAdapter 不显示 UI、不写数据库、不启动扫描、不直接调用 OrderCreateUI，只做第三方字段到标准建单上下文的映射。
2. MainExe 是编排层，负责单实例、基础设施、后台首页入口规则复核、工作台显示和 OrderCreateUI 挂载。
3. OrderCreateUI 只消费标准上下文和维护 UI 临时状态，不认识每个第三方私有字段。
4. 后续新增第三方时，优先在 `MyExternalLaunchAdapter` 内按 `thirdPartyType` 增加映射和校验规则；不应把第三方分支写到 MainExe 或 OrderCreateUI。
5. 外部启动当前框架期跳过登录用于打通链路；正式规则若需要登录态、离线许可或云端账号判断，应加在 MainExe/Workflow 层，但仍不能让首页闪现。

### 验证结果

- `MeyerScan_ExternalLaunchAdapter.sln` Release x64 构建通过，无新增中文注释编码警告。
- `MeyerScan_OrderCreateUI.sln` Release x64 构建通过。
- `MeyerScan_MainExe.sln` Release x64 构建通过；仍只有外部登录头文件既有 C4819/C4091 警告。
- 根 `F:\MeyerScan\MeyerScan_AllModules.sln` Release x64 构建通过。
- 单模块输出目录测试通过：`ExternalLaunchAdapterTest.exe`、`OrderCreateUITest.exe --smoke`、`MeyerScan.exe --smoke-external-order --external-order F:\MeyerScan\MyExternalLaunchAdapter\test\external_order_sample.json --external-order-type cmd_demo` 均返回 0。
- 根输出目录测试通过：`F:\MeyerScan\bin\Release\ExternalLaunchAdapterTest.exe`、`OrderCreateUITest.exe --smoke`、`MeyerScan.exe --smoke-external-order --external-order F:\MeyerScan\MyExternalLaunchAdapter\test\external_order_sample.json --external-order-type cmd_demo` 均返回 0。

## 2026-07-04 - 全模块偏移复查与测试回归

### 用户要求

用户要求把所有模块和今天新增模块梳理一遍，对照开发 md 文档检查模块是否有偏移、测试项目是否完善、代码注释是否完善、模块链路是否通畅。

### 本轮处理

- 复查当前活跃自研模块目录：Logger、Database、DatabaseQtAdapter、ConfigCenter、Permission、UIComponents、VersionManager、RuntimeDataCenter、CaseOrderService、HomeUI、CaseUI、SettingsUI、Calibration3DUI、CalibrationColorUI、OrderScanWorkspaceShell、OrderCreateUI、ExternalLaunchAdapter、MainExe 均具备 README、CHANGELOG、CMakeLists、VS2015 工程、Version.rc 和测试宿主或 smoke 入口；`MyCaseManager` 继续定位为旧 schema/旧数据库参考目录，不按活跃模块改造。
- 补充/复核测试宿主注释：重点确认测试入口、路径推导、造数边界、事件循环和跨 DLL buffer 返回等说明，避免测试代码只靠函数名表达意图。
- 修正 ExternalLaunchAdapter 测试样例路径解析：测试宿主先查 exe 同级样例，再查单模块 `../../test`，最后从 exe 目录逐级向上查找 `MyExternalLaunchAdapter/test/external_order_sample.json`，不再依赖固定 `F:\MeyerScan` 开发机路径。
- 修正任务总览启动流程中的旧口径：创建订单链路从“直接进入 OrderCreateUI”改为 `OrderScanWorkspaceShell -> OrderCreateUI -> OrderWorkflowService -> ScanReconstructStudio`，与当前 MainExe 实现保持一致。
- 静态扫描确认：Database 中 Qt/QtSql 命中均为说明性注释，未发现实际 Qt 依赖；运行资源路径未发现实际使用 `QDir::currentPath()`；`tr(` 命中中文均为规则注释，不是 `tr("中文")` 可见文案；开发机绝对路径命中均为注释/说明或外部登录历史说明，不作为运行期回退路径。

### 验证结果

- 根 `F:\MeyerScan\MeyerScan_AllModules.sln` Release x64 构建通过。
- 根输出目录 19 个测试/主流程 smoke 全部返回 0：`LoggerTest.exe`、`DatabaseTest.exe`、`ConfigCenterTest.exe`、`PermissionTest.exe`、`VersionManagerTest.exe`、`DatabaseQtAdapterTest.exe`、`CaseOrderServiceTest.exe`、`RuntimeDataCenterTest.exe`、`UIComponentsTest.exe`、`Calibration3DUITest.exe`、`CalibrationColorUITest.exe`、`OrderScanWorkspaceShellTest.exe`、`OrderCreateUITest.exe --smoke`、`ExternalLaunchAdapterTest.exe`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke`、`SettingsUITest.exe --smoke`、`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke-external-order --external-order ... --external-order-type cmd_demo`。
- `git diff --check` 未发现空白错误，仅有 Git 工作区 LF/CRLF 换行提示。
- 当前机器 PATH 未找到 `cmake.exe`，所以 CMake configure/build 仍未实际执行；本轮只确认 CMakeLists 文件和 VS2015/MSBuild 主链路。

### 当前结论

本轮未发现需要立刻重拆模块的架构偏移。当前链路保持为：UI/Service 通过 RuntimeDataCenter、CaseOrderService 或 DatabaseQtAdapter 访问数据；Database 继续保持纯 C++ 基础设施；第三方拉起只由 ExternalLaunchAdapter 做字段归一化；MainExe 只做壳、权限复核、页面挂载和流程编排；OrderCreateUI 只展示/收集建单数据，不写数据库、不识别第三方私有字段。

## 2026-07-06 - ScanReconstructStudio 拆为扫描阶段和数据处理阶段

### 用户要求

用户提出将 `ScanReconstructStudio.exe` 内部拆成两个大模块：一个“扫描”，一个“数据处理”。仍然使用 `ScanReconstructStudio.exe` 作为独立壳子套住它们，便于后续被 `OrderScanWorkspaceShell.dll` 集成，也允许 `ScanReconstructStudio.exe` 独立打开。两个模块开发环境需要引入 VTK8.0、QVTKWidget 和 OpenCV3.3，开发环境和第三方库参考 `D:\wj\SelectPolyData` 与 `D:\SoftWareInstall\opencv`，同时要注意项目复制到其他位置后的可移植性，不偏移既有重构文档。

### 本轮处理

- 新增 `F:\MeyerScan\MyScanWorkflowUI`，输出 `MeyerScan_ScanWorkflowUI.dll` 和 `ScanWorkflowUITest.exe`，作为扫描阶段 UI DLL。
- 新增 `F:\MeyerScan\MyDataProcessUI`，输出 `MeyerScan_DataProcessUI.dll` 和 `DataProcessUITest.exe`，作为数据处理阶段 UI DLL。
- 新增 `F:\MeyerScan\MyScanReconstructStudio`，输出 `ScanReconstructStudio.exe`，作为独立扫描重建壳。
- `ScanReconstructStudio.exe` 通过 `QLibrary` 动态加载扫描 UI DLL 与数据处理 UI DLL，并在扫描/数据处理阶段切换前调用离开模块的 `DeactivateAndRelease()` 释放 QVTKWidget、VTK renderer、OpenGL/显存等重资源。
- 新增集中第三方路径规则：`F:\MeyerScan\cmake\MeyerScanScanThirdParty.props` 和 `F:\MeyerScan\cmake\MeyerScanScanThirdParty.cmake`。路径优先读取 `QT_ROOT`、`VTK_ROOT`、`VTK_HEADERS_ROOT`、`OPENCV_ROOT` 环境变量，再尝试仓库 `ThirdParty`，最后使用当前开发机参考路径。
- 根 `CMakeLists.txt`、`MeyerScan_AllModules.sln`、MainExe `version_modules.json`、MainExe 内置默认版本模块数组和 MainExe PostBuild 复制规则均已接入三个新模块。
- 新增源码曾因 VS2015 代码页解析中文注释出现编译风险，当前扫描相关新增 C++ 源码注释临时改为稳定 ASCII；README/CHANGELOG 和全局 Markdown 继续使用中文记录。

### 设计边界

1. `ScanReconstructStudio.exe` 是独立进程壳，不实现设备协议、扫描算法、后处理算法、病例管理、云端上传或权限核心。
2. `MeyerScan_ScanWorkflowUI.dll` 只负责扫描阶段界面、动作上报和阶段重资源生命周期，不直接实现 DeviceTransport、DeviceCmd 或算法。
3. `MeyerScan_DataProcessUI.dll` 只负责数据处理阶段界面、处理入口和阶段重资源生命周期，不把编辑、颈缘、测量、倒凹、咬合、底座等重算法直接塞进 UI 模块。
4. 后续编辑、预处理、数据 IO、颈缘、测量、倒凹、咬合、底座等处理能力应继续拆 DLL 或独立库，UI 只调用接口。
5. 跨 DLL / 跨进程仍只传稳定动作 ID、UTF-8 JSON、订单 ID 或上下文路径，不传 VTK 对象、QObject、QString 指针、QJsonObject 指针、大块模型内存所有权。
6. 进入扫描重建前，MainExe 仍必须按既有规则释放 CaseUI 等非必要资源，避免与 VTK/OpenGL/算法争抢内存和显存。

### 验证结果

- `MeyerScan_ScanWorkflowUI.sln` Release x64 构建通过，`ScanWorkflowUITest.exe` 返回 0。
- `MeyerScan_DataProcessUI.sln` Release x64 构建通过，`DataProcessUITest.exe` 返回 0。
- `MeyerScan_ScanReconstructStudio.sln` Release x64 构建通过，`ScanReconstructStudio.exe --smoke` 返回 0。
- 当时机器未找到 `cmake.exe`，因此该轮 CMake 文件只完成规则同步，尚未实际 configure/build；2026-07-06 已安装 CMake 3.31.6，并使用 VS2015 x64 生成器完成根聚合 `Release` configure/build 补验证。

### 文档同步

- `MeyerScan重构任务总览.md`：第五阶段改为“扫描重建独立进程壳 + 扫描阶段 UI DLL + 数据处理阶段 UI DLL”，并补充 VTK/OpenCV 迁移路径规则。
- `MeyerScan架构设计与接口规范.md`：模块职责表、拆分模块清单、近期设计决策和尾部新增模块清单改为新三件套口径。
- `MeyerScan重构开发进度跟踪.md`：第五阶段状态改为进行中，记录三个新模块的构建和 smoke 结果，同时明确真实 IPC、设备、算法和处理工具仍未完成。
- 三个新模块内部 `README.md` / `CHANGELOG.md` 均记录职责、边界、依赖路径和验证方式。

## 2026-07-07 - 建单扫描流程创建链路接入

### 用户要求

用户要求在建单页面加入扫描流程创建能力：读取 `F:\MeyerScan\MyOrderCreateUI\扫描流程创建` 中的需求和参考代码，在建单页通过输入控制后续 Scan / Process 界面的扫描流程按钮；创建模块有建单页，练习模块没有建单页，练习模块默认流程固定为自然上颌、交换、自然下颌、自然咬合；建单页新增上颌异性扫描杆、下颌异性扫描杆、上颌扫描杆分段、下颌扫描杆分段四个开关，以及五种咬合类型下拉选项。

### 本轮处理

- `MyOrderCreateUI` 升级到 v0.3.0，新增扫描流程输入区和 `GetCurrentScanProcessJson()`，输出 `schemaVersion/source/config/steps` 结构。
- `OrderCreateActionScanProcessChanged` 用于通知 MainExe 当前建单流程输入已变化；MainExe 收到后重新读取建单页 JSON，不直接读取建单页控件。
- 扫描流程规则当前集中在 OrderCreateUI：异性扫描杆优先；分段开关只控制第二扫描杆/第二异性扫描杆；普通扫描杆流程仍由对应颌是否存在 implant 牙位触发；咬合类型支持自然、上颌临时、下颌临时、全口临时、咬合记录。
- `MyMainExe` 升级到 v0.1.2，创建模式进入 Scan / Process 前把建单页生成的 `scanProcess` 合并进工作台上下文；练习模式固定生成默认流程。
- `MyScanWorkflowUI` 和 `MyDataProcessUI` 升级到 v0.2.0，顶部按钮统一从 session JSON 的 `scanProcess.steps` 渲染；没有流程时回退默认练习流程。
- 模块 README/CHANGELOG 和全局重构文档同步记录边界：OrderCreateUI 生成，MainExe 转发，Scan/Process 只渲染，不反向解析建单开关。

### 验证结果

- CMake 根聚合 `MeyerScan` Release 构建通过。
- `OrderCreateUITest.exe --smoke` 返回 0，覆盖扫描流程控件和 JSON 输出断言。
- `ScanWorkflowUITest.exe` 返回 0，验证扫描页可按 `scanProcess.steps` 渲染自定义流程按钮。
- `DataProcessUITest.exe` 返回 0，验证处理页使用同一份 `scanProcess.steps`。
- `MeyerScan.exe --smoke-main` 返回 0，验证 MainExe 创建/练习工作台主链路仍可运行。

### 当前边界

1. 本轮只是把扫描流程按钮的生成和传递链路打通，不代表真实设备采集、算法重建、数据处理或 IPC 已完成。
2. `scanProcess` 是轻量 JSON 合同，后续规则复杂化后应迁入 `ScanSchemaService` / `OrderWorkflowService`，不要把规则复制到扫描页或处理页。
3. Scan / Process 页面只读 `steps` 并渲染按钮；按钮点击后的真实扫描状态机、跳转、回退、跳过和异常处理仍需后续专门设计。
