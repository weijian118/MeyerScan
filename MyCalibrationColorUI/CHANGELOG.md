# MeyerScan CalibrationColorUI 变更记录

## 2026-07-13

- 测试宿主使用命名 `QByteArray` 向 DLL Init 传入路径，明确 UTF-8 指针生命周期；颜色校准占位流程和 ABI 不变。
- `CalibrationColorUITest` 已登记到根 CTest 清单，默认自动退出，`--show` 继续用于人工界面验收。

## 2026-07-10

- 界面样式迁入 `Resources/qss/calibration_color.qss`，CMake/VS2015 构建后复制到模块运行目录；源码通过公共 QSS/日志辅助函数加载，不再维护局部样式字符串。
- `Version.rc` 补齐版权详细信息字段；VS2015/CMake Release 构建通过。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `CalibrationColorUITest.exe` 测试宿主中文注释，说明 QApplication 初始化、路径来源、日志目录准备、颜色校准根控件创建、`--show` 人工查看模式和自动化释放流程。
- 本轮仅补充注释，不改变颜色校准 UI 模块运行逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步工程规则：本模块是 Qt 界面模块，可以继续使用 Qt Widgets；颜色校准 UI 与流程入口保持独立，但后续算法/设备重资源仍通过清晰接口调用，不把病例/订单业务写入校准模块。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `CalibrationColorUIImpl.cpp`：补充 Qt 父子对象、布局容器、色彩校准占位区、Start/Cancel 占位按钮、多语言 `tr()`、日志 UTF-8 ABI 和未来算法/设备资源释放边界说明。
- 本轮只补充注释和文档记录，不改变颜色校准 UI 骨架逻辑。

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；颜色校准日志 `[Mod:]` 字段和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_CalibrationColorUI"`，保证后续日志宏输出正确颜色校准模块名。

## 2026-06-24

- 对齐新版 Logger 规则：当前颜色校准页面没有真实操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段。
- 根据“初学者可读”要求补强函数体内部注释：路径缓存、Qt 父子关系、布局创建、tr 英文 source text、占位区、Start/Cancel 占位动作、Flush 和 Logger UTF-8 ABI 均增加关键说明。
- 补充 `ICalibrationColorUI` 公共接口、实现类和关键界面创建流程的中文注释，明确颜色校准 UI、算法接入和设备接入都收敛在本模块内部。
- 新增 `MyCalibrationColorUI` 模块骨架，输出 `MeyerScan_CalibrationColorUI.dll`。
- 新增 `ICalibrationColorUI` 接口，当前提供初始化、创建 Qt Widgets 页面、版本查询和释放入口。
- 明确颜色校准 UI、流程编排和颜色校正参数生成入口归本模块，后续算法 DLL、DeviceCmd、DeviceTransport 在本模块内部接入。
- 明确本模块可以使用 Qt 默认能力，边界重点是职责隔离和跨进程对象所有权。
- 验证 Release x64 构建通过，0 warning / 0 error。

## 2026-07-03
- 新增 `CalibrationColorUITest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
