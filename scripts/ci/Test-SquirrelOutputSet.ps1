[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-SquirrelPackage.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-SquirrelPackage.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
foreach ($name in @('Get-Sha256Lower', 'Assert-SquirrelOutputs')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    if (-not $definition) { throw "Missing output function $name" }
    Invoke-Expression $definition.Extent.Text
}
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Get-PeCertificateTable { param($Path) return @{ Size = 0 } }
function Write-SquirrelLog { param($Message) }
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuOutputSet-' + [guid]::NewGuid())))
try {
    $payload = Join-Path $fixture 'payload'
    $release = Join-Path $fixture 'release'
    New-Item (Join-Path $payload 'lib/net45'), $release -ItemType Directory -Force | Out-Null
    Set-Content (Join-Path $payload 'lib/net45/bambu-studio.exe') 'fixture payload, never executed'
    $package = Join-Path $release 'Fixture-1.0.0-full.nupkg'
    [IO.Compression.ZipFile]::CreateFromDirectory($payload, $package)
    Set-Content (Join-Path $release 'Setup.exe') 'fixture setup, PE reader mocked, never executed'
    $sha1 = (Get-FileHash $package -Algorithm SHA1).Hash.ToLowerInvariant()
    $size = (Get-Item $package).Length
    $validRow = "$sha1 Fixture-1.0.0-full.nupkg $size"
    Set-Content (Join-Path $release 'RELEASES') $validRow
    $null = Assert-SquirrelOutputs $release 'Fixture' 'bambu-studio.exe'
    foreach ($case in @('hash', 'size', 'duplicate', 'unsafe', 'filename-substring')) {
        switch ($case) {
            'hash' { $row = ('0' * 40) + " Fixture-1.0.0-full.nupkg $size" }
            'size' { $row = "$sha1 Fixture-1.0.0-full.nupkg 1" }
            'duplicate' { $row = "$validRow`n$validRow" }
            'unsafe' { $row = "$sha1 ../Fixture-1.0.0-full.nupkg $size" }
            'filename-substring' { $row = "$sha1 prefix-Fixture-1.0.0-full.nupkg-suffix $size" }
        }
        Set-Content (Join-Path $release 'RELEASES') $row
        $rejected = $false
        try { $null = Assert-SquirrelOutputs $release 'Fixture' 'bambu-studio.exe' } catch { $rejected = $true }
        Assert-True $rejected "Invalid RELEASES '$case' was accepted."
    }
    Set-Content (Join-Path $release 'RELEASES') $validRow
    Copy-Item $package (Join-Path $release 'Fixture-1.0.0-delta.nupkg')
    $rejected = $false
    try { $null = Assert-SquirrelOutputs $release 'Fixture' 'bambu-studio.exe' } catch { $rejected = $true }
    Assert-True $rejected 'An unindexed delta package must not pass.'
    Remove-Item (Join-Path $release 'Fixture-1.0.0-delta.nupkg')
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Publish-SquirrelOutputs' }, $true)
    if (-not $definition) { throw 'Missing coherent output promotion function.' }
    Invoke-Expression $definition.Extent.Text
    $final = Join-Path $fixture 'output/squirrel'
    New-Item $final -ItemType Directory -Force | Out-Null
    Set-Content (Join-Path $final 'old-full.nupkg') 'previous output remains recoverable'
    Publish-SquirrelOutputs $release $final 'Fixture' 'bambu-studio.exe'
    Assert-True (@(Get-ChildItem $final -Filter '*-full.nupkg').Count -eq 1) 'Final output must contain one coherent full package.'
    $previous = @(Get-ChildItem (Split-Path $final) -Directory -Filter 'squirrel.previous-*')
    Assert-True ($previous.Count -eq 1 -and (Test-Path (Join-Path $previous[0].FullName 'old-full.nupkg'))) 'Previous packages must remain recoverable.'
    $setupHash = Get-Sha256Lower (Join-Path $final 'Setup.exe')
    Assert-True ((Get-Content (Join-Path $final 'Setup.exe.sha256') -Raw).Trim() -ceq "$setupHash *Setup.exe") 'Promoted checksum must match Setup bytes.'
    Set-Content (Join-Path $release 'RELEASES') "bad $validRow"
    $rejected = $false
    try { Publish-SquirrelOutputs $release $final 'Fixture' 'bambu-studio.exe' } catch { $rejected = $true }
    Assert-True ($rejected -and (Get-Sha256Lower (Join-Path $final 'Setup.exe')) -ceq $setupHash) 'Invalid new inputs must preserve the current output set.'
    Set-Content (Join-Path $release 'RELEASES') $validRow
    $script:originalAssert = (Get-Item function:Assert-SquirrelOutputs).ScriptBlock
    $script:validationCount = 0
    function Assert-SquirrelOutputs {
        param($ReleaseDirectory, $PackageId, $PayloadExecutable)
        $script:validationCount++
        if ($script:validationCount -eq 3) { throw 'Expected post-promotion fixture failure.' }
        & $script:originalAssert $ReleaseDirectory $PackageId $PayloadExecutable
    }
    $rejected = $false
    try { Publish-SquirrelOutputs $release $final 'Fixture' 'bambu-studio.exe' } catch { $rejected = $true }
    Assert-True ($rejected -and (Get-Sha256Lower (Join-Path $final 'Setup.exe')) -ceq $setupHash) 'Post-promotion failure must restore the previous output set.'
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Coherent Squirrel output checks passed (11 assertions; fixture packages and mocked PE reader only).'
