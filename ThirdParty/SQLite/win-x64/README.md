# SQLite x64 运行时目录

本目录只记录 `sqlite3.dll` 的放置规则，不提交第三方 DLL 本体。

VS2015 当前按 `x64 Release` 编译，运行时必须使用 x64 版 `sqlite3.dll`。请从 SQLite 官方 `sqlite-dll-win-x64` 包取得：

- `sqlite3.dll`
- `sqlite3.def`（可选，仅用于后续需要重新生成导入库时参考）

放置位置固定为：

```text
F:\MeyerScan\ThirdParty\SQLite\win-x64\sqlite3.dll
```

各模块 VS2015 PostBuild 会从该目录复制 `sqlite3.dll` 到自身 `bin\Release`。不要再从 `MyCaseManager\SQLite\sqlitestudio311\SQLiteStudio` 复制，因为该目录内的历史 `sqlite3.dll` 是 32 位 DLL，会导致 x64 程序 `LoadLibraryA("sqlite3.dll")` 失败。
