<#
.SYNOPSIS
    Validates the Hong Kong dish codename roster that names every release.

.DESCRIPTION
    `.github/workflows/build_all.yml` assigns each release a codename from a roster of
    Hong Kong dishes, then from style x dish combinations, then from those plus a serving
    counter. The roster lives inline in the workflow because the release step is a single
    self-contained PowerShell block; this test parses it back out and holds it to the
    contract the release step depends on:

      - every entry is bilingual (Latin name + Han characters), because the release title
        renders both;
      - no duplicates, or two releases would share a codename;
      - the assignment is a bijection over a long horizon - the whole point of the
        combinatorial scheme is that a name is never reused;
      - the roster only ever GROWS AT THE TAIL. Codenames are assigned by index, so
        inserting a dish renames every release after it. Already-published releases are
        immutable, so an insertion makes the workflow disagree with published history.

    The append-only check compares against the roster recorded in `codename-roster.json`
    next to this script. Regenerate that snapshot deliberately - with -UpdateBaseline -
    only when you have appended, never to paper over an insertion.

.EXAMPLE
    .\scripts\ci\Test-ReleaseCodenames.ps1
    .\scripts\ci\Test-ReleaseCodenames.ps1 -UpdateBaseline
#>
[CmdletBinding()]
param(
    [switch] $UpdateBaseline
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$workflow = Join-Path $repoRoot '.github\workflows\build_all.yml'
$baseline = Join-Path $PSScriptRoot 'codename-roster.json'

function Assert-True {
    param([Parameter(Mandatory)] [bool] $Condition, [Parameter(Mandatory)] [string] $Message)
    if (-not $Condition) { throw $Message }
}

Assert-True (Test-Path -LiteralPath $workflow -PathType Leaf) "Missing workflow '$workflow'."
$text = Get-Content -LiteralPath $workflow -Raw

# Pull the two arrays back out of the release step. The pattern is anchored on the
# assignment and the closing paren at the same indent, so a stray ')' inside a comment
# cannot end the match early.
function Get-Roster {
    param([Parameter(Mandatory)] [string] $Name)
    $pattern = '(?ms)^\s*\$' + $Name + ' = @\(\s*$(.*?)^\s*\)\s*$'
    $match = [regex]::Match($text, $pattern)
    Assert-True $match.Success "Could not find the `$$Name array in build_all.yml."
    # Only single-quoted literals count; '#' comment lines carry no quotes so they drop out.
    return @([regex]::Matches($match.Groups[1].Value, "'([^']+)'") | ForEach-Object { $_.Groups[1].Value })
}

$dishes = Get-Roster -Name 'dishes'
$styles = Get-Roster -Name 'styles'

Write-Host "Parsed $($dishes.Count) dishes and $($styles.Count) styles."
Assert-True ($dishes.Count -ge 97) "Dish roster shrank to $($dishes.Count); it must only grow."
Assert-True ($styles.Count -ge 40) "Style roster shrank to $($styles.Count); it must only grow."

# --- Every entry is bilingual -------------------------------------------------------
$han = [regex] '[\p{IsCJKUnifiedIdeographs}]'
foreach ($entry in @($dishes) + @($styles)) {
    Assert-True ($entry -match '^[A-Za-z0-9][A-Za-z0-9 \-]*\s') `
        "Codename entry '$entry' must start with a Latin name followed by a space."
    Assert-True ($han.IsMatch($entry)) `
        "Codename entry '$entry' has no Han characters; release titles render both languages."
    # The release step splits on spaces and treats the LAST token as the CJK half, so the
    # Han portion must be exactly one trailing token with no spaces inside it.
    $tokens = $entry -split ' '
    Assert-True ($han.IsMatch($tokens[-1])) `
        "Codename entry '$entry' must end with its Han name as the final space-separated token."
    Assert-True (-not ($han.IsMatch(($tokens[0..($tokens.Count - 2)] -join ' ')))) `
        "Codename entry '$entry' mixes Han characters into its Latin half; the release step would split it wrongly."
    Assert-True (-not $entry.Contains("'")) `
        "Codename entry '$entry' contains an apostrophe, which would break the single-quoted PowerShell literal."
}

# --- No duplicates ------------------------------------------------------------------
foreach ($pair in @(@{ Name = 'dish'; Items = $dishes }, @{ Name = 'style'; Items = $styles })) {
    $dupes = @($pair.Items | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
    Assert-True ($dupes.Count -eq 0) "Duplicate $($pair.Name) entries: $($dupes -join ', ')"
    # Two dishes sharing a Han name would render identically even with different English.
    $hanDupes = @($pair.Items | ForEach-Object { ($_ -split ' ')[-1] } |
        Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
    Assert-True ($hanDupes.Count -eq 0) "Duplicate $($pair.Name) Han names: $($hanDupes -join ', ')"
}

# --- The assignment is the same algorithm the workflow runs -------------------------
function Get-Codename {
    param([Parameter(Mandatory)] [int] $ReleaseNumber)
    $idx = $ReleaseNumber - 1
    if ($idx -lt $dishes.Count) { return $dishes[$idx] }
    $combo      = $idx - $dishes.Count
    $perServing = $styles.Count * $dishes.Count
    $serving    = [Math]::Floor($combo / $perServing)
    $within     = $combo % $perServing
    $style      = ($styles[[Math]::Floor($within / $dishes.Count)] -split ' ')
    $dish       = ($dishes[$within % $dishes.Count] -split ' ')
    $styleEn = ($style[0..($style.Count - 2)] -join ' '); $styleZh = $style[-1]
    $dishEn  = ($dish[0..($dish.Count - 2)]  -join ' '); $dishZh  = $dish[-1]
    $name = "$styleEn $dishEn $styleZh$dishZh"
    if ($serving -gt 0) { $name = "$name $($serving + 1)th serving 第$($serving + 1)籠" }
    return $name
}

# Uniqueness across the bare-dish range, the first full combo range, and into a second
# serving - the three regimes the algorithm switches between.
$horizon = $dishes.Count + ($styles.Count * $dishes.Count) + 50
$seen = [System.Collections.Generic.HashSet[string]]::new()
for ($n = 1; $n -le $horizon; $n++) {
    $name = Get-Codename -ReleaseNumber $n
    Assert-True (-not [string]::IsNullOrWhiteSpace($name)) "Release v$n produced an empty codename."
    Assert-True ($seen.Add($name)) "Release v$n reuses codename '$name'."
}
Write-Host "Verified $horizon unique codenames across all three assignment regimes."

# --- Append-only ---------------------------------------------------------------------
$current = [ordered]@{ dishes = $dishes; styles = $styles }
if ($UpdateBaseline) {
    $current | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $baseline -Encoding utf8
    Write-Host "Baseline updated: $($dishes.Count) dishes, $($styles.Count) styles."
}
elseif (Test-Path -LiteralPath $baseline -PathType Leaf) {
    $previous = Get-Content -LiteralPath $baseline -Raw | ConvertFrom-Json
    foreach ($pair in @(
        @{ Name = 'dishes'; Now = $dishes; Was = @($previous.dishes) },
        @{ Name = 'styles'; Now = $styles; Was = @($previous.styles) })) {
        Assert-True ($pair.Now.Count -ge $pair.Was.Count) `
            "The $($pair.Name) roster shrank from $($pair.Was.Count) to $($pair.Now.Count)."
        for ($i = 0; $i -lt $pair.Was.Count; $i++) {
            Assert-True ($pair.Now[$i] -ceq $pair.Was[$i]) @"
The $($pair.Name) roster changed at index $i ('$($pair.Was[$i])' -> '$($pair.Now[$i])').
Codenames are assigned by index, so this renames every release from v$($i + 1) onward and
makes the workflow disagree with already-published immutable releases. Append instead, or
run with -UpdateBaseline only if the rename is genuinely intended.
"@
        }
    }
    Write-Host "Append-only check passed against the recorded baseline."
}
else {
    Write-Host "No baseline recorded yet; run with -UpdateBaseline to create one."
}

Write-Host "Release codename validation passed: $($dishes.Count) dishes x $($styles.Count) styles."
