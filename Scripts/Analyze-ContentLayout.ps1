[CmdletBinding()]
param(
    [string]$ProjectRoot = ".",
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return (Resolve-Path -Path $Path).Path
}

function Get-RelativeProjectPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    $baseUri = [System.Uri]((Resolve-NormalizedPath -Path $BasePath).TrimEnd('\') + '\')
    $targetUri = [System.Uri](Resolve-NormalizedPath -Path $TargetPath)
    $relativePath = $baseUri.MakeRelativeUri($targetUri).ToString().Replace("/", "\")
    return [System.Uri]::UnescapeDataString($relativePath)
}

function Get-FolderCategory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FolderName
    )

    $projectRoots = @(
        "Audio",
        "BluePrints",
        "Characters",
        "Datas",
        "Fx",
        "Maps",
        "Materials",
        "Meshes",
        "Sequences",
        "Textures"
    )

    $thirdPartyRoots = @(
        "Assets",
        "BunkerConstructor",
        "EasyGameUI",
        "LED_Generator",
        "References",
        "StarterContent",
        "StorageHouse"
    )

    $specialRoots = @(
        "Collections",
        "Developers",
        "Localization",
        "__ExternalActors__",
        "__ExternalObjects__"
    )

    if ($FolderName -in $projectRoots) {
        return "Project"
    }

    if ($FolderName -in $thirdPartyRoots) {
        return "ThirdParty"
    }

    if ($FolderName -in $specialRoots) {
        return "Special"
    }

    return "Unclassified"
}

function Format-MarkdownTable {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Rows,

        [Parameter(Mandatory = $true)]
        [string[]]$Columns
    )

    $header = "| " + ($Columns -join " | ") + " |"
    $separator = "| " + (($Columns | ForEach-Object { "---" }) -join " | ") + " |"
    $lines = @($header, $separator)

    foreach ($row in $Rows) {
        $cells = foreach ($column in $Columns) {
            $value = $row.$column
            if ($null -eq $value -or [string]::IsNullOrWhiteSpace([string]$value)) {
                ""
            }
            else {
                ([string]$value).Replace("|", "\|")
            }
        }

        $lines += "| " + ($cells -join " | ") + " |"
    }

    return $lines
}

function Add-MarkdownLines {
    param(
        [Parameter(Mandatory = $true)]
        [object]$List,

        [Parameter(Mandatory = $true)]
        [string[]]$Lines
    )

    foreach ($line in $Lines) {
        $List.Add($line)
    }
}

$projectRootPath = Resolve-NormalizedPath -Path $ProjectRoot
$contentRoot = Join-Path $projectRootPath "Content"
$sourceRoot = Join-Path $projectRootPath "Source"
$configRoot = Join-Path $projectRootPath "Config"

if (-not (Test-Path -Path $contentRoot -PathType Container)) {
    throw "Content directory was not found at '$contentRoot'."
}

$topLevelFolders = Get-ChildItem -Path $contentRoot -Directory | Sort-Object Name

$folderSummary = foreach ($folder in $topLevelFolders) {
    $files = @(Get-ChildItem -Path $folder.FullName -Recurse -File -ErrorAction SilentlyContinue)
    $childFolders = @(Get-ChildItem -Path $folder.FullName -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -First 6 -ExpandProperty Name)

    [PSCustomObject]@{
        Folder = $folder.Name
        Category = (Get-FolderCategory -FolderName $folder.Name)
        Files = $files.Count
        UAssets = @($files | Where-Object { $_.Extension -eq ".uasset" }).Count
        Maps = @($files | Where-Object { $_.Extension -eq ".umap" }).Count
        SampleChildren = if ($childFolders.Count -gt 0) { $childFolders -join ", " } else { "-" }
    }
}

$referenceFiles = @()
if (Test-Path -Path $sourceRoot -PathType Container) {
    $referenceFiles += Get-ChildItem -Path $sourceRoot -Recurse -Include *.h,*.cpp,*.cs -File
}

if (Test-Path -Path $configRoot -PathType Container) {
    $referenceFiles += Get-ChildItem -Path $configRoot -Recurse -Include *.ini -File
}

$pathReferenceRows = foreach ($file in $referenceFiles) {
    $matches = Select-String -Path $file.FullName -Pattern "/Game/[A-Za-z0-9_./-]+" -AllMatches
    foreach ($match in $matches) {
        foreach ($value in $match.Matches.Value) {
            $segments = $value.Split("/")
            $root = if ($segments.Length -ge 3) { $segments[2] } else { "" }

            [PSCustomObject]@{
                File = Get-RelativeProjectPath -BasePath $projectRootPath -TargetPath $file.FullName
                Line = $match.LineNumber
                AssetPath = $value
                Root = $root
            }
        }
    }
}

$rootReferenceSummary = $pathReferenceRows |
    Group-Object Root |
    Sort-Object Count -Descending |
    ForEach-Object {
        [PSCustomObject]@{
            Root = $_.Name
            References = $_.Count
        }
    }

$generatedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"

$markdownLines = New-Object System.Collections.Generic.List[string]
$markdownLines.Add("# Content Layout Audit")
$markdownLines.Add("")
$markdownLines.Add("- Generated: $generatedAt")
$markdownLines.Add("- Project root: " + '`' + $projectRootPath + '`')
$markdownLines.Add("")
$markdownLines.Add("## Top-Level Content Folders")
Add-MarkdownLines -List $markdownLines -Lines ([string[]](Format-MarkdownTable -Rows $folderSummary -Columns @("Folder", "Category", "Files", "UAssets", "Maps", "SampleChildren")))
$markdownLines.Add("")
$markdownLines.Add("## Hardcoded /Game Path Roots")

if ($rootReferenceSummary.Count -gt 0) {
    Add-MarkdownLines -List $markdownLines -Lines ([string[]](Format-MarkdownTable -Rows $rootReferenceSummary -Columns @("Root", "References")))
}
else {
    $markdownLines.Add("No hardcoded " + '`' + "/Game/..." + '`' + " paths were found in " + '`' + "Source" + '`' + " or " + '`' + "Config" + '`' + ".")
}

$markdownLines.Add("")
$markdownLines.Add("## Immediate Rules")
$markdownLines.Add("- Do not move assets with Windows Explorer. Use Unreal Editor moves so redirectors can be fixed.")
$markdownLines.Add("- Treat " + '`' + "BluePrints" + '`' + ", " + '`' + "Maps" + '`' + ", and " + '`' + "EasyGameUI" + '`' + " as code-sensitive roots because they are referenced from C++ or " + '`' + ".ini" + '`' + " files.")
$markdownLines.Add("- Treat " + '`' + "Collections" + '`' + ", " + '`' + "Developers" + '`' + ", " + '`' + "Localization" + '`' + ", " + '`' + "__ExternalActors__" + '`' + ", and " + '`' + "__ExternalObjects__" + '`' + " as special folders. They are not normal cleanup targets.")
$markdownLines.Add("- Consolidate top-level " + '`' + "Materials" + '`' + ", " + '`' + "Meshes" + '`' + ", " + '`' + "Textures" + '`' + ", and " + '`' + "Fx" + '`' + " only after identifying the owning gameplay domain or pack.")
$markdownLines.Add("")
$markdownLines.Add("## Path Reference Details")

if ($pathReferenceRows.Count -gt 0) {
    $detailRows = $pathReferenceRows | Sort-Object File, Line, AssetPath
    Add-MarkdownLines -List $markdownLines -Lines ([string[]](Format-MarkdownTable -Rows $detailRows -Columns @("File", "Line", "Root", "AssetPath")))
}
else {
    $markdownLines.Add("No path references were found.")
}

$report = $markdownLines -join [Environment]::NewLine

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $report
    exit 0
}

$resolvedOutputPath = $OutputPath
if (-not [System.IO.Path]::IsPathRooted($resolvedOutputPath)) {
    $resolvedOutputPath = Join-Path $projectRootPath $resolvedOutputPath
}

$outputDirectory = Split-Path -Path $resolvedOutputPath -Parent
if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and -not (Test-Path -Path $outputDirectory -PathType Container)) {
    $null = New-Item -Path $outputDirectory -ItemType Directory -Force
}

Set-Content -Path $resolvedOutputPath -Value $report -Encoding UTF8
Write-Host "Content layout report written to $resolvedOutputPath"
