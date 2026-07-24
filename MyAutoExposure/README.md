# MyAutoExposure

`MeyerScan_AutoExposure.dll` 为组六图自动曝光提供会话级纯 C ABI。当前版本只固定设备上下文、历史状态位置和 16 字节曝光命令输出合同，算法尚未实现。

当前调用 `MeyerAutoExposure_Calculate` 会明确返回 `NotImplemented` 且 `valid=0`，CaptureService 不会因此下发伪造曝光参数。后续算法必须在本模块内部按设备 Profile、场景和历史帧状态实现。

构建入口：VS2015 打开 `MeyerScan_AutoExposure.sln`；VSCode/CMake 使用 `vs2015-x64` 预设。离线测试为 `bin/Release/AutoExposureTest.exe --smoke`。
