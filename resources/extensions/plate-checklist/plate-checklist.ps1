# Plate checklist, an extension that comes with Bambu Studio.
# After slicing it adds a note only when something deserves a second look;
# before a print is sent it answers checklist rows. Bambu Studio writes one
# JSON request to standard input and reads one JSON answer from standard
# output (docs/features/windows/extensions.md). Windows PowerShell 5.1 or later.
$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$reader = [System.IO.StreamReader]::new([Console]::OpenStandardInput(), $utf8)
$request = $reader.ReadToEnd() | ConvertFrom-Json
$slice = $request.context.slice

# Its settings (File > Extensions, Settings); the defaults when an older
# Bambu Studio sends none.
$settings = $request.settings
function Get-Setting([string]$name, $default) {
    if ($null -ne $settings -and $null -ne $settings.$name) { return $settings.$name }
    return $default
}
$longHours = [double](Get-Setting 'long_print_hours' 8)
$manyChanges = [int](Get-Setting 'filament_changes' 30)
$slotGrams = [double](Get-Setting 'grams_per_slot' 500)
$sayAllIsWell = [bool](Get-Setting 'say_all_is_well' $false)

function Format-Duration([double]$seconds) {
    $total = [int][math]::Floor([math]::Max(0, $seconds) + 0.5)
    $h = [int][math]::Floor($total / 3600)
    $m = [int][math]::Floor(($total % 3600) / 60)
    if ($h -gt 0) { return ('{0} h {1} min' -f $h, $m) }
    return ('{0} min' -f $m)
}

function Get-Short([string]$text, [int]$limit) {
    if (-not $text) { return '' }
    $one = ($text -replace '[\r\n\t]+', ' ').Trim()
    if ($one.Length -gt $limit) { $one = $one.Substring(0, $limit - 3) + '...' }
    return $one
}

$rows = @()
if ($null -eq $slice) {
    $rows += @{ state = 'fail'; text = 'The plate is not sliced yet.' }
} else {
    $warnings = @($slice.warnings | Where-Object { $null -ne $_ })
    if ($warnings.Count -gt 0) {
        $first = [string]$warnings[0].message
        if (-not $first) { $first = [string]$warnings[0].code }
        $rows += @{ state = 'warn'; text = ('{0} slicing warning(s), the first: {1}' -f $warnings.Count, (Get-Short $first 120)) }
    } else {
        $rows += @{ state = 'pass'; text = 'No slicing warnings.' }
    }
    if ([double]$slice.seconds -ge $longHours * 3600) {
        $rows += @{ state = 'warn'; text = ('A long print: {0}. Check that every spool has enough filament.' -f (Format-Duration $slice.seconds)) }
    }
    $changes = [int]$slice.filamentChanges
    if ($changes -ge $manyChanges) {
        $rows += @{ state = 'warn'; text = ('{0} filament changes: each one flushes filament and adds time.' -f $changes) }
    }
    foreach ($f in @($slice.filaments)) {
        if ($null -eq $f) { continue }
        if ([double]$f.grams -ge $slotGrams) {
            $rows += @{ state = 'warn'; text = ('Slot {0} needs {1:N0} g of {2}.' -f [int]$f.slot, [double]$f.grams, (Get-Short ([string]$f.type) 20)) }
        }
    }
}

$answer = @{ apiVersion = 1 }
if ($request.hook -eq 'print.before_send') {
    if ($rows.Count -gt 20) { $rows = $rows[0..19] }
    $answer.checklist = @($rows)
} else {
    $notes = @()
    foreach ($r in $rows) { if ($r.state -ne 'pass') { $notes += ('Check: ' + $r.text) } }
    if ($notes.Count -gt 10) { $notes = $notes[0..9] }
    if ($notes.Count -eq 0 -and $sayAllIsWell -and $null -ne $slice) { $notes += 'Checked: nothing needs a second look.' }
    if ($notes.Count -gt 0) { $answer.notes = @($notes) }
}
$writer = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $utf8)
$writer.Write(($answer | ConvertTo-Json -Compress -Depth 6))
$writer.Flush()
