param(
    [string]$RepositoryRoot = "F:\MeyerScan",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

# qrc 前缀必须与 C++ 加载器和 Version.rc 使用同一份合同定义。
# 脚本只解析 ASCII 合同头中的字符串宏，不复制一份容易漂移的常量。
$contractPath = Join-Path $RepositoryRoot "Common\include\MeyerUiResourceContract.h"

function Read-ContractStringMacro {
    param(
        [string]$Path,
        [string]$MacroName
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "UI resource contract header not found: $Path"
    }

    $pattern = '^\s*#define\s+' + [Regex]::Escape($MacroName) + '\s+"([^"]+)"\s*$'
    foreach ($line in [System.IO.File]::ReadAllLines($Path, [System.Text.Encoding]::ASCII)) {
        $match = [Regex]::Match($line, $pattern)
        if ($match.Success) {
            return $match.Groups[1].Value
        }
    }

    throw "UI resource contract macro not found: $MacroName"
}

# 只纳入正式 UI 资源类型。
# 临时文件、快捷方式和运行数据即使误放进 Resources，也不会进入客户发布 DLL。
$allowedExtensions = @(".png", ".qss", ".svg", ".ico", ".jpg", ".jpeg", ".bmp", ".gif", ".qm")

# 把绝对文件路径转换为相对于 qrc 清单目录的可移植路径。
# Windows PowerShell 5.1 没有 Path.GetRelativePath，因此使用 Uri.MakeRelativeUri 实现。
function ConvertTo-PortableRelativePath {
    param(
        [string]$BaseDirectory,
        [string]$TargetPath
    )

    $basePath = [System.IO.Path]::GetFullPath($BaseDirectory).TrimEnd('\') + "\"
    $targetFullPath = [System.IO.Path]::GetFullPath($TargetPath)
    $baseUri = New-Object System.Uri($basePath)
    $targetUri = New-Object System.Uri($targetFullPath)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString())
}

$repository = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$contractPath = Join-Path $repository "Common\include\MeyerUiResourceContract.h"
$resourcePrefix = Read-ContractStringMacro -Path $contractPath `
                                           -MacroName "MEYER_UI_RESOURCE_QRC_PREFIX"
if (-not $resourcePrefix.StartsWith("/")) {
    throw "UI resource qrc prefix must start with '/': $resourcePrefix"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repository "MyUIResources\resources\MeyerScanUiResources.qrc"
}

$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $outputFullPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

# 资源归属仍由各模块自己的 Resources 目录决定。
# 资源 DLL 只在构建时聚合，不把源文件搬进 MyUIResources，避免双份维护。
$resourceModules = Get-ChildItem -LiteralPath $repository -Directory |
    Where-Object {
        $_.Name -ne "MyUIResources" -and
        (Test-Path -LiteralPath (Join-Path $_.FullName "Resources"))
    } |
    Sort-Object Name

$entries = @()
foreach ($module in $resourceModules) {
    $resourceRoot = Join-Path $module.FullName "Resources"
    $files = Get-ChildItem -LiteralPath $resourceRoot -File -Recurse |
        Where-Object { $allowedExtensions -contains $_.Extension.ToLowerInvariant() } |
        Sort-Object FullName

    foreach ($file in $files) {
        $relativeInsideModule = ConvertTo-PortableRelativePath -BaseDirectory $resourceRoot -TargetPath $file.FullName
        $relativeFromManifest = ConvertTo-PortableRelativePath -BaseDirectory $outputDirectory -TargetPath $file.FullName
        $entries += [pscustomobject]@{
            Alias = ($module.Name + "/" + $relativeInsideModule)
            Source = $relativeFromManifest
        }
    }
}

# 使用 XmlWriter 生成 qrc，避免手工字符串拼接漏掉中文文件名或 XML 转义。
$settings = New-Object System.Xml.XmlWriterSettings
$settings.Encoding = New-Object System.Text.UTF8Encoding($true)
$settings.Indent = $true
$settings.IndentChars = "    "
$settings.NewLineChars = "`r`n"

$writer = [System.Xml.XmlWriter]::Create($outputFullPath, $settings)
try {
    $writer.WriteStartDocument()
    $writer.WriteStartElement("RCC")
    $writer.WriteStartElement("qresource")
    $writer.WriteAttributeString("prefix", $resourcePrefix)

    foreach ($entry in $entries) {
        $writer.WriteStartElement("file")
        $writer.WriteAttributeString("alias", $entry.Alias)
        $writer.WriteString($entry.Source)
        $writer.WriteEndElement()
    }

    $writer.WriteEndElement()
    $writer.WriteEndElement()
    $writer.WriteEndDocument()
} finally {
    $writer.Dispose()
}

Write-Host "Qt resource manifest generated: $outputFullPath ($($entries.Count) files, prefix=$resourcePrefix)"
