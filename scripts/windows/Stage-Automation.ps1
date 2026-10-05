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

# Both supported delivery routes require the actual Windows platform.
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'Automation compilation requires 64-bit Windows.'
}
$hosted = $env:GITHUB_ACTIONS -eq 'true'
if ($hosted -and ($env:RUNNER_OS -ne 'Windows' -or $env:GITHUB_RUN_ID -notmatch '^\d+$')) {
    throw 'Hosted automation compilation requires a Windows runner and workflow run identity.'
}
$dotnet = Get-Command dotnet -CommandType Application -ErrorAction Stop
$installedSdks = & $dotnet.Source --list-sdks
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate installed .NET SDKs.' }
$sdkVersion = @($installedSdks | ForEach-Object {
    if ($_ -match '^(10\.\d+\.\d+)\s+\[') { $Matches[1] }
} | Sort-Object { [version]$_ } -Descending | Select-Object -First 1)
if ($sdkVersion.Count -ne 1) { throw 'Automation compilation requires a stable .NET 10 SDK.' }
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $root 'automation/BambuAutomation/BambuAutomation.csproj'
$destination = Join-Path ([IO.Path]::GetFullPath($PayloadDirectory)) 'automation'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
# Select the supported SDK without changing the checkout or a machine-wide default.
$sdkSelection = Join-Path ([IO.Path]::GetTempPath()) ('BambuAutomation-Sdk-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sdkSelection | Out-Null
try {
    @{ sdk = @{ version = $sdkVersion[0]; rollForward = 'disable'; allowPrerelease = $false } } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $sdkSelection 'global.json') -Encoding utf8
    Push-Location $sdkSelection
    try {
        & $dotnet.Source publish $project --configuration $Configuration --runtime win-x64 --self-contained true --output $destination -p:ContinuousIntegrationBuild=true
        if ($LASTEXITCODE -ne 0) { throw "Automation publish failed ($LASTEXITCODE)." }
    }
    finally { Pop-Location }
}
finally {
    Remove-Item -LiteralPath (Join-Path $sdkSelection 'global.json') -Force
    [IO.Directory]::Delete($sdkSelection)
}
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
    workflowRun = $(if ($hosted) { $env:GITHUB_RUN_ID } else { $null })
    buildRoute = $(if ($hosted) { 'github-actions' } else { 'local-windows' })
    sdkVersion = $sdkVersion[0]
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'build-identity.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $root 'automation/README.md') -Destination (Join-Path $destination 'README.md')
