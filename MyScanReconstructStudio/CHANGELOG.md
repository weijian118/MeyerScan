# 变更记录

## 2026-07-13

- 补充顶部 Scan/Process/Back/工具按钮信号到页面切换或日志动作的内部实现注释；测试读取上下文失败时使用命名 UTF-8 路径缓冲区。
- EXE/DLL 行为和版本不变；`ScanReconstructStudio.exe --smoke` 已登记到根 CTest 清单。

## 2026-07-12

- EXE/DLL 双形态版本升级为 `v0.1.3`；`SwitchToStep()` 改为返回 bool，初始 Scan 页或目标页创建失败会向 Initialize/RunSmoke 传播，不再显示空壳后报告成功。
- 动态加载 ScanWorkflowUI/DataProcessUI 后检查子模块 `Init()` 和 `SetSessionContextJson()`；UTF-8 QByteArray 在跨 DLL 调用期间显式保持生命周期。
- Logger 初始化失败时无日志降级；窗口、DLL 单例、命令行上下文和 smoke 路径补充中文实现注释。
- VS2015/CMake Release、`ScanReconstructStudio.exe --smoke` 及 Scan/Process 联合测试通过。

## 2026-07-10

- 版本升级为 `v0.1.2`，修复独立 EXE smoke 在 Scan -> Process -> Scan 第二次创建 QVTK 页面时访问冲突的问题。
- 页面释放函数只排队 `deleteLater()`；smoke 和析构在完整切换调用返回后再处理 DeferredDelete，避免释放函数内部重入 QWidget/QVTK 析构。
- smoke 增加目标页面实际创建断言，不能只调用切换函数后直接判定成功。
- 删除 VS2015 PostBuild 对模块 `Resources` 散文件的复制；正式运行统一从 `MeyerScan_UIResources.dll` 读取 QSS，源码目录仅用于开发降级。
- 版本升级为 `v0.1.1`，独立 EXE 和嵌入 DLL 分别编译 `Version.rc` / `VersionDll.rc`，文件版本与代码版本保持一致。
- 新增 `MeyerScan_ScanReconstructStudio.dll` 和公开 `IScanReconstructStudio` 接口，同一套实现可生成可嵌入 QWidget，也可生成 `ScanReconstructStudio.exe` 独立运行。
- 新增 DLL VS2015 工程并加入单模块/根聚合解决方案；CMake 同时生成 EXE、DLL 和 smoke 入口。
- 界面样式和资源统一迁入模块 `Resources`，源码通过公共 QSS/资源函数加载。
- 嵌入工作台时由外层 OrderScanWorkspaceShell 负责品牌和步骤导航，后续接入必须避免双壳/双步骤条；独立 EXE 才拥有独立窗口显示形态。
- 最终回归中 `ScanReconstructStudio.exe --smoke`、ScanWorkflowUITest、DataProcessUITest 和 MainExe 工作台链路全部返回 0。

## 2026-07-06

- CMake 根聚合 `Release` 构建已通过，`ScanReconstructStudio.exe --smoke` 返回 0。
- MainExe 运行时版本清单已纳入本模块，记录 `fileVersion=0.1.0.0`、`codeVersion=ScanReconstructStudio v0.1.0`、`versionMatch=true`。
- MainExe 输出目录已补齐扫描 UI 所需 VTK/OpenCV 运行库复制，避免版本扫描或后续打开扫描工作区时因依赖缺失无法加载扫描 UI DLL。
- 修复新增源码在 VS2015 代码页下中文注释乱码导致的编译解析问题，源码注释临时改为稳定 ASCII，模块说明继续在 README/CHANGELOG 中使用中文。
- 将壳子内信号连接改为 `QObject::connect()`，保持非 QObject 辅助类和窗口类调用方式一致。
- VS2015 `Release|x64` 编译通过，生成 `ScanReconstructStudio.exe`。
- 运行 `ScanReconstructStudio.exe --smoke` 返回码为 0，验证动态加载扫描 UI / 数据处理 UI、页面创建、阶段切换和资源释放链路。
- CMake 工程规则已完成实际配置和构建验证。

## 2026-07-05

- 新增 `ScanReconstructStudio.exe` 初版壳子。
- 新增动态加载 `MeyerScan_ScanWorkflowUI.dll` 和 `MeyerScan_DataProcessUI.dll` 的流程。
- 新增扫描/数据处理阶段切换，切换前释放离开页面的 VTK/QVTK 重资源。
- 新增 `--smoke` 自检入口，用于验证壳子动态加载、页面创建和切换释放链路。
