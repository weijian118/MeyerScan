# MeyerScan MyLogin 变更记录

## 2026-07-23（总方案和根 CTest 接入）

- `MeyerLoginTest.vcxproj` 已加入 `F:\MeyerScan\MeyerScan_AllModules.sln`，打开总方案即可与其他模块一起使用 VS2015 Release x64 构建。
- 根 CMake 新增 `MeyerScan.LoginSDK` smoke，验证登录 SDK DLL、Qt 插件、qm、许可和运行依赖的复制与加载；正式版本清单仍只记录产品 DLL/EXE，不记录测试程序。

## 2026-07-23

- 登录测试宿主改用仓库 `External/MyLoginSDK` 和公共 `cmake/MeyerScanExternalSdk.props/.cmake`，移除 `D:\wj\My-wj\MyLogin` 编译与 PostBuild 路径。
- SDK 头文件、导入库、DLL、qm、许可及登录所需运行依赖统一随仓库保存；Qt 继续通过 `QT_ROOT/QTDIR/QtRoot` 选择，不复制第三方 Qt 开发包。

## 2026-07-10

- 测试宿主 `Version.rc` 补齐 `LegalCopyright` 文件详细信息字段；既有外部登录 DLL、qm、许可和登录参数链路保持不变。

## 2026-07-02

- 新增登录测试宿主 `CMakeLists.txt`，支持 VSCode/CMake Tools 与 VS2015 生成器构建；外部既有 `MeyerLoginWidget.dll/lib` 仍作为已交付依赖链接，不把登录源码纳入当前仓库重写。
- 按评审结论同步本地备份规则：测试宿主源码、工程文件、CMake 和自研测试产物随全部模块一起备份到 `F:\MeyerScan-Reposit`。

## 2026-07-01

- 按“实现技巧型注释”要求补强 `LoginHost.cpp`：补充登录模块信号槽连接、登录参数逐项赋值、`QString` 结构体不能 `memset`、`Resources/license.lic` 路径拼接、URL UTF-8 转换和 `QTimer::singleShot(0)` 延迟退出原因。
- 本轮只补充注释和文档记录，不改变既有登录 DLL 调用参数和 smoke 自动退出逻辑。

## 2026-06-25

- 离线许可运行路径同步改为 `Resources/license.lic`，与 MainExe 正式运行规则一致。
- 根据 `glm52` 工程一致性方向统一登录测试宿主 `Version.rc`：公司名、产品名、文件描述、Debug 标志、`FILEOS` 和 `FILETYPE` 与全项目版本资源规范保持一致。
- 在 VS2015 工程中补充 `MEYER_MODULE_NAME="MeyerScan_LoginTest"`，保证登录测试宿主日志可与正式 LoginAdapter 区分。

## 2026-06-24

- 根据“初学者可读”要求补强函数体内部注释：登录 DLL 信号连接、参数逐项赋值、离线许可路径、语言索引、AppPath、登录地址和 smoke 自动退出均增加关键说明。

## 2026-06-22

- 2026-06-24 复查补充：为 `LoginHost` 头文件、实现文件和 `MeyerLoginTest` 测试入口补充函数级中文注释，说明登录 DLL 调用参数、信号返回和 `--smoke` 自动退出用途。
- 新增 `MyLogin` 测试宿主模块。
- 接入既有 `MeyerLoginWidget.dll` 和 `MeyerLoginWidget.lib`。
- 使用 `D:\wj\My-wj\MyLogin\license.lic` 作为离线许可测试文件。
- 2026-06-23 补充：运行参数中的离线许可路径改为测试程序同级 `license.lic`，由 PostBuild 复制，避免运行时依赖开发目录；2026-06-25 又统一迁入 `Resources/license.lic`。
- 默认登录地址设置为 `https://myscan.meyerop.com/login`。
- 默认分辨率缩放系数 `nfaktoW` / `nfaktoH` 为 `1.0`，默认语言为简体中文。
- 补充 `--smoke` 自动退出参数，用于验证登录窗口和依赖装载。
- Release PostBuild 补齐登录模块间接依赖，包括 Qt5Qmqtt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi 等。
- 验证通过：VS2015 Release x64 构建通过；`MeyerLoginTest.exe --smoke` 返回 0。
- 记录外部登录头文件编码警告，后续建议用 `LoginAdapter` 隔离第三方头文件变化。
