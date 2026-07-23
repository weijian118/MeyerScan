# MeyerScan Calibration3DUI 变更记录

## 2026-07-23 - 三维采集上下文方案同步（未修改代码）

- 明确三维校准 UI 后续通过 CaptureService 获取采集结果，不直接创建 USB 会话或解析设备命令。
- 明确标定器连接检查仅属于三维校准且后续接入；当前不以三维校准完成状态阻止进入。
- 规定三维校准上下文必须记录机型系列和 Profile，设备编号/设备型号有则记录，并保留生产模式、固件、采集模式和扫描头。
- 详细方案同步到 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。

## 2026-07-15

- 版本升级为 `v0.1.1`；新增公共接口 ABI 版本导出，SettingsUI 获取校准接口前强制校验。

## 2026-07-13

- 测试宿主使用命名 `QByteArray` 向 DLL Init 传入路径，并补充“模块立即复制输入缓冲区”的实现注释；占位校准流程和 ABI 不变。
- `Calibration3DUITest` 已登记到根 CTest 清单，默认自动退出，`--show` 继续用于人工界面验收。

## 2026-07-10

- 界面样式迁入 `Resources/qss/calibration_3d.qss`，CMake/VS2015 构建后复制到模块运行目录；源码通过公共 QSS/日志辅助函数加载，不再维护局部样式字符串。
- `Version.rc` 补齐版权详细信息字段；VS2015/CMake Release 构建通过。

## 2026-07-05

- 新增统一 C ABI 版本函数 `GetMeyerModuleVersion()`，供 MainExe / VersionManager 生成运行时版本清单时读取 `codeVersion`；该函数只返回 `ModuleInfo::Version`，不创建业务对象。

## 2026-07-04

- 补充 `Calibration3DUITest.exe` 测试宿主中文注释，说明 QApplication 初始化、应用目录/日志目录推导、校准根控件创建、`--show` 人工查看模式和自动化模式资源释放流程。
- 本轮仅补充注释，不改变三维校准 UI 模块运行逻辑。
- 验证：根方案 `MeyerScan_AllModules.sln` Release x64 构建通过；本机未发现可用 `cmake.exe`，CMake 构建未能执行。

## 2026-07-02

- 新增模块 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建，同时保留原 VS2015 工程。
- 按评审结论同步工程规则：本模块是 Qt 界面模块，可以继续使用 Qt Widgets；校准 UI 与流程入口保持独立，但后续算法/设备重资源仍通过清晰接口调用，不把病例/订单业务写入校准模块。
- 模块纳入 `F:\MeyerScan-Reposit` 本地整体备份规则，随所有模块一起备份源码、工程文件、CMake 和自研产物。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `Calibration3DUIImpl.cpp`：补充 Qt 父子对象、布局容器、占位采集区、Start/Cancel 占位按钮、多语言 `tr()`、日志 UTF-8 ABI 和未来算法/设备资源释放边界说明。
- 本轮只补充注释和文档记录，不改变三维校准 UI 骨架逻辑。

## 2026-06-25

- 新增 `ModuleInfo::Name` / `ModuleInfo::Version` 统一模块信息来源；三维校准日志 `[Mod:]` 字段和 `GetModuleVersion()` 均从该结构读取，要求与 `MEYER_MODULE_NAME`、`Version.rc` 保持一致。
- 根据 `glm52` 建议统一 `Version.rc`：公司名、产品名和 `FileDescription` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_Calibration3DUI"`，保证后续日志宏输出正确三维校准模块名。

## 2026-06-24

- 对齐新版 Logger 规则：当前三维校准页面没有真实操作员上下文，日志 `operator` 字段传空字符串，由 Logger 省略 `Op` 字段。
- 根据“初学者可读”要求补强函数体内部注释：路径缓存、Qt 父子关系、布局创建、tr 英文 source text、占位区、Start/Cancel 占位动作、Flush 和 Logger UTF-8 ABI 均增加关键说明。
- 补充 `ICalibration3DUI` 公共接口、实现类和关键界面创建流程的中文注释，明确 MainExe 只嵌入 QWidget，不参与三维校准内部计算。
- 新增 `MyCalibration3DUI` 模块骨架，输出 `MeyerScan_Calibration3DUI.dll`。
- 新增 `ICalibration3DUI` 接口，当前提供初始化、创建 Qt Widgets 页面、版本查询和释放入口。
- 明确三维校准 UI、流程编排和计算入口归本模块，后续算法 DLL、DeviceCmd、DeviceTransport 在本模块内部接入。
- 明确本模块可以使用 Qt 默认能力，边界重点是职责隔离和跨进程对象所有权。
- 验证 Release x64 构建通过，0 warning / 0 error。

## 2026-07-03
- 新增 `Calibration3DUITest.exe` 最小自动测试宿主，覆盖模块初始化、核心接口、关闭流程和关键边界。
- 同步 VS2015 `.vcxproj/.sln` 与 CMake 测试入口，便于单模块调试和聚合构建。
- 测试配置和测试数据写入测试 EXE 输出目录，避免污染源码目录和正式发布配置。
