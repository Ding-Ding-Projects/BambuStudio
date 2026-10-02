[CmdletBinding()]
param([Parameter(Mandatory)][switch] $Initialize)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $Initialize -or $env:GITHUB_ACTIONS -eq 'true') {
    throw 'Automation evidence key initialization requires an explicit local administrative invocation outside GitHub Actions.'
}
if (-not $env:LOCALAPPDATA) { throw 'Current-user application data is unavailable.' }
$publicPath = Join-Path $PSScriptRoot 'hosted-automation-public-v1.pem'
if (Test-Path -LiteralPath $publicPath) { throw 'The automation recipient already exists; initialization never overwrites it.' }
$keyDirectory = Join-Path $env:LOCALAPPDATA 'BambuStudio/HostedAutomationEvidence/keys'
if ((Test-Path -LiteralPath $keyDirectory) -and
    ((Get-Item -LiteralPath $keyDirectory).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'The protected local key directory may not be a reparse point.'
}
$rsa = [Security.Cryptography.RSA]::Create(3072)
$privateBytes = $null
$protectedBytes = $null
$verificationBytes = $null
try {
    $keyId = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo())).ToLowerInvariant()
    $slot = Join-Path $keyDirectory ($keyId + '.dpapi')
    if (Test-Path -LiteralPath $slot) { throw 'The protected automation key slot already exists.' }
    $privateBytes = $rsa.ExportPkcs8PrivateKey()
    $protectedBytes = [Security.Cryptography.ProtectedData]::Protect(
        $privateBytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    # Administrative cryptographic handling only, never a product launch or runtime check.
    $verificationBytes = [Security.Cryptography.ProtectedData]::Unprotect(
        $protectedBytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    if (-not [Security.Cryptography.CryptographicOperations]::FixedTimeEquals($privateBytes, $verificationBytes)) {
        throw 'Current-user protection could not be verified; no key files were created.'
    }
    [void][IO.Directory]::CreateDirectory($keyDirectory)
    $stream = [IO.File]::Open($slot, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($protectedBytes, 0, $protectedBytes.Length) }
    finally { $stream.Dispose() }
    $pemBytes = [Text.Encoding]::ASCII.GetBytes($rsa.ExportSubjectPublicKeyInfoPem())
    $stream = [IO.File]::Open($publicPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($pemBytes, 0, $pemBytes.Length) }
    finally { $stream.Dispose() }
    Write-Host 'Dedicated automation evidence public recipient created. Keep the protected private slot local; commit only the public PEM.'
} finally {
    $rsa.Dispose()
    foreach ($bytes in @($privateBytes, $protectedBytes, $verificationBytes)) {
        if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
}
