param(
    [string]$SourceRoot = "F:\MeyerScan",
    [string]$BackupRoot = "F:\MeyerScan-Reposit",
    [string]$RefactorDocsRoot = "",
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

$robocopyArgs = @(
    "/MIR",
    "/R:1",
    "/W:1",
    "/NFL",
    "/NDL",
    "/NP",
    "/XD"
) + $excludeDirs + @("/XF") + $excludeFiles

Invoke-RobocopyChecked -From $source -To $backup -CopyArgs $robocopyArgs

$localIgnore = Join-Path $source "tools\LocalBackup.gitignore"
if (Test-Path -LiteralPath $localIgnore) {
    Copy-Item -LiteralPath $localIgnore `
              -Destination (Join-Path $backup ".gitignore") `
              -Force
}

# Refactor documents live outside the source repository. When the caller
# supplies the path, copy Markdown snapshots into the local backup repository.
if (-not [string]::IsNullOrWhiteSpace($RefactorDocsRoot) -and (Test-Path -LiteralPath $RefactorDocsRoot)) {
    $docsTarget = Join-Path $backup "_RefactorDocs"
    New-Item -ItemType Directory -Path $docsTarget -Force | Out-Null

    Invoke-RobocopyChecked -From (Resolve-Path -LiteralPath $RefactorDocsRoot).Path `
                           -To $docsTarget `
                           -CopyArgs @("*.md", "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NP")
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
