# MeyerScan 重构文档入口

本目录是重构文档的唯一维护位置，随源码提交 GitHub 并进入 `F:\MeyerScan-Reposit`。不再读取、同步或维护仓库外旧文档。

## 文档分工

| 文档 | 只负责 |
|---|---|
| `MeyerScan重构任务总览.md` | 产品范围、完整模块清单、关键流程、全局规则和开发优先级 |
| `MeyerScan架构设计与接口规范.md` | 分层、依赖、接口/数据/权限/UI/扫描/版本合同和禁止事项 |
| `MeyerScan重构开发进度跟踪.md` | 当前版本、真实成熟度、未闭环项、下一步和验收命令 |
| `MeyerScan重构-AI协作记录.md` | 会影响后续开发的关键决策索引 |
| `PowerShell开发与自动化脚本规范.md` | PowerShell 5.1、编码、路径、退出码、构建锁和备份安全 |

模块具体实现和逐次修改分别以公共头文件、源码、模块 README/CHANGELOG 和 Git 历史为准，不在全局文档重复。

## 阅读顺序

1. 先读任务总览，确认功能放在哪个模块以及不能放在哪里。
2. 再读架构规范，确认依赖方向和跨模块合同。
3. 查看进度文档，区分“骨架可用”“主链路可用”和“完成”。
4. 只在需要了解决策原因时查 AI 协作记录。
5. 修改目标模块前读取该模块 README/CHANGELOG 和公共头文件。

## 维护规则

1. 产品范围或模块清单变化：更新任务总览。
2. 边界或接口合同变化：更新架构规范。
3. 版本、成熟度或下一步变化：更新进度文档。
4. 重大方案取舍：在 AI 协作记录增加一条简短决策。
5. 普通代码、测试和样式修改只更新模块 CHANGELOG，避免全局文档再次膨胀。
6. 提交前运行受影响测试、MainExe smoke、注释安全检查和 `git diff --check`。
7. GitHub 提交后使用 `tools\BackupToLocalRepository.ps1` 整体备份，不单独备份某个模块。

## 快速验收

```powershell
cmake --build F:\MeyerScan\build --config Release
ctest --test-dir F:\MeyerScan\build -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File F:\MeyerScan\tools\CheckSourceCommentSafety.ps1
```

CTest 是快速回归，不替代三档 UI 截图、真实数据库迁移、设备/算法、长时间稳定性和安装升级验收。

---

> **文档版本**：v2.0（2026-07-14，更新为精简文档导航和防膨胀规则）
