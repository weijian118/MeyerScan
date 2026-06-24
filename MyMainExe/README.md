# MeyerScan MainExe

`MyMainExe` 是新架构下的主程序入口模块，输出 `MeyerScan.exe`。
当前阶段先跑通最小主链路：

1. 启动 Qt 主程序。
2. 执行单实例检查，重复启动时尝试激活已打开窗口。
3. 显示启动等待页。
4. 初始化 Logger、ConfigCenter、Permission、UIComponents、Database，并由 MainExe 内部生成版本清单。
5. 根据 ConfigCenter 的 `database.type` 设置数据库类型，并执行数据库健康检查。
6. 写出 `logs/versionList/versionList_时间戳.json`。
7. 调用既有 `MeyerLoginWidget.dll` 显示登录界面。
8. 登录成功后加载 `MyHomeUI` 首页模块。
9. HomeUI 发出入口点击事件，MainExe 集中切换到 `MyCaseUI` 案例管理模块。
10. CaseUI 发出 `Open` 操作事件时，MainExe 先进入扫描前准备流程：切换等待页、释放 CaseUI widget、处理延迟删除事件；后续再接入 `ScanReconstructStudio.exe`。

## 当前边界

- MainExe 只做启动、模块编排和窗口容器。
- 业务规则、数据库 SQL、权限核心、扫描采集不写在 MainExe。
- 当前 MainExe 直接调用 Database 只做启动健康检查，不做业务查询；正式病例、订单和扫描方案必须走 Service/Workflow。
- 所有运行路径基于 `QCoreApplication::applicationDirPath()`，不使用 `QDir::currentPath()`，避免第三方软件拉起时工作目录错误。
- 日志目录固定为 `MeyerScan.exe` 同级 `logs/`，版本清单写入 `logs/versionList/`。
- ConfigCenter 当前读取 `config/runtime_config.json`；Permission 当前读取 `config/permission_rules.json`，先用于首页“设置”和浏览“返回首页”的显隐控制。
- `runtime_config.json` 表示产品/客户默认策略，例如 `database.type`、`feature.home.settingsVisible`、`feature.case.backHomeVisible`。
- `permission_rules.json` 表示授权结果，`visible` 控制入口是否显示，`enabled` 控制动作是否可执行。
- MainExe 合并配置和权限后下发 UI：`最终可见 = 配置默认可见 && 权限可见`。后续高价值动作仍要由 Workflow / Service / IPC 复核 `enabled`。
- 等待页和单实例是固定启动流程，不读取 `runtime_config.json` 开关。
- 启动等待页由 UIComponents 创建；后续更复杂的启动检查仍放 MainExe 编排，不把检查逻辑写进等待页。
- 登录模块当前使用既有 `MeyerLoginWidget.dll`，后续可再包一层 `LoginAdapter` 降低第三方头文件编码和字段变化影响。
- HomeUI / CaseUI 只发入口或操作 ID，不直接切换其他模块；页面切换由 MainExe 的单内容区完成，一次只挂载一个全屏页面，首页和浏览不是并列兄弟页。
- 首页进入浏览、浏览返回首页、浏览进入扫描重建前准备都按“替换当前页面 + 释放离开页面资源”处理，避免隐藏页面长期占用内存/显存。
- UI 显隐和启用态统一由 MainExe 合并 ConfigCenter / Permission 后下发：`visible` 控制是否显示，`enabled` 控制是否可点击；MainExe 收到回调后还会按 `enabled` 二次复核。
- 运行时版本清单只记录 `config/version_modules.json` 中声明的拆分模块 EXE/DLL，不再扫描第三方库；字段说明见同目录 `config/version_modules.md`。
- `version_modules.json` 中声明的模块必须同步进入 MainExe Release 目录或后续安装包；缺失模块会在 `logs/versionList` 中记录为 `exists=false`，用于发现 PostBuild/打包漏复制。
- MainExe 自身工具栏、状态栏、等待页等界面可见文字统一使用 `tr("English source text")`；需求文案可写中文，但源码 source text 必须写英文。
- 首页、浏览页、等待页由 MainExe 按需创建；切换完成后释放非当前页面 widget，避免不可见模块长期占用资源。
- 页面释放使用 Qt 事件循环的安全释放方式，避免在按钮点击信号尚未返回时销毁当前 sender 所在控件树。
- 当前 `CaseActionOpenOrder` 已接入扫描前准备流程，作为后续加载订单、进入扫描重建的统一资源释放入口。
- 后续从案例管理进入 `ScanReconstructStudio.exe` 前，必须先执行 MainExe 的扫描前准备流程：切换到等待页、释放 CaseUI widget、处理 `deleteLater()`，再启动扫描重建进程；不能只隐藏案例管理界面。
- 后续扫描重建、三维显示、算法、相机和显存相关页面必须按“离开即释放或暂停重资源”的规则处理。
- 客户可触发的导航、按钮、页签、查询等操作必须写结构化日志；MainExe 记录跨模块导航和页面切换，UI 模块记录模块内操作。
- 登录离线许可文件统一放在 `Resources/license.lic`；后续 `Resources` 还会放图标、图片、多语言等资源。
- Release 输出目录会复制登录、首页、案例、数据库、日志模块及 Qt/VC/UCRT/OpenSSL/AWS 等运行依赖，作为后续安装包依赖清单参考。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_MainExe.sln /p:Configuration=Release /p:Platform=x64
```

## 运行验证

```powershell
cd F:\MeyerScan\MyMainExe\bin\Release
.\MeyerScan.exe --smoke
.\MeyerScan.exe --smoke-main
```

- `--smoke`：按正式流程初始化日志/数据库并拉起登录模块，3 秒后退出。
- `--smoke-main`：跳过登录，自动覆盖等待页、首页、浏览、返回首页、再次浏览、扫描前资源释放链路，3 秒后退出，用于验证主界面模块装载、页面切换和 CaseUI 释放。

## 已知风险

- 外部登录头文件当前存在 VS2015 代码页/声明警告，MainExe 暂只读取 `LoginReturnParameters.currentStatus`。
- 后续建议新增 `LoginAdapter`，把登录 DLL 的参数构造、状态转换、头文件兼容问题收口在单独边界内。
- 数据库配置只从运行目录 `config/db_config.json` 读取；缺失时只记录健康检查不可用，不回退到开发机绝对路径。
- 单实例在数据库检查或登录窗口阶段只做轻量激活尝试；如果主窗口尚未显示，不强制弹窗，避免干扰启动和登录流程。
