[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $OutputDirectory,
    [ValidateSet('display','colors')][string] $Destination = 'display',
    [ValidateRange(5, 90)][int] $TimeoutSeconds = 45,
    [switch] $InventoryWorker
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
    $env:RUNNER_OS -cne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'This probe requires a disposable GitHub-hosted Windows runner.'
}
$root = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be inside RUNNER_TEMP.'
}

function Write-Summary([string] $Reason, [int] $Count = 0) {
    [ordered]@{
        schema = 1; mode = 'read_only_inventory'; status = 'unavailable'; reason = $Reason
        run_id = $env:GITHUB_RUN_ID; requested_dpi = @(96, 120, 144, 192)
        selected_scale = $null; measured_target_dpi = $null; provisioned = $false
        settings_element_count = $Count; display_mutated = $false
        requested_destination = $Destination; navigation_completion = 'unverified'
        restoration = 'not_required_no_display_mutation'
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'capability.json') -Encoding utf8
}

if (-not $InventoryWorker) {
    if (Test-Path -LiteralPath $output) { throw 'Output directory must be new.' }
    [void](New-Item -ItemType Directory -Path $output)
    # Isolate UIA calls in a killable process: providers can hang inside native calls.
    $child = $null
    $startAttempted = $false
    $terminated = $false
    $reason = 'inventory_parent_failed'
    $termination = 'not_started'
    try {
        $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
        # Never forward raw child diagnostics. Native providers may include labels.
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        foreach ($argument in @('-NoProfile', '-File', $PSCommandPath, '-OutputDirectory', $output, '-Destination', $Destination, '-InventoryWorker')) {
            [void]$start.ArgumentList.Add($argument)
        }
        $child = [Diagnostics.Process]::new()
        $child.StartInfo = $start
        $startAttempted = $true
        if (-not $child.Start()) { throw 'Worker did not start.' }
        $terminated = $child.WaitForExit($TimeoutSeconds * 1000)
        if ($terminated) {
            $termination = 'observed_exit'
            $reason = if (Test-Path -LiteralPath (Join-Path $output 'capability.json')) {
                'inventory_worker_exited'
            } else { 'inventory_worker_failed' }
        } else {
            $reason = 'uia_inventory_timeout'
        }
    } catch {
        $reason = 'inventory_spawn_or_wait_exception'
    } finally {
        if ($startAttempted -and -not $terminated) {
            try {
                if (-not $child.HasExited) { $child.Kill($true) }
                $terminated = $child.WaitForExit(5000)
                $termination = if ($terminated) { 'observed_exit_after_stop' } else { 'termination_timeout' }
            } catch {
                $termination = 'termination_exception'
                # A kill can race a normal exit. Credit only an actual observation.
                try {
                    $terminated = $child.HasExited
                    if ($terminated) { $termination = 'observed_exit_after_exception' }
                } catch { $terminated = $false }
            }
        }
        $teardownVerified = (-not $startAttempted) -or $terminated
        # Parent never writes capability.json: an unkillable worker might still
        # own that path. This separate receipt is authoritative for termination.
        try {
            [ordered]@{
                schema = 1; status = 'unavailable'; reason = $reason
                worker_termination = $termination; teardown_verified = $teardownVerified
                inventory_stable = $terminated; provisioned = $false
                requested_destination = $Destination; navigation_completion = 'unverified'
                completed = $teardownVerified
                disposal_required = -not $teardownVerified
            } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'supervisor.json') -Encoding utf8
        } catch {
            [Console]::Error.WriteLine('supervisor_receipt_unavailable')
        }
        if ($null -ne $child) {
            try { $child.Dispose() } catch { [Console]::Error.WriteLine('process_handle_disposal_failed') }
        }
    }
    # Inventory is never a successful scale-provisioning verdict.
    exit 2
}

try {
    $session = (Get-Process -Id $PID).SessionId
    $settings = @(Get-Process -Name SystemSettings -ErrorAction SilentlyContinue |
        Where-Object { $_.SessionId -eq $session })
    if ($settings.Count -ne 1) { Write-Summary 'settings_process_missing_or_ambiguous'; exit 2 }
    $settingsStart = $settings[0].StartTime.ToUniversalTime().Ticks
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $condition = [Windows.Automation.PropertyCondition]::new(
        [Windows.Automation.AutomationElement]::ProcessIdProperty, [int]$settings[0].Id)
    $windows = [Windows.Automation.AutomationElement]::RootElement.FindAll(
        [Windows.Automation.TreeScope]::Children, $condition)
    $selected = $null
    $ownedRoot = $null
    $ownedRootStart = $null
    $selection = 'direct_process_root'
    $frameRootCount = 0
    $matchedRootCount = $windows.Count
    $discoveryCount = 0
    $discoveryLimited = $false
    $walker = [Windows.Automation.TreeWalker]::ControlViewWalker
    if ($windows.Count -eq 1) {
        $selected = $windows[0]
        $ownedRoot = $windows[0]
        $ownedRootStart = $settingsStart
    } elseif ($windows.Count -gt 1) {
        Write-Summary 'settings_window_missing_or_ambiguous'; exit 2
    } else {
        # Packaged Settings can be hosted below an ApplicationFrameHost root.
        # Match only a live same-session frame host and the exact Settings PID;
        # never select by a window title or inventory another process's labels.
        $selection = 'frame_host_process_descendant'
        $frames = @(Get-Process -Name ApplicationFrameHost -ErrorAction SilentlyContinue |
            Where-Object { $_.SessionId -eq $session })
        $matches = [Collections.Generic.List[object]]::new()
        foreach ($frame in $frames) {
            $frameCondition = [Windows.Automation.PropertyCondition]::new(
                [Windows.Automation.AutomationElement]::ProcessIdProperty, [int]$frame.Id)
            $roots = [Windows.Automation.AutomationElement]::RootElement.FindAll(
                [Windows.Automation.TreeScope]::Children, $frameCondition)
            $frameRootCount += $roots.Count
            foreach ($frameRoot in $roots) {
                $pending = [Collections.Generic.Queue[object]]::new()
                $pending.Enqueue($frameRoot)
                while ($pending.Count -gt 0 -and $discoveryCount -lt 1000) {
                    $candidate = $pending.Dequeue()
                    $discoveryCount++
                    if ($candidate.Current.ProcessId -eq $settings[0].Id) {
                        $matches.Add(@{element=$candidate; root=$frameRoot; start=$frame.StartTime.ToUniversalTime().Ticks})
                        # This is the process-owned subtree root. Its children
                        # must not count as additional independent surfaces.
                        continue
                    }
                    $childElement = $walker.GetFirstChild($candidate)
                    while ($null -ne $childElement) {
                        if (($pending.Count + $discoveryCount) -ge 1000) {
                            $discoveryLimited = $true
                            break
                        }
                        $pending.Enqueue($childElement)
                        $childElement = $walker.GetNextSibling($childElement)
                    }
                    if ($discoveryLimited) { break }
                }
                if ($pending.Count -gt 0) { $discoveryLimited = $true }
                if ($discoveryLimited) { break }
            }
            if ($discoveryLimited) { break }
        }
        $matchedRootCount = $matches.Count
        # An incomplete traversal cannot prove uniqueness.
        if ($discoveryLimited) { Write-Summary 'settings_host_discovery_limit'; exit 2 }
        if ($matches.Count -ne 1) { Write-Summary 'settings_host_descendant_missing_or_ambiguous'; exit 2 }
        $selected = $matches[0].element
        $ownedRoot = $matches[0].root
        $ownedRootStart = $matches[0].start
    }
    $rootPid = $ownedRoot.Current.ProcessId
    $rootHandle = $ownedRoot.Current.NativeWindowHandle
    if ($rootHandle -eq 0) { Write-Summary 'settings_root_handle_unavailable'; exit 2 }
    function Assert-InventoryIdentity {
        $liveSettings = Get-Process -Id $settings[0].Id
        $liveRoot = Get-Process -Id $rootPid
        if ($liveSettings.SessionId -ne $session -or $liveSettings.StartTime.ToUniversalTime().Ticks -ne $settingsStart -or
            $liveRoot.SessionId -ne $session -or $liveRoot.StartTime.ToUniversalTime().Ticks -ne $ownedRootStart -or
            $ownedRoot.Current.ProcessId -ne $rootPid -or $ownedRoot.Current.NativeWindowHandle -ne $rootHandle -or
            $selected.Current.ProcessId -ne $settings[0].Id) { throw 'Inventory identity changed.' }
    }
    Assert-InventoryIdentity
    $rows = [Collections.Generic.List[object]]::new()
    $queue = [Collections.Generic.Queue[object]]::new()
    $queue.Enqueue($selected)
    while ($queue.Count -gt 0 -and $rows.Count -lt 1000) {
        $element = $queue.Dequeue()
        $current = $element.Current
        if ($current.ProcessId -ne $settings[0].Id) { throw 'Inventory control owner changed.' }
        if ($current.Name.Length -gt 2048 -or $current.AutomationId.Length -gt 2048) { throw 'Inventory label exceeds bounds.' }
        $patterns = @($element.GetSupportedPatterns() | ForEach-Object { $_.ProgrammaticName })
        if ($patterns.Count -gt 32) { throw 'Inventory pattern count exceeds bounds.' }
        foreach ($pattern in $patterns) { if ($pattern.Length -gt 256) { throw 'Inventory pattern exceeds bounds.' } }
        $selectedState = $null
        try {
            $selectionItem = $element.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern)
            $selectedState = [bool]$selectionItem.Current.IsSelected
        } catch {} # Unsupported patterns remain unknown, never inferred.
        $rows.Add([ordered]@{
            name = $current.Name; automation_id = $current.AutomationId
            type = $current.ControlType.ProgrammaticName; enabled = $current.IsEnabled
            offscreen = $current.IsOffscreen
            patterns = $patterns; selected = $selectedState
        })
        $next = $walker.GetFirstChild($element)
        while ($null -ne $next -and ($queue.Count + $rows.Count) -lt 1000) {
            $queue.Enqueue($next)
            $next = $walker.GetNextSibling($next)
        }
        if ($null -ne $next) { throw 'Inventory traversal exceeds bounds.' }
    }
    if ($queue.Count -gt 0) { throw 'Inventory traversal exceeds bounds.' }
    Assert-InventoryIdentity
    $capturedAt = [DateTime]::UtcNow.ToString('o')
    if ($env:GITHUB_RUN_ID -cnotmatch '^\d{1,20}$' -or $env:GITHUB_SHA -cnotmatch '^[0-9a-f]{40}$') {
        throw 'Inventory workflow binding unavailable.'
    }
    # UI labels may contain profile details. Never print or persist plaintext.
    $plain = [Text.Encoding]::UTF8.GetBytes((@{ schema = 1; controls = $rows.ToArray()
        binding = @{requested_destination=$Destination; navigation_completion='unverified'; run_id=$env:GITHUB_RUN_ID
            workflow_source_commit=$env:GITHUB_SHA; observed_at_utc=$capturedAt
            probe_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()}
        selection = @{ method = $selection; direct_root_count = $windows.Count
            frame_root_count = $frameRootCount; matched_root_count = $matchedRootCount
            discovery_element_count = $discoveryCount; settings_pid = $settings[0].Id
            settings_start_ticks = $settingsStart; root_pid=$rootPid; root_start_ticks=$ownedRootStart; root_hwnd=$rootHandle
            selected_process_id = $selected.Current.ProcessId; session_id = $session }
    } |
        ConvertTo-Json -Depth 8 -Compress))
    if ($plain.Length -gt 4194304) { throw 'Inventory plaintext exceeds bounds.' }
    $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
    $tag = [byte[]]::new(16)
    $cipher = [byte[]]::new($plain.Length)
    $rsa = [Security.Cryptography.RSA]::Create()
    try {
        $rsa.ImportFromPem([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../md3/hosted-automation-public-v1.pem')))
        $wrapped = $rsa.Encrypt($key, [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [Security.Cryptography.AesGcm]::new($key, 16)
        try { $aes.Encrypt($nonce, $plain, $cipher, $tag) } finally { $aes.Dispose() }
        [IO.File]::WriteAllBytes((Join-Path $output 'settings-inventory.json.aesgcm'), $cipher)
        @{ schema = 2; protocol = 'display-scale-inventory-v2'
            algorithm = 'AES-256-GCM/RSA-OAEP-SHA256'; aad = 'none'
            recipient = 'hosted-automation-public-v1.pem'; wrapped_key = [Convert]::ToBase64String($wrapped)
            nonce = [Convert]::ToBase64String($nonce); tag = [Convert]::ToBase64String($tag)
            ciphertext_sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($cipher)).ToLowerInvariant()
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'envelope.json') -Encoding utf8
    } finally {
        [Array]::Clear($plain, 0, $plain.Length)
        [Array]::Clear($key, 0, $key.Length)
        $rsa.Dispose()
    }
    Write-Summary 'selectors_require_private_inventory_review' $rows.Count
} catch {
    # Exception strings can include provider labels or local paths.
    Write-Summary 'uia_inventory_or_encryption_failed'
}
exit 2
