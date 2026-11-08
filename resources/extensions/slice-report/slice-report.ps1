# Slice report, an extension that comes with Bambu Studio.
# Bambu Studio writes one JSON request to standard input and reads one JSON
# answer from standard output. The formats are in the Extensions article
# (docs/features/windows/extensions.md). Windows PowerShell 5.1 or later.
$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$reader = [System.IO.StreamReader]::new([Console]::OpenStandardInput(), $utf8)
$request = $reader.ReadToEnd() | ConvertFrom-Json
$slice = $request.context.slice
$printer = $request.context.printer

function Format-Duration([double]$seconds) {
    $total = [int][math]::Floor([math]::Max(0, $seconds) + 0.5)
    $h = [int][math]::Floor($total / 3600)
    $m = [int][math]::Floor(($total % 3600) / 60)
    $s = $total % 60
    if ($h -gt 0) { return ('{0} h {1} min' -f $h, $m) }
    if ($m -gt 0) { return ('{0} min {1} s' -f $m, $s) }
    return ('{0} s' -f $s)
}

function Get-FilamentText($slice) {
    $parts = @()
    foreach ($f in @($slice.filaments)) {
        if ($null -eq $f) { continue }
        $parts += ('{0:N1} g {1}' -f [double]$f.grams, [string]$f.type)
    }
    if ($parts.Count -eq 0) { return 'no filament' }
    $text = $parts -join ' + '
    if ($text.Length -gt 150) { $text = $text.Substring(0, 147) + '...' }
    return $text
}

function Write-Answer($answer) {
    $writer = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $utf8)
    $writer.Write(($answer | ConvertTo-Json -Compress -Depth 6))
    $writer.Flush()
}

$answer = @{ apiVersion = 1 }
if ($null -eq $slice) {
    $answer.notes = @('Slice the plate first, then run Save a slice report again.')
    Write-Answer $answer
    exit 0
}

$summary = '{0}, {1}, {2} layers' -f (Format-Duration $slice.seconds), (Get-FilamentText $slice), [int]$slice.layers
if ($slice.sample) { $summary = 'Sample: ' + $summary }

if ($request.hook -ne 'command') {
    $notes = @($summary)
    $warnings = @($slice.warnings | Where-Object { $null -ne $_ })
    if ($warnings.Count -gt 0) { $notes += ('{0} slicing warning(s): look at the plate before printing.' -f $warnings.Count) }
    $answer.notes = $notes
    Write-Answer $answer
    exit 0
}

# The command: a Markdown report for you to save.
$plate = 'Plate {0}' -f [int]$slice.plate
if ($slice.plateName) { $plate += ' (' + (([string]$slice.plateName) -replace '[\r\n]+', ' ') + ')' }
$lines = @()
$lines += '# Slice report: ' + $plate
$lines += ''
$lines += 'Made by the Slice report extension on ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss') + ', local time.'
if ($slice.sample) { $lines += ''; $lines += 'These are sample values, not a real slice.' }
$lines += ''
$lines += '- Print time: ' + (Format-Duration $slice.seconds) + ' (preparation ' + (Format-Duration $slice.prepareSeconds) + ')'
$lines += '- Layers: ' + [int]$slice.layers
$lines += '- Plate type: ' + [string]$slice.bedType
$lines += '- Filament changes: ' + [int]$slice.filamentChanges
$lines += ('- Filament: {0:N1} g, {1:N2} m' -f [double]$slice.totalGrams, [double]$slice.totalMeters)
if ($null -ne $slice.totalCost) { $lines += ('- Filament cost: {0:N2}' -f [double]$slice.totalCost) }
$lines += ''
$lines += '## Filament per slot'
$lines += ''
$lines += '| Slot | Type | Colour | Grams | Metres | Cost |'
$lines += '| --- | --- | --- | ---: | ---: | ---: |'
foreach ($f in @($slice.filaments)) {
    if ($null -eq $f) { continue }
    $cost = ''
    if ($null -ne $f.cost) { $cost = '{0:N2}' -f [double]$f.cost }
    $lines += ('| {0} | {1} | {2} | {3:N1} | {4:N2} | {5} |' -f [int]$f.slot, [string]$f.type, [string]$f.color, [double]$f.grams, [double]$f.meters, $cost)
}
$warnings = @($slice.warnings | Where-Object { $null -ne $_ })
$lines += ''
$lines += '## Slicing warnings'
$lines += ''
if ($warnings.Count -eq 0) { $lines += 'None.' }
foreach ($w in $warnings) {
    $text = [string]$w.message
    if (-not $text) { $text = [string]$w.code }
    $lines += ('- {0}: {1}' -f [string]$w.level, ($text -replace '[\r\n]+', ' '))
}
if ($null -ne $printer) {
    $lines += ''
    $lines += '## Printer'
    $lines += ''
    $lines += '- Preset: ' + [string]$printer.preset
    $lines += '- Nozzles: ' + ((@($printer.nozzles) | ForEach-Object { '{0} mm' -f $_ }) -join ', ')
}
$name = 'slice-report-plate-{0}.md' -f [int]$slice.plate
$answer.file = @{ name = $name; text = ($lines -join "`n") + "`n" }
$answer.notes = @('Report ready: ' + $summary)
Write-Answer $answer
