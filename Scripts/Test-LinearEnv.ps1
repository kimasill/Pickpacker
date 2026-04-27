[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$values = [ordered]@{
    LINEAR_API_KEY = $env:LINEAR_API_KEY
    LINEAR_TEAM_ID = $env:LINEAR_TEAM_ID
    LINEAR_WORKSPACE_URL = $env:LINEAR_WORKSPACE_URL
}

$missing = @()
foreach ($entry in $values.GetEnumerator()) {
    if ([string]::IsNullOrWhiteSpace($entry.Value)) {
        $missing += $entry.Key
    }
}

if ($missing.Count -gt 0) {
    Write-Host "Missing Linear environment variables:"
    $missing | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

$maskedKey = if ($env:LINEAR_API_KEY.Length -le 8) {
    "********"
}
else {
    $env:LINEAR_API_KEY.Substring(0, 4) + "..." + $env:LINEAR_API_KEY.Substring($env:LINEAR_API_KEY.Length - 4)
}

Write-Host "Linear environment variables are available."
Write-Host "  LINEAR_API_KEY       : $maskedKey"
Write-Host "  LINEAR_TEAM_ID       : $env:LINEAR_TEAM_ID"
Write-Host "  LINEAR_WORKSPACE_URL : $env:LINEAR_WORKSPACE_URL"
