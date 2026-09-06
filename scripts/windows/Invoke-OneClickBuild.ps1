<#
.SYNOPSIS
    Bootstrap, compile, and package Bambu Studio for Windows with one command.
.DESCRIPTION
    Installs missing ordinary build prerequisites through winget, builds the
    repository dependencies and Release application, stages the payload, adds
    the hash-pinned Mesa software OpenGL fallback, creates a CycloneDX SBOM,
    builds an unsigned Squirrel.Windows release, validates its Setup.exe,
    RELEASES index, and package, and writes a SHA-256 sidecar.

    Double-click OneClickBuildInstaller.cmd for the default incremental build.
    Use -Install only when the newly built unsigned installer should be run.
#>

[CmdletBinding()]
param(
    [ValidateSet('Incremental', 'Clean')]
    [string] $BuildMode = 'Incremental',

    [string] $OutputDirectory = '',

    [switch] $Install,

    [switch] $BootstrapOnly,

    [switch] $Plan,

    [switch] $BuildOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$script:LogDirectory = Join-Path $script:RepositoryRoot 'artifacts\windows'
$script:LogPath = Join-Path $script:LogDirectory 'one-click-build.log'
$script:TranscriptStarted = $false
$script:BuildMutex = $null
$script:BuildMutexHeld = $false

function Write-BuildLog {
    param([Parameter(Mandatory)][string] $Message)
    Write-Host ('[{0}] {1}' -f (Get-Date).ToString('u'), $Message)
}

function Assert-LastExitCode {
    param([Parameter(Mandatory)][string] $Action)
    if ($LASTEXITCODE -ne 0) {
        throw "$Action failed with exit code $LASTEXITCODE."
    }
}

function Get-FileSha256Lower {
    param([Parameter(Mandatory)][string] $Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Resolve-OutputDirectory {
    param([string] $RequestedPath)
    if ([string]::IsNullOrWhiteSpace($RequestedPath)) {
        return $script:LogDirectory
    }
    if ([System.IO.Path]::IsPathRooted($RequestedPath)) {
        return [System.IO.Path]::GetFullPath($RequestedPath)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $script:RepositoryRoot $RequestedPath))
}

function Get-SevenZipPath {
    $command = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($candidate in @(
        (Join-Path $env:ProgramFiles '7-Zip\7z.exe'),
        (Join-Path ${env:ProgramFiles(x86)} '7-Zip\7z.exe')
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return $null
}

function Install-WingetPackageIfMissing {
    param(
        [Parameter(Mandatory)][string] $DisplayName,
        [Parameter(Mandatory)][string] $PackageId,
        [Parameter(Mandatory)][scriptblock] $Probe
    )

    if (& $Probe) {
        Write-BuildLog "$DisplayName is already available."
        return
    }
    if ($Plan) {
        Write-BuildLog "PLAN: install $DisplayName from winget package $PackageId."
        return
    }
    if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
        throw "$DisplayName is missing and winget is unavailable. Install App Installer, then rerun."
    }

    Write-BuildLog "Installing $DisplayName ($PackageId)..."
    & winget.exe install --id $PackageId --exact --silent `
        --accept-package-agreements --accept-source-agreements
    $installExitCode = $LASTEXITCODE
    Update-SessionPath
    if (& $Probe) { return }

    if ($installExitCode -ne 0) {
        Write-BuildLog "Repairing a stale $DisplayName winget registration..."
        & winget.exe uninstall --id $PackageId --exact --silent `
            --accept-source-agreements
        $uninstallExitCode = $LASTEXITCODE
        if ($uninstallExitCode -ne 0) {
            throw "Removing the stale $DisplayName registration failed with exit code $uninstallExitCode."
        }
        & winget.exe install --id $PackageId --exact --silent `
            --accept-package-agreements --accept-source-agreements
        Assert-LastExitCode "Reinstalling $DisplayName"
        Update-SessionPath
    }
    if (-not (& $Probe)) {
        throw "$DisplayName installation completed but the tool is still unavailable."
    }
}

function Set-StrawberryPerlFirst {
    param([Parameter(Mandatory)][string] $PkgConfigPath)
    # The dependency build runs a bare `perl Configure` for OpenSSL. Git for
    # Windows ships its own msys perl, and Update-SessionPath APPENDS registry
    # entries, so Git's perl stayed first and OpenSSL died on a missing
    # Locale::Maketext::Simple (attempt five, 2026-09-05). Put Strawberry's
    # three bin directories at the front of this process's PATH and prove the
    # module loads before any dependency step runs.
    $perlBin = Split-Path -Parent $PkgConfigPath                 # ...\Strawberry\perl\bin
    $root = Split-Path -Parent (Split-Path -Parent $perlBin)     # ...\Strawberry
    $front = @(
        (Join-Path $root 'c\bin'),
        (Join-Path $root 'perl\site\bin'),
        $perlBin
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }
    $rest = ($env:Path -split ';') | Where-Object { $_ -and ($front -notcontains $_.TrimEnd('\')) }
    $env:Path = (($front + $rest) -join ';')
    $perl = Get-Command perl.exe -ErrorAction SilentlyContinue
    if ($null -eq $perl -or $perl.Source -notlike "$perlBin*") {
        throw "Strawberry Perl is not first on PATH after reordering (resolved '$($perl.Source)')."
    }
    & $perl.Source -MLocale::Maketext::Simple -e 1 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "The perl at $($perl.Source) cannot load Locale::Maketext::Simple, which OpenSSL's Configure requires."
    }
    Write-BuildLog "Using Perl at $($perl.Source) (Strawberry first on PATH; Locale::Maketext::Simple loads)."
}

function Resolve-PkgConfigExecutable {
    $native = Get-Command pkg-config.exe -ErrorAction SilentlyContinue
    if ($null -ne $native) {
        return $native.Source
    }

    # Strawberry Perl ships a maintained pkg-config implementation as a .bat
    # wrapper. CMake can execute the wrapper, while its extensionless sibling
    # is discoverable but cannot be launched by FindPkgConfig on Windows.
    $wrapper = Get-Command pkg-config.bat -ErrorAction SilentlyContinue
    if ($null -ne $wrapper) {
        return $wrapper.Source
    }

    throw 'pkg-config is missing. Install pkgconfiglite or Strawberry Perl, then rerun.'
}

function Normalize-PkgConfigFiles {
    param([Parameter(Mandatory)][string] $DependencyDestination)

    $pkgConfigDirectory = Join-Path $DependencyDestination 'usr\local\lib\pkgconfig'
    if (-not (Test-Path -LiteralPath $pkgConfigDirectory -PathType Container)) {
        return
    }
    $prefix = (Resolve-Path -LiteralPath (Join-Path $DependencyDestination 'usr\local')).Path.Replace('\', '/')
    foreach ($file in (Get-ChildItem -LiteralPath $pkgConfigDirectory -Filter '*.pc' -File)) {
        $content = [System.IO.File]::ReadAllText($file.FullName)
        $normalized = $content.Replace('prefix=./dist', "prefix=$prefix")
        $normalized = $normalized.Replace('libdir=./dist/lib', "libdir=$prefix/lib")
        $normalized = $normalized.Replace('includedir=./dist/include', "includedir=$prefix/include")
        if ($normalized -ne $content) {
            [System.IO.File]::WriteAllText(
                $file.FullName,
                $normalized,
                [System.Text.UTF8Encoding]::new($false)
            )
        }
    }
}

function Test-FreeDiskSpace {
    $driveRoot = [System.IO.Path]::GetPathRoot($script:RepositoryRoot)
    $drive = [System.IO.DriveInfo]::new($driveRoot)
    $freeGb = [math]::Round($drive.AvailableFreeSpace / 1GB, 1)
    Write-BuildLog "Free space on $driveRoot is $freeGb GB."
    if ($drive.AvailableFreeSpace -lt 40GB) {
        throw 'At least 40 GB of free disk space is required for a clean Windows build.'
    }
}

function Initialize-LocalToolchain {
    # Toolchain.ps1 is dot-sourced at script scope (see Import-ToolchainHelpers) so
    # its detection helpers stay visible to every later phase, not only this one.

    if ($Plan) {
        Write-BuildLog "PLAN: bootstrap Git, Visual Studio 2022 C++ tools, Windows SDK, and CMake."
    } else {
        $bootstrapDir = Join-Path $env:LOCALAPPDATA 'codingmachineedge\BambuStudioMD3-BuildTools'
        if (-not (Test-Path -LiteralPath $bootstrapDir)) {
            New-Item -ItemType Directory -Path $bootstrapDir -Force | Out-Null
        }
        Update-SessionPath
        Install-Git -WorkDir $bootstrapDir
        Install-VisualCppBuildTools -WorkDir $bootstrapDir
        Install-CMake -WorkDir $bootstrapDir
        Install-Node -WorkDir $bootstrapDir
        Update-SessionPath
    }

    # Probe for the pkg-config wrapper Strawberry Perl ships, not for perl.exe:
    # Git for Windows puts its own perl.exe on PATH, which satisfied the old
    # probe and left pkg-config permanently missing (verified 2026-09-05).
    Install-WingetPackageIfMissing -DisplayName 'Strawberry Perl' `
        -PackageId 'StrawberryPerl.StrawberryPerl' `
        -Probe { $null -ne (Get-Command pkg-config.exe, pkg-config.bat -ErrorAction SilentlyContinue) }
    if (-not $Plan) {
        # A package installed moments ago is only on the registry PATH, not on
        # this process's PATH, until the session path is refreshed.
        Update-SessionPath
        $pkgConfig = Resolve-PkgConfigExecutable
        $env:PKG_CONFIG_EXECUTABLE = $pkgConfig
        Write-BuildLog "Using pkg-config at $pkgConfig."
        Set-StrawberryPerlFirst -PkgConfigPath $pkgConfig
    }

    Install-WingetPackageIfMissing -DisplayName '7-Zip' -PackageId '7zip.7zip' `
        -Probe { $null -ne (Get-SevenZipPath) }

    if (-not $Plan) {
        $vsProduct = Get-VisualStudio2022Product
        $vsPath = Get-VisualStudio2022Path
        $sdkVersion = Get-WindowsSdkVersion
        if ([string]::IsNullOrWhiteSpace($vsProduct) -or
            [string]::IsNullOrWhiteSpace($vsPath) -or $null -eq $sdkVersion) {
            throw 'Visual Studio 2022 C++ tools or a complete Windows SDK could not be detected.'
        }
        Write-BuildLog "Toolchain ready: Visual Studio product=$vsProduct; SDK=$sdkVersion."
        return $vsProduct
    }
    return 'BuildTools'
}

function Invoke-RepositoryCommand {
    param(
        [Parameter(Mandatory)][string] $Label,
        [Parameter(Mandatory)][scriptblock] $Command
    )
    Write-BuildLog $Label
    if ($Plan) { return }
    & $Command
    Assert-LastExitCode $Label
}

function Invoke-DownloadWithRetry {
    param(
        [Parameter(Mandatory)][string] $Uri,
        [Parameter(Mandatory)][string] $OutFile,
        [int] $MaxAttempts = 3,
        [int] $RetryDelaySeconds = 15
    )

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing
            return
        }
        catch {
            if ($attempt -eq $MaxAttempts) {
                throw
            }
            Write-BuildLog "Download attempt $attempt failed; retrying in $RetryDelaySeconds seconds."
            Start-Sleep -Seconds $RetryDelaySeconds
        }
    }
}

function Add-MesaFallback {
    param(
        [Parameter(Mandatory)][string] $PayloadDirectory,
        [Parameter(Mandatory)][string] $SevenZip
    )

    $mesaVersion = '26.1.3'
    $archiveSha256 = '6dd431f4620cea73970b13e3ffa94f721f2a3924306b8a4283c97648cdb6eb9c'
    $fileHashes = @{
        'opengl32.dll'       = '12499866437a161d2b250d5105188ae00732dd74b4bebbcdf972e6145af00f9e'
        'libgallium_wgl.dll' = '1895f8c19ede5efd0497f9dfab463b19bf4377e3af7c06c2d4d073e4680c5f69'
    }
    $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
        ('BambuStudio-OneClick-' + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
        $archive = Join-Path $temporaryRoot "mesa3d-$mesaVersion-release-msvc.7z"
        $url = "https://github.com/pal1000/mesa-dist-win/releases/download/$mesaVersion/mesa3d-$mesaVersion-release-msvc.7z"
        Write-BuildLog "Downloading hash-pinned Mesa $mesaVersion fallback..."
        Invoke-DownloadWithRetry -Uri $url -OutFile $archive
        $actualArchiveHash = Get-FileSha256Lower -Path $archive
        if ($actualArchiveHash -ne $archiveSha256) {
            throw "Mesa archive SHA-256 mismatch; expected $archiveSha256, got $actualArchiveHash."
        }

        $extractDirectory = Join-Path $temporaryRoot 'extracted'
        & $SevenZip x $archive "-o$extractDirectory" 'x64\opengl32.dll' `
            'x64\libgallium_wgl.dll' -y | Out-Host
        Assert-LastExitCode 'Extracting Mesa fallback'

        $mesaPayload = Join-Path $PayloadDirectory 'mesa'
        New-Item -ItemType Directory -Path $mesaPayload -Force | Out-Null
        foreach ($name in $fileHashes.Keys) {
            $source = Join-Path $extractDirectory "x64\$name"
            $actualHash = Get-FileSha256Lower -Path $source
            if ($actualHash -ne $fileHashes[$name]) {
                throw "Mesa $name SHA-256 mismatch."
            }
            Copy-Item -LiteralPath $source -Destination (Join-Path $mesaPayload $name) -Force
        }
    }
    finally {
        if (Test-Path -LiteralPath $temporaryRoot) {
            $resolved = [System.IO.Path]::GetFullPath($temporaryRoot)
            $tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
            if (-not $resolved.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                [System.IO.Path]::GetFileName($resolved) -notlike 'BambuStudio-OneClick-*') {
                throw "Refusing to remove unexpected temporary directory '$resolved'."
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}

function Get-ProductVersion {
    $content = Get-Content -LiteralPath (Join-Path $script:RepositoryRoot 'version.inc') -Raw
    if ($content -notmatch 'set\(SLIC3R_VERSION "([^"]+)"\)') {
        throw 'Could not read SLIC3R_VERSION from version.inc.'
    }
    return $matches[1]
}

function Resolve-CMakeExecutable {
    param([Parameter(Mandatory)][string] $VisualStudioPath)
    # Never trust whichever cmake happens to be first on PATH. On this host that
    # was a MinGW (WinLibs) CMake whose curl has no Windows certificate store, so
    # every ExternalProject download failed with "certificate signer not trusted"
    # (build attempt four, 2026-09-05). Prefer the CMake that ships with the
    # Visual Studio C++ toolset, then a Kitware install, and prove the choice
    # with a real HTTPS download before handing it to the dependency build.
    $candidates = @(
        (Join-Path $VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\CMake\bin\cmake.exe')
    )
    $onPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $onPath -and $onPath.Source -notmatch 'mingw|msys|WinLibs') { $candidates += $onPath.Source }
    $probeScript = Join-Path $env:TEMP 'bambu-cmake-tls-probe.cmake'
    Set-Content -LiteralPath $probeScript -Encoding ascii -Value @(
        'file(DOWNLOAD "https://github.com/USCiLab/cereal/archive/refs/tags/v1.3.0.zip" "${OUT}" STATUS st TIMEOUT 60)',
        'list(GET st 0 code)',
        'if(NOT code EQUAL 0)',
        '  message(FATAL_ERROR "tls-probe-failed: ${st}")',
        'endif()'
    )
    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
        $probeOut = Join-Path $env:TEMP 'bambu-cmake-tls-probe.zip'
        & $candidate "-DOUT=$probeOut" -P $probeScript 2>&1 | Out-Null
        $ok = ($LASTEXITCODE -eq 0)
        Remove-Item -LiteralPath $probeOut -ErrorAction SilentlyContinue
        if ($ok) {
            Write-BuildLog "Using CMake at $candidate (HTTPS download probe passed)."
            return $candidate
        }
        Write-BuildLog "Rejecting CMake at ${candidate}: HTTPS download probe failed."
    }
    throw 'No CMake that can download over HTTPS was found. Install the Visual Studio "C++ CMake tools for Windows" component or Kitware CMake.'
}

function Resolve-BuildToolchain {
    # Our own toolchain resolution. The upstream build_win.bat asked vswhere for a
    # product id WITHOUT `-products *`, and vswhere hides Build Tools by default,
    # so a machine with only VS Build Tools was reported as having no Visual
    # Studio at all (verified 2026-09-05). We ask for every product explicitly.
    $vsPath = Get-VisualStudio2022Path
    if ([string]::IsNullOrWhiteSpace($vsPath)) {
        throw 'No Visual Studio 2022 C++ toolset was found by vswhere (-products *).'
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $version = [string](@(& $vswhere -path $vsPath -property installationVersion 2>$null)[0])
    if ($version -notmatch '^(\d+)\.') {
        throw "vswhere returned an unusable installation version '$version' for '$vsPath'."
    }
    $major = [int]$Matches[1]
    $generator = switch ($major) {
        16 { 'Visual Studio 16 2019' }
        17 { 'Visual Studio 17 2022' }
        18 { 'Visual Studio 18 2026' }
        default { throw "Unsupported Visual Studio major version $major for a CMake generator." }
    }
    $sdkVersion = Get-WindowsSdkVersion
    if ($null -eq $sdkVersion) { throw 'No complete Windows 10/11 SDK was detected.' }
    $sdkInclude = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Include\$sdkVersion"
    if (-not (Test-Path -LiteralPath (Join-Path $sdkInclude 'winrt\windows.graphics.printing3d.h') -PathType Leaf)) {
        throw "Windows SDK $sdkVersion at '$sdkInclude' lacks winrt\windows.graphics.printing3d.h, which CMakeLists.txt requires through WIN10SDK_PATH."
    }
    return [pscustomobject]@{
        VisualStudioPath = $vsPath
        Generator        = $generator
        SdkVersion       = [string]$sdkVersion
        SdkIncludePath   = $sdkInclude
        CMake            = (Resolve-CMakeExecutable -VisualStudioPath $vsPath)
    }
}

function Get-BuildParallelism {
    # PCH-heavy GUI translation units exhaust memory under an unbounded /m on
    # smaller hosts (HANDOFF.md records C3859/C1076 storms). Default to a bounded
    # count and let the caller raise it through BAMBU_BUILD_JOBS.
    $requested = $env:BAMBU_BUILD_JOBS
    if ($requested -match '^\d+$' -and [int]$requested -ge 1) { return [int]$requested }
    $cores = [Environment]::ProcessorCount
    return [Math]::Max(1, [Math]::Min(8, [int][Math]::Floor($cores / 4)))
}

function Invoke-DependencyBuild {
    param(
        [Parameter(Mandatory)] $Toolchain,
        [Parameter(Mandatory)][string] $Destination,
        [switch] $Clean
    )
    $buildDirectory = Join-Path $script:RepositoryRoot 'deps\build'
    $cache = Join-Path $buildDirectory 'CMakeCache.txt'
    if (-not $Clean -and (Test-Path -LiteralPath $cache -PathType Leaf)) {
        $cachedCMake = (Select-String -LiteralPath $cache -Pattern '^CMAKE_COMMAND:INTERNAL=(.*)$' |
            Select-Object -First 1).Matches.Groups[1].Value
        if ($cachedCMake -and ([System.IO.Path]::GetFullPath($cachedCMake) -ne [System.IO.Path]::GetFullPath($Toolchain.CMake))) {
            Write-BuildLog "Dependency cache was generated by a different CMake ($cachedCMake); rebuilding it cleanly."
            $Clean = $true
        }
    }
    if ($Clean) {
        foreach ($path in @($buildDirectory, $Destination)) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
        }
    }
    New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $jobs = Get-BuildParallelism
    Invoke-RepositoryCommand "Configuring dependencies ($($Toolchain.Generator))..." {
        & $Toolchain.CMake -S (Join-Path $script:RepositoryRoot 'deps') -B $buildDirectory `
            -G $Toolchain.Generator -A x64 `
            "-DDESTDIR=$Destination" -DCMAKE_BUILD_TYPE=Release -DDEP_DEBUG=OFF
    }
    Invoke-RepositoryCommand "Building dependencies (parallel $jobs)..." {
        & $Toolchain.CMake --build $buildDirectory --config Release --parallel $jobs
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Destination 'usr\local') -PathType Container)) {
        throw "The dependency build finished without producing '$Destination\usr\local'."
    }
}

function Invoke-ApplicationBuild {
    param(
        [Parameter(Mandatory)] $Toolchain,
        [Parameter(Mandatory)][string] $DependencyDestination,
        [Parameter(Mandatory)][string] $InstallPrefix,
        [switch] $Clean
    )
    $buildDirectory = Join-Path $script:RepositoryRoot 'build'
    if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
        Remove-Item -LiteralPath $buildDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
    $prefixPath = Join-Path $DependencyDestination 'usr\local'
    $jobs = Get-BuildParallelism
    Invoke-RepositoryCommand "Configuring Bambu Studio ($($Toolchain.Generator))..." {
        & $Toolchain.CMake -S $script:RepositoryRoot -B $buildDirectory `
            -G $Toolchain.Generator -A x64 `
            -DSLIC3R_MSVC_PDB=OFF -DBBL_RELEASE_TO_PUBLIC=1 -DBBL_INTERNAL_TESTING=0 `
            -DSLIC3R_BUILD_TESTS=OFF `
            "-DCMAKE_PREFIX_PATH=$prefixPath" "-DCMAKE_INSTALL_PREFIX=$InstallPrefix" `
            -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_BUILD_TYPE=Release `
            "-DWIN10SDK_PATH=$($Toolchain.SdkIncludePath)"
    }
    Invoke-RepositoryCommand 'Building the DeviceWeb page...' {
        & $Toolchain.CMake --build $buildDirectory --target device_page_build --config Release --parallel $jobs
    }
    Invoke-RepositoryCommand "Building and installing Bambu Studio (parallel $jobs)..." {
        & $Toolchain.CMake --build $buildDirectory --target install --config Release --parallel $jobs
    }
}

function Import-ToolchainHelpers {
    # Dot-sourcing inside a function scopes the helpers to that function and
    # loses them on return (this bit build attempt three on 2026-09-05); load
    # them here and dot-source THIS function from script scope.
    $toolchainScript = Join-Path $script:RepositoryRoot 'packaging\windows\build-from-source\Toolchain.ps1'
    . $toolchainScript
}

function Invoke-OneClickBuild {
    if ([System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) {
        throw 'The one-click installer build is supported only on Windows.'
    }
    if (-not [Environment]::Is64BitOperatingSystem) {
        throw 'A 64-bit Windows installation is required.'
    }

    Test-FreeDiskSpace
    $vsProduct = Initialize-LocalToolchain
    if ($BootstrapOnly -or $Plan) {
        Write-BuildLog $(if ($Plan) { 'Plan completed; no system or build files were changed.' } else { 'Dependency bootstrap completed.' })
        return
    }

    Push-Location $script:RepositoryRoot
    try {
        # This repository tracks no Git LFS objects (git lfs ls-files is empty), and
        # the project policy routes large files through its own transfer path, so the
        # former `git lfs install/pull` steps are gone rather than silently failing.

        $dependencyDestination = Join-Path $script:RepositoryRoot 'deps\build\BambuStudio_dep'
        $dependencyMarker = Join-Path $dependencyDestination 'usr\local'
        $payloadDirectory = Join-Path $script:RepositoryRoot 'install-dir'
        $cleanDependencies = ($BuildMode -eq 'Clean') -or
            -not (Test-Path -LiteralPath $dependencyMarker -PathType Container)
        $cleanApplication = ($BuildMode -eq 'Clean')

        $toolchain = Resolve-BuildToolchain
        Write-BuildLog "Generator: $($toolchain.Generator); SDK include: $($toolchain.SdkIncludePath)"

        Invoke-DependencyBuild -Toolchain $toolchain -Destination $dependencyDestination `
            -Clean:$cleanDependencies
        Normalize-PkgConfigFiles -DependencyDestination $dependencyDestination
        Invoke-ApplicationBuild -Toolchain $toolchain -DependencyDestination $dependencyDestination `
            -InstallPrefix $payloadDirectory -Clean:$cleanApplication

        # The application build installed into install-dir already; re-run the
        # install step so a partially staged payload from an interrupted run is
        # brought back into a complete state instead of being deleted blindly.
        Invoke-RepositoryCommand 'Staging the Release payload...' {
            & $toolchain.CMake --install (Join-Path $script:RepositoryRoot 'build') `
                --config Release --prefix $payloadDirectory
        }
        $application = Join-Path $payloadDirectory 'bambu-studio.exe'
        if (-not (Test-Path -LiteralPath $application -PathType Leaf)) {
            throw "The staged payload is missing '$application'."
        }

        if ($BuildOnly) {
            Write-BuildLog "Build-only workflow completed; runnable payload: $application"
            return
        }

        $sevenZip = Get-SevenZipPath
        Add-MesaFallback -PayloadDirectory $payloadDirectory -SevenZip $sevenZip

        $productVersion = Get-ProductVersion
        $sourceCommit = (& git rev-parse HEAD).Trim()
        Assert-LastExitCode 'Reading the source commit'
        if ($sourceCommit -notmatch '^[0-9a-fA-F]{40}$') {
            throw "Git returned invalid source commit '$sourceCommit'."
        }
        $sourceRepo = (& git remote get-url origin).Trim()
        Assert-LastExitCode 'Reading the origin URL'
        if ($sourceRepo -notmatch '^https://') {
            throw "Origin '$sourceRepo' is not an HTTPS clone URL required by installer source-build mode."
        }
        if (@(& git status --porcelain --untracked-files=no).Count -gt 0) {
            Write-Warning 'Tracked working-tree changes are included in this local payload but cannot be represented by the installer source commit.'
        }

        $outputDirectory = Resolve-OutputDirectory -RequestedPath $OutputDirectory
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
        $squirrelOutputDirectory = Join-Path $outputDirectory 'squirrel'
        $installer = Join-Path $squirrelOutputDirectory 'Setup.exe'
        $sbom = Join-Path $outputDirectory 'BambuStudioMD3.cdx.json'
        $checksum = "$installer.sha256"

        & (Join-Path $script:RepositoryRoot 'scripts\ci\New-WindowsCycloneDxSbom.ps1') `
            -PayloadDir $payloadDirectory -OutputPath $sbom -Version $productVersion `
            -Commit $sourceCommit -Repository ($sourceRepo -replace '^https://github.com/|\.git$', '')
        Write-BuildLog 'Building the unsigned Squirrel.Windows release...'
        & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
            -File (Join-Path $script:RepositoryRoot 'scripts\windows\Invoke-SquirrelPackage.ps1') `
            -PayloadDirectory $payloadDirectory -OutputDirectory $outputDirectory `
            -ProductVersion $productVersion -SourceCommit $sourceCommit `
            -Repository $sourceRepo -IconPath (Join-Path $script:RepositoryRoot 'resources\images\BambuStudio.ico')
        Assert-LastExitCode 'Building the Squirrel.Windows release'
        if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
            throw "Squirrel.Windows did not produce '$installer'."
        }
        $hash = Get-FileSha256Lower -Path $installer
        $checksumText = (Get-Content -LiteralPath $checksum -Raw).Trim()
        if ($checksumText -cne "$hash *Setup.exe") {
            throw 'Squirrel Setup.exe checksum sidecar does not match the generated installer.'
        }

        Write-BuildLog "Installer: $installer"
        Write-BuildLog "SBOM: $sbom"
        Write-BuildLog "SHA-256: $hash"

        if ($Install) {
            Write-Warning 'Launching the locally built, unsigned installer because -Install was explicitly supplied.'
            $process = Start-Process -FilePath $installer -Wait -PassThru
            if ($process.ExitCode -ne 0) {
                throw "The installer exited with code $($process.ExitCode)."
            }
        }
    }
    finally {
        Pop-Location
    }
}

try {
    $script:BuildMutex = New-Object System.Threading.Mutex(
        $false,
        'Local\codingmachineedge.BambuStudioMD3.OneClickBuild'
    )
    try {
        $script:BuildMutexHeld = $script:BuildMutex.WaitOne(0)
    }
    catch [System.Threading.AbandonedMutexException] {
        $script:BuildMutexHeld = $true
    }
    if (-not $script:BuildMutexHeld) {
        throw 'Another Bambu Studio one-click build is already running. Wait for it to finish and retry.'
    }

    New-Item -ItemType Directory -Path $script:LogDirectory -Force | Out-Null
    Start-Transcript -LiteralPath $script:LogPath -Append | Out-Null
    $script:TranscriptStarted = $true
    Write-BuildLog "Bambu Studio one-click build started (mode=$BuildMode, install=$Install, plan=$Plan)."
    . Import-ToolchainHelpers
    Invoke-OneClickBuild
    Write-BuildLog 'One-click workflow completed successfully.'
}
catch {
    Write-Error $_
    exit 1
}
finally {
    if ($script:TranscriptStarted) {
        Stop-Transcript | Out-Null
    }
    if ($script:BuildMutexHeld -and $null -ne $script:BuildMutex) {
        $script:BuildMutex.ReleaseMutex()
    }
    if ($null -ne $script:BuildMutex) {
        $script:BuildMutex.Dispose()
    }
}
