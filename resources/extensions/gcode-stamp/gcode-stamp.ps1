# G-code time stamp, an extension that comes with Bambu Studio.
# Answers comment lines for the start and the end of an exported G-code file.
# Bambu Studio puts the start lines after its header block and introduces
# them with a line naming this extension. One JSON request arrives on standard
# input and one JSON answer goes to standard output
# (docs/features/windows/extensions.md). Windows PowerShell 5.1 or later.
$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$reader = [System.IO.StreamReader]::new([Console]::OpenStandardInput(), $utf8)
$request = $reader.ReadToEnd() | ConvertFrom-Json
$slice = $request.context.slice

function Format-Duration([double]$seconds) {
    $total = [int][math]::Floor([math]::Max(0, $seconds) + 0.5)
    $h = [int][math]::Floor($total / 3600)
    $m = [int][math]::Floor(($total % 3600) / 60)
    if ($h -gt 0) { return ('{0} h {1} min' -f $h, $m) }
    return ('{0} min' -f $m)
}

function Get-Line([string]$text) {
    # One comment line: no line breaks, at most 200 characters.
    $one = ';' + (($text -replace '[\r\n\t]+', ' ').TrimEnd())
    if ($one.Length -gt 200) { $one = $one.Substring(0, 197) + '...' }
    return $one
}

$start = @(Get-Line (' exported {0} local time' -f (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')))
$end = @()
if ($null -ne $slice) {
    $plate = 'plate {0}' -f [int]$slice.plate
    if ($slice.plateName) { $plate += (' "{0}"' -f [string]$slice.plateName) }
    $grams = 0.0
    $types = @()
    foreach ($f in @($slice.filaments)) {
        if ($null -eq $f) { continue }
        $grams += [double]$f.grams
        if ($types -notcontains [string]$f.type) { $types += [string]$f.type }
    }
    $start += Get-Line (' {0}: {1}, {2:N1} g of {3}, {4} layers' -f $plate, (Format-Duration $slice.seconds), $grams, ($types -join ' and '), [int]$slice.layers)
    if ($slice.sample) { $start += Get-Line ' these are sample values, not a real slice' }
    $end += Get-Line (' end of {0}' -f $plate)
}

$gcode = @{ prepend = @($start) }
if ($end.Count -gt 0) { $gcode.append = @($end) }
$answer = @{ apiVersion = 1; gcode = $gcode }
$writer = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $utf8)
$writer.Write(($answer | ConvertTo-Json -Compress -Depth 6))
$writer.Flush()
