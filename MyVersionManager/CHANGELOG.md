# MeyerScan VersionManager 变更记录

## 2026-06-25

- 历史 VersionManager 骨架改为读取 `config/version_modules.json`，只记录清单中声明的 MeyerScan 拆分模块 EXE/DLL；不再扫描运行目录所有 DLL，避免 Qt、OpenSSL、AWS、VC/UCRT 等第三方库进入运行时版本清单。
- 新增缺失模块记录：manifest 中声明但运行目录不存在的文件仍写入 `exists=false`，便于安装包阶段发现漏复制。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_VersionManager"`，保证后续如恢复独立 DLL 时日志宏输出正确版本清单模块名。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：应用目录、日志目录、历史同级 EXE/DLL 扫描逻辑、versionList 目录创建、时间戳文件名、Windows 版本资源读取和版本号拼装均增加关键说明；2026-06-25 起历史扫描逻辑已废弃，改为 `config/version_modules.json` 清单驱动。
- 当前阶段版本清单生成逻辑已并入 MainExe，减少运行时 DLL 数量和调试点。
- 本模块骨架暂保留为历史实现参考；后续若版本管理扩展到算法 DLL 哈希、云端比对、自动更新策略，可再恢复为独立模块。
- 补充接口和实现注释，记录版本清单能力边界。

## 2026-06-23

- 新增版本清单模块骨架。
- 历史初版支持扫描应用目录下的 EXE/DLL 文件；2026-06-25 已废弃该方式，改为读取 `config/version_modules.json`。
- 支持写出带时间戳的 JSON 版本清单到 `logs/versionList`。
