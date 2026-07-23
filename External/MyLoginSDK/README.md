# MeyerLoginWidget 外部 SDK 副本

本目录保存 MainExe 和 `MeyerLoginTest.exe` 可移植构建所需的既有登录 SDK 交付物，不包含登录模块源码。

## 目录

- `include`：登录 SDK 公共头文件。
- `lib`：VS2015 x64 导入库 `MeyerLoginWidget.lib`。
- `runtime`：登录 DLL、模块自带 qm 和 `Resources/license.lic`。
- `runtime/dependencies`：登录 DLL 直接依赖的既有网络、加解密、Qmqtt、OpenSSL、AWS、curl 和 zlib 运行文件。

Qt 5.6.3 开发包不放入本目录。CMake 按 `QT_ROOT`、`QTDIR`、仓库 `ThirdParty/Qt5.6.3`、默认安装目录的顺序查找 Qt；VS2015 通过 `cmake/MeyerScanScanThirdParty.props` 使用同样的覆盖思路。

## 路径覆盖

默认根目录是仓库相对路径 `External/MyLoginSDK`。其他电脑需要使用另一份兼容 SDK 时：

- CMake 设置 cache 变量 `MEYER_LOGIN_SDK_ROOT`，或设置环境变量 `MEYER_LOGIN_ROOT`。
- VS2015/MSBuild 设置 `MeyerLoginSdkRoot` 属性，或设置环境变量 `MEYER_LOGIN_ROOT`。

覆盖目录必须继续保留 `include/lib/runtime` 层级。禁止在模块工程中重新写入 `D:\wj` 等开发机绝对路径。

## 安全依赖说明

`runtime/dependencies/MeyerScanNetworkHelper.dll` 当前构建产物包含云端访问凭据，
因此禁止提交到 GitHub。该文件只保存在受控开发机、本地整体备份仓库和正式安装包
制作环境中，由发布流程在构建前放入上述目录。缺少该文件时可以完成源码编译，
但登录 SDK 的网络功能和完整运行 smoke 不能通过。

后续必须轮换已经嵌入旧 DLL 的凭据，并修改网络模块为运行时读取安全配置或短期令牌，
重新构建确认不再包含长期密钥后，才能重新评估其分发方式。禁止使用 GitHub 的
“允许密钥”链接绕过 Push Protection。

## 更新约束

替换 SDK 时必须同时更新头文件、导入库、DLL、qm 和运行依赖，并重新执行 `MeyerLoginTest`、MainExe smoke、CMake 全量构建和 VS2015 根方案构建。许可文件仅用于当前离线链路测试，正式安装包由打包模块按授权规则提供。
