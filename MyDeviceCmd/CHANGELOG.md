# MyDeviceCmd 变更记录

## 2026-07-21 - 0.7.0

- 根据旧有线软件实例新增 `0x12/0x13` 投图板版本命令，复用四字节“主版本/次版本/大端修订号”解析规则；`0x14/0x15` 明确命名为主控板版本。
- 颜色校准预检在设备身份识别后读取下位机版本：所有系列读取主控板，只有 `mOS MyScan` 读取投图板，其它系列标记 `NotRequired` 且不发送投图板命令。
- `MeyerDeviceStateSnapshot`、`MeyerDeviceCalibrationPreflight` 增加版本值、有效位和版本读取状态，失败返回稳定状态 15；主控板/投图板失败可分别诊断。
- 模拟后端和 `DeviceCmdTest --smoke` 覆盖 MyScan5 单板、mOS MyScan 双板及两块板超时分支；公共 schema/整数 ABI 升级为 5，语义 API 升级为 2.2.0。

## 2026-07-20 - 0.6.1

- 公共预检状态追加 `ProductionDeviceNumberRequired=14`，供工作流宿主表达“已识别生产设备但正式流程要求真实编号”的准入结果。
- DeviceCmd 继续只负责检测并完整保存 reported/effective 身份；创建流程是否允许进入由 MainExe/DeviceSessionHost 决定，底层不读取 UI 模式。练习和校准可使用带来源的兼容身份。

## 2026-07-20 - 0.6.0

- 按 `口扫设备型号读取流程梳理.md` 固化“连接/USB -> D4/D9 -> 必要时 C2/C7 -> CD/CE -> 产品目录”的检测编排。
- 新增 `MeyerDeviceDetectionRecord`，分别保存设备真实上报的 `reported*`、兼容流程采用的 `effective*`、值来源、生产模式、兼容标志以及 D9/C7/CE 步骤状态。
- 详细帧解析可区分无回包、非校验坏包、求和校验失败、`0xFFFF` 未初始化、业务值非法和响应命令不匹配；D9 只有校验失败进入生产模式，其他坏包直接返回回包异常。
- 生产模式通过 C2/C7 回包能力区分 mOS MyScan 与 mOS MyScan 5/6 候选；MyScan 5 与 MyScan 6 的进一步区分规则尚未确认，当前按 MyScan 5 兼容值处理并显式记录推断来源。
- CE 旧固件、坏包、校验失败、未初始化或型号值非法时保留具体诊断，同时使用独立兼容值继续旧设备流程；兼容值不再覆盖真实回包字段，也不再标成 `DeviceReported`。
- 增加 C7 与 CE 系列冲突门禁，修复兼容来源覆盖既有 Conflict 状态的问题。
- 模拟器和 smoke 覆盖 D9 无回包/坏包/非法值、生产模式 C7 两类候选、CE 精确/旧固件/未初始化/坏校验/坏包/非法值以及两类证据冲突。
- 代码版本和文件版本升级为 0.6.0，语义 API 升级为 2.1.0，公共 schema 和整数 ABI 升级为 4。

## 2026-07-20 - 0.5.0

- 将 `MeyerDeviceModel` 明确收敛为协议/硬件能力 Profile，新增独立的产品系列、具体产品型号、识别状态和识别证据公共枚举。
- 新增 `DeviceProductCatalog` 和 `MeyerDeviceProductIdentity` POD，登记 mOS MyScan 与 mOS MyScan 5 当前 8 个已知完整型号代码；MyScan 5 P2、MyScan 6 有线/无线先保留稳定产品 ID，型号代码待定。
- 删除按型号代码首位猜 `MyScan3/5/6` 的错误逻辑；完整代码 `62000020` 现在精确映射到 MyScan3 Profile 和 `mOS MyScan/SY-KS1000(P1)`。
- 将旧有线 `0xCE` 前 8 字节型号代码布局与 MyScan 6 Wireless 授权信息布局分开解析，并通过 `responseLayout` 明确记录实际分支。
- 设备编号前缀只形成系列候选，完整型号代码确定具体产品；前缀/型号代码冲突返回 `Conflict` 并阻止校准，设备编号未写入时继续读取型号代码。
- 新增纯目录 API `MeyerDeviceCmd_IdentifyProduct`，预检 POD 向 MainExe 传递产品身份；公共 schema/整数 ABI 升级为 3，语义 API 升级为 2.0.0。
- smoke 新增 8 个型号映射、系列级识别、未写号、冲突和完整预检分支；VS2015 Release 和相关 CTest 通过。

## 2026-07-20 - 0.4.0

- 新增 `0xD4` 机器码读取请求和 `0xD9` 机器码上传解析，公开 `MeyerDeviceCmd_ReadMachineCode`，同时返回 13 字节原始数字和固定 UTF-8 机器码。
- 颜色校准预检顺序调整为“连接 -> USB3 -> `0xD4/0xD9` 机器码 -> `0xCD/0xCE` 设备信息/机型”，新增稳定状态 `MachineCodeReadFailed=9`。
- `0xD4` 和 `0xCD` 在真实 DeviceTransport 后端发送后分别等待 100 ms 和 200 ms，再接收回包；模拟后端不引入测试延迟。
- `0xCE` 前 8 字节集中解析为 `modelCodeUtf8`；只有全部为十进制数字且能可靠映射时才写具体型号，禁止 UI 或宿主重复解析回包。
- `Unknown` 探测 profile 同时开放只读 MachineCode 和 DeviceSecurityInfo 能力；模拟预检也改用 Unknown 入口，避免已知型号能力表掩盖真实探测门禁错误。
- 模块内部解析可使用 `std::string`/字节容器，跨 DLL 仍只传固定 POD、固定 UTF-8 数组和原始字节；不向公共 ABI 暴露 STL 容器。
- 公共结构 schema 和整数 ABI 升级为 2，语义 API 升级为 1.3.0；DeviceCmd smoke 新增机器码成功、超时和预检顺序验证。
- 实机复测确认 Cypress 设备为 USB3，`0xD4/0xD9` 成功返回 13 位机器码；后续 `0xCD/0xCE` 仍超时，预检按预期保留机器码并返回 `DeviceInfoReadFailed(6)`，未进入型号成功分支。

## 2026-07-17 - 0.3.0

- 新增 `MeyerDeviceCmd_PrepareColorCalibration`：统一打开设备、读取 USB 速率、发送 `0xCD`、解析 `0xCE`，并通过 `MeyerDeviceCalibrationPreflight` 返回连接、USB2、型号未知等稳定状态。
- `MyScan3/5/5H/6` 共用当前 Cypress 探测链路；MyScan 6 Wireless 的连接方法尚未开发，显式指定时返回 `WirelessProbeUnsupported`，不伪装成功。
- `0xCE` 正式协议没有独立机型字段，只在 337 字节预留区存在明确型号标记时返回 `DeviceReported`；不按设备编号前缀猜测机型。
- 模拟后端增加未连接、USB2、无型号标记和 USB3/型号成功四个分支；成功预检保留唯一会话，失败预检主动关闭。
- 测试宿主新增 `--preflight-real` 只读实机模式，可直接验收颜色校准设备链路且不修改设备参数。
- 当前实机验证通过 Cypress/USB3 两层检查，`0xCD` 发送成功但 `0xCE` 接收超时；模块按预期返回 `DeviceInfoReadFailed` 并关闭会话，未伪造型号或继续进入校准。
- 模块代码版本升级为 `0.3.0`，公共语义 API 为 `1.2.0`，整数 ABI 门禁仍为 `1`。

## 2026-07-16 - 中文注释完善

- 补充公共 ABI、设备命令服务、协议编解码、动态 Transport 加载、模拟后端和测试入口的函数级中文注释。
- 补充大小端转换、固定长度校验、异常边界、锁保护、资源释放顺序和模拟状态机等关键实现技术说明。
- 补充每组协议测试的验证目的，帮助不熟悉设备协议的维护人员理解测试数据和断言原因。
- 本次仅增加注释和文档记录，不改变命令编码、状态机和公共 ABI 行为；注释安全检查和模块 smoke 均通过。

## 2026-07-16 - 0.2.0

- 按 `美亚无线口内扫描仪通讯协议-20250808.pdf` 补齐协议表中的 48 组 A 类命令覆盖。
- 新增机器码固化、控制器复位、相机参数读取/固化、在线开窗位置设置、颜色矩阵读取/固化、温度读取和帧率设置。
- 新增主板固件擦除进度和 256 字节分包烧写接口，校验设备返回的包序、总包数和实际数据长度。
- 新增相机 1/2 标定参数、颜色标定参数、设备授权信息和曝光参数的固定 POD 结构及读写接口。
- 固化类命令统一解析 `0xFF` 成功和 `0x00` 失败状态；原始命令接口同样经过型号能力门禁和采集中响应互斥保护。
- 新增确定性模拟响应和全量协议 smoke，覆盖固定长度、大小端、状态回复、分包确认和数据回读。
- 增加 `docs/ProtocolCommandCoverage.md`，明确协议覆盖、模拟验证和真实设备联调边界。
- 模块代码版本更新为 `0.2.0`，公共语义 API 版本更新为 `1.1.0`，整数 ABI 版本仍为 `1`。

## 2026-07-16

- 新建 `MyDeviceCmd`，输出 `MeyerScan_DeviceCmd.dll` 和 `DeviceCmdTest.exe`。
- 按无线口内扫描仪协议实现 A 类命令帧：`5A 33` 数据头、命令码、两字节大端长度、命令数据、两字节求和校验和可变零尾。
- 集中实现机器码、主板版本、电池、设备信息/期限、普通灯光、强制灯光、开始传图和停止传图命令。
- 建立 `MyScan 3`、`MyScan 5`、`MyScan 5H`、`MyScan 6`、`MyScan 6 Wireless` 型号目录；5H 复用 5 的协议配置但保留独立产品型号。
- 真实后端通过显式绝对路径动态加载 `MeyerScan_DeviceTransport.dll`，先检查 `GetMeyerModuleApiVersion()`，不链接 DeviceTransport import lib。
- 增加无硬件模拟后端，完整验证“打开 -> 查询状态 -> 开灯 -> 开始采集 -> 取帧 -> 停止采集”调用链；模拟结果明确只用于测试。
- 状态快照增加 `validFields`，区分未读取、读取失败和真实的 0 值；期限码暂以原始十六进制返回，后续由授权/加解密模块解释。
- 状态快照增加 `modelSource`；当前型号明确来自宿主 `modelHint`，避免 UI 误认为已完成设备自动识别。
- 按机型目录封装停止顺序，调用方不再散落维护 `if/else + sleep`。
- 增加 CMake、CMakePresets、VSCode 任务、VS2015 DLL/测试工程和中文注释。
