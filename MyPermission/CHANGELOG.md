# MeyerScan Permission 变更记录

## 2026-07-10

- 修正 `Init()` 注释中的旧 Core.lib 前置口径：权限规则缺失、损坏、版本不兼容和验签失败等语义稳定后再升级返回类型；本轮不修改 ABI。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `PermissionTest.exe` 测试宿主中文注释，说明测试专用 `permission_rules.json` 写入、`visible` 与 `enabled` 区别、缺失功能默认值和权限缓存释放流程。
- 本轮仅补充注释，不改变 Permission 规则解析和权限判断逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论修正非界面模块 Qt 边界：当前 Qt JSON 读取规则文件只作为内部实现细节，公共接口保持 `const char*` / `bool`；后续新增能力优先评估非 Qt 实现。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake、权限配置说明和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `PermissionImpl.cpp`：补充授权文件路径推导、JSON 严格 bool 解析、`visible` / `enabled` 区别、默认全开放规则、不覆盖已有授权文件和 C ABI 工厂函数等说明。
- 本轮只补充注释和文档记录，不改变权限规则读取和判断逻辑。

## 2026-06-25

- 新增 `config/permission_rules.json` 默认模板，并在 VS2015 PostBuild 中复制到 Release `config/`；字段说明继续放在同级 `permission_rules.md`，JSON 内部不写注释。
- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；`GetModuleVersion()` 从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 新增 `config/permission_rules.md`，集中说明 `permission_rules.json` 字段含义、featureId 写法、`visible` 和 `enabled` 的区别。
- 明确权限 JSON 内部不写注释，人工说明写在同级 md 文件中。
- `enabled` 不再只是预留：MainExe 已将其下发给 HomeUI/CaseUI 设置禁用态，并在入口/动作回调后做二次复核。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_Permission"`，保证后续日志宏输出正确模块名。
- 在 `IPermission::Init()` 注释中标明当前 `bool` 返回值只是骨架期过渡；Core.lib 落地后应迁移到 `ErrorCode` / `VoidResult`，并区分规则缺失、损坏、版本不兼容和验签失败等原因。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：授权文件路径、默认全开放规则、visible/enabled 区别、JSON 逐级读取和已有授权不覆盖逻辑均增加关键说明。
- README 补充 `permission_rules.json` 字段含义：`visible` 控制是否显示入口，`enabled` 控制是否允许执行动作。
- README 补充 ConfigCenter 与 Permission 的关系：配置给默认策略，权限给授权结果，MainExe 合并后下发 UI，Workflow/Service/IPC 后续仍要复核。
- 补充接口、实现和关键 JSON 读取流程注释，明确 `visible` 控制入口是否显示，`enabled` 控制功能是否可执行。
- 重新说明 Permission 只负责授权结果，不替代 ConfigCenter 的产品默认配置，也不能只靠 UI 显隐作为安全边界。

## 2026-06-23

- 新增权限模块骨架。
- 支持从应用安装目录读取 `config/permission_rules.json`。
- 首次运行自动生成默认权限规则。
- 提供功能可见和可用判断接口，先用于 UI 显隐流程验证。

## 2026-07-03
- 新增 `PermissionTest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
