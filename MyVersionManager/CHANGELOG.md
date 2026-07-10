# MeyerScan VersionManager 变更记录

## 2026-07-10

- `Version.rc` 补齐 `LegalCopyright` 文件详细信息字段；历史骨架继续读取 `version_modules.json`，当前正式运行时版本清单仍由 MainExe 内置能力生成。

## 2026-07-06

- 历史 VersionManager 骨架默认清单同步 MainExe 当前 21 模块口径，补入 `ScanReconstructStudio.exe`、`MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll`，避免后续误用历史骨架时版本清单退回旧范围。
- README、头文件和实现注释同步明确运行时版本清单不记录 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库；第三方依赖后续只进入安装打包发布清单。
- 已在根聚合 CMake `Release` 构建中验证本模块和测试宿主可以随全工程编译通过；CMake 使用 `F:\Tools\CMakePython\cmake\data\bin\cmake.exe` 与 VS2015 x64 生成器。

## 2026-07-05

- 历史 VersionManager 骨架同步 MainExe 最新版本清单口径：`version_modules.json` 使用 schemaVersion=2，模块项支持 `file + versionFunction`。
- 新增统一版本函数读取逻辑：通过 `LoadLibraryW + GetProcAddress("GetMeyerModuleVersion")` 读取自研 DLL 的 `codeVersion`，不创建业务接口对象，不扫描第三方库。
- 输出 `fileVersion`、`codeVersion`、`versionMatch` 和 `codeVersionError`，文件名改为 `versionList_yyyyMMdd_HHmmss_zzz.json`。
- `VersionManagerTest.exe` 增加 schemaVersion、文件版本、代码版本和版本一致性校验。
- 验证：`MeyerScan_VersionManager.sln` Release x64 构建通过；`VersionManagerTest.exe` 返回 0。

## 2026-07-04

- 补充 `VersionManagerTest.exe` 测试宿主中文注释，说明测试 manifest 写入、版本清单输出目录、缺失模块条目验证、调用方路径推导和 Shutdown 清理流程。
- 本轮仅补充注释，不改变 VersionManager 版本清单生成逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步边界：当前版本清单能力已并入 MainExe，本模块作为历史骨架仍保持 manifest 驱动，不恢复目录全量扫描。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `VersionManagerImpl.cpp`：补充 manifest 驱动版本清单、为什么不扫描第三方库、`QFileInfo`、时间戳文件名、Windows 版本资源 API、`HIWORD/LOWORD` 版本号拆分和成员 `QByteArray` 返回路径生命周期说明。
- 本轮只补充注释和文档记录，不改变历史 VersionManager 骨架的 manifest 读取和 versionList 输出逻辑。

## 2026-06-25

- 历史 VersionManager 骨架改为读取 `config/version_modules.json`，只记录清单中声明的 MeyerScan 拆分模块 EXE/DLL；不再扫描运行目录所有 DLL，避免 Qt、VTK、OpenCV、OpenSSL、AWS、VC/UCRT 等第三方库进入运行时版本清单。
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
