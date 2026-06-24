# version_modules.json 字段说明

`version_modules.json` 用来声明 MeyerScan 启动时需要写入 `logs/versionList/versionList_时间戳.json` 的拆分模块文件。

## 设计规则

- 只记录 MeyerScan 拆分出来的模块 `exe` / `dll`，不记录 Qt、OpenSSL、AWS、VC/UCRT、SQL 驱动等第三方库。
- 新增模块后，优先维护本文件的 `modules` 数组，不修改 MainExe 的扫描代码。
- 新增模块加入清单后，必须同步修改 MainExe PostBuild 或后续安装包脚本，把该 DLL/EXE 复制到运行目录。
- 文件名按 `MeyerScan.exe` 所在目录解析，不允许写开发机绝对路径。
- 缺失文件也会写入版本清单，并标记 `exists=false`，便于发现安装包漏复制。

## 字段

- `description`：给维护人员看的简短说明，程序不依赖该字段。
- `modules`：字符串数组，每一项是需要记录版本的模块文件名。

## 输出关系

MainExe 启动后读取本清单，再读取每个文件的 Windows 版本资源、大小、修改时间，输出到 `logs/versionList`。后续如果版本控制扩展到哈希、签名、云端比对或算法 DLL，再考虑恢复独立 VersionManager 模块。
