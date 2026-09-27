<#
.SYNOPSIS
Builds the unmodified official Windows source on a hosted runner.
.DESCRIPTION
This diagnostic route bootstraps the official dependency superbuild and then
installs the native application. It deliberately performs no UI execution,
model opening, installer publication, or source replacement.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$diagnostic = Join-Path $root 'diagnostic'
$metadata = $null
New-Item -ItemType Directory -Force -Path $diagnostic | Out-Null
Start-Transcript -Path (Join-Path $diagnostic 'build.log') -Force | Out-Null

function Assert-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required build tool is unavailable: $Name"
    }
}

function Invoke-Native {
    param([string]$Description, [scriptblock]$Action)
    Write-Host "== $Description =="
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

function Install-ChocolateyIfMissing {
    param([string]$CommandName, [string]$Package, [string]$Version)
    if (Get-Command $CommandName -ErrorAction SilentlyContinue) { return }
    Assert-Command choco
    Invoke-Native "Install $Package $Version" {
        choco install $Package --version=$Version --yes --no-progress
    }
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = "$machine;$user;$env:Path"
    Assert-Command $CommandName
}

try {
    Set-Location $root
    foreach ($tool in @('git', 'cmake', 'msbuild')) { Assert-Command $tool }
    $cmakeExecutable = (Get-Command cmake.exe -ErrorAction Stop).Source
    $cmakeDirectory = Split-Path -Parent $cmakeExecutable
    $source = (git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source commit.' }
    $depsTree = (git rev-parse 'HEAD:deps').Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the dependency tree.' }
    $srcTree = (git rev-parse 'HEAD:src').Trim()
    $resourcesTree = (git rev-parse 'HEAD:resources').Trim()
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
    $sdk = Get-ChildItem -Path $sdkRoot -Directory -ErrorAction Stop |
        Where-Object { Test-Path (Join-Path $_.FullName 'winrt\windows.graphics.printing3d.h') } |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $sdk) { throw 'Windows SDK with windows.graphics.printing3d.h is unavailable.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'Visual Studio installation locator is unavailable.' }
    $vsArgs = @('-latest', '-version', '[18.0,19.0)', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
    $vs = [string](& $vswhere @vsArgs -property installationPath)
    if ($LASTEXITCODE -ne 0 -or -not $vs.Trim()) { throw 'Visual Studio 2026 C++ toolset is unavailable.' }
    $vs = $vs.Trim()
    $vsVersionText = [string](& $vswhere @vsArgs -property installationVersion)
    if ($LASTEXITCODE -ne 0 -or -not $vsVersionText.Trim()) { throw 'Visual Studio installation version is unavailable.' }
    $vsVersionText = $vsVersionText.Trim()
    $generator = 'Visual Studio 18 2026'
    $cmakeVersionText = (& $cmakeExecutable --version | Select-Object -First 1)
    if ($cmakeVersionText -notmatch '^cmake version (\d+\.\d+\.\d+)') {
        throw "Cannot identify CMake version: $cmakeVersionText"
    }
    $cmakeVersion = [version]$Matches[1]
    if ($cmakeVersion -lt [version]'4.2.0') {
        throw "Visual Studio 2026 requires CMake 4.2 or newer; found $cmakeVersion."
    }
    $metadata = [ordered]@{
        source_sha = $source
        official_tag = 'v02.08.04.57'
        official_tag_commit = 'f977235e6d736c4c0b650520ac5a5b72cbfe9244'
        dependency_tree = $depsTree
        src_tree = $srcTree
        resources_tree = $resourcesTree
        generator = $generator
        cmake_version = $cmakeVersion.ToString()
        cmake_executable = $cmakeExecutable
        cmake_sha256 = (Get-FileHash -LiteralPath $cmakeExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
        configuration = 'Release'
        sdk_include = $sdk.FullName
        visual_studio = $vs
        visual_studio_version = $vsVersionText
        dependency_cache = 'official tree keyed by workflow hashFiles(deps/**)'
        result = 'running'
    }
    $metadata | ConvertTo-Json | Set-Content (Join-Path $diagnostic 'build-metadata.json')

    if ($depsTree -ne '8e648ef0f91f8d5ef2b0844e1e252be91ce81ac9' -or
        $srcTree -ne '8d5c74a1221bfcdd6b8d156f1c3484887c391ac7' -or
        $resourcesTree -ne '96f2307bdec8f693029e4646db8dbb2cb25fe90a') {
        throw 'Native source, dependencies, or resources differ from the official baseline.'
    }

    Install-ChocolateyIfMissing -CommandName 'pkg-config.exe' -Package 'pkgconfiglite' -Version '0.28.0'
    $strawberryPerl = 'C:\Strawberry\perl\bin\perl.exe'
    if (-not (Test-Path $strawberryPerl)) {
        Assert-Command choco
        Invoke-Native 'Install Strawberry Perl' { choco install strawberryperl --yes --no-progress }
    }
    if (-not (Test-Path $strawberryPerl)) { throw 'Strawberry Perl is unavailable after bootstrap.' }
    $env:Path = "C:\Strawberry\c\bin;C:\Strawberry\perl\site\bin;C:\Strawberry\perl\bin;$env:Path"
    # Chocolatey may prepend an older CMake to PATH. Preserve the verified
    # executable for both direct calls and child tools that invoke `cmake`.
    $env:Path = "$cmakeDirectory;$env:Path"
    $resolvedCmake = (Get-Command cmake.exe -ErrorAction Stop).Source
    if ($resolvedCmake -ne $cmakeExecutable) {
        throw "CMake changed after bootstrap: expected $cmakeExecutable, found $resolvedCmake"
    }
    $env:LANG = 'C'; $env:LC_ALL = 'C'; $env:LC_CTYPE = 'C'
    Invoke-Native 'Verify Strawberry Perl module' { & $strawberryPerl -MLocale::Maketext::Simple -e 1 }

    $destination = Join-Path $root 'deps\build\BambuStudio_dep'
    $prefix = Join-Path $destination 'usr\local'
    if (-not (Test-Path (Join-Path $prefix 'include'))) {
        Invoke-Native 'Configure official dependencies' {
            & $cmakeExecutable -S deps -B deps/build -G $generator -A x64 "-DDESTDIR=$destination" -DDEP_DEBUG=OFF
        }
        Invoke-Native 'Build official dependencies' {
            & $cmakeExecutable --build deps/build --target ALL_BUILD --config Release --parallel 4
        }
    }
    if (-not (Test-Path (Join-Path $prefix 'include'))) {
        throw 'Dependency build did not produce the required usr/local/include prefix.'
    }

    Invoke-Native 'Configure official native application' {
        & $cmakeExecutable -S . -B build -G $generator -A x64 `
            -DBBL_RELEASE_TO_PUBLIC=1 -DBBL_INTERNAL_TESTING=0 `
            -DSLIC3R_MSVC_PDB=ON -DSLIC3R_BUILD_TESTS=OFF `
            "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_INSTALL_PREFIX=$root\install-dir" `
            "-DWIN10SDK_PATH=$($sdk.FullName)"
    }
    Invoke-Native 'Build and install official native application' {
        & $cmakeExecutable --build build --target install --config Release --parallel 4
    }
    $exe = Get-ChildItem -Path (Join-Path $root 'install-dir') -Filter 'bambu-studio.exe' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $exe) { throw 'The installed native payload does not contain bambu-studio.exe.' }
    $metadata.result = 'built'
    $metadata.executable_sha256 = (Get-FileHash -LiteralPath $exe.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "Official native executable SHA-256: $($metadata.executable_sha256)"
}
catch {
    if ($metadata) { $metadata.result = 'failed'; $metadata.failure = $_.Exception.Message }
    throw
}
finally {
    if ($metadata) { $metadata | ConvertTo-Json | Set-Content (Join-Path $diagnostic 'build-metadata.json') }
    Stop-Transcript | Out-Null
}
