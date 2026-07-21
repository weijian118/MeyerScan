# MeyerScan GitHub 提交与网络故障排查规范

## 1. 明确结论

### 是否必须开启 VPN

VPN 不是 GitHub 提交的必需条件。只要当前网络能够正常访问：

    github.com:443
    api.github.com:443

就可以不使用 VPN。

但是，根据本机最近一次实际验证：

- GitHub CLI 已登录 weijian118，Token 具有 repo 权限。
- git push 曾先返回认证问题，完成 gh auth setup-git 后认证配置已修正。
- 随后 GitHub HTTPS 连接出现 Connection was reset 和 Failed to connect to github.com port 443。
- gh api repos/weijian118/MeyerScan 也无法访问。

因此，对当前电脑和当前网络环境的明确建议是：提交 GitHub 前开启 VPN，并确认 VPN 确实能够访问 GitHub；如果关闭 VPN 后 443 端口也能连通，则可以关闭 VPN 提交。

VPN 只解决网络路由和连通性，不解决账号认证。认证仍由 GitHub CLI 或 Git Credential Manager 负责。

## 2. 之前遇到的问题

### 2.1 HTTPS 认证方式不一致

仓库远端使用：

    https://github.com/weijian118/MeyerScan.git

本机虽然已经通过 GitHub CLI 登录，但 Git 的 HTTPS Credential Manager 最初没有正确使用 GitHub CLI 凭据，因此出现：

    Invalid username or token
    Password authentication is not supported for Git operations

这不是代码、分支或仓库内容问题，也不能使用 GitHub 账号密码解决。

提交前固定执行：

    gh auth status
    gh auth setup-git

确认输出中包含当前账号、Active account: true 和 repo 权限后，再执行 git push。

不要把 Token 写进远端 URL、PowerShell 历史命令、脚本或提交日志。

### 2.2 网络断开和认证失败混淆

以下错误属于网络问题，不要反复修改 Token：

    Connection was reset
    Failed to connect to github.com port 443
    Could not resolve host
    Operation timed out

以下错误才优先检查认证：

    Invalid username or token
    Authentication failed
    Repository not found

判断顺序必须是：先检查网络，再检查认证，最后检查远端仓库和分支。

### 2.3 HTTP/2 或 VPN 连接不稳定

如果认证已确认有效，但 HTTPS 推送出现连接重置，可以只对本次命令尝试 HTTP/1.1：

    git -c http.version=HTTP/1.1 push origin master

这不是固定解决方案。若 443 端口本身不可达，切换 HTTP 版本没有意义，应先调整 VPN 或网络代理。

### 2.4 长时间等待被误认为 Git 卡死

Git push 可能在网络连接阶段等待较长时间。执行时应记录完整输出并等待命令结束，不要同时启动多个 push。

若需要确认是否仍在运行，另开 PowerShell 检查：

    Get-Process git,ssh -ErrorAction SilentlyContinue

确认没有正在执行的 Git 操作后，才能重试。

### 2.5 Clash 已运行但 Git 没有自动使用代理

VPN/Clash 客户端正在运行，不代表当前 PowerShell 或 Git 一定会自动走代理。本机实测出现过以下组合：

- Clash 进程正常运行。
- `127.0.0.1:7890` 正在监听。
- Git 没有持久化 `http.proxy/https.proxy` 配置。
- `Test-NetConnection github.com -Port 443` 直连失败。
- 普通 `git push` 失败，但显式使用本地代理后立即成功。

先确认本地代理端口确实存在：

    Get-Process | Where-Object { $_.ProcessName -match 'clash|v2ray|xray|sing|warp|vpn' }
    Get-NetTCPConnection -State Listen | Where-Object { $_.LocalPort -in 7890,7891,1080,10808 }

若确认 Clash HTTP 代理监听 `127.0.0.1:7890`，只对本次推送临时使用代理：

    git -c http.proxy=http://127.0.0.1:7890 -c https.proxy=http://127.0.0.1:7890 -c http.version=HTTP/1.1 push origin master

优先使用命令级临时配置，不要未经确认写入全局 Git 配置。代理端口可能随客户端或配置变化，必须以本机实际监听结果为准；没有监听时不要反复套用旧端口。

### 2.6 本地整体备份与 GitHub 不是同一个仓库

F:\MeyerScan 是 GitHub 主仓库，F:\MeyerScan-Reposit 是本地整体备份仓库。两者各自拥有 Git 历史和提交号，提交号不要求相同。

本地备份脚本会整体同步源码、工程、测试、文档和允许保留的自研构建产物；它会排除第三方库、日志、现场数据库、构建临时目录和 IDE 临时文件。

## 3. 标准提交顺序

### 3.1 修改完成后检查

    cd F:\MeyerScan
    git status --short
    git diff --check
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\CheckSourceCommentSafety.ps1 -RepositoryRoot F:\MeyerScan -FailOnWarning

按功能风险执行对应构建和测试：

    cmake --build F:\MeyerScan\build --config Release
    ctest --test-dir F:\MeyerScan\build -C Release --output-on-failure

涉及 VS2015 工程时，CMake 和 VS2015 必须串行执行：

    & 'C:\Program Files (x86)\MSBuild\14.0\Bin\amd64\MSBuild.exe' F:\MeyerScan\MeyerScan_AllModules.sln /p:Configuration=Release /p:Platform=x64 /m:1

### 3.2 暂存和确认

    git add -A
    git diff --cached --stat
    git diff --cached --check

确认暂存内容中没有：

- 第三方库和 Qt/VTK/OpenCV/PCL 运行库。
- 日志、现场数据库和用户数据。
- CMake/VS 中间目录。
- 未经确认的临时压缩包。
- 与本次功能无关的用户修改。

### 3.3 中文提交

提交标题和正文使用中文，标题说明主要变化，正文说明影响范围和验证内容。例如：

    git commit -m "完善患者订单读模型与主程序集成" -m "新增服务列表查询，MainExe 合并新旧快照；同步 CMake、VS2015、测试项目和重构文档；已通过根 CTest 与 VS2015 Release 构建。"

一个功能批次使用一个主提交。备份脚本修复、文档规范等确实独立的变更可以单独提交，但不要为了重试 push 重复提交相同代码。

### 3.4 配置 GitHub 认证

    gh auth status
    gh auth setup-git
    git remote -v

远端必须是预期仓库：

    https://github.com/weijian118/MeyerScan.git

### 3.5 推送

    git push origin master

推送前不需要执行 git pull，除非远端已经有其他人提交且本地不是最新。若远端确实有新提交，应先单独执行 git fetch、检查差异，再决定合并或变基，不能盲目覆盖远端。

### 3.6 同步本地仓库

GitHub 推送成功后或因网络暂时失败但主仓库已提交，都应同步本地整体仓库：

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File F:\MeyerScan\tools\BackupToLocalRepository.ps1 -SourceRoot F:\MeyerScan -BackupRoot F:\MeyerScan-Reposit -CommitMessage "整体备份当前 MeyerScan 源码、工程、测试和文档"

备份脚本已经处理排除规则、路径安全和中文提交。脚本需要完整执行到 Local backup completed，不能只看 robocopy 中间输出。

## 4. 网络检查顺序

### 4.1 先检查域名和 443

    Resolve-DnsName github.com
    Test-NetConnection github.com -Port 443

如果 DNS 或 TCP 443 失败，先开启或更换 VPN 或检查代理；此时不要反复执行 git credential，也不要修改代码。

### 4.2 再检查 GitHub CLI

    gh auth status
    gh api user --jq '.login'
    gh api repos/weijian118/MeyerScan --jq '.full_name'

如果 API 能访问但 Git push 认证失败，执行 gh auth setup-git 后重试。如果 API 也无法访问，说明仍是网络问题。

### 4.3 最后检查远端分支

    git ls-remote origin refs/heads/master
    git status -sb
    git rev-list --left-right --count origin/master...master

成功同步后，rev-list 应显示本地和远端没有差异，例如：

    0       0

## 5. 成功验收

主仓库：

    git status -sb
    git log -1 --oneline
    git ls-remote origin refs/heads/master

本地仓库：

    git -C F:\MeyerScan-Reposit status -sb
    git -C F:\MeyerScan-Reposit log -1 --oneline

两边都必须没有未提交修改。GitHub 主仓库的远端最新提交应与本地 master 指向同一提交；本地备份仓库因为是独立仓库，提交号可以不同，但最新备份时间和文件内容必须对应本次源码状态。

## 6. 禁止事项

- 不把 GitHub 账号密码当作 Git HTTPS 密码使用。
- 不把 Token 写入 remote URL、脚本、日志或 Markdown。
- 不在 VPN 不稳定时并行执行多个 push。
- 不使用 git reset --hard 清理网络或认证问题。
- 不因 GitHub 推送失败而回退已经通过测试的代码。
- 不把本地备份仓库的历史提交号强行改成 GitHub 提交号。
- 不把 D:\wj\重构文档重新作为当前文档来源；文档唯一来源仍是 F:\MeyerScan\Documents。

## 7. 本次经验结论

本次失败不是代码或 Git 提交内容问题，而是先后出现了两类独立问题：最初是 Git Credential Manager 没有接入 GitHub CLI 凭据，修复认证后又遇到当前网络无法连接 GitHub 443 端口。

以后固定采用：先测试 GitHub 网络，再执行 gh auth status 和 gh auth setup-git，然后一次性 push；push 失败仍保留本地主提交并执行本地整体备份。
