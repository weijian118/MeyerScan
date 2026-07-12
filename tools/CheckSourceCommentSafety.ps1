# 本脚本必须保存为带 BOM 的 UTF-8，确保 Windows PowerShell 5.1 正确读取中文注释。
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [switch]$FailOnWarning
)

$ErrorActionPreference = "Stop"

# 这些目录不属于自研可维护源码，或只是构建生成物；跳过后可以避免扫描第三方大目录和误报生成代码。
$excludedDirectoryNames = @(
    ".git", ".vs", "bin", "obj", "build", "build_vs2015", "CMakeFiles",
    "ThirdParty", "third_party", "generated", "GeneratedResources", "MySQL", "SQLite"
)

# 注释换行风险只检查会被编译器或 PowerShell 解释的源码；Markdown/QSS 由各自工具验证。
$sourcePatterns = @("*.c", "*.cc", "*.cpp", "*.cxx", "*.h", "*.hpp", "*.ps1", "*.rc")
$errors = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

# 解析仓库根目录并验证 Git 可访问，避免在错误目录中扫描整块磁盘。
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path.TrimEnd('\')
$relativeFiles = & git -c core.quotepath=false -C $root ls-files --cached --others --exclude-standard -- $sourcePatterns
if ($LASTEXITCODE -ne 0) {
    throw "无法从 Git 获取待检查源码列表，退出码：$LASTEXITCODE"
}

foreach ($relativeFile in $relativeFiles) {
    # Git 统一返回正斜杠路径；转换成本机分隔符后再做 LiteralPath 文件访问。
    $normalizedRelativePath = $relativeFile.Replace('/', '\')
    $pathSegments = $normalizedRelativePath.Split('\')
    $isExcluded = $false
    foreach ($segment in $pathSegments) {
        if ($excludedDirectoryNames -contains $segment) {
            $isExcluded = $true
            break
        }
    }
    if ($isExcluded) {
        continue
    }

    $fullPath = Join-Path $root $normalizedRelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        continue
    }

    # 直接读取字节可以可靠判断 UTF-8 BOM；不能用 Get-Content 的平台默认编码推测文件格式。
    $bytes = [System.IO.File]::ReadAllBytes($fullPath)
    $hasUtf8Bom = $bytes.Length -ge 3 `
        -and $bytes[0] -eq 0xEF `
        -and $bytes[1] -eq 0xBB `
        -and $bytes[2] -eq 0xBF
    $hasNonAsciiByte = $false
    foreach ($byte in $bytes) {
        if ($byte -ge 0x80) {
            $hasNonAsciiByte = $true
            break
        }
    }

    $extension = [System.IO.Path]::GetExtension($fullPath).ToLowerInvariant()
    if ($extension -eq ".rc") {
        # VS2015 的 rc.exe 对 UTF-8 BOM/中文支持不稳定，版本资源文件固定使用纯 ASCII。
        if ($hasUtf8Bom -or $hasNonAsciiByte) {
            $errors.Add("$normalizedRelativePath：.rc 文件必须保持纯 ASCII，不能包含 BOM 或中文。")
        }
        continue
    }

    if ($hasNonAsciiByte -and -not $hasUtf8Bom) {
        $errors.Add("$normalizedRelativePath：包含非 ASCII 字符但缺少 UTF-8 BOM，VS2015/PowerShell 5.1 可能误读中文注释。")
    }

    # UTF-8 解码仅用于逐物理行规则检查；BOM 字符不会影响下面的正则判断。
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $lines = [System.Text.RegularExpressions.Regex]::Split($text, "`r`n|`n|`r")
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; ++$lineIndex) {
        $line = $lines[$lineIndex]
        $lineNumber = $lineIndex + 1

        # C/C++ 预处理会在删除注释前处理“反斜杠+换行”；因此 // 注释末尾的反斜杠会吞掉下一行代码。
        if ($extension -ne ".ps1" -and $line -match '//.*\\\s*$') {
            $errors.Add("${normalizedRelativePath}:${lineNumber}：// 注释末尾禁止反斜杠，否则下一物理行可能继续被注释。")
        }

        # 该模式只做提醒：长中文注释后若同行出现典型赋值/控制语句，可能是补丁换行丢失造成的粘连。
        # 自动工具无法完全理解自然语言中的代码示例，所以默认不把这一项当成硬错误。
        if ($extension -ne ".ps1" `
            -and $line -match '^\s*//.*[\u4e00-\u9fff].*\s{2,}(?:if\s*\(|for\s*\(|while\s*\(|return\s+|[A-Za-z_][A-Za-z0-9_:<>]*\s*=).+;\s*$') {
            $warnings.Add("${normalizedRelativePath}:${lineNumber}：注释后疑似粘连代码，请人工确认物理换行。")
        }
    }
}

foreach ($warning in $warnings) {
    Write-Warning $warning
}

if ($errors.Count -gt 0) {
    foreach ($errorMessage in $errors) {
        # 不使用 Write-Error：脚本启用了 Stop，Write-Error 会在第一项就中断，维护者看不到完整问题清单。
        Write-Host $errorMessage -ForegroundColor Red
    }
    Write-Host "源码注释安全检查失败：$($errors.Count) 个错误，$($warnings.Count) 个警告。"
    exit 1
}

if ($FailOnWarning -and $warnings.Count -gt 0) {
    Write-Host "源码注释安全检查失败：启用了 FailOnWarning，发现 $($warnings.Count) 个警告。"
    exit 2
}

Write-Host "源码注释安全检查通过：0 个错误，$($warnings.Count) 个警告。"
exit 0
