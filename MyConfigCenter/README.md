# MeyerScan ConfigCenter

`MyConfigCenter` 是配置中心模块的第一版骨架，当前先用于打通主流程。

- 配置文件固定从 `MeyerScan.exe` 所在目录下的 `config/runtime_config.json` 读取。
- 配置字段说明写在同级 `config/runtime_config.md`，JSON 内部不写注释。
- 禁止使用 `QDir::currentPath()` 作为运行路径，避免第三方软件拉起时工作目录错误。
- 当前提供 `GetBool` / `GetInt` / `GetString` 三类轻接口。
- 暂不做配置加解密，接口后续可接入 Crypto 模块。

当前默认配置项：

- `feature.home.settingsVisible`
- `feature.case.backHomeVisible`
- `database.type`

字段含义：

- `database.type`：产品/客户默认数据库类型，当前支持 `mysql` / `sqlite`，MainExe 读取后调用 Database 切换类型。
- `feature.home.settingsVisible`：首页“设置”入口的产品默认显隐，最终还要与 Permission 的 `home.settings.visible` 合并。
- `feature.case.backHomeVisible`：案例管理“返回首页”按钮的产品默认显隐，最终还要与 Permission 的 `case.backHome.visible` 合并。

`runtime_config.json` 与 `permission_rules.json` 的关系：

- `runtime_config.json` 是产品/客户默认策略，表示这个版本或客户包默认想怎么显示、默认用哪类数据库。
- `permission_rules.json` 是授权/权限结果，表示当前账号、设备、客户、时间、版本等条件下是否被允许看到或执行功能。
- UI 最终显隐由 MainExe 合并：`ConfigCenter 默认策略 && Permission 授权结果`。
- 高价值动作不能只靠 UI 显隐，后续 Workflow、Service、IPC 接收端仍必须复核 Permission。

固定启动流程不放入配置文件：

- 等待页固定显示，由 MeyerScan.exe 启动流程控制。
- 单实例固定启用，由 MeyerScan.exe 启动入口控制。
- 旧配置中如果残留 `startup.showWaitPage` 或 `startup.singleInstance`，ConfigCenter 初始化时会自动迁移清理该 `startup` 段，避免维护人员误以为可以配置关闭。
