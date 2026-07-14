# 修改记录

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
