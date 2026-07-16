# CMake 与 VSCode 构建

本模块是纯 C++ Windows x64 模块，不依赖 Qt、VTK、OpenCV 或数据库。所有源码路径均由模块目录推导，移动整个 `MyDeviceCmd` 文件夹后不需要修改绝对路径。

```powershell
cmake --preset vs2015-x64
cmake --build --preset vs2015-x64-release
ctest --preset vs2015-x64-release
```

不使用 preset 时：

```powershell
cmake -S . -B build/vs2015-x64 -G "Visual Studio 14 2015" -A x64 -DBUILD_TESTING=ON
cmake --build build/vs2015-x64 --config Release
ctest --test-dir build/vs2015-x64 -C Release --output-on-failure
```

`DeviceCmdTest --smoke` 使用模拟后端，因此单模块编译不需要真实设备或 DeviceTransport DLL。当前 smoke 覆盖协议表中的全部命令组，包括 416/382/72 字节固定数据、固化状态和固件分包应答。正式运行时，MainExe/PostBuild 必须把 `MeyerScan_DeviceTransport.dll` 复制到 MeyerScan.exe 同级目录，且调用方传递其绝对路径。

协议命令、响应码、结构体和实机验证状态见 `docs/ProtocolCommandCoverage.md`。固件升级接口只提供擦除进度和单个分包写入原语，完整升级策略由 `MyUpdate` 或后续升级宿主实现。
