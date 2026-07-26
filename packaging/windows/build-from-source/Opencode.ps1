<#
.SYNOPSIS
    opencode install + blanket-allow config emitter + bounded repair invocation.
.DESCRIPTION
    Dot-sourced by Build-FromSource.ps1. Owns:
      * Install-Opencode        - global npm install of opencode-ai.
      * New-OpencodeAllowConfig - writes the project-local opencode.json into the
                                  cloned source directory so every opencode session
                                  launched from that directory is fully
                                  non-interactive (all action classes = allow),
                                  while question=deny (auto-denies, never blocks) and
                                  external_directory=deny scopes the grant strictly to
                                  the clone directory.
      * Invoke-OpencodeRepair   - one non-interactive opencode run in the clone dir.

    SECURITY POSTURE (surfaced deliberately):
      Every *action* permission class the live schema lists is set to "allow" so
      nothing ever blocks a headless build repair. The blanket allow is scoped to
      the cloned source directory only, because:
        - the config is project-local (applies only to sessions started in the clone), and
        - external_directory stays "deny" (opencode cannot edit files outside the clone).
      question stays "deny" so an interaction request auto-denies instead of hanging.
      The residual risk (edit/bash/webfetch = allow inside the clone) is documented in
      docs/features/releases/windows-build-from-source.md.
#>

Set-StrictMode -Version Latest

function Install-Opencode {
    [CmdletBinding()]
    param()

    # Already installed?
    $existing = Get-Command opencode -ErrorAction SilentlyContinue
    if ($existing) {
        return
    }

    $npm = Get-Command npm -ErrorAction SilentlyContinue
    if (-not $npm) {
        throw 'npm is not available; Node.js LTS must be installed before opencode.'
    }

    # npm on Windows is npm.cmd; call it and honour its exit code.
    & npm.cmd install -g opencode-ai 2>&1 | ForEach-Object { $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "npm install -g opencode-ai failed with exit code $LASTEXITCODE."
    }

    $probe = Get-Command opencode -ErrorAction SilentlyContinue
    if (-not $probe) {
        throw 'opencode was installed but is not on PATH.'
    }
}

function New-OpencodeAllowConfig {
    <#
    .SYNOPSIS
        Writes <CloneDir>\opencode.json granting every action permission class,
        with question=deny and external_directory=deny. Returns the file path.
    .PARAMETER CloneDir
        Directory the config is written into. The grant applies ONLY to opencode
        sessions launched with this directory as their working directory.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $CloneDir
    )

    # Baseline action classes known to the opencode permission schema. Every action
    # class is granted "allow". The two non-action guards stay "deny".
    $permission = [ordered]@{
        edit               = 'allow'
        bash               = 'allow'
        webfetch           = 'allow'
        question           = 'deny'
        external_directory = 'deny'
    }

    # Best-effort: mirror the LIVE opencode config schema so any future action class
    # is covered automatically. Offline / failure keeps the baseline above.
    try {
        $schema = Invoke-RestMethod -Uri 'https://opencode.ai/config.json' -TimeoutSec 15 -ErrorAction Stop
        $permProp = $null
        if ($schema.PSObject.Properties['properties']) {
            $permProp = $schema.properties.permission
        }
        if ($permProp -and $permProp.properties) {
            foreach ($name in $permProp.properties.PSObject.Properties.Name) {
                if ($name -eq 'question' -or $name -eq 'external_directory') {
                    # Non-action guards stay deny regardless of schema.
                    continue
                }
                # Any action class the schema lists is granted allow.
                $permission[$name] = 'allow'
            }
        }
    } catch {
        # No network or schema unavailable: baseline stands. Non-fatal by design.
    }

    # Enforce the two invariants even if a schema pass touched them.
    $permission['question'] = 'deny'
    $permission['external_directory'] = 'deny'

    $config = [ordered]@{
        '$schema'  = 'https://opencode.ai/config.json'
        permission = $permission
    }

    if (-not (Test-Path -LiteralPath $CloneDir)) {
        New-Item -ItemType Directory -Path $CloneDir -Force | Out-Null
    }

    $target = Join-Path $CloneDir 'opencode.json'
    $json = $config | ConvertTo-Json -Depth 6
    # UTF-8 without BOM keeps opencode's JSON parser happy on every platform.
    [System.IO.File]::WriteAllText($target, $json, (New-Object System.Text.UTF8Encoding($false)))
    return $target
}

function Invoke-OpencodeRepair {
    <#
    .SYNOPSIS
        Runs one non-interactive opencode session in the clone dir with the repair
        prompt. stdin is redirected from NUL and any auto-approve flag the installed
        opencode version exposes is passed, so no prompt can ever appear.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $CloneDir,

        [Parameter(Mandatory = $true)]
        [string] $Prompt
    )

    # Ensure the project-local blanket-allow config is present before the run.
    New-OpencodeAllowConfig -CloneDir $CloneDir | Out-Null

    # Discover whether this opencode build exposes an auto-approve flag.
    $autoFlag = @()
    try {
        $help = & opencode run --help 2>&1 | Out-String
        if ($help -match '(?m)^\s*--yes\b' -or $help -match '\B--yes\b') {
            $autoFlag = @('--yes')
        }
    } catch {
        $autoFlag = @()
    }

    $prev = Get-Location
    try {
        Set-Location -LiteralPath $CloneDir
        # cwd = clone dir makes the project-local opencode.json authoritative.
        # Piping $null closes opencode's stdin immediately (NUL-equivalent), so a
        # blocked prompt can never wait on input.
        $runArgs = @('run') + $autoFlag + @($Prompt)
        $null | & opencode @runArgs 2>&1 | ForEach-Object { $_ }
        return $LASTEXITCODE
    } finally {
        Set-Location -LiteralPath $prev
    }
}
