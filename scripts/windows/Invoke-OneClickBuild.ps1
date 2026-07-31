<#
.SYNOPSIS
    Bootstrap, compile, and package Bambu Studio for Windows with one command.
.DESCRIPTION
    Installs missing ordinary build prerequisites through winget, builds the
    repository dependencies and Release application, stages the payload, adds
    the hash-pinned Mesa software OpenGL fallback, creates a CycloneDX SBOM,
    compiles the NSIS installer, validates it, and writes a SHA-256 sidecar.

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

    [switch] $Plan
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

function Get-MakeNsisPath {
    $command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($candidate in @(
        (Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe'),
        (Join-Path $env:ProgramFiles 'NSIS\makensis.exe'),
        (Join-Path $env:LOCALAPPDATA 'codingmachineedge\BambuStudioMD3-BuildTools\nsis-3.12\makensis.exe')
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    $uninstallRoots = @(
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    foreach ($entry in @(Get-ItemProperty $uninstallRoots -ErrorAction SilentlyContinue |
        Where-Object {
            $_.PSObject.Properties['DisplayName'] -and
            $_.DisplayName -eq 'Nullsoft Install System'
        })) {
        if ($entry.PSObject.Properties['InstallLocation'] -and
            -not [string]::IsNullOrWhiteSpace($entry.InstallLocation)) {
            $candidate = Join-Path $entry.InstallLocation.Trim('"') 'makensis.exe'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    return $null
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
        [switch] $PortableNsisFallback,
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
            if (-not $PortableNsisFallback) {
                throw "Removing the stale $DisplayName registration failed with exit code $uninstallExitCode."
            }
            Install-PortableNsis
            if (& $Probe) { return }
            throw "$DisplayName portable installation completed but the tool is still unavailable."
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

function Install-PortableNsis {
    $sevenZip = Get-SevenZipPath
    if ([string]::IsNullOrWhiteSpace($sevenZip)) {
        throw '7-Zip is required for the non-admin NSIS portable fallback.'
    }

    $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
        ('BambuStudio-NSIS-' + [guid]::NewGuid().ToString('N'))
    $destination = Join-Path $env:LOCALAPPDATA `
        'codingmachineedge\BambuStudioMD3-BuildTools\nsis-3.12'
    try {
        New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
        Write-BuildLog 'Downloading the official NSIS 3.12 package through winget for verified portable extraction...'
        & winget.exe download --id NSIS.NSIS --exact --version 3.12 `
            --download-directory $temporaryRoot --accept-package-agreements `
            --accept-source-agreements
        Assert-LastExitCode 'Downloading the verified NSIS 3.12 package'
        $installers = @(Get-ChildItem -LiteralPath $temporaryRoot -File -Filter '*.exe')
        if ($installers.Count -ne 1) {
            throw "Expected one downloaded NSIS installer, found $($installers.Count)."
        }

        $extracted = Join-Path $temporaryRoot 'extracted'
        & $sevenZip x $installers[0].FullName "-o$extracted" -y | Out-Host
        Assert-LastExitCode 'Extracting the verified NSIS package'
        if (-not (Test-Path -LiteralPath (Join-Path $extracted 'makensis.exe') -PathType Leaf)) {
            throw 'The verified NSIS package did not contain makensis.exe.'
        }

        if (Test-Path -LiteralPath $destination) {
            $expectedPrefix = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA `
                'codingmachineedge\BambuStudioMD3-BuildTools')).TrimEnd('\') + '\'
            $resolvedDestination = [System.IO.Path]::GetFullPath($destination)
            if (-not $resolvedDestination.StartsWith($expectedPrefix,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to replace unexpected NSIS directory '$resolvedDestination'."
            }
            Remove-Item -LiteralPath $resolvedDestination -Recurse -Force
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Move-Item -LiteralPath $extracted -Destination $destination
        Write-BuildLog "Portable NSIS 3.12 is ready at '$destination'."
    }
    finally {
        if (Test-Path -LiteralPath $temporaryRoot) {
            $resolved = [System.IO.Path]::GetFullPath($temporaryRoot)
            $tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
            if (-not $resolved.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                [System.IO.Path]::GetFileName($resolved) -notlike 'BambuStudio-NSIS-*') {
                throw "Refusing to remove unexpected NSIS temporary directory '$resolved'."
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
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
    $toolchainScript = Join-Path $script:RepositoryRoot 'packaging\windows\build-from-source\Toolchain.ps1'
    . $toolchainScript

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
        Update-SessionPath
    }

    Install-WingetPackageIfMissing -DisplayName '7-Zip' -PackageId '7zip.7zip' `
        -Probe { $null -ne (Get-SevenZipPath) }
    Install-WingetPackageIfMissing -DisplayName 'NSIS 3' -PackageId 'NSIS.NSIS' `
        -PortableNsisFallback -Probe { $null -ne (Get-MakeNsisPath) }

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
        Invoke-WebRequest -Uri $url -OutFile $archive -MaximumRetryCount 3 -RetryIntervalSec 15
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

function Get-FileVersion {
    param([Parameter(Mandatory)][string] $ProductVersion)
    $parts = @([regex]::Matches($ProductVersion, '\d+') | ForEach-Object { [int]$_.Value })
    if ($parts.Count -gt 4) { $parts = @($parts[0..3]) }
    while ($parts.Count -lt 4) { $parts += 0 }
    if ($parts | Where-Object { $_ -gt 65535 }) {
        throw "Version '$ProductVersion' contains a component larger than 65535."
    }
    return $parts -join '.'
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
        Invoke-RepositoryCommand 'Initializing Git LFS for this checkout...' {
            & git lfs install --local
        }
        Invoke-RepositoryCommand 'Downloading required Git LFS objects...' {
            & git lfs pull
        }

        $dependencyDestination = Join-Path $script:RepositoryRoot 'deps\build\BambuStudio_dep'
        $dependencyMarker = Join-Path $dependencyDestination 'usr\local'
        $dependencyStep = if ($BuildMode -eq 'Clean' -or
            -not (Test-Path -LiteralPath $dependencyMarker -PathType Container)) { 'deps' } else { 'deps-dirty' }
        $appStep = if ($BuildMode -eq 'Clean' -or
            -not (Test-Path -LiteralPath (Join-Path $script:RepositoryRoot 'build\CMakeCache.txt'))) { 'app' } else { 'app-dirty' }

        Invoke-RepositoryCommand "Building dependencies ($dependencyStep)..." {
            & (Join-Path $script:RepositoryRoot 'build_win.bat') -v 17 -p $vsProduct `
                -c Release -d $dependencyDestination -s $dependencyStep -r none
        }
        Invoke-RepositoryCommand "Building Bambu Studio ($appStep)..." {
            & (Join-Path $script:RepositoryRoot 'build_win.bat') -v 17 -p $vsProduct `
                -c Release -d $dependencyDestination -s $appStep -r none
        }

        $payloadDirectory = Join-Path $script:RepositoryRoot 'install-dir'
        if (Test-Path -LiteralPath $payloadDirectory) {
            $expectedPayload = [System.IO.Path]::GetFullPath((Join-Path $script:RepositoryRoot 'install-dir'))
            $resolvedPayload = [System.IO.Path]::GetFullPath($payloadDirectory)
            if ($resolvedPayload -ne $expectedPayload) {
                throw "Refusing to clean unexpected payload path '$resolvedPayload'."
            }
            Remove-Item -LiteralPath $resolvedPayload -Recurse -Force
        }
        Invoke-RepositoryCommand 'Staging the Release payload...' {
            & cmake.exe --install (Join-Path $script:RepositoryRoot 'build') `
                --config Release --prefix $payloadDirectory
        }
        $application = Join-Path $payloadDirectory 'bambu-studio.exe'
        if (-not (Test-Path -LiteralPath $application -PathType Leaf)) {
            throw "The staged payload is missing '$application'."
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
        $installer = Join-Path $outputDirectory 'BambuStudioMD3-Setup.exe'
        $sbom = Join-Path $outputDirectory 'BambuStudioMD3.cdx.json'
        $checksum = "$installer.sha256"
        $uninstallInclude = Join-Path $outputDirectory 'BambuStudioMD3-UninstallFiles.nsh'

        & (Join-Path $script:RepositoryRoot 'scripts\ci\New-WindowsCycloneDxSbom.ps1') `
            -PayloadDir $payloadDirectory -OutputPath $sbom -Version $productVersion `
            -Commit $sourceCommit -Repository ($sourceRepo -replace '^https://github.com/|\.git$', '')

        & (Join-Path $script:RepositoryRoot 'packaging\windows\GenerateUninstallInclude.ps1') `
            -PayloadDir $payloadDirectory -OutputPath $uninstallInclude

        $makeNsis = Get-MakeNsisPath
        $fileVersion = Get-FileVersion -ProductVersion $productVersion
        Write-BuildLog "Compiling NSIS installer $installer..."
        & $makeNsis /V3 /INPUTCHARSET UTF8 `
            "/DPRODUCT_VERSION=$productVersion" `
            "/DFILE_VERSION=$fileVersion" `
            "/DPRODUCT_SOURCE_REPO_URL=$sourceRepo" `
            "/DPRODUCT_SOURCE_TAG=$sourceCommit" `
            "/DPAYLOAD_DIR=$payloadDirectory" `
            "/DOUT_FILE=$installer" `
            "/DINSTALLER_ICON=$(Join-Path $script:RepositoryRoot 'resources\images\BambuStudio.ico')" `
            "/DUNINSTALL_INCLUDE=$uninstallInclude" `
            (Join-Path $script:RepositoryRoot 'packaging\windows\BambuStudioMD3.nsi')
        Assert-LastExitCode 'Compiling the NSIS installer'

        & $sevenZip t $installer | Out-Host
        Assert-LastExitCode 'Validating the installer archive'
        $hash = Get-FileSha256Lower -Path $installer
        "$hash *BambuStudioMD3-Setup.exe" | Set-Content -LiteralPath $checksum -Encoding Ascii
        Remove-Item -LiteralPath $uninstallInclude -Force

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
