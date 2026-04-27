[CmdletBinding()]
param(
    [string]$ProjectRoot = ".",
    [string]$OutputDir = ".\Docs"
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

function Get-AssetCounts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -Path $Path -PathType Container)) {
        return [PSCustomObject]@{
            Exists = $false
            Files = 0
            UAssets = 0
            Maps = 0
        }
    }

    $files = @(Get-ChildItem -Path $Path -Recurse -File -ErrorAction SilentlyContinue)

    return [PSCustomObject]@{
        Exists = $true
        Files = $files.Count
        UAssets = @($files | Where-Object { $_.Extension -eq ".uasset" }).Count
        Maps = @($files | Where-Object { $_.Extension -eq ".umap" }).Count
    }
}

$projectRootPath = Resolve-NormalizedPath -Path $ProjectRoot
$contentRoot = Join-Path $projectRootPath "Content"
$outputRoot = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $projectRootPath $OutputDir }

if (-not (Test-Path -Path $contentRoot -PathType Container)) {
    throw "Content directory was not found at '$contentRoot'."
}

if (-not (Test-Path -Path $outputRoot -PathType Container)) {
    $null = New-Item -Path $outputRoot -ItemType Directory -Force
}

$moveRules = @(
    [PSCustomObject]@{ Batch = "00-Setup"; Source = "Content"; Target = "/Game/Pickpacker and /Game/ThirdParty"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Create target roots"; Notes = "Create both roots in Unreal Editor before any moves." }

    [PSCustomObject]@{ Batch = "01-Safe"; Source = "Content/Audio"; Target = "/Game/Pickpacker/Audio"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Small project-owned audio root." }
    [PSCustomObject]@{ Batch = "01-Safe"; Source = "Content/Fx"; Target = "/Game/Pickpacker/VFX"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Project VFX root. Keep separate from third-party VFX packs." }
    [PSCustomObject]@{ Batch = "01-Safe"; Source = "Content/Sequences"; Target = "/Game/Pickpacker/Cinematics"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Project sequences only." }
    [PSCustomObject]@{ Batch = "01-Safe"; Source = "Content/Characters"; Target = "/Game/Pickpacker/Characters"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Small root. Validate mannequin dependencies after move." }

    [PSCustomObject]@{ Batch = "02-Project"; Source = "Content/Datas"; Target = "/Game/Pickpacker/Core/Data"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Centralize project data assets." }
    [PSCustomObject]@{ Batch = "02-Project"; Source = "Content/Materials"; Target = "/Game/Pickpacker/Core/Shared/Materials"; Risk = "Medium"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move selectively"; Notes = "Move only project-owned assets. Some entries already have CoreRedirects." }
    [PSCustomObject]@{ Batch = "02-Project"; Source = "Content/Meshes"; Target = "/Game/Pickpacker/Core/Shared/Meshes"; Risk = "Medium"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move selectively"; Notes = "Mixed content. Validate robot asset redirects and hardcoded paths." }
    [PSCustomObject]@{ Batch = "02-Project"; Source = "Content/Textures"; Target = "/Game/Pickpacker/Core/Shared/Textures"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move selectively"; Notes = "Move only textures still owned by project systems." }

    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/Inputs"; Target = "/Game/Pickpacker/Core/Input"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "No"; Action = "Move"; Notes = "Referenced from BlasterCharacter.cpp and BlasterPlayerController.cpp." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/UI"; Target = "/Game/Pickpacker/Core/UI"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "No"; Action = "Move"; Notes = "Referenced from player controller and NPC dialogue code." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/HUD"; Target = "/Game/Pickpacker/Core/UI/HUD"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "No"; Action = "Move"; Notes = "Referenced from interaction and HUD loading code." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/GameModes"; Target = "/Game/Pickpacker/Core/GameModes"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move"; Notes = "DefaultEngine.ini references BP_PickPackerGameMode." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/Data"; Target = "/Game/Pickpacker/Core/Data/Blueprints"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move"; Notes = "DefaultEngine.ini contains redirects to this root." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/AI"; Target = "/Game/Pickpacker/Gameplay/AI"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "No direct hardcoded path found, but behavior trees may reference assets internally." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/Weapon"; Target = "/Game/Pickpacker/Gameplay/Weapons"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Validate spawned projectile classes after move." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/Parcel"; Target = "/Game/Pickpacker/Gameplay/Parcel"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Project gameplay domain root." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/Station"; Target = "/Game/Pickpacker/Gameplay/Stations"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Project gameplay domain root." }
    [PSCustomObject]@{ Batch = "03-Blueprints"; Source = "Content/BluePrints/GamePlay"; Target = "/Game/Pickpacker/Gameplay/Systems"; Risk = "High"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move selectively"; Notes = "Contains prior redirect targets in DefaultEngine.ini." }

    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/Assets"; Target = "/Game/ThirdParty/Assets"; Risk = "Medium"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move pack-by-pack"; Notes = "Very large mixed third-party root. Robot assets are referenced by redirects." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/BunkerConstructor"; Target = "/Game/ThirdParty/BunkerConstructor"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Standalone asset pack." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/EasyGameUI"; Target = "/Game/ThirdParty/EasyGameUI"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; Action = "Move"; Notes = "DefaultGame.ini references input prompt data tables." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/LED_Generator"; Target = "/Game/ThirdParty/LED_Generator"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move"; Notes = "Tiny isolated pack." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/References"; Target = "/Game/ThirdParty/References"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move or archive"; Notes = "Already excluded from git. Good candidate for quarantine." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/StarterContent"; Target = "/Game/ThirdParty/StarterContent"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; Action = "Move or delete"; Notes = "Remove if unused." }
    [PSCustomObject]@{ Batch = "04-ThirdParty"; Source = "Content/StorageHouse"; Target = "/Game/ThirdParty/StorageHouse"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; Action = "Move"; Notes = "DefaultEngine.ini contains redirects from StorageHouse and Warehouse_Storage." }

    [PSCustomObject]@{ Batch = "05-Maps"; Source = "Content/Maps"; Target = "/Game/Pickpacker/Maps"; Risk = "Critical"; CodeSensitive = "Yes"; IniSensitive = "Yes"; Action = "Move last"; Notes = "Referenced by C++ travel paths, startup maps, and cook settings." }
)

$batchRows = foreach ($rule in $moveRules) {
    $sourcePath = if ($rule.Source -eq "Content") { $contentRoot } else { Join-Path $projectRootPath $rule.Source.Replace("/", "\") }
    $counts = Get-AssetCounts -Path $sourcePath

    [PSCustomObject]@{
        Batch = $rule.Batch
        Action = $rule.Action
        Source = $rule.Source
        Target = $rule.Target
        Exists = if ($counts.Exists) { "Yes" } else { "No" }
        Files = $counts.Files
        UAssets = $counts.UAssets
        Maps = $counts.Maps
        Risk = $rule.Risk
        CodeSensitive = $rule.CodeSensitive
        IniSensitive = $rule.IniSensitive
        Notes = $rule.Notes
    }
}

$referenceFiles = @()
$sourceRoot = Join-Path $projectRootPath "Source"
$configRoot = Join-Path $projectRootPath "Config"

if (Test-Path -Path $sourceRoot -PathType Container) {
    $referenceFiles += Get-ChildItem -Path $sourceRoot -Recurse -Include *.h,*.cpp,*.cs -File
}

if (Test-Path -Path $configRoot -PathType Container) {
    $referenceFiles += Get-ChildItem -Path $configRoot -Recurse -Include *.ini -File
}

$referenceRows = foreach ($file in $referenceFiles) {
    $matches = Select-String -Path $file.FullName -Pattern "/Game/[A-Za-z0-9_./-]+" -AllMatches
    foreach ($match in $matches) {
        foreach ($value in $match.Matches.Value) {
            $segments = $value.Split("/")
            $root = if ($segments.Length -ge 3) { $segments[2] } else { "" }

            [PSCustomObject]@{
                Root = $root
                AssetPath = $value
                File = Get-RelativeProjectPath -BasePath $projectRootPath -TargetPath $file.FullName
                Line = $match.LineNumber
            }
        }
    }
}

$batchOutputPath = Join-Path $outputRoot "Content_Move_Batches.csv"
$referenceOutputPath = Join-Path $outputRoot "Content_PathSensitive_References.csv"

$batchRows | Export-Csv -Path $batchOutputPath -NoTypeInformation -Encoding UTF8
$referenceRows | Sort-Object Root, File, Line, AssetPath | Export-Csv -Path $referenceOutputPath -NoTypeInformation -Encoding UTF8

Write-Host "Move batch plan written to $batchOutputPath"
Write-Host "Path reference report written to $referenceOutputPath"
