#requires -Version 7.5
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $BundlePath,
    [Parameter(Mandatory)][string] $EnvelopePath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^\d{1,20}$')][string] $ExpectedRunId,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedCommit,
    [Parameter(Mandatory)][ValidateSet('display','colors')][string] $ExpectedDestination,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string] $ExpectedProbeSha256
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Require([bool] $Value) { if (-not $Value) { throw 'Diagnostic integrity contract failed.' } }
function Hash-Bytes([byte[]] $Bytes) { return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant() }
function Plain-Path([string] $Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            Require (-not ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint))
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}
function Read-Bounded([string] $Path, [long] $Limit) {
    $full = Plain-Path $Path
    $file = Get-Item -LiteralPath $full
    Require (-not $file.PSIsContainer -and $file.Length -gt 0 -and $file.Length -le $Limit)
    $stream = [IO.File]::Open($full,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        Require ($stream.Length -gt 0 -and $stream.Length -le $Limit)
        $bytes = [byte[]]::new([int]$stream.Length)
        $offset = 0
        while ($offset -lt $bytes.Length) {
            $count = $stream.Read($bytes,$offset,$bytes.Length-$offset)
            Require ($count -gt 0); $offset += $count
        }
        Require ($stream.ReadByte() -eq -1)
        return ,$bytes
    } finally { $stream.Dispose() }
}
function Write-NewFile([string] $Path, [byte[]] $Bytes) {
    $stream = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.Write($Bytes,0,$Bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
}
function Timestamp([string] $Value) {
    Require ($Value -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$')
    return [DateTime]::ParseExact($Value,"yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'",[Globalization.CultureInfo]::InvariantCulture,
        ([Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal))
}
function Check-JsonElement($Element) {
    if ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object) {
        $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($property in $Element.EnumerateObject()) {
            Require ($names.Add($property.Name))
            Check-JsonElement $property.Value
        }
    } elseif ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array) {
        foreach ($item in $Element.EnumerateArray()) { Check-JsonElement $item }
    }
}
function Parse-Json([byte[]] $Bytes) {
    $utf8 = [Text.UTF8Encoding]::new($false,$true)
    $text = $utf8.GetString($Bytes).TrimStart([char]0xFEFF)
    $options = [Text.Json.JsonDocumentOptions]::new(); $options.MaxDepth = 16
    $document = [Text.Json.JsonDocument]::Parse($text,$options)
    try {
        Require ($document.RootElement.ValueKind -eq [Text.Json.JsonValueKind]::Object)
        Check-JsonElement $document.RootElement
    } finally { $document.Dispose() }
    return ($text | ConvertFrom-Json -Depth 16 -DateKind String)
}
function Fields($Object, [string[]] $Names) {
    Require ($null -ne $Object -and $Object -is [pscustomobject])
    $actual = @($Object.PSObject.Properties.Name)
    Require ($actual.Count -eq $Names.Count)
    foreach ($name in $actual) { Require ($Names -ccontains $name) }
}
function Integer($Value, [long] $Minimum, [long] $Maximum) {
    Require (($Value -is [long] -or $Value -is [int]) -and $Value -ge $Minimum -and $Value -le $Maximum)
}

$rsa = $null; $privateBytes = $null; $key = $null; $plain = $null; $stage = $null
$phase = 'output_boundary'
try {
    $final = Plain-Path $OutputDirectory
    $parent = [IO.Path]::GetDirectoryName($final)
    Require ((Test-Path -LiteralPath $parent -PathType Container) -and -not (Test-Path -LiteralPath $final))
    $phase = 'envelope_schema'
    $envelope = Parse-Json (Read-Bounded $EnvelopePath 32768)
    Fields $envelope @('schema','protocol','algorithm','aad','recipient','wrapped_key','nonce','tag','ciphertext_sha256')
    Integer $envelope.schema 2 2
    foreach ($name in @('protocol','algorithm','aad','recipient','wrapped_key','nonce','tag','ciphertext_sha256')) {
        Require ($envelope.$name -is [string])
    }
    Require ($envelope.protocol -ceq 'display-scale-inventory-v2' -and
        $envelope.algorithm -ceq 'AES-256-GCM/RSA-OAEP-SHA256' -and $envelope.aad -ceq 'none' -and
        $envelope.recipient -ceq 'hosted-automation-public-v1.pem' -and
        $envelope.ciphertext_sha256 -cmatch '^[0-9a-f]{64}$')
    $phase = 'ciphertext_hash'
    $cipher = Read-Bounded $BundlePath 4194304
    Require ((Hash-Bytes $cipher) -ceq $envelope.ciphertext_sha256)
    $phase = 'encryption_parameters'
    $nonce = [Convert]::FromBase64String($envelope.nonce)
    $tag = [Convert]::FromBase64String($envelope.tag)
    $wrapped = [Convert]::FromBase64String($envelope.wrapped_key)
    Require ($nonce.Length -eq 12 -and $tag.Length -eq 16 -and $wrapped.Length -le 1024)
    $phase = 'recipient_public_binding'
    $rsa = [Security.Cryptography.RSA]::Create()
    $rsa.ImportFromPem([Text.Encoding]::UTF8.GetString((Read-Bounded (Join-Path $PSScriptRoot 'hosted-automation-public-v1.pem') 16384)))
    $keyId = Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())
    Require ($wrapped.Length -eq ($rsa.KeySize/8))
    $phase = 'protected_custody'
    $privatePath = Join-Path $env:LOCALAPPDATA ("BambuStudio/HostedAutomationEvidence/keys/$keyId.dpapi")
    $privateBytes = [Security.Cryptography.ProtectedData]::Unprotect((Read-Bounded $privatePath 32768),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    $phase = 'private_recipient_binding'
    $read = 0; $rsa.ImportPkcs8PrivateKey($privateBytes,[ref]$read)
    Require ($read -eq $privateBytes.Length -and (Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())) -ceq $keyId)
    $phase = 'key_unwrap'
    $key = $rsa.Decrypt($wrapped,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    Require ($key.Length -eq 32)
    $phase = 'authenticated_decryption'
    $plain = [byte[]]::new($cipher.Length)
    $aes = [Security.Cryptography.AesGcm]::new($key,16)
    # Version 2 explicitly authenticates the ciphertext without associated data.
    try { $aes.Decrypt($nonce,$cipher,$tag,$plain) } finally { $aes.Dispose() }
    $phase = 'inventory_schema'
    $inventory = Parse-Json $plain
    Fields $inventory @('schema','controls','binding','selection')
    Integer $inventory.schema 1 1
    $phase = 'authenticated_expected_binding'
    $binding = $inventory.binding
    Fields $binding @('requested_destination','navigation_completion','run_id','workflow_source_commit','observed_at_utc','probe_sha256')
    foreach ($name in @('requested_destination','navigation_completion','run_id','workflow_source_commit','observed_at_utc','probe_sha256')) {
        Require ($binding.$name -is [string])
    }
    Require ($binding.requested_destination -ceq $ExpectedDestination -and
        $binding.navigation_completion -ceq 'unverified' -and $binding.run_id -ceq $ExpectedRunId -and
        $binding.workflow_source_commit -ceq $ExpectedCommit -and $binding.probe_sha256 -ceq $ExpectedProbeSha256)
    [void](Timestamp $binding.observed_at_utc)
    $phase = 'process_identity'
    $selection = $inventory.selection
    Fields $selection @('method','direct_root_count','frame_root_count','matched_root_count','discovery_element_count',
        'settings_pid','settings_start_ticks','root_pid','root_start_ticks','root_hwnd','selected_process_id','session_id')
    Require ($selection.method -is [string] -and $selection.method -cin @('direct_process_root','frame_host_process_descendant'))
    foreach ($name in @('settings_pid','root_pid','selected_process_id')) { Integer $selection.$name 1 4294967295 }
    foreach ($name in @('settings_start_ticks','root_start_ticks')) { Integer $selection.$name 1 ([long]::MaxValue) }
    # UIA exposes NativeWindowHandle as signed Int32; never coerce a negative
    # nonzero bit pattern into a different HWND or infer a current live window.
    Integer $selection.root_hwnd ([int]::MinValue) ([int]::MaxValue)
    Require ($selection.root_hwnd -ne 0 -and $selection.selected_process_id -eq $selection.settings_pid)
    Integer $selection.session_id 0 ([int]::MaxValue)
    Integer $selection.direct_root_count 0 1
    Integer $selection.frame_root_count 0 1000
    Integer $selection.matched_root_count 1 1
    Integer $selection.discovery_element_count 0 1000
    if ($selection.method -ceq 'direct_process_root') {
        Require ($selection.direct_root_count -eq 1 -and $selection.frame_root_count -eq 0 -and
            $selection.discovery_element_count -eq 0 -and $selection.root_pid -eq $selection.settings_pid -and
            $selection.root_start_ticks -eq $selection.settings_start_ticks)
    } else {
        Require ($selection.direct_root_count -eq 0 -and $selection.frame_root_count -ge 1 -and
            $selection.discovery_element_count -ge 1 -and $selection.root_pid -ne $selection.settings_pid)
    }
    $phase = 'control_schema'
    Require ($inventory.controls -is [array] -and $inventory.controls.Count -ge 1 -and $inventory.controls.Count -le 1000)
    foreach ($control in $inventory.controls) {
        Fields $control @('name','automation_id','type','enabled','offscreen','patterns','selected')
        foreach ($name in @('name','automation_id','type')) {
            Require ($control.$name -is [string] -and $control.$name.Length -le 2048)
        }
        Require ($control.enabled -is [bool] -and $control.offscreen -is [bool] -and
            ($null -eq $control.selected -or $control.selected -is [bool]))
        Require ($control.patterns -is [array] -and $control.patterns.Count -le 32)
        foreach ($pattern in $control.patterns) { Require ($pattern -is [string] -and $pattern.Length -le 256) }
    }
    $phase = 'output_staging'
    [void](Plain-Path $final); Require (-not (Test-Path -LiteralPath $final))
    $stage = Join-Path $parent ('.display-inventory-stage-' + [Guid]::NewGuid().ToString('N'))
    Require (-not (Test-Path -LiteralPath $stage))
    [void][IO.Directory]::CreateDirectory($stage)
    [void](Plain-Path $stage)
    # Preserve the authenticated plaintext bytes exactly; no normalization of
    # private labels or observed identities is needed to inspect the inventory.
    Write-NewFile (Join-Path $stage 'private-inventory.json') $plain
    $validation = @{schema=1; protocol='display-scale-inventory-v2'; integrity='verified'
        run_id=$ExpectedRunId; workflow_source_commit=$ExpectedCommit; requested_destination=$ExpectedDestination
        probe_sha256=$ExpectedProbeSha256; observed_at_utc=$binding.observed_at_utc
        control_count=$inventory.controls.Count; navigation_completion='unverified'; privacy_review='unverified'; publication='not_authorized'
    } | ConvertTo-Json
    Write-NewFile (Join-Path $stage 'validation.json') ([Text.Encoding]::UTF8.GetBytes($validation))
    $phase = 'output_publish'
    [void](Plain-Path $final); Require (-not (Test-Path -LiteralPath $final))
    [IO.Directory]::Move($stage,$final); $stage = $null
    Write-Host 'Display inventory integrity verified. Destination and privacy review remain unverified; publication is not authorized.'
} catch {
    throw ('Display inventory opening failed at fixed phase: ' + $phase + '. No output was published.')
} finally {
    if ($null -ne $rsa) { $rsa.Dispose() }
    foreach ($bytes in @($privateBytes,$key,$plain)) {
        if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
    if ($stage) { Write-Warning 'Incomplete restricted staging was preserved; no publication is authorized.' }
}
