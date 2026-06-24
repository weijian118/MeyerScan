# permission_rules.json 字段说明

`permission_rules.json` 是授权/权限结果配置，用来表达当前账号、设备、客户、时间、版本等条件下是否允许显示或执行某个功能。

## 与 runtime_config.json 的关系

- ConfigCenter 读取 `runtime_config.json`，表达产品/客户默认策略。
- Permission 读取 `permission_rules.json`，表达授权结果。
- MainExe 合并后下发给 UI。UI 模块只接收最终 `visible` / `enabled`，不直接读取权限文件。

## 当前结构

```json
{
    "features": {
        "home": {
            "settings": {
                "visible": true,
                "enabled": true
            }
        }
    }
}
```

## 字段

- `features`：功能树根节点。
- `visible`：是否显示入口。`false` 表示界面上不出现该入口。
- `enabled`：是否允许点击或执行。`false` 表示入口可以显示为禁用态，或由服务层拒绝执行。
- `featureId`：代码调用时使用点号路径，例如 `home.settings`、`case.backHome`、`case.browse`、`order.create`、`scan.practice`。

## 重要边界

UI 显隐不是安全边界。正式业务动作必须在 Workflow、Service、IPC 接收端继续复核 `enabled`，不能只靠按钮隐藏或禁用。
