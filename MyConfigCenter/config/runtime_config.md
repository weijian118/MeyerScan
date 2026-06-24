# runtime_config.json 字段说明

`runtime_config.json` 是产品/客户默认策略配置，只表达“默认怎么运行、默认显示哪些入口”，不表达授权结果。

## 与 permission_rules.json 的关系

- `runtime_config.json`：产品包或客户包的默认策略，例如默认数据库类型、默认是否显示某个入口。
- `permission_rules.json`：授权结果，例如当前账号、设备、客户、时间、版本条件下是否允许看到或执行功能。
- MainExe 合并两者后下发给 UI：`最终可见 = ConfigCenter 默认可见 && Permission 可见`。
- `enabled` 不放在 runtime_config 中，当前由 Permission 控制；后续高价值动作还需要 Workflow / Service / IPC 接收端复核。

## 当前字段

- `database.type`：数据库类型。当前支持 `mysql` 和 `sqlite`，MainExe 读取后调用 Database 切换类型。
- `feature.home.settingsVisible`：首页设置入口的产品默认显隐。
- `feature.case.backHomeVisible`：案例管理返回首页按钮的产品默认显隐。

## 固定流程不进入配置

等待页和单实例是 MainExe 固定流程，不允许用配置关闭，因此不写 `showWaitPage`、`singleInstance`。如果旧配置残留 `startup` 段，ConfigCenter 初始化时会迁移清理。
