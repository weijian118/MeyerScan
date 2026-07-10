# MeyerScan MainExe 变更记录

## 2026-07-10

- 版本升级为 `v0.1.6`，接收浏览页云端/截图稳定动作 ID 并记录状态，实际云端与截图服务继续留待后续模块实现。
- 版本升级为 `v0.1.5`，新增 `MeyerScan_UIResources.dll` 运行依赖和 versionList 项，正式输出不再复制各 UI 模块散落的 PNG/QSS。
- MainExe VS2015/CMake 构建改为复制单一资源 DLL；`Resources` 目录继续保留登录许可等非 UI 运行文件。
- VS2015 PostBuild 的根输出 DLL 兜底复制只在独立 `MeyerScan_MainExe.sln` 构建时执行，根聚合方案不再把输出文件复制到自身。
- 首页、浏览和建单模块分别升级到 v0.3.1/v0.3.1/v0.5.0，并完成 1920x1080、1366x768 截图复核。
- 版本升级为 `v0.1.4`，同步更新 `ModuleInfo::Version`、CMake 工程版本和 `Version.rc` 文件版本。
- 主窗口固定使用 `Qt::FramelessWindowHint + showFullScreen()`；补齐 `ShowMainWindow()`，单实例激活和最小化恢复后仍回到全屏无边框状态。
- 删除此前试验性的 36px 跨页面通用可见标题栏。MainExe 只保留顶层窗口、单内容区和窗口动作执行，HomeUI、CaseUI、OrderScanWorkspaceShell 各自维护页面语义顶部区域。
- 接入 HomeUI、CaseUI、OrderScanWorkspaceShell 新增的最小化、关闭、返回动作 ID；页面模块只上报动作，MainExe 统一执行窗口操作和页面替换。
- 根方案和 MainExe 依赖补齐 `MeyerScan_ScanReconstructStudio.dll`；版本清单默认增加该 DLL，确保 EXE/DLL 双形态同时记录文件版本和代码版本。
- `--smoke-main` 继续覆盖首页、浏览、创建/练习工作台、Scan/Process/Send 切换和重资源释放。
- 最终回归：VS2015/CMake 根构建通过，24 项模块/主链路测试全部返回 0；最新 versionList 为 24 项、0 缺失、0 版本不一致、0 `codeVersionError`。

## 2026-07-08

- MainExe CMake 和 VS2015 PostBuild 补充复制 `MyOrderCreateUI/Resources/icon/createModule/sacanPlan`，运行目录统一落到 `Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan`。
- 明确资源复制规则：模块私有资源先放模块源码 `Resources/`，主程序构建/打包时复制到 MeyerScan.exe 同级 `Resources/Modules/<ProjectName>/...`；公共资源后续放 `Resources/Common` 或由 UIComponents 管理。
- 本轮不升级 MainExe 代码版本；仅保证建单治疗方案图片、mask 和桥连接点资源在单模块输出和主程序输出目录都可被找到。

## 2026-07-07

- 版本升级为 `v0.1.3`，创建模式从 DataProcess 的 Next 动作进入工作台 Send 步骤，并动态加载 `MeyerScan_SendUI.dll`。
- MainExe 为 SendUI 注入当前工作台订单上下文，并处理 `Previous`、`Export`、`Compress`、`Email Send`、`Upload`、`Finish` 动作；当前只记录动作和流程跳转，真实发送业务后续接服务模块。
- `version_modules.json`、CMake、VS2015 工程和根聚合方案纳入 `MeyerScan_SendUI.dll`，运行时版本清单开始记录 SendUI 文件版本和代码版本。
- 进入 Send 步骤前释放 ScanWorkflowUI / DataProcessUI 重资源，避免 QVTK/VTK/OpenGL 资源继续占用。
- `--smoke-main` 覆盖创建工作台 Scan → Process → Send 链路。
- 版本升级为 `v0.1.2`，MainExe 增加工作台扫描流程上下文编排：创建模式从 OrderCreateUI 获取 `scanProcess` JSON，合并到 `m_workspaceContextJson` 后转发给 ScanWorkflowUI/DataProcessUI；练习模式固定使用 Natural maxilla / Exchange / Natural mandible / Natural occlusion 默认流程。
- 工作台切到 Scan 或 Process 前会重新读取建单页最新扫描流程，防止用户直接点击顶部步骤按钮时跳过 Next 按钮导致流程不同步。
- 若 ScanWorkflowUI/DataProcessUI 已创建，MainExe 更新 `scanProcess` 后会再次调用对应模块的 `SetSessionContextJson()`，保证已有页面按钮可刷新。
- 版本升级为 `v0.1.1`，同步更新 `ModuleInfo::Version`、CMake `project(VERSION)` 和 `Version.rc` 文件版本，保证运行时版本清单能区分本轮工作台集成改动。
- 复核创建工作台、练习工作台、Scan/Process 懒加载和重资源释放链路；当前 MainExe 直接挂载 `MeyerScan_ScanWorkflowUI.dll` / `MeyerScan_DataProcessUI.dll` 是阶段性集成方案，用于先跑通创建/练习流程，后续真实扫描仍保持 `ScanReconstructStudio.exe` 独立进程边界。
- `--smoke-main` 保持覆盖首页、设置、创建工作台、练习工作台、Scan/Process 切换、案例管理和扫描前资源释放。

## 2026-07-06

- 首页 Practice 入口接入练习工作台：MainExe 复用 `MeyerScan_OrderScanWorkspaceShell.dll` 的练习模式，只显示 Scan / Process，并挂载 `MeyerScan_ScanWorkflowUI.dll` 与 `MeyerScan_DataProcessUI.dll`。
- 创建工作台和练习工作台右上角统一只保留 `Minimize` / `Close`；`Minimize` 最小化主窗口，`Close` 关闭当前工作台并返回首页，不退出 MeyerScan.exe。
- Scan / Process 页面改为按步骤懒加载；切换离开时通过 `Shutdown()` / `DeactivateAndRelease()` 释放 QVTK/VTK/OpenGL 重资源，并用占位页替换旧步骤，避免壳子持有待删除 QWidget。
- `--smoke-main` 覆盖范围扩大到创建工作台、练习工作台、Scan/Process 切换和重资源释放，自动退出时间调整为 5 秒。
- CMake 构建入口完成实际验证：使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 和 VS2015 x64 生成器，根聚合工程 `Release` 配置与构建通过。
- CMake PostBuild 补齐运行目录配置复制：`db_config.json`、`version_modules.json/.md`、`runtime_config.json/.md`、`permission_rules.json/.md` 和第三方建单样例都会同步到 `MeyerScan.exe` 同级目录。
- MainExe 运行目录补齐扫描 UI 所需的 VTK/OpenCV 运行库复制；这些 DLL 只作为运行依赖存在，不进入 `logs/versionList`。
- `version_modules.json` 已纳入 `ScanReconstructStudio.exe`、`MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll`；最新 `versionList` 为 schemaVersion=2、21 个模块，扫描三件套均记录 `fileVersion`、`codeVersion`，且 `versionMatch=true`。
- 验证：CMake 根聚合 `Release` 构建通过；`MeyerScan.exe --smoke-main` 返回 0；最新运行时版本清单无 `codeVersionError`，且未混入 Qt/VTK/OpenCV 等第三方库。

## 2026-07-05

- MainExe 对自研模块改为运行时动态加载：Logger、ConfigCenter、Permission、UIComponents、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI、OrderCreateUI、OrderScanWorkspaceShell、ExternalLaunchAdapter 均通过 `QLibrary + extern "C" GetXxx()` 获取接口，VS2015 工程不再链接这些模块的 import lib。
- `config/version_modules.json` 升级为 schemaVersion=2，模块项从字符串扩展为 `{ file, versionFunction }`；自研模块统一通过 `GetMeyerModuleVersion()` 读取代码版本，启动时 `logs/versionList/versionList_*.json` 同时记录 `fileVersion`、`codeVersion`、`versionMatch` 和 `codeVersionError`。
- `config/version_modules.md` 重写字段说明，明确 Windows 文件详细信息来自 `Version.rc`，代码版本来自 `ModuleInfo::Version` / `GetMeyerModuleVersion()`，二者必须同步维护。
- CMake 中 MainExe 不再通过 `meyer_link_sibling_module()` 链接自研 DLL，只保留头文件目录和构建顺序依赖；Qt、Windows `Version.lib`、既有登录模块 `MeyerLoginWidget.lib` 仍保持当前链接方式。
- 版本清单文件名改为 `versionList_yyyyMMdd_HHmmss_zzz.json`，避免同一秒内连续 smoke、第三方拉起或重复启动覆盖版本快照。
- VS2015 PostBuild 增加根聚合输出目录 `MeyerScan_*.dll` 兜底覆盖；CMake POST_BUILD 也复制依赖 target 的 DLL，保证单模块输出目录不会残留旧自研 DLL。
- 验证：`MeyerScan_MainExe.sln`、根 `MeyerScan_AllModules.sln`、`MyDatabaseQtAdapter\MeyerScan_DatabaseQtAdapter.sln` Release x64 构建通过；根输出目录主链路/第三方建单 smoke 和单模块输出目录主链路 smoke 均正常返回；根/单模块最新 `versionList` 均无缺失、无 `codeVersionError`，自研模块 `versionMatch=true`。

## 2026-07-04

- 首页 `Create` 入口接入 `MeyerScan_OrderScanWorkspaceShell.dll`，由 MainExe 创建 `MeyerScan_OrderCreateUI.dll` 并挂载到工作台建单步骤。
- 新增 `--external-order <json>` 和 `--external-order-type <type>` 命令行参数，用于模拟第三方软件拉起本地口扫软件并下发建单信息。
- 单实例 IPC 消息从简单激活扩展为 JSON，已运行实例在登录完成后可接收第二个进程转发的第三方订单路径和第三方类型。
- 第三方拉起建单时，MainExe 后台准备 HomeUI 的“Create”入口并复核 `order.create` 的 `visible/enabled`，但不显示首页，客户视觉上直接看到 `OrderScanWorkspaceShell/OrderCreateUI`。
- 新增 `--smoke-external-order` 自动验证入口，覆盖 ExternalLaunchAdapter → MainExe → OrderScanWorkspaceShell → OrderCreateUI 链路并自动退出。
- `config/version_modules.json`、VS2015 工程和 CMake 已纳入 `MeyerScan_ExternalLaunchAdapter.dll`，保证运行时版本清单和发布目录包含第三方拉起适配模块。
- 验证：`MeyerScan_MainExe.sln` 和根 `MeyerScan_AllModules.sln` Release x64 构建通过；单模块输出和根输出目录的 `MeyerScan.exe --smoke-external-order --external-order ... --external-order-type cmd_demo` 均返回 0。MainExe 仍只有外部登录头文件既有 C4819/C4091 警告。

## 2026-07-02

- 2026-07-03 复查补充：单模块 `MeyerScan_MainExe.sln` 和根 `MeyerScan_AllModules.sln` 均重新构建；`MyMainExe\bin\Release` 与 `F:\MeyerScan\bin\Release` 均复制 x64 `sqlite3.dll`，`MeyerScan.exe --smoke-main` 在两个目录均返回 0。
- 新增模块 `CMakeLists.txt`，作为 VSCode/CMake Tools 构建入口，同时继续保留 VS2015 `MeyerScan.vcxproj` 和聚合解决方案。
- 新增根目录 CMake 聚合工程后，MainExe 作为最终主程序目标放在构建顺序末尾，链接 HomeUI、CaseUI、SettingsUI、ConfigCenter、Permission、RuntimeDataCenter、DatabaseQtAdapter、Logger 和外部登录库。
- 启动期数据库健康检查改为通过 `MyDatabaseQtAdapter` 访问纯 C++ Database，MainExe 不再直接包含 `Database.h`；版本清单 `version_modules.json` 同步新增 `MeyerScan_DatabaseQtAdapter.dll`。
- VS2015 PostBuild 中 `sqlite3.dll` 复制来源改为仓库内相对路径；后续打包模块再统一纳入安装清单。
- 按评审结论同步工程规则：GitHub 提交之外，MainExe 及其依赖产物需要随全部模块一起备份到 `F:\MeyerScan-Reposit`。
- 继续按“实现技巧型注释”要求补强 `MainWindow.cpp`：补充 Home/Case/Settings 页面创建失败降级、UI 模块初始化边界、单内容区页面释放、`deleteLater()` 延迟析构、Layout stretch、配置/权限 visible 与 enabled 合并、版本 manifest 顺序、Windows 文件版本资源和 Logger 早期初始化的实现说明。
- 本轮只补充注释和文档记录，不改变 MainExe 启动、登录、单实例、页面切换、RuntimeDataCenter 或版本清单行为。

## 2026-07-01

- 按“实现技巧型注释”要求补强 MainExe 相关注释口径：前一轮已补充启动等待页、登录参数、基础设施初始化、配置/权限合并、页面创建/切换/释放、扫描前释放 CaseUI、版本清单、Windows 文件版本 API、`deleteLater()` / 事件循环和单实例测试入口说明。
- 本轮在全局文档中把这些注释要求升级为“解释代码实现技巧”，后续 MainExe 新增逻辑必须同步说明 Qt 事件循环、页面所有权、跨 DLL 缓冲区、Windows API 和权限复核机制。
- 本轮只补充注释规则和文档记录，不改变 MainExe 启动、页面切换、版本清单或 RuntimeDataCenter 集成逻辑。

## 2026-06-30

- MainExe 接入 `MeyerScan_RuntimeDataCenter.dll`，数据库连接后初始化运行时数据中心并执行 `ReloadAll()`。
- 版本清单 `config/version_modules.json` 新增 `MeyerScan_RuntimeDataCenter.dll`，Release PostBuild 同步复制该模块。
- 默认运行链路切换为 SQLite 后，RuntimeDataCenter 允许空库/缺表以 Warning 形式记录，不阻断主程序启动。
- Release PostBuild 的自研 Qt DLL、平台插件和 SQL 驱动复制来源统一为编译所用 `C:\Qt\Qt5.6.3\5.6.3\msvc2015_64`，避免聚合根输出目录混用 Qt 5.6.2 / 5.6.3；外部登录相关既有 DLL 仍从已安装软件目录复制。
- `MeyerScan.exe --smoke-main` 需同时覆盖单模块输出目录和根聚合输出目录，保证 MainExe、CaseUI、RuntimeDataCenter、Database 和 Qt 运行库链路一致。

## 2026-06-25

- 版本清单改为读取 `config/version_modules.json`，只记录拆分模块 EXE/DLL，不再把 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT 等第三方库写入 `logs/versionList`。
- Release PostBuild 补充复制 `runtime_config.json`、`permission_rules.json` 及对应说明 md 到 `config/`，避免运行时只依赖首次启动自动生成默认配置。
- Release PostBuild 补齐 `MeyerScan_CaseOrderService.dll`、`MeyerScan_OrderScanWorkspaceShell.dll`、`MeyerScan_Calibration3DUI.dll`、`MeyerScan_CalibrationColorUI.dll`，保证 `version_modules.json` 中声明的已开发拆分模块能进入 MainExe 运行目录。
- README 和 `config/version_modules.md` 补充维护规则：新增模块写入 manifest 后，必须同步修改 PostBuild/安装包脚本，否则运行时 versionList 会以 `exists=false` 暴露漏复制问题。
- 新增 `config/version_modules.md`，说明版本清单 manifest 字段含义和后续扩展方式；JSON 文件内部保持无注释。
- 主界面切换从 `QStackedWidget` 并列页面改为单内容区替换页面：首页、浏览、等待页一次只挂载一个全屏页面，离开后按资源规则释放旧页面。
- HomeUI / CaseUI 的 `enabled` 权限开始生效：MainExe 下发按钮启用态，并在收到入口/动作回调后再次复核 `Permission::IsFeatureEnabled()`。
- 登录离线许可路径改为 `Resources/license.lic`，PostBuild 同步复制到 `Resources` 目录；后续资源文件统一放入 `Resources`。
- 新增 `F:\MeyerScan\MeyerScan_AllModules.sln`，用于 VS2015 一次打开当前已拆分模块工程，外部既有登录模块不纳入。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名、`FileDescription`、Debug 标志、`FILEOS` 和 `FILETYPE` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_MainExe"`，保证 MainExe 便捷日志宏输出正确主程序模块名。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：启动等待页、登录参数、基础设施初始化、配置/权限合并、页面创建/切换/释放、扫描前释放 CaseUI、版本清单和单实例测试入口均增加关键说明。
- 修复登录模块显示后等待页未关闭的问题：登录窗口弹出前释放等待页并隐藏 MainWindow，登录成功后再显示首页。
- Logger 初始化提前到 MainWindow 构造阶段，并缓存 `ILogger*` 成员变量，后续 MainExe 写日志不再每次调用 `GetLogger()`。
- Release 子系统改为 Windows，并指定 `mainCRTStartup`，隐藏客户启动时的 CMD 窗口。
- 版本清单生成逻辑并入 MainExe 内部，启动时仍输出 `logs/versionList/versionList_时间戳.json`，当前不再依赖 `MeyerScan_VersionManager.dll`。
- 固定流程不再写入 `runtime_config.json`：等待页和单实例由 MainExe 固定控制，不提供配置开关。
- README 补充 `runtime_config.json` 与 `permission_rules.json` 的关系，以及 `visible` / `enabled` 字段在 MainExe 合并 UI 显隐时的含义。
- 补充 MainExe 头文件、实现文件和测试入口函数级注释，关键启动流程、单实例、等待页、页面释放和版本清单逻辑增加说明。
- 移除 VS2015 v140 项目中不兼容/无必要的 `LanguageStandard` 标签，降低 VS2015 打开项目时报错的概率。

## 2026-06-23

- 新增单实例控制：重复启动时尝试通知已运行实例激活窗口；数据库检查或登录阶段主窗口未显示时不强行打断流程。
- 新增启动等待页，由 UIComponents 提供基础等待界面。
- 接入 ConfigCenter，启动时读取 `config/runtime_config.json`，并根据 `database.type` 控制 Database 的 MySQL/SQLite 类型。
- 接入 Permission，先用于控制首页“设置”入口和浏览模块“返回首页”按钮显隐。
- 接入 UIComponents，提供等待页和后续共享控件入口。
- 当时接入 VersionManager 生成 `logs/versionList/versionList_时间戳.json`；2026-06-24 已将该轻量生成逻辑并入 MainExe。
- 路径统一基于 `QCoreApplication::applicationDirPath()`，日志、配置、版本清单都放在 `MeyerScan.exe` 同级目录下，不依赖进程 current directory。
- 首页、浏览页、等待页改为按需创建；页面切换完成后释放非当前页面 widget，避免不可见模块长期占用资源。
- 页面释放使用 `deleteLater()`，避免在按钮点击信号尚未返回时直接销毁控件树。
- 2026-06-23 复查补充：删除数据库配置开发路径回退，只读取运行目录 `config/db_config.json`；缺失时只记录健康检查不可用。
- 2026-06-23 复查补充：登录许可路径改为 `MeyerScan.exe` 同级 `license.lic`，不在运行参数中引用开发目录。
- 2026-06-23 复查补充：首页“设置”和浏览“返回首页”显隐改为 ConfigCenter 与 Permission 同时允许才显示。
- 2026-06-23 复查补充：页面 widget 释放后再次进入时不重复初始化 HomeUI/CaseUI 模块，只重建页面 widget。
- 2026-06-23 复查补充：MainExe 预留 `PrepareForScanReconstruct()`，后续打开扫描重建进程前必须先切到等待页并释放 CaseUI widget，不能只隐藏浏览模块。
- 2026-06-23 交互复查补充：CaseUI 的 `Open` 操作已接入 `PrepareForScanReconstruct()`，当前先切到等待页、释放 CaseUI widget、处理延迟删除事件，后续再接入 `ScanReconstructStudio.exe` 启动。
- 2026-06-23 交互复查补充：`--smoke-main` 自动覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前资源释放链路，确保页面创建、切换、释放和日志记录均被验证。
- 2026-06-23 交互复查补充：单实例激活改为登录完成且主窗口可见后才响应，数据库检查或登录阶段不抢占前台窗口。
- 2026-06-23 复查验证：新增扫描前资源释放预留函数后，`MeyerScan_MainExe.sln` Release x64 构建通过，`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0。
- 2026-06-23 复查补充：MainExe 自身工具栏、状态栏、等待页等可见文字统一使用 `tr("English source text")`，并固定 `MainExe` 翻译上下文。
- 2026-06-23 复查验证：文案规则收口后，`MeyerScan_MainExe.sln` Release x64 构建通过，`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0；仅保留既有登录头文件编码/声明警告。
- 重新验证：ConfigCenter、Permission、UIComponents、VersionManager、HomeUI、CaseUI、MainExe Release x64 构建通过；`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke` 均返回 0。

## 2026-06-22

- 修复登录成功后未进入首页的问题：MainExe 接收 `LoginReturnParameters`，对登录成功状态统一进入 HomeUI。
- 接入 HomeUI 入口回调：点击首页“浏览”后由 MainExe 集中切换到 CaseUI。
- 接入 CaseUI 操作回调：点击浏览模块“返回首页”后由 MainExe 集中切回 HomeUI。
- 历史初版页面切换曾使用 `QStackedWidget` 预创建首页和浏览页；2026-06-25 已改为 MainExe 单内容区替换全屏页面，首页和浏览不再作为并列兄弟页长期驻留。
- 新增客户操作日志：工具栏导航、首页入口、浏览模块操作、页签切换、页面切换、登录状态均写入结构化日志。
- 重新验证：`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`CaseUITest.exe --smoke` 均返回 0。

- 新增 `MyMainExe` 主程序入口模块。
- 输出目标命名为 `MeyerScan.exe`。
- 集成既有登录模块 `MeyerLoginWidget.dll`。
- 历史记录：启动流程曾调整为 Logger 初始化、Database 健康检查、Login、HomeUI、CaseUI；2026-07-03 起数据库健康检查改为通过 `MyDatabaseQtAdapter` 访问纯 C++ `MeyerScan_Database.dll`。
- 当前 MainExe 链接 Logger、DatabaseQtAdapter、RuntimeDataCenter、HomeUI、CaseUI、SettingsUI 等模块；MainExe 不再直接包含 `Database.h`，数据库仅用于启动健康检查和运行时快照准备。
- 登录成功后加载 HomeUI；HomeUI 通过入口回调通知 MainExe 切换到 CaseUI，CaseUI 通过操作回调通知 MainExe 返回首页。
- MainExe 当前只做启动、模块编排和窗口容器，不承载业务规则和数据库 SQL。
- 新增 `--smoke` 和 `--smoke-main` 两种自动退出验证模式。
- Release PostBuild 补齐登录、数据库、首页、案例、日志模块运行依赖，包括 Qt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi、SQL 驱动和平台插件。
- 验证通过：VS2015 Release x64 构建通过；`MeyerLoginTest.exe --smoke`、`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main` 均返回 0。
- 记录外部登录头文件编码/声明警告，后续建议新增 `LoginAdapter` 隔离。

## 2026-06-22 复测记录

- 重新执行 VS2015 Release x64 构建，`MeyerScan_MainExe.sln` 通过，0 warning / 0 error。
- 重新执行 `MeyerScan.exe --smoke`，返回 0。
- 重新执行 `MeyerScan.exe --smoke-main`，返回 0。
- 同步验证 `MeyerLoginTest.exe --smoke`，返回 0，确认登录测试宿主仍可正常装载。
- 递归检查 Release 目录 EXE/DLL 依赖闭包，非 API Set DLL 缺失为空；当前 Release 目录共 50 个 DLL。
- 对照重构文档复核：MainExe 仍保持“薄主 EXE”定位，只做 Logger、Database 健康检查、Login、HomeUI、CaseUI 编排；未加入业务 SQL、订单规则、扫描算法或设备协议。
- 后续建议优先新增 `LoginAdapter`，并把登录 URL、语言、许可路径、数据库配置路径迁入 ConfigCenter，避免框架期硬编码继续扩散。
