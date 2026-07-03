# MeyerScan VersionManager 变更记录

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步边界：当前版本清单能力已并入 MainExe，本模块作为历史骨架仍保持 manifest 驱动，不恢复目录全量扫描。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `VersionManagerImpl.cpp`：补充 manifest 驱动版本清单、为什么不扫描第三方库、`QFileInfo`、时间戳文件名、Windows 版本资源 API、`HIWORD/LOWORD` 版本号拆分和成员 `QByteArray` 返回路径生命周期说明。
- 本轮只补充注释和文档记录，不改变历史 VersionManager 骨架的 manifest 读取和 versionList 输出逻辑。

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

## 2026-07-03
- 新增 `VersionManagerTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
