# 修改记录

## 2026-07-24 - 1.3.0 原始 B 包持续接收与诊断

- 新增 `MeyerDeviceTransport_ReceiveRawCapturePacket` 和流诊断结构，使一个调用结果严格对应一个 USB 异步完成槽的 B 包，不在传输层解析图像业务。
- DeviceTransport 内部保持 `queueDepth` 深度的 USB IN 请求环；完成槽复制给调用方后立即重新提交，第一版不增加第二个用户态原始包队列。
- 新增首次超时、连续两次超时、部分包、设备断连和 I/O 失败的独立结果与累计统计，供 CaptureService 转换为 UI 可消费事件。
- 当前适配机型的 Profile 显式传入 `queueDepth=64`；默认值不作为协议事实，后续差异只在对应机型 Profile 中修改。
- 公共 schema 升级为 `2`，整数 ABI 升级为 `2`，语义 API 升级为 `1.1.0`；代码/文件版本升级为 `1.3.0`。
- 无硬件 smoke 覆盖参数门禁、64 深度配置、原始包复制、超时和诊断合同；真实设备长时间连续采集仍待联调。

## 2026-07-23 - 采集方案文档同步（未修改代码）

- 记录持续取走 16384 字节 B 包的采集约束；`queueDepth` 允许跨越组六图边界，不能为了发送控制命令停止所有 USB IN 接收。
- 采集方案暂统一规定所有适配机型使用 `queueDepth=64`；本轮仅更新文档，代码默认值调整和实机验证列为后续任务。
- 明确本模块只负责 USB 和原始 B 包传输；单图解密、整组状态、自动曝光、图像排序、镜像和减黑图由上层采集/数据处理模块负责。
- 规定传输日志必须记录机型系列和 Profile，设备编号、设备型号有则记录，不解析或伪造缺失身份。
- 详细方案同步到 `F:\MeyerScan\Documents\设备相关\数据采集-原始图像预处理方案.md`。

## 2026-07-17 - 1.2.0

- 默认 `deviceIndex` 改为 `MEYER_DEVICE_TRANSPORT_AUTO_DEVICE_INDEX`，CyAPI 按枚举顺序尝试全部设备，避免其它 Cypress 设备占用索引 0 时漏检口扫设备。
- USB2 判定按 `bHighSpeed + BcdUSB [0x0200,0x0300)`，USB3 判定按 `bSuperSpeed + BcdUSB >= 0x0300`；未知速度不再被默认 `false` 误判为 USB3。
- 显式设备索引仍保留给诊断工具；自动模式和默认值加入无硬件 smoke，测试数更新为 32 项。
- 实机验证已确认自动枚举和 USB3 判断；命令测试按旧软件增加发送后 200 ms 间隔，当前设备仍未返回只读 `0xCD` 的 `0xCE` 响应。

## 2026-07-16 - 1.1.0

- 增加统一整数 ABI 导出 `GetMeyerModuleApiVersion()`，使 `MyDeviceCmd` 能在解析 Transport 函数表前完成版本门禁。
- 日志 DLL 改为按 `MeyerScan_DeviceTransport.dll` 自身目录的绝对路径加载，不再受第三方拉起时 current directory 影响。
- 公共 API 语义版本仍为 `1.0.0`；新增导出向后兼容，文件/代码模块版本提升到 `1.1.0`。

## 2026-07-14 - 1.0.0

- 将项目、DLL、公共符号和测试统一命名为 `MyDeviceTransport`、`MeyerScan_DeviceTransport.dll`、`MeyerDeviceTransport_*` 和 `DeviceTransportTest.exe`。
- 调整为 `include/src/test/thirdparty` 标准目录，删除重复 `Meyer_USB_FX3` 工程、生成目录、SDF、空 WinUSB/WiFi 占位和无调用的 AES/命令编码实现。
- 公共接口改为带 `structSize/schemaVersion/reserved` 的 C ABI，增加结果码、错误文本、模块名、API 版本和统一代码版本接口。
- 修复重复 Open、采集线程普通 bool 数据竞争、裸线程指针、完整帧队列无上限、IMU 重置重复消费和异常跨 DLL 边界等问题。
- 修复 CyAPI 发送超时未完成上下文、流缓冲空指针、队列深度无上限、异步槽位上下文校验和释放时复用修改后长度等问题。
- 移除 Eigen/OpenCV 绝对路径；IMU 改为固定内存四元数互补处理，增加零范数保护和时间步长限制。
- 增加可选 Logger 动态适配，关键连接、命令、流和采集动作写入统一日志。
- 重写测试宿主：默认无硬件 smoke；硬件枚举、命令、流和采集必须显式选择。
- 补齐 VS2015、CMake/VSCode、Version.rc、README 和中文实现注释，并接入根 CMake、CTest、总解决方案及版本清单。
- 增加采集尺寸、包数量、传输队列、超时和总内存预算校验，所有乘法使用 64 位中间值，避免异常参数触发整数回绕或巨量分配。
- 统一 `GetFrame` 为真正的非阻塞合同，无完整帧时立即返回 `NotReady`，由调用方决定轮询节奏。
- 增加可移植的 VSCode CMake 任务，并确保构建必需的 `CyAPI.lib` 不会被模块忽略规则漏掉。
- 按仓库规范增加 `ModuleInfo` 单一信息源；代码版本使用“模块名 + v版本 + 日期”，API 版本继续使用纯语义版本。
- 测试宿主对流包数量、采集尺寸和帧数增加上限，并复用 DLL 的安全末包推导，避免命令行异常值在测试进程内先发生整数回绕。

### 验证

- VS2015 x64 Debug/Release 编译。
- 单模块 CMake x64 Debug/Release 编译。
- `DeviceTransportTest --smoke`：30/30 通过，包含公共 ABI、异常采集参数和无硬件 IMU 数值稳定性。
- `MeyerScan_AllModules.sln` Release 全量构建通过；根 CMake/CTest 25/25 通过。
- 将模块复制到无仓库公共 CMake helper 的临时目录后，独立 configure/build/CTest 通过，验证工程移动后仍可构建。
- 真实设备测试：本轮未执行，不能据此认定硬件主链路完成。
