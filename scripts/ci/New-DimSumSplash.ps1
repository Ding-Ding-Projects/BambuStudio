<#
.SYNOPSIS
    Deterministically composes a unique 94x122 dim sum splash SVG for a release.

.DESCRIPTION
    Every release of this fork ships one-of-a-kind splash art. This script takes
    a seed (normally the GitHub Actions run number) and composes a flat, simple
    dim sum scene in the same hand-drawn style as the repository's default
    resources/images/splash_logo.svg:

      - 2 to 4 dishes picked from a library of ten hand-authored SVG fragments
        (har gow, siu mai with roe, custard bun, egg tart, spring roll, pleated
        bao, turnip cake slice, stylized chicken foot, egg waffle wedge, tea cup)
      - arrangement slots (left / right / front, plus a small back slot when a
        fourth dish is drawn)
      - a 1- or 2-tier bamboo steamer, always present with chopsticks so the
        mark reads as dim sum at a glance
      - 2 to 4 steam wisps with seeded position and curvature
      - one of four warm palettes rotating the bamboo hues and wrapper tints

    All variation flows from a simple 32-bit LCG seeded with -Seed, so the same
    seed always reproduces byte-identical art. In CI this overwrites only the
    packaged payload copy (install-dir\resources\images\splash_logo.svg); the
    repository default file is never modified.

.PARAMETER Seed
    Integer seed (e.g. the release/run number). Same seed => identical SVG.

.PARAMETER OutPath
    Destination path for the generated SVG. Parent directories are created.

.EXAMPLE
    .\scripts\ci\New-DimSumSplash.ps1 -Seed 123 -OutPath out\splash_logo.svg
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [long]$Seed,

    [Parameter(Mandatory = $true)]
    [string]$OutPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Seeded pseudo-randomness: plain 32-bit LCG (Numerical Recipes constants).
# Deterministic across PowerShell versions and platforms; no System.Random.
$script:RngState = [uint64](([math]::Abs($Seed)) % 4294967296)

function Step-Rng {
    $script:RngState = ($script:RngState * 1664525 + 1013904223) % 4294967296
    return [uint32]$script:RngState
}

# Warm-up so small consecutive seeds diverge quickly.
for ($i = 0; $i -lt 5; $i++) { $null = Step-Rng }

function Get-RandInt {
    param([int]$Min, [int]$MaxExclusive)
    return $Min + [int]((Step-Rng) % [uint32]($MaxExclusive - $Min))
}

function Get-Shuffled {
    param([object[]]$Items)
    $a = @($Items)
    for ($i = $a.Count - 1; $i -gt 0; $i--) {
        $j = Get-RandInt -Min 0 -MaxExclusive ($i + 1)
        $t = $a[$i]; $a[$i] = $a[$j]; $a[$j] = $t
    }
    return ,$a
}

function F {
    # Invariant-culture number formatting for SVG coordinates.
    param([double]$Value)
    return $Value.ToString('0.##', [System.Globalization.CultureInfo]::InvariantCulture)
}

# --- Warm palette rotation (bamboo hues + wrapper tints; steam stays cool grey).
$palettes = @(
    @{ Name = 'toasted-bamboo'; Body = '#C8A468'; Lip = '#B08D50'; Slat = '#A17E44'; Foot = '#8A6D3B'; FootLip = '#7A5E30'; ChopA = '#D96A28'; ChopB = '#E58F3C'; Steam = '#B9C0BA'; Wrap = '#F6E7DC'; WrapLine = '#E8CDBC'; Bun = '#FBF3E4'; BunLine = '#EAD9BC' }
    @{ Name = 'golden-hour';    Body = '#D2A857'; Lip = '#BC9142'; Slat = '#AA8238'; Foot = '#94743C'; FootLip = '#826331'; ChopA = '#C75A22'; ChopB = '#E0863A'; Steam = '#C2C6BC'; Wrap = '#F8EADF'; WrapLine = '#EDD2C0'; Bun = '#FCF4E2'; BunLine = '#EDDCBA' }
    @{ Name = 'rosewood';       Body = '#C09265'; Lip = '#A87C4E'; Slat = '#966E42'; Foot = '#7E5B33'; FootLip = '#6E4E2A'; ChopA = '#B34F24'; ChopB = '#D07E3B'; Steam = '#BFC3C6'; Wrap = '#F5E3D8'; WrapLine = '#E5C8B4'; Bun = '#FAF0DE'; BunLine = '#E8D5B4' }
    @{ Name = 'jade-morning';   Body = '#C5A26A'; Lip = '#AD8B51'; Slat = '#9C7B45'; Foot = '#86683A'; FootLip = '#765A2F'; ChopA = '#CE6329'; ChopB = '#E2913F'; Steam = '#B4C3BB'; Wrap = '#F6E8DE'; WrapLine = '#E9CFBE'; Bun = '#FBF3E6'; BunLine = '#EBDABF' }
)

# --- Dish library: each fragment is authored centered at (0,0), roughly within
# a 34x26 box, flat and simple to match the default splash mark's style.
$dishLibrary = [ordered]@{
    'har gow' = {
        param($P)
        @"
    <ellipse cx="0" cy="0" rx="16" ry="12" fill="$($P.Wrap)"/>
    <path d="M-14 -2 Q0 -14 14 -2" stroke="$($P.WrapLine)" stroke-width="2.5" fill="none"/>
    <path d="M-8 -6 L-6 0 M-2 -8.5 L-1 -1 M4 -8.5 L5 -1 M10 -6 L9 0" stroke="$($P.WrapLine)" stroke-width="2" stroke-linecap="round"/>
"@
    }
    'siu mai with roe' = {
        param($P)
        @"
    <path d="M-13 -2 h26 l-2.5 14 h-21 z" fill="#F3D06A"/>
    <path d="M-10 -2 v13 M-5 -2 v14 M0 -2 v14 M5 -2 v14 M10 -2 v13" stroke="#DDB04E" stroke-width="2"/>
    <ellipse cx="0" cy="-3" rx="13" ry="5.5" fill="#E58F3C"/>
    <circle cx="-3.5" cy="-4.5" r="1.7" fill="#D96A28"/>
    <circle cx="2.5" cy="-3" r="1.7" fill="#D96A28"/>
    <circle cx="0" cy="-6" r="1.5" fill="#D96A28"/>
"@
    }
    'custard bun' = {
        param($P)
        @"
    <ellipse cx="0" cy="0" rx="15" ry="11.5" fill="$($P.Bun)"/>
    <path d="M0 -9 L-4 -3 M0 -9 L0 -2 M0 -9 L4 -3 M0 -9 L-7 -5 M0 -9 L7 -5" stroke="$($P.BunLine)" stroke-width="2" stroke-linecap="round"/>
"@
    }
    'egg tart' = {
        param($P)
        @"
    <path d="M-14 -2 l3 10 q1 3 4 3 h14 q3 0 4 -3 l3 -10 z" fill="#E8B96B"/>
    <path d="M-14 -2 q3.5 -4 7 0 q3.5 -4 7 0 q3.5 -4 7 0 q3.5 -4 7 0" fill="none" stroke="#D6A254" stroke-width="2"/>
    <ellipse cx="0" cy="-1" rx="10" ry="4" fill="#F5C842"/>
    <ellipse cx="-3" cy="-2" rx="3" ry="1.3" fill="#F9DD7E"/>
"@
    }
    'spring roll' = {
        param($P)
        @"
    <g transform="rotate(-10)">
      <rect x="-16" y="-6" width="32" height="12" rx="6" fill="#E2A84B"/>
      <path d="M-9 -5.5 q-3 5.5 0 11 M9 -5.5 q3 5.5 0 11" stroke="#C88932" stroke-width="2" fill="none"/>
      <ellipse cx="-14.5" cy="0" rx="2.5" ry="4.5" fill="#C88932"/>
    </g>
"@
    }
    'pleated bao' = {
        param($P)
        @"
    <ellipse cx="0" cy="1" rx="14" ry="11.5" fill="$($P.Bun)"/>
    <path d="M0 -8 L-5 -2 M0 -8 L0 -1 M0 -8 L5 -2 M0 -8 Q-8 -6 -10 -1 M0 -8 Q8 -6 10 -1" stroke="$($P.BunLine)" stroke-width="2" stroke-linecap="round" fill="none"/>
    <circle cx="0" cy="-8.5" r="1.8" fill="$($P.BunLine)"/>
"@
    }
    'turnip cake slice' = {
        param($P)
        @"
    <rect x="-15" y="-8" width="30" height="16" rx="3" fill="#F2E3C4"/>
    <rect x="-15" y="-8" width="30" height="5" rx="2.5" fill="#E4C98F"/>
    <circle cx="-8" cy="1" r="1.4" fill="#C99A5B"/>
    <circle cx="-1" cy="4" r="1.4" fill="#7FA36B"/>
    <circle cx="6" cy="0" r="1.4" fill="#C99A5B"/>
    <circle cx="10" cy="4" r="1.2" fill="#7FA36B"/>
    <circle cx="-11" cy="5" r="1.2" fill="#C99A5B"/>
"@
    }
    'chicken foot (stylized)' = {
        param($P)
        @"
    <path d="M-12 6 q-2 -8 4 -10 l2 -4 q1 -2 2.5 0 l0.5 3 l1.5 -4 q1 -2 2.5 0 l0 4 l2 -3 q1.5 -1.5 2.5 0.5 l-1 5 q6 3 4 9 q-1 3 -5 3 h-11 q-4 0 -4.5 -3.5 z" fill="#C97B4A"/>
    <path d="M-6 3 q6 -3 12 0" stroke="#B05E2E" stroke-width="2" fill="none" stroke-linecap="round"/>
"@
    }
    'egg waffle wedge' = {
        param($P)
        @"
    <path d="M-15 10 L0 -10 L15 10 z" fill="#EFBE52"/>
    <circle cx="0" cy="-3.5" r="3" fill="#F6D783"/>
    <circle cx="-5" cy="3" r="3" fill="#F6D783"/>
    <circle cx="5" cy="3" r="3" fill="#F6D783"/>
    <circle cx="-9.5" cy="8.5" r="2.6" fill="#F6D783"/>
    <circle cx="0" cy="8.7" r="2.6" fill="#F6D783"/>
    <circle cx="9.5" cy="8.5" r="2.6" fill="#F6D783"/>
"@
    }
    'tea cup' = {
        param($P)
        @"
    <path d="M-11 -4 h22 l-2 10 q-0.8 3 -4 3 h-10 q-3.2 0 -4 -3 z" fill="#F7F1E6"/>
    <ellipse cx="0" cy="-4" rx="11" ry="3.5" fill="#B0722F"/>
    <ellipse cx="0" cy="-4.6" rx="8" ry="2.3" fill="#C98A45"/>
    <path d="M-8.5 4 h17" stroke="$($P.ChopA)" stroke-width="1.6"/>
"@
    }
}

# --- Seeded choices (order of draws is part of the deterministic contract;
# do not reorder these calls without bumping the doc).
$palette   = $palettes[(Get-RandInt -Min 0 -MaxExclusive $palettes.Count)]
$dishCount = Get-RandInt -Min 2 -MaxExclusive 5   # 2..4 dishes
$tierCount = Get-RandInt -Min 1 -MaxExclusive 3   # 1..2 steamer tiers
$wispCount = Get-RandInt -Min 2 -MaxExclusive 5   # 2..4 steam wisps

$dishNames    = @($dishLibrary.Keys)
$pickedDishes = @((Get-Shuffled -Items $dishNames) | Select-Object -First $dishCount)

# Arrangement slots. Front is the hero slot; a small back slot appears only
# when a fourth dish is drawn. Painter's order: back, left, right, front.
$mainSlots = @(
    @{ Name = 'left';  X = 29; Y = 52; Scale = 0.95 }
    @{ Name = 'right'; X = 64; Y = 50; Scale = 0.95 }
    @{ Name = 'front'; X = 47; Y = 66; Scale = 1.0  }
)
$backSlot = @{ Name = 'back'; X = 46; Y = 41; Scale = 0.72 }

$slotsInUse = @((Get-Shuffled -Items $mainSlots) | Select-Object -First ([math]::Min($dishCount, 3)))
if ($dishCount -eq 4) { $slotsInUse = @($backSlot) + $slotsInUse }

$placements = @()
for ($i = 0; $i -lt $dishCount; $i++) {
    $placements += @{ Dish = $pickedDishes[$i]; Slot = $slotsInUse[$i] }
}
# Painter's order so the front dish overlaps back/side dishes naturally.
$drawOrder = @('back', 'left', 'right', 'front')
$placements = @($placements | Sort-Object { $drawOrder.IndexOf($_.Slot.Name) })

# Steam wisps: seeded x positions, curvature amplitude, and direction.
$wispCandidates = @(30, 47, 64, 38, 56)
$wispXs = @((Get-Shuffled -Items $wispCandidates) | Select-Object -First $wispCount | Sort-Object)
$wisps = @()
foreach ($wx in $wispXs) {
    $amp = Get-RandInt -Min 3 -MaxExclusive 7
    $dir = if ((Get-RandInt -Min 0 -MaxExclusive 2) -eq 0) { 1 } else { -1 }
    $y0  = 22 + (Get-RandInt -Min 0 -MaxExclusive 3)
    $wisps += @{ X = $wx; Amp = $amp; Dir = $dir; Y0 = $y0 }
}

# --- Compose the SVG.
$sb = [System.Text.StringBuilder]::new()
$null = $sb.AppendLine('<svg width="94" height="122" viewBox="0 0 94 122" fill="none" xmlns="http://www.w3.org/2000/svg">')
$null = $sb.AppendLine("  <!-- Dim sum steamer splash mark, release edition. Seed $Seed :: palette $($palette.Name) :: dishes: $($pickedDishes -join ', ') :: tiers $tierCount :: wisps $wispCount -->")

# Steam wisps (drawn first, behind the food).
foreach ($w in $wisps) {
    $x  = $w.X; $y0 = $w.Y0; $a = $w.Amp; $d = $w.Dir
    $c1x = F ($x - $d * $a);        $c1y = F ($y0 - 5)
    $c2x = F ($x + $d * ($a - 2));  $c2y = F ($y0 - 10)
    $ex  = F ($x - $d * 2);         $ey  = F ($y0 - 15)
    $null = $sb.AppendLine("  <path d=""M$(F $x) $(F $y0) C$c1x $c1y $c2x $c2y $ex $ey"" stroke=""$($palette.Steam)"" stroke-width=""3"" stroke-linecap=""round"" fill=""none""/>")
}

# Dishes in their slots.
foreach ($p in $placements) {
    $slot = $p.Slot
    $fragment = (& $dishLibrary[$p.Dish] $palette).TrimEnd()
    $null = $sb.AppendLine("  <g transform=""translate($($slot.X) $($slot.Y)) scale($(F $slot.Scale))""> <!-- $($p.Dish) @ $($slot.Name) -->")
    $null = $sb.AppendLine($fragment)
    $null = $sb.AppendLine('  </g>')
}

# Bamboo steamer: always present so the mark reads as dim sum at a glance.
$slatXs = @(14, 22, 30, 38, 46, 54, 62, 70, 78)
if ($tierCount -eq 1) {
    $null = $sb.AppendLine("  <path d=""M8 74 h78 v14 q0 5 -5 5 H13 q-5 0 -5 -5 z"" fill=""$($palette.Body)""/>")
    $null = $sb.AppendLine("  <path d=""M8 74 h78 v5 H8 z"" fill=""$($palette.Lip)""/>")
    $slats = ($slatXs | ForEach-Object { "M$_ 79 v14" }) -join ' '
    $null = $sb.AppendLine("  <path d=""$slats"" stroke=""$($palette.Slat)"" stroke-width=""2""/>")
    $null = $sb.AppendLine("  <path d=""M12 97 h70 v9 q0 4 -4 4 H16 q-4 0 -4 -4 z"" fill=""$($palette.Foot)""/>")
    $null = $sb.AppendLine("  <path d=""M12 97 h70 v4 H12 z"" fill=""$($palette.FootLip)""/>")
}
else {
    # Two stacked tiers, slightly compressed so chopsticks keep their spot.
    $null = $sb.AppendLine("  <path d=""M8 70 h78 v9 q0 4 -4 4 H12 q-4 0 -4 -4 z"" fill=""$($palette.Body)""/>")
    $null = $sb.AppendLine("  <path d=""M8 70 h78 v4 H8 z"" fill=""$($palette.Lip)""/>")
    $slats1 = ($slatXs | ForEach-Object { "M$_ 74 v9" }) -join ' '
    $null = $sb.AppendLine("  <path d=""$slats1"" stroke=""$($palette.Slat)"" stroke-width=""2""/>")
    $null = $sb.AppendLine("  <path d=""M8 85 h78 v9 q0 4 -4 4 H12 q-4 0 -4 -4 z"" fill=""$($palette.Body)""/>")
    $null = $sb.AppendLine("  <path d=""M8 85 h78 v4 H8 z"" fill=""$($palette.Lip)""/>")
    $slats2 = ($slatXs | ForEach-Object { "M$_ 89 v9" }) -join ' '
    $null = $sb.AppendLine("  <path d=""$slats2"" stroke=""$($palette.Slat)"" stroke-width=""2""/>")
    $null = $sb.AppendLine("  <path d=""M12 100 h70 v7 q0 4 -4 4 H16 q-4 0 -4 -4 z"" fill=""$($palette.Foot)""/>")
    $null = $sb.AppendLine("  <path d=""M12 100 h70 v3.5 H12 z"" fill=""$($palette.FootLip)""/>")
}

# Chopsticks (always present).
$null = $sb.AppendLine("  <path d=""M6 116 L88 108"" stroke=""$($palette.ChopA)"" stroke-width=""3.4"" stroke-linecap=""round""/>")
$null = $sb.AppendLine("  <path d=""M6 121 L88 114"" stroke=""$($palette.ChopB)"" stroke-width=""3.4"" stroke-linecap=""round""/>")
$null = $sb.AppendLine('</svg>')

$svg = $sb.ToString()

# Well-formedness gate before anything touches disk.
try {
    $null = [xml]$svg
}
catch {
    throw "Generated SVG for seed $Seed is not well-formed XML: $($_.Exception.Message)"
}

$parent = Split-Path -Parent $OutPath
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    $null = New-Item -ItemType Directory -Force -Path $parent
}
[System.IO.File]::WriteAllText($OutPath, $svg, [System.Text.UTF8Encoding]::new($false))

Write-Host "Dim sum splash composed for seed $Seed -> $OutPath"
Write-Host "  Palette : $($palette.Name)"
Write-Host "  Dishes  : $(($placements | ForEach-Object { ""$($_.Dish) @ $($_.Slot.Name)"" }) -join '; ')"
Write-Host "  Steamer : $tierCount tier(s)"
Write-Host "  Steam   : $wispCount wisp(s) at x = $($wispXs -join ', ')"
Write-Host "  Bytes   : $((Get-Item -LiteralPath $OutPath).Length)"
