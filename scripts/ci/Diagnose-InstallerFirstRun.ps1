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
  started_exited   an application process ran and ended without a visible window
  started_hidden   an application process was still running at the end, without a visible window
  started_visible  an application process showed a visible window

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

# Install events and Squirrel's stub are not the application. A process whose command line was never
# seen counts as the application, since an install event is long-lived enough to be seen.
function Get-FirstRunClassification {
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Processes)
    $candidates = @($Processes | Where-Object { $_.role -in @('firstrun', 'app', 'unknown') })
    $visible = @($candidates | Where-Object { $_.visible_window })
    if ($visible.Count -gt 0) {
        $shown = $visible[0]
        return [ordered]@{
            classification = 'started_visible'; pid = $shown.pid; exit = $null
            basis = "pid $($shown.pid) ($($shown.role)) showed a visible window: $($shown.window_titles -join ' | ')" +
                    $(if ($shown.ever_foreground) { '; it was the foreground window' } else { '; it was never the foreground window' })
        }
    }
    $hidden = @($candidates | Where-Object { $_.alive_at_end })
    if ($hidden.Count -gt 0) {
        $running = $hidden[0]
        return [ordered]@{
            classification = 'started_hidden'; pid = $running.pid; exit = $null
            basis = "pid $($running.pid) ($($running.role)) was still running at the end, seen in $($running.polls) polls, without a visible window"
        }
    }
    $exited = @($candidates | Where-Object { $_.exited } | Sort-Object -Property exited)
    if ($exited.Count -gt 0) {
        $ended = $exited[-1]
        return [ordered]@{
            classification = 'started_exited'; pid = $ended.pid
            exit = [ordered]@{ pid = $ended.pid; role = $ended.role; exit_code = $ended.exit_code; exit_code_hex = $ended.exit_code_hex; lifetime_ms = $ended.lifetime_ms }
            basis = "pid $($ended.pid) ($($ended.role)) ended without a visible window, exit code " +
                    $(if ($null -ne $ended.exit_code_hex) { $ended.exit_code_hex } else { 'unknown' }) +
                    $(if ($null -ne $ended.lifetime_ms) { $(if ($ended.exit_time_is_bound) { " within $($ended.lifetime_ms) ms" } else { " after $($ended.lifetime_ms) ms" }) } else { '' })
        }
    }
    return [ordered]@{ classification = 'not_started'; pid = $null; exit = $null; basis = 'no application process was seen' }
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
    return , $inventory.ToArray()
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
    $verdict = Get-FirstRunClassification -Processes $processes
    $files = Save-PhaseEvidence -Directory $directory -Since $started
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
        started = Format-Utc $started
        trigger_finished = Format-Utc $finished
        ended = Format-Utc ([datetime]::UtcNow)
        observe_seconds = $ObserveSeconds
        poll_seconds = $PollSeconds
        polls = $samples.Count
        process_events = [ordered]@{ registered = @($registration.identifiers.Keys); errors = $registration.errors.ToArray(); count = @($events).Count }
        processes = $processes
        files = $files
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
