# MeyerScan SendUI

`MySendUI` 输出 `MeyerScan_SendUI.dll`，用于提供工作台最后一步“发送”页面。

发送页 QSS 源码归本模块 `Resources` 维护，正式运行由 `MeyerScan_UIResources.dll` 注册；STL/PLY/OBJ 等所有可见选项仍使用 `tr("English source text")`，不能因其是技术缩写而绕过多语言规则。

## 当前定位

- 本模块是 Qt Widgets UI 模块，可以使用 `QWidget`、Qt Layout、`QString` 和 JSON 解析。
- 本模块只负责发送页界面展示、上下文字段显示、按钮动作回调和日志记录。
- 本模块不直接访问数据库，不调用网络，不执行真实导出、压缩、邮件发送或上传。
- 真实发送业务后续应拆到 DataExport / Network / Workflow 等服务模块，由 MainExe 或工作流服务根据 `SendUIActionId` 编排。
- 通用按钮、输入框、下拉框和字段标签优先通过 `MeyerScan_UIComponents.dll` 创建；共享 UI 缺失时降级为本地控件样式。

## 页面内容

- 案例信息：`Name`、`Doctor`、`Order No.`、`Order Type`、`Clinic`、`Data Format`、`Note`。
- 发送动作：本地 `Export`、`Compress`，技工所 `Email Send`、`Upload`。
- 底部动作：`Previous` 返回 Process，`Finish` 完成流程。

## 接口约束

- `SetSessionContextJson(const char*)` 接收 UTF-8 JSON，只读取需要显示的字段。
- 上下文必须先完整解析再替换缓存；非法 JSON 返回 false 并保留上一份有效字段。程序填充下拉框时屏蔽信号，只有客户真实改变格式才上报 `SendUIActionDataFormatChanged`。
- 生产页面不提供测试患者、默认医生或本地订单号；缺失字段保持空值，由上游标准上下文负责提供真实数据。
- 动作回调只传 `int actionId`，不跨 DLL 传递 `QObject`、`QString` 或复杂业务对象。
- 可见文本源码必须使用 `tr("English source text")`。
- 路径必须来自 `Init(appDirUtf8, logDirUtf8)` 或 `QCoreApplication::applicationDirPath()`，禁止使用 `QDir::currentPath()`。

## 构建和测试

- VS2015：打开 `MeyerScan_SendUI.sln`，构建 `Release|x64`。
- CMake/VSCode：根目录 `F:\MeyerScan` 构建会自动包含本模块。
- CMake 构建会把 `MeyerScan_Logger.dll` 和 `MeyerScan_UIComponents.dll` 复制到 `SendUITest.exe` 输出目录，保证测试宿主不依赖开发机 PATH 或手工拷贝。
- 测试宿主：`SendUITest.exe`，覆盖有效/非法上下文、字段应用、程序填充不触发动作、真实格式变化和按钮动作 ID。
- 显示界面：`SendUITest.exe --show`。

## 维护记录要求

- `CHANGELOG.md` 使用中文记录。
- 代码注释使用中文，函数和关键实现技巧都要说明。
- GitHub 和本地仓库提交日志使用中文，描述变更内容尽量详细。
