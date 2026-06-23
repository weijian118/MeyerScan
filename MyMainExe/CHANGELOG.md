# MeyerScan MainExe 变更记录

## 2026-06-23

- 新增单实例控制：重复启动时尝试通知已运行实例激活窗口；数据库检查或登录阶段主窗口未显示时不强行打断流程。
- 新增启动等待页，由 UIComponents 提供基础等待界面。
- 接入 ConfigCenter，启动时读取 `config/runtime_config.json`，并根据 `database.type` 控制 Database 的 MySQL/SQLite 类型。
- 接入 Permission，先用于控制首页“设置”入口和浏览模块“返回首页”按钮显隐。
- 接入 UIComponents，提供等待页和后续共享控件入口。
- 接入 VersionManager，启动时生成 `logs/versionList/versionList_时间戳.json`。
- 路径统一基于 `QCoreApplication::applicationDirPath()`，日志、配置、版本清单都放在 `MeyerScan.exe` 同级目录下，不依赖进程 current directory。
- 首页、浏览页、等待页改为按需创建；页面切换完成后释放非当前页面 widget，避免不可见模块长期占用资源。
- 页面释放使用 `deleteLater()`，避免在按钮点击信号尚未返回时直接销毁控件树。
- 2026-06-23 复查补充：删除数据库配置开发路径回退，只读取运行目录 `config/db_config.json`；缺失时只记录健康检查不可用。
- 2026-06-23 复查补充：登录许可路径改为 `MeyerScan.exe` 同级 `license.lic`，不在运行参数中引用开发目录。
- 2026-06-23 复查补充：首页“设置”和浏览“返回首页”显隐改为 ConfigCenter 与 Permission 同时允许才显示。
- 2026-06-23 复查补充：页面 widget 释放后再次进入时不重复初始化 HomeUI/CaseUI 模块，只重建页面 widget。
- 2026-06-23 复查补充：MainExe 预留 `PrepareForScanReconstruct()`，后续打开扫描重建进程前必须先切到等待页并释放 CaseUI widget，不能只隐藏浏览模块。
- 2026-06-23 复查验证：新增扫描前资源释放预留函数后，`MeyerScan_MainExe.sln` Release x64 构建通过，`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0。
- 2026-06-23 复查补充：MainExe 自身工具栏、状态栏、等待页等可见文字统一使用 `tr("English source text")`，并固定 `MainExe` 翻译上下文。
- 2026-06-23 复查验证：文案规则收口后，`MeyerScan_MainExe.sln` Release x64 构建通过，`MeyerScan.exe --smoke-main`、`MeyerScan.exe --smoke` 均返回 0；仅保留既有登录头文件编码/声明警告。
- 重新验证：ConfigCenter、Permission、UIComponents、VersionManager、HomeUI、CaseUI、MainExe Release x64 构建通过；`MeyerScan.exe --smoke`、`MeyerScan.exe --smoke-main`、`HomeUITest.exe --smoke`、`CaseUITest.exe --smoke` 均返回 0。

## 2026-06-22

- 修复登录成功后未进入首页的问题：MainExe 接收 `LoginReturnParameters`，对登录成功状态统一进入 HomeUI。
- 接入 HomeUI 入口回调：点击首页“浏览”后由 MainExe 集中切换到 CaseUI。
- 接入 CaseUI 操作回调：点击浏览模块“返回首页”后由 MainExe 集中切回 HomeUI。
- 页面切换统一走 `QStackedWidget`，首页和浏览页预创建后复用，切换时短暂关闭更新并立即恢复，降低窗口闪现风险。
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
