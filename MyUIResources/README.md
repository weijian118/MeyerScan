# MyUIResources

## 模块职责

`MyUIResources` 输出 `MeyerScan_UIResources.dll`，集中承载当前各 UI 模块的 PNG、QSS 等只读发布资源。

- 资源源码仍放在所属模块的 `Resources` 目录中，便于模块独立维护和评审。
- 构建时由 `tools/GenerateResourceManifest.ps1` 自动扫描允许的资源类型并生成 qrc 清单。
- `resources/MeyerScanUiResources.qrc` 是可重复生成的构建中间文件，不提交仓库；全新检出后 VS2015/CMake 都会在调用 rcc 前创建它。
- Qt `rcc -binary` 生成二进制资源包，随后作为 Windows `RCDATA` 嵌入 DLL。
- DLL 初始化后使用 `:/MeyerScan/Modules/<模块名>/...` 访问资源。
- MainExe 正式发布目录不再需要散落的 `Resources/Modules/.../icon` 和 `qss` 文件。
- 当前构建清单自动收集 608 个 PNG/QSS/qm 等允许类型资源；数量由生成脚本输出和测试确认，不在业务代码中硬编码。

## 设计边界

本模块只负责只读资源注册，不创建 QWidget，不理解首页、浏览、建单或扫描业务，也不管理语言切换。

不拆分 `Icon.dll` 和 `Qss.dll`：两类资源具有相同的加载时机、发布版本和故障处理方式，拆成两个 DLL 会增加清单、加载和升级的原子性问题。后续只有在资源包明显过大或客户包需要独立裁剪时，才按业务域拆成多个资源包。

资源编译进 DLL 可以防止客户直接修改或误删单个 PNG/QSS，但不能作为加密或版权保护手段。熟悉 PE/Qt 资源格式的人仍可能提取资源；真正的完整性保障应由安装包文件清单、哈希校验和自动修复承担。

资源 DLL 是全部 UI 发布资源的单一产物。任一模块新增、删除或修改正式发布资源时，所属 UI 模块需要按功能变化递增版本，`MyUIResources` 也必须递增版本并重新生成清单；否则现场只能看到图片已变化，却无法从 versionList 判断资源包批次。后续可在 DLL 内再生成分模块资源哈希清单，减少人工核对。

## 构建与测试

- VS2015：打开 `MeyerScan_UIResources.sln`，构建 `Release|x64`。
- CMake：由根 `F:\MeyerScan\CMakeLists.txt` 聚合构建。
- 测试：运行 `bin\Release\UIResourcesTest.exe`，验证注册、QSS/PNG 读取、版本接口和注销生命周期。

脚本必须保存为带 BOM 的 UTF-8，并使用 Windows PowerShell 5.1 兼容语法。
