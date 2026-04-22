[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Title,

    [string]$Description = "",

    [string]$TeamId = $env:LINEAR_TEAM_ID,

    [string]$ApiKey = $env:LINEAR_API_KEY,

    [string]$WorkspaceUrl = $env:LINEAR_WORKSPACE_URL,

    [switch]$CreateBranch,

    [switch]$OutputJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function ConvertTo-BranchSlug {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text
    )

    $slug = $Text.ToLowerInvariant()
    $slug = [System.Text.RegularExpressions.Regex]::Replace($slug, "[^a-z0-9]+", "-")
    $slug = $slug.Trim("-")
    return $slug
}

function Get-WorkspaceBaseUrl {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $trimmed = $Value.Trim()
    if ($trimmed -match "^https?://") {
        return $trimmed.TrimEnd("/")
    }

    return "https://linear.app/$($trimmed.Trim('/'))"
}

function Invoke-LinearGraphQL {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Query,

        [Parameter(Mandatory = $true)]
        [hashtable]$Variables,

        [Parameter(Mandatory = $true)]
        [string]$AuthToken
    )

    $body = @{
        query = $Query
        variables = $Variables
    } | ConvertTo-Json -Depth 20

    try {
        $response = Invoke-RestMethod `
            -Method Post `
            -Uri "https://api.linear.app/graphql" `
            -Headers @{
                Authorization = $AuthToken
                "Content-Type" = "application/json"
            } `
            -Body $body
    }
    catch {
        $message = $_.Exception.Message
        if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
            $message = $_.ErrorDetails.Message
        }
        throw "Linear API request failed: $message"
    }

    if ($response.errors) {
        $errors = $response.errors | ConvertTo-Json -Depth 20 -Compress
        throw "Linear API returned GraphQL errors: $errors"
    }

    return $response.data
}

if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    throw "LINEAR_API_KEY is required. Pass -ApiKey or set the LINEAR_API_KEY environment variable."
}

if ([string]::IsNullOrWhiteSpace($TeamId)) {
    throw "LINEAR_TEAM_ID is required. Pass -TeamId or set the LINEAR_TEAM_ID environment variable."
}

$mutation = @"
mutation IssueCreate(`$input: IssueCreateInput!) {
  issueCreate(input: `$input) {
    success
    issue {
      id
      identifier
      title
    }
  }
}
"@

$data = Invoke-LinearGraphQL `
    -Query $mutation `
    -Variables @{
        input = @{
            title = $Title
            description = $Description
            teamId = $TeamId
        }
    } `
    -AuthToken $ApiKey

$issueCreate = $data.issueCreate
if (-not $issueCreate -or -not $issueCreate.success -or -not $issueCreate.issue) {
    throw "Linear API did not return a created issue."
}

$issue = $issueCreate.issue
$slug = ConvertTo-BranchSlug -Text $issue.title
$branchName = if ([string]::IsNullOrWhiteSpace($slug)) {
    $issue.identifier
}
else {
    "$($issue.identifier)-$slug"
}

$workspaceBaseUrl = Get-WorkspaceBaseUrl -Value $WorkspaceUrl
$issueUrl = $null
if ($workspaceBaseUrl) {
    $issueUrl = "$workspaceBaseUrl/issue/$($issue.identifier)"
    if (-not [string]::IsNullOrWhiteSpace($slug)) {
        $issueUrl = "$issueUrl/$slug"
    }
}

if ($CreateBranch) {
    $null = git rev-parse --is-inside-work-tree 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "The current directory is not inside a git work tree."
    }

    git rev-parse --verify --quiet $branchName *> $null
    if ($LASTEXITCODE -eq 0) {
        throw "Git branch '$branchName' already exists."
    }

    $null = git checkout -b $branchName
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create git branch '$branchName'."
    }
}

$result = [ordered]@{
    issueId = $issue.id
    identifier = $issue.identifier
    title = $issue.title
    branchName = $branchName
    issueUrl = $issueUrl
    branchCreated = [bool]$CreateBranch
}

if ($OutputJson) {
    $result | ConvertTo-Json -Compress
    exit 0
}

Write-Host "Linear issue created:"
Write-Host "  Identifier : $($result.identifier)"
Write-Host "  Title      : $($result.title)"
Write-Host "  Branch     : $($result.branchName)"

if ($result.issueUrl) {
    Write-Host "  URL        : $($result.issueUrl)"
}

if ($CreateBranch) {
    Write-Host "  Git        : checked out '$($result.branchName)'"
}
