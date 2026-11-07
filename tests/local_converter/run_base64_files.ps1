param([switch]$Mutant)
$ErrorActionPreference='Stop'
$sourceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vcvars=Join-Path $env:LOCALAPPDATA 'TrafficMonitorBuildTools/VS2022/VC/Auxiliary/Build/vcvars64.bat'
if(-not(Test-Path -LiteralPath $vcvars)){throw 'Installed MSVC toolchain unavailable; native core verification is unverified.'}
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('bambu-base64-core-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$core=Join-Path $sourceRoot 'src/libslic3r/LocalConverter/Converter.cpp'
if($Mutant){
    $text=[IO.File]::ReadAllText($core)
    $needle='check(content_sha256(roundtrip.output)==content_sha256(encode?input:base64_payload(input)),"output_validation");'
    if(-not $text.Contains($needle)){throw 'Mutation target missing.'}
    $text=$text.Replace($needle,'check(true,"output_validation");')
    $core=Join-Path $testRoot 'Converter.cpp'
    [IO.File]::WriteAllText($core,$text,[Text.UTF8Encoding]::new($false))
}
$command='cl /nologo /std:c++17 /EHsc /utf-8 /I"'+$sourceRoot+'\src" /I"'+$sourceRoot+'\src\libslic3r\LocalConverter" "'+$sourceRoot+'\tests\local_converter\converter_tests.cpp" "'+$core+'" "'+$sourceRoot+'\src\libslic3r\LocalConverter\ArchiveAdapter.cpp" "'+$sourceRoot+'\src\libslic3r\LocalConverter\Worker.cpp" "'+$sourceRoot+'\src\miniz\miniz.c" /Fe:converter-tests.exe /link bcrypt.lib userenv.lib advapi32.lib'
$lines=@('@echo off',('call "'+$vcvars+'" >nul'),('cd /d "'+$testRoot+'"'),$command,'if errorlevel 1 exit /b %errorlevel%','converter-tests.exe')
$launcher=Join-Path $testRoot 'run.cmd'
[IO.File]::WriteAllLines($launcher,$lines,[Text.UTF8Encoding]::new($false))
& $launcher
exit $LASTEXITCODE
