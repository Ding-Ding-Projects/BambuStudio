[CmdletBinding()]
param(
    [Parameter(Mandatory)][switch] $Initialize,
    [ValidateSet(2, 3)][int] $Version = 2
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $Initialize -or $env:GITHUB_ACTIONS -eq 'true') {
    throw 'Owner-key initialization requires an explicit local invocation outside GitHub Actions.'
}
if (-not $env:LOCALAPPDATA) { throw 'The current user has no local application-data directory.' }

$legacyPublic = Join-Path $PSScriptRoot 'hosted-gui-public-v1.pem'
$newPublic = Join-Path $PSScriptRoot "hosted-gui-public-v$Version.pem"
if (-not (Test-Path -LiteralPath $legacyPublic -PathType Leaf)) {
    throw 'The archived legacy public key is missing.'
}
if (Test-Path -LiteralPath $newPublic) { throw 'The selected versioned public-key file already exists.' }
if ($Version -eq 3 -and -not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'hosted-gui-public-v2.pem') -PathType Leaf)) {
    throw 'The archived version 2 public key is missing.'
}

$keyDirectory = Join-Path $env:LOCALAPPDATA 'BambuStudio\HostedGuiEvidence\keys'
foreach ($target in @($keyDirectory, $PSScriptRoot)) {
    $ancestor = [System.IO.Path]::GetFullPath($target)
    while ($ancestor) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw 'Key initialization refuses reparse ancestors.'
        }
        $ancestor = [System.IO.Path]::GetDirectoryName($ancestor)
    }
}
$rsa = [System.Security.Cryptography.RSA]::Create(3072)
$privateBytes = $null
$protectedBytes = $null
$publicBytes = $null
try {
    $publicBytes = $rsa.ExportSubjectPublicKeyInfo()
    $keyId = ([Convert]::ToHexString(
        [System.Security.Cryptography.SHA256]::HashData($publicBytes))).ToLowerInvariant()
    $slot = Join-Path $keyDirectory ($keyId + '.dpapi')
    if (Test-Path -LiteralPath $slot) { throw 'The selected protected key slot already exists.' }
    $privateBytes = $rsa.ExportPkcs8PrivateKey()
    $protectedBytes = [System.Security.Cryptography.ProtectedData]::Protect(
        $privateBytes, $null, [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
    [void][System.IO.Directory]::CreateDirectory($keyDirectory)
    $stream = [System.IO.File]::Open($slot, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try { $stream.Write($protectedBytes, 0, $protectedBytes.Length) }
    finally { $stream.Dispose() }
    $publicPem = $rsa.ExportSubjectPublicKeyInfoPem()
    $publicStream = [System.IO.File]::Open($newPublic, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $pemBytes = [System.Text.Encoding]::ASCII.GetBytes($publicPem)
        $publicStream.Write($pemBytes, 0, $pemBytes.Length)
    }
    finally { $publicStream.Dispose() }
    Write-Host "Version $Version public key created with SPKI SHA-256 $keyId. Keep the protected slot local; review and publish only the public PEM."
}
finally {
    $rsa.Dispose()
    foreach ($bytes in @($privateBytes, $protectedBytes, $publicBytes)) {
        if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
}
