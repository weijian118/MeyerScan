# MyExternalLaunchAdapter 修改记录

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充自研源码中文注释，说明 Qt JSON 解析、调用方缓冲区、第三方类型分流、标准上下文组装和测试路径解析等实现技巧。
- 新源码保存为 UTF-8 BOM，避免 VS2015 按代码页 936 误读中文注释。
- README 补充 MainExe 调用方式和“客户只看到 OrderScanWorkspaceShell/OrderCreateUI，不看到首页闪现”的集成要求。
- 新增 `MeyerScan_ExternalLaunchAdapter.dll` 第三方拉起适配模块。
- 新增 `IExternalLaunchAdapter` 接口和 `GetExternalLaunchAdapter()` C ABI 工厂函数。
- 新增 `NormalizeOrderFile()`，支持读取第三方 JSON 文件并输出标准建单上下文 JSON。
- 标准建单上下文中增加 `source.thirdPartyType`、`source.thirdPartyName`、`source.sourceSystem`、`source.sourceVersion`，用于记录多个第三方来源和后续映射规则分流。
- 新增 `ExternalLaunchAdapterTest.exe` 测试宿主和 `test/external_order_sample.json` 样例文件。
- 新增 VS2015 工程、CMakeLists.txt、版本资源和模块 README。
- 验证：单模块 `MeyerScan_ExternalLaunchAdapter.sln` Release x64 构建通过；单模块输出和根输出目录的 `ExternalLaunchAdapterTest.exe` 均返回 0。


