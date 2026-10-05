[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $PayloadDirectory,
    [string] $Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Get-Sha256Lower {
    param([Parameter(Mandatory)][string] $Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

# The automation companion is built only on the hosted Windows delivery route.
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'Automation compilation is hosted-only. Run the Windows build workflow.'
}
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $root 'automation/BambuAutomation/BambuAutomation.csproj'
$destination = Join-Path ([IO.Path]::GetFullPath($PayloadDirectory)) 'automation'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
& dotnet publish $project --configuration $Configuration --runtime win-x64 --self-contained true --output $destination -p:ContinuousIntegrationBuild=true
if ($LASTEXITCODE -ne 0) { throw "Automation publish failed ($LASTEXITCODE)." }
$executable = Join-Path $destination 'bambu-automation.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Automation publish did not produce bambu-automation.exe.'
}
$sourceCommit = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $sourceCommit -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify automation source.' }
[ordered]@{
    schemaVersion = 1
    sourceCommit = $sourceCommit
    runtime = 'win-x64'
    selfContained = $true
    executable = 'bambu-automation.exe'
    sha256 = (Get-Sha256Lower -Path $executable)
    workflowRun = $env:GITHUB_RUN_ID
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'build-identity.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $root 'automation/README.md') -Destination (Join-Path $destination 'README.md')
