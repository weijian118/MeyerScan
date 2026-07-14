# CMake 与 VSCode 构建

本模块没有 Eigen/OpenCV/Qt 依赖，也没有开发机绝对路径。`CMakeLists.txt` 在根聚合工程和模块单独复制两种情况下都可使用。

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

输出固定在模块自己的 `bin/<Config>`，CMake 中间文件位于 `build/`。VSCode 通过 `CMakePresets.json` 和 `.vscode/tasks.json` 使用相同命令，不保存开发机绝对路径。

CyAPI.lib 是 x64 且使用静态 Release CRT，因此本模块 Debug/Release 均固定 `/MT`。不要增加 Win32 配置，也不要改成 `/MTd` 与该库混链。

`thirdparty/CyAPI/lib/x64/CyAPI.lib` 是可移植构建必需输入，不是本项目生成的 `.lib`。模块 `.gitignore` 对该文件做了明确例外；备份或提交时必须确认它没有被全局忽略规则遗漏。
