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

function Get-BlueprintSubfolderFromAssetPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$AssetPath
    )

    if ($AssetPath -notmatch "^/Game/Blueprints?/") {
        return ""
    }

    $trimmed = $AssetPath -replace "^/Game/Blueprints?/", ""
    $segments = $trimmed.Split("/")
    if ($segments.Length -eq 0) {
        return ""
    }

    return $segments[0]
}

$projectRootPath = Resolve-NormalizedPath -Path $ProjectRoot
$contentBlueprintRoot = Join-Path $projectRootPath "Content\BluePrints"
$outputRoot = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $projectRootPath $OutputDir }

if (-not (Test-Path -Path $contentBlueprintRoot -PathType Container)) {
    throw "Blueprint root was not found at '$contentBlueprintRoot'."
}

if (-not (Test-Path -Path $outputRoot -PathType Container)) {
    $null = New-Item -Path $outputRoot -ItemType Directory -Force
}

$referenceCsvPath = Join-Path $outputRoot "Content_PathSensitive_References.csv"
$referenceRows = @()

if (Test-Path -Path $referenceCsvPath -PathType Leaf) {
    $referenceRows = @(Import-Csv $referenceCsvPath | Where-Object { $_.Root -in @("BluePrints", "Blueprints") })
}
else {
    $sourceFiles = @()
    $sourceRoot = Join-Path $projectRootPath "Source"
    $configRoot = Join-Path $projectRootPath "Config"

    if (Test-Path -Path $sourceRoot -PathType Container) {
        $sourceFiles += Get-ChildItem -Path $sourceRoot -Recurse -Include *.h,*.cpp,*.cs -File
    }

    if (Test-Path -Path $configRoot -PathType Container) {
        $sourceFiles += Get-ChildItem -Path $configRoot -Recurse -Include *.ini -File
    }

    foreach ($file in $sourceFiles) {
        $matches = Select-String -Path $file.FullName -Pattern "/Game/Blueprints?/[A-Za-z0-9_./-]+" -AllMatches
        foreach ($match in $matches) {
            foreach ($value in $match.Matches.Value) {
                $referenceRows += [PSCustomObject]@{
                    Root = "BluePrints"
                    AssetPath = $value
                    File = Get-RelativeProjectPath -BasePath $projectRootPath -TargetPath $file.FullName
                    Line = $match.LineNumber
                }
            }
        }
    }
}

$referenceRows = @($referenceRows | ForEach-Object {
    [PSCustomObject]@{
        Subfolder = Get-BlueprintSubfolderFromAssetPath -AssetPath $_.AssetPath
        AssetPath = $_.AssetPath
        File = $_.File
        Line = $_.Line
    }
} | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Subfolder) })

$moveRules = @(
    [PSCustomObject]@{ Step = 1; Source = "Inputs"; Target = "/Game/Pickpacker/Core/Input"; Risk = "Critical"; CodeSensitive = "Yes"; IniSensitive = "No"; MoveMode = "Move first"; Notes = "Input mapping context and input actions are loaded directly from C++." }
    [PSCustomObject]@{ Step = 2; Source = "UI"; Target = "/Game/Pickpacker/Core/UI"; Risk = "Critical"; CodeSensitive = "Yes"; IniSensitive = "Yes"; MoveMode = "Move next"; Notes = "Loading screen, lobby menu, NPC dialogue, and legacy redirect entries live here." }
    [PSCustomObject]@{ Step = 3; Source = "HUD"; Target = "/Game/Pickpacker/Core/UI/HUD"; Risk = "Critical"; CodeSensitive = "Yes"; IniSensitive = "No"; MoveMode = "Move next"; Notes = "Interaction prompt and gameplay HUD are loaded directly from C++." }
    [PSCustomObject]@{ Step = 4; Source = "GameModes"; Target = "/Game/Pickpacker/Core/GameModes"; Risk = "Critical"; CodeSensitive = "Yes"; IniSensitive = "Yes"; MoveMode = "Move next"; Notes = "DefaultEngine.ini points to BP_PickPackerGameMode." }
    [PSCustomObject]@{ Step = 5; Source = "Data"; Target = "/Game/Pickpacker/Core/Data/Blueprints"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; MoveMode = "Move next"; Notes = "DefaultEngine.ini redirects already point at this folder." }
    [PSCustomObject]@{ Step = 6; Source = "RenderTarget"; Target = "/Game/Pickpacker/Core/Shared/RenderTarget"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; MoveMode = "Move after Data"; Notes = "DirectoriesToAlwaysCook must be updated after move." }
    [PSCustomObject]@{ Step = 7; Source = "GamePlay"; Target = "/Game/Pickpacker/Gameplay/Systems"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; MoveMode = "Move after core UI"; Notes = "Legacy redirect targets AC_CallAction and BP_RobotHend live here." }
    [PSCustomObject]@{ Step = 8; Source = "AI"; Target = "/Game/Pickpacker/Gameplay/AI"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after GamePlay"; Notes = "Large folder. Validate behavior trees and tasks after move." }
    [PSCustomObject]@{ Step = 9; Source = "Weapon"; Target = "/Game/Pickpacker/Gameplay/Weapons"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after AI"; Notes = "Includes projectile and casing blueprints." }
    [PSCustomObject]@{ Step = 10; Source = "Parcel"; Target = "/Game/Pickpacker/Gameplay/Parcel"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Weapon"; Notes = "Gameplay-specific parcel actors." }
    [PSCustomObject]@{ Step = 11; Source = "Station"; Target = "/Game/Pickpacker/Gameplay/Stations"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Parcel"; Notes = "Station gameplay actors." }
    [PSCustomObject]@{ Step = 12; Source = "SpawnPoints"; Target = "/Game/Pickpacker/Gameplay/SpawnPoints"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Station"; Notes = "Small isolated folder." }
    [PSCustomObject]@{ Step = 13; Source = "Gates"; Target = "/Game/Pickpacker/Gameplay/Gates"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after SpawnPoints"; Notes = "Gate actors only." }
    [PSCustomObject]@{ Step = 14; Source = "Shelf"; Target = "/Game/Pickpacker/Gameplay/Shelf"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Gates"; Notes = "Single blueprint folder." }
    [PSCustomObject]@{ Step = 15; Source = "Acting"; Target = "/Game/Pickpacker/Gameplay/Acting"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Shelf"; Notes = "Small helper folder." }
    [PSCustomObject]@{ Step = 16; Source = "Notify"; Target = "/Game/Pickpacker/Characters/Animation/Notify"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Acting"; Notes = "Animation notify assets." }
    [PSCustomObject]@{ Step = 17; Source = "Camera"; Target = "/Game/Pickpacker/Core/Camera"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move after Notify"; Notes = "Single camera blueprint." }
    [PSCustomObject]@{ Step = 18; Source = "Character"; Target = "/Game/Pickpacker/Characters/Blueprints"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move near the end"; Notes = "Animation blueprints and character-specific assets." }
    [PSCustomObject]@{ Step = 19; Source = "Pawn"; Target = "/Game/Pickpacker/Characters/Pawn"; Risk = "High"; CodeSensitive = "No"; IniSensitive = "Yes"; MoveMode = "Move near the end"; Notes = "DefaultEngine.ini contains a redirect for ThirdPersonCharacter." }
    [PSCustomObject]@{ Step = 20; Source = "PlayerController"; Target = "/Game/Pickpacker/Core/PlayerController"; Risk = "Medium"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move near the end"; Notes = "Blueprint subclasses of player controller." }
    [PSCustomObject]@{ Step = 21; Source = "PlayerState"; Target = "/Game/Pickpacker/Core/PlayerState"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move near the end"; Notes = "Small isolated folder." }
    [PSCustomObject]@{ Step = 22; Source = "GameState"; Target = "/Game/Pickpacker/Core/GameState"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move near the end"; Notes = "Small isolated folder." }
    [PSCustomObject]@{ Step = 23; Source = "Edits"; Target = "/Game/Pickpacker/Editor/Edits"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Move last"; Notes = "Editor-only or scratch assets. Keep out of gameplay roots if possible." }
    [PSCustomObject]@{ Step = 24; Source = "Library"; Target = "/Game/Pickpacker/Core/Library"; Risk = "Low"; CodeSensitive = "No"; IniSensitive = "No"; MoveMode = "Skip if empty"; Notes = "Currently empty. No action needed unless assets appear later." }
)

$checklistRows = foreach ($rule in $moveRules) {
    $sourcePath = Join-Path $contentBlueprintRoot $rule.Source
    $counts = Get-AssetCounts -Path $sourcePath
    $subfolderRefs = @($referenceRows | Where-Object { $_.Subfolder -ieq $rule.Source })

    $files = @($subfolderRefs | Select-Object -ExpandProperty File -Unique | Sort-Object)
    $paths = @($subfolderRefs | Select-Object -ExpandProperty AssetPath -Unique | Sort-Object)

    [PSCustomObject]@{
        Step = $rule.Step
        Source = "Content/BluePrints/$($rule.Source)"
        Target = $rule.Target
        Exists = if ($counts.Exists) { "Yes" } else { "No" }
        Files = $counts.Files
        UAssets = $counts.UAssets
        Maps = $counts.Maps
        DirectReferenceCount = $subfolderRefs.Count
        ReferencedBy = if ($files.Count -gt 0) { $files -join "; " } else { "-" }
        ReferencedPaths = if ($paths.Count -gt 0) { $paths -join "; " } else { "-" }
        Risk = $rule.Risk
        CodeSensitive = $rule.CodeSensitive
        IniSensitive = $rule.IniSensitive
        MoveMode = $rule.MoveMode
        Notes = $rule.Notes
        Validation = "Move in Unreal Editor -> Fix Up Redirectors -> Compile affected blueprints -> PIE smoke test"
    }
}

$detailRows = foreach ($reference in $referenceRows | Sort-Object Subfolder, File, Line, AssetPath) {
    [PSCustomObject]@{
        Step = ($moveRules | Where-Object { $_.Source -ieq $reference.Subfolder } | Select-Object -First 1 -ExpandProperty Step)
        Subfolder = $reference.Subfolder
        File = $reference.File
        Line = $reference.Line
        AssetPath = $reference.AssetPath
    }
}

$checklistPath = Join-Path $outputRoot "Blueprint_Move_Checklist.csv"
$referencePath = Join-Path $outputRoot "Blueprint_Move_Reference_Details.csv"

$checklistRows | Sort-Object Step | Export-Csv -Path $checklistPath -NoTypeInformation -Encoding UTF8
$detailRows | Export-Csv -Path $referencePath -NoTypeInformation -Encoding UTF8

Write-Host "Blueprint move checklist written to $checklistPath"
Write-Host "Blueprint move reference details written to $referencePath"
