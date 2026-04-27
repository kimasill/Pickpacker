param(
	[string]$ProjectPath = (Get-Location).Path,
	[string]$Version,
	[ValidateSet('Root', 'Editor', 'BuildBat', 'RunUAT')]
	[string]$Kind = 'Root'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProjectFilePath {
	param([string]$PathHint)

	if ([string]::IsNullOrWhiteSpace($PathHint)) {
		$PathHint = (Get-Location).Path
	}

	$resolvedPath = Resolve-Path -LiteralPath $PathHint -ErrorAction Stop
	$item = Get-Item -LiteralPath $resolvedPath.Path

	if ($item.PSIsContainer) {
		$projectFile = Get-ChildItem -LiteralPath $item.FullName -Filter *.uproject -File | Select-Object -First 1
		if ($projectFile) {
			return $projectFile.FullName
		}

		$currentDirectory = $item.FullName
		while ($currentDirectory) {
			$parentProject = Get-ChildItem -LiteralPath $currentDirectory -Filter *.uproject -File | Select-Object -First 1
			if ($parentProject) {
				return $parentProject.FullName
			}

			$parent = Split-Path -Path $currentDirectory -Parent
			if ($parent -eq $currentDirectory) {
				break
			}
			$currentDirectory = $parent
		}

		return $null
	}

	if ($item.Extension -ieq '.uproject') {
		return $item.FullName
	}

	return $null
}

function Get-EngineAssociationFromProject {
	param([string]$ProjectFilePath)

	if (-not $ProjectFilePath -or -not (Test-Path -LiteralPath $ProjectFilePath)) {
		return $null
	}

	$projectJson = Get-Content -LiteralPath $ProjectFilePath -Raw | ConvertFrom-Json
	return $projectJson.EngineAssociation
}

function Get-UnrealInstallRoots {
	$roots = New-Object System.Collections.Generic.List[string]

	$preferredRoots = @(
		$env:UNREAL_ENGINE_INSTALL_ROOT,
		$env:UNREAL_ENGINE_HOME,
		$env:UE_INSTALL_ROOT,
		'S:\UnrealEngine',
		'C:\Program Files\Epic Games'
	)

	foreach ($candidate in $preferredRoots) {
		if ([string]::IsNullOrWhiteSpace($candidate)) {
			continue
		}

		if (-not $roots.Contains($candidate)) {
			$roots.Add($candidate)
		}
	}

	return $roots
}

function Resolve-UnrealEngineRoot {
	param(
		[string]$RequestedVersion,
		[string]$ProjectFilePath
	)

	$normalizedVersion = $RequestedVersion
	if ([string]::IsNullOrWhiteSpace($normalizedVersion)) {
		$normalizedVersion = Get-EngineAssociationFromProject -ProjectFilePath $ProjectFilePath
	}

	$versionSpecificEnvMap = @{
		'5.4' = $env:UE_5_4_ROOT
		'5.6' = $env:UE_5_6_ROOT
	}

	if ($normalizedVersion -and $versionSpecificEnvMap.ContainsKey($normalizedVersion)) {
		$envRoot = $versionSpecificEnvMap[$normalizedVersion]
		if ($envRoot -and (Test-Path -LiteralPath $envRoot)) {
			return (Resolve-Path -LiteralPath $envRoot).Path
		}
	}

	foreach ($root in Get-UnrealInstallRoots) {
		if (-not (Test-Path -LiteralPath $root)) {
			continue
		}

		if ($normalizedVersion) {
			$versionedRoot = Join-Path $root ("UE_{0}" -f $normalizedVersion)
			if (Test-Path -LiteralPath $versionedRoot) {
				return (Resolve-Path -LiteralPath $versionedRoot).Path
			}
		}

		$engineMarkers = @(
			Join-Path $root 'Engine\Binaries\Win64\UnrealEditor.exe',
			Join-Path $root 'Engine\Build\BatchFiles\Build.bat'
		)

		if ($engineMarkers | Where-Object { Test-Path -LiteralPath $_ }) {
			return (Resolve-Path -LiteralPath $root).Path
		}
	}

	throw "Unable to resolve Unreal Engine root. Checked version '$normalizedVersion' and install roots: $((Get-UnrealInstallRoots) -join ', ')"
}

$projectFilePath = Get-ProjectFilePath -PathHint $ProjectPath
$engineRoot = Resolve-UnrealEngineRoot -RequestedVersion $Version -ProjectFilePath $projectFilePath

$resolvedPath = switch ($Kind) {
	'Editor' { Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe' }
	'BuildBat' { Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat' }
	'RunUAT' { Join-Path $engineRoot 'Engine\Build\BatchFiles\RunUAT.bat' }
	default { $engineRoot }
}

if (-not (Test-Path -LiteralPath $resolvedPath)) {
	throw "Resolved Unreal path does not exist: $resolvedPath"
}

Write-Output $resolvedPath
