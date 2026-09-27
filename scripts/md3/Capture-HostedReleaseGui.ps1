[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $InstallReceipt,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string] $VerificationCommit,
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [string] $BehaviorDirectory,
    [ValidateSet('diagnostic', 'behavior')][string] $CaptureScope = 'behavior',
    [Parameter(Mandatory)][string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Test-ExactContractArray {
    param([object] $Observed, [object] $Expected)
    if ($Observed -isnot [array] -or $Expected -isnot [array] -or
        $Observed.Count -ne $Expected.Count) { return $false }
    for ($index = 0; $index -lt $Observed.Count; $index++) {
        if ($Observed[$index] -isnot [string] -or $Expected[$index] -isnot [string] -or
            -not [string]::Equals($Observed[$index], $Expected[$index],
                                  [System.StringComparison]::Ordinal)) {
            return $false
        }
    }
    return $true
}
$legacyMode = [string]::IsNullOrWhiteSpace($BehaviorDirectory)
if (-not $legacyMode -and [string]::IsNullOrWhiteSpace($VerificationCommit)) {
    throw 'Schema v2 requires an exact verifier commit.'
}

if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'Hosted GUI capture requires a disposable GitHub-hosted Windows runner.'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Capture output directory already exists.' }
[void](New-Item -ItemType Directory -Path $OutputDirectory)
$evidence = [ordered]@{
    schema = if ($legacyMode) { 1 } else { 2 }
    status = 'failed'
    run_id = $env:GITHUB_RUN_ID
    source_commit = $ExpectedCommit.ToLowerInvariant()
    verification_commit = if ($legacyMode) { $null } else { $VerificationCommit.ToLowerInvariant() }
    release_tag = $Tag
    package_version = $null
    installed_exe_sha256 = $null
    installer_sha256 = $null
    runner = 'github-hosted-windows'
    capture_method = 'lowlevel-computer-use-cheap hidden desktop PrintWindow'
    capture_tool_commit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    privacy = 'fresh disposable runner profile; raw images withheld from public workflow artifacts; no pixel privacy review'
    image_availability = 'not_uploaded'
    captures = @()
    manifest = @()
    capture_failure = $null
    behavior_failure = $null
    capture_scope = if ($legacyMode) { 'legacy' } else { $CaptureScope }
    diagnostics_excluded = $null
}
$zipPath = $null
$stageRoot = $null

try {
    $receipt = Get-Content -LiteralPath $InstallReceipt -Raw | ConvertFrom-Json
    if ($receipt.status -cne 'verified' -or $receipt.source_commit -cne $ExpectedCommit.ToLowerInvariant() -or
        $receipt.release_tag -cne $Tag) {
        throw 'The isolated installation receipt does not match this published source and release.'
    }
    $installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
    $exe = Join-Path (Join-Path $installRoot "app-$($receipt.package_version)") 'bambu-studio.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'The verified installed executable is missing.' }
    $exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($exeHash -cne $receipt.installed_exe_sha256 -or $exeHash -cne $receipt.package_exe_sha256) {
        throw 'The installed executable changed after Squirrel verification.'
    }
    $evidence.package_version = $receipt.package_version
    $evidence.installed_exe_sha256 = $exeHash
    $evidence.installer_sha256 = $receipt.asset_sha256.'Setup.exe'

    $images = Join-Path $env:RUNNER_TEMP ('bambu-capture-images-' + $env:GITHUB_RUN_ID)
    $files = @()
    if ($legacyMode) {
    [void](New-Item -ItemType Directory -Path $images)
    try {
    if (-not $env:LLCU_CHEAP -or -not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) {
        if (-not $legacyMode) { throw 'The pinned job-local headless capture tool is missing.' }
        $toolRoot = Join-Path $env:RUNNER_TEMP ('lowlevel-capture-' + $env:GITHUB_RUN_ID)
        $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
        & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
        if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the pinned headless capture tool.' }
        & git -C $toolRoot checkout --quiet --detach $toolCommit
        if ($LASTEXITCODE -ne 0 -or (& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) {
            throw 'Headless capture tool source mismatch.'
        }
        $venv = Join-Path $env:RUNNER_TEMP ('lowlevel-capture-venv-' + $env:GITHUB_RUN_ID)
        & py -3 -m venv $venv
        if ($LASTEXITCODE -ne 0) { throw 'Python could not create an isolated capture environment.' }
        $legacyPython = Join-Path $venv 'Scripts\python.exe'
        & $legacyPython -m pip install --disable-pip-version-check --quiet $toolRoot
        if ($LASTEXITCODE -ne 0) { throw 'Could not bootstrap the pinned headless capture tool.' }
        $env:LLCU_CHEAP = Join-Path $venv 'Scripts\lowlevel-computer-use-cheap.exe'
    }
    $python = Join-Path (Split-Path -Parent $env:LLCU_CHEAP) 'python.exe'
    if (-not (Test-Path -LiteralPath $python -PathType Leaf)) { throw 'The isolated Python executable is missing.' }

    $dataRoot = Join-Path $env:RUNNER_TEMP ('bambu-capture-profile-' + $env:GITHUB_RUN_ID)
    & $python (Join-Path $PSScriptRoot 'prepare-capture-datadirs.py') $dataRoot `
        --languages en --themes light --densities comfortable
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare a fresh capture profile.' }
    $tuple = 'en-light-comfortable'
    $dataDir = Join-Path $dataRoot $tuple
    & $python (Join-Path $PSScriptRoot 'capture-tuple.py') `
        --exe $exe --datadir $dataDir --tuple $tuple --out $images `
        --suffix hosted --desktop ('bambu-' + $env:GITHUB_RUN_ID)
    $captureExit = $LASTEXITCODE
    if ($captureExit -ne 0) { throw "Hidden-desktop capture exited with code $captureExit." }

    $files = @(Get-ChildItem -LiteralPath $images -File -Filter '*.png' | Sort-Object Name)
    if ($files.Count -ne 11) { $evidence.capture_failure = "Expected 11 captured surfaces, found $($files.Count)." }
    $expectedNames = @('home', 'prepare', 'preview', 'device', 'project', 'ink',
        'preferences-appearance', 'preferences-general', 'preferences-user',
        'preferences-3d', 'preferences-other') |
        ForEach-Object { "${_}--en-light-comfortable--hosted.png" }
    if (@($files | Where-Object { $expectedNames -cnotcontains $_.Name }).Count -gt 0) {
        throw 'The capture set contains an unexpected surface.'
    }
    foreach ($file in $files) {
        if ($file.Length -lt 10000 -or $file.Length -gt 33554432) {
            throw "Capture '$($file.Name)' is outside the 10 KiB to 32 MiB image limit."
        }
        $imageJson = & $python -c 'import json,sys; from PIL import Image,ImageStat; im=Image.open(sys.argv[1]).convert("RGB"); w,h=im.size; im.thumbnail((128,128)); colors=im.getcolors(maxcolors=65536); print(json.dumps({"width":w,"height":h,"distinct_colors":len(colors) if colors is not None else 65536,"max_channel_stddev":max(ImageStat.Stat(im).stddev)}))' $file.FullName
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect captured pixels in '$($file.Name)'." }
        $image = $imageJson | ConvertFrom-Json
        if ($image.width -lt 700 -or $image.height -lt 500 -or
            $image.distinct_colors -lt 12 -or $image.max_channel_stddev -lt 10) {
            throw "Capture '$($file.Name)' is blank, uniform, or below the expected viewport."
        }
        $evidence.captures += [ordered]@{
            surface = $file.BaseName
            bytes = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            width = $image.width
            height = $image.height
            distinct_colors = $image.distinct_colors
        }
    }
    }
    catch {
        throw
    }
    }
    if (-not $legacyMode) {
    $behaviorRoot = [System.IO.Path]::GetFullPath($BehaviorDirectory)
    $runnerRoot = [System.IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
    if (-not $behaviorRoot.StartsWith($runnerRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $behaviorRoot -PathType Container)) {
        throw 'Behavior evidence must be inside the disposable runner directory.'
    }
    $tupleDirs = @(Get-ChildItem -LiteralPath $behaviorRoot -Directory | Sort-Object Name)
    if ($tupleDirs.Count -lt 1 -or $tupleDirs.Count -gt 8) { throw 'Expected one to eight behavior tuple directories.' }
    $expectedTupleCount = if ($CaptureScope -eq 'diagnostic') { 1 } else { 8 }
    if ($tupleDirs.Count -ne $expectedTupleCount) {
        $evidence.behavior_failure = 'The behavior tuple set is incomplete for the selected verification scope.'
    }
    $stageRoot = Join-Path $env:RUNNER_TEMP ('bambu-encrypt-stage-' + $env:GITHUB_RUN_ID)
    if (Test-Path -LiteralPath $stageRoot) { throw 'Encrypted evidence staging directory already exists.' }
    [void](New-Item -ItemType Directory -Path $stageRoot)
    [void](New-Item -ItemType Directory -Path (Join-Path $stageRoot 'captures'))
    foreach ($file in $files) {
        $relative = 'captures/' + $file.Name
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stageRoot 'captures' $file.Name)
        $evidence.manifest += [ordered]@{
            path = $relative; kind = 'capture'; tuple = 'en-light-comfortable'
            bytes = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    [void](New-Item -ItemType Directory -Path (Join-Path $stageRoot 'behavior'))
    $imageCount = 0
    foreach ($dir in $tupleDirs) {
        if ($dir.Name -cnotmatch '^(en|yue_HK|bilingual_en_yue_HK)-(light|dark)-(1|1\.25|1\.5|2)-(1200x800|1000x600)$' -or
            ($dir.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw 'Behavior tuple directory name or type is invalid.'
        }
        $reportPath = Join-Path $dir.FullName 'behavior-report.json'
        if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw 'A behavior tuple report is missing.' }
        $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        if ($report.source_commit -cne $evidence.source_commit -or
            $report.verification_commit -cne $evidence.verification_commit -or
            $report.release_tag -cne $Tag -or $report.hosted_run_id -cne $env:GITHUB_RUN_ID -or
            $report.installed_exe_sha256 -cne $exeHash) {
            throw "Behavior tuple '$($dir.Name)' has a different source, verifier, run, tag, or executable."
        }
        if ($null -eq $report.requested_tuple) { throw "Behavior tuple '$($dir.Name)' omitted requested tuple metadata." }
        $parts = $dir.Name.Split('-')
        $expectedLanguage = $parts[0]
        $expectedTheme = $parts[1]
        $expectedScale = [double]::Parse($parts[2], [System.Globalization.CultureInfo]::InvariantCulture)
        if ($report.requested_tuple.language -cne $expectedLanguage -or
            $report.requested_tuple.theme -cne $expectedTheme -or
            [double]$report.requested_tuple.scale -ne $expectedScale -or
            [string]::Join('x', @($report.requested_tuple.viewport)) -cne $parts[3]) {
            throw "Behavior tuple '$($dir.Name)' report does not match its requested controls."
        }
        $expectedDriverScope = if ($CaptureScope -eq 'diagnostic') { 'diagnostic' } elseif (
            $dir.Name -ceq 'en-light-1-1200x800') { 'behavior' } else { 'layout' }
        if ($CaptureScope -ne 'diagnostic') {
            $contractModule = Join-Path $PSScriptRoot 'behavior_contract.py'
            $contractPython = if ($env:LLCU_CHEAP) {
                Join-Path (Split-Path -Parent $env:LLCU_CHEAP) 'python.exe'
            } else { '' }
            if (-not (Test-Path -LiteralPath $contractModule -PathType Leaf) -or
                [string]::IsNullOrWhiteSpace($contractPython) -or
                -not (Test-Path -LiteralPath $contractPython -PathType Leaf) -or
                $null -eq $report.PSObject.Properties['contract_rows'] -or
                $null -eq $report.PSObject.Properties['contract_result'] -or
                $null -eq $report.contract_rows -or $null -eq $report.contract_result -or
                $null -eq $report.PSObject.Properties['contract_source_sha256'] -or
                $null -eq $report.PSObject.Properties['contract_state'] -or
                $report.contract_state -cne 'complete' -or
                $report.contract_source_sha256 -cne
                    (Get-FileHash -LiteralPath $contractModule -Algorithm SHA256).Hash.ToLowerInvariant()) {
                $evidence.behavior_failure = 'The behavior completeness contract is missing or stale.'
            }
            else {
                $contractCheckScript = @'
import dataclasses, importlib.util, json, sys
spec = importlib.util.spec_from_file_location('behavior_contract', sys.argv[1])
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)
with open(sys.argv[2], encoding='utf-8') as stream:
    report = json.load(stream)
result = module.validate_behavior_rows(report['scope'], report['requested_tuple']['language'], report['contract_rows'])
print(json.dumps(dataclasses.asdict(result), separators=(',', ':')))
'@
                $resultJson = & $contractPython -c $contractCheckScript $contractModule $reportPath 2>$null
                if ($LASTEXITCODE -ne 0 -or -not $resultJson) {
                    $evidence.behavior_failure = 'The behavior completeness contract could not be recomputed.'
                }
                else {
                    $recomputed = $resultJson | ConvertFrom-Json
                    $actual = $report.contract_result
                    foreach ($key in @('version', 'scope', 'language', 'verdict')) {
                        if ($null -eq $actual.PSObject.Properties[$key] -or
                            [string]$actual.$key -cne [string]$recomputed.$key) {
                            $evidence.behavior_failure = 'The behavior completeness result differs from its rows.'
                        }
                    }
                    foreach ($key in @('missing', 'invalid', 'limitations', 'confirmed')) {
                        if ($null -eq $actual.PSObject.Properties[$key] -or
                            -not (Test-ExactContractArray -Observed ($actual.$key) -Expected ($recomputed.$key))) {
                            $evidence.behavior_failure = 'The behavior completeness inventory differs from its rows.'
                        }
                    }
                    $requiredVerdict = if ($expectedDriverScope -ceq 'behavior') {
                        'ready_for_pixel_review'
                    } else { 'layout_only' }
                    if ($recomputed.version -ne 2 -or
                        $recomputed.scope -cne $expectedDriverScope -or
                        $recomputed.language -cne $expectedLanguage -or
                        $recomputed.verdict -cne $requiredVerdict) {
                        $evidence.behavior_failure = 'Required behavior flow coverage is incomplete.'
                    }
                    $namedImages = @($report.images | ForEach-Object { [string]$_.file })
                    foreach ($contractRow in @($report.contract_rows)) {
                        if ($null -eq $contractRow) {
                            $evidence.behavior_failure = 'A behavior contract row is missing.'
                            continue
                        }
                        if ($contractRow.status -ceq 'probe_confirmed') {
                            if ($null -eq $contractRow.PSObject.Properties['proof'] -or
                                $null -eq $contractRow.proof -or
                                $null -eq $contractRow.proof.PSObject.Properties['capture_ids']) {
                                $evidence.behavior_failure = 'A confirmed behavior flow has no capture inventory.'
                                continue
                            }
                            foreach ($captureId in @($contractRow.proof.capture_ids)) {
                                if ($namedImages -cnotcontains [string]$captureId) {
                                    $evidence.behavior_failure = 'A confirmed behavior flow cites an unreported capture.'
                                }
                            }
                        }
                    }
                }
            }
        }
        $expectedVerdict = if ($CaptureScope -eq 'diagnostic') { 'diagnostic_only' } else { 'pending_visual_review' }
        if ($report.scope -cne $expectedDriverScope -or $report.verdict -cne $expectedVerdict) {
            $evidence.behavior_failure = 'At least one behavior tuple did not reach the expected driver scope and pending-review verdict.'
        }
        if ($null -eq $report.PSObject.Properties['measured_tuple'] -or $null -eq $report.measured_tuple) {
            if ($report.verdict -cne 'blocked') { throw 'An unblocked behavior report omitted measured tuple metadata.' }
            $evidence.behavior_failure = 'At least one behavior tuple lacked measured geometry after a blocked launch or probe.'
        }
        elseif ($report.verdict -ceq 'blocked') {
            $evidence.behavior_failure = 'At least one behavior tuple recorded a blocked launch, probe, or teardown.'
        }
        $tupleFiles = @(Get-ChildItem -LiteralPath $dir.FullName -File | Sort-Object Name)
        $expectedImages = @($report.images | ForEach-Object { [string]$_.file } | Sort-Object)
        if ($expectedImages.Count -eq 0) {
            $evidence.behavior_failure = 'At least one behavior tuple produced no rendered image.'
        }
        $actualImages = @($tupleFiles | Where-Object Extension -CEQ '.png' | ForEach-Object Name | Sort-Object)
        if ($expectedImages.Count -ne $actualImages.Count -or
            ($expectedImages.Count -gt 0 -and
            @(Compare-Object -ReferenceObject $expectedImages -DifferenceObject $actualImages).Count -ne 0)) {
            throw "Behavior tuple '$($dir.Name)' has a partial or unreported image set."
        }
        if (@($tupleFiles | Where-Object { $_.Name -cne 'behavior-report.json' -and $_.Extension -cne '.png' }).Count -gt 0) {
            throw 'Behavior tuple contains a file outside the explicit report and image inventory.'
        }
        $destination = Join-Path (Join-Path $stageRoot 'behavior') $dir.Name
        [void](New-Item -ItemType Directory -Path $destination)
        foreach ($file in $tupleFiles) {
            if ($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint -or
                $file.Name -cnotmatch '^(behavior-report\.json|[a-zA-Z0-9-]+\.png)$' -or
                $file.Length -lt 1 -or $file.Length -gt 33554432) {
                throw 'Behavior evidence file name, type, or length is invalid.'
            }
            if ($file.Extension -ceq '.png') { $imageCount++ }
            if ($file.Extension -ceq '.png') {
                $imageRow = @($report.images | Where-Object file -CEQ $file.Name)
                if ($imageRow.Count -ne 1 -or $imageRow[0].bytes -ne $file.Length -or
                    $imageRow[0].sha256 -cne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()) {
                    throw 'A behavior image differs from the report inventory.'
                }
            }
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $file.Name)
            $evidence.manifest += [ordered]@{
                path = "behavior/$($dir.Name)/$($file.Name)"
                kind = if ($file.Extension -ceq '.png') { 'behavior_image' } else { 'behavior_report' }
                tuple = $dir.Name
                bytes = $file.Length
                sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        $logDirectory = Join-Path $dir.FullName 'restricted-logs'
        $logRows = @(if ($null -ne $report.PSObject.Properties['restricted_logs']) {
            $report.restricted_logs
        })
        if (Test-Path -LiteralPath $logDirectory -PathType Container) {
            $logFiles = @(Get-ChildItem -LiteralPath $logDirectory -File | Sort-Object Name)
            if ($logFiles.Count -gt 8 -or $logFiles.Count -ne $logRows.Count -or
                @(Get-ChildItem -LiteralPath $logDirectory -Directory).Count -gt 0) {
                throw 'Restricted behavior logs do not match a bounded flat inventory.'
            }
            $logDestination = Join-Path $destination 'restricted-logs'
            [void](New-Item -ItemType Directory -Path $logDestination)
            foreach ($logFile in $logFiles) {
                $logRow = @($logRows | Where-Object file -CEQ $logFile.Name)
                if ($logFile.Name -cnotmatch '^[a-zA-Z0-9_.-]+$' -or $logFile.Name.Contains('..') -or
                    $logFile.Length -lt 1 -or $logFile.Length -gt 33554432 -or
                    ($logFile.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -or
                    $logRow.Count -ne 1 -or $logRow[0].bytes -ne $logFile.Length -or
                    $logRow[0].sha256 -cne (Get-FileHash -LiteralPath $logFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()) {
                    throw 'A restricted behavior log differs from its report inventory.'
                }
                Copy-Item -LiteralPath $logFile.FullName -Destination (Join-Path $logDestination $logFile.Name)
                $evidence.manifest += [ordered]@{
                    path = "behavior/$($dir.Name)/restricted-logs/$($logFile.Name)"
                    kind = 'restricted_diagnostic'; tuple = $dir.Name
                    bytes = $logFile.Length
                    sha256 = (Get-FileHash -LiteralPath $logFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
        }
        elseif ($logRows.Count -gt 0) { throw 'A behavior report names restricted logs that are missing.' }
    }
    $evidence.diagnostics_excluded = 'Windows Error Reporting dumps were omitted because their PID, process-creation interval, and installed executable identity were not independently attributable to this hosted run.'
    if ($imageCount -gt 256 -or $evidence.manifest.Count -gt 280) { throw 'Behavior evidence count exceeds the encryption limit.' }
    $totalPlainBytes = [long]0
    foreach ($entry in $evidence.manifest) { $totalPlainBytes += [long]$entry.bytes }
    if ($totalPlainBytes -gt 268435456) { throw 'Evidence inventory exceeds the 256 MiB plaintext limit.' }
    }
    elseif ($files.Count -ne 11) { throw 'The legacy capture path requires exactly eleven images.' }
    if ([string]::IsNullOrWhiteSpace($env:GITHUB_RUN_ID) -or $env:GITHUB_RUN_ID -notmatch '^\d+$') {
        throw 'The hosted run ID is missing or invalid.'
    }
    $zipPath = Join-Path $env:RUNNER_TEMP ('bambu-capture-' + $env:GITHUB_RUN_ID + '.zip')
    Add-Type -AssemblyName System.IO.Compression.ZipFile
    if ($legacyMode) {
        [System.IO.Compression.ZipFile]::CreateFromDirectory(
            $images, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    }
    else {
        $zipStream = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::CreateNew)
        $zipArchive = [System.IO.Compression.ZipArchive]::new(
            $zipStream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            foreach ($row in @($evidence.manifest | Sort-Object { $_['path'] })) {
                $entry = $zipArchive.CreateEntry($row.path, [System.IO.Compression.CompressionLevel]::Optimal)
                $inputStream = [System.IO.File]::OpenRead((Join-Path $stageRoot $row.path))
                $entryStream = $entry.Open()
                try { $inputStream.CopyTo($entryStream) }
                finally { $entryStream.Dispose(); $inputStream.Dispose() }
            }
        }
        finally { $zipArchive.Dispose(); $zipStream.Dispose() }
    }
    $zipFile = Get-Item -LiteralPath $zipPath
    if ($zipFile.Length -le 0 -or $zipFile.Length -gt 268435456) {
        throw 'The capture ZIP exceeds the 256 MiB encryption limit.'
    }

    if ($legacyMode) {
        $bindingLines = @('bambu-hosted-gui-v1', $env:GITHUB_RUN_ID,
            $evidence.source_commit, $Tag, $exeHash)
        foreach ($capture in @($evidence.captures | Sort-Object { $_['surface'] })) {
            $bindingLines += "$($capture.surface)|$($capture.bytes)|$($capture.sha256)"
        }
    }
    else {
        $bindingLines = @('bambu-hosted-gui-v2', $env:GITHUB_RUN_ID,
            $evidence.source_commit, $evidence.verification_commit, $Tag, $exeHash)
        foreach ($entry in @($evidence.manifest | Sort-Object { $_['path'] })) {
            $bindingLines += "$($entry.path)|$($entry.kind)|$($entry.tuple)|$($entry.bytes)|$($entry.sha256)"
        }
    }
    $aad = [System.Text.Encoding]::UTF8.GetBytes(($bindingLines -join "`n") + "`n")
    $publicPath = Join-Path $PSScriptRoot 'hosted-gui-public-v2.pem'
    if (-not (Test-Path -LiteralPath $publicPath -PathType Leaf)) {
        throw 'The selected versioned public key is unavailable in this verifier checkout.'
    }
    $publicPem = [System.IO.File]::ReadAllText($publicPath)
    $rsa = [System.Security.Cryptography.RSA]::Create()
    $key = $null
    $plaintext = $null
    $ciphertext = $null
    try {
        $key = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
        $nonce = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $tagBytes = [byte[]]::new(16)
        $plaintext = [System.IO.File]::ReadAllBytes($zipPath)
        $ciphertext = [byte[]]::new($plaintext.Length)
        $rsa.ImportFromPem($publicPem)
        $publicKeyHash = ([Convert]::ToHexString(
            [System.Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo()))).ToLowerInvariant()
        $wrappedKey = $rsa.Encrypt($key, [System.Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [System.Security.Cryptography.AesGcm]::new($key, 16)
        try { $aes.Encrypt($nonce, $plaintext, $ciphertext, $tagBytes, $aad) }
        finally { $aes.Dispose() }
        $cipherPath = Join-Path $OutputDirectory 'images.zip.aesgcm'
        [System.IO.File]::WriteAllBytes($cipherPath, $ciphertext)
        $cipherHash = (Get-FileHash -LiteralPath $cipherPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $envelope = [ordered]@{
            schema = if ($legacyMode) { 1 } else { 2 }
            run_id = $env:GITHUB_RUN_ID
            source_commit = $evidence.source_commit
            verification_commit = $evidence.verification_commit
            release_tag = $Tag
            installed_exe_sha256 = $exeHash
            public_key_sha256 = $publicKeyHash
            ciphertext_sha256 = $cipherHash
            aad_sha256 = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($aad))).ToLowerInvariant()
            wrapped_key = [Convert]::ToBase64String($wrappedKey)
            nonce = [Convert]::ToBase64String($nonce)
            tag = [Convert]::ToBase64String($tagBytes)
        }
        if (-not $legacyMode) { $envelope.key_id = $publicKeyHash }
        $envelope | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'envelope.json') -Encoding utf8
        $evidence.encrypted_bundle_sha256 = $cipherHash
        $evidence.image_availability = 'encrypted_bundle_only'
        if ($legacyMode) { $evidence.status = 'encrypted_capture_pending_restricted_review' }
        elseif ($evidence.behavior_failure) { $evidence.status = 'encrypted_partial_behavior_pending_restricted_review' }
        elseif ($CaptureScope -eq 'diagnostic') { $evidence.status = 'encrypted_diagnostic_pending_restricted_review' }
        else { $evidence.status = 'encrypted_behavior_pending_restricted_review' }
    }
    finally {
        $rsa.Dispose()
        foreach ($bytes in @($key, $plaintext, $ciphertext)) {
            if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
        }
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    }
}
catch {
    $evidence.failure = if ($legacyMode) { $_.Exception.Message } else {
        'Evidence validation or encryption failed before a complete encrypted bundle was created.'
    }
    throw
}
finally {
    if ($zipPath -and (Test-Path -LiteralPath $zipPath)) { Remove-Item -LiteralPath $zipPath -Force }
    if ($stageRoot -and (Test-Path -LiteralPath $stageRoot -PathType Container)) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
}
