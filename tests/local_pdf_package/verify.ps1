#requires -Version 7.0
param([string]$CacheDirectory = (Join-Path $PSScriptRoot '../../artifacts/local-pdf-cache'))
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$installer = Join-Path $root 'scripts/windows/Install-LocalPdfTools.ps1'
$scratch = Join-Path $root ('artifacts/pdf-package-test-' + [Guid]::NewGuid().ToString('N'))
$runtime = Join-Path $scratch 'runtime'
$sdk = Join-Path $scratch 'sdk'
& $installer -Destination $runtime -SdkDestination $sdk -CacheDirectory $CacheDirectory -Offline
& $installer -Destination $runtime -SdkDestination $sdk -VerifyOnly
& $installer -Destination $runtime -SdkDestination $sdk -Offline
$count=3
function Expect-Rejected([scriptblock]$Action) {
    $rejected=$false
    try { & $Action | Out-Null } catch { $rejected=$true }
    if (!$rejected) { throw 'Negative package check unexpectedly succeeded.' }
}
$dll=Join-Path $runtime 'qpdf30.dll'
$original=[IO.File]::ReadAllBytes($dll)
try {
    [IO.File]::WriteAllBytes($dll, [byte[]]@(0,1,2))
    Expect-Rejected { & $installer -Destination $runtime -VerifyOnly }
    Expect-Rejected { & $installer -Destination $runtime -Offline }
    $count+=2
} finally { [IO.File]::WriteAllBytes($dll,$original) }
$extra=Join-Path $runtime 'unlisted.dll'
[IO.File]::WriteAllText($extra,'unexpected')
Expect-Rejected { & $installer -Destination $runtime -VerifyOnly }
[IO.File]::Move($extra,(Join-Path $scratch 'unlisted.dll'))
$count++
$license=Join-Path $runtime 'licenses/qpdf-LICENSE.txt'
[IO.File]::Move($license,($license+'.held'))
Expect-Rejected { & $installer -Destination $runtime -VerifyOnly }
[IO.File]::Move(($license+'.held'),$license)
$count++
Expect-Rejected { & $installer -Destination (Join-Path $scratch 'missing') -CacheDirectory (Join-Path $scratch 'no-cache') -Offline }
$count++
$packageManifest=Join-Path $runtime 'manifest.json'
$manifestBytes=[IO.File]::ReadAllBytes($packageManifest)
try {
    [IO.File]::WriteAllText($packageManifest,'{}')
    Expect-Rejected { & $installer -Destination $runtime -VerifyOnly }
    $count++
} finally { [IO.File]::WriteAllBytes($packageManifest,$manifestBytes) }
$badCache=Join-Path $scratch 'bad-cache'
[IO.Directory]::CreateDirectory($badCache)|Out-Null
[IO.File]::WriteAllText((Join-Path $badCache 'qpdf-12.4.2-msvc64.zip'),'bad')
Expect-Rejected { & $installer -Destination (Join-Path $scratch 'bad-destination') -CacheDirectory $badCache -Offline }
$count++
& $installer -Destination $runtime -VerifyOnly
$qpdf=Join-Path $runtime 'qpdf.exe'
$version=& $qpdf --version
if ($LASTEXITCODE -ne 0 -or $version[0] -ne 'qpdf version 12.4.2') { throw 'Unexpected qpdf runtime version.' }
$count++
# An owned, content-free PDF with two differently sized pages verifies a real transformation.
$objects=@('<< /Type /Catalog /Pages 2 0 R >>','<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>','<< /Type /Page /Parent 2 0 R /Resources << >> /MediaBox [0 0 100 100] >>','<< /Type /Page /Parent 2 0 R /Resources << >> /MediaBox [0 0 200 200] >>')
$pdf="%PDF-1.4`n"
$offsets=@(0)
for($i=0;$i -lt $objects.Count;$i++){ $offsets+=[Text.Encoding]::ASCII.GetByteCount($pdf); $pdf+="$($i+1) 0 obj`n$($objects[$i])`nendobj`n" }
$xref=[Text.Encoding]::ASCII.GetByteCount($pdf)
$pdf+="xref`n0 5`n0000000000 65535 f `n"
foreach($offset in $offsets[1..4]){$pdf+=('{0:0000000000} 00000 n ' -f $offset)+"`n"}
$pdf+="trailer`n<< /Size 5 /Root 1 0 R >>`nstartxref`n$xref`n%%EOF`n"
$inputPdf=Join-Path $scratch 'synthetic.pdf'
$outputPdf=Join-Path $scratch 'rotated.pdf'
[IO.File]::WriteAllText($inputPdf,$pdf,[Text.Encoding]::ASCII)
$oldPath=$env:PATH
try {
    $env:PATH="$env:SystemRoot/system32"
    & $qpdf $inputPdf --check | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Synthetic PDF did not validate.'}
    & $qpdf $inputPdf --rotate=+90:1 $outputPdf
    if($LASTEXITCODE -ne 0){throw 'Rotation failed.'}
    & $qpdf $outputPdf --check | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Rotated PDF did not validate.'}
    $pages=& $qpdf $outputPdf --show-npages
    if($LASTEXITCODE -ne 0 -or $pages -ne '2'){throw 'Page count mismatch.'}
    $json=(& $qpdf $outputPdf --json | Out-String)|ConvertFrom-Json
    if($LASTEXITCODE -ne 0){throw 'JSON inspection failed.'}
    $page=$json.qpdf[1].PSObject.Properties | Where-Object { $_.Value.value.'/Type' -eq '/Page' -and $_.Value.value.'/Rotate' -eq 90 }
    if(!$page){throw 'Rotation metadata missing.'}
    $count++
} finally { $env:PATH=$oldPath }
Write-Output "PASS: $count local PDF package checks; synthetic operation only, application sandbox remains separately verified."
