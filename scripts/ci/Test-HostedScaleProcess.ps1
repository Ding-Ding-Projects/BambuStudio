[CmdletBinding()]
param([Parameter(Mandatory)][string] $OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or
    $env:RUNNER_OS -cne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'Lifecycle checks require disposable hosted Windows execution.'
}
$tempRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $output)) { throw 'Lifecycle output must be a new RUNNER_TEMP child.' }
[void](New-Item -ItemType Directory -Path $output)
$scratch = Join-Path $output 'private-children'
[void](New-Item -ItemType Directory -Path $scratch)
$pwsh = (Get-Process -Id $PID).Path
$cases = [Collections.Generic.List[object]]::new()
$stage = 'compile_actual_helper'
$cleanupVerified = $true
$success = $false
$descendantIdentity = Join-Path $scratch 'descendant.json'
$timer = [Diagnostics.Stopwatch]::StartNew()

function Require-Result([bool] $Condition) {
    if (-not $Condition) { throw 'Lifecycle assertion failed.' }
}
function Run-Child([string] $Script, [string[]] $Values, [int] $Seconds = 8) {
    return [HostedScaleProcess]::Run($pwsh, [string[]](@('-NoProfile','-File',$Script) + $Values), $Seconds, $true)
}
function Record-Pass([string] $Name) { $cases.Add(@{name=$Name; status='passed'}) }

try {
    # Compile and exercise the checked-in implementation, not a test copy.
    Add-Type -Path (Join-Path $PSScriptRoot 'HostedScaleProcess.cs')
    $stage = 'minimum_resolution_tuple_contract'
    # Load only these exact production function definitions. Do not dot-source
    # the supervisor, which would initialize Settings or change display state.
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $PSScriptRoot 'Invoke-HostedDisplayScale.ps1'),[ref]$tokens,[ref]$parseErrors)
    Require-Result ($parseErrors.Count -eq 0)
    foreach ($name in @('Test-NativeTuple','Test-UncertainInput')) {
        $definitions = @($ast.FindAll({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name
        }, $true))
        Require-Result ($definitions.Count -eq 1)
        . ([scriptblock]::Create($definitions[0].Extent.Text))
    }
    $tuple = @{resolution='1920x1080'; scope='minimum-resize'; viewport='measured-minimum'}
    Require-Result (-not (Test-NativeTuple $tuple 125 $true))
    Require-Result (-not (Test-NativeTuple $tuple 100 $false))
    $tuple.scope = 'menus'
    Require-Result (-not (Test-NativeTuple $tuple 100 $true))
    $tuple.scope = 'minimum-resize'; $tuple.viewport = '1000x600'
    Require-Result (-not (Test-NativeTuple $tuple 100 $true))
    $tuple.viewport = 'measured-minimum'
    Require-Result (Test-NativeTuple $tuple 100 $true)
    $tuple = @{resolution='unchanged'; scope='menus'; viewport='1200x800'}
    Require-Result (-not (Test-NativeTuple $tuple 100 $false))
    Require-Result (Test-NativeTuple $tuple 125 $false)
    Record-Pass $stage

    $stage = 'minimum_input_recovery_contract'
    $receiptOutput = $output
    try {
        $output = Join-Path $scratch 'input-state'
        [void](New-Item -ItemType Directory -Path $output)
        $nativeRequestPath = Join-Path $output 'request.json'
        [IO.File]::WriteAllText($nativeRequestPath,'{"fixture":1}')
        $requestDigest = (Get-FileHash -LiteralPath $nativeRequestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        Require-Result (-not (Test-UncertainInput)) # No product invocation yet.
        [IO.File]::WriteAllText((Join-Path $output 'native-input.started'),$requestDigest)
        Require-Result (Test-UncertainInput) # Job exit cannot clear this state.
        [IO.File]::WriteAllText((Join-Path $output 'native-input.restored'),('0' * 64))
        Require-Result (Test-UncertainInput)
        [IO.File]::WriteAllText((Join-Path $output 'native-input.restored'),$requestDigest)
        [IO.File]::WriteAllText($nativeRequestPath,'{"fixture":2}')
        Require-Result (Test-UncertainInput) # Evidence from another invocation.
        [IO.File]::WriteAllText($nativeRequestPath,'{"fixture":1}')
        Require-Result (-not (Test-UncertainInput))
        [IO.File]::WriteAllText((Join-Path $output 'native-input.started'),'')
        Require-Result (Test-UncertainInput)
    } finally { $output = $receiptOutput }
    Record-Pass $stage
    $stage = 'display_mode_public_layout_contract'
    Add-Type -Path (Join-Path $PSScriptRoot 'HostedDisplayMode.cs')
    function New-ModeContractFixture([uint16] $Size, [uint16] $Extra = 0, [uint32] $Fields = 0x207c00a0) {
        $bytes = [byte[]]::new(220)
        [BitConverter]::GetBytes($Size).CopyTo($bytes,68)
        [BitConverter]::GetBytes($Extra).CopyTo($bytes,70)
        [BitConverter]::GetBytes($Fields).CopyTo($bytes,72)
        [BitConverter]::GetBytes([uint32]32).CopyTo($bytes,168)
        [BitConverter]::GetBytes([uint32]1024).CopyTo($bytes,172)
        [BitConverter]::GetBytes([uint32]768).CopyTo($bytes,176)
        [BitConverter]::GetBytes([uint32]64).CopyTo($bytes,184)
        return ,$bytes
    }
    # Reject incomplete/unknown layouts and private data before proving both
    # supported public layouts. No native display API is called by this case.
    foreach ($invalid in @((New-ModeContractFixture 187), (New-ModeContractFixture 189),
        (New-ModeContractFixture 220 1), (New-ModeContractFixture 188 0 0x207c00a1),
        (New-ModeContractFixture 188 0 0x203c00a0))) {
        $rejected = $false
        try { [void][HostedDisplayMode]::Width($invalid) } catch { $rejected = $true }
        Require-Result $rejected
    }
    foreach ($publicSize in @(188,220)) {
        $valid = New-ModeContractFixture $publicSize
        $preserved = [Convert]::ToBase64String($valid)
        Require-Result ([HostedDisplayMode]::Width($valid) -eq 1024 -and [HostedDisplayMode]::Height($valid) -eq 768)
        Require-Result ([Convert]::ToBase64String($valid) -ceq $preserved -and [BitConverter]::ToUInt16($valid,68) -eq $publicSize)
    }
    Record-Pass $stage
    $writer = Join-Path $scratch 'writer.ps1'
    @'
param([int] $Size)
if ($Size -eq 0) { [Console]::Out.Write('{"ok":true}'); exit 0 }
# UTF-8 ASCII only: six prefix bytes, Size-8 content bytes, two suffix bytes.
[Console]::Out.Write('{"v":"' + ('a' * ($Size - 8)) + '"}')
'@ | Set-Content -LiteralPath $writer -Encoding utf8

    $stage = 'reject_oversized_output'
    $result = Run-Child $writer @('65537')
    Require-Result ($result.Terminated -and $result.Code -ne 0 -and $result.Output -eq '')
    Record-Pass $stage

    $stage = 'normal_output_after_overflow'
    $result = Run-Child $writer @('0')
    Require-Result ($result.Terminated -and $result.Code -eq 0 -and $result.Output -ceq '{"ok":true}')
    Record-Pass $stage

    $sleeper = Join-Path $scratch 'sleeper.ps1'
    @'
param([string] $Identity, [int] $ParentPid, [long] $ParentStart)
$self = Get-Process -Id $PID
@{pid=$PID; start_ticks=$self.StartTime.ToUniversalTime().Ticks} | ConvertTo-Json |
    Set-Content -LiteralPath ($Identity + '.tmp') -Encoding utf8
Move-Item -LiteralPath ($Identity + '.tmp') -Destination $Identity
$deadline = [DateTime]::UtcNow.AddSeconds(3)
do {
    $parent = Get-Process -Id $ParentPid -ErrorAction SilentlyContinue
    if ($null -eq $parent -or $parent.StartTime.ToUniversalTime().Ticks -ne $ParentStart) {
        [IO.File]::WriteAllText($Identity + '.parent-exited', 'observed')
        break
    }
    Start-Sleep -Milliseconds 20
} while ([DateTime]::UtcNow -lt $deadline)
Start-Sleep -Seconds 30
'@ | Set-Content -LiteralPath $sleeper -Encoding utf8
    $spawner = Join-Path $scratch 'spawner.ps1'
    @'
param([string] $Sleeper, [string] $Identity)
$start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
foreach ($arg in @('-NoProfile','-File',$Sleeper,'-Identity',$Identity,'-ParentPid',"$PID",
    '-ParentStart',"$((Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks)")) {
    [void]$start.ArgumentList.Add($arg)
}
$child = [Diagnostics.Process]::Start($start)
$deadline = [DateTime]::UtcNow.AddSeconds(3)
while (-not (Test-Path -LiteralPath $Identity) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 20
}
if (-not (Test-Path -LiteralPath $Identity)) { exit 2 }
[Console]::Out.Write('{"ok":true}')
$child.Dispose()
# Exit normally while the owned descendant continues holding its inherited
# streams. Direct process exit and a valid JSON body must not prove completion.
exit 0
'@ | Set-Content -LiteralPath $spawner -Encoding utf8

    $stage = 'terminate_descendant_after_parent_exit'
    $result = Run-Child $spawner @('-Sleeper',$sleeper,'-Identity',$descendantIdentity) 5
    Require-Result ($result.Terminated -and $result.Code -ne 0 -and $result.Output -eq '' -and
        (Test-Path -LiteralPath $descendantIdentity) -and
        (Test-Path -LiteralPath ($descendantIdentity + '.parent-exited')))
    $identity = Get-Content -LiteralPath $descendantIdentity -Raw | ConvertFrom-Json
    $live = Get-Process -Id ([int]$identity.pid) -ErrorAction SilentlyContinue
    Require-Result ($null -eq $live -or $live.StartTime.ToUniversalTime().Ticks -ne $identity.start_ticks)
    Record-Pass $stage

    $stage = 'normal_output_after_descendant_timeout'
    $result = Run-Child $writer @('0')
    Require-Result ($result.Terminated -and $result.Code -eq 0 -and $result.Output -ceq '{"ok":true}')
    Record-Pass $stage

    $stage = 'accept_exact_output_limit'
    $result = Run-Child $writer @('65536')
    Require-Result ($result.Terminated -and $result.Code -eq 0 -and
        [Text.Encoding]::UTF8.GetByteCount($result.Output) -eq 65536)
    $json = $result.Output | ConvertFrom-Json
    Require-Result ($json.v.Length -eq 65528)
    Record-Pass $stage

    $stage = 'query_only_named_job_membership'
    $membership = Join-Path $scratch 'membership.ps1'
    @'
param([string] $JobName)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class MembershipCheck {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr OpenJobObject(uint rights,bool inherit,string name);
    [DllImport("kernel32.dll")] public static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] public static extern bool IsProcessInJob(IntPtr process,IntPtr job,out bool member);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
"@
$query = [MembershipCheck]::OpenJobObject(4,$false,$JobName)
if ($query -eq [IntPtr]::Zero) { exit 3 }
try {
    [bool]$member = $false
    if (-not [MembershipCheck]::IsProcessInJob([MembershipCheck]::GetCurrentProcess(),$query,[ref]$member) -or -not $member) { exit 4 }
} finally { [void][MembershipCheck]::CloseHandle($query) }
foreach ($right in @(0x40000,0x20000,0x2,0x1,0x8)) {
    $unexpected = [MembershipCheck]::OpenJobObject($right,$false,$JobName)
    $reason = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($unexpected -ne [IntPtr]::Zero) {
        [void][MembershipCheck]::CloseHandle($unexpected)
        exit 5
    }
    if ($reason -ne 5) { exit 6 }
}
[Console]::Out.Write('{"ok":true}')
exit 0
'@ | Set-Content -LiteralPath $membership -Encoding utf8
    $jobName = 'Local\BambuNativeScale-' + [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant()
    $result = [HostedScaleProcess]::RunNamed($pwsh,
        [string[]]@('-NoProfile','-File',$membership,'-JobName',$jobName), 10, $true, $jobName)
    Require-Result ($result.Terminated -and $result.Code -eq 0 -and $result.Output -ceq '{"ok":true}')
    Record-Pass $stage
    $success = $cases.Count -eq 9
} catch {
    # Neither exception text nor benign child payloads enter public logs.
    $cases.Add(@{name=$stage; status='failed'})
} finally {
    # A broken containment implementation must not leave a known test child.
    # Match both PID and start time; never kill by a generic process name.
    if (Test-Path -LiteralPath $descendantIdentity) {
        try {
            $identity = Get-Content -LiteralPath $descendantIdentity -Raw | ConvertFrom-Json
            $live = Get-Process -Id ([int]$identity.pid) -ErrorAction SilentlyContinue
            if ($null -ne $live -and $live.StartTime.ToUniversalTime().Ticks -eq $identity.start_ticks) {
                $success = $false
                $live.Kill($true)
                $cleanupVerified = $live.WaitForExit(5000)
            }
        } catch { $cleanupVerified = $false; $success = $false }
    }
    $passed = @($cases | Where-Object status -eq 'passed').Count
    @{schema=1; status=$(if ($success -and $cleanupVerified) {'passed'} else {'failed'})
      passed=$passed; expected=9; cases=$cases.ToArray(); cleanup_verified=$cleanupVerified
      elapsed_ms=$timer.ElapsedMilliseconds; settings_mutated=$false
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
}
if ($success -and $cleanupVerified) { Write-Host 'Hosted scale lifecycle checks passed: 9/9'; exit 0 }
Write-Host 'Hosted scale lifecycle checks failed; inspect the fixed receipt.'
exit 2
