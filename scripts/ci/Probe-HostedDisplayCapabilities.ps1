[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $OutputDirectory,
    [ValidateSet('supervisor','worker')][string] $Mode = 'supervisor'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
    $env:RUNNER_OS -cne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'Display capability observation requires disposable hosted Windows execution.'
}
$tempRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Output escapes RUNNER_TEMP.' }
$ancestor = [IO.DirectoryInfo]::new($output)
while ($null -ne $ancestor) {
    if ($ancestor.Exists -and ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse output unavailable.' }
    $ancestor = $ancestor.Parent
}
$workerPath = Join-Path $output 'worker.json'
if ($Mode -eq 'supervisor') {
    if (Test-Path -LiteralPath $output) { throw 'Capability output must be new.' }
    [void](New-Item -ItemType Directory -Path $output)
    $receipt = @{schema=1; status='unavailable'; reason='worker_unavailable'; read_only=$true
        process_termination_verified=$false; disposal_required=$true}
    try {
        Add-Type -Path (Join-Path $PSScriptRoot 'HostedScaleProcess.cs')
        $pwsh = (Get-Process -Id $PID).Path
        $result = [HostedScaleProcess]::Run($pwsh,
            [string[]]@('-NoProfile','-File',$PSCommandPath,'-OutputDirectory',$output,'-Mode','worker'),45,$false)
        $receipt.process_termination_verified = $result.Terminated
        $receipt.disposal_required = -not $result.Terminated
        if ($result.Terminated -and $result.Code -in @(0,2) -and (Test-Path -LiteralPath $workerPath)) {
            $file = Get-Item -LiteralPath $workerPath
            if ($file.Length -le 0 -or $file.Length -gt 131072 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw 'Capability receipt exceeds bounds.'
            }
            $worker = Get-Content -LiteralPath $workerPath -Raw | ConvertFrom-Json -AsHashtable
            foreach ($key in @('status','reason','current_mode','supported_modes','mode_enumeration_complete',
                'monitor_width','monitor_height','primary_monitor','active_display_target_count','resolution_combo_matches','resolution_selection')) {
                if ($worker.ContainsKey($key)) { $receipt[$key] = $worker[$key] }
            }
            if ($result.Code -ne 0 -and $receipt.status -eq 'observed') { $receipt.status='unavailable'; $receipt.reason='worker_exit_mismatch' }
        }
    } catch {
        $receipt.status = 'unavailable'
    } finally {
        $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
    }
    if ($receipt.status -eq 'observed' -and $receipt.process_termination_verified) { exit 0 }
    exit 2
}

$receipt = @{schema=1; status='unavailable'; reason='initialize_observation'; read_only=$true}
$stage = 'initialize_observation'
try {
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DisplayCapabilityNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct MONITOR {
        public uint Size; public RECT Bounds,Work; public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Device;
    }
    // DEVMODEW display union offsets and complete native structure size.
    [StructLayout(LayoutKind.Explicit,CharSet=CharSet.Unicode,Size=220)] public struct MODE {
        [FieldOffset(68)] public ushort Size;
        [FieldOffset(72)] public uint Fields;
        [FieldOffset(84)] public uint Orientation;
        [FieldOffset(168)] public uint BitsPerPixel;
        [FieldOffset(172)] public uint Width;
        [FieldOffset(176)] public uint Height;
        [FieldOffset(184)] public uint Frequency;
    }
    // Only active paths are queried. The private adapter/target identities are
    // retained in memory solely to reject topology changes between observations.
    [StructLayout(LayoutKind.Explicit,Size=72)] public struct PATH {
        [FieldOffset(0)] public uint SourceLow;
        [FieldOffset(4)] public int SourceHigh;
        [FieldOffset(8)] public uint SourceId;
        [FieldOffset(20)] public uint TargetLow;
        [FieldOffset(24)] public int TargetHigh;
        [FieldOffset(28)] public uint TargetId;
        [FieldOffset(68)] public uint Flags;
    }
    [StructLayout(LayoutKind.Explicit,Size=64)] public struct MODEINFO {
        [FieldOffset(0)] public uint Type;
    }
    [DllImport("user32.dll")] static extern int GetDisplayConfigBufferSizes(uint flags,out uint paths,out uint modes);
    [DllImport("user32.dll")] static extern int QueryDisplayConfig(uint flags,ref uint paths,[Out] PATH[] pathArray,ref uint modes,[Out] MODEINFO[] modeArray,IntPtr topology);
    public static bool SingleActiveTarget(out string identity,out uint count) {
        identity=null; count=0;
        for(int attempt=0; attempt<2; attempt++) {
            uint paths,modes;
            if(GetDisplayConfigBufferSizes(2,out paths,out modes)!=0 || paths==0 || paths>64 || modes==0 || modes>256) return false;
            var pathArray=new PATH[paths]; var modeArray=new MODEINFO[modes];
            int result=QueryDisplayConfig(2,ref paths,pathArray,ref modes,modeArray,IntPtr.Zero);
            if(result==122) continue; // Configuration changed; retry once within fixed bounds.
            if(result!=0) return false;
            count=paths;
            if(paths!=1 || (pathArray[0].Flags & 1)==0) return false;
            PATH path=pathArray[0];
            identity=path.SourceLow+":"+path.SourceHigh+":"+path.SourceId+":"+
                path.TargetLow+":"+path.TargetHigh+":"+path.TargetId;
            return true;
        }
        return false;
    }
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr window,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr monitor,ref MONITOR info);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool EnumDisplaySettings(string device,int index,ref MODE mode);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
}
'@
    # Coordinate observation only, not a scale change or scale proof.
    if ([DisplayCapabilityNative]::SetThreadDpiAwarenessContext([IntPtr](-4)) -eq [IntPtr]::Zero) { throw 'Observation context unavailable.' }
    $stage = 'settings_identity'
    $session = (Get-Process -Id $PID).SessionId
    $settings = @(Get-Process -Name SystemSettings -ErrorAction SilentlyContinue | Where-Object SessionId -eq $session)
    if ($settings.Count -ne 1) { throw 'Unique Settings identity unavailable.' }
    $settingsId = $settings[0].Id
    $frames = @(Get-Process -Name ApplicationFrameHost -ErrorAction SilentlyContinue | Where-Object SessionId -eq $session)
    $owners = @{}
    foreach ($process in (@($settings[0]) + $frames)) { $owners[$process.Id] = $process.StartTime.ToUniversalTime().Ticks }
    function Assert-Owners {
        foreach ($id in $owners.Keys) {
            $live = Get-Process -Id $id
            if ($live.SessionId -ne $session -or $live.StartTime.ToUniversalTime().Ticks -ne $owners[$id]) {
                throw 'Settings identity changed.'
            }
        }
    }
    $stage = 'settings_controls'
    $desktop = [Windows.Automation.AutomationElement]::RootElement
    $walker = [Windows.Automation.TreeWalker]::ControlViewWalker
    $queue = [Collections.Generic.Queue[object]]::new()
    foreach ($id in $owners.Keys) {
        $condition = [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,[int]$id)
        foreach ($top in $desktop.FindAll([Windows.Automation.TreeScope]::Children,$condition)) { $queue.Enqueue(@{element=$top; top=$top}) }
    }
    $rows = [Collections.Generic.List[object]]::new()
    $count = 0
    while ($queue.Count -gt 0) {
        if (++$count -gt 1000) { throw 'Control traversal exceeded bounds.' }
        $row = $queue.Dequeue()
        if ($row.element.Current.ProcessId -eq $settingsId) { $rows.Add($row) }
        $child = $walker.GetFirstChild($row.element)
        while ($null -ne $child) {
            if (($count + $queue.Count) -ge 1000) { throw 'Control traversal exceeded bounds.' }
            $queue.Enqueue(@{element=$child; top=$row.top})
            $child = $walker.GetNextSibling($child)
        }
    }
    Assert-Owners
    $scales = @($rows | Where-Object {
        $_.element.Current.AutomationId -ceq 'SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox' -and
        $_.element.Current.ControlType -eq [Windows.Automation.ControlType]::ComboBox -and
        $_.element.Current.IsEnabled -and -not $_.element.Current.IsOffscreen
    })
    if ($scales.Count -ne 1) { throw 'Unique Scale root unavailable.' }
    $stage = 'settings_monitor'
    $window = [IntPtr]$scales[0].top.Current.NativeWindowHandle
    [uint32]$nativePid = 0
    [void][DisplayCapabilityNative]::GetWindowThreadProcessId($window,[ref]$nativePid)
    if (-not $owners.ContainsKey([int]$nativePid)) { throw 'Settings monitor owner unavailable.' }
    $monitor = [DisplayCapabilityNative]::MonitorFromWindow($window,0)
    $info = [DisplayCapabilityNative+MONITOR]::new()
    $info.Size = [Runtime.InteropServices.Marshal]::SizeOf($info)
    if ($monitor -eq [IntPtr]::Zero -or -not [DisplayCapabilityNative]::GetMonitorInfo($monitor,[ref]$info)) {
        throw 'Settings monitor unavailable.'
    }
    $receipt.monitor_width = $info.Bounds.Right - $info.Bounds.Left
    $receipt.monitor_height = $info.Bounds.Bottom - $info.Bounds.Top
    $receipt.primary_monitor = ($info.Flags -band 1) -ne 0
    $stage = 'single_active_display_target'
    [string]$displayIdentity = $null
    [uint32]$displayCount = 0
    $single = [DisplayCapabilityNative]::SingleActiveTarget([ref]$displayIdentity,[ref]$displayCount)
    $receipt.active_display_target_count = if ($displayCount -gt 0) { $displayCount } else { $null }
    if (-not $single) { throw 'Unique active display target unavailable.' }
    function Mode-Values($Value) {
        return @{width=[int]$Value.Width; height=[int]$Value.Height; bits_per_pixel=[int]$Value.BitsPerPixel
            frequency_hz=[int]$Value.Frequency; fields=[uint32]$Value.Fields
            orientation=$(if ($Value.Fields -band 0x80) { [int]$Value.Orientation } else { $null })}
    }
    $stage = 'current_display_mode'
    $current = [DisplayCapabilityNative+MODE]::new(); $current.Size = 220
    if (-not [DisplayCapabilityNative]::EnumDisplaySettings($info.Device,-1,[ref]$current)) { throw 'Current mode unavailable.' }
    $receipt.current_mode = Mode-Values $current
    $stage = 'supported_display_modes'
    $modes = [Collections.Generic.List[object]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new()
    $complete = $false
    for ($index=0; $index -lt 512; $index++) {
        $modeValue = [DisplayCapabilityNative+MODE]::new(); $modeValue.Size=220
        if (-not [DisplayCapabilityNative]::EnumDisplaySettings($info.Device,$index,[ref]$modeValue)) { $complete=$true; break }
        $orientation = if ($modeValue.Fields -band 0x80) { "$($modeValue.Orientation)" } else { 'unknown' }
        $key = "$($modeValue.Width):$($modeValue.Height):$($modeValue.BitsPerPixel):$($modeValue.Frequency):$orientation"
        if ($seen.Add($key)) { $modes.Add((Mode-Values $modeValue)) }
    }
    $receipt.supported_modes = $modes.ToArray()
    $receipt.mode_enumeration_complete = $complete
    $stage = 'resolution_selection'
    $resolutionCandidates = [Collections.Generic.List[object]]::new()
    foreach ($row in $rows) {
        $value = $row.element.Current
        if (-not $row.top.Equals($scales[0].top) -or [IntPtr]$row.top.Current.NativeWindowHandle -ne $window -or
            $value.ControlType -ne [Windows.Automation.ControlType]::ComboBox -or
            -not $value.IsEnabled -or $value.IsOffscreen) { continue }
        try {
            $pattern = $row.element.GetCurrentPattern([Windows.Automation.SelectionPattern]::Pattern)
            $selected = @($pattern.Current.GetSelection())
            if ($selected.Count -ne 1) { continue }
            $name = $selected[0].Current.Name
            if ($name -cmatch '^([0-9]{3,5})\s*[x×]\s*([0-9]{3,5})( \(Recommended\))?$') {
                $width = [int]$Matches[1]; $height = [int]$Matches[2]
                if ($width -eq $current.Width -and $height -eq $current.Height) { $resolutionCandidates.Add(@{width=$width; height=$height}) }
            }
        } catch {} # Unsupported patterns never authorize another observation route.
    }
    Assert-Owners
    $stage = 'stable_monitor_observation'
    [string]$afterIdentity = $null
    [uint32]$afterCount = 0
    if (-not [DisplayCapabilityNative]::SingleActiveTarget([ref]$afterIdentity,[ref]$afterCount) -or
        $afterCount -ne 1 -or $afterIdentity -cne $displayIdentity) { throw 'Active display target changed.' }
    [void][DisplayCapabilityNative]::GetWindowThreadProcessId($window,[ref]$nativePid)
    $after = [DisplayCapabilityNative+MODE]::new(); $after.Size=220
    if (-not $owners.ContainsKey([int]$nativePid) -or
        [DisplayCapabilityNative]::MonitorFromWindow($window,0) -ne $monitor -or
        -not [DisplayCapabilityNative]::EnumDisplaySettings($info.Device,-1,[ref]$after) -or
        $after.Width -ne $current.Width -or $after.Height -ne $current.Height -or
        $after.BitsPerPixel -ne $current.BitsPerPixel -or $after.Frequency -ne $current.Frequency -or
        ($after.Fields -band 0x80) -ne ($current.Fields -band 0x80) -or
        (($current.Fields -band 0x80) -and $after.Orientation -ne $current.Orientation)) { throw 'Monitor mode changed during observation.' }
    $receipt.resolution_combo_matches = $resolutionCandidates.Count
    if ($resolutionCandidates.Count -eq 1) { $receipt.resolution_selection = $resolutionCandidates[0] }
    if (-not $complete) { $receipt.reason='display_mode_limit_reached' }
    elseif ($modes.Count -eq 0) { $receipt.reason='supported_modes_unavailable' }
    elseif ($resolutionCandidates.Count -ne 1) { $receipt.reason='resolution_selection_unavailable' }
    else { $receipt.status='observed'; $receipt.reason='numeric_capabilities_observed' }
} catch {
    $receipt.status = 'unavailable'
    $receipt.reason = $stage
} finally {
    # Device names, process/window identities, UI labels and exception text never
    # enter this fixed numeric receipt or subprocess output.
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $workerPath -Encoding utf8
}
if ($receipt.status -eq 'observed') { exit 0 }
exit 2
