<#
Diagnoses whether an installed copy of the application updates itself, on a disposable GitHub-hosted
Windows runner. Dispatched only by .github/workflows/diagnose-self-update.yml.

The run first reads the release to install and the latest release with its RELEASES file, while the
run's read-only token is still in this process. It then installs -FromTag silently through
Verify-HostedSquirrelInstall.ps1, which verifies the release, the installed executable and the
shortcuts, and clears the token before anything downloaded runs, and checks that the install root
holds exactly the app-<version> folder of that release.

No preference is changed. Update automatically (auto_update) is on by default, and a fresh runner has
no configuration file, so the application's own default applies; the receipt records the value the
application saved. The application is started through the install root's bambu-studio.exe, with no
argument, the way both shortcuts start it. On a fresh profile the first-run Setup Wizard opens as a
modal dialog, and the startup update check runs only after it closes (config_wizard_startup and then
check_new_version in GUI_App). A person closes it by finishing it; this diagnostic closes it with
WM_CLOSE whenever it appears and records every close.

For -ObserveSeconds, or until a newer version has been staged and Update.exe has finished, every
bambu-studio.exe, Update.exe and Setup.exe process and the application's windows are polled every
five seconds with the installer first-run diagnostic's own helpers, loaded from its source, and the
install root's app-<version> folders and the notification history are read. The application is then
asked to close through its windows, so it writes out its log, and whatever still runs is stopped.
When a newer folder holds its bambu-studio.exe, its files are compared with the full package
Update.exe downloaded, and the install root's bambu-studio.exe is started once more, as a shortcut
would, to record which version runs.

The evidence is text only: the application's update log lines (a release build encrypts its log with
the key it compiles in for logs written before a region is chosen; the key is read from this
checkout's src/libslic3r/LogSink.cpp, never copied here), every Update.exe run with its command line
and exit code, Squirrel's logs and its local RELEASES file, the app-<version> folders, the ready
banner and the failure notice as the notification history records them (the banner is drawn inside
the 3D view, where no window enumeration sees it), and the Application Error, Windows Error
Reporting and .NET Runtime entries. receipt.json holds one of:

  app_crashed                an application process crashed during the observation or at the next
                             start, by the installer first-run classifier's rules; this wins over
                             every other value
  updated_staged             a newer app-<version> folder with its bambu-studio.exe was staged and
                             Update.exe is shown to have finished it: an Update.exe --update run
                             exited with 0 (or the application logged exit code 0 and not that it
                             found nothing newer), nothing failed, Squirrel's local RELEASES lists
                             the version, every file of the full package is in the folder at its
                             size with bambu-studio.exe matching by SHA-256, and the next start ran
                             that folder
  update_failed              Update.exe exited with an error or crashed, or exited with 0 although
                             the feed holds a newer version and nothing was staged, or the
                             application logged a failed update or release check, or recorded the
                             failure notice; or a newer folder holds its bambu-studio.exe but is not
                             shown to be finished (partial staging: Update.exe extracts the package
                             straight into app-<version> and leaves what it wrote when it fails)
  update_offered_not_staged  an update was offered (Update.exe --update ran, or the application
                             logged a newer release) but nothing was staged by the end, and nothing
                             failed
  no_update_seen             none of the above

The classification is the result. The script fails only when the releases cannot be read, the
release cannot be verified or installed, or the install root's bambu-studio.exe cannot be started;
never because of a classification. No screenshots are taken.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $FromTag,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [Parameter(Mandatory)][ValidateRange(120, 1500)][int] $ObserveSeconds,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $CiExecutionApproved -or -not $IsWindows -or $env:GITHUB_ACTIONS -ne 'true' -or
    $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'The self-update diagnostic installs, starts, updates and stops the application; it runs only on an explicitly approved disposable GitHub-hosted Windows runner.'
}

$PollSeconds = 5
$SettleSeconds = 60
$CloseSeconds = 60
$IdleStopSeconds = 60
$NextStartSeconds = 90
$MaxCollectedBytes = [int64] 1MB
$MaxDecodedBytes = [int64] 2MB
$MaxReadLogBytes = [int64] 64MB
$MaxAppLogs = 8
$MaxListedLines = 200
$installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
$stubPath = Join-Path $installRoot 'bambu-studio.exe'
$squirrelTemp = Join-Path $env:LOCALAPPDATA 'SquirrelTemp'
$launcherTrace = Join-Path $env:TEMP 'bbs-launcher-trace.log'
$dataDirectory = Join-Path $env:APPDATA 'BambuStudio'
$appLogDirectory = Join-Path $dataDirectory 'log'
$appConfigPath = Join-Path $dataDirectory 'BambuStudio.conf'
$notificationHistoryPath = Join-Path $dataDirectory 'notification_history.json'
$watchedNames = @('bambu-studio.exe', 'Update.exe', 'Setup.exe')
# The title of the first-run guide in English and in Hong Kong Cantonese.
$SetupWizardTitle = 'Setup Wizard|\u8A2D\u5B9A\u56AE\u5C0E'
$UpdateLogPattern = 'check new version|auto update:|run wizard|GuideFrame'
$SquirrelLinePattern = '(?i)releases|download|apply|updat|newer|version|error|exception|fail|checksum|hash'

# The observation, crash and evidence helpers are the installer first-run diagnostic's, loaded from its
# source rather than copied, so both diagnostics poll processes, read crashes and collect logs the same
# way. They read the variables above ($watchedNames, $MaxCollectedBytes, $installRoot, $stubPath,
# $squirrelTemp, $launcherTrace, $appLogDirectory, $MaxAppLogs) by these names.
$ReusedFunctions = @(
    'Format-Utc', 'Format-ExitCode', 'Test-CrashExitCode', 'ConvertTo-UtcTime', 'Get-CimValue',
    'Register-ProcessTraces', 'Read-ProcessTraces', 'Get-FirstRunSample', 'New-ProcessRecord', 'Merge-FirstRunProcesses',
    'ConvertFrom-HexText', 'ConvertFrom-ApplicationEventText', 'Test-MainFrameShown',
    'Get-FirstRunClassification', 'Save-TextFile', 'Save-PhaseEvidence', 'Stop-InstalledProcesses',
    'Start-InstalledStub')
$firstRunPath = Join-Path $PSScriptRoot 'Diagnose-InstallerFirstRun.ps1'
$firstRunTokens = $null
$firstRunErrors = $null
$firstRunAst = [System.Management.Automation.Language.Parser]::ParseFile($firstRunPath, [ref] $firstRunTokens, [ref] $firstRunErrors)
if ($firstRunErrors.Count -ne 0) { throw "The installer first-run diagnostic does not parse: $($firstRunErrors[0].Message)" }
$firstRunFunctions = @{}
$firstRunTypes = [System.Collections.Generic.List[object]]::new()
foreach ($statement in $firstRunAst.EndBlock.Statements) {
    if ($statement -is [System.Management.Automation.Language.FunctionDefinitionAst]) {
        $firstRunFunctions[$statement.Name] = $statement
    }
    elseif ($statement -is [System.Management.Automation.Language.PipelineAst] -and
            $statement.PipelineElements[0] -is [System.Management.Automation.Language.CommandAst] -and
            $statement.PipelineElements[0].GetCommandName() -eq 'Add-Type') {
        $firstRunTypes.Add($statement)
    }
}
foreach ($name in $ReusedFunctions) {
    if (-not $firstRunFunctions.ContainsKey($name)) { throw "The installer first-run diagnostic no longer defines $name." }
    . ([scriptblock]::Create($firstRunFunctions[$name].Extent.Text))
}
# Its window enumeration (FirstRunDesktop), which Get-FirstRunSample calls.
if ($firstRunTypes.Count -ne 1) { throw 'The installer first-run diagnostic no longer defines exactly one window type.' }
& ([scriptblock]::Create($firstRunTypes[0].Extent.Text))

# The package entry hash is the hosted install check's, which compares the installed executable with
# the full package in the same way; it is loaded from that script's source too.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$installCheckPath = Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1'
$installCheckTokens = $null
$installCheckErrors = $null
$installCheckAst = [System.Management.Automation.Language.Parser]::ParseFile($installCheckPath, [ref] $installCheckTokens, [ref] $installCheckErrors)
if ($installCheckErrors.Count -ne 0) { throw "The hosted install check does not parse: $($installCheckErrors[0].Message)" }
$entryHashFunction = @($installCheckAst.EndBlock.Statements | Where-Object {
    $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -eq 'Get-EntrySha256' })
if ($entryHashFunction.Count -ne 1) { throw 'The hosted install check no longer defines Get-EntrySha256.' }
. ([scriptblock]::Create($entryHashFunction[0].Extent.Text))

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class SelfUpdateWindow
{
    public const uint WM_CLOSE = 0x0010;

    // Asks a window to close, as its close button or Alt+F4 does; it does not wait for the answer.
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@

# A property of an object read from JSON, or $null when it is missing.
function Get-JsonValue {
    param([AllowNull()][object] $Object, [Parameter(Mandatory)][string] $Name)
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

# The dotted numbers at the start of a package version, or after "app-" in a folder name, as the
# application reads an app-<version> folder ("app-2.8.4848" is 2, 8, 4848); a suffix is ignored.
function ConvertTo-VersionParts {
    param([AllowNull()][AllowEmptyString()][string] $Text)
    $match = [regex]::Match([string] $Text, '^(?:app-)?(?<version>\d+(?:\.\d+)*)')
    if (-not $match.Success) { return , [long[]] @() }
    return , [long[]] @($match.Groups['version'].Value.Split('.') | ForEach-Object {
        if ($_.Length -gt 12) { [long] 1000000000000 } else { [long] $_ } })
}

# -1, 0 or 1, number by number; a version that is the start of the other is the older one.
function Compare-VersionParts {
    param([AllowEmptyCollection()][long[]] $Left, [AllowEmptyCollection()][long[]] $Right)
    for ($i = 0; $i -lt [math]::Min($Left.Count, $Right.Count); ++$i) {
        if ($Left[$i] -lt $Right[$i]) { return -1 }
        if ($Left[$i] -gt $Right[$i]) { return 1 }
    }
    return [math]::Sign($Left.Count - $Right.Count)
}

# One row of a Squirrel RELEASES file ("<SHA-1> <package file> <bytes>"), or $null when the row is
# not of that form or names a path.
function ConvertFrom-ReleasesRow {
    param([AllowNull()][AllowEmptyString()][string] $Row)
    $match = [regex]::Match([string] $Row,
        '^(?<sha1>[0-9a-fA-F]{40})\s+(?<file>[^\\/\s]+-(?<version>\d+(?:\.\d+)+)-(?<kind>full|delta)\.nupkg)\s+(?<bytes>\d+)\s*$')
    if (-not $match.Success) { return $null }
    return [pscustomobject][ordered]@{
        sha1 = $match.Groups['sha1'].Value.ToLowerInvariant()
        file = $match.Groups['file'].Value
        version = $match.Groups['version'].Value
        full = $match.Groups['kind'].Value -eq 'full'
        bytes = [int64] $match.Groups['bytes'].Value
    }
}

# Every app-<version> folder of the install root, and whether it holds the application yet.
function Get-AppFolders {
    if (-not (Test-Path -LiteralPath $installRoot -PathType Container)) { return , @() }
    return , @(Get-ChildItem -LiteralPath $installRoot -Directory -Filter 'app-*' -ErrorAction SilentlyContinue | ForEach-Object {
        $executable = Join-Path $_.FullName 'bambu-studio.exe'
        $present = Test-Path -LiteralPath $executable -PathType Leaf
        $product = $null
        if ($present) {
            try { $product = (Get-Item -LiteralPath $executable).VersionInfo.ProductVersion } catch { $product = $null }
        }
        [pscustomobject][ordered]@{
            name = $_.Name
            version = (ConvertTo-VersionParts $_.Name) -join '.'
            has_executable = $present
            product_version = $product
            created = Format-Utc $_.CreationTimeUtc
        }
    })
}

# The newest app-<version> folder that is newer than the installed version, or $null.
function Get-NewerFolder {
    param([AllowEmptyCollection()][object[]] $Folders, [Parameter(Mandatory)][string] $InstalledVersion)
    $installed = ConvertTo-VersionParts $InstalledVersion
    $newest = $null
    foreach ($folder in $Folders) {
        $version = ConvertTo-VersionParts $folder.name
        if ((Compare-VersionParts $version $installed) -le 0) { continue }
        if ($null -eq $newest -or (Compare-VersionParts $version (ConvertTo-VersionParts $newest.name)) -gt 0) { $newest = $folder }
    }
    return $newest
}

# Every Update.exe process still running, with its command line.
function Get-RunningUpdateExe {
    try {
        return , @(Get-CimInstance -ClassName Win32_Process -Filter "Name = 'Update.exe'" | ForEach-Object {
            [pscustomobject][ordered]@{ pid = [int] $_.ProcessId; ppid = [int] $_.ParentProcessId; command_line = [string] $_.CommandLine }
        })
    }
    catch {
        return , @([pscustomobject][ordered]@{ pid = $null; ppid = $null; command_line = $null; error = $_.Exception.Message })
    }
}

# The notification centre's history, which NotificationManager rewrites on every change. The ready
# banner is drawn inside the 3D view, so this record of its being pushed is how it is seen here.
function Read-NotificationHistory {
    $result = [ordered]@{ found = $false; error = $null; ready = @(); failure = @() }
    if (-not (Test-Path -LiteralPath $notificationHistoryPath -PathType Leaf)) { return [pscustomobject] $result }
    $result.found = $true
    try {
        $stream = [System.IO.File]::Open($notificationHistoryPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]'ReadWrite, Delete')
        try { $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8); $text = $reader.ReadToEnd() }
        finally { $stream.Dispose() }
        $entries = @(Get-JsonValue ($text | ConvertFrom-Json) 'entries' | Where-Object { $null -ne $_ })
        $describe = {
            param([object] $Entry)
            [pscustomobject][ordered]@{
                id = Get-JsonValue $Entry 'id'; timestamp_ms = Get-JsonValue $Entry 'timestamp_ms'
                type = Get-JsonValue $Entry 'type'; level = Get-JsonValue $Entry 'level'; level_name = Get-JsonValue $Entry 'level_name'
                text = Get-JsonValue $Entry 'text'; dismissed = Get-JsonValue $Entry 'dismissed'
            }
        }
        $result.ready = @($entries | Where-Object { (Get-JsonValue $_ 'type') -eq 'AppUpdateReady' } | ForEach-Object { & $describe $_ })
        # The failure notice: a warning-level custom notification that names a release tag.
        $result.failure = @($entries | Where-Object {
            (Get-JsonValue $_ 'type') -eq 'CustomNotification' -and [string] (Get-JsonValue $_ 'level') -eq '7' -and
            ([string] (Get-JsonValue $_ 'text')) -match 'md3-v\d+' } | ForEach-Object { & $describe $_ })
    }
    catch {
        $result.error = $_.Exception.Message
    }
    return [pscustomobject] $result
}

# The auto_update value the application saved in BambuStudio.conf (JSON, then an MD5 checksum line).
function Read-AutoUpdatePreference {
    if (-not (Test-Path -LiteralPath $appConfigPath -PathType Leaf)) {
        return [pscustomobject][ordered]@{ file = 'absent'; value = $null; error = $null }
    }
    try {
        $text = Get-Content -LiteralPath $appConfigPath -Raw -Encoding utf8
        $json = ($text -split '(?m)^# MD5 checksum')[0]
        $app = Get-JsonValue ($json | ConvertFrom-Json) 'app'
        return [pscustomobject][ordered]@{ file = 'present'; value = Get-JsonValue $app 'auto_update'; error = $null }
    }
    catch {
        return [pscustomobject][ordered]@{ file = 'present'; value = $null; error = $_.Exception.Message }
    }
}

# BBL_Encrypt fills the AES key and IV by repeating the characters of the text to the length needed.
function ConvertTo-RepeatedBytes {
    param([Parameter(Mandatory)][string] $Text, [Parameter(Mandatory)][int] $Length)
    $source = [System.Text.Encoding]::UTF8.GetBytes($Text)
    if ($source.Length -eq 0) { throw 'An empty key cannot be repeated.' }
    $result = [byte[]]::new($Length)
    for ($i = 0; $i -lt $Length; ++$i) { $result[$i] = $source[$i % $source.Length] }
    return , $result
}

# The keys a release build compiles in for logs written before a region is chosen (the "local" keys),
# read from this checkout's LogSink.cpp and indexed by the key tag that each log header names.
function Get-LocalLogKeys {
    $source = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) (Join-Path 'src' (Join-Path 'libslic3r' 'LogSink.cpp'))
    $values = @{}
    $pattern = '(?m)^#define DEFAULT_KEY_(?<part>TAG|STR|IV)_(?<set>[A-Z]+_\d+)\s+"(?<value>[^"]*)"'
    foreach ($match in [regex]::Matches((Get-Content -LiteralPath $source -Raw), $pattern)) {
        $values["$($match.Groups['part'].Value)/$($match.Groups['set'].Value)"] = $match.Groups['value'].Value
    }
    $keys = @{}
    foreach ($entry in @($values.Keys | Where-Object { $_ -like 'TAG/*' })) {
        $set = $entry.Substring(4)
        if (-not $values.ContainsKey("STR/$set") -or -not $values.ContainsKey("IV/$set")) { continue }
        $keys[$values[$entry]] = [pscustomobject][ordered]@{
            set = $set
            key = ConvertTo-RepeatedBytes -Text $values["STR/$set"] -Length 32
            iv = ConvertTo-RepeatedBytes -Text $values["IV/$set"] -Length 16
        }
    }
    return $keys
}

# Up to Limit bytes from the start of a file another process may still be writing.
function Read-SharedBytes {
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][int64] $Limit)
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]'ReadWrite, Delete')
    try {
        $length = [int64] $stream.Length
        $take = [int] [math]::Min($length, $Limit)
        $buffer = [byte[]]::new($take)
        $read = 0
        while ($read -lt $take) {
            $count = $stream.Read($buffer, $read, $take - $read)
            if ($count -le 0) { break }
            $read += $count
        }
        if ($read -lt $take) { [Array]::Resize([ref] $buffer, $read) }
        return [pscustomobject][ordered]@{ bytes = $buffer; length = $length }
    }
    finally {
        $stream.Dispose()
    }
}

# One application log as text. A release build writes "BEGIN_HEADER", a JSON header that names the
# key tag, and "END_HEADER", then every line padded with spaces to whole 16-byte blocks and encrypted
# with AES-256-CBC as one chain from the key's IV (LogSinkBackend::consume). A build without log
# encryption writes plain text.
function ConvertFrom-ApplicationLog {
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][hashtable] $Keys)
    $result = [ordered]@{
        bytes = $null; read_bytes = $null; encrypted = $null; key_tag = $null; key_type = $null
        app_version = $null; app_build_time = $null; decoded = $false; text = $null; error = $null
    }
    try {
        $file = Read-SharedBytes -Path $Path -Limit $MaxReadLogBytes
        $bytes = [byte[]] $file.bytes
        $result.bytes = $file.length
        $result.read_bytes = $bytes.Length
        $latin = [System.Text.Encoding]::Latin1.GetString($bytes)
        $beginMarker = "BEGIN_HEADER`n"
        $endMarker = "`nEND_HEADER`n"
        if (-not $latin.StartsWith($beginMarker, [StringComparison]::Ordinal)) {
            $result.encrypted = $false
            $result.text = [System.Text.Encoding]::UTF8.GetString($bytes)
            $result.decoded = $true
            return [pscustomobject] $result
        }
        $end = $latin.IndexOf($endMarker, [StringComparison]::Ordinal)
        if ($end -lt 0) { throw 'The log header has no end marker.' }
        $header = $latin.Substring($beginMarker.Length, $end - $beginMarker.Length) | ConvertFrom-Json
        $result.app_version = Get-JsonValue $header 'app_version'
        $result.app_build_time = Get-JsonValue $header 'app_build_time'
        $result.key_tag = [string] (Get-JsonValue $header 'enc_key_tag')
        $result.key_type = Get-JsonValue $header 'enc_key_type'
        $result.encrypted = (Get-JsonValue $header 'enc_type') -eq 'AES_256'
        $offset = $end + $endMarker.Length
        $length = $bytes.Length - $offset
        if (-not $result.encrypted) {
            $result.text = [System.Text.Encoding]::UTF8.GetString($bytes, $offset, $length)
            $result.decoded = $true
            return [pscustomobject] $result
        }
        if (-not $result.key_tag -or -not $Keys.ContainsKey($result.key_tag)) {
            throw "The log is encrypted with key tag '$($result.key_tag)', which is not a local key in src/libslic3r/LogSink.cpp."
        }
        $key = $Keys[$result.key_tag]
        $length -= $length % 16
        $plain = [byte[]]::new(0)
        if ($length -gt 0) {
            $cipher = [byte[]]::new($length)
            [Array]::Copy($bytes, $offset, $cipher, 0, $length)
            $aes = [System.Security.Cryptography.Aes]::Create()
            try {
                $aes.Key = $key.key
                $plain = $aes.DecryptCbc($cipher, $key.iv, [System.Security.Cryptography.PaddingMode]::None)
            }
            finally {
                $aes.Dispose()
            }
        }
        $result.text = [System.Text.Encoding]::UTF8.GetString($plain).Replace([string] [char] 0, '')
        $result.decoded = $true
    }
    catch {
        $result.error = $_.Exception.Message
    }
    return [pscustomobject] $result
}

# Decodes the application logs written since the phase began, saves each as text (its last
# MaxDecodedBytes) and collects the update-check lines in update-log-lines.txt.
function Save-ApplicationLogs {
    param([Parameter(Mandatory)][string] $Directory, [Parameter(Mandatory)][datetime] $Since, [Parameter(Mandatory)][hashtable] $Keys)
    $files = [System.Collections.Generic.List[object]]::new()
    $lines = [System.Collections.Generic.List[string]]::new()
    $decodedDirectory = Join-Path (Join-Path $Directory 'logs') 'app-decoded'
    if (Test-Path -LiteralPath $appLogDirectory -PathType Container) {
        $recent = @(Get-ChildItem -LiteralPath $appLogDirectory -File |
            Where-Object { $_.Name -like 'studio_*' -and $_.LastWriteTimeUtc -ge $Since.AddSeconds(-1) } |
            Sort-Object -Property LastWriteTimeUtc | Select-Object -Last $MaxAppLogs)
        foreach ($file in $recent) {
            $log = ConvertFrom-ApplicationLog -Path $file.FullName -Keys $Keys
            $saved = $null
            $savedBytes = $null
            $truncated = $null
            if ($log.decoded) {
                foreach ($line in ([string] $log.text -split "`n")) {
                    $trimmed = $line.TrimEnd()
                    if ($trimmed -match $UpdateLogPattern) { $lines.Add("$($file.Name): $trimmed") }
                }
                $text = [System.Text.Encoding]::UTF8.GetBytes([string] $log.text)
                $keep = [int] [math]::Min([int64] $text.Length, $MaxDecodedBytes)
                $tail = [byte[]]::new($keep)
                [Array]::Copy($text, $text.Length - $keep, $tail, 0, $keep)
                New-Item -ItemType Directory -Force -Path $decodedDirectory | Out-Null
                $saved = Join-Path $decodedDirectory ($file.Name + '.txt')
                [System.IO.File]::WriteAllBytes($saved, $tail)
                $savedBytes = $keep
                $truncated = $keep -lt $text.Length
            }
            $files.Add([pscustomobject][ordered]@{
                source = $file.FullName; bytes = $log.bytes; encrypted = $log.encrypted; key_tag = $log.key_tag
                key_type = $log.key_type; app_version = $log.app_version; app_build_time = $log.app_build_time
                decoded = $log.decoded; saved = $saved; saved_bytes = $savedBytes; truncated = $truncated; error = $log.error
            })
        }
    }
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $linesText = if ($lines.Count -gt 0) { $lines.ToArray() } else { @('No update line was found in a decoded application log.') }
    Set-Content -LiteralPath (Join-Path $Directory 'update-log-lines.txt') -Value $linesText -Encoding utf8
    return [pscustomobject][ordered]@{ files = $files.ToArray(); lines = $lines.ToArray() }
}

# What the application's update-check lines say (GUI_App::check_new_version, start_auto_update and
# run_squirrel_update). A line of the "auto update:" family that none of the known forms matches,
# such as the text of an exception, is unexpected and counts as a failure.
function Get-UpdateLogFacts {
    param([AllowEmptyCollection()][string[]] $Lines)
    $facts = [ordered]@{
        release_check = $null; checked_tag = $null; check_error = $null; nothing_to_do = $null
        update_tag = $null; update_started = $false; update_exit_code = $null; nothing_newer = $false
        start_failed = $null; gave_up = $null; left_running_on_close = $false; not_installed = $false
        unexpected = [System.Collections.Generic.List[string]]::new()
    }
    foreach ($line in $Lines) {
        if ($line -match 'check new version: (?<tag>\S+) published \S+, built \S+, (?<verdict>newer|not newer) than this build') {
            $facts.checked_tag = $Matches['tag']
            $facts.release_check = if ($Matches['verdict'] -eq 'newer') { 'newer' } else { 'not_newer' }
        }
        elseif ($line -match 'check new version error (?<error>.*)$') { $facts.check_error = $Matches['error'].Trim() }
        elseif ($line -match 'check new version: nothing to do for (?<tag>.*)$') { $facts.nothing_to_do = $Matches['tag'].Trim() }
        elseif ($line -match 'auto update: updating to (?<tag>\S+)') { $facts.update_tag = $Matches['tag'] }
        elseif ($line -match 'auto update: Update\.exe started') { $facts.update_started = $true }
        elseif ($line -match 'auto update: Update\.exe exited with code (?<code>\d+)') { $facts.update_exit_code = [int64] $Matches['code'] }
        elseif ($line -match 'auto update: Update\.exe found nothing newer to install') { $facts.nothing_newer = $true }
        elseif ($line -match 'auto update: could not start Update\.exe to restart') { continue }
        elseif ($line -match 'auto update: could not start Update\.exe(?<error>.*)$') { $facts.start_failed = $Matches['error'].Trim(' ', ',') }
        elseif ($line -match 'auto update: gave up waiting for Update\.exe(?<detail>.*)$') { $facts.gave_up = $Matches['detail'].Trim() }
        elseif ($line -match 'auto update: the application is closing') { $facts.left_running_on_close = $true }
        elseif ($line -match 'auto update: no Update\.exe next to this copy') { $facts.not_installed = $true }
        elseif ($line -match 'auto update: (?:an update is already running|could not read the exit code|\S+ is not newer than this build|restart requested|Update\.exe will start|restart requested but)') { continue }
        elseif ($line -match 'auto update: (?<message>.+)$') { $facts.unexpected.Add($Matches['message'].Trim()) }
    }
    $facts.unexpected = $facts.unexpected.ToArray()
    return [pscustomobject] $facts
}

# Every Update.exe process the polls or the process events saw, and what it was asked to do.
function Get-UpdateExeRuns {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Samples,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Events,
        [AllowEmptyCollection()][object[]] $ApplicationIds = @()
    )
    $records = [ordered]@{}
    $record = {
        param([int] $ProcessId)
        $key = [string] $ProcessId
        if (-not $records.Contains($key)) {
            $records[$key] = [ordered]@{ pid = $ProcessId; ppid = $null; command_line = $null; started = $null; exited = $null; exit_code = $null; polls = 0 }
        }
        $records[$key]
    }
    foreach ($sample in $Samples) {
        foreach ($row in @($sample.processes | Where-Object { $_.name -eq 'Update.exe' })) {
            $entry = & $record $row.pid
            $entry.polls++
            if ($row.ppid) { $entry.ppid = [int] $row.ppid }
            if ($row.command_line) { $entry.command_line = [string] $row.command_line }
            if ($row.created -and -not $entry.started) { $entry.started = ConvertTo-UtcTime $row.created }
        }
        foreach ($exit in @($sample.exits | Where-Object { $_.name -eq 'Update.exe' })) {
            $entry = & $record $exit.pid
            if ($exit.exited) { $entry.exited = ConvertTo-UtcTime $exit.exited }
            if ($null -ne $exit.exit_code) { $entry.exit_code = [int64] $exit.exit_code }
        }
    }
    foreach ($traced in $Events) {
        if ([string] $traced.name -ne 'Update.exe' -or $null -eq $traced.pid) { continue }
        $entry = & $record ([int] $traced.pid)
        if ($null -ne $traced.ppid -and -not $entry.ppid) { $entry.ppid = [int] $traced.ppid }
        switch ($traced.kind) {
            'start' { if (-not $entry.started) { $entry.started = $traced.time } }
            'stop' {
                $entry.exited = $traced.time
                if ($null -ne $traced.exit_code) { $entry.exit_code = [int64] $traced.exit_code }
            }
            'create' {
                if ($traced.command_line -and -not $entry.command_line) { $entry.command_line = [string] $traced.command_line }
                if (-not $entry.started) { $entry.started = $traced.time }
            }
        }
    }
    $alive = @()
    if ($Samples.Count -gt 0) {
        $alive = @($Samples[$Samples.Count - 1].processes | Where-Object { $_.name -eq 'Update.exe' } | ForEach-Object { [string] $_.pid })
    }
    $applications = @($ApplicationIds | ForEach-Object { [string] $_ })
    return , @($records.Values | ForEach-Object {
        $entry = $_
        $command = [string] $entry.command_line
        $role = if (-not $command) { 'unknown' }
                elseif ($command -match '--update(?:=|\s)') { 'update' }
                elseif ($command -match '--processStart') { 'process-start' }
                elseif ($command -match '--(?:create|remove)Shortcut') { 'shortcut' }
                else { 'other' }
        $isAlive = $alive -contains [string] $entry.pid
        [pscustomobject][ordered]@{
            pid = $entry.pid; ppid = $entry.ppid; role = $role; command_line = $entry.command_line
            started_by_application = $null -ne $entry.ppid -and $applications -contains [string] $entry.ppid
            started = Format-Utc $entry.started
            exited = if ($isAlive) { $null } else { Format-Utc $entry.exited }
            exit_code = if ($isAlive) { $null } else { $entry.exit_code }
            exit_code_hex = if ($isAlive) { $null } else { Format-ExitCode $entry.exit_code }
            polls = $entry.polls; alive_at_end = $isAlive
        }
    })
}

# Application log entries about Update.exe: Application Error and Windows Error Reporting crash
# entries, and the .NET Runtime entry of an unhandled exception (Update.exe is a .NET program). The
# first-run evidence keeps only entries that name the application.
function Save-UpdateExeEvents {
    param([Parameter(Mandatory)][string] $Directory, [Parameter(Mandatory)][datetime] $Since)
    $text = [System.Collections.Generic.List[string]]::new()
    try {
        $entries = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $Since.ToLocalTime() } -ErrorAction Stop |
            Where-Object { $_.ProviderName -in @('Application Error', 'Windows Error Reporting', '.NET Runtime', 'Application Hang') } |
            Where-Object { [string] $_.Message -match 'Update\.exe' })
        foreach ($entry in $entries) {
            $text.Add(('{0} id={1} provider={2} level={3}' -f (Format-Utc $entry.TimeCreated), $entry.Id, $entry.ProviderName, $entry.LevelDisplayName))
            $text.Add([string] $entry.Message)
            $text.Add('')
        }
        if ($entries.Count -eq 0) { $text.Add('No Application event-log entry names Update.exe.') }
    }
    catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { $text.Add('No Application event-log entry since the phase started.') }
        else { $text.Add("Reading the Application event log failed: $($_.Exception.Message)") }
    }
    $logs = Join-Path $Directory 'logs'
    New-Item -ItemType Directory -Force -Path $logs | Out-Null
    Set-Content -LiteralPath (Join-Path $logs 'update-exe-events.txt') -Value $text -Encoding utf8
    $parsed = ConvertFrom-ApplicationEventText -Lines $text.ToArray()
    return , @($parsed | Where-Object {
        ($_.kind -eq 'crash' -and ([string] $_.application) -ieq 'Update.exe') -or
        ($_.provider -eq '.NET Runtime' -and $_.id -eq 1026) -or
        ($_.provider -eq 'Windows Error Reporting' -and $_.event_name -eq 'CLR20r3') } | ForEach-Object {
        [pscustomobject][ordered]@{
            time = Format-Utc $_.time; provider = $_.provider; id = $_.id; event_name = $_.event_name
            exception_code = $_.exception_code; module = $_.module; report_id = $_.report_id
        }
    })
}

# Squirrel's side: the files in the install root's packages folder, its local RELEASES file, and the
# lines of Squirrel's logs about the feed, downloads and updates (the newest MaxListedLines).
function Save-SquirrelEvidence {
    param([Parameter(Mandatory)][string] $Directory)
    $packages = Join-Path $installRoot 'packages'
    $listing = @()
    $localReleases = @()
    if (Test-Path -LiteralPath $packages -PathType Container) {
        $listing = @(Get-ChildItem -LiteralPath $packages -File | ForEach-Object {
            [pscustomobject][ordered]@{ name = $_.Name; bytes = $_.Length; written = Format-Utc $_.LastWriteTimeUtc } })
        $releasesPath = Join-Path $packages 'RELEASES'
        if (Test-Path -LiteralPath $releasesPath -PathType Leaf) {
            $localReleases = @(Get-Content -LiteralPath $releasesPath | Where-Object { $_.Trim() })
            $releasesText = if ($localReleases.Count -gt 0) { $localReleases } else { @('The local RELEASES file is empty.') }
            Set-Content -LiteralPath (Join-Path $Directory 'local-RELEASES.txt') -Value $releasesText -Encoding utf8
        }
    }
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($folder in @($installRoot, $squirrelTemp)) {
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) { continue }
        foreach ($file in @(Get-ChildItem -LiteralPath $folder -File -Filter '*.log' | Sort-Object -Property Name)) {
            try {
                $text = [System.Text.Encoding]::UTF8.GetString((Read-SharedBytes -Path $file.FullName -Limit $MaxReadLogBytes).bytes)
                foreach ($line in ($text -split '\r?\n')) {
                    if ($line -match $SquirrelLinePattern) { $lines.Add("$($file.Name): $($line.TrimEnd())") }
                }
            }
            catch {
                $lines.Add("$($file.Name): could not be read: $($_.Exception.Message)")
            }
        }
    }
    $kept = @($lines | Select-Object -Last $MaxListedLines)
    $keptText = if ($kept.Count -gt 0) { $kept } else { @('No Squirrel log line about the feed, a download or an update.') }
    Set-Content -LiteralPath (Join-Path $Directory 'squirrel-update-lines.txt') -Value $keptText -Encoding utf8
    return [pscustomobject][ordered]@{
        packages = $listing
        local_releases = $localReleases
        releases_lines = @($kept | Where-Object { $_ -match 'RELEASES' })
        log_lines = $kept
        log_lines_total = $lines.Count
    }
}

# Whether a newer folder holds the whole package Update.exe was to stage. Squirrel's install step
# (ExtractZipForInstall) writes every file under lib/<framework>/ of the full package straight into
# app-<version>, except the *_ExecutionStub.exe stubs, which go to the install root, and a failure
# part of the way leaves what it wrote. The package must be the one the feed serves (its RELEASES row
# holds the SHA-1 that Squirrel checks after the download), every file of it must be in the folder
# at its size, and bambu-studio.exe must have its SHA-256. Returns what it found; problems lists
# every reason the folder is not that package, and is empty when it is.
function Test-StagedFiles {
    param(
        [Parameter(Mandatory)][object] $Folder,
        [AllowEmptyCollection()][string[]] $FeedReleases = @(),
        [AllowEmptyCollection()][string[]] $LocalReleases = @()
    )
    $problems = [System.Collections.Generic.List[string]]::new()
    $examples = [System.Collections.Generic.List[string]]::new()
    $result = [ordered]@{
        folder = $Folder.name; package = $null; package_sha1 = $null; feed_sha1 = $null; package_files = $null
        missing_files = 0; differing_files = 0; examples = @(); executable_sha256 = $null
        package_executable_sha256 = $null; problems = @(); error = $null
    }
    try {
        $version = ConvertTo-VersionParts $Folder.name
        $fromFeed = @(@($FeedReleases) | ForEach-Object { ConvertFrom-ReleasesRow $_ } | Where-Object {
            $null -ne $_ -and $_.full -and (Compare-VersionParts (ConvertTo-VersionParts $_.version) $version) -eq 0 })
        $fromLocal = @(@($LocalReleases) | ForEach-Object { ConvertFrom-ReleasesRow $_ } | Where-Object {
            $null -ne $_ -and $_.full -and (Compare-VersionParts (ConvertTo-VersionParts $_.version) $version) -eq 0 })
        $row = if ($fromFeed.Count -gt 0) { $fromFeed[0] } elseif ($fromLocal.Count -gt 0) { $fromLocal[0] } else { $null }
        if ($null -eq $row) { throw "neither the feed's RELEASES nor packages\RELEASES names a full package of $($Folder.version)" }
        $result.package = $row.file
        $package = Join-Path (Join-Path $installRoot 'packages') $row.file
        if (-not (Test-Path -LiteralPath $package -PathType Leaf)) { throw "packages\$($row.file) is not there" }
        $result.package_sha1 = (Get-FileHash -LiteralPath $package -Algorithm SHA1).Hash.ToLowerInvariant()
        if ($fromFeed.Count -gt 0) {
            $result.feed_sha1 = $fromFeed[0].sha1
            if ($result.package_sha1 -ne $result.feed_sha1) {
                $problems.Add("packages\$($row.file) is not the package the feed serves (SHA-1 $($result.package_sha1), the feed's $($result.feed_sha1))")
            }
        }

        # The folder's files by their path inside it; the table ignores case, as Windows paths do.
        $directory = Join-Path $installRoot $Folder.name
        $prefix = $directory.TrimEnd('\') + '\'
        $present = @{}
        foreach ($file in @(Get-ChildItem -LiteralPath $directory -Recurse -File -Force -ErrorAction Stop)) {
            $present[$file.FullName.Substring($prefix.Length).Replace('\', '/')] = $file
        }
        # Squirrel's own rule for the files it installs: the first lib/<framework>/ in the entry's path.
        $libFolder = [regex]::new('lib[\\/][^\\/]*[\\/]', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        $count = 0
        $archive = [System.IO.Compression.ZipFile]::OpenRead($package)
        try {
            foreach ($entry in $archive.Entries) {
                $name = $entry.FullName.Replace('\', '/')
                if ($name.EndsWith('/') -or -not $libFolder.IsMatch($name) -or $name.Contains('_ExecutionStub.exe')) { continue }
                $relative = $libFolder.Replace($name, '', 1)
                $unescaped = [Uri]::UnescapeDataString($relative)
                $count++
                $file = if ($present.ContainsKey($relative)) { $present[$relative] }
                        elseif ($present.ContainsKey($unescaped)) { $present[$unescaped] }
                        else { $null }
                if ($null -eq $file) {
                    $result.missing_files++
                    if ($examples.Count -lt 10) { $examples.Add("missing: $relative") }
                    continue
                }
                if ($file.Length -ne $entry.Length) {
                    $result.differing_files++
                    if ($examples.Count -lt 10) { $examples.Add("$relative is $($file.Length) bytes, the package's $($entry.Length)") }
                    continue
                }
                if ($relative -ieq 'bambu-studio.exe') {
                    $result.package_executable_sha256 = Get-EntrySha256 -Entry $entry
                    $result.executable_sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
        }
        finally {
            $archive.Dispose()
        }
        $result.package_files = $count
        if ($result.missing_files -gt 0) { $problems.Add("$($result.missing_files) of the package's $count files are missing from $($Folder.name)") }
        if ($result.differing_files -gt 0) { $problems.Add("$($result.differing_files) of the package's $count files have another size in $($Folder.name)") }
        if ($null -ne $result.package_executable_sha256) {
            if ($result.executable_sha256 -ne $result.package_executable_sha256) {
                $problems.Add("its bambu-studio.exe differs from the package's (SHA-256 $($result.executable_sha256), the package's $($result.package_executable_sha256))")
            }
        }
        elseif ($result.missing_files -eq 0 -and $result.differing_files -eq 0) {
            $problems.Add("$($row.file) has no lib/<framework>/bambu-studio.exe to compare with")
        }
    }
    catch {
        $result.error = $_.Exception.Message
        $problems.Add("its files could not be compared with the package: $($_.Exception.Message)")
    }
    $result.examples = $examples.ToArray()
    $result.problems = $problems.ToArray()
    return [pscustomobject] $result
}

# Asks the application to close through its windows, as a person closing it does, so it writes out
# its log and exits normally. Returns when no application process is left or after CloseSeconds.
function Close-Application {
    param(
        [Parameter(Mandatory)][hashtable] $Tracked,
        [Parameter(Mandatory)][datetime] $Started,
        [Parameter(Mandatory)][AllowEmptyCollection()][System.Collections.Generic.List[object]] $Samples,
        [Parameter(Mandatory)][string] $SamplePath
    )
    $requested = [datetime]::UtcNow
    $deadline = $requested.AddSeconds($CloseSeconds)
    $posted = [System.Collections.Generic.List[object]]::new()
    $remaining = @()
    while ($true) {
        $sample = Get-FirstRunSample -Tracked $Tracked -Started $Started
        $Samples.Add($sample)
        Add-Content -LiteralPath $SamplePath -Value ($sample | ConvertTo-Json -Depth 6 -Compress) -Encoding utf8
        $remaining = @($sample.processes | Where-Object { $_.name -eq 'bambu-studio.exe' -and -not (([string] $_.path) -ieq $stubPath) })
        $now = [datetime]::UtcNow
        if ($remaining.Count -eq 0 -or $now -ge $deadline) { break }
        foreach ($window in @($sample.windows | Where-Object { $_.Visible })) {
            $earlier = @($posted | Where-Object { $_.handle -eq $window.Handle })
            if ($earlier.Count -gt 0 -and ($now - (ConvertTo-UtcTime $earlier[$earlier.Count - 1].time)).TotalSeconds -lt 15) { continue }
            $sent = [SelfUpdateWindow]::PostMessageW([IntPtr] $window.Handle, [SelfUpdateWindow]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            $posted.Add([pscustomobject][ordered]@{ time = Format-Utc $now; pid = $window.ProcessId; handle = $window.Handle; title = $window.Title; posted = $sent })
        }
        Start-Sleep -Seconds 2
    }
    return [pscustomobject][ordered]@{
        requested = Format-Utc $requested
        finished = Format-Utc ([datetime]::UtcNow)
        closed = $remaining.Count -eq 0
        remaining_pids = @($remaining | ForEach-Object { $_.pid })
        windows_closed = $posted.ToArray()
    }
}

# One start through the install root's bambu-studio.exe: polls until Seconds have passed (or, with
# StopWhenStaged, until a newer version is staged and settled), optionally closing the first-run
# Setup Wizard and closing the application at the end, then collects the evidence.
function Invoke-ObservationPhase {
    param(
        [Parameter(Mandatory)][string] $Name,
        [Parameter(Mandatory)][int] $Seconds,
        [Parameter(Mandatory)][string] $InstalledVersion,
        [switch] $CloseSetupWizard,
        [switch] $StopWhenStaged,
        [switch] $CloseAtEnd
    )
    $directory = Join-Path $OutputDirectory $Name
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $samplePath = Join-Path $directory 'samples.jsonl'
    $samples = [System.Collections.Generic.List[object]]::new()
    $tracked = @{}
    $wizardCloses = [System.Collections.Generic.List[object]]::new()
    $sightings = [ordered]@{}
    $stopReason = 'window_elapsed'
    $readySeen = $null
    $settledSince = $null
    $wizardOpen = $false
    $idlePolls = 0
    $close = $null
    $events = @()
    $started = [datetime]::UtcNow
    $observed = $null
    $registration = Register-ProcessTraces -Prefix "self-update-$Name"
    try {
        $null = Start-InstalledStub
        $deadline = $started.AddSeconds($Seconds)
        while ($true) {
            $tick = [datetime]::UtcNow.AddSeconds($PollSeconds)
            $sample = Get-FirstRunSample -Tracked $tracked -Started $started
            $folders = Get-AppFolders
            $sample.PSObject.Properties.Add([psnoteproperty]::new('app_folders', $folders))
            $now = [datetime]::UtcNow
            foreach ($folder in $folders) {
                if (-not $sightings.Contains($folder.name)) {
                    $sightings[$folder.name] = [ordered]@{ folder = $folder.name; seen = Format-Utc $now; executable_seen = $null }
                }
                if ($folder.has_executable -and -not $sightings[$folder.name].executable_seen) { $sightings[$folder.name].executable_seen = Format-Utc $now }
            }
            if ($CloseSetupWizard) {
                foreach ($window in @($sample.windows | Where-Object { $_.Visible -and ([string] $_.Title) -match $SetupWizardTitle })) {
                    $earlier = @($wizardCloses | Where-Object { $_.handle -eq $window.Handle })
                    if ($earlier.Count -ge 5) { continue }
                    if ($earlier.Count -gt 0 -and ($now - (ConvertTo-UtcTime $earlier[$earlier.Count - 1].time)).TotalSeconds -lt 15) { continue }
                    $sent = [SelfUpdateWindow]::PostMessageW([IntPtr] $window.Handle, [SelfUpdateWindow]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
                    $wizardCloses.Add([pscustomobject][ordered]@{
                        time = Format-Utc $now; elapsed_s = [math]::Round(($now - $started).TotalSeconds, 1)
                        pid = $window.ProcessId; handle = $window.Handle; title = $window.Title; posted = $sent
                    })
                    Write-Host "${Name}: closed the Setup Wizard window $($window.Handle) of pid $($window.ProcessId) with WM_CLOSE"
                }
            }
            # Whether the guide is still open at the last poll of the observation, before any close.
            $wizardOpen = @($sample.windows | Where-Object { $_.Visible -and ([string] $_.Title) -match $SetupWizardTitle }).Count -gt 0
            $samples.Add($sample)
            Add-Content -LiteralPath $samplePath -Value ($sample | ConvertTo-Json -Depth 6 -Compress) -Encoding utf8
            $updating = @($sample.processes | Where-Object { $_.name -eq 'Update.exe' }).Count -gt 0
            $running = @($sample.processes | Where-Object { $_.name -eq 'bambu-studio.exe' -and -not (([string] $_.path) -ieq $stubPath) }).Count -gt 0
            if ($StopWhenStaged) {
                $newer = Get-NewerFolder -Folders $folders -InstalledVersion $InstalledVersion
                if ($null -ne $newer -and $newer.has_executable -and -not $updating) {
                    if ($null -eq $settledSince) { $settledSince = $now }
                    if ($null -eq $readySeen -and @((Read-NotificationHistory).ready).Count -gt 0) { $readySeen = $now }
                    if ($null -ne $readySeen -or ($now - $settledSince).TotalSeconds -ge $SettleSeconds) { $stopReason = 'staged_and_settled'; break }
                }
                else {
                    $settledSince = $null
                }
            }
            # Nothing left to watch: no application and no Update.exe for three polls in a row.
            if (-not $updating -and -not $running -and ($now - $started).TotalSeconds -ge $IdleStopSeconds) { $idlePolls++ } else { $idlePolls = 0 }
            if ($idlePolls -ge 3) { $stopReason = 'nothing_running'; break }
            if ($now -ge $deadline) { break }
            $wait = [int] ($tick - [datetime]::UtcNow).TotalMilliseconds
            if ($wait -gt 0) { Start-Sleep -Milliseconds $wait }
        }
        $observed = [datetime]::UtcNow
        if ($CloseAtEnd) { $close = Close-Application -Tracked $tracked -Started $started -Samples $samples -SamplePath $samplePath }
    }
    finally {
        $events = Read-ProcessTraces -Registration $registration
        ConvertTo-Json -InputObject @($events | ForEach-Object {
            [ordered]@{ kind = $_.kind; time = Format-Utc $_.time; pid = $_.pid; ppid = $_.ppid; name = $_.name
                        exit_code = $_.exit_code; exit_code_hex = Format-ExitCode $_.exit_code; path = $_.path; command_line = $_.command_line }
        }) -Depth 4 | Set-Content -LiteralPath (Join-Path $directory 'process-events.json') -Encoding utf8
    }
    # The state the run ends with, before anything is stopped.
    $final = [pscustomobject][ordered]@{ time = Format-Utc ([datetime]::UtcNow); app_folders = Get-AppFolders; update_exe = Get-RunningUpdateExe }
    $traceLines = @()
    if (Test-Path -LiteralPath $launcherTrace -PathType Leaf) { $traceLines = @(Get-Content -LiteralPath $launcherTrace -Encoding utf8) }
    $processes = Merge-FirstRunProcesses -Samples $samples.ToArray() -Events $events -TraceLines $traceLines -Since $started -StubPath $stubPath
    # The evidence first: the Application log entries it collects can make the phase a crash.
    $evidence = Save-PhaseEvidence -Directory $directory -Since $started
    $updateExeCrashes = Save-UpdateExeEvents -Directory $directory -Since $started
    $collected = [datetime]::UtcNow
    $applicationEvents = ConvertFrom-ApplicationEventText -Lines $evidence.application_events
    $verdict = Get-FirstRunClassification -Processes $processes -ApplicationEvents $applicationEvents -Since $started -Until $collected
    $applicationIds = @($processes | Where-Object { $_.role -in @('app', 'firstrun', 'unknown') } | ForEach-Object { $_.pid })
    $updateRuns = Get-UpdateExeRuns -Samples $samples.ToArray() -Events $events -ApplicationIds $applicationIds
    $appLogs = Save-ApplicationLogs -Directory $directory -Since $started -Keys $LogKeys
    $history = Read-NotificationHistory
    $files = [System.Collections.Generic.List[object]]::new()
    foreach ($file in @($evidence.files)) { $files.Add($file) }
    if ($history.found) { Save-TextFile -Source $notificationHistoryPath -Destination (Join-Path $directory 'notification_history.json') -Inventory $files }
    return [pscustomobject][ordered]@{
        name = $Name
        started = Format-Utc $started
        observed = Format-Utc $observed
        ended = Format-Utc ([datetime]::UtcNow)
        stop_reason = $stopReason
        polls = $samples.Count
        setup_wizard_closes = $wizardCloses.ToArray()
        setup_wizard_open_at_end = $wizardOpen
        folder_sightings = @($sightings.Values | ForEach-Object { [pscustomobject] $_ })
        ready_banner_seen = Format-Utc $readySeen
        close = $close
        final = $final
        process_events = [ordered]@{ registered = @($registration.identifiers.Keys); errors = $registration.errors.ToArray(); count = @($events).Count }
        processes = $processes
        verdict = $verdict
        update_runs = $updateRuns
        update_exe_crashes = $updateExeCrashes
        app_logs = $appLogs
        notifications = $history
        files = $files.ToArray()
    }
}

# The result of the whole run, from the facts the phases gathered. Decided in this order: a crash,
# a staged version, a failure, an update that was offered, and nothing. A newer app-<version> folder
# with its bambu-studio.exe (the candidate) counts as staged only when Update.exe is shown to have
# finished it, as the application itself requires (run_squirrel_update counts an update only when
# Update.exe exited with 0 and a newer folder is there); a candidate that is not is partial staging,
# a failure. Reads only the facts, so ui-md3/tests/self-update-diagnostic.test.mjs and
# Test-SelfUpdateClassification.ps1 can run it on synthetic ones.
function Get-SelfUpdateClassification {
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Facts)
    $runs = @($Facts.update_runs | Where-Object { $null -ne $_ -and $_.role -eq 'update' })
    $log = $Facts.log
    $installed = "app-$($Facts.installed_version)"
    $feed = $Facts.feed
    $candidate = $Facts.candidate
    $describe = {
        param([object] $Run)
        "Update.exe --update (pid $($Run.pid)$(if ($Run.started_by_application) { ', started by the application' }))"
    }
    $result = { param([string] $Classification, [string] $Basis) [ordered]@{ classification = $Classification; basis = $Basis } }

    # A crash wins over every other value, as in the installer first-run classifier.
    foreach ($phase in @(@{ verdict = $Facts.observe_verdict; when = 'while the update was observed' },
                         @{ verdict = $Facts.next_start_verdict; when = 'at the next start' })) {
        if ($null -eq $phase.verdict -or $phase.verdict.classification -ne 'started_crashed') { continue }
        $basis = "$($phase.when): $($phase.verdict.basis)"
        $crash = $phase.verdict.crash
        $crashTime = if ($null -ne $crash -and $crash.event_time) { ConvertTo-UtcTime $crash.event_time } else { $null }
        $closeTime = ConvertTo-UtcTime $Facts.close_requested
        if ($null -ne $crashTime -and $null -ne $closeTime -and $crashTime -ge $closeTime -and $phase.when -like 'while*') {
            $basis += '; the crash came after the diagnostic asked the application to close'
        }
        return & $result 'app_crashed' $basis
    }

    # Every sign that the update failed, whether or not a newer folder is there.
    $failures = [System.Collections.Generic.List[string]]::new()
    foreach ($run in @($runs | Where-Object { $null -ne $_.exit_code -and [int64] $_.exit_code -ne 0 })) {
        $failures.Add("$(& $describe $run) exited with $($run.exit_code_hex)")
    }
    foreach ($crash in @($Facts.update_exe_crashes | Where-Object { $null -ne $_ })) {
        $failures.Add("the Application log has a $($crash.provider) $($crash.id) entry for Update.exe" +
                      $(if ($crash.exception_code) { ", exception $($crash.exception_code)" } else { '' }))
    }
    if ($null -ne $log.update_exit_code -and [int64] $log.update_exit_code -ne 0) {
        $failures.Add("the application logged that Update.exe exited with code $($log.update_exit_code)")
    }
    if ($null -ne $log.start_failed) { $failures.Add("the application could not start Update.exe ($($log.start_failed))") }
    if ($null -ne $log.gave_up) { $failures.Add("the application gave up waiting for Update.exe $($log.gave_up)".TrimEnd()) }
    foreach ($line in @($log.unexpected)) { $failures.Add("the application logged: auto update: $line") }
    if (@($Facts.notifications.failure).Count -gt 0) { $failures.Add('the notification history records the failure notice') }
    $stillRunning = @($runs | Where-Object { $_.alive_at_end })
    $cleanExits = @($runs | Where-Object { $null -ne $_.exit_code -and [int64] $_.exit_code -eq 0 })
    if ($null -eq $candidate -and $feed.newer -and $stillRunning.Count -eq 0 -and ($cleanExits.Count -gt 0 -or $log.nothing_newer)) {
        $failures.Add("Update.exe exited with 0 but staged nothing newer than $installed, although the feed holds $($feed.version) ($($feed.tag))")
    }
    if ($null -ne $log.check_error -and $runs.Count -eq 0) { $failures.Add("the release check failed: $($log.check_error)") }

    # What a newer folder with its bambu-studio.exe still lacks to count as staged. The folder alone
    # proves nothing: Update.exe extracts the package straight into it, leaves what it wrote when it
    # fails part of the way, and rewrites packages\RELEASES only once the extraction has finished.
    $gaps = [System.Collections.Generic.List[string]]::new()
    $listed = @()
    if ($null -ne $candidate) {
        $loggedClean = $null -ne $log.update_exit_code -and [int64] $log.update_exit_code -eq 0 -and -not $log.nothing_newer
        if ($cleanExits.Count -eq 0 -and -not $loggedClean) {
            $gaps.Add('no Update.exe --update run was seen to exit with 0, by its exit code or by the application log')
        }
        if ($log.nothing_newer) { $gaps.Add('the application logged that Update.exe found nothing newer to install') }
        $localRows = if ($null -ne $Facts.squirrel) { @($Facts.squirrel.local_releases) } else { @() }
        $listed = @($localRows | ForEach-Object { ConvertFrom-ReleasesRow $_ } | Where-Object {
            $null -ne $_ -and $_.full -and (Compare-VersionParts (ConvertTo-VersionParts $_.version) (ConvertTo-VersionParts $candidate.name)) -eq 0 })
        if ($listed.Count -eq 0) {
            $gaps.Add("packages\RELEASES does not list the full package of $($candidate.version), which Update.exe records only after extracting it")
        }
        if ($null -eq $Facts.staged_files) { $gaps.Add('its files were not compared with the package') }
        else { foreach ($problem in @($Facts.staged_files.problems | Where-Object { $_ })) { $gaps.Add([string] $problem) } }
        if ($null -eq $Facts.next_start) { $gaps.Add('the install root was not started again') }
        else {
            $ran = @($Facts.next_start.folders_run | Where-Object { $_ })
            if ($ran -notcontains $candidate.name) {
                $gaps.Add($(if ($ran.Count -gt 0) { "the next start ran $($ran -join ', '), not $($candidate.name) ($($Facts.next_start.classification))" }
                            else { "the next start ran no app-<version> executable ($($Facts.next_start.classification))" }))
            }
        }
    }
    $product = if ($null -ne $candidate -and $candidate.product_version) { " (product version $($candidate.product_version))" } else { '' }

    if ($null -ne $candidate -and $failures.Count -eq 0 -and $gaps.Count -eq 0) {
        $parts = [System.Collections.Generic.List[string]]::new()
        $parts.Add("$($candidate.name) was staged beside $installed, with its bambu-studio.exe$product")
        if ($runs.Count -eq 0) { $parts.Add('no Update.exe --update run was seen by the polls or the process events') }
        foreach ($run in $runs) {
            $parts.Add("$(& $describe $run) " + $(if ($null -ne $run.exit_code_hex) { "exited with $($run.exit_code_hex)" } else { 'ended, exit code not captured' }))
        }
        $parts.Add($(if ($null -ne $log.update_exit_code) { "the application logged that Update.exe exited with code $($log.update_exit_code)" }
                     elseif (-not $Facts.log_decoded) { 'the application log could not be read' }
                     else { 'the application log has no Update.exe exit line' }))
        if (@($Facts.updated_events).Count -gt 0) { $parts.Add("Squirrel ran $(@($Facts.updated_events)[0]) for the new version") }
        $parts.Add($(if ($candidate.version -eq $feed.version) { "it is the feed's version ($($feed.tag), $($feed.version))" }
                     else { "the feed holds $($feed.version) ($($feed.tag))" }))
        $parts.Add("packages\RELEASES lists $($listed[0].file)")
        $parts.Add("all $($Facts.staged_files.package_files) files of the package are in it at their sizes, and its bambu-studio.exe has the package's SHA-256")
        $parts.Add($(if (@($Facts.notifications.ready).Count -gt 0) { 'the notification history records the ready banner (AppUpdateReady)' }
                     elseif ($Facts.notifications.found) { 'the notification history has no ready banner' }
                     else { 'there is no notification history' }))
        $parts.Add("the next start ran $($candidate.name)\bambu-studio.exe ($($Facts.next_start.classification))")
        return & $result 'updated_staged' ($parts -join '; ')
    }

    if ($failures.Count -gt 0 -or $null -ne $candidate) {
        $reasons = [System.Collections.Generic.List[string]]::new()
        foreach ($failure in $failures) { $reasons.Add($failure) }
        if ($null -ne $candidate) {
            $unfinished = "partial staging: $($candidate.name) holds a bambu-studio.exe$product but is not counted as staged"
            $reasons.Add($(if ($gaps.Count -gt 0) { "${unfinished}: $($gaps -join '; ')" } else { "$unfinished because of the failures above" }))
        }
        elseif ($null -ne $Facts.partial) { $reasons.Add("$($Facts.partial.name) was left without a complete bambu-studio.exe") }
        return & $result 'update_failed' ($reasons -join '; ')
    }

    $offers = [System.Collections.Generic.List[string]]::new()
    foreach ($run in $stillRunning) { $offers.Add("$(& $describe $run) was still running at the end of the observation") }
    foreach ($run in @($runs | Where-Object { -not $_.alive_at_end -and $null -eq $_.exit_code })) {
        $offers.Add("$(& $describe $run) ended with an exit code that was not captured")
    }
    if ($runs.Count -eq 0 -and ($log.release_check -eq 'newer' -or $log.update_started -or $null -ne $log.update_tag)) {
        $offers.Add("the application logged $(if ($null -ne $log.update_tag) { "an update to $($log.update_tag)" } else { "a newer release ($($log.checked_tag))" }), but no Update.exe --update run was seen")
    }
    if ($offers.Count -gt 0) {
        $basis = $offers -join '; '
        if ($null -ne $Facts.partial) { $basis += "; $($Facts.partial.name) was being staged" }
        $basis += "; nothing newer than $installed was staged"
        return & $result 'update_offered_not_staged' $basis
    }

    $seen = [System.Collections.Generic.List[string]]::new()
    if ($runs.Count -gt 0) { $seen.Add("Update.exe --update exited with 0 and staged nothing") }
    else { $seen.Add('no Update.exe --update run was seen') }
    if (-not $Facts.log_decoded) { $seen.Add('the application log could not be read') }
    elseif ($null -ne $log.nothing_to_do) { $seen.Add("the application logged nothing to do for $($log.nothing_to_do)") }
    elseif ($log.release_check -eq 'not_newer') { $seen.Add("the application logged that $($log.checked_tag) is not newer than this build") }
    elseif ($log.not_installed) { $seen.Add('the application logged that it found no Update.exe next to it') }
    elseif ($null -ne $log.check_error) { $seen.Add("the release check failed: $($log.check_error)") }
    else { $seen.Add('the application log has no update-check line') }
    if (-not $feed.newer) { $seen.Add("the feed holds $($feed.version) ($($feed.tag)), not newer than $installed") }
    if ($Facts.setup_wizard_open_at_end) { $seen.Add('the Setup Wizard was still open at the end, so the startup update check may not have run') }
    return & $result 'no_update_seen' ($seen -join '; ')
}

function Write-StepSummary {
    param([AllowNull()][object] $Receipt, [string] $Failure)
    if (-not $env:GITHUB_STEP_SUMMARY) { return }
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("### Self-update from $FromTag")
    $lines.Add('')
    if ($null -ne $Receipt) {
        $lines.Add("Installed ``app-$($Receipt.installed.package_version)``; the feed ($($Receipt.feed.tag)) holds ``$($Receipt.feed.version)``.")
        $lines.Add('')
        $lines.Add('| Classification | Basis |')
        $lines.Add('| --- | --- |')
        $lines.Add("| ``$($Receipt.classification)`` | $(([string] $Receipt.basis).Replace('|', '\|')) |")
    }
    if ($Failure) { $lines.Add(''); $lines.Add("Diagnostic failure: $Failure") }
    Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value $lines -Encoding utf8
}

# Absolute, because .NET file calls do not share this location.
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$failure = $null
$receipt = $null
$sourceCommit = $null
try {
    $LogKeys = Get-LocalLogKeys
    if ($LogKeys.Count -eq 0) { throw 'No local log key was found in src/libslic3r/LogSink.cpp.' }

    # Every release read happens now: Verify-HostedSquirrelInstall.ps1 clears the run's token from this
    # process before anything downloaded runs.
    $from = & gh release view $FromTag --repo $Repository --json tagName,targetCommitish,publishedAt | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the release to install.' }
    $sourceCommit = ([string] $from.targetCommitish).ToLowerInvariant()
    if ($sourceCommit -notmatch '^[0-9a-f]{40}$') { throw "The release target '$sourceCommit' is not a commit; the install check needs one." }
    $latest = & gh release view --repo $Repository --json tagName,targetCommitish,publishedAt | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the latest release.' }
    $feedDirectory = Join-Path $env:RUNNER_TEMP ('self-update-feed-' + $env:GITHUB_RUN_ID)
    New-Item -ItemType Directory -Force -Path $feedDirectory | Out-Null
    & gh release download ([string] $latest.tagName) --repo $Repository --pattern RELEASES --dir $feedDirectory
    if ($LASTEXITCODE -ne 0) { throw "Could not download the RELEASES file of $($latest.tagName)." }
    $feedRows = @(Get-Content -LiteralPath (Join-Path $feedDirectory 'RELEASES') | Where-Object { $_.Trim() })
    if ($feedRows.Count -eq 0) { throw "The RELEASES file of $($latest.tagName) is empty." }
    Set-Content -LiteralPath (Join-Path $OutputDirectory 'feed-RELEASES.txt') -Value $feedRows -Encoding utf8
    $feedVersion = $null
    foreach ($row in @($feedRows | ForEach-Object { ConvertFrom-ReleasesRow $_ } | Where-Object { $null -ne $_ -and $_.full })) {
        if ($null -eq $feedVersion -or (Compare-VersionParts (ConvertTo-VersionParts $row.version) (ConvertTo-VersionParts $feedVersion)) -gt 0) {
            $feedVersion = $row.version
        }
    }
    if ($null -eq $feedVersion) { throw "The RELEASES file of $($latest.tagName) names no full package." }

    $preflight = [ordered]@{
        from_tag = $FromTag
        latest_tag = [string] $latest.tagName
        install_root_absent = -not (Test-Path -LiteralPath $installRoot)
        data_directory_absent = -not (Test-Path -LiteralPath $dataDirectory)
        configuration_absent = -not (Test-Path -LiteralPath $appConfigPath)
        launcher_trace_absent = -not (Test-Path -LiteralPath $launcherTrace)
        session_id = (Get-Process -Id $PID).SessionId
        user_interactive = [Environment]::UserInteractive
        os = [Environment]::OSVersion.VersionString
        powershell = $PSVersionTable.PSVersion.ToString()
        log_key_tags = @($LogKeys.Keys | Sort-Object)
    }
    $preflight | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'preflight.json') -Encoding utf8
    if (-not $preflight.install_root_absent) { throw 'A prior installation exists on this runner.' }

    # A silent install starts nothing, so the first start below is the one a shortcut makes.
    $installReceiptPath = Join-Path $OutputDirectory 'install-receipt.json'
    & (Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1') -Tag $FromTag -Repository $Repository `
        -ExpectedCommit $sourceCommit -OutputPath $installReceiptPath -CiExecutionApproved
    $install = Get-Content -LiteralPath $installReceiptPath -Raw | ConvertFrom-Json
    $installedVersion = [string] $install.package_version
    $installedFolders = Get-AppFolders
    if ($installedFolders.Count -ne 1 -or $installedFolders[0].name -ne "app-$installedVersion" -or -not $installedFolders[0].has_executable) {
        throw "After the installation the install root holds $(@($installedFolders | ForEach-Object { $_.name }) -join ', '), not exactly app-$installedVersion with its bambu-studio.exe."
    }
    $feedNewer = (Compare-VersionParts (ConvertTo-VersionParts $feedVersion) (ConvertTo-VersionParts $installedVersion)) -gt 0
    $preferenceBefore = Read-AutoUpdatePreference
    Write-Host "Installed app-$installedVersion from $FromTag; the feed ($($latest.tagName)) holds $feedVersion."

    $observe = Invoke-ObservationPhase -Name 'observe' -Seconds $ObserveSeconds -InstalledVersion $installedVersion `
        -CloseSetupWizard -StopWhenStaged -CloseAtEnd
    $null = Stop-InstalledProcesses
    $squirrel = Save-SquirrelEvidence -Directory (Join-Path $OutputDirectory 'observe')
    $preferenceAfter = Read-AutoUpdatePreference

    # The candidate: a newer folder with its executable, and no Update.exe --update still at work. It
    # counts as staged only when Get-SelfUpdateClassification finds that Update.exe finished it.
    $newest = Get-NewerFolder -Folders @($observe.final.app_folders) -InstalledVersion $installedVersion
    $stillUpdating = @($observe.update_runs | Where-Object { $_.role -eq 'update' -and $_.alive_at_end }).Count -gt 0 -or
                     @($observe.final.update_exe | Where-Object { ([string] $_.command_line) -match '--update(?:=|\s)' }).Count -gt 0
    $candidate = if ($null -ne $newest -and $newest.has_executable -and -not $stillUpdating) { $newest } else { $null }
    $partial = if ($null -ne $newest -and $null -eq $candidate) { $newest } else { $null }
    # Compared with the package before the next start runs the folder.
    $stagedFiles = if ($null -ne $candidate) {
        Test-StagedFiles -Folder $candidate -FeedReleases $feedRows -LocalReleases @($squirrel.local_releases)
    } else { $null }

    $nextStart = $null
    $nextVerdict = $null
    if ($null -ne $candidate) {
        $next = Invoke-ObservationPhase -Name 'next-start' -Seconds $NextStartSeconds -InstalledVersion $installedVersion
        $null = Stop-InstalledProcesses
        $foldersRun = @($next.processes | Where-Object { $_.role -eq 'app' -and $_.path } | ForEach-Object {
            $match = [regex]::Match([string] $_.path, '\\(?<folder>app-[^\\]+)\\bambu-studio\.exe$')
            if ($match.Success) { $match.Groups['folder'].Value } } | Select-Object -Unique)
        $nextVerdict = $next.verdict
        $nextStart = [ordered]@{
            folders_run = $foldersRun
            classification = $next.verdict.classification
            basis = $next.verdict.basis
            crash = $next.verdict.crash
            started = $next.started
            ended = $next.ended
            processes = $next.processes
            files = $next.files
        }
    }

    $logFacts = Get-UpdateLogFacts -Lines @($observe.app_logs.lines)
    $updatedEvents = @($observe.processes | Where-Object { ([string] $_.command_line) -match '--squirrel-updated' } |
        ForEach-Object { [regex]::Replace(([string] $_.command_line).Trim(), '^"?[^"]*\\(?<folder>app-[^\\]+)\\bambu-studio\.exe"?', '${folder}\bambu-studio.exe') })
    $facts = [ordered]@{
        installed_version = $installedVersion
        feed = [ordered]@{ tag = [string] $latest.tagName; version = $feedVersion; newer = $feedNewer }
        candidate = $candidate
        partial = $partial
        staged_files = $stagedFiles
        squirrel = $squirrel
        update_runs = $observe.update_runs
        update_exe_crashes = $observe.update_exe_crashes
        updated_events = $updatedEvents
        log = $logFacts
        log_decoded = @($observe.app_logs.files | Where-Object { $_.decoded }).Count -gt 0
        notifications = $observe.notifications
        observe_verdict = $observe.verdict
        next_start_verdict = $nextVerdict
        next_start = $nextStart
        close_requested = if ($null -ne $observe.close) { $observe.close.requested } else { $null }
        setup_wizard_open_at_end = $observe.setup_wizard_open_at_end
    }
    $result = Get-SelfUpdateClassification -Facts $facts

    $receipt = [ordered]@{
        schema = 1
        diagnostic = 'self-update'
        from_tag = $FromTag
        from_published = [string] $from.publishedAt
        source_commit = $sourceCommit
        run_id = $env:GITHUB_RUN_ID
        status = 'diagnosed'
        classification = $result.classification
        basis = $result.basis
        installed = [ordered]@{
            package_version = $installedVersion
            product_version = $install.product_version
            app_folders = $installedFolders
        }
        feed = [ordered]@{
            tag = [string] $latest.tagName
            commit = [string] $latest.targetCommitish
            published = [string] $latest.publishedAt
            releases = $feedRows
            version = $feedVersion
            newer_than_installed = $feedNewer
        }
        auto_update = [ordered]@{
            preference = 'auto_update'
            'default' = 'on'
            changed_by_diagnostic = $false
            before_start = $preferenceBefore
            saved_by_application = $preferenceAfter
        }
        setup_wizard = [ordered]@{
            reason = 'On a fresh profile the first-run guide is modal and the startup update check runs after it closes; the diagnostic closes it with WM_CLOSE.'
            closes = $observe.setup_wizard_closes
            open_at_end = $observe.setup_wizard_open_at_end
        }
        observation = [ordered]@{
            started = $observe.started
            observed = $observe.observed
            ended = $observe.ended
            stop_reason = $observe.stop_reason
            observe_seconds = $ObserveSeconds
            poll_seconds = $PollSeconds
            polls = $observe.polls
            close = $observe.close
            process_events = $observe.process_events
        }
        staged = if ($result.classification -eq 'updated_staged') { $candidate } else { $null }
        staged_candidate = $candidate
        staged_files = $stagedFiles
        partial = $partial
        app_folders_at_end = $observe.final.app_folders
        folder_sightings = $observe.folder_sightings
        update_exe = [ordered]@{
            runs = $observe.update_runs
            running_at_end = $observe.final.update_exe
            crashes = $observe.update_exe_crashes
            squirrel_updated_events = $updatedEvents
        }
        app_log = [ordered]@{
            files = $observe.app_logs.files
            update_lines = @($observe.app_logs.lines | Select-Object -Last $MaxListedLines)
            facts = $logFacts
        }
        squirrel = $squirrel
        banner = [ordered]@{
            on_screen = 'not observable: the banner is drawn inside the 3D view and no screenshot is taken'
            seen_in_history = $observe.ready_banner_seen
            notification_history_found = $observe.notifications.found
            notification_history_error = $observe.notifications.error
            ready = $observe.notifications.ready
            failure_notice = $observe.notifications.failure
        }
        application = [ordered]@{
            classification = $observe.verdict.classification
            basis = $observe.verdict.basis
            crash = $observe.verdict.crash
            application_faults = $observe.verdict.application_faults
            not_counted = $observe.verdict.not_counted
        }
        next_start = $nextStart
        processes = $observe.processes
        files = $observe.files
    }
    $receipt | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
    Write-Host "self-update: $($result.classification): $($result.basis)"
}
catch {
    $failure = $_.Exception.Message
    [ordered]@{ schema = 1; diagnostic = 'self-update'; from_tag = $FromTag; source_commit = $sourceCommit; status = 'failed'; failure = $failure } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'failure.json') -Encoding utf8
    throw
}
finally {
    # The runner is disposable, but nothing installed is left running past the diagnosis.
    try { $null = Stop-InstalledProcesses } catch { Write-Warning "Stopping the installed processes failed: $($_.Exception.Message)" }
    try { Write-StepSummary -Receipt $receipt -Failure $failure } catch { Write-Warning "Writing the step summary failed: $($_.Exception.Message)" }
}
