# MeyerScan ConfigCenter

`MyConfigCenter` 是配置中心模块的第一版骨架，当前先用于打通主流程。

- 配置文件固定从 `MeyerScan.exe` 所在目录下的 `config/runtime_config.json` 读取。
- 禁止使用 `QDir::currentPath()` 作为运行路径，避免第三方软件拉起时工作目录错误。
- 当前提供 `GetBool` / `GetInt` / `GetString` 三类轻接口。
- 暂不做配置加解密，接口后续可接入 Crypto 模块。

当前默认配置项：

- `feature.home.settingsVisible`
- `feature.case.backHomeVisible`
- `database.type`
- `startup.showWaitPage`
- `startup.singleInstance`
