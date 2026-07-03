# MeyerScan VersionManager

`MyVersionManager` 是运行时版本清单模块的历史骨架。

当前阶段版本清单生成逻辑已经并入 `MyMainExe`，由 `MeyerScan.exe` 启动时读取 `config/version_modules.json` 并写出清单。这样可以减少一个运行时 DLL 和一个调试点，先让主链路更轻。

- MainExe 当前输出到 `logs/versionList/versionList_yyyyMMdd_HHmmss.json`。
- 当前只记录 `version_modules.json` 中声明的 MeyerScan 拆分模块 EXE/DLL，不记录 Qt、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库。
- 历史 `MeyerScan_VersionManager.dll` 骨架也已改成同样的 manifest 规则，避免后续误用旧模块时重新扫描全部 DLL。

保留本模块目录的原因：

- 作为后续版本管理重新独立化的参考实现。
- 如果版本管理扩展到算法 DLL 哈希、签名校验、云端版本比对、自动更新策略，再恢复为独立 DLL。
- 当前不要在 MainExe 中继续扩展复杂版本策略；MainExe 只保留“启动生成运行目录版本清单”的轻逻辑。

## 测试入口
- VS2015：打开 `MeyerScan_VersionManager.sln`，构建并运行 `VersionManagerTest.exe`。
- CMake/VSCode：默认开启 `VersionManagerTest` 测试目标，可通过 `MEYER_BUILD_VERSIONMANAGERTEST` 控制。
- 测试宿主只验证本模块边界和必要依赖链路，测试配置/数据写在 exe 输出目录下。
