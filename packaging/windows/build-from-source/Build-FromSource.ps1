<#
.SYNOPSIS
    From-source build orchestrator for the Bambu Studio MD3 Windows installer.
.DESCRIPTION
    Launched asynchronously by BambuStudioMD3.nsi's non-closable build progress page.
    Pipeline: pre-flight -> toolchain bootstrap -> clone -> opencode install ->
    build (deps -> app -> install target) with a bounded opencode repair loop ->
    stage payload + ownership manifest.

    Progress is reported plugin-free through two files in -SessionDir:
      * build.log   - append-only UTF-16LE transcript the NSIS poller tails.
      * status.json - atomically replaced UTF-16LE status object (one field per line
                      so the NSIS poller can read it without a JSON plugin).

    Exit-code contract (consumed by NSIS):
        0  payload staged + manifest written
        10 toolchain bootstrap failed
        11 clone/checkout failed
        12 opencode install failed
        20 build failed after the maximum repair cycles
        30 pre-flight (net/disk/writable) failed
        40 unexpected error (trap)
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $SessionDir,
    [Parameter(Mandatory = $true)][string] $CloneUrl,
    [Parameter(Mandatory = $true)][string] $Tag,
    [string] $LanguageMode = 'en',
    [Parameter(Mandatory = $true)][string] $PayloadOut
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:SessionDir = $SessionDir
$script:LogPath = Join-Path $SessionDir 'build.log'
$script:StatusPath = Join-Path $SessionDir 'status.json'
$script:MaxAttempts = 5

# UTF-16LE encodings: BOM for the first log write + every status rewrite so the
# Unicode NSIS installer detects and reads them natively.
$script:Utf16Bom = New-Object System.Text.UnicodeEncoding($false, $true)
$script:Utf16NoBom = New-Object System.Text.UnicodeEncoding($false, $false)

function Initialize-Log {
    if (-not (Test-Path -LiteralPath $SessionDir)) {
        New-Item -ItemType Directory -Path $SessionDir -Force | Out-Null
    }
    # Seed build.log with a UTF-16LE BOM once; later writes append without a BOM.
    [System.IO.File]::WriteAllText($script:LogPath, "", $script:Utf16Bom)
}

function Write-BuildLog {
    param([Parameter(Mandatory = $true)][string] $Message)
    $line = ('[{0}] {1}{2}' -f (Get-Date).ToString('u'), $Message, "`r`n")
    [System.IO.File]::AppendAllText($script:LogPath, $line, $script:Utf16NoBom)
}

function Set-Status {
    param(
        [ValidateSet('running', 'success', 'failed', 'fatal')][string] $State = 'running',
        [string] $Phase = 'preflight',
        [string] $Step = '',
        [int] $Attempt = 0,
        [int] $PctHint = 0,
        [System.Object] $ExitCode = $null
    )

    # Keep step text free of quotes so the line-based NSIS reader stays trivial.
    $safeStep = ($Step -replace '"', "'")
    $exit = if ($null -eq $ExitCode) { 'null' } else { [string]([int]$ExitCode) }

    # One field per line; still valid JSON. NSIS matches on the leading key token.
    $json = @"
{
"schema":1,
"state":"$State",
"phase":"$Phase",
"step":"$safeStep",
"attempt":$Attempt,
"maxAttempts":$($script:MaxAttempts),
"pctHint":$PctHint,
"exitCode":$exit,
"updatedUtc":"$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
}
"@
    $tmp = "$($script:StatusPath).tmp"
    [System.IO.File]::WriteAllText($tmp, $json, $script:Utf16Bom)
    Move-Item -LiteralPath $tmp -Destination $script:StatusPath -Force
}

function Exit-Build {
    param([int] $Code, [string] $Phase, [string] $Step)
    $state = if ($Code -eq 0) { 'success' } elseif ($Code -eq 40) { 'fatal' } else { 'failed' }
    Set-Status -State $state -Phase $Phase -Step $Step -ExitCode $Code
    Write-BuildLog "EXIT $Code ($Step)"
    exit $Code
}

function Get-LogTail {
    param([int] $Lines = 200)
    if (-not (Test-Path -LiteralPath $script:LogPath)) { return '' }
    return (Get-Content -LiteralPath $script:LogPath -Tail $Lines -ErrorAction SilentlyContinue) -join "`r`n"
}

function Write-Manifest {
    param(
        [Parameter(Mandatory = $true)][string] $PayloadDir,
        [Parameter(Mandatory = $true)][string] $OutFile
    )
    # Files first (F|relpath), then directories deepest-first (D|relpath). The
    # pipe character is invalid in Windows path names, so it is a safe delimiter.
    $root = (Get-Item -LiteralPath $PayloadDir).FullName.TrimEnd('\')
    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force | ForEach-Object {
        'F|' + [System.IO.Path]::GetRelativePath($root, $_.FullName)
    })
    $dirs = @(Get-ChildItem -LiteralPath $root -Recurse -Directory -Force |
        Sort-Object { ($_.FullName -split '\\').Count } -Descending | ForEach-Object {
            'D|' + [System.IO.Path]::GetRelativePath($root, $_.FullName)
        })
    $lines = $files + $dirs
    [System.IO.File]::WriteAllLines($OutFile, [string[]]$lines, $script:Utf16Bom)
}

function Test-Preflight {
    Set-Status -State 'running' -Phase 'preflight' -Step 'Checking network, disk and permissions' -PctHint 2
    Write-BuildLog 'Pre-flight checks starting.'

    # Session directory writable.
    try {
        $probe = Join-Path $SessionDir '.write-probe'
        Set-Content -LiteralPath $probe -Value 'ok' -Encoding ascii
        Remove-Item -LiteralPath $probe -Force
    } catch {
        Write-BuildLog "Session directory is not writable: $($_.Exception.Message)"
        return $false
    }

    # Free space >= ~40 GB on the session drive.
    try {
        $drive = [System.IO.Path]::GetPathRoot((Resolve-Path -LiteralPath $SessionDir).Path)
        $info = New-Object System.IO.DriveInfo($drive)
        $freeGb = [math]::Round($info.AvailableFreeSpace / 1GB, 1)
        Write-BuildLog "Free space on ${drive}: $freeGb GB."
        if ($info.AvailableFreeSpace -lt (40GB)) {
            Write-BuildLog 'Insufficient free disk space (need ~40 GB).'
            return $false
        }
    } catch {
        Write-BuildLog "Could not determine free disk space: $($_.Exception.Message)"
        return $false
    }

    # Internet reachable (github over 443).
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $iar = $client.BeginConnect('github.com', 443, $null, $null)
        $ok = $iar.AsyncWaitHandle.WaitOne(8000, $false)
        if ($ok -and $client.Connected) {
            $client.EndConnect($iar)
            $client.Close()
        } else {
            $client.Close()
            Write-BuildLog 'No internet connection to github.com:443.'
            return $false
        }
    } catch {
        Write-BuildLog "Internet check failed: $($_.Exception.Message)"
        return $false
    }

    Write-BuildLog 'Pre-flight checks passed.'
    return $true
}

function Invoke-BuildStep {
    <#
    Runs one build phase in the clone directory. Returns $true on success.
    On failure the step name + last ~200 log lines are captured for a repair prompt.
    #>
    param(
        [Parameter(Mandatory = $true)][string] $CloneDir,
        [Parameter(Mandatory = $true)][string] $Phase,
        [Parameter(Mandatory = $true)][string] $Label,
        [Parameter(Mandatory = $true)][scriptblock] $Action,
        [int] $Attempt,
        [int] $PctHint
    )
    Set-Status -State 'running' -Phase $Phase -Step $Label -Attempt $Attempt -PctHint $PctHint
    Write-BuildLog "STEP $Phase : $Label (attempt $Attempt)"
    try {
        $prev = Get-Location
        Set-Location -LiteralPath $CloneDir
        try {
            & $Action *>&1 | ForEach-Object {
                [System.IO.File]::AppendAllText($script:LogPath, ($_.ToString() + "`r`n"), $script:Utf16NoBom)
            }
        } finally {
            Set-Location -LiteralPath $prev
        }
        if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
            throw "Command exited with code $LASTEXITCODE."
        }
        return $true
    } catch {
        Write-BuildLog "STEP FAILED $Phase : $($_.Exception.Message)"
        return $false
    }
}

# --------------------------------------------------------------------------- #

try {
    Initialize-Log
    Write-BuildLog "From-source build starting. Session=$SessionDir Tag=$Tag Url=$CloneUrl"

    . (Join-Path $PSScriptRoot 'Toolchain.ps1')
    . (Join-Path $PSScriptRoot 'Opencode.ps1')

    $toolsDir = Join-Path $SessionDir 'tools'
    $srcDir = Join-Path $SessionDir 'src'
    $depsDir = Join-Path $SessionDir 'deps'

    # 1. Pre-flight
    if (-not (Test-Preflight)) {
        Exit-Build -Code 30 -Phase 'preflight' -Step 'Pre-flight checks failed'
    }

    # 2. Toolchain bootstrap
    Set-Status -State 'running' -Phase 'toolchain' -Step 'Installing developer tools' -PctHint 6
    try {
        Initialize-Toolchain -WorkDir $toolsDir
    } catch {
        Write-BuildLog "Toolchain bootstrap failed: $($_.Exception.Message)"
        Exit-Build -Code 10 -Phase 'toolchain' -Step 'Toolchain bootstrap failed'
    }

    # 3. Clone + checkout
    Set-Status -State 'running' -Phase 'clone' -Step 'Downloading source code' -PctHint 18
    try {
        if (Test-Path -LiteralPath $srcDir) {
            Remove-Item -LiteralPath $srcDir -Recurse -Force
        }
        & git clone $CloneUrl $srcDir 2>&1 | ForEach-Object { Write-BuildLog $_ }
        if ($LASTEXITCODE -ne 0) { throw "git clone exited $LASTEXITCODE." }
        Push-Location $srcDir
        try {
            & git checkout $Tag 2>&1 | ForEach-Object { Write-BuildLog $_ }
            if ($LASTEXITCODE -ne 0) { throw "git checkout $Tag exited $LASTEXITCODE." }
        } finally {
            Pop-Location
        }
    } catch {
        Write-BuildLog "Clone/checkout failed: $($_.Exception.Message)"
        Exit-Build -Code 11 -Phase 'clone' -Step 'Clone or checkout failed'
    }

    # 4. opencode install + project-local blanket-allow config
    Set-Status -State 'running' -Phase 'opencode-install' -Step 'Installing the repair assistant' -PctHint 24
    try {
        Install-Opencode
    } catch {
        Write-BuildLog "opencode install failed: $($_.Exception.Message)"
        Exit-Build -Code 12 -Phase 'opencode-install' -Step 'opencode install failed'
    }
    New-OpencodeAllowConfig -CloneDir $srcDir | Out-Null
    Write-BuildLog 'Wrote project-local opencode.json (blanket allow, scoped to the clone).'

    # 5. Build with a bounded opencode repair loop.
    $installDir = $PayloadOut
    $phases = @(
        @{ Phase = 'deps';           Label = 'Compiling dependencies';   Pct = 40; Action = { & cmd /c "build_win.bat -d `"$depsDir`" -s deps" } },
        @{ Phase = 'app';            Label = 'Compiling the application'; Pct = 70; Action = { & cmd /c "build_win.bat -s app" } },
        @{ Phase = 'install-target'; Label = 'Staging the payload';       Pct = 88; Action = {
                & cmake --build build --target install --config Release "-DCMAKE_INSTALL_PREFIX=$installDir"
            } }
    )

    $attempt = 0
    foreach ($p in $phases) {
        while ($true) {
            $ok = Invoke-BuildStep -CloneDir $srcDir -Phase $p.Phase -Label $p.Label -Action $p.Action -Attempt $attempt -PctHint $p.Pct
            if ($ok) { break }

            if ($attempt -ge $script:MaxAttempts) {
                Write-BuildLog "Build failed after $($script:MaxAttempts) repair cycles at phase $($p.Phase)."
                Exit-Build -Code 20 -Phase $p.Phase -Step "Build failed after $($script:MaxAttempts) repair attempts"
            }

            $attempt++
            $tail = Get-LogTail -Lines 200
            $prompt = @"
The Bambu Studio Windows build failed at step $($p.Phase) in $srcDir. Log tail:
$tail

Diagnose and fix the cause by editing files in this repository, then stop. Do not run the build yourself.
"@
            Set-Status -State 'running' -Phase 'repair' -Step "Repairing the build (attempt $attempt)" -Attempt $attempt -PctHint $p.Pct
            Write-BuildLog "Invoking opencode repair, attempt $attempt/$($script:MaxAttempts)."
            $rc = Invoke-OpencodeRepair -CloneDir $srcDir -Prompt $prompt
            Write-BuildLog "opencode repair returned $rc; re-running phase $($p.Phase)."
            # Loop re-runs the same phase.
        }
    }

    # 6. Stage verification + ownership manifest.
    Set-Status -State 'running' -Phase 'stage' -Step 'Verifying the built payload' -Attempt $attempt -PctHint 95
    $builtExe = Join-Path $installDir 'bambu-studio.exe'
    if (-not (Test-Path -LiteralPath $builtExe)) {
        Write-BuildLog "Staged payload is missing bambu-studio.exe at $builtExe."
        Exit-Build -Code 20 -Phase 'stage' -Step 'Built payload is incomplete'
    }
    $manifest = Join-Path $SessionDir 'owned-manifest.txt'
    Write-Manifest -PayloadDir $installDir -OutFile $manifest
    Write-BuildLog "Wrote ownership manifest at $manifest."

    Exit-Build -Code 0 -Phase 'stage' -Step 'Build complete'
} catch {
    # Unexpected failure trap.
    try {
        Write-BuildLog "UNEXPECTED ERROR: $($_.Exception.Message)"
        Set-Status -State 'fatal' -Phase 'stage' -Step 'Unexpected error' -ExitCode 40
    } catch {
        # Nothing more we can do.
    }
    exit 40
}
