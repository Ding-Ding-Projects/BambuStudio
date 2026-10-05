<#
.SYNOPSIS
    Detect / bootstrap the Windows build toolchain for a from-source Bambu Studio MD3 build.
.DESCRIPTION
    Dot-sourced by Build-FromSource.ps1. For each tool: probe first, install only if
    missing. Prefer winget except for Visual Studio, whose vendor bootstrapper uses
    an explicit project-owned instance path. Vendor installers run silently. The
    mode choice on the installer page is the user's consent; no per-tool
    prompt is shown. Initialize-Toolchain throws on any tool that cannot be made
    present (the caller maps that to exit code 10).
#>

Set-StrictMode -Version Latest

$script:WingetArgs = @('-e', '--silent', '--accept-package-agreements', '--accept-source-agreements')
$script:MinimumCMakeVersion = [version]'3.21.0'
$script:MaximumCMakeVersionExclusive = [version]'5.0.0'
$script:MinimumWindowsSdkVersion = [version]'10.0.22000.0'
$script:MinimumNodeLtsMajor = 22
$script:RequiredNodeArchitecture = 'x64'
$script:NodeFallbackVersion = '22.22.2'
$script:CMakeFallbackVersion = '4.4.0'
$script:GitFallbackVersion = '2.54.0'
$script:GitFallbackTag = 'v2.54.0.windows.1'
# Fixed-version vendor digests are copied from the publishers' release
# metadata: GitHub's asset digest for Git for Windows, Node's SHASUMS256.txt,
# and Kitware's cmake-4.4.0-SHA-256.txt. The aka.ms Visual Studio bootstrapper
# is mutable, so it is protected by its exact Authenticode publisher only.
$script:GitFallbackSha256 = '2B96E7854F0520F0F6B709C21041D9801B1BE44D5E1A0D9FA621B2FBC40F1983'
$script:NodeFallbackSha256 = '57456AA33FCD6FB6A9418E09227DE0B0CA604F7B2123566ACC66B555CB2F42E5'
$script:NodePortableSha256 = '7C93E9D92BF68C07182B471AA187E35EE6CD08EF0F24AB060DFFF605FCC1C57C'
$script:CMakeFallbackSha256 = '82DB53FCB8F38BE541A26093489F39D5ED79B71B53CD121FC32A022A6BF310B1'
$script:GitTrustedPublishers = @('Johannes Schindelin')
$script:NodeTrustedPublishers = @('OpenJS Foundation')
$script:CMakeTrustedPublishers = @('Kitware, Inc.')
$script:VisualStudioTrustedPublishers = @('Microsoft Corporation')

function Test-Command {
    param([Parameter(Mandatory = $true)][string] $Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-NodeLts {
    param(
        [string] $NodePath,
        [string] $NpmPath
    )

    if ([string]::IsNullOrWhiteSpace($NodePath)) {
        $node = Get-Command node -ErrorAction SilentlyContinue
        if (-not $node) { return $false }
        $NodePath = $node.Source
    }
    if ([string]::IsNullOrWhiteSpace($NpmPath)) {
        $npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
        if (-not $npm) { return $false }
        $NpmPath = $npm.Source
    }

    try {
        $output = @(& $NodePath -p "process.release && process.release.lts ? [process.release.lts, process.versions.node, process.arch].join('|') : ''" 2>$null)
        $exitCode = $LASTEXITCODE
        $probe = [string]($output | Select-Object -First 1)
        $parts = @($probe -split '\|')
        $version = $null
        if ($exitCode -ne 0 -or $parts.Count -ne 3 -or
            [string]::IsNullOrWhiteSpace($parts[0]) -or
            -not [version]::TryParse($parts[1], [ref]$version)) {
            return $false
        }
        return ($version.Major -ge $script:MinimumNodeLtsMajor -and
            $parts[2] -eq $script:RequiredNodeArchitecture -and
            (Test-Path -LiteralPath $NpmPath -PathType Leaf))
    } catch {
        return $false
    }
}

function Test-CMakeVersion {
    param([string] $CMakePath, [version] $MinimumVersion = $script:MinimumCMakeVersion)

    if ([string]::IsNullOrWhiteSpace($CMakePath)) {
        $cmake = Get-Command cmake -ErrorAction SilentlyContinue
        if (-not $cmake) { return $false }
        $CMakePath = $cmake.Source
    }

    try {
        $output = @(& $CMakePath --version 2>$null)
        $exitCode = $LASTEXITCODE
        $firstLine = $output | Select-Object -First 1
        if ($exitCode -ne 0) {
            return $false
        }
        $match = [regex]::Match([string]$firstLine, '^cmake version\s+(\d+\.\d+(?:\.\d+)?)')
        if (-not $match.Success) { return $false }
        $version = [version]$match.Groups[1].Value
        return ($version -ge $MinimumVersion -and
            $version -lt $script:MaximumCMakeVersionExclusive)
    } catch {
        return $false
    }
}

function Get-WindowsSdkVersion {
    param([string[]] $Roots)

    if ($null -eq $Roots -or $Roots.Count -eq 0) {
        $Roots = @()
        try {
            $installedRoots = Get-ItemProperty `
                -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' `
                -ErrorAction Stop
            if (-not [string]::IsNullOrWhiteSpace($installedRoots.KitsRoot10)) {
                $Roots += $installedRoots.KitsRoot10
            }
        } catch {
            # The conventional path below remains a valid fallback.
        }
        if (-not [string]::IsNullOrWhiteSpace(${env:ProgramFiles(x86)})) {
            $Roots += (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10')
        }
    }

    $versions = @()
    foreach ($root in @($Roots | Select-Object -Unique)) {
        $includeRoot = Join-Path $root 'Include'
        foreach ($directory in @(Get-ChildItem -LiteralPath $includeRoot -Directory -ErrorAction SilentlyContinue)) {
            $version = $null
            if (-not [version]::TryParse($directory.Name, [ref]$version) -or
                $version -lt $script:MinimumWindowsSdkVersion) {
                continue
            }

            $requiredFiles = @(
                (Join-Path $directory.FullName 'um\Windows.h'),
                (Join-Path $directory.FullName 'shared\sdkddkver.h'),
                (Join-Path $directory.FullName 'ucrt\stdio.h'),
                (Join-Path $root "Lib\$($directory.Name)\um\x64\kernel32.lib"),
                (Join-Path $root "Lib\$($directory.Name)\ucrt\x64\ucrt.lib")
            )
            if (@($requiredFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -eq 0) {
                $versions += $version
            }
        }
    }

    $latest = $versions | Sort-Object -Descending | Select-Object -First 1
    return $latest
}

function Test-VisualStudioMSBuild {
    param([Parameter(Mandatory)][string] $Path, [int] $TimeoutMilliseconds = 15000)
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $Path
    $process.StartInfo.Arguments = '-nologo -version'
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        $null = $process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $process.Kill()
            return [pscustomobject]@{ Succeeded = $false; Reason = 'MSBuild startup/version probe timed out after 15 seconds.' }
        }
        if (-not $stdout.Wait(1000) -or -not $stderr.Wait(1000)) {
            return [pscustomobject]@{ Succeeded = $false; Reason = 'MSBuild probe output did not complete.' }
        }
        if ($process.ExitCode -ne 0) {
            $detail = (($stderr.Result -split '\r?\n' | Select-Object -First 1) -join '').Trim()
            return [pscustomobject]@{ Succeeded = $false; Reason = "MSBuild startup/version probe exited $($process.ExitCode): $detail" }
        }
        if ($stdout.Result -notmatch '(?m)^\d+\.\d+\.\d+(?:\.\d+)?\s*$') {
            return [pscustomobject]@{ Succeeded = $false; Reason = 'MSBuild startup/version probe returned no numeric version.' }
        }
        return [pscustomobject]@{ Succeeded = $true; Reason = '' }
    } catch {
        return [pscustomobject]@{ Succeeded = $false; Reason = "MSBuild startup/version probe could not run: $($_.Exception.Message)" }
    } finally {
        $process.Dispose()
    }
}

function Get-VisualStudioInstance {
    [CmdletBinding()]
    param([string] $VsWherePath, [string] $VersionRange = '[17.0,19.0)')

    $vswhere = $VsWherePath
    if ([string]::IsNullOrWhiteSpace($vswhere)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { return $null }

    # Enumerate every candidate: a stale newest registration must not hide a
    # usable older instance. Keep the product and path from the same record.
    # Registration completeness is not file presence. An interrupted IDE setup
    # may still contain a usable compiler, including an installed preview toolset.
    $output = & $vswhere -all -prerelease -products * -sort `
        -version $VersionRange `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -format json -utf8 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $output) { return $null }
    try { $instances = @((($output -join "`n") | ConvertFrom-Json)) }
    catch { return $null }
    foreach ($instance in $instances) {
        if (-not $instance.PSObject.Properties['installationPath'] -or
            -not $instance.PSObject.Properties['productId'] -or
            -not $instance.PSObject.Properties['installationVersion']) { continue }
        $version = $null
        if ([string]$instance.installationVersion -notmatch '^\d+\.\d+\.\d+\.\d+$' -or
            -not [version]::TryParse([string]$instance.installationVersion, [ref]$version) -or
            $version.Major -notin @(17, 18)) { continue }
        $installPath = [string]$instance.installationPath
        if ([string]::IsNullOrWhiteSpace($installPath) -or
            [string]$instance.productId -notmatch '^Microsoft\.VisualStudio\.Product\.(BuildTools|Community|Professional|Enterprise)$') { continue }
        $requiredFiles = @(
            (Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'),
            (Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'),
            (Join-Path $installPath 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')
        )
        if (@($requiredFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count) { continue }
        $toolVersion = (Get-Content -LiteralPath $requiredFiles[2] -Raw).Trim()
        if ($toolVersion -notmatch '^\d+\.\d+\.\d+$') { continue }
        if (-not (Test-Path -LiteralPath (Join-Path $installPath "VC\Tools\MSVC\$toolVersion\bin\Hostx64\x64\cl.exe") -PathType Leaf)) { continue }
        # CMake's x64 Visual Studio generator invokes this executable. A working
        # cl.exe alone cannot compensate for a broken managed MSBuild runtime.
        $msbuildPath = Join-Path $installPath 'MSBuild\Current\Bin\amd64\MSBuild.exe'
        if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) { continue }
        $probe = Test-VisualStudioMSBuild -Path $msbuildPath
        if (-not $probe.Succeeded) {
            Write-Warning "Skipping Visual Studio instance '$installPath' ($($instance.installationVersion)): $($probe.Reason)"
            continue
        }
        return $instance
    }
    return $null
}

function Get-VisualStudioPath {
    param([string] $VsWherePath, [string] $VersionRange = '[17.0,19.0)')
    $instance = Get-VisualStudioInstance -VsWherePath $VsWherePath -VersionRange $VersionRange
    if ($null -eq $instance) { return $null }
    return [string]$instance.installationPath
}

function Get-VisualStudioProduct {
    param([string] $VsWherePath, [string] $VersionRange = '[17.0,19.0)')
    $instance = Get-VisualStudioInstance -VsWherePath $VsWherePath -VersionRange $VersionRange
    if ($null -eq $instance) { return $null }
    return ([string]$instance.productId).Substring('Microsoft.VisualStudio.Product.'.Length)
}

# Preserve version-specific callers without silently changing their contract.
function Get-VisualStudio2022Path {
    param([string] $VsWherePath)
    return Get-VisualStudioPath -VsWherePath $VsWherePath -VersionRange '[17.0,18.0)'
}

function Get-VisualStudio2022Product {
    param([string] $VsWherePath)
    return Get-VisualStudioProduct -VsWherePath $VsWherePath -VersionRange '[17.0,18.0)'
}

function Get-SafeRelativePath {
    param(
        [Parameter(Mandatory = $true)][string] $Root,
        [Parameter(Mandatory = $true)][string] $Path
    )

    $separator = [System.IO.Path]::DirectorySeparatorChar
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    $rootPrefix = $rootFull + $separator
    if (-not $pathFull.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path '$pathFull' is outside root '$rootFull'."
    }

    $relative = $pathFull.Substring($rootPrefix.Length)
    if ([string]::IsNullOrWhiteSpace($relative)) {
        throw "Path '$pathFull' does not identify an item below root '$rootFull'."
    }
    return $relative
}

function Get-Winget {
    return (Get-Command winget -ErrorAction SilentlyContinue)
}

function Test-InstallerSignatureMetadata {
    param(
        [string] $Status,
        [string] $Publisher,
        [string[]] $TrustedPublishers
    )

    if (-not [string]::Equals($Status, 'Valid', [System.StringComparison]::Ordinal) -or
        [string]::IsNullOrWhiteSpace($Publisher)) {
        return $false
    }
    foreach ($trustedPublisher in @($TrustedPublishers)) {
        if ([string]::Equals($Publisher, $trustedPublisher, [System.StringComparison]::Ordinal)) {
            return $true
        }
    }
    return $false
}

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string] $Path)

    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Assert-TrustedInstaller {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string[]] $TrustedPublishers,
        [string] $ExpectedSha256 = ''
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Downloaded installer '$Path' is missing."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        if ($ExpectedSha256 -notmatch '^[0-9A-Fa-f]{64}$') {
            throw "The expected SHA-256 policy for '$Path' is malformed."
        }
        $actualSha256 = Get-FileSha256 -Path $Path
        if (-not [string]::Equals($actualSha256, $ExpectedSha256,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Downloaded installer '$Path' failed its pinned SHA-256 check."
        }
    }

    $securityModule = Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1'
    if (-not (Get-Command Get-AuthenticodeSignature -ErrorAction SilentlyContinue)) {
        Import-Module -Name $securityModule -ErrorAction Stop
    }
    $signature = Microsoft.PowerShell.Security\Get-AuthenticodeSignature `
        -LiteralPath $Path -ErrorAction Stop
    $status = [string]$signature.Status
    $publisher = ''
    if ($null -ne $signature.SignerCertificate) {
        $publisher = $signature.SignerCertificate.GetNameInfo(
            [System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName,
            $false)
    }
    if (-not (Test-InstallerSignatureMetadata -Status $status -Publisher $publisher `
            -TrustedPublishers $TrustedPublishers)) {
        if (-not [string]::Equals($status, 'Valid', [System.StringComparison]::Ordinal)) {
            throw "Downloaded installer '$Path' has Authenticode status '$status'; refusing to execute it."
        }
        if ([string]::IsNullOrWhiteSpace($publisher)) {
            throw "Downloaded installer '$Path' has no Authenticode publisher; refusing to execute it."
        }
        throw "Downloaded installer '$Path' is signed by untrusted publisher '$publisher'; refusing to execute it."
    }
    Write-BuildLog "Verified installer trust for '$Path' (publisher '$publisher')."
}

function Invoke-SilentInstaller {
    param(
        [Parameter(Mandatory = $true)][string] $Url,
        [Parameter(Mandatory = $true)][string] $FileName,
        [Parameter(Mandatory = $true)][string[]] $Arguments,
        [Parameter(Mandatory = $true)][string] $WorkDir,
        [Parameter(Mandatory = $true)][string[]] $TrustedPublishers,
        [string] $ExpectedSha256 = ''
    )

    $target = Join-Path $WorkDir $FileName
    Write-BuildLog "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $target -UseBasicParsing -ErrorAction Stop
    Assert-TrustedInstaller -Path $target -TrustedPublishers $TrustedPublishers `
        -ExpectedSha256 $ExpectedSha256

    if ($FileName -match '\.msi$') {
        $msiArgs = @('/i', "`"$target`"") + $Arguments
        $proc = Start-Process -FilePath 'msiexec.exe' -ArgumentList $msiArgs `
            -Wait -PassThru -WindowStyle Hidden
    } else {
        $proc = Start-Process -FilePath $target -ArgumentList $Arguments `
            -Wait -PassThru -WindowStyle Hidden
    }
    if ($proc.ExitCode -notin @(0, 1641, 3010)) {
        throw "Installer $FileName exited with code $($proc.ExitCode)."
    }
}

function Install-Git {
    param([string] $WorkDir)
    if (Test-Command 'git') { return }

    $winget = Get-Winget
    if ($winget) {
        & winget install --id Git.Git @script:WingetArgs
        Update-SessionPath
        if ($LASTEXITCODE -eq 0 -and (Test-Command 'git')) { return }
    }

    # Vendor fallback: Git for Windows silent installer.
    Invoke-SilentInstaller `
        -Url "https://github.com/git-for-windows/git/releases/download/$($script:GitFallbackTag)/Git-$($script:GitFallbackVersion)-64-bit.exe" `
        -FileName 'Git-64-bit.exe' `
        -Arguments @('/VERYSILENT', '/NORESTART', '/NOCANCEL', '/SP-') `
        -WorkDir $WorkDir `
        -TrustedPublishers $script:GitTrustedPublishers `
        -ExpectedSha256 $script:GitFallbackSha256

    Update-SessionPath
    if (-not (Test-Command 'git')) {
        throw 'Git could not be installed.'
    }
}

function Install-Node {
    param([string] $WorkDir)
    if (Test-NodeLts) { return }

    # A project-local portable LTS installation can coexist with a newer global
    # Node version and never contends with Windows Installer or downgrades it.
    $portableRoot = Join-Path $WorkDir "node-v$($script:NodeFallbackVersion)-win-x64"
    if (Test-NodeLts -NodePath (Join-Path $portableRoot 'node.exe') -NpmPath (Join-Path $portableRoot 'npm.cmd')) {
        $env:PATH = "$portableRoot;$env:PATH"
        return
    }
    $archive = Join-Path $WorkDir "node-v$($script:NodeFallbackVersion)-win-x64.zip"
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or
        (Get-FileSha256 -Path $archive) -cne $script:NodePortableSha256) {
        Invoke-WebRequest -Uri "https://nodejs.org/dist/v$($script:NodeFallbackVersion)/node-v$($script:NodeFallbackVersion)-win-x64.zip" `
            -OutFile $archive -UseBasicParsing -TimeoutSec 120 -ErrorAction Stop
    }
    if ((Get-FileSha256 -Path $archive) -cne $script:NodePortableSha256) {
        throw 'Portable Node.js archive checksum does not match the pinned publisher digest.'
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $staging = Join-Path $WorkDir ('node-staging-' + [guid]::NewGuid().ToString('N'))
    [IO.Compression.ZipFile]::ExtractToDirectory($archive, $staging)
    if (Test-Path -LiteralPath $portableRoot) {
        Move-Item -LiteralPath $portableRoot -Destination ($portableRoot + '.previous-' + [guid]::NewGuid().ToString('N'))
    }
    Move-Item -LiteralPath (Join-Path $staging "node-v$($script:NodeFallbackVersion)-win-x64") -Destination $portableRoot
    $env:PATH = "$portableRoot;$env:PATH"
    if (-not (Test-NodeLts)) {
        throw 'Node.js LTS with npm could not be installed.'
    }
}

function Test-VisualCppBuildTools {
    param(
        [string] $VsWherePath,
        [string[]] $WindowsSdkRoots
    )
    return (-not [string]::IsNullOrWhiteSpace((Get-VisualStudioPath -VsWherePath $VsWherePath)) -and
        $null -ne (Get-WindowsSdkVersion -Roots $WindowsSdkRoots))
}

function Install-VisualCppBuildTools {
    param([string] $WorkDir)
    if (Test-VisualCppBuildTools) { return }

    # Request the current supported Windows 11 SDK. Detection accepts any complete
    # Desktop SDK at or above MinimumWindowsSdkVersion.
    $addSet = @(
        '--add', 'Microsoft.VisualStudio.Workload.VCTools',
        '--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '--add', 'Microsoft.VisualStudio.Component.Windows11SDK.26100',
        '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project'
    )

    # An implicit bootstrapper target (including winget's) can select a stale
    # registration owned by another application. Explicitly select our own
    # stable installation path; never uninstall or repair that other instance.
    # Major 18 on Stable is distinct from a stale major 17 Release registration.
    $installPath = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3\toolchain\BuildTools2026'
    Write-BuildLog "Installing Visual Studio 2026 Build Tools at '$installPath'. Elevation may be required."

    Invoke-SilentInstaller `
        -Url 'https://aka.ms/vs/stable/vs_buildtools.exe' `
        -FileName 'vs_BuildTools.exe' `
        -Arguments ($addSet + @('--installPath', "`"$installPath`"", '--nickname', 'BambuMD3', '--quiet', '--wait', '--norestart')) `
        -WorkDir $WorkDir `
        -TrustedPublishers $script:VisualStudioTrustedPublishers

    if (-not (Test-VisualCppBuildTools)) {
        throw 'Visual Studio 2022 or 2026 Build Tools (C++ workload) could not be installed.'
    }
}

function Install-CMake {
    param([string] $WorkDir)
    # CMake may already have arrived via the VS "VC.CMake.Project" component.
    $minimumVersion = $script:MinimumCMakeVersion
    $instance = Get-VisualStudioInstance
    if ($null -ne $instance -and ([version]$instance.installationVersion).Major -eq 18) {
        $minimumVersion = [version]'4.2.0'
    }
    if (Test-CMakeVersion -MinimumVersion $minimumVersion) { return }
    if ($null -ne $instance) {
        $bundledCMake = Join-Path ([string]$instance.installationPath) 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if ([IO.File]::Exists($bundledCMake) -and
            (Test-CMakeVersion -CMakePath $bundledCMake -MinimumVersion $minimumVersion)) {
            # Discover this inside the elevated producer, rather than depending
            # on a process-only PATH edit made before the RunAs handoff.
            $bundledDirectory = [IO.Path]::GetDirectoryName($bundledCMake)
            $env:Path = $bundledDirectory + ';' + $env:Path
            Update-SessionPath
            Write-BuildLog "Using supported Visual Studio bundled CMake at '$bundledCMake'."
            return
        }
    }

    $winget = Get-Winget
    if ($winget) {
        if (Test-Command 'cmake') {
            & winget upgrade --id Kitware.CMake @script:WingetArgs
        } else {
            & winget install --id Kitware.CMake @script:WingetArgs
        }
        Update-SessionPath
        if ($LASTEXITCODE -eq 0 -and (Test-CMakeVersion -MinimumVersion $minimumVersion)) { return }
    }

    Invoke-SilentInstaller `
        -Url "https://github.com/Kitware/CMake/releases/download/v$($script:CMakeFallbackVersion)/cmake-$($script:CMakeFallbackVersion)-windows-x86_64.msi" `
        -FileName 'cmake-x64.msi' `
        -Arguments @('/qn', '/norestart', 'ADD_CMAKE_TO_PATH=System') `
        -WorkDir $WorkDir `
        -TrustedPublishers $script:CMakeTrustedPublishers `
        -ExpectedSha256 $script:CMakeFallbackSha256

    Update-SessionPath
    if (-not (Test-CMakeVersion -MinimumVersion $minimumVersion)) {
        throw "CMake $minimumVersion or newer, below $($script:MaximumCMakeVersionExclusive), could not be installed."
    }
}

function Update-SessionPath {
    # Add registry PATH entries so freshly installed tools are visible without a
    # relaunch, while preserving process-only/portable entries from the caller.
    $machine = [System.Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [System.Environment]::GetEnvironmentVariable('Path', 'User')
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' `
        ([System.StringComparer]::OrdinalIgnoreCase)
    $merged = New-Object 'System.Collections.Generic.List[string]'
    # Preserve the caller's process-local ordering. Portable toolchains are a
    # deliberate override and must not be shadowed by stale registry entries.
    # Machine/user PATH values are appended so tools installed during this run
    # still become discoverable without relaunching the process.
    foreach ($source in @($env:Path, $machine, $user)) {
        foreach ($entry in @([string]$source -split ';')) {
            $trimmed = $entry.Trim()
            if (-not [string]::IsNullOrWhiteSpace($trimmed) -and $seen.Add($trimmed.TrimEnd('\'))) {
                $merged.Add($trimmed)
            }
        }
    }
    $env:Path = $merged -join ';'
}

function Initialize-Toolchain {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string] $WorkDir)

    if (-not (Test-Path -LiteralPath $WorkDir)) {
        New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
    }

    Update-SessionPath
    Install-Git -WorkDir $WorkDir
    Install-Node -WorkDir $WorkDir
    Install-VisualCppBuildTools -WorkDir $WorkDir
    Install-CMake -WorkDir $WorkDir
    Update-SessionPath
}
