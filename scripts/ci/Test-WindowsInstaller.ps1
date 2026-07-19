[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Installer,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $PreviousInstaller,

    [Parameter(Mandatory)]
    [string] $PreviousOwnedFile,

    [Parameter(Mandatory)]
    [switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $CiExecutionApproved -or $env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This script executes unsigned installers and is restricted to an explicitly approved disposable GitHub-hosted Actions runner.'
}

$installerId = 'codingmachineedge.BambuStudioMD3.owned-v1'
$installDir = Join-Path $env:LOCALAPPDATA 'Programs\Bambu Studio MD3'
$productRegistry = 'HKCU:\Software\codingmachineedge\BambuStudioMD3'
$preferenceRegistry = 'HKCU:\Software\codingmachineedge\BambuStudioMD3Preferences'
$uninstallRegistry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\BambuStudioMD3'
$shortcutDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Bambu Studio MD3'
$runnerTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$testRoot = Join-Path $runnerTemp ('BambuStudioMD3-InstallerTest-' + [guid]::NewGuid().ToString('N'))
$projectSentinel = Join-Path $testRoot 'project-must-survive.3mf'
$profileRoot = Join-Path $env:APPDATA 'BambuStudio'
$profileSentinel = Join-Path $profileRoot 'md3-installer-ci-profile.keep'
$createdProfileRoot = -not (Test-Path -LiteralPath $profileRoot)

function Assert-True {
    param(
        [Parameter(Mandatory)]
        [bool] $Condition,

        [Parameter(Mandatory)]
        [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-Executable {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,

        [string] $Arguments = '',

        [int] $TimeoutSeconds = 900
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Could not start '$FilePath'."
    }

    try {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill($true)
            throw "'$FilePath' did not exit within $TimeoutSeconds seconds."
        }
        return $process.ExitCode
    }
    finally {
        $process.Dispose()
    }
}

function Get-InstallerMarker {
    if (-not (Test-Path -LiteralPath $productRegistry)) {
        return $null
    }

    return (Get-ItemProperty -LiteralPath $productRegistry -Name InstallerId -ErrorAction SilentlyContinue).InstallerId
}

function Invoke-StagedUninstaller {
    $uninstaller = Join-Path $installDir 'Uninstall.exe'
    Assert-True (Test-Path -LiteralPath $uninstaller -PathType Leaf) "Expected uninstaller '$uninstaller' is missing."

    $staged = Join-Path $testRoot ('Staged-Uninstall-' + [guid]::NewGuid().ToString('N') + '.exe')
    Copy-Item -LiteralPath $uninstaller -Destination $staged
    try {
        return Invoke-Executable -FilePath $staged -Arguments "/S _?=$installDir"
    }
    finally {
        Remove-Item -LiteralPath $staged -Force -ErrorAction SilentlyContinue
    }
}

function Assert-Installed {
    param([Parameter(Mandatory)][string] $ExpectedLanguageMode)

    Assert-True (Test-Path -LiteralPath (Join-Path $installDir 'bambu-studio.exe') -PathType Leaf) 'Installed application executable is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $installDir 'Uninstall.exe') -PathType Leaf) 'Installed recovery uninstaller is missing.'
    Assert-True ((Get-InstallerMarker) -eq $installerId) 'Installer ownership marker is missing or invalid.'
    Assert-True (Test-Path -LiteralPath $uninstallRegistry) 'Windows uninstall registration is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $shortcutDir 'Bambu Studio MD3.lnk') -PathType Leaf) 'Start-menu application shortcut is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $shortcutDir 'Uninstall Bambu Studio MD3.lnk') -PathType Leaf) 'Start-menu uninstall shortcut is missing.'
    $productMode = (Get-ItemProperty -LiteralPath $productRegistry -Name LanguageMode -ErrorAction Stop).LanguageMode
    $preferenceMode = (Get-ItemProperty -LiteralPath $preferenceRegistry -Name LanguageMode -ErrorAction Stop).LanguageMode
    Assert-True ($productMode -eq $ExpectedLanguageMode) "Product language hand-off is '$productMode', expected '$ExpectedLanguageMode'."
    Assert-True ($preferenceMode -eq $ExpectedLanguageMode) "Persistent language preference is '$preferenceMode', expected '$ExpectedLanguageMode'."
}

$resolvedInstaller = (Resolve-Path -LiteralPath $Installer).Path
$resolvedPreviousInstaller = (Resolve-Path -LiteralPath $PreviousInstaller).Path
$resolvedRunnerTemp = (Resolve-Path -LiteralPath $runnerTemp).Path.TrimEnd('\')
$installRootFull = [System.IO.Path]::GetFullPath($installDir).TrimEnd('\')
Assert-True (-not [string]::IsNullOrWhiteSpace($PreviousOwnedFile)) 'PreviousOwnedFile must not be empty.'
Assert-True (-not [System.IO.Path]::IsPathRooted($PreviousOwnedFile)) 'PreviousOwnedFile must be relative.'
$previousOwnedPath = [System.IO.Path]::GetFullPath((Join-Path $installDir $PreviousOwnedFile))
Assert-True ($previousOwnedPath.StartsWith($installRootFull + '\', [System.StringComparison]::OrdinalIgnoreCase)) 'PreviousOwnedFile escapes the fixed install directory.'

Assert-True (-not (Test-Path -LiteralPath $installDir)) "Refusing to overwrite pre-existing path '$installDir' on the runner."
Assert-True (-not (Test-Path -LiteralPath $productRegistry)) 'Refusing to overwrite a pre-existing Bambu Studio MD3 ownership registration.'
Assert-True (-not (Test-Path -LiteralPath $preferenceRegistry)) 'Refusing to overwrite a pre-existing Bambu Studio MD3 language preference.'
Assert-True (-not (Test-Path -LiteralPath $uninstallRegistry)) 'Refusing to overwrite a pre-existing Bambu Studio MD3 uninstall registration.'
Assert-True (-not (Test-Path -LiteralPath $shortcutDir)) 'Refusing to overwrite a pre-existing Bambu Studio MD3 shortcut directory.'
Assert-True (-not (Test-Path -LiteralPath $profileSentinel)) 'Refusing to overwrite a pre-existing Bambu Studio profile sentinel.'

New-Item -ItemType Directory -Path $testRoot | Out-Null
New-Item -ItemType Directory -Path $profileRoot -Force | Out-Null
Set-Content -LiteralPath $projectSentinel -Value 'preserve project' -Encoding ascii
Set-Content -LiteralPath $profileSentinel -Value 'preserve profile' -Encoding ascii

try {
    Write-Host 'Installing the previous owned-payload fixture...'
    $exitCode = Invoke-Executable -FilePath $resolvedPreviousInstaller -Arguments '/S /LANGMODE=en'
    Assert-True ($exitCode -eq 0) "Previous fixture installer returned $exitCode."
    Assert-Installed -ExpectedLanguageMode 'en'

    $obsoletePath = $previousOwnedPath
    Assert-True (Test-Path -LiteralPath $obsoletePath -PathType Leaf) "Previous owned fixture '$PreviousOwnedFile' was not installed."

    Write-Host 'Upgrading synchronously to the current installer...'
    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S /LANGMODE=yue_HK'
    Assert-True ($exitCode -eq 0) "Current installer upgrade returned $exitCode."
    Assert-Installed -ExpectedLanguageMode 'yue_HK'
    Assert-True (-not (Test-Path -LiteralPath $obsoletePath)) 'Upgrade left an obsolete file owned by the previous installer.'

    Write-Host 'Verifying unknown-path preservation and fail-closed upgrade behavior...'
    $unknownPath = Join-Path $installDir 'unknown-user-file.keep'
    Set-Content -LiteralPath $unknownPath -Value 'not owned by installer' -Encoding ascii
    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S'
    Assert-True ($exitCode -ne 0) 'Upgrade unexpectedly succeeded while an unknown path remained.'
    Assert-True (Test-Path -LiteralPath $unknownPath -PathType Leaf) 'Upgrade removed an unknown path.'

    Remove-Item -LiteralPath $unknownPath -Force
    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S'
    Assert-True ($exitCode -eq 0) "Clean recovery install returned $exitCode."
    Assert-Installed -ExpectedLanguageMode 'yue_HK'

    Write-Host 'Verifying locked-file uninstall recovery...'
    $lockedPath = Join-Path $installDir 'bambu-studio.exe'
    $lock = [System.IO.File]::Open($lockedPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
    try {
        $exitCode = Invoke-StagedUninstaller
        Assert-True ($exitCode -ne 0) 'Uninstall unexpectedly succeeded while an owned file was exclusively locked.'
        Assert-True (Test-Path -LiteralPath $lockedPath -PathType Leaf) 'Locked owned file was removed.'
        Assert-True ((Get-InstallerMarker) -eq $installerId) 'Failed uninstall did not preserve the ownership marker.'
        Assert-True (Test-Path -LiteralPath $uninstallRegistry) 'Failed uninstall did not preserve Windows uninstall registration.'
    }
    finally {
        $lock.Dispose()
    }

    $exitCode = Invoke-StagedUninstaller
    Assert-True ($exitCode -eq 0) "Retry uninstall returned $exitCode."
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $installDir 'bambu-studio.exe'))) 'Retry uninstall left the owned application executable.'
    Assert-True (-not (Test-Path -LiteralPath $installDir)) 'Retry uninstall left the installation directory.'
    Assert-True (-not (Test-Path -LiteralPath $productRegistry)) 'Retry uninstall left the ownership registration.'
    Assert-True (-not (Test-Path -LiteralPath $uninstallRegistry)) 'Retry uninstall left Windows uninstall registration.'
    Assert-True (-not (Test-Path -LiteralPath $shortcutDir)) 'Retry uninstall left the Start-menu shortcut directory.'
    Assert-True (Test-Path -LiteralPath $profileSentinel -PathType Leaf) 'Uninstall removed a profile/project sentinel outside the install directory.'
    Assert-True (Test-Path -LiteralPath $projectSentinel -PathType Leaf) 'Uninstall removed the external project sentinel.'
    Assert-True ((Get-ItemProperty -LiteralPath $preferenceRegistry -Name LanguageMode).LanguageMode -eq 'yue_HK') 'Uninstall did not preserve the selected language preference.'

    Write-Host 'Verifying bilingual mode hand-off and invalid-mode rejection...'
    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S /LANGMODE=not-a-mode'
    Assert-True ($exitCode -ne 0) 'Installer unexpectedly accepted an invalid language mode.'
    Assert-True (-not (Test-Path -LiteralPath $installDir)) 'Invalid language mode created an install directory.'

    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S /LANGMODE=bilingual_en_yue_HK'
    Assert-True ($exitCode -eq 0) "Bilingual-mode install returned $exitCode."
    Assert-Installed -ExpectedLanguageMode 'bilingual_en_yue_HK'
    $exitCode = Invoke-StagedUninstaller
    Assert-True ($exitCode -eq 0) "Bilingual-mode uninstall returned $exitCode."
    Assert-True (-not (Test-Path -LiteralPath $installDir)) 'Bilingual-mode uninstall left the installation directory.'

    Write-Host 'Verifying destination-junction rejection...'
    $installParent = Split-Path -Parent $installDir
    New-Item -ItemType Directory -Path $installParent -Force | Out-Null
    $junctionTarget = Join-Path $testRoot 'junction-target'
    New-Item -ItemType Directory -Path $junctionTarget | Out-Null
    New-Item -ItemType Junction -Path $installDir -Target $junctionTarget | Out-Null
    try {
        $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S'
        Assert-True ($exitCode -ne 0) 'Installer unexpectedly accepted a destination junction.'
        Assert-True ((Get-ChildItem -LiteralPath $junctionTarget -Force | Measure-Object).Count -eq 0) 'Installer wrote through the rejected destination junction.'
    }
    finally {
        $item = Get-Item -LiteralPath $installDir -Force -ErrorAction SilentlyContinue
        if ($item) {
            Assert-True ($item.LinkType -eq 'Junction') 'Refusing to remove an install path that is no longer the test junction.'
            Remove-Item -LiteralPath $installDir -Force
        }
    }

    Write-Host 'Windows installer behavior tests passed.'
}
finally {
    if ((Get-InstallerMarker) -eq $installerId -and (Test-Path -LiteralPath (Join-Path $installDir 'Uninstall.exe'))) {
        try {
            Invoke-StagedUninstaller | Out-Null
        }
        catch {
            Write-Warning "Best-effort owned-install cleanup failed: $($_.Exception.Message)"
        }
    }

    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
        Assert-True ($resolvedTestRoot.StartsWith($resolvedRunnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) 'Refusing to clean a test path outside RUNNER_TEMP.'
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }

    if (Test-Path -LiteralPath $preferenceRegistry) {
        Remove-Item -LiteralPath $preferenceRegistry -Recurse -Force
    }
    if (Test-Path -LiteralPath $profileSentinel) {
        Remove-Item -LiteralPath $profileSentinel -Force
    }
    if ($createdProfileRoot -and (Test-Path -LiteralPath $profileRoot) -and
        (Get-ChildItem -LiteralPath $profileRoot -Force | Measure-Object).Count -eq 0) {
        Remove-Item -LiteralPath $profileRoot -Force
    }
}
