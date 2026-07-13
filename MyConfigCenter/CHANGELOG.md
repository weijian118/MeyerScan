# MeyerScan ConfigCenter 变更记录

## 2026-07-13

- `ConfigCenterTest` 使用命名 UTF-8 目录缓冲区调用 Init，补充跨 DLL 指针生命周期注释；配置字段和模块 ABI 不变。
- 测试已登记到根 CTest 清单，继续在独立测试目录验证默认配置生成。

## 2026-07-06

- 复查补漏：同步修正 `ConfigCenterImpl::EnsureDefaultConfig()` 的首次生成逻辑和 `ConfigCenterTest` 断言，避免代码仍生成旧 `mysql` 默认值。
- `ConfigCenterTest` 运行前会删除测试运行目录中的旧 `runtime_config.json`，确保真正覆盖默认配置生成链路。

## 2026-07-10

- 修正 `Init()` 注释中的旧 Core.lib 前置口径：待真实失败语义稳定后再升级返回类型；公共结果合同只在出现真实复用后按需抽取，本轮不修改 ABI。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `ConfigCenterTest.exe` 测试宿主中文注释，说明独立测试运行目录、默认配置生成、缺失配置默认值、字符串 buffer 返回和 Shutdown 清理流程。
- 本轮仅补充注释，不改变 ConfigCenter 配置读取逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；该日 CMake 未执行，2026-07-06 已使用 CMake 3.31.6 和 VS2015 x64 生成器完成根聚合 Release 补验证。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论修正非界面模块 Qt 边界：当前 Qt JSON/文件读取只作为内部实现细节，公共接口不暴露 Qt 类型；后续新增能力优先评估非 Qt 实现。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、配置说明和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `ConfigCenterImpl.cpp`：补充 UTF-8 路径转换、`QDir::mkpath/filePath`、JSON 解析、点号 key 解析、调用方缓冲区、默认配置生成和旧配置迁移写回等内部机制说明。
- 本轮只补充注释和文档记录，不改变配置读取、默认值或迁移逻辑。

## 2026-06-30

- `runtime_config.json` 默认数据库类型切换为 `sqlite`，MainExe 启动后默认调用 Database 的 SQLite 链路。
- README 明确当前默认值为 SQLite，MySQL 仅作为可切换能力保留。

## 2026-06-25

- 新增 `config/runtime_config.json` 默认模板，并在 VS2015 PostBuild 中复制到 Release `config/`；字段说明继续放在同级 `runtime_config.md`，JSON 内部不写注释。
- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；`GetModuleVersion()` 从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 新增 `config/runtime_config.md`，集中说明 `runtime_config.json` 字段含义、与 `permission_rules.json` 的关系，以及等待页/单实例不进入配置的原因。
- 明确配置 JSON 内部不写注释，人工说明写在同级 md 文件中。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_ConfigCenter"`，保证后续日志宏输出正确模块名。
- 在 `IConfigCenter::Init()` 注释中标明当前 `bool` 返回值只是骨架期过渡；Core.lib 落地后应迁移到 `ErrorCode` / `VoidResult` 并补齐失败原因。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：应用目录来源、config 目录创建、默认配置生成、JSON 缓存、旧 startup 段迁移清理、调用方缓冲区和默认值回退均增加关键说明。
- 增加旧配置迁移：如果已有 `runtime_config.json` 中残留 `startup.showWaitPage` / `startup.singleInstance`，初始化时自动移除 `startup` 段并写回文件。
- README 补充 `runtime_config.json` 与 `permission_rules.json` 的关系，以及 `database.type`、`feature.home.settingsVisible`、`feature.case.backHomeVisible` 字段含义。
- 固定启动流程不再进入 `runtime_config.json`：等待页显示和单实例控制由 MainExe 固定执行。
- 默认配置仅保留产品/客户默认策略，例如 `database.type`、`feature.home.settingsVisible`、`feature.case.backHomeVisible`。
- 补充接口、实现和关键 JSON 读取流程注释，说明 ConfigCenter 与 Permission 的职责边界。

## 2026-06-23

- 新增配置中心模块骨架。
- 支持从应用安装目录读取 `config/runtime_config.json`。
- 首次运行自动生成默认配置文件。
- 提供布尔、整数、字符串读取接口，后续预留加解密和配置迁移能力。

## 2026-07-03
- 新增 `ConfigCenterTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
