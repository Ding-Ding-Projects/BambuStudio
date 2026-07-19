[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$generator = Join-Path $repoRoot 'packaging\windows\GenerateUninstallInclude.ps1'
$sbomGenerator = Join-Path $repoRoot 'scripts\ci\New-WindowsCycloneDxSbom.ps1'
$languageVerifier = Join-Path $repoRoot 'scripts\i18n\Test-LanguageModes.ps1'

function Assert-True {
    param(
        [Parameter(Mandatory)]
        [bool] $Condition,

        [Parameter(Mandatory)]
        [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-Node {
    param(
        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $FailureMessage
    )

    & node @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (node exit code $LASTEXITCODE)."
    }
}

Write-Host 'Parsing PowerShell sources...'
$powerShellPaths = @(& git -C $repoRoot ls-files --cached --others --exclude-standard -- '*.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Could not enumerate PowerShell source files with git.'
}
foreach ($relativePath in $powerShellPaths) {
    $file = Get-Item -LiteralPath (Join-Path $repoRoot $relativePath)
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $file.FullName,
        [ref] $tokens,
        [ref] $errors
    ) | Out-Null

    if ($errors.Count -gt 0) {
        $details = ($errors | ForEach-Object { $_.Message }) -join '; '
        throw "PowerShell parse failure in '$($file.FullName)': $details"
    }
}

Write-Host 'Parsing JSON resources...'
$jsonRoots = @(
    (Join-Path $repoRoot 'src\slic3r\GUI\DeviceWeb\device_page'),
    (Join-Path $repoRoot 'ui-md3'),
    (Join-Path $repoRoot 'bbl\i18n')
)
foreach ($jsonRoot in $jsonRoots) {
    if (-not (Test-Path -LiteralPath $jsonRoot)) {
        continue
    }

    foreach ($file in Get-ChildItem -LiteralPath $jsonRoot -Recurse -File -Filter '*.json') {
        try {
            Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json -AsHashtable | Out-Null
        }
        catch {
            throw "JSON parse failure in '$($file.FullName)': $($_.Exception.Message)"
        }
    }
}

Write-Host 'Checking browser JavaScript syntax...'
$javaScriptFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'ui-md3') -Recurse -File |
    Where-Object { $_.Extension -in @('.js', '.mjs') })
foreach ($file in $javaScriptFiles) {
    Invoke-Node -Arguments @('--check', $file.FullName) -FailureMessage "JavaScript syntax failure in '$($file.FullName)'"
}

Write-Host 'Running dependency-free DeviceWeb tests...'
$devicePageRoot = Join-Path $repoRoot 'src\slic3r\GUI\DeviceWeb\device_page'
$deviceTests = @(Get-ChildItem -LiteralPath (Join-Path $devicePageRoot 'tests') -File -Filter '*.test.ts' | Sort-Object Name)
Assert-True ($deviceTests.Count -gt 0) 'No DeviceWeb TypeScript tests were found.'
Push-Location $devicePageRoot
try {
    foreach ($test in $deviceTests) {
        Invoke-Node -Arguments @('--experimental-strip-types', '--no-warnings', $test.FullName) -FailureMessage "DeviceWeb test '$($test.Name)' failed"
    }
}
finally {
    Pop-Location
}

Write-Host 'Exercising deterministic ownership-scoped uninstall generation...'
Assert-True (Test-Path -LiteralPath $generator) "Missing uninstall generator '$generator'."

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BambuStudioMD3-CI-' + [guid]::NewGuid().ToString('N'))
$resolvedTempBase = (Resolve-Path -LiteralPath ([System.IO.Path]::GetTempPath())).Path.TrimEnd('\')
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $payload = Join-Path $tempRoot 'payload'
    $nested = Join-Path $payload 'resources\nested'
    New-Item -ItemType Directory -Path $nested -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $payload 'bambu-studio.exe') -Value 'fixture' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $nested 'resource.txt') -Value 'fixture' -Encoding ascii

    $first = Join-Path $tempRoot 'first.nsh'
    $second = Join-Path $tempRoot 'second.nsh'
    & $generator -PayloadDir $payload -OutputPath $first
    & $generator -PayloadDir $payload -OutputPath $second

    $firstContent = Get-Content -LiteralPath $first -Raw
    $secondContent = Get-Content -LiteralPath $second -Raw
    Assert-True ($firstContent -ceq $secondContent) 'Uninstall include generation is not deterministic.'
    Assert-True ($firstContent.Contains('!macro BambuMD3AssertDestinationPaths')) 'Destination reparse guard macro is missing.'
    Assert-True ($firstContent.Contains('!macro BambuMD3DeletePayloadFiles')) 'Owned-file delete macro is missing.'
    Assert-True ($firstContent.Contains('!macro BambuMD3RemovePayloadDirectories')) 'Empty-directory removal macro is missing.'
    Assert-True (-not $firstContent.Contains('RMDir /r')) 'Recursive directory removal is forbidden.'

    $deleteCount = ([regex]::Matches($firstContent, '(?m)^\s*Delete\s+')).Count
    Assert-True ($deleteCount -eq 2) "Expected two fixture payload deletes, found $deleteCount."

    $insidePayload = Join-Path $payload 'forbidden.nsh'
    $insidePayloadRejected = $false
    try {
        & $generator -PayloadDir $payload -OutputPath $insidePayload
    }
    catch {
        $insidePayloadRejected = $true
    }
    Assert-True $insidePayloadRejected 'The generator accepted an output path inside its payload.'
    Assert-True (-not (Test-Path -LiteralPath $insidePayload)) 'Rejected in-payload output was still created.'

    Write-Host 'Exercising CycloneDX payload inventory generation...'
    Assert-True (Test-Path -LiteralPath $sbomGenerator) "Missing SBOM generator '$sbomGenerator'."
    $sbomPath = Join-Path $tempRoot 'fixture.cdx.json'
    & $sbomGenerator `
        -PayloadDir $payload `
        -OutputPath $sbomPath `
        -Version '0.0.0-test' `
        -Commit ('0' * 40)

    $sbom = Get-Content -LiteralPath $sbomPath -Raw | ConvertFrom-Json
    Assert-True ($sbom.bomFormat -eq 'CycloneDX') 'Fixture SBOM format is incorrect.'
    Assert-True ($sbom.specVersion -eq '1.6') 'Fixture SBOM specification version is incorrect.'
    Assert-True (@($sbom.components).Count -eq 2) 'Fixture SBOM does not contain exactly two payload components.'
    $componentNames = @($sbom.components | ForEach-Object { $_.name })
    Assert-True ($componentNames -contains 'bambu-studio.exe') 'Fixture SBOM omitted the executable.'
    Assert-True ($componentNames -contains 'resources/nested/resource.txt') 'Fixture SBOM omitted the nested resource.'
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        $resolvedTempRoot = (Resolve-Path -LiteralPath $tempRoot).Path
        Assert-True ($resolvedTempRoot.StartsWith($resolvedTempBase + '\', [System.StringComparison]::OrdinalIgnoreCase)) 'Refusing to clean a temporary path outside the system temp directory.'
        Remove-Item -LiteralPath $resolvedTempRoot -Recurse -Force
    }
}

Write-Host 'Validating language-mode resources...'
Assert-True (Test-Path -LiteralPath $languageVerifier) "Missing language-mode verifier '$languageVerifier'."
& $languageVerifier

Write-Host 'Windows pre-release input tests passed.'
