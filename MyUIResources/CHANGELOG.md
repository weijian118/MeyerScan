# 修改记录

## 2026-07-16

- 版本升级为 `v0.1.4`，同步 CMake、代码版本和 Windows DLL 文件版本。
- 资源清单从 608 项增加到 611 项，新增 MyCalibrationColorUI 的 `init_image.png`、`close_b.png`、`close_h.png`，并打包新版 `calibration_color.qss`。
- `UIResourcesTest` 增加颜色校准 QSS、预览图、关闭 normal/hover 图片和新版 QSS 内容断言，避免资源路径存在但发布 DLL 漏包。

## 2026-07-12

- 本轮资源内容未变化，版本保持 `v0.1.3`；`UIResourcesTest` 增加重复初始化幂等、注销、重新注册和重注册后资源仍可读验证，并补充生命周期实现注释。
- `UIResourcesTest.exe --smoke` 通过，确认测试不会把资源 DLL 的进程级注册状态误判为一次性状态。
- 版本升级为 `v0.1.3`，重新打包 MyOrderCreateUI v0.5.2 的五类型 hover QSS 和扫描流程稳定布局样式。
- 普通四种修复类型不再出现整块绿色 hover，种植体继续使用独立宽按钮状态；资源 DLL 与业务 DLL 同批更新，便于 versionList 定位现场样式版本。
- 版本升级为 `v0.1.2`，重新打包 MyOrderCreateUI v0.5.1 的治疗类型 hover/选中 QSS 和多分辨率布局样式。
- 资源清单继续由脚本自动生成，共收录 608 个允许类型资源；全冠、缺失牙、嵌体、贴面、种植体的 b/h 与 1x/2x 图标均可从资源 DLL 读取。
- 验证：根 VS2015 方案重新生成 qrc 并构建通过，`UIResourcesTest.exe --smoke` 返回 0，OrderCreateUI 根输出 smoke 和三档截图均使用新版资源 DLL。

## 2026-07-10

- 版本升级为 `v0.1.1`，收录 HomeUI 中英组合品牌 Logo，并明确“任一发布资源变化都要递增资源 DLL 版本”的规则。
- 新建 `MyUIResources`，输出 `MeyerScan_UIResources.dll` 和 `UIResourcesTest.exe`。
- 使用 qrc + `rcc -binary` + Windows `RCDATA`，避免把约 24MB 图片展开成庞大的 C++ 数组源码。
- 资源路径统一为 `:/MeyerScan/Modules/<模块名>/...`，资源源码继续归各 UI 模块维护。
- 增加自动资源清单脚本、VS2015/CMake 双构建入口、文件详细信息版本和独立测试宿主。
- 按统一版本规范修正 DLL 的公司名、产品名、文件说明和 Windows SDK 类型常量，并为 `UIResourcesTest.exe` 增加独立 `Version.rc`。
- 最终清单自动收集 608 个资源；VS2015/CMake 根构建、`UIResourcesTest.exe --smoke` 和 MainExe 资源加载链路均通过。
- 自动生成的 `resources/MeyerScanUiResources.qrc` 加入模块 `.gitignore`，仓库只维护资源源文件、生成脚本和工程规则。
