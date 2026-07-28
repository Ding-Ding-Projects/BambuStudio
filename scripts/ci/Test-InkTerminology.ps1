<#
.SYNOPSIS
    Enforces the fork's user-facing vocabulary: filament -> ink, AMS -> Ink Dispenser.

.DESCRIPTION
    `docs/features/windows/ink-terminology.md` describes the rename as a display-only change:
    every `msgid`, config key, preset name, file format field and CLI flag keeps the upstream
    spelling, and only what the user READS becomes ink. Nothing enforced that, so it can drift back
    one merge at a time — and a half-renamed product reads as a bug to the user.

    This checks the surfaces that actually render text:

      - the native English override catalogue and the Cantonese catalogue (msgstr values only),
      - the DeviceWeb localisation catalogues (values only),
      - the ui-md3 site and app copy tables (values only).

    It is deliberately blind to identifiers. Catalogue KEYS, msgids, object keys, CSS classes and
    `data-*` values are what code matches on; renaming those breaks lookups and profile
    compatibility, so they are out of scope here and must stay upstream-spelled.

.NOTES
    Obsolete PO entries are skipped. gettext marks them `#~`, they are never loaded by
    wxTranslations, and they exist only as merge history — flagging them would report 71 phantom
    failures on a catalogue that is in fact clean.

.EXAMPLE
    .\scripts\ci\Test-InkTerminology.ps1
    .\scripts\ci\Test-InkTerminology.ps1 -Detailed
#>
[CmdletBinding()]
param(
    [switch] $Detailed
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$failures = [System.Collections.Generic.List[string]]::new()

# "filament"/"filaments" and "AMS" as whole words. AMS is matched case-sensitively so it cannot
# fire on ordinary words that merely contain those letters.
$englishTerm = [regex] '(?i)\bfilaments?\b'
$amsTerm     = [regex] '\bAMS\b'
# 線材 is the Chinese term the Cantonese catalogue replaced with 墨水.
$cantoTerm   = [regex] '線材'

function Add-Failure {
    param([string] $Surface, [string] $Location, [string] $Text)
    $trimmed = if ($Text.Length -gt 110) { $Text.Substring(0, 110) + '...' } else { $Text }
    $failures.Add("[$Surface] ${Location}: $trimmed")
}

function Test-PoCatalog {
    param([Parameter(Mandatory)] [string] $Path, [Parameter(Mandatory)] [string] $Surface,
          [Parameter(Mandatory)] [regex[]] $Terms)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing catalogue '$Path'." }
    $lineNo = 0
    $checked = 0
    foreach ($line in (Get-Content -LiteralPath $Path -Encoding utf8)) {
        $lineNo++
        # Only translated OUTPUT. msgid lines are the upstream spelling by design, and `#~` marks
        # an obsolete entry that is never loaded.
        if ($line -notmatch '^msgstr(\[\d+\])?\s+"(.*)"\s*$') { continue }
        $value = $Matches[2]
        if ([string]::IsNullOrWhiteSpace($value)) { continue }
        $checked++
        foreach ($t in $Terms) {
            if ($t.IsMatch($value)) { Add-Failure -Surface $Surface -Location "$(Split-Path $Path -Leaf):$lineNo" -Text $value; break }
        }
    }
    Write-Host ("  {0}: {1} translated entries checked" -f $Surface, $checked)
}

function Test-JsonValues {
    param([Parameter(Mandatory)] [string] $Path, [Parameter(Mandatory)] [string] $Surface,
          [Parameter(Mandatory)] [regex[]] $Terms)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing catalogue '$Path'." }
    $json = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $checked = 0
    foreach ($prop in $json.PSObject.Properties) {
        $value = [string] $prop.Value          # the KEY is an identifier; only the value renders
        if ([string]::IsNullOrWhiteSpace($value)) { continue }
        $checked++
        foreach ($t in $Terms) {
            if ($t.IsMatch($value)) { Add-Failure -Surface $Surface -Location "$(Split-Path $Path -Leaf) key '$($prop.Name)'" -Text $value; break }
        }
    }
    Write-Host ("  {0}: {1} values checked" -f $Surface, $checked)
}

function Test-JsCopyTable {
    param([Parameter(Mandatory)] [string] $Path, [Parameter(Mandatory)] [string] $Surface,
          [Parameter(Mandatory)] [regex[]] $Terms)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing copy table '$Path'." }
    $lineNo = 0
    $checked = 0
    foreach ($line in (Get-Content -LiteralPath $Path -Encoding utf8)) {
        $lineNo++
        if ($line -match '^\s*//') { continue }
        # Quoted strings that sit on the VALUE side of a `key: '...'` pair. A bare quoted token at
        # the start of a line is a key and is skipped.
        foreach ($m in [regex]::Matches($line, "[:,]\s*'([^']{2,})'|[:,]\s*`"([^`"]{2,})`"")) {
            $value = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }
            # Skip anything that looks like an identifier or a path rather than prose.
            if ($value -match '^[a-z0-9_.\-\/]+$' -and $value -notmatch '\s') { continue }
            $checked++
            foreach ($t in $Terms) {
                if ($t.IsMatch($value)) { Add-Failure -Surface $Surface -Location "$(Split-Path $Path -Leaf):$lineNo" -Text $value; break }
            }
        }
    }
    Write-Host ("  {0}: {1} candidate strings checked" -f $Surface, $checked)
}

Write-Host 'Checking native translation catalogues...'
Test-PoCatalog -Path (Join-Path $repoRoot 'bbl\i18n\en\BambuStudio_en.po') `
    -Surface 'native/en' -Terms @($englishTerm, $amsTerm)
Test-PoCatalog -Path (Join-Path $repoRoot 'bbl\i18n\yue_HK\BambuStudio_yue_HK.po') `
    -Surface 'native/yue_HK' -Terms @($englishTerm, $cantoTerm)

Write-Host 'Checking DeviceWeb localisation catalogues...'
$deviceRoot = Join-Path $repoRoot 'src\slic3r\GUI\DeviceWeb\device_page\locales'
if (Test-Path -LiteralPath $deviceRoot) {
    Test-JsonValues -Path (Join-Path $deviceRoot 'en.json') -Surface 'deviceweb/en' -Terms @($englishTerm, $amsTerm)
    Test-JsonValues -Path (Join-Path $deviceRoot 'yue_HK.json') -Surface 'deviceweb/yue_HK' -Terms @($englishTerm, $cantoTerm)
}

Write-Host 'Checking ui-md3 copy tables...'
foreach ($pair in @(
    @{ Path = 'ui-md3\site\copy.js';          Surface = 'ui-md3/site' },
    @{ Path = 'ui-md3\app\i18n.resources.js'; Surface = 'ui-md3/app'  })) {
    $full = Join-Path $repoRoot $pair.Path
    if (Test-Path -LiteralPath $full) {
        Test-JsCopyTable -Path $full -Surface $pair.Surface -Terms @($englishTerm, $amsTerm, $cantoTerm)
    }
}

# The catalogues above are only half the story, and checking them alone is a trap: in this app the
# English source string IS the lookup key, so a catalogue that has been renamed end to end still
# reads perfectly while the screen that PRODUCES the string says something else - and then the
# lookup misses and the user gets raw English in Cantonese mode. An earlier revision of this script
# scanned only the two files above and reported a clean pass while exactly that regression was live
# in calibration.logic.js. So the producers are checked too.
Write-Host 'Checking ui-md3 string producers (templates, screen logic, assembled shell)...'
$producerRoots = @(
    @{ Glob = 'ui-md3\app\screens\*.template.html'; Surface = 'ui-md3/templates' },
    @{ Glob = 'ui-md3\app\screens\*.logic.js';      Surface = 'ui-md3/screen-logic' },
    @{ Glob = 'ui-md3\app\main.logic.js';           Surface = 'ui-md3/app-logic' },
    @{ Glob = 'ui-md3\index.html';                  Surface = 'ui-md3/shell' }
)
foreach ($entry in $producerRoots) {
    foreach ($file in @(Get-ChildItem -Path (Join-Path $repoRoot $entry.Glob) -ErrorAction SilentlyContinue)) {
        $lineNo = 0
        $hits = 0
        foreach ($line in (Get-Content -LiteralPath $file.FullName -Encoding utf8)) {
            $lineNo++
            # Identifiers legitimately keep the upstream spelling: file names, view ids, state keys,
            # element ids/classes and data-* values. Skip the constructs that carry them so this
            # reports rendered copy rather than machinery.
            $scrubbed = $line
            # {{ ... }} is a DATA BINDING, not copy: `<sc-for list="{{ filaments }}">` renders rows
            # from a field named filaments while the visible label beside it already says "Ink".
            # Strip bindings first or every such row reports as a violation.
            $scrubbed = [regex]::Replace($scrubbed, '\{\{[^}]*\}\}', ' ')
            $scrubbed = [regex]::Replace($scrubbed, '<!--.*?-->', ' ')                                            # authoring comments
            $scrubbed = [regex]::Replace($scrubbed, '^\s*(\*|//).*$', ' ')                                        # JS comment lines
            $scrubbed = [regex]::Replace($scrubbed, '/\*.*?\*/', ' ')
            $scrubbed = [regex]::Replace($scrubbed, '(?i)\bfilament[a-z0-9_]*\.(html|js|webp|png|svg)\b', ' ')    # file names
            $scrubbed = [regex]::Replace($scrubbed, '(?i)(id|class|name|data-[a-z-]+|href|src|view|list|as)\s*=\s*"[^"]*"', ' ')
            $scrubbed = [regex]::Replace($scrubbed, "(?i)(id|class|name|view)\s*:\s*'[^']*'", ' ')
            # Object keys and method calls are machinery: `filaments: this.render_filaments()`.
            $scrubbed = [regex]::Replace($scrubbed, '(?i)\bfilaments?\s*:', ' ')
            $scrubbed = [regex]::Replace($scrubbed, '(?i)\brender_filaments?\b|\bis_?Filament\b', ' ')
            # View-id comparisons: `this.state.view === 'filament'` matches a route, not a label.
            $scrubbed = [regex]::Replace($scrubbed, "(?i)===?\s*'filaments?'", ' ')
            $scrubbed = [regex]::Replace($scrubbed, '(?i)\b(isFilament|filamentRows|addFilamentQuery|setFilQuery|filQuery)\b', ' ')
            $scrubbed = [regex]::Replace($scrubbed, '(?i)\bbbsflmt\b', ' ')
            foreach ($t in @($englishTerm, $amsTerm, $cantoTerm)) {
                if ($t.IsMatch($scrubbed)) {
                    Add-Failure -Surface $entry.Surface -Location "$($file.Name):$lineNo" -Text $line.Trim()
                    $hits++
                    break
                }
            }
        }
        if ($Detailed -or $hits -gt 0) { Write-Host ("  {0}: {1} ({2} hit(s))" -f $entry.Surface, $file.Name, $hits) }
    }
}

Write-Host ''
if ($failures.Count -gt 0) {
    Write-Host "Ink terminology violations: $($failures.Count)" -ForegroundColor Red
    $show = if ($Detailed) { $failures } else { $failures | Select-Object -First 25 }
    foreach ($f in $show) { Write-Host "  $f" }
    if (-not $Detailed -and $failures.Count -gt 25) {
        Write-Host "  ... and $($failures.Count - 25) more; re-run with -Detailed"
    }
    throw @"
User-facing text still uses the upstream vocabulary. This fork presents filament as "ink" and AMS as
"Ink Dispenser" (docs/features/windows/ink-terminology.md); a half-renamed product reads as a bug.
Rename the DISPLAYED text only - msgids, catalogue keys, config keys, preset names and API fields
keep the upstream spelling on purpose, because profile and 3MF compatibility depend on them.
"@
}

Write-Host 'Ink terminology validation passed: no user-facing filament/AMS text remains.' -ForegroundColor Green
