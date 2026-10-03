# Definitions only. The caller owns the disposable hosted session and recovery.
function Test-PageRefreshScope([bool] $Requested, [bool] $Native, [bool] $Resolution, [bool] $Diagnostic, [string] $Route) {
    return -not $Requested -or (-not $Native -and $Resolution -and $Diagnostic -and $Route -ceq 'hosted-foreground')
}
function Test-UncertainNavigation {
    # Launcher exit and process-tree termination cannot settle broker navigation.
    return Test-Path -LiteralPath (Join-Path $output 'navigation.pending')
}
function Test-ColorsPageMarkers($Rows) {
    $required = @(
        @{id='SystemSettings_Personalize_Color_ColorMode_ComboBox'; type='ControlType.ComboBox'; patterns=@('SelectionPatternIdentifiers.Pattern','ExpandCollapsePatternIdentifiers.Pattern')},
        @{id='SystemSettings_Personalize_Color_AccentColorMode_ComboBox'; type='ControlType.ComboBox'; patterns=@('SelectionPatternIdentifiers.Pattern','ExpandCollapsePatternIdentifiers.Pattern')},
        @{id='SystemSettings_Personalize_Color_EnableTransparency_ToggleSwitch'; type='ControlType.Button'; patterns=@('TogglePatternIdentifiers.Pattern')})
    foreach ($item in $required) {
        $matches = @($Rows | Where-Object { $_.id -ceq $item.id })
        if ($matches.Count -ne 1) { return $false }
        $match = $matches[0]
        if ($match.type -cne $item.type -or $match.enabled -isnot [bool] -or -not $match.enabled -or
            $match.offscreen -isnot [bool] -or $match.offscreen) { return $false }
        foreach ($pattern in $item.patterns) { if ($match.patterns -cnotcontains $pattern) { return $false } }
    }
    return $true
}
function Assert-NavigationOwner([IntPtr] $Root, [uint32] $Owner) {
    if ((Test-UncertainChildren) -or (Test-UncertainInput)) { throw 'Navigation ownership unavailable.' }
    [HostedDisplayMode]::AssertBinding($script:ResolutionState)
    if ((Object-Name ([ScaleNative]::GetProcessWindowStation())) -cne 'WinSta0' -or
        (Desktop-Name ([ScaleNative]::GetCurrentThreadId())) -cne 'Default') { throw 'Navigation desktop unavailable.' }
    $inputDesktop = [ScaleNative]::OpenInputDesktop(0,$false,1)
    try { if ((Object-Name $inputDesktop) -cne 'Default') { throw 'Navigation input desktop changed.' } }
    finally { if ($inputDesktop -ne [IntPtr]::Zero) { [void][ScaleNative]::CloseDesktop($inputDesktop) } }
    foreach ($processId in $allowed) {
        $live = Get-Process -Id $processId
        if ($live.SessionId -ne $session -or $live.StartTime.ToUniversalTime().Ticks -ne $processStarts[$processId]) {
            throw 'Navigation process identity changed.'
        }
    }
    [uint32]$currentOwner = 0
    $thread = [ScaleNative]::GetWindowThreadProcessId($Root,[ref]$currentOwner)
    if ($Root -eq [IntPtr]::Zero -or $currentOwner -ne $Owner -or $allowed -notcontains [int]$Owner -or
        (Desktop-Name $thread) -cne 'Default' -or -not [ScaleNative]::IsWindowVisible($Root)) {
        throw 'Navigation root identity changed.'
    }
}
function Wait-OwnedSettingsPage([ValidateSet('colors','display')][string] $Page, [IntPtr] $Root, [uint32] $Owner) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $consecutive = 0
    do {
        Assert-NavigationOwner $Root $Owner
        $observationStarted = [DateTime]::UtcNow
        $rows = @(Read-Controls)
        $owned = @($rows | Where-Object {
            [ScaleNative]::GetAncestor([IntPtr]$_.top.Current.NativeWindowHandle,2) -eq $Root
        })
        $matched = $false
        if ($Page -ceq 'colors') {
            $markers = @($owned | Where-Object {
                $_.element.Current.AutomationId -cin @('SystemSettings_Personalize_Color_ColorMode_ComboBox',
                    'SystemSettings_Personalize_Color_AccentColorMode_ComboBox','SystemSettings_Personalize_Color_EnableTransparency_ToggleSwitch')
            } | ForEach-Object {
                $current = $_.element.Current
                if ($current.ProcessId -ne $settingsId) { throw 'Navigation control owner changed.' }
                @{id=$current.AutomationId; type=$current.ControlType.ProgrammaticName; enabled=$current.IsEnabled
                    offscreen=$current.IsOffscreen; patterns=@($_.element.GetSupportedPatterns() | ForEach-Object ProgrammaticName)}
            })
            $matched = Test-ColorsPageMarkers $markers
        } else {
            try {
                $state = Read-Scale $owned
                # Uses fresh visible geometry plus actual foreground/point owner.
                # It observes only and does not activate or send input.
                Observe-ForegroundAfterInput $Root
                $matched = $state.combo.element.Current.ProcessId -eq $settingsId
            } catch { $matched = $false }
        }
        Assert-NavigationOwner $Root $Owner
        if ($matched -and ([DateTime]::UtcNow-$observationStarted).TotalMilliseconds -le 1000) { $consecutive++ }
        else { $consecutive = 0 }
        if ($consecutive -ge 2 -and $timer.ElapsedMilliseconds -lt 10000) { return }
        Start-Sleep -Milliseconds 150
    } while ($timer.ElapsedMilliseconds -lt 10000)
    throw 'Owned Settings page acknowledgement unavailable.'
}
function Invoke-OwnedSettingsPageRefresh([IntPtr] $Root, [uint32] $owner) {
    if (-not $RefreshSettingsPage -or $Mode -cne 'run' -or (Test-UncertainNavigation)) { throw 'Page diagnostic unavailable.' }
    Assert-NavigationOwner $Root $owner
    $marker = Join-Path $output 'navigation.pending'
    $hash = (Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $stream = [IO.File]::Open($marker,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $bytes=[Text.Encoding]::ASCII.GetBytes($hash); $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
    $script:Stage = 'navigate_colors'
    # Fixed protocol launches only, never an arbitrary URI or a control action.
    # Completion is acknowledged by owned page content, not launcher exit.
    Start-Process -FilePath 'ms-settings:colors' -WindowStyle Hidden -ErrorAction Stop
    Wait-OwnedSettingsPage 'colors' $Root $owner
    $script:NavigationObservation.colors_acknowledged = $true
    $script:Stage = 'navigate_display'
    Assert-NavigationOwner $Root $owner
    Start-Process -FilePath 'ms-settings:display' -WindowStyle Hidden -ErrorAction Stop
    Wait-OwnedSettingsPage 'display' $Root $owner
    $script:NavigationObservation.display_acknowledged = $true
    # Only positive return acknowledgement clears the durable uncertainty.
    if ([IO.File]::ReadAllText($marker) -cne $hash -or
        (Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash) {
        throw 'Navigation recovery binding changed.'
    }
    Move-Item -LiteralPath $marker -Destination (Join-Path $output 'navigation.returned') -ErrorAction Stop
}
