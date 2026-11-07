param([switch]$Mutant, [switch]$RepresentationMutant)
$ErrorActionPreference='Stop'
$sourceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$candidates=@()
if($env:LOCALAPPDATA){$candidates+=Join-Path $env:LOCALAPPDATA 'BambuBuildTools/VS2026'}
$locator=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(Test-Path -LiteralPath $locator){
    $candidates+=@(& $locator -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
}
$vcvars=$null
foreach($candidate in $candidates){
    $entry=Join-Path $candidate 'VC/Auxiliary/Build/vcvars64.bat'
    if(Test-Path -LiteralPath $entry -PathType Leaf){$vcvars=$entry;break}
}
if(-not $vcvars){throw 'Installed MSVC toolchain unavailable; native core verification is unverified.'}
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('bambu-base64-core-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$core=Join-Path $sourceRoot 'src/libslic3r/LocalConverter/Converter.cpp'
if($Mutant -or $RepresentationMutant){
    $text=[IO.File]::ReadAllText($core)
    $needle='check(content_sha256(roundtrip.output)==content_sha256(encode?input:base64_payload(input)),"output_validation");'
    if(-not $text.Contains($needle)){throw 'Mutation target missing.'}
    if($Mutant){$text=$text.Replace($needle,'check(true,"output_validation");')}
    if($RepresentationMutant){
        $limit='? Limits::base64_representation_bytes : Limits::input_bytes;'
        if(-not $text.Contains($limit)){throw 'Representation mutation target missing.'}
        $text=$text.Replace($limit,'? Limits::input_bytes : Limits::input_bytes;')
    }
    $core=Join-Path $testRoot 'Converter.cpp'
    [IO.File]::WriteAllText($core,$text,[Text.UTF8Encoding]::new($false))
}
$command='cl /nologo /std:c++17 /EHsc /utf-8 /I"'+$sourceRoot+'\src" /I"'+$sourceRoot+'\src\libslic3r\LocalConverter" "'+$sourceRoot+'\tests\local_converter\converter_tests.cpp" "'+$core+'" "'+$sourceRoot+'\src\libslic3r\LocalConverter\ArchiveAdapter.cpp" "'+$sourceRoot+'\src\libslic3r\LocalConverter\Worker.cpp" "'+$sourceRoot+'\src\miniz\miniz.c" /Fe:converter-tests.exe /link bcrypt.lib userenv.lib advapi32.lib'
$lines=@('@echo off',('call "'+$vcvars+'" >nul'),('cd /d "'+$testRoot+'"'),$command,'if errorlevel 1 exit /b %errorlevel%','converter-tests.exe')
$launcher=Join-Path $testRoot 'run.cmd'
[IO.File]::WriteAllLines($launcher,$lines,[Text.UTF8Encoding]::new($false))
& $launcher
exit $LASTEXITCODE
