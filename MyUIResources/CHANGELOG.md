# 修改记录

## 2026-07-10

- 版本升级为 `v0.1.1`，收录 HomeUI 中英组合品牌 Logo，并明确“任一发布资源变化都要递增资源 DLL 版本”的规则。
- 新建 `MyUIResources`，输出 `MeyerScan_UIResources.dll` 和 `UIResourcesTest.exe`。
- 使用 qrc + `rcc -binary` + Windows `RCDATA`，避免把约 24MB 图片展开成庞大的 C++ 数组源码。
- 资源路径统一为 `:/MeyerScan/Modules/<模块名>/...`，资源源码继续归各 UI 模块维护。
- 增加自动资源清单脚本、VS2015/CMake 双构建入口、文件详细信息版本和独立测试宿主。
- 按统一版本规范修正 DLL 的公司名、产品名、文件说明和 Windows SDK 类型常量，并为 `UIResourcesTest.exe` 增加独立 `Version.rc`。
- 最终清单自动收集 608 个资源；VS2015/CMake 根构建、`UIResourcesTest.exe --smoke` 和 MainExe 资源加载链路均通过。
- 自动生成的 `resources/MeyerScanUiResources.qrc` 加入模块 `.gitignore`，仓库只维护资源源文件、生成脚本和工程规则。
