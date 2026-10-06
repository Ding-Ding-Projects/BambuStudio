#requires -Version 7.0
param([Parameter(Mandatory)][string]$First,[Parameter(Mandatory)][string]$Second)
$ErrorActionPreference='Stop'
if([IO.Path]::GetFullPath($First) -eq [IO.Path]::GetFullPath($Second)){throw 'Reproducibility requires distinct output directories.'}
foreach($name in @('qpdf30.dll','qpdf.exe')){
 $a=Get-FileHash -LiteralPath (Join-Path $First $name) -Algorithm SHA256
 $b=Get-FileHash -LiteralPath (Join-Path $Second $name) -Algorithm SHA256
 if($a.Hash -ne $b.Hash){throw "Independent native outputs differ: $name"}
}
$a=Get-Content (Join-Path $First 'manifest.json') -Raw|ConvertFrom-Json
$b=Get-Content (Join-Path $Second 'manifest.json') -Raw|ConvertFrom-Json
if(($a.provenance|ConvertTo-Json -Depth 20 -Compress) -ne ($b.provenance|ConvertTo-Json -Depth 20 -Compress)){throw 'Build provenance differs.'}
Write-Output 'PASS: 3 native reproducibility checks, two binaries and build provenance.'
