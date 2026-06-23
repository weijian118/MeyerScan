# MeyerScan VersionManager

`MyVersionManager` 负责生成运行时版本清单。

- 启动时扫描 `MeyerScan.exe` 同级目录下的软件相关 `exe` / `dll`。
- 输出到 `logs/versionList/versionList_yyyyMMdd_HHmmss.json`。
- 当前只记录软件文件，后续可扩展算法 DLL、插件目录、哈希校验和云端版本比对。

该模块独立存在，避免版本控制、自动更新、安装打包的规则散落在 MainExe 中。
