[CmdletBinding()]
param([Parameter(Mandatory)][switch] $Initialize)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $Initialize -or $env:GITHUB_ACTIONS -eq 'true') {
    throw 'Owner-key initialization requires an explicit local invocation outside GitHub Actions.'
}
if (-not $env:LOCALAPPDATA) { throw 'The current user has no local application-data directory.' }

$legacyPublic = Join-Path $PSScriptRoot 'hosted-gui-public-v1.pem'
$newPublic = Join-Path $PSScriptRoot 'hosted-gui-public-v2.pem'
if (-not (Test-Path -LiteralPath $legacyPublic -PathType Leaf)) {
    throw 'The archived legacy public key is missing.'
}
if (Test-Path -LiteralPath $newPublic) { throw 'The version 2 public-key file already exists.' }

$keyDirectory = Join-Path $env:LOCALAPPDATA 'BambuStudio\HostedGuiEvidence\keys'
$rsa = [System.Security.Cryptography.RSA]::Create(3072)
$privateBytes = $null
$protectedBytes = $null
$publicBytes = $null
try {
    $publicBytes = $rsa.ExportSubjectPublicKeyInfo()
    $keyId = ([Convert]::ToHexString(
        [System.Security.Cryptography.SHA256]::HashData($publicBytes))).ToLowerInvariant()
    $slot = Join-Path $keyDirectory ($keyId + '.dpapi')
    if (Test-Path -LiteralPath $slot) { throw 'The version 2 protected key slot already exists.' }
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
    Write-Host "Version 2 public key created with SPKI SHA-256 $keyId. Keep the protected slot local; review and publish only the public PEM."
}
finally {
    $rsa.Dispose()
    foreach ($bytes in @($privateBytes, $protectedBytes, $publicBytes)) {
        if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
}
