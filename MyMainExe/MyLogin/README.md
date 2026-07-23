# MeyerScan MyLogin

`MyLogin` 是连接既有 `MeyerLoginWidget.dll` 的测试宿主模块，用于先验证登录模块能够在新仓库结构下独立拉起界面。

- 登录 SDK 来源：仓库 `External/MyLoginSDK`，由公共 CMake/props 统一定位。
- 可通过 `MEYER_LOGIN_ROOT` 覆盖兼容 SDK 根目录，但目录必须包含 `include/lib/runtime`。
- 离线许可测试文件：运行目录 `Resources/license.lic`，由 PostBuild 从既有登录模块目录复制
- 默认登录地址：`https://myscan.meyerop.com/login`

当前只做登录模块连接验证。正式 `MeyerScan.exe` 复用同样的参数构造逻辑，并在登录成功后进入 HomeUI。

## 构建

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_MyLogin.sln /p:Configuration=Release /p:Platform=x64
```

## 运行验证

```powershell
cd F:\MeyerScan\MyMainExe\MyLogin\bin\Release
.\MeyerLoginTest.exe --smoke
```

`--smoke` 会拉起登录模块并在 3 秒后自动退出，用于验证 DLL、Qt 插件和运行库依赖是否完整。

## 注意事项

- `MeyerLoginWidget.dll` 依赖 `deviceAuthAndCrypto.dll`、`MeyerScanNetworkHelper.dll`、Qt5Core/Gui/Widgets/Network/Qmqtt、VC120/VC140/UCRT、OpenSSL、libcurl、AWS SDK、zlib/zlibwapi 等运行库。
- Release 输出目录需要复制登录 DLL、语言 qm、许可文件、Qt 插件、SQL 驱动和旧安装目录中的登录依赖。
- 当前 `nfaktoW` / `nfaktoH` 默认都为 `1.0`，`languageType` 默认简体中文。
- 外部登录头文件 `MeyerLoginWidget.h` / `globalLoginValue.h` 当前存在 VS2015 代码页警告，业务代码暂只依赖稳定的 `currentStatus` 字段。
