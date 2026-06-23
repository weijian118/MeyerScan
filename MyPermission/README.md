# MeyerScan Permission

`MyPermission` 是权限模块的第一版骨架，当前用于打通功能显隐流程。

- 从 `MeyerScan.exe` 同级 `config/permission_rules.json` 读取权限规则。
- 当前只提供 `IsFeatureVisible()` 和 `IsFeatureEnabled()`。
- 暂不做授权文件加解密，后续接入 Crypto 和扫码授权流程。
- 当前先用于控制首页“设置”入口、浏览模块“返回首页”按钮等显隐。

注意：UI 隐藏不是安全边界。正式功能必须在 Workflow、Service、IPC 接收端继续复核权限。
