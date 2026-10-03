[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $OutputDirectory,
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
        restoration = 'not_required_no_display_mutation'
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'capability.json') -Encoding utf8
}

if (-not $InventoryWorker) {
    if (Test-Path -LiteralPath $output) { throw 'Output directory must be new.' }
    [void](New-Item -ItemType Directory -Path $output)
    # Isolate UIA calls in a killable process: providers can hang inside native calls.
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    foreach ($argument in @('-NoProfile', '-File', $PSCommandPath, '-OutputDirectory', $output, '-InventoryWorker')) {
        [void]$start.ArgumentList.Add($argument)
    }
    $child = [Diagnostics.Process]::Start($start)
    try {
        if (-not $child.WaitForExit($TimeoutSeconds * 1000)) {
            $child.Kill($true)
            [void]$child.WaitForExit(5000)
            Write-Summary 'uia_inventory_timeout'
            exit 2
        }
        if (-not (Test-Path -LiteralPath (Join-Path $output 'capability.json'))) {
            Write-Summary 'inventory_worker_failed'
        }
    } finally { $child.Dispose() }
    # Inventory is never a successful scale-provisioning verdict.
    exit 2
}

try {
    $session = (Get-Process -Id $PID).SessionId
    $settings = @(Get-Process -Name SystemSettings -ErrorAction SilentlyContinue |
        Where-Object { $_.SessionId -eq $session })
    if ($settings.Count -ne 1) { Write-Summary 'settings_process_missing_or_ambiguous'; exit 2 }
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $condition = [Windows.Automation.PropertyCondition]::new(
        [Windows.Automation.AutomationElement]::ProcessIdProperty, [int]$settings[0].Id)
    $windows = [Windows.Automation.AutomationElement]::RootElement.FindAll(
        [Windows.Automation.TreeScope]::Children, $condition)
    if ($windows.Count -ne 1) { Write-Summary 'settings_window_missing_or_ambiguous'; exit 2 }
    $rows = [Collections.Generic.List[object]]::new()
    $queue = [Collections.Generic.Queue[object]]::new()
    $queue.Enqueue($windows[0])
    $walker = [Windows.Automation.TreeWalker]::ControlViewWalker
    while ($queue.Count -gt 0 -and $rows.Count -lt 1000) {
        $element = $queue.Dequeue()
        $current = $element.Current
        $rows.Add([ordered]@{
            name = $current.Name; automation_id = $current.AutomationId
            type = $current.ControlType.ProgrammaticName; enabled = $current.IsEnabled
            offscreen = $current.IsOffscreen
            patterns = @($element.GetSupportedPatterns() | ForEach-Object { $_.ProgrammaticName })
        })
        $next = $walker.GetFirstChild($element)
        while ($null -ne $next -and ($queue.Count + $rows.Count) -lt 1000) {
            $queue.Enqueue($next)
            $next = $walker.GetNextSibling($next)
        }
    }
    # UI labels may contain profile details. Never print or persist plaintext.
    $plain = [Text.Encoding]::UTF8.GetBytes((@{ schema = 1; controls = $rows.ToArray() } |
        ConvertTo-Json -Depth 8 -Compress))
    $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
    $tag = [byte[]]::new(16)
    $cipher = [byte[]]::new($plain.Length)
    $rsa = [Security.Cryptography.RSA]::Create()
    try {
        $rsa.ImportFromPem([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../md3/hosted-gui-public-v2.pem')))
        $wrapped = $rsa.Encrypt($key, [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [Security.Cryptography.AesGcm]::new($key, 16)
        try { $aes.Encrypt($nonce, $plain, $cipher, $tag) } finally { $aes.Dispose() }
        [IO.File]::WriteAllBytes((Join-Path $output 'settings-inventory.json.aesgcm'), $cipher)
        @{ schema = 1; algorithm = 'AES-256-GCM/RSA-OAEP-SHA256'; aad = 'none'
            recipient = 'hosted-gui-public-v2.pem'; wrapped_key = [Convert]::ToBase64String($wrapped)
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
