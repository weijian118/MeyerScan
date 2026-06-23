# MeyerScan MyLogin 变更记录

## 2026-06-22

- 新增 `MyLogin` 测试宿主模块。
- 接入既有 `MeyerLoginWidget.dll` 和 `MeyerLoginWidget.lib`。
- 使用 `D:\wj\My-wj\MyLogin\license.lic` 作为离线许可测试文件。
- 2026-06-23 补充：运行参数中的离线许可路径改为测试程序同级 `license.lic`，由 PostBuild 复制，避免运行时依赖开发目录。
- 默认登录地址设置为 `https://myscan.meyerop.com/login`。
- 默认分辨率缩放系数 `nfaktoW` / `nfaktoH` 为 `1.0`，默认语言为简体中文。
- 补充 `--smoke` 自动退出参数，用于验证登录窗口和依赖装载。
- Release PostBuild 补齐登录模块间接依赖，包括 Qt5Qmqtt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi 等。
- 验证通过：VS2015 Release x64 构建通过；`MeyerLoginTest.exe --smoke` 返回 0。
- 记录外部登录头文件编码警告，后续建议用 `LoginAdapter` 隔离第三方头文件变化。
