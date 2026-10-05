#requires -Version 7.5
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $BundlePath,
    [Parameter(Mandatory)][string] $EnvelopePath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^\d{1,20}$')][string] $ExpectedRunId,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedCommit,
    [Parameter(Mandatory)][ValidateSet('before_selector','expanded_selector')][string] $ExpectedPhase
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
function Is-SerializedEmptyRectangle($Value) {
    return ($Value -is [array] -and $Value.Count -eq 4 -and
        $Value[0] -is [string] -and $Value[1] -is [string] -and
        $Value[2] -is [string] -and $Value[3] -is [string] -and
        $Value[0] -ceq 'Infinity' -and $Value[1] -ceq 'Infinity' -and
        $Value[2] -ceq '-Infinity' -and $Value[3] -ceq '-Infinity')
}
function Control-Rectangle($Value, [bool] $Offscreen) {
    if (Is-SerializedEmptyRectangle $Value) {
        if (-not $Offscreen) { $script:validationPhase = 'onscreen_empty_rectangle'; Require $false }
        return $false # Explicitly unavailable, never measured coordinates.
    }
    Rect $Value
    return $true
}
function Rect($Value) {
    if ($Value -isnot [array] -or $Value.Count -ne 4) {
        $script:validationPhase = 'rectangle_shape'; Require $false
    }
    # Observe the exact serialized WPF Rect.Empty representation without
    # admitting it as measured geometry or publishing any coordinates.
    if (Is-SerializedEmptyRectangle $Value) {
        $script:validationPhase = 'rectangle_serialized_empty'; Require $false
    }
    foreach ($number in $Value) {
        if ($number -isnot [long] -and $number -isnot [int] -and $number -isnot [double]) {
            $script:validationPhase = 'rectangle_number_type'; Require $false
        }
        if (-not [double]::IsFinite([double]$number)) {
            $script:validationPhase = 'rectangle_nonfinite'; Require $false
        }
        if ([Math]::Abs([double]$number) -gt 1000000) {
            $script:validationPhase = 'rectangle_bound'; Require $false
        }
    }
}
function Binding($Value) {
    Fields $Value @('protocol','run_id','source_commit','phase','captured_at_utc','width','height','capture_method',
        'png_sha256','cheap_sha256','helper_sha256','diagnostic_sha256')
    foreach ($name in @('protocol','run_id','source_commit','phase','captured_at_utc','capture_method',
        'png_sha256','cheap_sha256','helper_sha256','diagnostic_sha256')) { Require ($Value.$name -is [string]) }
    Require ($Value.protocol -ceq 'hosted-scale-diagnostic-v1' -and $Value.run_id -ceq $ExpectedRunId -and
        $Value.source_commit -ceq $ExpectedCommit -and $Value.phase -ceq $ExpectedPhase -and $Value.capture_method -ceq 'cheap_exact_hwnd')
    Integer $Value.width 1 8192; Integer $Value.height 1 8192
    foreach ($name in @('png_sha256','cheap_sha256','helper_sha256','diagnostic_sha256')) {
        Require ($Value.$name -is [string] -and $Value.$name -cmatch '^[0-9a-f]{64}$')
    }
    Require ($Value.captured_at_utc -is [string] -and $Value.captured_at_utc -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$')
    [void](Timestamp $Value.captured_at_utc)
}
function Same-Binding($Left,$Right) {
    Binding $Left; Binding $Right
    foreach ($property in $Left.PSObject.Properties) { Require ($property.Value -ceq $Right.($property.Name)) }
}
$rsa = $null; $privateBytes = $null; $key = $null; $plain = $null; $pixels = $null; $stage = $null
$validationPhase = 'output_boundary'
try {
    $final = Plain-Path $OutputDirectory
    $parent = [IO.Path]::GetDirectoryName($final)
    Require ((Test-Path -LiteralPath $parent -PathType Container) -and -not (Test-Path -LiteralPath $final))
    $validationPhase = 'envelope_parse'
    $envelope = Parse-Json (Read-Bounded $EnvelopePath 32768)
    $validationPhase = 'envelope_schema'
    Fields $envelope @('schema','protocol','recipient','recipient_sha256','aad_base64','binding','wrapped_key','nonce','tag','ciphertext_sha256')
    foreach ($name in @('protocol','recipient','recipient_sha256','aad_base64','wrapped_key','nonce','tag','ciphertext_sha256')) {
        Require ($envelope.$name -is [string])
    }
    Integer $envelope.schema 1 1
    Require ($envelope.protocol -ceq 'hosted-scale-diagnostic-v1' -and $envelope.recipient -ceq 'hosted-automation-public-v1.pem')
    $validationPhase = 'envelope_binding'
    Binding $envelope.binding
    $validationPhase = 'envelope_hash_fields'
    foreach ($name in @('recipient_sha256','ciphertext_sha256')) { Require ($envelope.$name -cmatch '^[0-9a-f]{64}$') }
    $validationPhase = 'aad_decode'
    $aad = [Convert]::FromBase64String($envelope.aad_base64)
    Require ($aad.Length -gt 0 -and $aad.Length -le 8192)
    $validationPhase = 'aad_parse'
    $authenticatedBinding = Parse-Json $aad
    $validationPhase = 'aad_binding'
    Same-Binding $authenticatedBinding $envelope.binding
    $validationPhase = 'ciphertext_hash'
    $cipher = Read-Bounded $BundlePath 16777216
    Require ((Hash-Bytes $cipher) -ceq $envelope.ciphertext_sha256)
    $validationPhase = 'encryption_parameters'
    $nonce = [Convert]::FromBase64String($envelope.nonce); $tag = [Convert]::FromBase64String($envelope.tag)
    $wrapped = [Convert]::FromBase64String($envelope.wrapped_key)
    Require ($nonce.Length -eq 12 -and $tag.Length -eq 16 -and $wrapped.Length -le 1024)
    $validationPhase = 'recipient_public_binding'
    $rsa = [Security.Cryptography.RSA]::Create()
    $rsa.ImportFromPem([Text.Encoding]::UTF8.GetString((Read-Bounded (Join-Path $PSScriptRoot 'hosted-automation-public-v1.pem') 16384)))
    $keyId = Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())
    Require ($keyId -ceq $envelope.recipient_sha256 -and $wrapped.Length -eq ($rsa.KeySize/8))
    $validationPhase = 'protected_custody'
    $privatePath = Join-Path $env:LOCALAPPDATA ("BambuStudio/HostedAutomationEvidence/keys/$keyId.dpapi")
    $privateBytes = [Security.Cryptography.ProtectedData]::Unprotect((Read-Bounded $privatePath 32768),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    $validationPhase = 'private_recipient_binding'
    $read = 0; $rsa.ImportPkcs8PrivateKey($privateBytes,[ref]$read)
    Require ($read -eq $privateBytes.Length -and (Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())) -ceq $keyId)
    $validationPhase = 'key_unwrap'
    $key = $rsa.Decrypt($wrapped,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    Require ($key.Length -eq 32)
    $validationPhase = 'authenticated_decryption'
    $plain = [byte[]]::new($cipher.Length)
    $aes = [Security.Cryptography.AesGcm]::new($key,16)
    try { $aes.Decrypt($nonce,$cipher,$tag,$plain,$aad) } finally { $aes.Dispose() }
    $validationPhase = 'inventory_parse'
    $inventory = Parse-Json $plain
    $validationPhase = 'inventory_schema'
    Fields $inventory @('schema','binding','observed_at_utc','settings_pid','settings_start','owned_root','bounds',
        'foreground_owned','settings_dpi','selected_percent','controls','png_base64')
    Integer $inventory.schema 1 1
    $validationPhase = 'inventory_binding'
    Same-Binding $inventory.binding $authenticatedBinding
    $validationPhase = 'inventory_timestamps'
    Require ($inventory.observed_at_utc -is [string] -and $inventory.observed_at_utc -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$')
    $observed = Timestamp $inventory.observed_at_utc
    Require ($observed -le (Timestamp $inventory.binding.captured_at_utc))
    $validationPhase = 'inventory_identity_and_scale'
    Integer $inventory.settings_pid 1 4294967295; Integer $inventory.settings_start 1 ([long]::MaxValue)
    Integer $inventory.owned_root 1 ([long]::MaxValue); Integer $inventory.settings_dpi 96 192
    Integer $inventory.selected_percent 100 200
    Require ($inventory.foreground_owned -is [bool] -and $inventory.selected_percent -in @(100,125,150,200))
    $validationPhase = 'inventory_frame'
    Rect $inventory.bounds
    Require (($inventory.bounds[2]-$inventory.bounds[0]) -eq $inventory.binding.width -and
        ($inventory.bounds[3]-$inventory.bounds[1]) -eq $inventory.binding.height)
    $validationPhase = 'control_count'
    Require ($inventory.controls -is [array] -and $inventory.controls.Count -le 1000)
    $unavailableControlRectangles = 0
    foreach ($control in $inventory.controls) {
        $validationPhase = 'control_schema'
        Fields $control @('name','automation_id','type','enabled','offscreen','rect','patterns','scroll')
        foreach ($name in @('name','automation_id','type')) { Require ($control.$name -is [string] -and $control.$name.Length -le 2048) }
        Require ($control.enabled -is [bool] -and $control.offscreen -is [bool])
        $validationPhase = 'control_rectangle'
        if (-not (Control-Rectangle $control.rect $control.offscreen)) { $unavailableControlRectangles++ }
        $validationPhase = 'control_patterns'
        Require ($control.patterns -is [array] -and $control.patterns.Count -le 32)
        foreach ($pattern in $control.patterns) { Require ($pattern -is [string] -and $pattern.Length -le 256) }
        if ($null -ne $control.scroll) {
            $validationPhase = 'control_scroll'
            Fields $control.scroll @('horizontal','vertical','horizontal_percent','vertical_percent','horizontal_view','vertical_view')
            Require ($control.scroll.horizontal -is [bool] -and $control.scroll.vertical -is [bool])
            foreach ($name in @('horizontal_percent','vertical_percent','horizontal_view','vertical_view')) {
                $number = $control.scroll.$name
                Require (($number -is [long] -or $number -is [int] -or $number -is [double]) -and
                    [double]::IsFinite([double]$number) -and $number -ge -1 -and $number -le 100)
            }
        }
    }
    $validationPhase = 'png_decode_and_hash'
    Require ($inventory.png_base64 -is [string] -and $inventory.png_base64.Length -le 11184812)
    $pixels = [Convert]::FromBase64String($inventory.png_base64)
    Require ($pixels.Length -ge 45 -and $pixels.Length -le 8388608 -and (Hash-Bytes $pixels) -ceq $inventory.binding.png_sha256)
    $validationPhase = 'png_header'
    Require ([Convert]::ToHexString($pixels[0..7]) -ceq '89504E470D0A1A0A' -and
        [Convert]::ToHexString($pixels[8..11]) -ceq '0000000D' -and [Text.Encoding]::ASCII.GetString($pixels,12,4) -ceq 'IHDR')
    $validationPhase = 'png_dimensions_and_end'
    $width = [uint32]$pixels[16]*16777216+[uint32]$pixels[17]*65536+[uint32]$pixels[18]*256+$pixels[19]
    $height = [uint32]$pixels[20]*16777216+[uint32]$pixels[21]*65536+[uint32]$pixels[22]*256+$pixels[23]
    Require ($width -eq $inventory.binding.width -and $height -eq $inventory.binding.height)
    Require ([Convert]::ToHexString($pixels[($pixels.Length-12)..($pixels.Length-1)]) -ceq '0000000049454E44AE426082')
    # All authentication, bindings and bounded structure checks precede writes.
    $validationPhase = 'output_staging'
    [void](Plain-Path $final)
    Require (-not (Test-Path -LiteralPath $final))
    $stage = Join-Path $parent ('.scale-diagnostic-stage-' + [Guid]::NewGuid().ToString('N'))
    Require (-not (Test-Path -LiteralPath $stage))
    [void][IO.Directory]::CreateDirectory($stage)
    [void](Plain-Path $stage)
    $validationPhase = 'output_files'
    Write-NewFile (Join-Path $stage 'capture.png') $pixels
    $inventory.PSObject.Properties.Remove('png_base64')
    Write-NewFile (Join-Path $stage 'private-inventory.json') ([Text.Encoding]::UTF8.GetBytes(($inventory | ConvertTo-Json -Depth 16)))
    $validation = @{schema=1; protocol='hosted-scale-diagnostic-v1'; run_id=$ExpectedRunId; source_commit=$ExpectedCommit; phase=$ExpectedPhase
        captured_at_utc=$inventory.binding.captured_at_utc; integrity='verified'; pixel_review='unverified'; privacy_review='unverified'
        publication='not_authorized'; png_sha256=$inventory.binding.png_sha256; width=$width; height=$height
        unavailable_control_rectangles=$unavailableControlRectangles
        unavailable_geometry_reason=$(if ($unavailableControlRectangles -gt 0) {'offscreen_uia_empty'} else {$null})
    } | ConvertTo-Json
    Write-NewFile (Join-Path $stage 'validation.json') ([Text.Encoding]::UTF8.GetBytes($validation))
    $validationPhase = 'output_publish'
    [void](Plain-Path $final); Require (-not (Test-Path -LiteralPath $final))
    [IO.Directory]::Move($stage,$final); $stage = $null
    Write-Host 'Scale diagnostic integrity verified. Pixel and privacy review remain unverified; publication is not authorized.'
} catch {
    # This variable is assigned fixed literals only, never exception or input data.
    throw ('Scale diagnostic opening failed at fixed phase: ' + $validationPhase + '. No output was published.')
} finally {
    if ($null -ne $rsa) { $rsa.Dispose() }
    foreach ($bytes in @($privateBytes,$key,$plain,$pixels)) {
        if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
    if ($stage) { Write-Warning 'Incomplete restricted staging was preserved; no publication is authorized.' }
}
