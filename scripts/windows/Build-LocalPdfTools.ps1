#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Destination,
    [Parameter(Mandatory)][string]$BuildRoot,
    [string]$SdkDestination,
    [string]$CompilerRoot = (Join-Path $env:LOCALAPPDATA 'BambuStudioMD3/toolchain/BuildTools2026'),
    [string]$CacheDirectory = (Join-Path $PSScriptRoot '../../artifacts/local-pdf-cache'),
    [ValidateRange(1,16)][int]$Parallel = 2,
    [switch]$Offline
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$specPath=Join-Path $PSScriptRoot 'local-pdf-tools.json'
$spec=Get-Content $specPath -Raw|ConvertFrom-Json
$Destination=[IO.Path]::GetFullPath($Destination)
$BuildRoot=[IO.Path]::GetFullPath($BuildRoot)
$CacheDirectory=[IO.Path]::GetFullPath($CacheDirectory)
if(Test-Path -LiteralPath $Destination){throw 'Native runtime destination already exists; use a fresh staging directory.'}
if(Test-Path -LiteralPath $BuildRoot){throw 'Native build root already exists; use a fresh build directory.'}
function Assert-PlainPath([string]$Path){
    for($cursor=[IO.Path]::GetFullPath($Path);$cursor;$cursor=[IO.Path]::GetDirectoryName($cursor)){
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Reparse points are not allowed in native build paths.'}
    }
}
foreach($p in @($Destination,$BuildRoot,$CacheDirectory,$CompilerRoot)){Assert-PlainPath $p}
function Assert-Hash([string]$Path,[string]$Hash,[long]$Bytes){
    if(!(Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -ne $Bytes -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash){throw 'Pinned native build input did not verify.'}
}
$vcvars=Join-Path $CompilerRoot 'VC/Auxiliary/Build/vcvars64.bat'
$bin=Join-Path $CompilerRoot "VC/Tools/MSVC/$($spec.nativeSource.compilerVersion)/bin/Hostx64/x64"
$cmake=Join-Path $CompilerRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja=Join-Path $CompilerRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$dumpbin=Join-Path $bin 'dumpbin.exe'
$cl=Join-Path $bin 'cl.exe'
foreach($p in @($vcvars,$cmake,$ninja,$dumpbin,$cl)){if(!(Test-Path -LiteralPath $p -PathType Leaf)){throw 'The pinned project MSVC toolchain is unavailable.'}}
[IO.Directory]::CreateDirectory($BuildRoot)|Out-Null
[IO.Directory]::CreateDirectory($CacheDirectory)|Out-Null
foreach($inputSpec in @($spec.nativeSource.source,$spec.nativeSource.dependencies)){
    $archive=Join-Path $CacheDirectory $inputSpec.archive
    if(!(Test-Path -LiteralPath $archive)){
        if($Offline){throw 'Native source archive is unavailable in the offline cache.'}
        $download=Join-Path $BuildRoot ('download-'+[Guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($download)|Out-Null
        & gh release download $spec.tag --repo $spec.repository --pattern $inputSpec.archive --dir $download
        if($LASTEXITCODE -ne 0){throw 'Official native source download failed.'}
        $downloaded=Join-Path $download $inputSpec.archive
        Assert-Hash $downloaded $inputSpec.sha256 $inputSpec.bytes
        [IO.File]::Move($downloaded,$archive,$false)
    }
    Assert-Hash $archive $inputSpec.sha256 $inputSpec.bytes
}
$sourceParent=Join-Path $BuildRoot 'source'
[IO.Directory]::CreateDirectory($sourceParent)|Out-Null
$tar=Join-Path $env:SystemRoot 'System32/tar.exe'
if(!(Test-Path -LiteralPath $tar)){throw 'Windows archive extraction tool is unavailable.'}
& $tar -xzf (Join-Path $CacheDirectory $spec.nativeSource.source.archive) -C $sourceParent
if($LASTEXITCODE -ne 0){throw 'Pinned source extraction failed.'}
$source=Join-Path $sourceParent "qpdf-$($spec.version)"
$deps=Join-Path $BuildRoot 'dependencies'
$zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $CacheDirectory $spec.nativeSource.dependencies.archive))
try{
 foreach($entry in $zip.Entries){
  $prefix='vcpkg/installed/x64-windows-static/'
  if(!$entry.FullName.StartsWith($prefix,[StringComparison]::Ordinal)){continue}
  $relative=$entry.FullName.Substring($prefix.Length)
  if(!$entry.Name -or !($relative.StartsWith('include/') -or $relative -in @('lib/zs.lib','lib/jpeg.lib'))){continue}
  if($relative -notmatch '^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$' -or $relative.Split('/') -contains '..'){throw 'Invalid dependency archive path.'}
  $path=Join-Path $deps $relative
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
  [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path,$false)
 }
}finally{$zip.Dispose()}
$stage=Join-Path $BuildRoot 'runtime'
& (Join-Path $PSScriptRoot 'Install-LocalPdfTools.ps1') -Destination $stage -SdkDestination $SdkDestination -CacheDirectory $CacheDirectory -Offline:$Offline
# Environment changes apply only to this build process. Never discover a compiler from PATH.
$envLines=& $env:ComSpec /d /s /c "`"$vcvars`" -vcvars_ver=$($spec.nativeSource.compilerVersion) >nul && set"
if($LASTEXITCODE -ne 0){throw 'Project compiler environment failed.'}
foreach($line in $envLines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$build=Join-Path $BuildRoot 'build'
$flags='/experimental:deterministic /Brepro /pathmap:"'+$BuildRoot+'=/qpdf-build"'
$options=@($spec.nativeSource.cmakeOptions)+@("-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_C_COMPILER=$cl","-DCMAKE_CXX_COMPILER=$cl","-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /W3 $flags","-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /W3 /GR /EHsc $flags",'-DCMAKE_SHARED_LINKER_FLAGS=/Brepro','-DCMAKE_EXE_LINKER_FLAGS=/Brepro',"-DZLIB_H_PATH=$deps/include","-DZLIB_LIB_PATH=$deps/lib/zs.lib","-DLIBJPEG_H_PATH=$deps/include","-DLIBJPEG_LIB_PATH=$deps/lib/jpeg.lib")
& $cmake -S $source -B $build -G Ninja @options
if($LASTEXITCODE -ne 0){throw 'Native qpdf configure failed.'}
& $cmake --build $build --target libqpdf qpdf --parallel $Parallel
if($LASTEXITCODE -ne 0){throw 'Native qpdf build failed.'}
Copy-Item -LiteralPath (Join-Path $build 'libqpdf/qpdf30.dll') -Destination (Join-Path $stage 'qpdf30.dll')
Copy-Item -LiteralPath (Join-Path $build 'qpdf/qpdf.exe') -Destination (Join-Path $stage 'qpdf.exe')
$version=& (Join-Path $stage 'qpdf.exe') --version
if($LASTEXITCODE -ne 0 -or $version[0] -ne "qpdf version $($spec.version)"){throw 'Native qpdf version mismatch.'}
$crypto=& (Join-Path $stage 'qpdf.exe') --show-crypto
if($LASTEXITCODE -ne 0 -or @($crypto).Count -ne 1 -or $crypto -ne 'native'){throw 'Native crypto selection did not verify.'}
$imports=& $dumpbin /dependents (Join-Path $stage 'qpdf30.dll')
if($LASTEXITCODE -ne 0 -or ($imports -match '^\s+(USER32|GDI32|WS2_32|CRYPT32)\.dll\s*$')){throw 'Native PDF runtime has an unexpected desktop or external crypto dependency.'}
$imports|Set-Content (Join-Path $BuildRoot 'imports.txt') -Encoding utf8
foreach($file in $spec.files){
 $path=Join-Path $stage $file.path
 $file.sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
 $file.bytes=(Get-Item -LiteralPath $path).Length
 if($file.path -in @('qpdf30.dll','qpdf.exe')){$file.PSObject.Properties.Remove('archivePath');$file|Add-Member origin 'local-source-build'}
}
$spec|Add-Member distribution 'local-source-build-native-crypto'
$provenance=[ordered]@{sourceArchive=$spec.nativeSource.source.archive;sourceSHA256=$spec.nativeSource.source.sha256;dependencyArchive=$spec.nativeSource.dependencies.archive;dependencySHA256=$spec.nativeSource.dependencies.sha256;compilerVersion=$spec.nativeSource.compilerVersion;compilerSHA256=(Get-FileHash $cl -Algorithm SHA256).Hash.ToLowerInvariant();cmakeSHA256=(Get-FileHash $cmake -Algorithm SHA256).Hash.ToLowerInvariant();ninjaSHA256=(Get-FileHash $ninja -Algorithm SHA256).Hash.ToLowerInvariant();configuration=@($spec.nativeSource.cmakeOptions);reproducibleFlags='/experimental:deterministic /Brepro /pathmap:build-root=/qpdf-build';sourceModified=$false;restrictedWorkerVerified=$false}
$spec|Add-Member provenance $provenance
$spec.components=@($spec.components|Where-Object name -ne 'OpenSSL')
$bom=$spec.metadataFiles[0].text|ConvertFrom-Json
$bom.components=@($bom.components|Where-Object name -ne 'OpenSSL')
foreach($component in $bom.components){if($component.type -eq 'file'){$component.hashes[0].content=($spec.files|Where-Object path -eq $component.name).sha256}}
$bomText=($bom|ConvertTo-Json -Depth 30)+"`n"
$spec.metadataFiles[0].text=$bomText
$spec.metadataFiles[0].sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($bomText))).ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $stage 'sbom.cdx.json'),$bomText,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'),($spec|ConvertTo-Json -Depth 40)+"`n",[Text.UTF8Encoding]::new($false))
# Inspect ASCII and UTF-16 representations before the candidate can leave its build directory.
$privateRoots=@($env:USERPROFILE,$BuildRoot,$PSScriptRoot)|Where-Object {$_}
foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){
 $bytes=[IO.File]::ReadAllBytes($file.FullName)
 foreach($encoding in @([Text.Encoding]::UTF8,[Text.Encoding]::Unicode)){
  $text=$encoding.GetString($bytes)
  foreach($privateRoot in $privateRoots){foreach($needle in @($privateRoot,$privateRoot.Replace('\','/'),$privateRoot.Replace('\','\\'))){if($text.Contains($needle,[StringComparison]::OrdinalIgnoreCase)){throw 'Private build path detected in native runtime output.'}}}
 }
}
foreach($file in $spec.files){Assert-Hash (Join-Path $stage $file.path) $file.sha256 $file.bytes}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Destination))|Out-Null
[IO.Directory]::Move($stage,$Destination)
Write-Output 'Native-only qpdf source build and package verified. Runtime pins must be compiled into the worker from this completed output manifest before use.'
Write-Output $Destination
