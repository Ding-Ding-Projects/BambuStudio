<#
.SYNOPSIS
    Detect / bootstrap the Windows build toolchain for a from-source Bambu Studio MD3 build.
.DESCRIPTION
    Dot-sourced by Build-FromSource.ps1. For each tool: probe first, install only if
    missing. Prefer winget; fall back to the pinned official vendor installer, always
    silent. The mode choice on the installer page is the user's consent; no per-tool
    prompt is shown. Initialize-Toolchain throws on any tool that cannot be made
    present (the caller maps that to exit code 10).
#>

Set-StrictMode -Version Latest

$script:WingetArgs = @('-e', '--silent', '--accept-package-agreements', '--accept-source-agreements')

function Test-Command {
    param([Parameter(Mandatory = $true)][string] $Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-Winget {
    return (Get-Command winget -ErrorAction SilentlyContinue)
}

function Invoke-SilentInstaller {
    param(
        [Parameter(Mandatory = $true)][string] $Url,
        [Parameter(Mandatory = $true)][string] $FileName,
        [Parameter(Mandatory = $true)][string[]] $Arguments,
        [Parameter(Mandatory = $true)][string] $WorkDir
    )

    $target = Join-Path $WorkDir $FileName
    Write-BuildLog "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $target -UseBasicParsing -ErrorAction Stop

    if ($FileName -match '\.msi$') {
        $msiArgs = @('/i', "`"$target`"") + $Arguments
        $proc = Start-Process -FilePath 'msiexec.exe' -ArgumentList $msiArgs -Wait -PassThru
    } else {
        $proc = Start-Process -FilePath $target -ArgumentList $Arguments -Wait -PassThru
    }
    if ($proc.ExitCode -ne 0) {
        throw "Installer $FileName exited with code $($proc.ExitCode)."
    }
}

function Install-Git {
    param([string] $WorkDir)
    if (Test-Command 'git') { return }

    $winget = Get-Winget
    if ($winget) {
        & winget install --id Git.Git @script:WingetArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Command 'git')) { return }
    }

    # Vendor fallback: Git for Windows silent installer.
    Invoke-SilentInstaller `
        -Url 'https://github.com/git-for-windows/git/releases/latest/download/Git-64-bit.exe' `
        -FileName 'Git-64-bit.exe' `
        -Arguments @('/VERYSILENT', '/NORESTART', '/NOCANCEL', '/SP-') `
        -WorkDir $WorkDir

    Update-SessionPath
    if (-not (Test-Command 'git')) {
        throw 'Git could not be installed.'
    }
}

function Install-Node {
    param([string] $WorkDir)
    if (Test-Command 'node') { return }

    $winget = Get-Winget
    if ($winget) {
        & winget install --id OpenJS.NodeJS.LTS @script:WingetArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Command 'node')) { return }
    }

    Invoke-SilentInstaller `
        -Url 'https://nodejs.org/dist/v20.17.0/node-v20.17.0-x64.msi' `
        -FileName 'node-lts-x64.msi' `
        -Arguments @('/qn', '/norestart') `
        -WorkDir $WorkDir

    Update-SessionPath
    if (-not (Test-Command 'node')) {
        throw 'Node.js LTS could not be installed.'
    }
}

function Test-VisualCppBuildTools {
    # Prefer vswhere to locate a VC++ toolset with the C++ workload.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installPath = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($installPath) { return $true }
    }
    return [bool](Test-Command 'cl')
}

function Install-VisualCppBuildTools {
    param([string] $WorkDir)
    if (Test-VisualCppBuildTools) { return }

    # Pin the Win10 SDK 10.0.22000 to match build_bambu.yml's WIN10SDK_PATH.
    $addSet = @(
        '--add', 'Microsoft.VisualStudio.Workload.VCTools',
        '--add', 'Microsoft.VisualStudio.Component.Windows11SDK.22000',
        '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project'
    )

    $winget = Get-Winget
    if ($winget) {
        $override = ($addSet + @('--quiet', '--wait', '--norestart')) -join ' '
        & winget install --id Microsoft.VisualStudio.2022.BuildTools @script:WingetArgs `
            --override $override
        if ($LASTEXITCODE -eq 0 -and (Test-VisualCppBuildTools)) { return }
    }

    Invoke-SilentInstaller `
        -Url 'https://aka.ms/vs/17/release/vs_BuildTools.exe' `
        -FileName 'vs_BuildTools.exe' `
        -Arguments ($addSet + @('--quiet', '--wait', '--norestart')) `
        -WorkDir $WorkDir

    if (-not (Test-VisualCppBuildTools)) {
        throw 'Visual Studio 2022 Build Tools (C++ workload) could not be installed.'
    }
}

function Install-CMake {
    param([string] $WorkDir)
    # CMake may already have arrived via the VS "VC.CMake.Project" component.
    if (Test-Command 'cmake') { return }

    $winget = Get-Winget
    if ($winget) {
        & winget install --id Kitware.CMake @script:WingetArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Command 'cmake')) { return }
    }

    Invoke-SilentInstaller `
        -Url 'https://github.com/Kitware/CMake/releases/latest/download/cmake-windows-x86_64.msi' `
        -FileName 'cmake-x64.msi' `
        -Arguments @('/qn', '/norestart', 'ADD_CMAKE_TO_PATH=System') `
        -WorkDir $WorkDir

    Update-SessionPath
    if (-not (Test-Command 'cmake')) {
        throw 'CMake could not be installed.'
    }
}

function Update-SessionPath {
    # Refresh PATH from the machine + user registry so a freshly installed tool is
    # visible to this same session without a relaunch.
    $machine = [System.Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [System.Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = @($machine, $user) -join ';'
}

function Initialize-Toolchain {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string] $WorkDir)

    if (-not (Test-Path -LiteralPath $WorkDir)) {
        New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
    }

    Install-Git -WorkDir $WorkDir
    Install-Node -WorkDir $WorkDir
    Install-VisualCppBuildTools -WorkDir $WorkDir
    Install-CMake -WorkDir $WorkDir
    Update-SessionPath
}
