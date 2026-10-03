[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet(100,125,150,200)][int] $ScalePercent,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][string] $CheapExecutable,
    [ValidateSet('background','hosted-foreground')][string] $InputRoute = 'background',
    [switch] $NativeRuntime,
    [switch] $ProvisionResolution,
    [ValidateSet('1920x1080','1600x1200')][string] $ResolutionMode = '1920x1080',
    [switch] $DiagnosticEvidence,
    [switch] $RefreshSettingsPage,
    [ValidateSet('supervisor','run','restore')][string] $Mode = 'supervisor'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
    $env:RUNNER_OS -cne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'Disposable hosted Windows execution is required.'
}
if ($ProvisionResolution -and $NativeRuntime -and $ScalePercent -notin @(100,200)) {
    throw 'Combined resolution and native execution requires the baseline minimum tuple.'
}
if ($ProvisionResolution -and $InputRoute -cne 'hosted-foreground') { throw 'Resolution provisioning requires the disposable foreground route.' }
if ($DiagnosticEvidence -and ($NativeRuntime -or $InputRoute -cne 'hosted-foreground')) {
    throw 'Diagnostic evidence is limited to standalone disposable foreground discovery.'
}
$tempRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be within RUNNER_TEMP.'
}
if (-not (Test-Path -LiteralPath $CheapExecutable -PathType Leaf)) { throw 'Cheap input executable unavailable.' }
$pwsh = (Get-Process -Id $PID).Path
$originalPath = Join-Path $output 'original.json'
. (Join-Path $PSScriptRoot 'Invoke-HostedDisplayScaleNavigation.ps1')
$nativeObservation = $NativeRuntime -and $ProvisionResolution -and $ScalePercent -eq 200 -and $ResolutionMode -ceq '1600x1200' -and $RefreshSettingsPage -and -not $DiagnosticEvidence -and $InputRoute -ceq 'hosted-foreground'
if (-not $nativeObservation -and -not (Test-PageRefreshScope $RefreshSettingsPage $NativeRuntime $ProvisionResolution $DiagnosticEvidence $InputRoute)) {
    throw 'Page refresh requires standalone foreground resolution diagnostics.'
}
$script:NavigationObservation = @{requested=[bool]$RefreshSettingsPage; colors_acknowledged=$false; display_acknowledged=$false}
function Test-ResolutionDiagnosticScope([string] $Resolution, [bool] $Native, [bool] $Provision,
    [bool] $Diagnostic, [bool] $Refresh, [string] $Route) {
    return $Resolution -ceq '1920x1080' -or ($Resolution -ceq '1600x1200' -and -not $Native -and
        $Provision -and $Diagnostic -and $Refresh -and $Route -ceq 'hosted-foreground')
}
if (-not $nativeObservation -and -not (Test-ResolutionDiagnosticScope $ResolutionMode $NativeRuntime $ProvisionResolution $DiagnosticEvidence $RefreshSettingsPage $InputRoute)) {
    throw 'Alternate resolution requires standalone foreground page diagnostics.'
}
$nativeRequestPath = Join-Path $env:RUNNER_TEMP ('native-scale-request-' + $env:GITHUB_RUN_ID + '.json')
$nativeReceiptPath = Join-Path $env:RUNNER_TEMP ('native-scale-adapter-' + $env:GITHUB_RUN_ID + '.json')
$nativeAdapter = Join-Path $PSScriptRoot 'run-scaled-native-interface.py'
$nativePython = Join-Path $env:RUNNER_TEMP ('automation-python-' + $env:GITHUB_RUN_ID + '/Scripts/python.exe')
Add-Type -Path (Join-Path $PSScriptRoot 'HostedScaleProcess.cs')
if ($ProvisionResolution) { Add-Type -Path (Join-Path $PSScriptRoot 'HostedDisplayMode.cs') }
$script:ResolutionState = $null
$script:ChildTerminationUncertain = $false

function Test-UncertainChildren {
    return $script:ChildTerminationUncertain -or
        @(Get-ChildItem -LiteralPath $output -File | Where-Object Name -Match '^child-(run|restore)-.*\.pending$').Count -gt 0
}

function Test-UncertainInput {
    # This is independent of Job termination. An interrupted drag can leave a
    # held button or a different input desktop after every child has exited.
    $started = Join-Path $output 'native-input.started'
    if (-not (Test-Path -LiteralPath $started)) { return $false }
    try {
        $requestFile = Get-Item -LiteralPath $nativeRequestPath
        if ($requestFile.Length -le 0 -or $requestFile.Length -gt 8192 -or
            ($requestFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $true }
        $hash = (Get-FileHash -LiteralPath $nativeRequestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        foreach ($path in @($started,(Join-Path $output 'native-input.restored'))) {
            $file = Get-Item -LiteralPath $path
            if ($file.Length -ne 64 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                [IO.File]::ReadAllText($path) -cne $hash) { return $true }
        }
        return $false
    } catch { return $true }
}

# Each child starts suspended, enters a non-breakaway kill-on-close job, then
# runs. Only a zero active-process count proves the complete tree has stopped.
# Non-JSON streams go to NUL. Cheap JSON is capped at 64 KiB with a bounded drain.
function Invoke-BoundedProcess([string] $Executable, [string[]] $Arguments, [int] $Seconds, [bool] $Capture = $false, [string] $JobName = '') {
    if (Test-UncertainChildren) { throw 'Child termination is unverified.' }
    $pending = Join-Path $output ('child-' + $Mode + '-' + [Guid]::NewGuid().ToString('N') + '.pending')
    # Durable state must precede creation. An interrupted worker leaves this
    # marker, which blocks later input and recovery instead of assuming exit.
    [IO.File]::WriteAllText($pending, 'pending')
    try {
        if ($JobName) {
            $result = [HostedScaleProcess]::RunNamed([IO.Path]::GetFullPath($Executable), $Arguments, $Seconds, $Capture, $JobName)
        } else {
            $result = [HostedScaleProcess]::Run([IO.Path]::GetFullPath($Executable), $Arguments, $Seconds, $Capture)
        }
        if ($result.Terminated) {
            # Rename preserves the durable termination proof and clears pending
            # atomically. This contains no child output, labels, or arguments.
            Move-Item -LiteralPath $pending -Destination ($pending + '.stopped')
        } else {
            $script:ChildTerminationUncertain = $true
        }
        return @{ terminated=$result.Terminated; code=$result.Code; stdout=$result.Output
            process_stage=[int]$result.ProcessStage; native_error=$result.NativeError }
    } catch {
        $script:ChildTerminationUncertain = $true
        throw 'Child containment or receipt is unverified.'
    }
}

function Test-NativeTuple($Request, [int] $Percent, [bool] $Resolution) {
    if ($Resolution -and $Request.scope -ceq 'minimum-observe') {
        return $Request.resolution -ceq '1600x1200' -and $Request.viewport -ceq 'measured-minimum' -and $Percent -eq 200 -and $Request.language -ceq 'en' -and $Request.theme -ceq 'light' -and $Request.refresh_page -ceq 'acknowledged-roundtrip'
    }
    if ($Resolution) {
        return $Request.resolution -ceq '1920x1080' -and $Request.scope -ceq 'minimum-resize' -and
            $Request.viewport -ceq 'measured-minimum' -and $Percent -eq 100
    }
    return $Request.resolution -ceq 'unchanged' -and $Request.scope -notin @('minimum-resize','minimum-observe') -and
        $Percent -in @(125,150,200)
}

function Read-NativeRequest {
    # The fixed adapter performs the complete strict schema, duplicate, path and
    # hash validation before any Settings input. These fields are only used to
    # invoke that validator and must never select an executable or script.
    $file = Get-Item -LiteralPath $nativeRequestPath
    if ($file.Length -le 0 -or $file.Length -gt 8192 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Native request unavailable.'
    }
    $request = Get-Content -LiteralPath $nativeRequestPath -Raw | ConvertFrom-Json
    if ($request.job_name -cnotmatch '^Local\\BambuNativeScale-[0-9a-f]{64}$' -or
        $request.scale_percent -ne $ScalePercent -or $request.run_id -cne $env:GITHUB_RUN_ID -or
        ($ScalePercent -eq 100 -and -not $ProvisionResolution) -or $InputRoute -cne 'hosted-foreground' -or
        $output -ine [IO.Path]::GetFullPath((Join-Path $env:RUNNER_TEMP ('native-scale-' + $env:GITHUB_RUN_ID))) -or
        [IO.Path]::GetFullPath($CheapExecutable) -ine [IO.Path]::GetFullPath((Join-Path (Split-Path $nativePython) 'lowlevel-computer-use-cheap.exe'))) {
        throw 'Native request binding unavailable.'
    }
    if (-not (Test-NativeTuple $request $ScalePercent ([bool]$ProvisionResolution))) {
        throw 'Native resolution tuple binding unavailable.'
    }
    if (($request.refresh_page -ceq 'acknowledged-roundtrip') -ne [bool]$RefreshSettingsPage -or ($ProvisionResolution -and $request.resolution -cne $ResolutionMode)) { throw 'Native display route binding unavailable.' }
    return $request
}
function Read-ResolutionRecoveryState {
    $file = Get-Item -LiteralPath $originalPath
    if ($file.Length -le 0 -or $file.Length -gt 16384 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Resolution recovery state unavailable.'
    }
    $saved = Get-Content -LiteralPath $originalPath -Raw | ConvertFrom-Json
    if ($saved.scale -notin @(100,125,150,200) -or $saved.dpi -ne (96 * $saved.scale / 100)) {
        throw 'Original scale recovery state unavailable.'
    }
    return [HostedDisplayMode]::Recover($saved.resolution.device,$saved.resolution.identity,
        [Convert]::FromBase64String($saved.resolution.mode))
}

if ($Mode -eq 'supervisor') {
    if (Test-Path -LiteralPath $output) { throw 'Output directory must be new.' }
    [void](New-Item -ItemType Directory -Path $output)
    $run = @{ terminated = $false; code = -1 }
    $recovery = @{ terminated = $true; code = 0 }
    try {
        if ($NativeRuntime) {
            $nativeRequest = Read-NativeRequest
            $run = Invoke-BoundedProcess $nativePython @($nativeAdapter,'--validate-request',$nativeRequest.job_name) 20
            if (-not $run.terminated -or $run.code -ne 0) { throw 'Native request validation failed.' }
            # Open only the fixed owned Settings surface after installation,
            # bootstrap and strict request validation. Never force activation;
            # the worker still requires observed foreground ownership.
            $run = @{ terminated = $true; code = -1 }
            Start-Process -FilePath 'ms-settings:display' -WindowStyle Hidden
        }
        $run = @{ terminated = $false; code = -1 }
        $arguments = @('-NoProfile','-File',$PSCommandPath,'-ScalePercent',"$ScalePercent",
            '-OutputDirectory',$output,'-CheapExecutable',$CheapExecutable,'-InputRoute',$InputRoute,'-Mode','run',
            '-ResolutionMode',$ResolutionMode)
        $seconds = 120
        if ($NativeRuntime) { $arguments += '-NativeRuntime'; $seconds = 1920 }
        if ($ProvisionResolution) { $arguments += '-ProvisionResolution'; if (-not $NativeRuntime) { $seconds = 180 } }
        if ($DiagnosticEvidence) { $arguments += '-DiagnosticEvidence' }
        if ($RefreshSettingsPage) { $arguments += '-RefreshSettingsPage' }
        $run = Invoke-BoundedProcess $pwsh $arguments $seconds
    } catch {} finally {
        # The durable original state exists before the first input. Recovery is
        # isolated too, and never races an unterminated first worker.
        if ($run.terminated -and -not (Test-UncertainChildren) -and -not (Test-UncertainInput) -and (Test-Path -LiteralPath $originalPath)) {
            try {
                $recoveryArguments = @('-NoProfile','-File',$PSCommandPath,
                    '-ScalePercent',"$ScalePercent",'-OutputDirectory',$output,
                    '-CheapExecutable',$CheapExecutable,'-InputRoute',$InputRoute,'-Mode','restore')
                $recoverySeconds = 60
                if ($ProvisionResolution) { $recoveryArguments += '-ProvisionResolution'; $recoverySeconds = 90 }
                $recovery = Invoke-BoundedProcess $pwsh $recoveryArguments $recoverySeconds
            } catch { $recovery = @{ terminated = $false; code = -1 } }
        } elseif (-not $run.terminated -or (Test-UncertainChildren) -or (Test-UncertainInput)) {
            $recovery = @{ terminated = $false; code = -1 }
        }
        $uncertain = Test-UncertainChildren
        $inputUncertain = Test-UncertainInput
        $navigationUncertain = Test-UncertainNavigation
        $restored = $recovery.terminated -and $recovery.code -eq 0 -and -not $uncertain -and -not $inputUncertain -and -not $navigationUncertain
        $success = $run.terminated -and $run.code -eq 0 -and $restored
        @{schema=1; status=$(if ($success) {'verified_settings_scale_and_restoration'} else {'unavailable'})
          requested_scale=$ScalePercent; worker_termination_verified=$run.terminated
          input_route=$InputRoute; foreground_input_atomic=$false
          native_runtime_requested=[bool]$NativeRuntime
          resolution_provisioning_requested=[bool]$ProvisionResolution
          requested_resolution=$(if ($ProvisionResolution) {$ResolutionMode} else {'unchanged'})
          recovery_termination_verified=$recovery.terminated; restoration_verified=$restored
          child_termination_uncertain=$uncertain; process_containment='suspended_start_nonbreakaway_job'
          input_recovery_uncertain=$inputUncertain
          navigation_recovery_uncertain=$navigationUncertain; page_refresh_requested=[bool]$RefreshSettingsPage
          target_application_dpi='requires_independent_runtime_measurement'
          disposal_required=(-not $run.terminated -or -not $restored -or $uncertain -or $inputUncertain -or $navigationUncertain)
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'supervisor.json') -Encoding utf8
    }
    if ($success) { exit 0 }; exit 2
}

if ($Mode -eq 'restore' -and (Test-UncertainInput)) {
    # Do not initialize UIA, change modes, or send Settings input when a native
    # desktop handoff or button release has not been proved restored.
    @{schema=1; status='unavailable'; restored=$false; disposal_required=$true
      input_recovery_uncertain=$true; failure_stage='native_input_recovery_unverified'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'restore.json') -Encoding utf8
    exit 2
}

if ($ProvisionResolution -and $Mode -eq 'restore') {
    # Recover mode independently before UIA initialization. Missing Settings or
    # a UIA exception cannot skip an otherwise safe identity-bound mode restore.
    # Apply first observes current mode, making interrupted recovery idempotent.
    $earlyModeRestored = $false
    try {
        if (Test-UncertainChildren) { throw 'Uncertain child blocks mode recovery.' }
        $script:ResolutionState = Read-ResolutionRecoveryState
        $earlyModeRestored = [HostedDisplayMode]::Apply($script:ResolutionState,$script:ResolutionState.Original).Verified
    } catch {}
    @{schema=1; status='unavailable'; restored=$false; resolution_restored=$earlyModeRestored
      disposal_required=$true; failure_stage='scale_recovery_pending'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'restore.json') -Encoding utf8
}

if ($Mode -eq 'restore' -and (Test-UncertainNavigation)) {
    # Broker navigation remains unknown even after every launcher has exited.
    # Independent mode recovery above is safe; no further URI or scale input is.
    @{schema=1; status='unavailable'; restored=$false; disposal_required=$true
      resolution_restored=([bool]$ProvisionResolution -and $earlyModeRestored)
      navigation_recovery_uncertain=$true; failure_stage='navigation_return_unverified'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'restore.json') -Encoding utf8
    exit 2
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
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h,uint flags);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
  [DllImport("user32.dll")] public static extern bool CloseDesktop(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetProcessWindowStation();
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
$script:Stage = 'read_original'
$script:Observation = @{}
$script:DiagnosticResults = @{}
if ($DiagnosticEvidence) { . (Join-Path $PSScriptRoot 'Save-HostedScaleDiagnostic.ps1') }

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
function Object-Name([IntPtr] $Handle) {
    $buffer = [Text.StringBuilder]::new(256)
    [uint32]$needed = 0
    if ($Handle -eq [IntPtr]::Zero -or
        -not [ScaleNative]::GetUserObjectInformation($Handle,2,$buffer,512,[ref]$needed)) {
        throw 'Interactive desktop identity unavailable.'
    }
    return $buffer.ToString()
}
function Assert-HostedForeground([IntPtr] $Root, [int] $X, [int] $Y) {
    # This opt-in route is scoped to a wholly disposable hosted machine. The
    # separate CLI cannot atomically bind foreground input to an HWND. These
    # checks detect changed ownership; they do not remove that scheduling race.
    if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
        $env:RUNNER_OS -cne 'Windows' -or $InputRoute -cne 'hosted-foreground') {
        throw 'Disposable foreground input unavailable.'
    }
    if ((Object-Name ([ScaleNative]::GetProcessWindowStation())) -cne 'WinSta0' -or
        (Desktop-Name ([ScaleNative]::GetCurrentThreadId())) -cne 'Default') {
        throw 'Owned interactive desktop unavailable.'
    }
    $inputDesktop = [ScaleNative]::OpenInputDesktop(0,$false,1)
    try {
        if ((Object-Name $inputDesktop) -cne 'Default') { throw 'Input desktop changed.' }
    } finally { if ($inputDesktop -ne [IntPtr]::Zero) { [void][ScaleNative]::CloseDesktop($inputDesktop) } }
    foreach ($processId in $allowed) {
        $live = Get-Process -Id $processId
        if ($live.SessionId -ne $session -or $live.StartTime.ToUniversalTime().Ticks -ne $processStarts[$processId]) {
            throw 'Foreground owner identity changed.'
        }
    }
    if ($Root -eq [IntPtr]::Zero -or [ScaleNative]::GetForegroundWindow() -ne $Root) {
        throw 'Settings is not the foreground target.'
    }
    [uint32]$rootPid = 0
    [void][ScaleNative]::GetWindowThreadProcessId($Root,[ref]$rootPid)
    if ($allowed -notcontains [int]$rootPid) { throw 'Foreground root owner changed.' }
    $point = [ScaleNative+POINT]::new(); $point.X=$X; $point.Y=$Y
    $hit = [ScaleNative]::WindowFromPoint($point)
    [uint32]$hitPid = 0
    $thread = [ScaleNative]::GetWindowThreadProcessId($hit,[ref]$hitPid)
    if ($hit -eq [IntPtr]::Zero -or [ScaleNative]::GetAncestor($hit,2) -ne $Root -or
        $allowed -notcontains [int]$hitPid -or (Desktop-Name $thread) -cne 'Default') {
        throw 'Foreground input point is obscured or changed.'
    }
}
function Observe-ForegroundAfterInput([IntPtr] $Root) {
    # A successful scale change can move the clicked option and the entire
    # Settings layout. Reusing that old coordinate would test obsolete geometry.
    # Observe only: no activation, input, or change to the original owned root.
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        try {
            $started = [DateTime]::UtcNow
            $state = Read-Scale @(Read-Controls)
            $current = $state.combo.element.Current
            $rect = $current.BoundingRectangle
            $currentRoot = [ScaleNative]::GetAncestor([IntPtr]$state.combo.top.Current.NativeWindowHandle,2)
            if ($currentRoot -ne $Root -or $current.ProcessId -ne $settingsId -or
                -not $current.IsEnabled -or $current.IsOffscreen -or $rect.IsEmpty -or
                $rect.Width -le 0 -or $rect.Height -le 0) { throw 'Post-input Settings geometry unavailable.' }
            $x = [int][Math]::Floor($rect.X + $rect.Width / 2)
            $y = [int][Math]::Floor($rect.Y + $rect.Height / 2)
            Assert-HostedForeground $Root $x $y
            $fresh = $state.combo.element.Current
            if ($fresh.ProcessId -ne $settingsId -or -not $fresh.IsEnabled -or $fresh.IsOffscreen -or
                -not $fresh.BoundingRectangle.Equals($rect) -or ([DateTime]::UtcNow - $started).TotalMilliseconds -gt 500) {
                throw 'Post-input Settings observation expired.'
            }
            $script:Observation.post_input_fresh_combo_owned = $true
            return
        } catch {
            $script:Observation.post_input_fresh_combo_owned = $false
            Start-Sleep -Milliseconds 150
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Post-input foreground ownership did not converge.'
}
function Observe-AllowedOptionDomain($Rows, $Combo) {
    $domain = @{}
    foreach ($percent in @(100,125,150,200)) {
        $counts = @{observed=0; visible_enabled=0; matching_container=0; container_unavailable=0}
        foreach ($row in $Rows) {
            $current = $row.element.Current
            if ($current.ControlType -ne [Windows.Automation.ControlType]::ListItem -or
                $current.Name -cnotmatch ('^' + $percent + '%( \(Recommended\))?$')) { continue }
            $counts.observed++
            if ($current.IsEnabled -and -not $current.IsOffscreen) { $counts.visible_enabled++ }
            try {
                $item = $row.element.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern)
                if ($item.Current.SelectionContainer.Equals($Combo.element)) { $counts.matching_container++ }
            } catch { $counts.container_unavailable++ }
        }
        $domain["scale_$percent"] = $counts
    }
    # This records only predefined numeric-domain counts, never raw labels or
    # identities. An absent virtualized item remains unavailable, not unsupported.
    $script:Observation.allowed_option_domain = $domain
}
function Click-Control($Entry) {
    $script:Stage = 'validate_input'
    if ((Test-UncertainChildren) -or (Test-UncertainInput) -or (Test-UncertainNavigation)) { throw 'Input blocked by unverified child, input or navigation recovery.' }
    if ($ProvisionResolution) { [HostedDisplayMode]::AssertBinding($script:ResolutionState) }
    $started = [DateTime]::UtcNow
    $current = $Entry.element.Current
    if ($current.ProcessId -ne $settingsId -or -not $current.IsEnabled -or $current.IsOffscreen) { throw 'Input control changed.' }
    $rect = $current.BoundingRectangle
    if ($rect.IsEmpty -or $rect.Width -le 0 -or $rect.Height -le 0) { throw 'Input bounds unavailable.' }
    $hwnd = [IntPtr]$Entry.top.Current.NativeWindowHandle
    $root = [ScaleNative]::GetAncestor($hwnd,2)
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
    if ($InputRoute -eq 'hosted-foreground') {
        $script:Stage = 'validate_foreground'
        Assert-HostedForeground $root $x $y
        if ($ProvisionResolution) { [HostedDisplayMode]::AssertBinding($script:ResolutionState) }
        $fresh = $Entry.element.Current
        if (-not $fresh.IsEnabled -or $fresh.IsOffscreen -or $fresh.ProcessId -ne $settingsId -or
            -not $fresh.BoundingRectangle.Equals($rect) -or ([DateTime]::UtcNow - $started).TotalMilliseconds -gt 500) {
            throw 'Foreground input observation expired.'
        }
    }
    $script:Stage = 'cheap_spawn'
    if ($InputRoute -eq 'hosted-foreground') {
        $result = Invoke-BoundedProcess $CheapExecutable @('mouse_click','--x',"$x",'--y',"$y",
            '--button','left','--instant_move','true','--confirm_focus_disruption','true') 10 $true
    } else {
        $result = Invoke-BoundedProcess $CheapExecutable @('mouse_click','--hwnd',"$($hwnd.ToInt64())",
            '--x',"$($point.X)",'--y',"$($point.Y)",'--button','left') 10 $true
    }
    $script:Stage = 'cheap_result'
    $script:Observation.cheap_terminated = [bool]$result.terminated
    $script:Observation.cheap_exit = [int]$result.code
    if (-not $result.terminated -or $result.code -ne 0) { throw 'Cheap input unavailable.' }
    $reply = $result.stdout | ConvertFrom-Json
    $script:Observation.cheap_ok = $reply.ok -eq $true
    if ($reply.ok -ne $true) { throw 'Cheap input rejected.' }
    if ($InputRoute -eq 'hosted-foreground') {
        $script:Stage = 'observe_foreground_after_input'
        Observe-ForegroundAfterInput $root
    }
}
function Set-Scale([int] $Percent) {
    if ((Test-UncertainChildren) -or (Test-UncertainInput) -or (Test-UncertainNavigation)) { throw 'Scale change blocked by unverified child, input or navigation recovery.' }
    if ($ProvisionResolution) { [HostedDisplayMode]::AssertBinding($script:ResolutionState) }
    $script:Stage = 'resolve_combo'
    $rows = @(Read-Controls)
    $state = Read-Scale $rows
    if ($state.percent -ne $Percent) {
        if ($DiagnosticEvidence -and $Mode -eq 'run') {
            Save-HostedScaleDiagnostic 'before_selector'
            # Diagnostic capture can take time. Resolve fresh input geometry after it.
            $state = Read-Scale @(Read-Controls)
        }
        $script:Observation.input_target = 'scale_selector'
        Click-Control $state.combo
        $script:Stage = 'observe_expanded'
        $expanded = $false
        $expandDeadline = [DateTime]::UtcNow.AddSeconds(3)
        do {
            Start-Sleep -Milliseconds 150
            $state = Read-Scale @(Read-Controls)
            # Observation only. Input remains the HWND-targeted cheap route.
            $expansion = $state.combo.element.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern)
            $expansionState = $expansion.Current.ExpandCollapseState
            $script:Observation.expansion_state = [int]$expansionState
            $expanded = $expansionState -eq [Windows.Automation.ExpandCollapseState]::Expanded
        } while (-not $expanded -and [DateTime]::UtcNow -lt $expandDeadline)
        $script:Observation.expansion_observed = $expanded
        if (-not $expanded) { throw 'Scale selector expansion was not observed.' }
        if ($DiagnosticEvidence -and $Mode -eq 'run') { Save-HostedScaleDiagnostic 'expanded_selector' }
        $script:Stage = 'match_option'
        $rows = @(Read-Controls)
        $state = Read-Scale $rows
        Observe-AllowedOptionDomain $rows $state.combo
        $options = @($rows | Where-Object {
            $_.element.Current.ControlType -eq [Windows.Automation.ControlType]::ListItem -and
            $_.element.Current.Name -cmatch ('^' + $Percent + '%( \(Recommended\))?$') -and
            $_.element.Current.IsEnabled -and -not $_.element.Current.IsOffscreen
        })
        $script:Observation.matching_option_count = $options.Count
        if ($options.Count -ne 1) { throw 'Unique predefined scale option unavailable.' }
        $script:Stage = 'validate_container'
        # Pattern access is read-only, never Select, Invoke, Expand or SetValue.
        $itemPattern = $options[0].element.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern)
        if (-not $itemPattern.Current.SelectionContainer.Equals($state.combo.element)) {
            throw 'Scale option belongs to another control.'
        }
        $script:Stage = 'click_option'
        $script:Observation.input_target = 'scale_option'
        Click-Control $options[0]
    }
    $script:Stage = 'converge_dpi'
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 300
        $state = Read-Scale @(Read-Controls)
        if ($state.percent -eq $Percent -and $state.dpi -eq (96 * $Percent / 100)) { return $state }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Selected scale and native DPI did not converge.'
}

$receipt = @{schema=1; requested_scale=$ScalePercent; status='unavailable'; restored=$false
    requested_resolution=$(if ($Mode -eq 'restore') {'original'} elseif ($ProvisionResolution) {$ResolutionMode} else {'unchanged'})
    input_method='cheap_mouse_click'; input_route=$InputRoute; foreground_input_atomic=$false
    isolation='disposable_hosted_machine'; uia='read_only'; action='unsupported_standalone_only'
    target_application_dpi='requires_independent_runtime_measurement'
    failure_stage=$null; restoration_failure_stage=$null}
$original = $null
try {
    if ($Mode -eq 'restore') {
        $saved = Get-Content -LiteralPath $originalPath -Raw | ConvertFrom-Json
        if ($saved.settings_pid -ne $settingsId -or $saved.settings_start -ne $settingsStart -or
            $saved.session -ne $session -or $saved.scale -notin @(100,125,150,200)) { throw 'Restoration identity changed.' }
        $original = $saved
        if ($ProvisionResolution -and $null -eq $script:ResolutionState) {
            $script:ResolutionState = Read-ResolutionRecoveryState
        }
    } else {
        $before = Read-Scale @(Read-Controls)
        if ($RefreshSettingsPage) {
            $navigationRoot = [ScaleNative]::GetAncestor([IntPtr]$before.combo.top.Current.NativeWindowHandle,2)
            [uint32]$navigationOwner = 0
            [void][ScaleNative]::GetWindowThreadProcessId($navigationRoot,[ref]$navigationOwner)
        }
        if ($before.dpi -ne (96 * $before.percent / 100)) { throw 'Original Settings DPI does not match its selection.' }
        $original = @{scale=$before.percent; dpi=$before.dpi; settings_pid=$settingsId; settings_start=$settingsStart; session=$session}
        if ($ProvisionResolution) {
            $script:Stage = 'capture_original_resolution'
            $script:ResolutionState = [HostedDisplayMode]::Capture([IntPtr]$before.combo.top.Current.NativeWindowHandle)
            $original.resolution = @{device=$script:ResolutionState.Device; identity=$script:ResolutionState.Identity
                mode=[Convert]::ToBase64String($script:ResolutionState.Original)}
            $receipt.original_width = [HostedDisplayMode]::Width($script:ResolutionState.Original)
            $receipt.original_height = [HostedDisplayMode]::Height($script:ResolutionState.Original)
        }
        # This is private recovery state, excluded from public upload. Publish
        # atomically before any input so the supervisor can restore after timeout.
        $original | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($originalPath + '.tmp') -Encoding utf8
        Move-Item -LiteralPath ($originalPath + '.tmp') -Destination $originalPath
        if ($ProvisionResolution) {
            $script:Stage = 'provision_resolution'
            $target = [HostedDisplayMode]::Target($script:ResolutionState,$ResolutionMode)
            $modeResult = [HostedDisplayMode]::Apply($script:ResolutionState,$target)
            $receipt.resolution_test_code = $modeResult.TestCode
            $receipt.resolution_apply_code = $modeResult.ApplyCode
            $receipt.resolution_already_current = $modeResult.AlreadyCurrent
            $receipt.resolution_verified = $modeResult.Verified
            if (-not $modeResult.Verified) { throw 'Requested resolution did not verify.' }
            $receipt.selected_width = [HostedDisplayMode]::Width($target)
            $receipt.selected_height = [HostedDisplayMode]::Height($target)
            $script:Stage = 'observe_scale_after_resolution'
            $afterResolution = Read-Scale @(Read-Controls)
            $receipt.scale_after_resolution = $afterResolution.percent
            $receipt.settings_dpi_after_resolution = $afterResolution.dpi
        }
        if ($RefreshSettingsPage) { Invoke-OwnedSettingsPageRefresh $navigationRoot $navigationOwner }
        $selected = Set-Scale $ScalePercent
        $receipt.selected_scale = $selected.percent
        $receipt.measured_settings_dpi = $selected.dpi
        $receipt.status = 'selected_and_measured'
        if ($NativeRuntime) {
            # Product execution is unreachable until this invocation has both
            # observed the selected value and measured the requested native DPI.
            $script:Stage = 'native_runtime'
            $nativeRequest = Read-NativeRequest
            $requestHash = (Get-FileHash -LiteralPath $nativeRequestPath -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($nativeRequest.scope -ceq 'minimum-resize') {
                # Create before launching any product. Only the fixed adapter
                # can publish matching restored evidence after the complete
                # minimum operation and native teardown have both succeeded.
                $started = Join-Path $output 'native-input.started'
                $stream = [IO.File]::Open($started,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                try { $bytes = [Text.Encoding]::ASCII.GetBytes($requestHash); $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) }
                finally { $stream.Dispose() }
            }
            $nativeResult = Invoke-BoundedProcess $nativePython @($nativeAdapter,'--job-name',$nativeRequest.job_name) 1800 $false $nativeRequest.job_name
            $receipt.native_runtime_termination_verified = $nativeResult.terminated
            if (-not $nativeResult.terminated -or $nativeResult.code -ne 0) { throw 'Contained native runtime unavailable.' }
            if (Test-UncertainInput) { throw 'Native input restoration is unverified.' }
            $nativeFile = Get-Item -LiteralPath $nativeReceiptPath
            if ($nativeFile.Length -le 0 -or $nativeFile.Length -gt 8192 -or
                ($nativeFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Native receipt unavailable.' }
            $nativeEvidence = Get-Content -LiteralPath $nativeReceiptPath -Raw | ConvertFrom-Json
            if ($nativeEvidence.status -cne 'runtime_and_membership_verified' -or
                $nativeEvidence.request_id -cne $nativeRequest.request_id -or
                $nativeEvidence.request_sha256 -cne $requestHash -or
                $nativeEvidence.holder_membership_count -lt 1 -or $nativeEvidence.product_membership_count -lt 1) {
                throw 'Native runtime receipt binding failed.'
            }
            $receipt.action = 'native_runtime_and_membership_verified'
        }
    }
} catch {
    $receipt.status = 'unavailable'
    $receipt.failure_stage = $script:Stage
} finally {
    # Copy before recovery, so successful restoration cannot overwrite the
    # failed selection's observations. Only fixed keys and scalar values leave.
    if ($ProvisionResolution) {
        $receipt.resolution_selection_diagnostic = @{stage=[HostedDisplayMode]::DiagnosticStage; code=[HostedDisplayMode]::DiagnosticCode
            last_mode_buffer_bytes=[HostedDisplayMode]::DiagnosticBufferBytes; last_mode_size=[HostedDisplayMode]::DiagnosticModeSize
            last_driver_extra_bytes=[HostedDisplayMode]::DiagnosticDriverExtra; last_enum_succeeded=[HostedDisplayMode]::DiagnosticEnumSucceeded}
    }
    $receipt.selection_observations = $script:Observation.Clone()
    $script:Observation = @{}
    if ($null -ne $original -and -not (Test-UncertainChildren) -and -not (Test-UncertainInput)) {
        try {
            if (Test-UncertainNavigation) { $script:Stage='navigation_return_unverified'; throw 'Navigation recovery remains unverified.' }
            $restored = Set-Scale ([int]$original.scale)
            $receipt.restored = $restored.percent -eq $original.scale -and $restored.dpi -eq $original.dpi
            $receipt.original_scale = $original.scale
            $receipt.restored_settings_dpi = $restored.dpi
        } catch {
            $receipt.restored = $false
            $receipt.restoration_failure_stage = $script:Stage
        }
        if ($ProvisionResolution) {
            # Keep this independent of the scale/UIA attempt above. Never let a
            # failed UI observation skip restoring a known owned display mode.
            $receipt.resolution_restored = $false
            if (-not (Test-UncertainChildren)) {
                try {
                    $modeRestore = [HostedDisplayMode]::Apply($script:ResolutionState,$script:ResolutionState.Original)
                    $receipt.resolution_restore_test_code = $modeRestore.TestCode
                    $receipt.resolution_restore_apply_code = $modeRestore.ApplyCode
                    $receipt.resolution_restore_already_current = $modeRestore.AlreadyCurrent
                    $receipt.resolution_restored = $modeRestore.Verified -and [HostedDisplayMode]::OriginalCurrent($script:ResolutionState)
                } catch {}
            }
            # Reobserve scale after restoring resolution, because a mode change
            # may itself affect the offered/selected scale. Both must match.
            $receipt.restored = $false
            if ($receipt.resolution_restored -and -not (Test-UncertainChildren) -and -not (Test-UncertainNavigation)) {
                try {
                    [HostedDisplayMode]::AssertBinding($script:ResolutionState)
                    $finalScale = Read-Scale @(Read-Controls)
                    $receipt.restored = $finalScale.percent -eq $original.scale -and $finalScale.dpi -eq $original.dpi
                    $receipt.restored_settings_dpi = $finalScale.dpi
                } catch {}
            }
        }
    }
    if ($ProvisionResolution) {
        $receipt.resolution_restoration_diagnostic = @{stage=[HostedDisplayMode]::DiagnosticStage; code=[HostedDisplayMode]::DiagnosticCode
            last_mode_buffer_bytes=[HostedDisplayMode]::DiagnosticBufferBytes; last_mode_size=[HostedDisplayMode]::DiagnosticModeSize
            last_driver_extra_bytes=[HostedDisplayMode]::DiagnosticDriverExtra; last_enum_succeeded=[HostedDisplayMode]::DiagnosticEnumSucceeded}
    }
    $receipt.restoration_observations = $script:Observation.Clone()
    if ($DiagnosticEvidence) { $receipt.diagnostic_evidence = $script:DiagnosticResults.Clone() }
    $receipt.child_termination_uncertain = Test-UncertainChildren
    $receipt.input_recovery_uncertain = Test-UncertainInput
    $receipt.navigation_recovery_uncertain = Test-UncertainNavigation
    $receipt.page_refresh = $script:NavigationObservation.Clone()
    $receipt.disposal_required = $receipt.child_termination_uncertain -or $receipt.input_recovery_uncertain -or $receipt.navigation_recovery_uncertain -or -not $receipt.restored
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output ($Mode + '.json')) -Encoding utf8
}
if ($receipt.restored -and ($Mode -eq 'restore' -or $receipt.status -eq 'selected_and_measured')) { exit 0 }
exit 2
