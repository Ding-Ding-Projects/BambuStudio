<#
Diagnoses the first start of the application after a Squirrel installation, on a disposable
GitHub-hosted Windows runner. Dispatched only by .github/workflows/diagnose-installer-first-run.yml.

-Arm Interactive  Installs with Setup.exe and no arguments, in a visible window, the way a person
                  does, so Squirrel itself starts the application with --squirrel-firstrun when it
                  finishes. Then, as a control, stops the application and starts it through the
                  install root's bambu-studio.exe, the target of both shortcuts.
-Arm Silent       Installs with --silent (Squirrel starts nothing), then makes the first start
                  through the install root's bambu-studio.exe.

The release is downloaded and verified, and installed, by Verify-HostedSquirrelInstall.ps1 (with
-Interactive for the interactive arm). While that runs, and for -ObserveSeconds after each start,
every bambu-studio.exe process and the foreground window are polled every five seconds; process
start and stop events add exit codes for processes too short-lived to be polled. Each phase then
collects, as text only, the Squirrel logs, the launcher trace, the newest application logs and the
Application event-log entries about the application, and writes receipt.json with one of:

  not_started      no application process was seen
  started_crashed  an application process was ended by an exception (an NTSTATUS error exit code other
                   than the launcher's own 0xFFFFFFFF), or the Application log has a crash entry for
                   bambu-studio.exe from the phase; this wins over every other value
  started_exited   an application process ran and ended without a crash; the basis says whether it
                   had shown a visible window
  started_hidden   an application process was still running at the end, without a visible window
  started_visible  an application process showed a visible window and was still running at the end,
                   or showed the main frame, and nothing crashed

The classification is the result. The script fails only when the release cannot be verified or
installed, or the install root's bambu-studio.exe cannot be started; never because of a
classification. No screenshots are taken.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Interactive', 'Silent')][string] $Arm,
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [Parameter(Mandatory)][ValidateRange(30, 420)][int] $ObserveSeconds,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $CiExecutionApproved -or -not $IsWindows -or $env:GITHUB_ACTIONS -ne 'true' -or
    $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'The installer first-run diagnostic installs, starts and stops the application; it runs only on an explicitly approved disposable GitHub-hosted Windows runner.'
}

$PollSeconds = 5
$InstallLimitSeconds = 1200
$StubLimitSeconds = 60
$MaxCollectedBytes = [int64] 1MB
$MaxAppLogs = 8
$installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
$stubPath = Join-Path $installRoot 'bambu-studio.exe'
$squirrelTemp = Join-Path $env:LOCALAPPDATA 'SquirrelTemp'
$launcherTrace = Join-Path $env:TEMP 'bbs-launcher-trace.log'
$appLogDirectory = Join-Path (Join-Path $env:APPDATA 'BambuStudio') 'log'
$watchedNames = @('bambu-studio.exe', 'Update.exe', 'Setup.exe')

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class FirstRunWindow
{
    public long Handle { get; set; }
    public uint ProcessId { get; set; }
    public int ZOrder { get; set; }
    public bool Visible { get; set; }
    public bool Minimized { get; set; }
    public string Title { get; set; }
    public string ClassName { get; set; }
    public int Left { get; set; }
    public int Top { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }
}

public static class FirstRunDesktop
{
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { public int Left; public int Top; public int Right; public int Bottom; }

    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] private static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetWindowTextW(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassNameW(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("kernel32.dll")] public static extern uint WTSGetActiveConsoleSessionId();

    private static FirstRunWindow Describe(IntPtr window, int zOrder)
    {
        uint processId;
        GetWindowThreadProcessId(window, out processId);
        StringBuilder title = new StringBuilder(512);
        GetWindowTextW(window, title, title.Capacity);
        StringBuilder className = new StringBuilder(256);
        GetClassNameW(window, className, className.Capacity);
        Rect rect;
        bool hasRect = GetWindowRect(window, out rect);
        FirstRunWindow result = new FirstRunWindow();
        result.Handle = window.ToInt64();
        result.ProcessId = processId;
        result.ZOrder = zOrder;
        result.Visible = IsWindowVisible(window);
        result.Minimized = IsIconic(window);
        result.Title = title.ToString();
        result.ClassName = className.ToString();
        result.Left = hasRect ? rect.Left : 0;
        result.Top = hasRect ? rect.Top : 0;
        result.Width = hasRect ? rect.Right - rect.Left : 0;
        result.Height = hasRect ? rect.Bottom - rect.Top : 0;
        return result;
    }

    // The top-level windows of the given processes, in z-order: 0 is the topmost window of the desktop.
    // Input-method helper windows, which every GUI thread owns and never shows, are left out.
    public static FirstRunWindow[] TopLevel(uint[] processIds)
    {
        HashSet<uint> wanted = new HashSet<uint>(processIds);
        List<FirstRunWindow> result = new List<FirstRunWindow>();
        int zOrder = 0;
        EnumWindows(delegate (IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (wanted.Contains(processId)) {
                FirstRunWindow described = Describe(window, zOrder);
                if (described.ClassName != "IME" && described.ClassName != "MSCTFIME UI")
                    result.Add(described);
            }
            zOrder++;
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }

    public static FirstRunWindow Foreground()
    {
        IntPtr window = GetForegroundWindow();
        return window == IntPtr.Zero ? null : Describe(window, -1);
    }
}
'@

function Format-Utc {
    param([object] $Value)
    if ($null -eq $Value) { return $null }
    return ([datetime] $Value).ToUniversalTime().ToString('o', [Globalization.CultureInfo]::InvariantCulture)
}

# A process exit code as the unsigned hexadecimal Windows shows: -1 and 4294967295 are both 0xFFFFFFFF.
function Format-ExitCode {
    param([object] $Code)
    if ($null -eq $Code) { return $null }
    $value = [int64] $Code
    if ($value -lt 0) { $value += 4294967296 }
    return '0x{0:X8}' -f $value
}

# Whether an exit code is one an exception leaves: an NTSTATUS error, 0xC0000000 (3221225472) and
# above. 0xFFFFFFFF (4294967295) is the launcher's own -1, an ordinary exit.
function Test-CrashExitCode {
    param([object] $Code)
    if ($null -eq $Code) { return $false }
    $value = [int64] $Code
    if ($value -lt 0) { $value += 4294967296 }
    return ($value -ge 3221225472 -and $value -ne 4294967295)
}

# A time as UTC, from a DateTime or from the round-trip text that Format-Utc writes.
function ConvertTo-UtcTime {
    param([object] $Value)
    if ($null -eq $Value) { return $null }
    if ($Value -is [datetime]) { return $Value.ToUniversalTime() }
    $text = [string] $Value
    if (-not $text) { return $null }
    return [datetime]::Parse($text, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
}

function Get-CimValue {
    param([object] $Instance, [Parameter(Mandatory)][string] $Name)
    if ($null -eq $Instance) { return $null }
    $property = $Instance.CimInstanceProperties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

# Process start and stop events (exact, with exit codes, but no command line) and process creation
# events (with the command line, for processes that live at least about a second).
function Register-ProcessTraces {
    param([Parameter(Mandatory)][string] $Prefix)
    # The kernel truncates a process name to 15 characters in start and stop events.
    $traceNames = "ProcessName = 'bambu-studio.exe' OR ProcessName = 'bambu-studio.ex' OR ProcessName = 'Update.exe' OR ProcessName = 'Setup.exe'"
    $queries = [ordered]@{
        start = "SELECT * FROM Win32_ProcessStartTrace WHERE $traceNames"
        stop = "SELECT * FROM Win32_ProcessStopTrace WHERE $traceNames"
        create = "SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process' AND " +
                 "(TargetInstance.Name = 'bambu-studio.exe' OR TargetInstance.Name = 'Update.exe' OR TargetInstance.Name = 'Setup.exe')"
    }
    $registration = [ordered]@{ identifiers = [ordered]@{}; errors = [System.Collections.Generic.List[string]]::new() }
    foreach ($kind in $queries.Keys) {
        $identifier = "$Prefix-$kind"
        try {
            Register-CimIndicationEvent -Query $queries[$kind] -SourceIdentifier $identifier -ErrorAction Stop
            $registration.identifiers[$kind] = $identifier
        }
        catch {
            $registration.errors.Add("${kind}: $($_.Exception.Message)")
        }
    }
    return $registration
}

function Read-ProcessTraces {
    param([Parameter(Mandatory)] $Registration)
    $rows = [System.Collections.Generic.List[object]]::new()
    foreach ($kind in @($Registration.identifiers.Keys)) {
        $identifier = $Registration.identifiers[$kind]
        foreach ($queued in @(Get-Event -SourceIdentifier $identifier -ErrorAction SilentlyContinue)) {
            $cim = $queued.SourceEventArgs.NewEvent
            $row = [ordered]@{
                kind = $kind; time = $queued.TimeGenerated.ToUniversalTime(); pid = $null; ppid = $null
                name = $null; exit_code = $null; path = $null; command_line = $null
            }
            if ($kind -eq 'create') {
                $target = Get-CimValue $cim 'TargetInstance'
                $row.pid = Get-CimValue $target 'ProcessId'
                $row.ppid = Get-CimValue $target 'ParentProcessId'
                $row.name = Get-CimValue $target 'Name'
                $row.path = Get-CimValue $target 'ExecutablePath'
                $row.command_line = Get-CimValue $target 'CommandLine'
                $created = Get-CimValue $target 'CreationDate'
                if ($null -ne $created) { $row.time = ([datetime] $created).ToUniversalTime() }
            }
            else {
                $row.pid = Get-CimValue $cim 'ProcessID'
                $row.ppid = Get-CimValue $cim 'ParentProcessID'
                $row.name = Get-CimValue $cim 'ProcessName'
                $fileTime = Get-CimValue $cim 'TIME_CREATED'
                if ($null -ne $fileTime) { $row.time = [datetime]::FromFileTimeUtc([int64] $fileTime) }
                if ($kind -eq 'stop') { $row.exit_code = Get-CimValue $cim 'ExitStatus' }
            }
            $rows.Add([pscustomobject] $row)
        }
        Unregister-Event -SourceIdentifier $identifier -ErrorAction SilentlyContinue
        Remove-Event -SourceIdentifier $identifier -ErrorAction SilentlyContinue
    }
    return , $rows.ToArray()
}

# One poll: every watched process with its window state, every top-level window of the
# application's processes, the foreground window, and the exit of each process seen before.
function Get-FirstRunSample {
    param([Parameter(Mandatory)][hashtable] $Tracked, [Parameter(Mandatory)][datetime] $Started)
    $now = [datetime]::UtcNow
    $filter = ($watchedNames | ForEach-Object { "Name = '$_'" }) -join ' OR '
    $sampleError = $null
    $processes = @()
    try { $processes = @(Get-CimInstance -ClassName Win32_Process -Filter $filter) }
    catch { $sampleError = "process query: $($_.Exception.Message)" }
    $rows = [System.Collections.Generic.List[object]]::new()
    foreach ($process in $processes) {
        $created = $process.CreationDate
        $key = '{0}/{1}' -f $process.ProcessId, (Format-Utc $created)
        $row = [ordered]@{
            pid = [int] $process.ProcessId; ppid = [int] $process.ParentProcessId; name = [string] $process.Name
            path = [string] $process.ExecutablePath; command_line = [string] $process.CommandLine
            created = Format-Utc $created; session_id = [int] $process.SessionId
            main_window_handle = [int64] 0; main_window_title = ''; responding = $null; error = $null
        }
        try {
            $live = Get-Process -Id $process.ProcessId -ErrorAction Stop
            $row.main_window_handle = $live.MainWindowHandle.ToInt64()
            $row.main_window_title = [string] $live.MainWindowTitle
            $row.responding = [bool] $live.Responding
            if (-not $Tracked.ContainsKey($key)) {
                $null = $live.Handle  # held open, so the exit code can still be read after the exit
                $Tracked[$key] = [ordered]@{
                    process = $live; pid = [int] $process.ProcessId; name = [string] $process.Name
                    created = $created; reported = $false
                }
            }
        }
        catch {
            $row.error = $_.Exception.Message
        }
        $rows.Add([pscustomobject] $row)
    }
    $exits = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in @($Tracked.Values)) {
        if ($entry.reported) { continue }
        try {
            if ($entry.process.HasExited) {
                $entry.reported = $true
                $exits.Add([pscustomobject][ordered]@{
                    pid = $entry.pid; name = $entry.name; created = Format-Utc $entry.created
                    exited = Format-Utc $entry.process.ExitTime; exit_code = [int64] $entry.process.ExitCode
                })
            }
        }
        catch {
            $entry.reported = $true
            $exits.Add([pscustomobject][ordered]@{
                pid = $entry.pid; name = $entry.name; created = Format-Utc $entry.created
                exited = Format-Utc $now; exit_code = $null
            })
        }
    }
    $appIds = [uint32[]] @($rows | Where-Object { $_.name -eq 'bambu-studio.exe' } | ForEach-Object { [uint32] $_.pid })
    $windows = @()
    $foreground = $null
    try {
        if ($appIds.Count -gt 0) { $windows = @([FirstRunDesktop]::TopLevel($appIds)) }
        $window = [FirstRunDesktop]::Foreground()
        if ($null -ne $window) {
            $owner = Get-Process -Id $window.ProcessId -ErrorAction SilentlyContinue
            $foreground = [ordered]@{
                handle = $window.Handle; pid = $window.ProcessId
                process = if ($null -ne $owner) { $owner.ProcessName } else { $null }
                title = $window.Title; class_name = $window.ClassName
            }
        }
    }
    catch {
        $sampleError = (@($sampleError, "windows: $($_.Exception.Message)") | Where-Object { $_ }) -join '; '
    }
    return [pscustomobject][ordered]@{
        time = Format-Utc $now
        elapsed_s = [math]::Round(($now - $Started).TotalSeconds, 1)
        processes = $rows.ToArray()
        windows = $windows
        foreground = $foreground
        exits = $exits.ToArray()
        error = $sampleError
    }
}

function New-ProcessRecord {
    param([Parameter(Mandatory)][int] $ProcessId)
    return [ordered]@{
        pid = $ProcessId; ppid = $null; path = $null; command_line = $null; command_line_source = $null
        role = 'unknown'; started = $null; exited = $null; exit_code = $null; exit_code_hex = $null
        lifetime_ms = $null; exit_time_is_bound = $false; polls = 0; last_seen = $null; alive_at_end = $false; visible_window = $false
        ever_foreground = $false; window_titles = [System.Collections.Generic.List[string]]::new()
    }
}

# One record per bambu-studio.exe process of a phase, from the polls, the held handles, the process
# events and the launcher trace (whose "launcher start" lines carry the process id from this change on).
function Merge-FirstRunProcesses {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Samples,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Events,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]] $TraceLines,
        [Parameter(Mandatory)][datetime] $Since,
        [Parameter(Mandatory)][string] $StubPath
    )
    $records = [ordered]@{}
    $record = {
        param([int] $ProcessId)
        $key = [string] $ProcessId
        if (-not $records.Contains($key)) { $records[$key] = New-ProcessRecord -ProcessId $ProcessId }
        $records[$key]
    }
    $isApp = { param([object] $Name) ([string] $Name) -like 'bambu-studio.ex*' }

    foreach ($sample in $Samples) {
        foreach ($row in @($sample.processes)) {
            if (-not (& $isApp $row.name)) { continue }
            $entry = & $record $row.pid
            $entry.polls++
            $entry.last_seen = [datetime]::Parse($sample.time, $null, 'RoundtripKind').ToUniversalTime()
            if ($row.ppid) { $entry.ppid = $row.ppid }
            if ($row.path) { $entry.path = $row.path }
            if ($row.command_line) { $entry.command_line = $row.command_line; $entry.command_line_source = 'process' }
            if ($row.created -and -not $entry.started) { $entry.started = [datetime]::Parse($row.created, $null, 'RoundtripKind').ToUniversalTime() }
            if ($row.main_window_handle -ne 0) {
                $entry.visible_window = $true
                if ($row.main_window_title -and -not $entry.window_titles.Contains($row.main_window_title)) { $entry.window_titles.Add($row.main_window_title) }
            }
        }
        foreach ($window in @($sample.windows)) {
            $key = [string] $window.ProcessId
            if (-not $records.Contains($key)) { continue }
            if ($window.Visible -and -not $window.Minimized -and $window.Width -gt 0 -and $window.Height -gt 0) {
                $records[$key].visible_window = $true
                if ($window.Title -and -not $records[$key].window_titles.Contains($window.Title)) { $records[$key].window_titles.Add($window.Title) }
            }
        }
        if ($null -ne $sample.foreground -and $records.Contains([string] $sample.foreground.pid)) {
            $records[[string] $sample.foreground.pid].ever_foreground = $true
        }
        foreach ($exit in @($sample.exits)) {
            if (-not (& $isApp $exit.name)) { continue }
            $entry = & $record $exit.pid
            if ($exit.exited) { $entry.exited = [datetime]::Parse($exit.exited, $null, 'RoundtripKind').ToUniversalTime() }
            if ($null -ne $exit.exit_code) { $entry.exit_code = [int64] $exit.exit_code }
        }
    }
    foreach ($traced in $Events) {
        if (-not (& $isApp $traced.name) -or $null -eq $traced.pid) { continue }
        $entry = & $record ([int] $traced.pid)
        if ($null -ne $traced.ppid -and -not $entry.ppid) { $entry.ppid = [int] $traced.ppid }
        switch ($traced.kind) {
            'start' { $entry.started = $traced.time }
            'stop' {
                $entry.exited = $traced.time
                if ($null -ne $traced.exit_code) { $entry.exit_code = [int64] $traced.exit_code }
            }
            'create' {
                if ($traced.path -and -not $entry.path) { $entry.path = $traced.path }
                if ($traced.command_line -and -not $entry.command_line) {
                    $entry.command_line = $traced.command_line; $entry.command_line_source = 'creation-event'
                }
                if (-not $entry.started) { $entry.started = $traced.time }
            }
        }
    }
    foreach ($line in $TraceLines) {
        $match = [regex]::Match($line, '^(?<time>\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}) pid=(?<pid>\d+) launcher start: (?<command>.*)$')
        if (-not $match.Success) { continue }
        $time = [datetime]::ParseExact($match.Groups['time'].Value, 'yyyy-MM-ddTHH:mm:ss.fff', [Globalization.CultureInfo]::InvariantCulture,
                                       [Globalization.DateTimeStyles]::AssumeLocal).ToUniversalTime()
        if ($time -lt $Since.AddSeconds(-1)) { continue }
        $entry = & $record ([int] $match.Groups['pid'].Value)
        if (-not $entry.command_line) { $entry.command_line = $match.Groups['command'].Value; $entry.command_line_source = 'launcher-trace' }
        if (-not $entry.started) { $entry.started = $time }
    }

    $last = if ($Samples.Count -gt 0) { $Samples[-1] } else { $null }
    $alive = @()
    if ($null -ne $last) { $alive = @($last.processes | Where-Object { & $isApp $_.name } | ForEach-Object { [string] $_.pid }) }
    foreach ($entry in $records.Values) {
        $entry.alive_at_end = $alive -contains [string] $entry.pid
        $command = [string] $entry.command_line
        if (-not $entry.path -and $command -match '^\s*"(?<exe>[^"]+)"|^\s*(?<exe>\S+)') { $entry.path = $Matches['exe'] }
        if ($command -match '--squirrel-(install|updated|uninstall|obsolete)\b') { $entry.role = 'install-event' }
        elseif ($command -match '--squirrel-firstrun') { $entry.role = 'firstrun' }
        elseif ($entry.path -and $entry.path -ieq $StubPath) { $entry.role = 'stub' }
        elseif ($entry.path) { $entry.role = 'app' }
        # A process that is missing from a poll after it was seen (or started) has exited by the
        # time of that poll, even when no event or held handle reported its exit.
        if (-not $entry.alive_at_end -and -not $entry.exited) {
            $after = if ($entry.last_seen) { $entry.last_seen } else { $entry.started }
            foreach ($sample in $Samples) {
                $time = [datetime]::Parse($sample.time, $null, 'RoundtripKind').ToUniversalTime()
                if ($null -ne $after -and $time -le $after) { continue }
                if (@($sample.processes | Where-Object { $_.pid -eq $entry.pid }).Count -eq 0) {
                    $entry.exited = $time
                    $entry.exit_time_is_bound = $true
                    break
                }
            }
        }
        $entry.exit_code_hex = Format-ExitCode $entry.exit_code
        if ($entry.started -and $entry.exited) { $entry.lifetime_ms = [int64] ($entry.exited - $entry.started).TotalMilliseconds }
    }
    return , @($records.Values | ForEach-Object {
        $entry = $_
        [pscustomobject][ordered]@{
            pid = $entry.pid; ppid = $entry.ppid; role = $entry.role; path = $entry.path
            command_line = $entry.command_line; command_line_source = $entry.command_line_source
            started = Format-Utc $entry.started; exited = Format-Utc $entry.exited
            exit_code = $entry.exit_code; exit_code_hex = $entry.exit_code_hex; lifetime_ms = $entry.lifetime_ms
            exit_time_is_bound = $entry.exit_time_is_bound
            polls = $entry.polls; alive_at_end = $entry.alive_at_end; visible_window = $entry.visible_window
            ever_foreground = $entry.ever_foreground; window_titles = $entry.window_titles.ToArray()
        }
    })
}

# "0x1E4C", "c0000005" or "000000000311d895" as a number; $null when the text is not hexadecimal.
function ConvertFrom-HexText {
    param([string] $Text)
    $match = [regex]::Match([string] $Text, '^\s*(?:0[xX])?(?<hex>[0-9a-fA-F]{1,16})\s*$')
    if (-not $match.Success) { return $null }
    return [Convert]::ToInt64($match.Groups['hex'].Value, 16)
}

# The Application event-log entries as Save-PhaseEvidence writes them: a line
# "<UTC time> id=<id> provider=<provider> level=<level>", then the message. Crash entries are an
# Application Error 1000 entry, which names the faulting process id, and a Windows Error Reporting
# 1001 entry for an APPCRASH, BEX or BEX64 report, which does not. Both name the application, the
# exception code, the fault offset and the faulting module. Hang reports and other entries are 'other'.
function ConvertFrom-ApplicationEventText {
    param([Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]] $Lines)
    $header = '^(?<time>\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?Z) id=(?<id>\d+) provider=(?<provider>.*?) level=(?<level>.*)$'
    $raw = [System.Collections.Generic.List[object]]::new()
    foreach ($line in @($Lines | ForEach-Object { ([string] $_) -split '\r?\n' })) {
        $text = $line.TrimStart([char] 0xFEFF)
        $match = [regex]::Match($text, $header)
        if ($match.Success) {
            $raw.Add([ordered]@{
                time = ConvertTo-UtcTime $match.Groups['time'].Value; id = [int] $match.Groups['id'].Value
                provider = $match.Groups['provider'].Value; level = $match.Groups['level'].Value
                message = [System.Collections.Generic.List[string]]::new()
            })
        }
        elseif ($raw.Count -gt 0) {
            $raw[$raw.Count - 1].message.Add($text)
        }
    }
    $entries = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in $raw) {
        $message = ($entry.message -join "`n").Trim()
        $field = {
            param([string] $Name)
            $found = [regex]::Match($message, "(?m)^$([regex]::Escape($Name)):[ \t]*(?<value>[^\n]*)$")
            if ($found.Success -and $found.Groups['value'].Value.Trim()) { $found.Groups['value'].Value.Trim() } else { $null }
        }
        $kind = 'other'
        $eventName = $null
        $application = $null
        $processId = $null
        $code = $null
        $offset = $null
        $module = $null
        $modulePath = $null
        if ($entry.provider -eq 'Application Error' -and $entry.id -eq 1000) {
            $kind = 'crash'
            $application = & $field 'Faulting application name'
            $module = & $field 'Faulting module name'
            $code = & $field 'Exception code'
            $offset = & $field 'Fault offset'
            $modulePath = & $field 'Faulting module path'
            $processText = [string] (& $field 'Faulting process id')
            if ($processText -match '^0[xX][0-9a-fA-F]+$') { $processId = [int] (ConvertFrom-HexText $processText) }
            elseif ($processText -match '^\d+$') { $processId = [int] $processText }
        }
        elseif ($entry.provider -eq 'Windows Error Reporting' -and $entry.id -eq 1001) {
            $eventName = & $field 'Event Name'
            if ($eventName -in @('APPCRASH', 'MoAppCrash', 'BEX', 'BEX64')) {
                $kind = 'crash'
                $application = & $field 'P1'
                $module = & $field 'P4'
                # APPCRASH gives the exception code before the offset; BEX and BEX64 the other way round.
                if ($eventName -like 'BEX*') { $offset = & $field 'P7'; $code = & $field 'P8' }
                else { $code = & $field 'P7'; $offset = & $field 'P8' }
            }
        }
        # "bambu-studio.exe, version: 2.8.4.61, time stamp: 0x6ac79c73" names bambu-studio.exe.
        if ($application) { $application = ($application -split ',')[0].Trim() }
        if ($module) { $module = ($module -split ',')[0].Trim() }
        $codeValue = ConvertFrom-HexText $code
        if ($offset -and $offset -notmatch '^0[xX]') { $offset = "0x$offset" }
        $entries.Add([pscustomobject][ordered]@{
            time = $entry.time; id = $entry.id; provider = $entry.provider; level = $entry.level; kind = $kind
            event_name = $eventName; application = $application; pid = $processId
            exception_code = if ($null -ne $codeValue) { Format-ExitCode $codeValue } else { $null }
            fault_offset = $offset; module = $module; module_path = $modulePath; report_id = & $field 'Report Id'
        })
    }
    return , $entries.ToArray()
}

# The main frame's title is "<project> - <display name>", "Untitled - Bambu Studio" on a fresh runner.
# The startup splash has no title.
function Test-MainFrameShown {
    param([Parameter(Mandatory)][object] $Process)
    return (@($Process.window_titles | Where-Object { ([string] $_) -match '\S - Bambu Studio$' }).Count -gt 0)
}

# Install events and Squirrel's stub are not the application. A process whose command line was never
# seen counts as the application, since an install event is long-lived enough to be seen. Not counted
# as the phase's start either: a process that was already running before the phase began, and the
# child with its parent's command line that a faulting process creates at the fault, which is how the
# copy Windows Error Reporting takes of a faulting process appears.
#
# A crash wins over every other classification: an application process ended with a crash exit code
# (Test-CrashExitCode), or a crash entry for bambu-studio.exe in the phase's Application log. A crash
# entry whose process then ended by itself with an ordinary exit code is a handled fault, reported but
# not a crash: the launcher exits with -1 when BambuStudio.dll fails to initialise after a fault in it.
# A visible window counts as a visible start only from a process that was still running at the end,
# or when it was the main frame.
function Get-FirstRunClassification {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Processes,
        [AllowEmptyCollection()][object[]] $ApplicationEvents = @(),
        [object] $Since = $null,
        [object] $Until = $null
    )
    $sinceUtc = ConvertTo-UtcTime $Since
    $untilUtc = ConvertTo-UtcTime $Until
    $appRoles = @('firstrun', 'app', 'unknown')
    $byPid = @{}
    foreach ($process in $Processes) { $byPid[[string] $process.pid] = $process }
    $lifetime = {
        param([object] $Process)
        if ($null -eq $Process.lifetime_ms) { return '' }
        if ($Process.exit_time_is_bound) { return " within $($Process.lifetime_ms) ms" }
        return " after $($Process.lifetime_ms) ms"
    }
    $titles = {
        param([object] $Process)
        $named = @($Process.window_titles | Where-Object { $_ })
        if ($named.Count -gt 0) { return $named -join ' | ' }
        return 'untitled'
    }
    $exitOf = {
        param([object] $Process)
        return [ordered]@{
            pid = $Process.pid; role = $Process.role; exit_code = $Process.exit_code; exit_code_hex = $Process.exit_code_hex
            lifetime_ms = $Process.lifetime_ms; visible_window = [bool] $Process.visible_window
        }
    }

    # Crash entries for the application from the phase window. A Windows Error Reporting entry with the
    # report id of an Application Error entry describes the same fault.
    $entries = @($ApplicationEvents | Where-Object {
        $_.kind -eq 'crash' -and ([string] $_.application) -ieq 'bambu-studio.exe' -and
        ($null -eq $sinceUtc -or $_.time -ge $sinceUtc) -and ($null -eq $untilUtc -or $_.time -le $untilUtc) })
    $reportIds = @($entries | Where-Object { $_.provider -eq 'Application Error' -and $_.report_id } | ForEach-Object { [string] $_.report_id })
    $faults = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in $entries) {
        if ($entry.provider -ne 'Application Error' -and $entry.report_id -and $reportIds -contains [string] $entry.report_id) { continue }
        $process = if ($null -ne $entry.pid) { $byPid[[string] $entry.pid] } else { $null }
        $handled = $null -ne $process -and $null -ne $process.exit_code -and -not (Test-CrashExitCode $process.exit_code)
        $faults.Add([pscustomobject][ordered]@{
            time = Format-Utc $entry.time; event_id = $entry.id; provider = $entry.provider; event_name = $entry.event_name
            pid = $entry.pid; role = if ($null -ne $process) { $process.role } else { $null }
            exception_code = $entry.exception_code; fault_offset = $entry.fault_offset
            faulting_module = $entry.module; faulting_module_path = $entry.module_path; report_id = $entry.report_id
            outcome = if ($handled) { 'handled' } else { 'crash' }
            exit_code_hex = if ($null -ne $process) { $process.exit_code_hex } else { $null }
        })
    }

    # When each process faulted: the time of its crash entries, or its exit with a crash exit code.
    $faultTimes = @{}
    foreach ($fault in $faults) {
        if ($null -eq $fault.pid) { continue }
        $key = [string] $fault.pid
        if (-not $faultTimes.ContainsKey($key)) { $faultTimes[$key] = [System.Collections.Generic.List[datetime]]::new() }
        $faultTimes[$key].Add((ConvertTo-UtcTime $fault.time))
    }
    foreach ($process in $Processes) {
        if (-not (Test-CrashExitCode $process.exit_code) -or -not $process.exited) { continue }
        $key = [string] $process.pid
        if (-not $faultTimes.ContainsKey($key)) { $faultTimes[$key] = [System.Collections.Generic.List[datetime]]::new() }
        $faultTimes[$key].Add((ConvertTo-UtcTime $process.exited))
    }

    $notCounted = [System.Collections.Generic.List[object]]::new()
    foreach ($process in $Processes) {
        if ($process.role -notin $appRoles) { continue }
        $created = ConvertTo-UtcTime $process.started
        if ($null -ne $sinceUtc -and $null -ne $created -and $created -lt $sinceUtc.AddSeconds(-5)) {
            $notCounted.Add([pscustomobject][ordered]@{ pid = $process.pid; role = $process.role; reason = 'started_before_phase'; of_pid = $null })
            continue
        }
        $parentKey = [string] $process.ppid
        if ($null -eq $process.ppid -or $null -eq $created -or -not $faultTimes.ContainsKey($parentKey)) { continue }
        $parent = $byPid[$parentKey]
        $parentCommand = if ($null -ne $parent) { ([string] $parent.command_line).Trim() } else { '' }
        $command = ([string] $process.command_line).Trim()
        $sameCommand = -not $parentCommand -or -not $command -or $parentCommand -ieq $command
        $atFault = @($faultTimes[$parentKey] | Where-Object { [math]::Abs(($created - $_).TotalSeconds) -le 10 }).Count -gt 0
        if ($sameCommand -and $atFault) {
            $notCounted.Add([pscustomobject][ordered]@{ pid = $process.pid; role = $process.role; reason = 'fault_copy'; of_pid = $process.ppid })
        }
    }
    $skipped = @($notCounted | ForEach-Object { [string] $_.pid })
    $candidates = @($Processes | Where-Object { $_.role -in $appRoles -and $skipped -notcontains [string] $_.pid })

    $notes = [System.Collections.Generic.List[string]]::new()
    foreach ($fault in @($faults | Where-Object { $_.outcome -eq 'handled' })) {
        $notes.Add("the Application log reports exception $($fault.exception_code)" +
                   $(if ($fault.faulting_module) { " in $($fault.faulting_module)" } else { '' }) +
                   $(if ($fault.fault_offset) { " at offset $($fault.fault_offset)" } else { '' }) +
                   " in pid $($fault.pid), which then ended by itself with exit code $($fault.exit_code_hex)")
    }
    foreach ($skip in $notCounted) {
        if ($skip.reason -eq 'fault_copy') { $notes.Add("pid $($skip.pid), created by pid $($skip.of_pid) at its fault with the same command line, is not counted") }
        else { $notes.Add("pid $($skip.pid), which was running before the phase began, is not counted") }
    }
    $suffix = if ($notes.Count -gt 0) { '; ' + ($notes -join '; ') } else { '' }
    $verdict = {
        param([string] $Classification, [object] $ProcessId, [object] $Exit, [string] $Basis, [object] $Crash)
        return [ordered]@{
            classification = $Classification; pid = $ProcessId; exit = $Exit; basis = $Basis + $suffix; crash = $Crash
            application_faults = $faults.ToArray(); not_counted = $notCounted.ToArray()
        }
    }

    # Crashes: application processes with a crash exit code, and crash entries that were not handled.
    $crashes = [System.Collections.Generic.List[object]]::new()
    foreach ($process in @($candidates | Where-Object { Test-CrashExitCode $_.exit_code })) {
        $crashes.Add([ordered]@{ pid = $process.pid; process = $process; fault = $null; time = ConvertTo-UtcTime $process.exited })
    }
    foreach ($fault in @($faults | Where-Object { $_.outcome -eq 'crash' })) {
        $known = @($crashes | Where-Object { $null -ne $fault.pid -and [string] $_.pid -eq [string] $fault.pid })
        if ($known.Count -gt 0) {
            if ($null -eq $known[0].fault) { $known[0].fault = $fault; $known[0].time = ConvertTo-UtcTime $fault.time }
            continue
        }
        $process = if ($null -ne $fault.pid) { $byPid[[string] $fault.pid] } else { $null }
        $crashes.Add([ordered]@{ pid = $fault.pid; process = $process; fault = $fault; time = ConvertTo-UtcTime $fault.time })
    }
    if ($crashes.Count -gt 0) {
        # The first crash of a process the polls or events recorded explains the phase best.
        $first = @($crashes | Sort-Object -Property @{ Expression = { $null -eq $_.process } },
                                                    @{ Expression = { if ($null -ne $_.time) { $_.time } else { [datetime]::MaxValue } } })[0]
        $process = $first.process
        $fault = $first.fault
        $byExit = $null -ne $process -and (Test-CrashExitCode $process.exit_code)
        $code = if ($null -ne $fault -and $fault.exception_code) { $fault.exception_code } elseif ($byExit) { $process.exit_code_hex } else { $null }
        $windowTitles = @()
        if ($null -ne $process) { $windowTitles = @($process.window_titles) }
        $copies = @($notCounted | Where-Object { $_.reason -eq 'fault_copy' -and [string] $_.of_pid -eq [string] $first.pid } | ForEach-Object { $_.pid })
        $crash = [ordered]@{
            pid = $first.pid
            role = if ($null -ne $process) { $process.role } else { $null }
            source = if ($byExit -and $null -ne $fault) { 'exit_code_and_application_log' } elseif ($byExit) { 'exit_code' } else { 'application_log' }
            exception_code = $code
            fault_offset = if ($null -ne $fault) { $fault.fault_offset } else { $null }
            faulting_module = if ($null -ne $fault) { $fault.faulting_module } else { $null }
            faulting_module_path = if ($null -ne $fault) { $fault.faulting_module_path } else { $null }
            event_time = if ($null -ne $fault) { $fault.time } else { $null }
            report_id = if ($null -ne $fault) { $fault.report_id } else { $null }
            exit_code = if ($null -ne $process) { $process.exit_code } else { $null }
            exit_code_hex = if ($null -ne $process) { $process.exit_code_hex } else { $null }
            lifetime_ms = if ($null -ne $process) { $process.lifetime_ms } else { $null }
            window_shown_before_crash = if ($null -ne $process) { [bool] $process.visible_window } else { $null }
            window_titles = $windowTitles
            fault_copies = $copies
            crashes_in_phase = $crashes.Count
        }
        $basis = if ($null -eq $first.pid) { 'a bambu-studio.exe process that the Application log does not identify crashed' }
                 elseif ($null -eq $process) { "pid $($first.pid), which no poll or process event recorded, crashed" }
                 else { "pid $($first.pid) ($($process.role)) crashed" }
        if ($code) { $basis += ": exception $code" }
        if ($crash.faulting_module) { $basis += " in $($crash.faulting_module)" }
        if ($crash.fault_offset) { $basis += " at offset $($crash.fault_offset)" }
        if ($null -ne $process) {
            if ($null -ne $process.exit_code_hex -and $process.exit_code_hex -ne $code) { $basis += ", exit code $($process.exit_code_hex)" }
            $basis += & $lifetime $process
            $basis += if ($process.visible_window) { "; it had shown a visible window ($(& $titles $process)) before the crash" } else { '; it had shown no visible window before the crash' }
        }
        if ($crashes.Count -gt 1) { $basis += "; $($crashes.Count - 1) more crash(es) in this phase" }
        $exit = if ($null -ne $process) { & $exitOf $process } else { $null }
        return & $verdict 'started_crashed' $first.pid $exit $basis $crash
    }

    $shown = @(@($candidates | Where-Object { $_.visible_window -and $_.alive_at_end }) +
               @($candidates | Where-Object { $_.visible_window -and -not $_.alive_at_end -and (Test-MainFrameShown $_) }))
    if ($shown.Count -gt 0) {
        $window = $shown[0]
        $basis = "pid $($window.pid) ($($window.role)) showed a visible window: $(& $titles $window)" +
                 $(if ($window.ever_foreground) { '; it was the foreground window' } else { '; it was never the foreground window' })
        if ($window.alive_at_end) { $basis += '; it was still running at the end' }
        else {
            $basis += '; it was the main frame, and the process then ended without a crash, exit code ' +
                      $(if ($null -ne $window.exit_code_hex) { $window.exit_code_hex } else { 'unknown' }) + (& $lifetime $window)
        }
        return & $verdict 'started_visible' $window.pid $null $basis $null
    }
    $hidden = @($candidates | Where-Object { $_.alive_at_end })
    if ($hidden.Count -gt 0) {
        $running = $hidden[0]
        $basis = "pid $($running.pid) ($($running.role)) was still running at the end, seen in $($running.polls) polls, without a visible window"
        foreach ($ended in @($candidates | Where-Object { $_.visible_window -and -not $_.alive_at_end })) {
            $basis += "; pid $($ended.pid) ($($ended.role)) had shown a visible window ($(& $titles $ended)) and ended without a crash"
        }
        return & $verdict 'started_hidden' $running.pid $null $basis $null
    }
    $exited = @($candidates | Where-Object { $_.exited } | Sort-Object -Property exited)
    if ($exited.Count -gt 0) {
        # A process that showed a window and then ended is the one to report: the window is the news.
        $windowed = @($exited | Where-Object { $_.visible_window })
        $ended = if ($windowed.Count -gt 0) { $windowed[-1] } else { $exited[-1] }
        $code = if ($null -ne $ended.exit_code_hex) { $ended.exit_code_hex } else { 'unknown' }
        $basis = if ($ended.visible_window) {
            "pid $($ended.pid) ($($ended.role)) showed a visible window ($(& $titles $ended)) and then ended without a crash, exit code $code" + (& $lifetime $ended)
        }
        else {
            "pid $($ended.pid) ($($ended.role)) ended without a visible window, exit code $code" + (& $lifetime $ended)
        }
        return & $verdict 'started_exited' $ended.pid (& $exitOf $ended) $basis $null
    }
    return & $verdict 'not_started' $null $null 'no application process was seen' $null
}

# Copies the last MaxCollectedBytes of a text file the application may still be writing.
function Save-TextFile {
    param(
        [Parameter(Mandatory)][string] $Source,
        [Parameter(Mandatory)][string] $Destination,
        [Parameter(Mandatory)][AllowEmptyCollection()][System.Collections.Generic.List[object]] $Inventory
    )
    try {
        $stream = [System.IO.File]::Open($Source, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]'ReadWrite, Delete')
        try {
            $length = [int64] $stream.Length
            $take = [int64] [math]::Min($length, $MaxCollectedBytes)
            $null = $stream.Seek($length - $take, [System.IO.SeekOrigin]::Begin)
            $buffer = [byte[]]::new([int] $take)
            $read = 0
            while ($read -lt $take) {
                $count = $stream.Read($buffer, $read, [int] $take - $read)
                if ($count -le 0) { break }
                $read += $count
            }
            if ($read -lt $take) { [Array]::Resize([ref] $buffer, $read) }
        }
        finally {
            $stream.Dispose()
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        [System.IO.File]::WriteAllBytes($Destination, $buffer)
        $Inventory.Add([pscustomobject][ordered]@{
            source = $Source; saved = $Destination; bytes = $length; saved_bytes = $buffer.Length
            truncated = $buffer.Length -lt $length
            sha256 = [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($buffer)).ToLowerInvariant()
        })
    }
    catch {
        $Inventory.Add([pscustomobject][ordered]@{ source = $Source; saved = $null; error = $_.Exception.Message })
    }
}

function Save-PhaseEvidence {
    param([Parameter(Mandatory)][string] $Directory, [Parameter(Mandatory)][datetime] $Since)
    $inventory = [System.Collections.Generic.List[object]]::new()
    $logs = Join-Path $Directory 'logs'
    foreach ($folder in @(@{ path = $squirrelTemp; name = 'squirrel-temp' }, @{ path = $installRoot; name = 'install-root' })) {
        if (-not (Test-Path -LiteralPath $folder.path -PathType Container)) { continue }
        foreach ($file in @(Get-ChildItem -LiteralPath $folder.path -File -Filter '*.log')) {
            Save-TextFile -Source $file.FullName -Destination (Join-Path (Join-Path $logs $folder.name) $file.Name) -Inventory $inventory
        }
    }
    if (Test-Path -LiteralPath $launcherTrace -PathType Leaf) {
        Save-TextFile -Source $launcherTrace -Destination (Join-Path (Join-Path $logs 'temp') 'bbs-launcher-trace.log') -Inventory $inventory
    }
    if (Test-Path -LiteralPath $appLogDirectory -PathType Container) {
        $recent = @(Get-ChildItem -LiteralPath $appLogDirectory -File |
            Where-Object { $_.Extension -in @('.log', '.txt') -and $_.LastWriteTimeUtc -ge $Since.AddSeconds(-1) } |
            Sort-Object -Property LastWriteTimeUtc -Descending | Select-Object -First $MaxAppLogs)
        foreach ($file in $recent) {
            Save-TextFile -Source $file.FullName -Destination (Join-Path (Join-Path $logs 'app') $file.Name) -Inventory $inventory
        }
    }
    # Application event-log errors and crash reports that name the application.
    $eventText = [System.Collections.Generic.List[string]]::new()
    try {
        $entries = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $Since.ToLocalTime() } -ErrorAction Stop |
            Where-Object { $_.Level -in @(1, 2) -or $_.ProviderName -in @('Windows Error Reporting', 'Application Error', 'Application Hang') } |
            Where-Object { [string] $_.Message -match 'bambu-studio|BambuStudio' })
        foreach ($entry in $entries) {
            $eventText.Add(('{0} id={1} provider={2} level={3}' -f (Format-Utc $entry.TimeCreated), $entry.Id, $entry.ProviderName, $entry.LevelDisplayName))
            $eventText.Add([string] $entry.Message)
            $eventText.Add('')
        }
        if ($entries.Count -eq 0) { $eventText.Add('No Application event-log entry names the application.') }
    }
    catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { $eventText.Add('No Application event-log entry since the phase started.') }
        else { $eventText.Add("Reading the Application event log failed: $($_.Exception.Message)") }
    }
    $eventPath = Join-Path $logs 'application-events.txt'
    New-Item -ItemType Directory -Force -Path $logs | Out-Null
    Set-Content -LiteralPath $eventPath -Value $eventText -Encoding utf8
    $inventory.Add([pscustomobject][ordered]@{ source = 'Application event log'; saved = $eventPath; bytes = (Get-Item -LiteralPath $eventPath).Length })
    # The event text goes back to the classification too, which reads it with ConvertFrom-ApplicationEventText.
    return [pscustomobject][ordered]@{ files = $inventory.ToArray(); application_events = $eventText.ToArray() }
}

# Stops every process started from the install root, so the control start begins from nothing.
function Stop-InstalledProcesses {
    $prefix = $installRoot.TrimEnd('\') + '\'
    for ($attempt = 0; $attempt -lt 10; ++$attempt) {
        $running = @(Get-CimInstance -ClassName Win32_Process | Where-Object {
            $_.ExecutablePath -and ([string] $_.ExecutablePath).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) })
        if ($running.Count -eq 0) { return $true }
        foreach ($process in $running) { Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Start-InstalledStub {
    # Never hand workflow credentials to installed code, even when an earlier step failed before clearing them.
    foreach ($credentialName in @('GH_TOKEN', 'GITHUB_TOKEN', 'ORG_TOKEN', 'RELEASE_TOKEN')) {
        [Environment]::SetEnvironmentVariable($credentialName, $null, 'Process')
    }
    if (-not (Test-Path -LiteralPath $stubPath -PathType Leaf)) { throw "The install root has no '$stubPath'." }
    $process = Start-Process -FilePath $stubPath -WorkingDirectory $installRoot -PassThru
    $null = $process.Handle  # held open, so the exit code can still be read after the exit
    return $process
}

# Observes one start: polls until the trigger (the installation, or Squirrel's stub) has finished
# and then for ObserveSeconds more, then collects the evidence and writes receipt.json.
function Invoke-FirstRunPhase {
    param(
        [Parameter(Mandatory)][string] $Name,
        [Parameter(Mandatory)][string] $Trigger,
        [Parameter(Mandatory)][scriptblock] $State,
        [Parameter(Mandatory)][int] $LimitSeconds
    )
    $directory = Join-Path $armDirectory $Name
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $started = [datetime]::UtcNow
    $registration = Register-ProcessTraces -Prefix "$Arm-$Name"
    $samples = [System.Collections.Generic.List[object]]::new()
    $tracked = @{}
    $samplePath = Join-Path $directory 'samples.jsonl'
    $finished = $null
    $outcome = 'running'
    $events = @()
    try {
        while ($true) {
            $tick = [datetime]::UtcNow.AddSeconds($PollSeconds)
            $sample = Get-FirstRunSample -Tracked $tracked -Started $started
            $samples.Add($sample)
            Add-Content -LiteralPath $samplePath -Value ($sample | ConvertTo-Json -Depth 6 -Compress) -Encoding utf8
            $now = [datetime]::UtcNow
            if ($null -eq $finished) {
                $outcome = [string] (& $State)
                if ($outcome -ne 'running') { $finished = $now }
                elseif (($now - $started).TotalSeconds -ge $LimitSeconds) { $outcome = 'limit-reached'; $finished = $now }
            }
            if ($null -ne $finished -and ($outcome -eq 'failed' -or ($now - $finished).TotalSeconds -ge $ObserveSeconds)) { break }
            $wait = [int] ($tick - [datetime]::UtcNow).TotalMilliseconds
            if ($wait -gt 0) { Start-Sleep -Milliseconds $wait }
        }
    }
    finally {
        $events = Read-ProcessTraces -Registration $registration
        ConvertTo-Json -InputObject @($events | ForEach-Object {
            [ordered]@{ kind = $_.kind; time = Format-Utc $_.time; pid = $_.pid; ppid = $_.ppid; name = $_.name
                        exit_code = $_.exit_code; exit_code_hex = Format-ExitCode $_.exit_code; path = $_.path; command_line = $_.command_line }
        }) -Depth 4 | Set-Content -LiteralPath (Join-Path $directory 'process-events.json') -Encoding utf8
    }
    $traceLines = @()
    if (Test-Path -LiteralPath $launcherTrace -PathType Leaf) { $traceLines = @(Get-Content -LiteralPath $launcherTrace -Encoding utf8) }
    $processes = Merge-FirstRunProcesses -Samples $samples.ToArray() -Events $events -TraceLines $traceLines -Since $started -StubPath $stubPath
    # The evidence first: the Application log entries it collects can make the start a crash.
    $evidence = Save-PhaseEvidence -Directory $directory -Since $started
    $collected = [datetime]::UtcNow
    $applicationEvents = ConvertFrom-ApplicationEventText -Lines $evidence.application_events
    $verdict = Get-FirstRunClassification -Processes $processes -ApplicationEvents $applicationEvents -Since $started -Until $collected
    $receipt = [ordered]@{
        schema = 1
        arm = $Arm.ToLowerInvariant()
        phase = $Name
        trigger = $Trigger
        trigger_outcome = $outcome
        release_tag = $Tag
        source_commit = $sourceCommit
        run_id = $env:GITHUB_RUN_ID
        status = 'diagnosed'
        classification = $verdict.classification
        basis = $verdict.basis
        exit = $verdict.exit
        crash = $verdict.crash
        application_faults = $verdict.application_faults
        not_counted = $verdict.not_counted
        started = Format-Utc $started
        trigger_finished = Format-Utc $finished
        ended = Format-Utc ([datetime]::UtcNow)
        observe_seconds = $ObserveSeconds
        poll_seconds = $PollSeconds
        polls = $samples.Count
        process_events = [ordered]@{ registered = @($registration.identifiers.Keys); errors = $registration.errors.ToArray(); count = @($events).Count }
        processes = $processes
        files = $evidence.files
    }
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $directory 'receipt.json') -Encoding utf8
    Write-Host "$Arm/${Name}: $($verdict.classification): $($verdict.basis)"
    return [pscustomobject][ordered]@{ phase = $Name; classification = $verdict.classification; basis = $verdict.basis }
}

function Write-StepSummary {
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Phases, [string] $Failure)
    if (-not $env:GITHUB_STEP_SUMMARY) { return }
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("### Installer first run: $Arm install of $Tag")
    $lines.Add('')
    $lines.Add('| Phase | Classification | Basis |')
    $lines.Add('| --- | --- | --- |')
    foreach ($phase in $Phases) { $lines.Add("| $($phase.phase) | ``$($phase.classification)`` | $(([string] $phase.basis).Replace('|', '\|')) |") }
    if ($Failure) { $lines.Add(''); $lines.Add("Diagnostic failure: $Failure") }
    Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value $lines -Encoding utf8
}

# Absolute, because .NET file calls and the installation's thread job do not share this location.
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$armDirectory = Join-Path $OutputDirectory $Arm.ToLowerInvariant()
New-Item -ItemType Directory -Force -Path $armDirectory | Out-Null
$phases = [System.Collections.Generic.List[object]]::new()
$failure = $null
$sourceCommit = $null
try {
    $release = & gh release view $Tag --repo $Repository --json targetCommitish | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the published release.' }
    $sourceCommit = ([string] $release.targetCommitish).ToLowerInvariant()
    if ($sourceCommit -notmatch '^[0-9a-f]{40}$') { throw "The release target '$sourceCommit' is not a commit; the install check needs one." }

    $preflight = [ordered]@{
        arm = $Arm.ToLowerInvariant()
        install_root = $installRoot
        install_root_absent = -not (Test-Path -LiteralPath $installRoot)
        launcher_trace_absent = -not (Test-Path -LiteralPath $launcherTrace)
        gpu = @(Get-CimInstance -ClassName Win32_VideoController | ForEach-Object {
            [ordered]@{ name = $_.Name; driver_version = $_.DriverVersion; status = $_.Status; video_mode = $_.VideoModeDescription } })
        session_id = (Get-Process -Id $PID).SessionId
        console_session_id = [FirstRunDesktop]::WTSGetActiveConsoleSessionId()
        user_interactive = [Environment]::UserInteractive
        os = [Environment]::OSVersion.VersionString
        powershell = $PSVersionTable.PSVersion.ToString()
    }
    $preflight | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $armDirectory 'preflight.json') -Encoding utf8
    if (-not $preflight.install_root_absent) { throw 'A prior installation exists on this runner.' }

    # Verify-HostedSquirrelInstall.ps1 downloads and verifies the release, clears the workflow
    # credentials, installs it and checks the installed files and shortcuts. It runs beside the
    # observation, so whatever Squirrel starts at the end of the installation is seen.
    $verifyArguments = @{
        Tag = $Tag; Repository = $Repository; ExpectedCommit = $sourceCommit
        OutputPath = (Join-Path $armDirectory 'install-receipt.json'); CiExecutionApproved = $true
        Interactive = ($Arm -eq 'Interactive')
    }
    $installJob = Start-ThreadJob -Name 'squirrel-install' -StreamingHost $Host -ArgumentList (Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1'), $verifyArguments -ScriptBlock {
        param([string] $Verify, [hashtable] $Arguments)
        $ErrorActionPreference = 'Stop'
        & $Verify @Arguments
    }
    $installState = {
        switch ($installJob.State.ToString()) {
            'NotStarted' { 'running' }
            'Running' { 'running' }
            'Completed' { 'done' }
            default { 'failed' }
        }
    }.GetNewClosure()
    $installFailure = $null
    try {
        $installPhase = if ($Arm -eq 'Interactive') { 'install-firstrun' } else { 'install-silent' }
        $phases.Add((Invoke-FirstRunPhase -Name $installPhase -Trigger 'setup' -State $installState -LimitSeconds $InstallLimitSeconds))
    }
    finally {
        if ($installJob.State -in @('NotStarted', 'Running')) { Stop-Job -Job $installJob }
        try { Receive-Job -Job $installJob -Wait -ErrorAction Stop | Out-Null }
        catch { $installFailure = $_.Exception.Message }
        if ($installJob.State -ne 'Completed' -and -not $installFailure) { $installFailure = "The installation ended in state $($installJob.State)." }
        Remove-Job -Job $installJob -Force
    }
    if ($installFailure) { throw "Installation failed: $installFailure" }

    if ($Arm -eq 'Interactive') {
        # The control: the same installation, started the way the shortcuts start it.
        $stopped = Stop-InstalledProcesses
        if (-not $stopped) { throw 'The application processes of the first run could not be stopped for the control start.' }
        if (Test-Path -LiteralPath $launcherTrace -PathType Leaf) {
            Move-Item -LiteralPath $launcherTrace -Destination "$launcherTrace.$installPhase" -Force
        }
    }
    $stub = Start-InstalledStub
    $stubState = { if ($stub.HasExited) { 'done' } else { 'running' } }.GetNewClosure()
    $stubPhase = if ($Arm -eq 'Interactive') { 'control-stub' } else { 'first-stub' }
    $phases.Add((Invoke-FirstRunPhase -Name $stubPhase -Trigger 'stub' -State $stubState -LimitSeconds $StubLimitSeconds))
}
catch {
    $failure = $_.Exception.Message
    [ordered]@{ schema = 1; arm = $Arm.ToLowerInvariant(); release_tag = $Tag; source_commit = $sourceCommit; status = 'failed'; failure = $failure } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $armDirectory 'failure.json') -Encoding utf8
    throw
}
finally {
    # The runner is disposable, but nothing installed is left running past the diagnosis.
    try { $null = Stop-InstalledProcesses } catch { Write-Warning "Stopping the installed processes failed: $($_.Exception.Message)" }
    try { Write-StepSummary -Phases $phases.ToArray() -Failure $failure } catch { Write-Warning "Writing the step summary failed: $($_.Exception.Message)" }
}
