#requires -Version 7.0
param([Parameter(Mandatory)][string]$RuntimeDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$RuntimeDirectory=[IO.Path]::GetFullPath($RuntimeDirectory)
$trusted=Join-Path $RuntimeDirectory 'manifest.json'
$manifest=Get-Content $trusted -Raw|ConvertFrom-Json
if($manifest.distribution -ne 'local-source-build-native-crypto' -or $manifest.provenance.sourceModified -ne $false -or $manifest.provenance.restrictedWorkerVerified -ne $false){throw 'Incorrect native build provenance.'}
$installer=Join-Path $root 'scripts/windows/Install-LocalPdfTools.ps1'
$scratch=Join-Path $root ('artifacts/native-package-test-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$copy=Join-Path $scratch 'copied-runtime'
Copy-Item -LiteralPath $RuntimeDirectory -Destination $copy -Recurse
& $installer -Destination $copy -VerifyOnly -TrustedManifestPath $trusted
function Expect-Rejected([scriptblock]$Action){$rejected=$false;try{& $Action|Out-Null}catch{$rejected=$true};if(!$rejected){throw 'Native negative check unexpectedly succeeded.'}}
Expect-Rejected {& $installer -Destination $copy -TrustedManifestPath $trusted}
$dll=Join-Path $copy 'qpdf30.dll'
$bytes=[IO.File]::ReadAllBytes($dll)
try{[IO.File]::WriteAllBytes($dll,[byte[]]@(0,1,2));Expect-Rejected {& $installer -Destination $copy -VerifyOnly -TrustedManifestPath $trusted}}finally{[IO.File]::WriteAllBytes($dll,$bytes)}
$copyManifest=Join-Path $copy 'manifest.json'
$manifestBytes=[IO.File]::ReadAllBytes($copyManifest)
try{[IO.File]::WriteAllText($copyManifest,'{}');Expect-Rejected {& $installer -Destination $copy -VerifyOnly -TrustedManifestPath $trusted}}finally{[IO.File]::WriteAllBytes($copyManifest,$manifestBytes)}
& $installer -Destination $copy -VerifyOnly -TrustedManifestPath $trusted
$qpdf=Join-Path $copy 'qpdf.exe'
$crypto=& $qpdf --show-crypto
if($LASTEXITCODE -ne 0 -or @($crypto).Count -ne 1 -or $crypto -ne 'native'){throw 'Native crypto mode is incorrect.'}
$empty=Join-Path $scratch 'empty.pdf'
& $qpdf --empty $empty
if($LASTEXITCODE -ne 0){throw 'Synthetic empty PDF creation failed.'}
& $qpdf $empty --check 2>&1|Out-Null
if($LASTEXITCODE -ne 2){throw 'Zero-page PDF was not rejected by validation.'}
$count=& $qpdf $empty --show-npages
if($LASTEXITCODE -ne 0 -or $count -ne '0'){throw 'Synthetic page count mismatch.'}
Write-Output 'PASS: 7 native package checks; application sandbox proof remains separate.'
