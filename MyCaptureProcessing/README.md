# MyCaptureProcessing

`MeyerScan_CaptureProcessing.dll` 是不依赖 Qt 和 USB SDK 的协议级图像标准化模块。

## 职责

- 按固定 B 包参数拼接单图和连续六图。
- 校验单图数据头、图序号和组同步状态。
- 保留 40 字节数据头并执行历史逆 S-Box 解密。
- 按组六图规则汇总开灯、长按和扫描头状态。
- 将原始顺序调整为黑/R/G/B/激光G/激光B，镜像相机 1，并执行饱和减黑图。
- 不负责 USB、线程、自动曝光、RGB888、颜色校准、AI 或 UI。

## 构建与测试

- VS2015：打开 `MeyerScan_CaptureProcessing.sln`，选择 `Release|x64`。
- VSCode/CMake：执行 `cmake --preset vs2015-x64`，再执行 `cmake --build --preset vs2015-x64-release`。
- 测试：运行 `bin/Release/CaptureProcessingTest.exe --smoke`。

公共边界只传递 `Common/include/MeyerCaptureTypes.h` 中的固定布局 POD 和调用方缓冲区，不跨 DLL 传递 STL、Qt 对象或异常。
