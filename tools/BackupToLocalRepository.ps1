# 本脚本必须保存为“带 BOM 的 UTF-8”，确保 Windows PowerShell 5.1 能正确解析中文注释。
param(
    [string]$SourceRoot = "F:\MeyerScan",
    [string]$BackupRoot = "F:\MeyerScan-Reposit",
    [string]$CommitMessage = "Local full backup: sync MeyerScan modules"
)

$ErrorActionPreference = "Stop"

function Resolve-OrCreateDirectory {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Invoke-RobocopyChecked {
    param(
        [string]$From,
        [string]$To,
        [string[]]$CopyArgs
    )

    & robocopy $From $To @CopyArgs
    $exitCode = $LASTEXITCODE

    if ($exitCode -gt 7) {
        throw "robocopy failed: $From -> $To, exit code $exitCode"
    }
}

function Remove-ExcludedBackupContent {
    param(
        [string]$Root,
        [string[]]$DirectoryNames,
        [string[]]$FilePatterns
    )

    # robocopy /MIR 不会删除被 /XD 或 /XF 排除的历史文件，因此镜像完成后还要
    # 主动清理备份仓库里过去遗留的第三方运行库、构建目录和运行现场目录。
    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
    $rootPrefix = $resolvedRoot + "\"

    # 先删最深层的排除目录，避免删除父目录后继续处理已经不存在的子目录。
    $excludedDirectories = Get-ChildItem -LiteralPath $resolvedRoot -Directory -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $DirectoryNames -contains $_.Name -and $_.Name -ne ".git" } |
        Sort-Object { $_.FullName.Length } -Descending
    $excludedDirectories = @($excludedDirectories)
    Write-Host "发现需要清理的历史排除目录：$($excludedDirectories.Count) 个"

    foreach ($directory in $excludedDirectories) {
        $fullPath = [System.IO.Path]::GetFullPath($directory.FullName)
        if (-not $fullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove directory outside backup root: $fullPath"
        }

        Remove-Item -LiteralPath $fullPath -Recurse -Force
    }

    # 再按文件名通配符清理第三方 DLL、现场数据库和 IDE/链接临时文件。
    # 只比较叶子文件名，与 robocopy /XF 的匹配语义保持一致。
    $excludedFiles = Get-ChildItem -LiteralPath $resolvedRoot -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object {
            $name = $_.Name
            $matched = $false
            foreach ($pattern in $FilePatterns) {
                if ($name -like $pattern) {
                    $matched = $true
                    break
                }
            }
            $matched
        }
    $excludedFiles = @($excludedFiles)
    Write-Host "发现需要清理的历史排除文件：$($excludedFiles.Count) 个"

    foreach ($file in $excludedFiles) {
        $fullPath = [System.IO.Path]::GetFullPath($file.FullName)
        if (-not $fullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove file outside backup root: $fullPath"
        }

        Remove-Item -LiteralPath $fullPath -Force
    }
}

$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$backup = Resolve-OrCreateDirectory $BackupRoot

# Safety check before robocopy /MIR. The backup path must not point to the
# source directory or to a child directory under the source tree.
if ($backup.TrimEnd('\') -ieq $source.TrimEnd('\')) {
    throw "BackupRoot must not equal SourceRoot."
}

if ($backup.StartsWith($source.TrimEnd('\') + "\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BackupRoot must not be inside SourceRoot."
}

if ($backup.Length -lt 8 -or $backup -match "^[A-Z]:\\$") {
    throw "BackupRoot looks unsafe: $backup"
}

$excludeDirs = @(
    ".git", ".vs", "obj", "build", "build_vs2015",
    "logs", "platforms", "sqldrivers",
    "plugins", "MySQL", "SQLite", "backup", "CMakeFiles"
)

$excludeFiles = @(
    "Qt5*.dll",
    "api-ms-win-*.dll",
    "msvcp*.dll",
    "msvcr*.dll",
    "vcruntime*.dll",
    "ucrtbase.dll",
    "ucrtbased.dll",
    "concrt*.dll",
    "libcrypto*.dll",
    "libssl*.dll",
    "libeay32.dll",
    "ssleay32.dll",
    "libcurl.dll",
    "libmysql.dll",
    "libmysqld.dll",
    "sqlite.dll",
    "sqlite3.dll",
    "opencv*.dll",
    "vtk*.dll",
    "pcl*.dll",
    "boost*.dll",
    "flann*.dll",
    "qhull*.dll",
    "OpenNI*.dll",
    "tbb*.dll",
    "aws-*.dll",
    "zlib*.dll",
    "zlib1.dll",
    "zlibwapi.dll",
    "q*.dll",
    "adt_null.dll",
    "auth.dll",
    "auth_test_plugin.dll",
    "ConfigMigration.dll",
    "coreSQLiteStudio.dll",
    "CsvExport.dll",
    "CsvImport.dll",
    "DbAndroid.dll",
    "DbSqlite*.dll",
    "guiSQLiteStudio.dll",
    "HtmlExport.dll",
    "JsonExport.dll",
    "mysql_no_login.dll",
    "PdfExport.dll",
    "Printing.dll",
    "RegExpImport.dll",
    "ScriptingTcl.dll",
    "semisync_*.dll",
    "Sql*.dll",
    "tcl86.dll",
    "validate_password.dll",
    "windowsprintersupport.dll",
    "XmlExport.dll",
    "*.iobj",
    "*.ipdb",
    "*.exp",
    "*.ilk",
    "*.sdf",
    "*.suo",
    "*.VC.db",
    "*.opendb",
    "*.user",
    "*.db",
    "*.sqlite",
    "*.sqlite3",
    "*.frm",
    "*.MYD",
    "*.MYI"
)

# 历史压缩归档不属于当前源码，但本地仓库已有该归档时必须保留，避免 /MIR 删除人工留存文件。
# 它不参与复制，也不进入后面的排除内容清理列表。
$robocopyArgs = @(
    "/MIR",
    "/R:1",
    "/W:1",
    "/NFL",
    "/NDL",
    "/NP",
    "/XD"
) + $excludeDirs + @("/XF", "*.rar") + $excludeFiles

# robocopy /MIR 即使配合 /XF 也可能删除目标中的额外文件。
# 这里先保护本地仓库已有的历史归档，镜像完成后恢复，避免备份脚本误删人工留存资料。
$preserveDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("MeyerScanBackupPreserve_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $preserveDirectory | Out-Null
$preservedFiles = @("MyDeviceManager.rar")
foreach ($fileName in $preservedFiles) {
    $sourceFile = Join-Path $backup $fileName
    if (Test-Path -LiteralPath $sourceFile) {
        Copy-Item -LiteralPath $sourceFile -Destination (Join-Path $preserveDirectory $fileName) -Force
    }
}

try {
    Invoke-RobocopyChecked -From $source -To $backup -CopyArgs $robocopyArgs
}
finally {
    foreach ($fileName in $preservedFiles) {
        $preservedFile = Join-Path $preserveDirectory $fileName
        if (Test-Path -LiteralPath $preservedFile) {
            Copy-Item -LiteralPath $preservedFile -Destination (Join-Path $backup $fileName) -Force
        }
    }
    Remove-Item -LiteralPath $preserveDirectory -Recurse -Force
}

# /MIR 与 /XF、/XD 同时使用时不会清理目标中早期提交留下的排除内容。
# 这里补做一次受路径校验保护的清理，保证本地仓库长期满足“只备份自研文件”。
Remove-ExcludedBackupContent -Root $backup -DirectoryNames $excludeDirs -FilePatterns $excludeFiles

$localIgnore = Join-Path $source "tools\LocalBackup.gitignore"
if (Test-Path -LiteralPath $localIgnore) {
    Copy-Item -LiteralPath $localIgnore `
              -Destination (Join-Path $backup ".gitignore") `
              -Force
}

if (-not (Test-Path -LiteralPath (Join-Path $backup ".git"))) {
    git -C $backup init | Out-Null
}

# Keep local Git identity repository-scoped, so this script does not change the
# user's global Git configuration.
if ([string]::IsNullOrWhiteSpace((git -C $backup config user.name))) {
    git -C $backup config user.name "MeyerScan Local Backup"
}

if ([string]::IsNullOrWhiteSpace((git -C $backup config user.email))) {
    git -C $backup config user.email "meyerscan-local-backup@example.local"
}

git -C $backup add -A

$status = git -C $backup status --short
if ([string]::IsNullOrWhiteSpace($status)) {
    Write-Host "No local backup changes to commit."
    exit 0
}

git -C $backup commit -m $CommitMessage
Write-Host "Local backup completed: $backup"
