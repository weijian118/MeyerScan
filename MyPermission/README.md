# MeyerScan Permission

`MyPermission` 是权限模块的第一版骨架，当前用于打通功能显隐流程。

2026-07-02 评审后，Permission 按非界面模块管理：后续新增能力优先评估非 Qt 实现。当前 Qt JSON 读取规则文件只作为内部实现细节，公共接口保持 `const char*` / `bool`。

- 从 `MeyerScan.exe` 同级 `config/permission_rules.json` 读取权限规则。
- 权限字段说明写在同级 `config/permission_rules.md`，JSON 内部不写注释。
- 当前只提供 `IsFeatureVisible()` 和 `IsFeatureEnabled()`。
- 暂不做授权文件加解密，后续接入 Crypto 和扫码授权流程。
- 当前先用于控制首页入口和浏览模块“返回首页”按钮等显隐/启用态；`enabled=false` 已由 MainExe 下发到 UI 并在动作回调入口二次复核。

## 字段含义

`permission_rules.json` 当前示例：

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

- `visible`：控制入口是否显示。`false` 表示用户界面上不出现该入口。
- `enabled`：控制入口或动作是否可执行。`false` 表示入口可以显示为禁用态，或服务层拒绝执行。
- `featureId`：调用接口时使用点号路径，例如 `home.settings`、`case.backHome`。

## 与 ConfigCenter 的关系

- ConfigCenter 提供产品/客户默认策略，例如默认是否显示首页设置入口。
- Permission 提供授权结果，例如当前设备/账号/客户是否允许设置功能。
- MainExe 合并两者后再下发给 UI：`最终可见 = 配置默认可见 && 权限可见`。
- 后续 Workflow / Service / IPC 需要继续判断 `enabled`，不能只依赖 UI 是否隐藏。

注意：UI 隐藏不是安全边界。正式功能必须在 Workflow、Service、IPC 接收端继续复核权限。

## 测试入口
- VS2015：打开 `MeyerScan_Permission.sln`，构建并运行 `PermissionTest.exe`。
- CMake/VSCode：默认开启 `PermissionTest` 测试目标，可通过 `MEYER_BUILD_PERMISSIONTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
