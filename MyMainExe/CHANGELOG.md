# MeyerScan MainExe 变更记录

## 2026-06-25

- 版本清单改为读取 `config/version_modules.json`，只记录拆分模块 EXE/DLL，不再把 Qt、OpenSSL、AWS、VC/UCRT 等第三方库写入 `logs/versionList`。
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
- 启动流程调整为：Logger 初始化、Database 健康检查、Login、HomeUI、CaseUI。
- 链接并调用 `MeyerScan_Logger.dll` 和 `MeyerScan_Database.dll`，数据库仅用于启动健康检查。
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
