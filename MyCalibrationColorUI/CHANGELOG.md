# MeyerScan CalibrationColorUI 变更记录

## 2026-07-21 - 0.7.0 下位机版本快照

- 公共接口 ABI 升级为 6、设备上下文 schema 升级为 5，新增主控板/投图板版本值和读取状态副本。
- `SetDeviceContext` 强制校验主控板版本有效；投图板只接受 `Valid` 或 `NotRequired`，有效状态下版本字符串不能为空。
- 根控件增加 `mainBoardFirmwareVersion` 和 `projectionBoardFirmwareVersion` 只读属性，日志记录完整设备身份及两块板版本。
- 测试宿主使用 MyScan5 单主控板场景验证版本上下文；代码/CMake/Windows 文件版本升级为 0.7.0。

## 2026-07-20 - 0.6.0 完整设备检测记录

- 公共接口 ABI 升级为 5、设备上下文 schema 升级为 4，新增完整检测记录副本。
- `SetDeviceContext` 额外检查检测状态和 effective 编号/型号代码，失败、冲突或空兼容值不能创建颜色校准页面。
- 模块按值保存 D9/C7/CE 步骤状态、生产模式、reported/effective 身份和值来源，并写入结构化日志。
- 根控件 `deviceId/modelCode` 使用 effective 值，同时增加 reported 值、检测状态、生产模式和兼容标志动态属性，后续算法入口无需解析协议兼容规则。
- 测试宿主补充精确检测记录和根控件属性断言；代码版本、CMake 版本和 Windows 文件版本统一升级为 0.6.0。

## 2026-07-20 - 0.5.0 产品身份快照

- 公共接口 ABI 升级为 4、设备上下文 schema 升级为 3，增加产品系列、具体产品、协议 Profile、识别状态、证据和产品名称字段。
- `SetDeviceContext` 按值保存产品身份 POD，并在日志及根控件动态属性中记录产品结果，便于后续按 Profile 接入颜色校准设备参数。
- 本模块仍不解析 `0xD9/0xCE` 原始回包、不维护型号映射、不创建第二个 DeviceCmd/Transport 会话。
- 独立测试宿主改用 `mOS MyScan 5/mOS MyScan 5` 的一致设备编号/型号代码快照；CMake、代码版本和 Windows 文件版本统一升级为 0.5.0。

## 2026-07-20 - 0.4.0 设备身份快照扩展

- 公共接口 ABI 升级为 3，设备上下文 schema 升级为 2；在原保留空间加入 `modelCodeUtf8[32]`，结构总大小保持不变。
- 保存并记录 MainExe 注入的机器码、机型枚举、机型名称和 `0xCE` 原始机型标识，不解析设备原始回包，也不创建第二个 USB 会话。
- 根控件增加只读 `modelCode` 动态属性，独立测试宿主补充确定性机型原始标识并通过 smoke。

## 2026-07-17 - 0.3.0 设备快照接入

- 公共接口 ABI 升级为 2，新增 `SetDeviceContext`；只接收 MainExe 已验证的固定 POD 快照，不持有 DeviceCmd 句柄。
- `CreateWidget` 在没有设备快照、设备未打开、USB2 或未知型号时拒绝创建，防止绕过 SettingsUI 入口门禁。
- 根控件记录只读 `deviceModel/deviceId` 动态属性并输出型号、来源和设备编号日志，便于后续按机型接入采集参数。
- 独立测试宿主注入确定性 USB3/MyScan6 快照，继续支持 smoke、拖动和截图验证。

## 2026-07-17 - 标题栏拖动

- 增加自定义标题栏拖动：独立测试窗口可整体移动，SettingsUI 模态遮罩中只移动颜色校准面板，并限制面板不会拖出宿主可见区域。
- 移除 SettingsUI 对颜色校准面板的布局托管，改为打开时手动居中，使拖动位置不会被 Qt Layout 重置。

## 2026-07-16

- 版本升级为 `v0.2.0`，同步代码版本、CMake 项目版本和 Windows DLL 文件版本。
- 按 `D:\wj\pp\颜色校准` 参考图复原颜色校准弹窗：450x585 无边框面板、64px 自定义标题栏、400x400 方形预览、关闭 normal/hover 状态以及 Calibrate/Exit 操作区。
- 将 `init_image.png`、`close_b.png`、`close_h.png` 归档到模块 Resources，并由 `MeyerScan_UIResources.dll` 统一编译管理；源码不硬编码当前工作目录。
- 动态加载 `MeyerScan_UIComponents.dll` 创建主按钮，样式角色对齐浏览页 Search；加载失败时使用带相同语义属性的 Qt 按钮降级。
- 补充 Calibrate、Exit、右上角关闭、共享组件加载和资源加载日志；关闭逻辑只允许关闭独立窗口或带合同标记的设置遮罩宿主。
- 测试宿主新增 `--capture-screenshot <png>` 视觉验收模式，固定抓取不含外部阴影的 450x585 面板。
- smoke 测试新增预览区、关闭按钮、Calibrate/Exit 按钮和主按钮语义属性断言，防止界面退化为空骨架仍返回成功。
- 测试宿主新增 `--drag-test`，使用 Qt 鼠标事件验证标题栏拖动后的窗口位置变化。
- 修复双击 `CalibrationColorUITest.exe` 只闪现控制台、不显示界面的问题：无参数运行改为默认显示颜色校准窗口，`--smoke` 专用于自动化创建、校验、释放后退出。
- VS2015 和 CMake 测试宿主统一改为 Windows 子系统，人工运行不再附带 CMD 窗口；根 CTest 清单显式传入 `--smoke`，避免自动化回归阻塞。
- 人工测试路径补充 `aboutToQuit` 清理，窗口关闭时按顺序销毁根控件并调用模块 `Shutdown()`。
- 修复单模块 VS2015 解决方案引用 Logger GUID 却未包含 Logger 项目的问题；解决方案现在显式包含 `MeyerScan_Logger.vcxproj`，并保证 Logger、颜色校准 DLL、测试宿主按依赖顺序构建。

## 2026-07-15

- 版本升级为 `v0.1.1`；新增公共接口 ABI 版本导出，SettingsUI 获取校准接口前强制校验。

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
