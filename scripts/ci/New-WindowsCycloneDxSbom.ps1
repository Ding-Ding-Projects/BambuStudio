[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string] $PayloadDir,

    [Parameter(Mandatory)]
    [string] $OutputPath,

    [Parameter(Mandatory)]
    [string] $Version,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $Commit
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$payloadRootItem = Get-Item -LiteralPath $PayloadDir -Force
if (($payloadRootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
    throw "Refusing to describe reparse-point payload root '$($payloadRootItem.FullName)'."
}
$payloadRoot = $payloadRootItem.FullName.TrimEnd('\')
$resolvedOutputParent = (Resolve-Path -LiteralPath (Split-Path -Parent $OutputPath)).Path
$resolvedOutput = Join-Path $resolvedOutputParent (Split-Path -Leaf $OutputPath)

if ($resolvedOutput.StartsWith($payloadRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'The SBOM output must not be written inside the payload it describes.'
}

$components = [System.Collections.Generic.List[object]]::new()
$entries = @(Get-ChildItem -LiteralPath $payloadRoot -Recurse -Force)
$reparseEntry = $entries | Where-Object {
    ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0
} | Select-Object -First 1
if ($reparseEntry) {
    throw "Refusing to describe payload containing reparse point '$($reparseEntry.FullName)'."
}
$files = @($entries | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
if ($files.Count -eq 0) {
    throw "Payload '$payloadRoot' contains no files."
}

foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($payloadRoot.Length + 1).Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $components.Add([ordered]@{
        type = 'file'
        name = $relativePath
        'bom-ref' = "file:$relativePath"
        hashes = @(
            [ordered]@{
                alg = 'SHA-256'
                content = $hash
            }
        )
        properties = @(
            [ordered]@{
                name = 'bambustudio:file-size'
                value = [string] $file.Length
            }
        )
    })
}

$bom = [ordered]@{
    '$schema' = 'https://cyclonedx.org/schema/bom-1.6.schema.json'
    bomFormat = 'CycloneDX'
    specVersion = '1.6'
    serialNumber = 'urn:uuid:' + [guid]::NewGuid().ToString()
    version = 1
    metadata = [ordered]@{
        timestamp = [DateTimeOffset]::UtcNow.ToString('o')
        tools = [ordered]@{
            components = @(
                [ordered]@{
                    type = 'application'
                    author = 'codingmachineedge'
                    name = 'BambuStudio Windows SBOM generator'
                    version = '1'
                }
            )
        }
        component = [ordered]@{
            type = 'application'
            group = 'codingmachineedge'
            name = 'Bambu Studio MD3 Windows payload'
            version = $Version
            'bom-ref' = "pkg:github/codingmachineedge/BambuStudio@$Commit"
            externalReferences = @(
                [ordered]@{
                    type = 'vcs'
                    url = "https://github.com/codingmachineedge/BambuStudio/tree/$Commit"
                }
            )
        }
    }
    components = $components
}

$bom | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resolvedOutput -Encoding utf8NoBOM
$sbomSize = (Get-Item -LiteralPath $resolvedOutput).Length
if ($sbomSize -gt 16MB) {
    throw "Generated SBOM is $sbomSize bytes, above the 16 MiB attestation limit."
}

try {
    $parsed = Get-Content -LiteralPath $resolvedOutput -Raw | ConvertFrom-Json
}
catch {
    throw "Generated SBOM is not valid JSON: $($_.Exception.Message)"
}

if ($parsed.bomFormat -ne 'CycloneDX' -or $parsed.specVersion -ne '1.6') {
    throw 'Generated SBOM does not identify itself as CycloneDX 1.6.'
}
if (@($parsed.components).Count -ne $files.Count) {
    throw "Generated SBOM component count does not match payload file count ($($files.Count))."
}
$componentNames = @($parsed.components | ForEach-Object { $_.name })
if (($componentNames | Sort-Object -Unique).Count -ne $componentNames.Count) {
    throw 'Generated SBOM contains duplicate component names.'
}
if ($componentNames -notcontains 'bambu-studio.exe') {
    throw 'Generated SBOM does not contain the main bambu-studio.exe payload.'
}
foreach ($component in $parsed.components) {
    $sha256 = @($component.hashes | Where-Object { $_.alg -eq 'SHA-256' })
    if ($sha256.Count -ne 1 -or $sha256[0].content -notmatch '^[0-9a-f]{64}$') {
        throw "Generated SBOM component '$($component.name)' lacks one valid lowercase SHA-256 hash."
    }
}

Write-Host "Generated CycloneDX SBOM with $($files.Count) file components at '$resolvedOutput'."
