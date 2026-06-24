# MeyerScan ConfigCenter 变更记录

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
