[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet(100,125,150,200)][int] $ScalePercent,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][string] $CheapExecutable,
    [string] $ActionScript,
    [ValidateRange(10,2400)][int] $ActionTimeoutSeconds = 60,
    [ValidateSet('supervisor','run','restore')][string] $Mode = 'supervisor'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
    $env:RUNNER_OS -cne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'Disposable hosted Windows execution is required.'
}
$tempRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be within RUNNER_TEMP.'
}
if (-not (Test-Path -LiteralPath $CheapExecutable -PathType Leaf)) { throw 'Cheap input executable unavailable.' }
if ($ActionScript -and -not (Test-Path -LiteralPath $ActionScript -PathType Leaf)) { throw 'Action script unavailable.' }
$pwsh = (Get-Process -Id $PID).Path
$originalPath = Join-Path $output 'original.json'

# Capture child output without forwarding provider text. All public diagnostics
# are fixed strings. Every process has a bounded wait and verified termination.
function Invoke-BoundedProcess([string] $Executable, [string[]] $Arguments, [int] $Seconds) {
    $p = [Diagnostics.Process]::new()
    $p.StartInfo = [Diagnostics.ProcessStartInfo]::new($Executable)
    $p.StartInfo.UseShellExecute = $false
    $p.StartInfo.CreateNoWindow = $true
    $p.StartInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $p.StartInfo.RedirectStandardOutput = $true
    $p.StartInfo.RedirectStandardError = $true
    foreach ($arg in $Arguments) { [void]$p.StartInfo.ArgumentList.Add($arg) }
    $started = $false
    $terminated = $false
    try {
        $started = $p.Start()
        if (-not $started) { throw 'Child process unavailable.' }
        $stdout = $p.StandardOutput.ReadToEndAsync()
        $stderr = $p.StandardError.ReadToEndAsync()
        $terminated = $p.WaitForExit($Seconds * 1000)
        if (-not $terminated) {
            $p.Kill($true)
            $terminated = $p.WaitForExit(5000)
            return @{ terminated = $terminated; code = -1; stdout = '' }
        }
        return @{ terminated = $true; code = $p.ExitCode; stdout = $stdout.GetAwaiter().GetResult() }
    } finally {
        if ($started -and -not $terminated) {
            try { if (-not $p.HasExited) { $p.Kill($true) }; [void]$p.WaitForExit(5000) } catch {}
        }
        $p.Dispose()
    }
}

if ($Mode -eq 'supervisor') {
    if (Test-Path -LiteralPath $output) { throw 'Output directory must be new.' }
    [void](New-Item -ItemType Directory -Path $output)
    $run = @{ terminated = $false; code = -1 }
    $recovery = @{ terminated = $true; code = 0 }
    try {
        $arguments = @('-NoProfile','-File',$PSCommandPath,'-ScalePercent',"$ScalePercent",
            '-OutputDirectory',$output,'-CheapExecutable',$CheapExecutable,
            '-ActionTimeoutSeconds',"$ActionTimeoutSeconds",'-Mode','run')
        if ($ActionScript) { $arguments += @('-ActionScript',[IO.Path]::GetFullPath($ActionScript)) }
        $run = Invoke-BoundedProcess $pwsh $arguments ($ActionTimeoutSeconds + 120)
    } catch {} finally {
        # The durable original state exists before the first input. Recovery is
        # isolated too, and never races an unterminated first worker.
        if ($run.terminated -and (Test-Path -LiteralPath $originalPath)) {
            try {
                $recovery = Invoke-BoundedProcess $pwsh @('-NoProfile','-File',$PSCommandPath,
                    '-ScalePercent',"$ScalePercent",'-OutputDirectory',$output,
                    '-CheapExecutable',$CheapExecutable,'-Mode','restore') 60
            } catch { $recovery = @{ terminated = $false; code = -1 } }
        } elseif (-not $run.terminated) {
            $recovery = @{ terminated = $false; code = -1 }
        }
        $restored = $recovery.terminated -and $recovery.code -eq 0
        $success = $run.terminated -and $run.code -eq 0 -and $restored
        @{schema=1; status=$(if ($success) {'verified_settings_scale_and_restoration'} else {'unavailable'})
          requested_scale=$ScalePercent; worker_termination_verified=$run.terminated
          recovery_termination_verified=$recovery.terminated; restoration_verified=$restored
          target_application_dpi='requires_independent_runtime_measurement'
          disposal_required=(-not $run.terminated -or -not $restored)
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'supervisor.json') -Encoding utf8
    }
    if ($success) { exit 0 }; exit 2
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ScaleNative {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern IntPtr GetThreadDesktop(uint t);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetUserObjectInformation(IntPtr h,int i,StringBuilder b,uint n,out uint need);
  [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h,ref POINT p);
  [DllImport("user32.dll")] public static extern IntPtr ChildWindowFromPointEx(IntPtr h,POINT p,uint f);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
}
'@
# Coordinate conversion must use the physical pixels returned by UIA. This
# affects only this helper's thread; it does not set a display scale or prove it.
# Scale proof below is read from the existing Settings window, never this thread.
if ([ScaleNative]::SetThreadDpiAwarenessContext([IntPtr](-4)) -eq [IntPtr]::Zero) { exit 2 }
$session = (Get-Process -Id $PID).SessionId
$settings = @(Get-Process -Name SystemSettings -ErrorAction SilentlyContinue | Where-Object SessionId -eq $session)
if ($settings.Count -ne 1) { exit 2 }
$settingsId = $settings[0].Id
$settingsStart = $settings[0].StartTime.ToUniversalTime().Ticks
$frames = @(Get-Process -Name ApplicationFrameHost -ErrorAction SilentlyContinue | Where-Object SessionId -eq $session)
$allowed = @($settingsId) + @($frames | ForEach-Object Id)
$processStarts = @{}
foreach ($process in (@($settings[0]) + $frames)) { $processStarts[$process.Id] = $process.StartTime.ToUniversalTime().Ticks }
$walker = [Windows.Automation.TreeWalker]::ControlViewWalker
$desktop = [Windows.Automation.AutomationElement]::RootElement
$selectorId = 'SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox'

function Read-Controls {
    $live = Get-Process -Id $settingsId
    if ($live.SessionId -ne $session -or $live.StartTime.ToUniversalTime().Ticks -ne $settingsStart) {
        throw 'Settings identity changed.'
    }
    $tops = [Collections.Generic.List[object]]::new()
    foreach ($processId in $allowed) {
        $process = Get-Process -Id $processId
        if ($process.SessionId -ne $session -or $process.StartTime.ToUniversalTime().Ticks -ne $processStarts[$processId]) {
            throw 'Settings host identity changed.'
        }
        $condition = [Windows.Automation.PropertyCondition]::new(
            [Windows.Automation.AutomationElement]::ProcessIdProperty,[int]$processId)
        foreach ($top in $desktop.FindAll([Windows.Automation.TreeScope]::Children,$condition)) { $tops.Add($top) }
    }
    $queue = [Collections.Generic.Queue[object]]::new()
    foreach ($top in $tops) { $queue.Enqueue(@{element=$top; top=$top}) }
    $rows = [Collections.Generic.List[object]]::new()
    $count = 0
    while ($queue.Count -gt 0) {
        if (++$count -gt 1000) { throw 'Settings traversal limit.' }
        $entry = $queue.Dequeue()
        $element = $entry.element
        if ($element.Current.ProcessId -eq $settingsId) { $rows.Add($entry) }
        $child = $walker.GetFirstChild($element)
        while ($null -ne $child) {
            if (($queue.Count + $count) -ge 1000) { throw 'Settings traversal limit.' }
            $queue.Enqueue(@{element=$child; top=$entry.top})
            $child = $walker.GetNextSibling($child)
        }
    }
    return $rows.ToArray()
}
function Read-Scale($Rows) {
    $combos = @($Rows | Where-Object {
        $_.element.Current.AutomationId -ceq $selectorId -and
        $_.element.Current.ControlType -eq [Windows.Automation.ControlType]::ComboBox -and
        $_.element.Current.IsEnabled -and -not $_.element.Current.IsOffscreen
    })
    if ($combos.Count -ne 1) { throw 'Unique enabled scale selector unavailable.' }
    $pattern = $combos[0].element.GetCurrentPattern([Windows.Automation.SelectionPattern]::Pattern)
    $selected = @($pattern.Current.GetSelection())
    if ($selected.Count -ne 1) { throw 'Unique scale selection unavailable.' }
    $name = $selected[0].Current.Name
    if ($name -cnotmatch '^(100|125|150|200)%( \(Recommended\))?$') { throw 'Unsupported scale selection.' }
    return @{percent=[int]$Matches[1]; combo=$combos[0]
        dpi=[ScaleNative]::GetDpiForWindow([IntPtr]$combos[0].top.Current.NativeWindowHandle)}
}
function Desktop-Name([uint32] $Thread) {
    $buffer = [Text.StringBuilder]::new(256)
    [uint32]$needed = 0
    if (-not [ScaleNative]::GetUserObjectInformation([ScaleNative]::GetThreadDesktop($Thread),2,$buffer,512,[ref]$needed)) {
        throw 'Native desktop unavailable.'
    }
    return $buffer.ToString()
}
function Click-Control($Entry) {
    $started = [DateTime]::UtcNow
    $current = $Entry.element.Current
    if ($current.ProcessId -ne $settingsId -or -not $current.IsEnabled -or $current.IsOffscreen) { throw 'Input control changed.' }
    $rect = $current.BoundingRectangle
    if ($rect.IsEmpty -or $rect.Width -le 0 -or $rect.Height -le 0) { throw 'Input bounds unavailable.' }
    $hwnd = [IntPtr]$Entry.top.Current.NativeWindowHandle
    [uint32]$nativePid = 0
    $thread = [ScaleNative]::GetWindowThreadProcessId($hwnd,[ref]$nativePid)
    if ($allowed -notcontains [int]$nativePid -or
        (Desktop-Name $thread) -cne (Desktop-Name ([ScaleNative]::GetCurrentThreadId()))) { throw 'Input desktop identity unavailable.' }
    $x = [int][Math]::Floor($rect.X + $rect.Width / 2)
    $y = [int][Math]::Floor($rect.Y + $rect.Height / 2)
    for ($i=0; $i -lt 32; $i++) {
        $point = [ScaleNative+POINT]::new(); $point.X=$x; $point.Y=$y
        if (-not [ScaleNative]::ScreenToClient($hwnd,[ref]$point)) { throw 'Input coordinate conversion unavailable.' }
        $child = [ScaleNative]::ChildWindowFromPointEx($hwnd,$point,7)
        if ($child -eq [IntPtr]::Zero -or $child -eq $hwnd) { break }
        $hwnd = $child
    }
    [void][ScaleNative]::GetWindowThreadProcessId($hwnd,[ref]$nativePid)
    if ($allowed -notcontains [int]$nativePid -or -not [ScaleNative]::IsWindowVisible($hwnd) -or
        -not [ScaleNative]::IsWindowEnabled($hwnd)) { throw 'Input native child unavailable.' }
    $bounds = [ScaleNative+RECT]::new()
    if (-not [ScaleNative]::GetWindowRect($hwnd,[ref]$bounds) -or
        $x -lt $bounds.Left -or $x -ge $bounds.Right -or $y -lt $bounds.Top -or $y -ge $bounds.Bottom) {
        throw 'Input point left native child bounds.'
    }
    $point = [ScaleNative+POINT]::new(); $point.X=$x; $point.Y=$y
    if (-not [ScaleNative]::ScreenToClient($hwnd,[ref]$point)) { throw 'Input coordinate conversion unavailable.' }
    $fresh = $Entry.element.Current
    if (-not $fresh.IsEnabled -or $fresh.IsOffscreen -or $fresh.ProcessId -ne $settingsId -or
        -not $fresh.BoundingRectangle.Equals($rect) -or ([DateTime]::UtcNow - $started).TotalMilliseconds -gt 500) {
        throw 'Input observation expired.'
    }
    $result = Invoke-BoundedProcess $CheapExecutable @('mouse_click','--hwnd',"$($hwnd.ToInt64())",
        '--x',"$($point.X)",'--y',"$($point.Y)",'--button','left') 10
    if (-not $result.terminated -or $result.code -ne 0) { throw 'Cheap input unavailable.' }
    $reply = $result.stdout | ConvertFrom-Json
    if ($reply.ok -ne $true) { throw 'Cheap input rejected.' }
}
function Set-Scale([int] $Percent) {
    $rows = @(Read-Controls)
    $state = Read-Scale $rows
    if ($state.percent -ne $Percent) {
        Click-Control $state.combo
        Start-Sleep -Milliseconds 250
        $rows = @(Read-Controls)
        $state = Read-Scale $rows
        $options = @($rows | Where-Object {
            $_.element.Current.ControlType -eq [Windows.Automation.ControlType]::ListItem -and
            $_.element.Current.Name -cmatch ('^' + $Percent + '%( \(Recommended\))?$') -and
            $_.element.Current.IsEnabled -and -not $_.element.Current.IsOffscreen
        })
        if ($options.Count -ne 1) { throw 'Unique predefined scale option unavailable.' }
        # Pattern access is read-only, never Select, Invoke, Expand or SetValue.
        $itemPattern = $options[0].element.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern)
        if (-not $itemPattern.Current.SelectionContainer.Equals($state.combo.element)) {
            throw 'Scale option belongs to another control.'
        }
        Click-Control $options[0]
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 300
        $state = Read-Scale @(Read-Controls)
        if ($state.percent -eq $Percent -and $state.dpi -eq (96 * $Percent / 100)) { return $state }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Selected scale and native DPI did not converge.'
}

$receipt = @{schema=1; requested_scale=$ScalePercent; status='unavailable'; restored=$false
    input_method='cheap_mouse_click'; uia='read_only'; action='not_started'
    target_application_dpi='requires_independent_runtime_measurement'}
$original = $null
try {
    if ($Mode -eq 'restore') {
        $saved = Get-Content -LiteralPath $originalPath -Raw | ConvertFrom-Json
        if ($saved.settings_pid -ne $settingsId -or $saved.settings_start -ne $settingsStart -or
            $saved.session -ne $session -or $saved.scale -notin @(100,125,150,200)) { throw 'Restoration identity changed.' }
        $original = $saved
    } else {
        $before = Read-Scale @(Read-Controls)
        if ($before.dpi -ne (96 * $before.percent / 100)) { throw 'Original Settings DPI does not match its selection.' }
        $original = @{scale=$before.percent; dpi=$before.dpi; settings_pid=$settingsId; settings_start=$settingsStart; session=$session}
        # This is private recovery state, excluded from public upload. Publish
        # atomically before any input so the supervisor can restore after timeout.
        $original | ConvertTo-Json | Set-Content -LiteralPath ($originalPath + '.tmp') -Encoding utf8
        Move-Item -LiteralPath ($originalPath + '.tmp') -Destination $originalPath
        $selected = Set-Scale $ScalePercent
        $receipt.selected_scale = $selected.percent
        $receipt.measured_settings_dpi = $selected.dpi
        if ($ActionScript) {
            $action = Invoke-BoundedProcess $pwsh @('-NoProfile','-File',[IO.Path]::GetFullPath($ActionScript)) $ActionTimeoutSeconds
            $receipt.action = if ($action.terminated -and $action.code -eq 0) {'succeeded'} else {'failed'}
            if (-not $action.terminated -or $action.code -ne 0) { throw 'Bounded action failed.' }
        } else { $receipt.action = 'standalone_measurement_only' }
        $receipt.status = 'selected_and_measured'
    }
} catch {
    $receipt.status = 'unavailable'
} finally {
    if ($null -ne $original) {
        try {
            $restored = Set-Scale ([int]$original.scale)
            $receipt.restored = $restored.percent -eq $original.scale -and $restored.dpi -eq $original.dpi
            $receipt.original_scale = $original.scale
            $receipt.restored_settings_dpi = $restored.dpi
        } catch { $receipt.restored = $false }
    }
    $receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output ($Mode + '.json')) -Encoding utf8
}
if ($receipt.restored -and ($Mode -eq 'restore' -or $receipt.status -eq 'selected_and_measured')) { exit 0 }
exit 2
