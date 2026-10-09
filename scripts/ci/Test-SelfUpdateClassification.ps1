<#
Runs Get-SelfUpdateClassification of Diagnose-SelfUpdate.ps1 on synthetic facts. Nothing is
installed, started or downloaded, so it runs anywhere pwsh does: the self-update workflow runs it
before the diagnostic, and ui-md3/tests/self-update-diagnostic.test.mjs runs it where pwsh is
installed.

The classifier and the helpers it calls are taken from the parsed scripts, as the diagnostic takes
the installer first-run helpers; nothing else of either script runs. The cases pin the rule that a
newer app-<version> folder with its bambu-studio.exe counts as staged only when Update.exe is shown
to have finished it. Squirrel's Update.exe extracts the package straight into that folder and leaves
what it wrote when it fails part of the way, so the folder alone is no proof.

Prints one line per case and throws when any case gets another classification or basis.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$loaded = [ordered]@{
    'Diagnose-SelfUpdate.ps1' = @('ConvertTo-VersionParts', 'Compare-VersionParts', 'ConvertFrom-ReleasesRow',
                                  'Get-UpdateLogFacts', 'Get-SelfUpdateClassification')
    'Diagnose-InstallerFirstRun.ps1' = @('ConvertTo-UtcTime', 'Format-ExitCode')
}
foreach ($source in $loaded.Keys) {
    $tokens = $null
    $parseErrors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $source), [ref] $tokens, [ref] $parseErrors)
    if ($parseErrors.Count -ne 0) { throw "$source does not parse: $($parseErrors[0].Message)" }
    foreach ($name in $loaded[$source]) {
        $definition = @($ast.EndBlock.Statements | Where-Object {
            $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -eq $name })
        if ($definition.Count -ne 1) { throw "$source does not define $name exactly once." }
        . ([scriptblock]::Create($definition[0].Extent.Text))
    }
}

$installedVersion = '2.8.4835'
$stagedVersion = '2.8.4848'
$stagedFolder = "app-$stagedVersion"
$newPackageRow = "$('A' * 40) BambuStudioMD3-$stagedVersion-full.nupkg 187654321"
$oldPackageRow = "$('B' * 40) BambuStudioMD3-$installedVersion-full.nupkg 187650000"

# The application's update lines, as GUI_App writes them, ending with the exit-code line given.
function New-LogLines {
    param([AllowNull()][string] $ExitLine)
    $lines = @(
        'studio_1.log: [info] check new version: md3-v232 published 2026-10-08T10:00:00Z, built 2026-10-01T10:00:00Z, newer than this build',
        'studio_1.log: [info] auto update: updating to md3-v232 (Bambu Studio) through Update.exe',
        'studio_1.log: [info] auto update: Update.exe started, waiting for it to finish')
    if ($ExitLine) { $lines += "studio_1.log: [info] auto update: $ExitLine" }
    return , $lines
}

# One Update.exe --update run started by the application, shaped as Get-UpdateExeRuns returns it.
function New-UpdateRun {
    param([AllowNull()][object] $ExitCode, [switch] $Alive)
    return [pscustomobject][ordered]@{
        pid = 4100; ppid = 4000; role = 'update'; command_line = '"Update.exe" --update=https://example.invalid/feed'
        started_by_application = $true; started = '2026-10-09T12:01:00.0000000Z'
        exited = if ($Alive) { $null } else { '2026-10-09T12:03:00.0000000Z' }
        exit_code = if ($Alive) { $null } else { $ExitCode }
        exit_code_hex = if ($Alive) { $null } else { Format-ExitCode $ExitCode }
        polls = 24; alive_at_end = [bool] $Alive
    }
}

function New-Verdict {
    param([string] $Classification = 'started_visible')
    return [pscustomobject][ordered]@{ classification = $Classification; basis = "synthetic $Classification"; crash = $null }
}

function New-Notifications {
    param([switch] $Ready, [switch] $Failure)
    return [pscustomobject][ordered]@{
        found = $true; error = $null
        ready = @(if ($Ready) { [pscustomobject][ordered]@{ type = 'AppUpdateReady'; text = 'md3-v232 is ready' } })
        failure = @(if ($Failure) { [pscustomobject][ordered]@{ type = 'CustomNotification'; text = 'md3-v232 could not be installed' } })
    }
}

# The facts of an update that finished: the candidate folder, Update.exe exiting with 0, the
# application's log line, local RELEASES listing the new package, every packaged file in place, the
# ready banner, and the next start running the new folder. Each case changes what it tests.
function New-FinishedFacts {
    return [ordered]@{
        installed_version = $installedVersion
        feed = [ordered]@{ tag = 'md3-v232'; version = $stagedVersion; newer = $true }
        candidate = [pscustomobject][ordered]@{
            name = $stagedFolder; version = $stagedVersion; has_executable = $true
            product_version = $stagedVersion; created = '2026-10-09T12:02:00.0000000Z'
        }
        partial = $null
        staged_files = [pscustomobject][ordered]@{
            folder = $stagedFolder; package = "BambuStudioMD3-$stagedVersion-full.nupkg"; package_files = 1200
            missing_files = 0; differing_files = 0; problems = @(); error = $null
        }
        squirrel = [pscustomobject][ordered]@{
            packages = @(); local_releases = @($newPackageRow); releases_lines = @(); log_lines = @(); log_lines_total = 0
        }
        update_runs = @(New-UpdateRun -ExitCode 0)
        update_exe_crashes = @()
        updated_events = @("$stagedFolder\bambu-studio.exe --squirrel-updated $stagedVersion")
        log = Get-UpdateLogFacts -Lines (New-LogLines 'Update.exe exited with code 0')
        log_decoded = $true
        notifications = New-Notifications -Ready
        observe_verdict = New-Verdict
        next_start_verdict = New-Verdict
        next_start = [ordered]@{ folders_run = @($stagedFolder); classification = 'started_visible' }
        close_requested = '2026-10-09T12:10:00.0000000Z'
        setup_wizard_open_at_end = $false
    }
}

# The facts of a run in which no newer folder appeared.
function New-UnstagedFacts {
    $facts = New-FinishedFacts
    $facts.candidate = $null
    $facts.staged_files = $null
    $facts.squirrel.local_releases = @($oldPackageRow)
    $facts.updated_events = @()
    $facts.notifications = New-Notifications
    $facts.next_start_verdict = $null
    $facts.next_start = $null
    return $facts
}

$failed = [System.Collections.Generic.List[string]]::new()
$count = 0
function Test-Case {
    param(
        [Parameter(Mandatory)][string] $Name,
        [Parameter(Mandatory)][System.Collections.IDictionary] $Facts,
        [Parameter(Mandatory)][string] $Expected,
        [string[]] $BasisPatterns = @()
    )
    $script:count++
    $outcome = Get-SelfUpdateClassification -Facts $Facts
    $problems = @()
    if ($outcome.classification -ne $Expected) { $problems += "classified $($outcome.classification), expected $Expected" }
    foreach ($pattern in $BasisPatterns) {
        if ([string] $outcome.basis -notmatch $pattern) { $problems += "the basis does not match '$pattern'" }
    }
    if ($problems.Count -gt 0) {
        $script:failed.Add("$Name`: $($problems -join '; ')`n    basis: $($outcome.basis)")
        Write-Host "FAIL $Name -> $($outcome.classification): $($outcome.basis)"
    }
    else {
        Write-Host "ok   $Name -> $($outcome.classification)"
    }
}

# An update Update.exe finished is the only staged one.
Test-Case 'an update Update.exe finished' (New-FinishedFacts) 'updated_staged' @(
    "^$stagedFolder was staged", 'exited with 0x00000000', 'packages\\RELEASES lists BambuStudioMD3-2\.8\.4848-full\.nupkg',
    'all 1200 files', "the next start ran $stagedFolder\\bambu-studio\.exe")

# The application's own log line stands in for an exit code the polls did not capture.
$facts = New-FinishedFacts
$facts.update_runs = @(New-UpdateRun -ExitCode $null)
Test-Case 'exit code not captured, the application logged code 0' $facts 'updated_staged' @('ended, exit code not captured', 'exited with code 0')

# A staged folder and a non-zero Update.exe exit are never an update, whatever else looks right.
$facts = New-FinishedFacts
$facts.update_runs = @(New-UpdateRun -ExitCode -1)
Test-Case 'a staged folder and Update.exe exiting with 0xFFFFFFFF' $facts 'update_failed' @(
    'exited with 0xFFFFFFFF', "partial staging: $stagedFolder holds a bambu-studio\.exe")

$facts = New-FinishedFacts
$facts.update_runs = @(New-UpdateRun -ExitCode 1)
$facts.log = Get-UpdateLogFacts -Lines (New-LogLines 'Update.exe exited with code 1')
Test-Case 'a staged folder and Update.exe exiting with 1' $facts 'update_failed' @(
    'exited with 0x00000001', 'the application logged that Update.exe exited with code 1', 'partial staging')

# The finding's case: Update.exe extracts bambu-studio.exe, then fails, so local RELEASES still names
# only the installed package, files are missing, the application logs the exit code and shows its
# failure notice instead of the ready banner.
$facts = New-FinishedFacts
$facts.update_runs = @(New-UpdateRun -ExitCode 4294967295)
$facts.log = Get-UpdateLogFacts -Lines (New-LogLines 'Update.exe exited with code 4294967295')
$facts.squirrel.local_releases = @($oldPackageRow)
$facts.staged_files.missing_files = 412
$facts.staged_files.problems = @("412 of the package's 1200 files are missing from $stagedFolder")
$facts.notifications = New-Notifications -Failure
$facts.updated_events = @()
Test-Case 'Update.exe failing part of the way through the extraction' $facts 'update_failed' @(
    'exited with 0xFFFFFFFF', 'the notification history records the failure notice',
    'partial staging: .*packages\\RELEASES does not list the full package of 2\.8\.4848', '412 of the package')

$facts = New-FinishedFacts
$facts.update_exe_crashes = @([pscustomobject][ordered]@{
    time = '2026-10-09T12:02:30.0000000Z'; provider = '.NET Runtime'; id = 1026; event_name = $null
    exception_code = 'e0434352'; module = $null; report_id = $null })
Test-Case 'a staged folder and an Update.exe crash entry' $facts 'update_failed' @('\.NET Runtime 1026 entry for Update\.exe', 'partial staging')

$facts = New-FinishedFacts
$facts.notifications = New-Notifications -Failure
Test-Case 'a staged folder and the failure notice' $facts 'update_failed' @('the failure notice', 'because of the failures above')

$facts = New-FinishedFacts
$facts.update_runs = @(New-UpdateRun -ExitCode $null)
$facts.log = Get-UpdateLogFacts -Lines (New-LogLines $null)
Test-Case 'a staged folder and no exit 0 seen anywhere' $facts 'update_failed' @('no Update\.exe --update run was seen to exit with 0')

$facts = New-FinishedFacts
$facts.log = Get-UpdateLogFacts -Lines ((New-LogLines 'Update.exe exited with code 0') +
    'studio_1.log: [info] auto update: Update.exe found nothing newer to install')
Test-Case 'a staged folder although the application found nothing newer' $facts 'update_failed' @('found nothing newer to install')

$facts = New-FinishedFacts
$facts.squirrel.local_releases = @($oldPackageRow)
Test-Case 'a staged folder that local RELEASES does not list' $facts 'update_failed' @('packages\\RELEASES does not list the full package of 2\.8\.4848')

$facts = New-FinishedFacts
$facts.staged_files.problems = @("its bambu-studio.exe differs from the package's (SHA-256 $('1' * 64), the package's $('2' * 64))")
Test-Case 'a staged bambu-studio.exe that is not the package one' $facts 'update_failed' @('bambu-studio\.exe differs from the package')

$facts = New-FinishedFacts
$facts.staged_files = $null
Test-Case 'a staged folder never compared with the package' $facts 'update_failed' @('its files were not compared with the package')

$facts = New-FinishedFacts
$facts.next_start = [ordered]@{ folders_run = @("app-$installedVersion"); classification = 'started_visible' }
Test-Case 'the next start ran the installed folder' $facts 'update_failed' @("the next start ran app-2\.8\.4835, not $stagedFolder")

$facts = New-FinishedFacts
$facts.next_start = [ordered]@{ folders_run = @(); classification = 'started_exited' }
Test-Case 'the next start ran no app folder' $facts 'update_failed' @('the next start ran no app-<version> executable')

# A crash still wins over everything else.
$facts = New-FinishedFacts
$facts.next_start_verdict = New-Verdict 'started_crashed'
Test-Case 'the staged version crashing at the next start' $facts 'app_crashed' @('^at the next start: ')

# Without a candidate the earlier rules hold.
$facts = New-UnstagedFacts
$facts.log = Get-UpdateLogFacts -Lines ((New-LogLines 'Update.exe exited with code 0') +
    'studio_1.log: [info] auto update: Update.exe found nothing newer to install')
Test-Case 'Update.exe exiting with 0 and staging nothing' $facts 'update_failed' @('staged nothing newer than app-2\.8\.4835')

$facts = New-UnstagedFacts
$facts.update_runs = @(New-UpdateRun -Alive)
$facts.log = Get-UpdateLogFacts -Lines (New-LogLines $null)
$facts.partial = [pscustomobject][ordered]@{
    name = $stagedFolder; version = $stagedVersion; has_executable = $false; product_version = $null; created = '2026-10-09T12:02:00.0000000Z' }
Test-Case 'Update.exe still at work' $facts 'update_offered_not_staged' @('still running', "$stagedFolder was being staged")

$facts = New-UnstagedFacts
$facts.feed = [ordered]@{ tag = 'md3-v231'; version = $installedVersion; newer = $false }
$facts.update_runs = @()
$facts.log = Get-UpdateLogFacts -Lines @(
    'studio_1.log: [info] check new version: md3-v231 published 2026-10-01T10:00:00Z, built 2026-10-01T10:00:00Z, not newer than this build',
    'studio_1.log: [info] check new version: nothing to do for md3-v231')
Test-Case 'nothing newer to update to' $facts 'no_update_seen' @('nothing to do for md3-v231', 'not newer than app-2\.8\.4835')

if ($failed.Count -gt 0) {
    throw "$($failed.Count) of $count self-update classification cases failed:`n$($failed -join "`n")"
}
Write-Host "All $count self-update classification cases passed."
