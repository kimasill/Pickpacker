[CmdletBinding()]
param(
    [string]$ApiKey,
    [string]$TeamId,
    [string]$WorkspaceUrl,
    [ValidateSet("User", "Process")]
    [string]$Scope = "User"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-SecretValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Prompt
    )

    $secure = Read-Host -Prompt $Prompt -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    }
    finally {
        if ($bstr -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
        }
    }
}

function Set-EnvValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Value,

        [Parameter(Mandatory = $true)]
        [ValidateSet("User", "Process")]
        [string]$TargetScope
    )

    if ($TargetScope -eq "Process") {
        Set-Item -Path "Env:$Name" -Value $Value
        return
    }

    [Environment]::SetEnvironmentVariable($Name, $Value, [EnvironmentVariableTarget]::User)
    Set-Item -Path "Env:$Name" -Value $Value
}

if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    $ApiKey = Read-SecretValue -Prompt "LINEAR_API_KEY"
}

if ([string]::IsNullOrWhiteSpace($TeamId)) {
    $TeamId = Read-Host -Prompt "LINEAR_TEAM_ID"
}

if ([string]::IsNullOrWhiteSpace($WorkspaceUrl)) {
    $WorkspaceUrl = Read-Host -Prompt "LINEAR_WORKSPACE_URL"
}

if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    throw "LINEAR_API_KEY cannot be empty."
}

if ([string]::IsNullOrWhiteSpace($TeamId)) {
    throw "LINEAR_TEAM_ID cannot be empty."
}

if ([string]::IsNullOrWhiteSpace($WorkspaceUrl)) {
    throw "LINEAR_WORKSPACE_URL cannot be empty."
}

Set-EnvValue -Name "LINEAR_API_KEY" -Value $ApiKey -TargetScope $Scope
Set-EnvValue -Name "LINEAR_TEAM_ID" -Value $TeamId -TargetScope $Scope
Set-EnvValue -Name "LINEAR_WORKSPACE_URL" -Value $WorkspaceUrl -TargetScope $Scope

Write-Host "Linear environment variables configured."
Write-Host "  Scope      : $Scope"
Write-Host "  Team ID    : $TeamId"
Write-Host "  Workspace  : $WorkspaceUrl"
Write-Host ""

if ($Scope -eq "User") {
    Write-Host "Saved to Windows user environment variables."
    Write-Host "Open a new terminal for other sessions to pick them up."
}
else {
    Write-Host "Saved only for the current shell process."
}
