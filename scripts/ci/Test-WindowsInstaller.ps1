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
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Sbom,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $AncestorGuardInstaller,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $BootstrapFailureInstaller,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $ShortcutGuardInstaller,

    [Parameter(Mandatory)]
    [string] $AncestorGuardRoot,

    [Parameter(Mandatory)]
    [string] $ShortcutGuardRoot,

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
    $recoveryState = (Get-ItemProperty -LiteralPath $productRegistry -Name RecoveryState -ErrorAction Stop).RecoveryState
    Assert-True ($productMode -eq $ExpectedLanguageMode) "Product language hand-off is '$productMode', expected '$ExpectedLanguageMode'."
    Assert-True ($preferenceMode -eq $ExpectedLanguageMode) "Persistent language preference is '$preferenceMode', expected '$ExpectedLanguageMode'."
    Assert-True ($recoveryState -eq 'ready') "Installed recovery state is '$recoveryState', expected 'ready'."
}

function Assert-InstalledPayloadMatchesSbom {
    $document = Get-Content -LiteralPath $resolvedSbom -Raw | ConvertFrom-Json
    $expected = @{}
    foreach ($component in @($document.components)) {
        $name = [string] $component.name
        Assert-True (-not [string]::IsNullOrWhiteSpace($name)) 'SBOM contains an unnamed payload component.'
        Assert-True (-not $expected.ContainsKey($name)) "SBOM contains duplicate component '$name'."
        $sha256 = @($component.hashes | Where-Object { $_.alg -eq 'SHA-256' })
        Assert-True ($sha256.Count -eq 1 -and [string] $sha256[0].content -match '^[0-9a-f]{64}$') `
            "SBOM component '$name' does not contain exactly one lowercase SHA-256 digest."
        $expected[$name] = [string] $sha256[0].content
    }

    $actual = @{}
    foreach ($file in @(Get-ChildItem -LiteralPath $installDir -Recurse -Force -File)) {
        $relative = [System.IO.Path]::GetRelativePath($installDir, $file.FullName).Replace('\', '/')
        if ($relative -ceq 'Uninstall.exe') {
            continue
        }
        Assert-True (-not $actual.ContainsKey($relative)) "Installed payload contains duplicate path '$relative'."
        $actual[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }

    Assert-True ($actual.Count -eq $expected.Count) `
        "Installed payload has $($actual.Count) files, while the SBOM describes $($expected.Count)."
    foreach ($name in @($expected.Keys | Sort-Object)) {
        Assert-True ($actual.ContainsKey($name)) "Installed payload is missing SBOM component '$name'."
        Assert-True ($actual[$name] -ceq $expected[$name]) "Installed payload hash differs from the SBOM for '$name'."
    }
    foreach ($name in @($actual.Keys | Sort-Object)) {
        Assert-True ($expected.ContainsKey($name)) "Installed payload contains '$name', which is absent from the SBOM."
    }
}

$resolvedInstaller = (Resolve-Path -LiteralPath $Installer).Path
$resolvedPreviousInstaller = (Resolve-Path -LiteralPath $PreviousInstaller).Path
$resolvedSbom = (Resolve-Path -LiteralPath $Sbom).Path
$resolvedAncestorGuardInstaller = (Resolve-Path -LiteralPath $AncestorGuardInstaller).Path
$resolvedBootstrapFailureInstaller = (Resolve-Path -LiteralPath $BootstrapFailureInstaller).Path
$resolvedShortcutGuardInstaller = (Resolve-Path -LiteralPath $ShortcutGuardInstaller).Path
$resolvedRunnerTemp = (Resolve-Path -LiteralPath $runnerTemp).Path.TrimEnd('\')
$resolvedAncestorGuardRoot = [System.IO.Path]::GetFullPath($AncestorGuardRoot).TrimEnd('\')
$resolvedShortcutGuardRoot = [System.IO.Path]::GetFullPath($ShortcutGuardRoot).TrimEnd('\')
$installRootFull = [System.IO.Path]::GetFullPath($installDir).TrimEnd('\')
Assert-True ($resolvedAncestorGuardRoot.StartsWith($resolvedRunnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) `
    'AncestorGuardRoot must be a dedicated path beneath RUNNER_TEMP.'
Assert-True ($resolvedShortcutGuardRoot.StartsWith($resolvedRunnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) `
    'ShortcutGuardRoot must be a dedicated path beneath RUNNER_TEMP.'
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
Assert-True (-not (Test-Path -LiteralPath $resolvedAncestorGuardRoot)) `
    "Refusing to overwrite pre-existing ancestor-guard fixture root '$resolvedAncestorGuardRoot'."
Assert-True (-not (Test-Path -LiteralPath $resolvedShortcutGuardRoot)) `
    "Refusing to overwrite pre-existing shortcut-guard fixture root '$resolvedShortcutGuardRoot'."

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
    Write-Host 'Comparing the installed payload exactly with the attested SBOM input...'
    Assert-InstalledPayloadMatchesSbom

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

    Write-Host 'Verifying recoverable bootstrap-cleanup failure...'
    $exitCode = Invoke-Executable -FilePath $resolvedBootstrapFailureInstaller -Arguments '/S /LANGMODE=en'
    Assert-True ($exitCode -ne 0) 'Bootstrap-failure fixture unexpectedly succeeded.'
    Assert-True ((Get-InstallerMarker) -eq $installerId) `
        'Bootstrap cleanup failure did not preserve the ownership marker.'
    Assert-True (Test-Path -LiteralPath (Join-Path $installDir 'Uninstall.exe') -PathType Leaf) `
        'Bootstrap cleanup failure did not preserve its recovery file.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $installDir 'bambu-studio.exe'))) `
        'Bootstrap failure fixture extracted application payload unexpectedly.'
    Assert-True (-not (Test-Path -LiteralPath $uninstallRegistry)) `
        'Bootstrap cleanup failure exposed a potentially incomplete Windows uninstall entry.'
    $bootstrapState = (Get-ItemProperty -LiteralPath $productRegistry -Name RecoveryState -ErrorAction Stop).RecoveryState
    Assert-True ($bootstrapState -eq 'bootstrap_cleanup') `
        "Bootstrap cleanup state is '$bootstrapState', expected 'bootstrap_cleanup'."

    $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S /LANGMODE=yue_HK'
    Assert-True ($exitCode -eq 0) "Installer could not recover from bootstrap cleanup; returned $exitCode."
    Assert-Installed -ExpectedLanguageMode 'yue_HK'
    $exitCode = Invoke-StagedUninstaller
    Assert-True ($exitCode -eq 0) "Bootstrap-recovery uninstall returned $exitCode."
    Assert-True (-not (Test-Path -LiteralPath $installDir)) `
        'Bootstrap-recovery uninstall left the installation directory.'

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

    Write-Host 'Verifying unknown Start-menu path preservation...'
    New-Item -ItemType Directory -Path $shortcutDir | Out-Null
    $unknownShortcutPath = Join-Path $shortcutDir 'unknown-user-shortcut.keep'
    Set-Content -LiteralPath $unknownShortcutPath -Value 'not owned by installer' -Encoding ascii
    try {
        $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S'
        Assert-True ($exitCode -ne 0) 'Installer unexpectedly adopted an unknown Start-menu path.'
        Assert-True (Test-Path -LiteralPath $unknownShortcutPath -PathType Leaf) `
            'Installer removed an unknown Start-menu path.'
        Assert-True (-not (Test-Path -LiteralPath $installDir)) `
            'Unknown Start-menu path rejection still created an application directory.'
    }
    finally {
        Remove-Item -LiteralPath $unknownShortcutPath -Force -ErrorAction SilentlyContinue
        if ((Test-Path -LiteralPath $shortcutDir) -and
            (Get-ChildItem -LiteralPath $shortcutDir -Force | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $shortcutDir -Force
        }
    }

    Write-Host 'Verifying product Start-menu junction rejection...'
    $shortcutParent = Split-Path -Parent $shortcutDir
    New-Item -ItemType Directory -Path $shortcutParent -Force | Out-Null
    $shortcutTarget = Join-Path $testRoot 'shortcut-junction-target'
    New-Item -ItemType Directory -Path $shortcutTarget | Out-Null
    New-Item -ItemType Junction -Path $shortcutDir -Target $shortcutTarget | Out-Null
    try {
        $exitCode = Invoke-Executable -FilePath $resolvedInstaller -Arguments '/S'
        Assert-True ($exitCode -ne 0) 'Installer unexpectedly accepted a Start-menu product junction.'
        Assert-True ((Get-ChildItem -LiteralPath $shortcutTarget -Force | Measure-Object).Count -eq 0) `
            'Installer wrote through the rejected Start-menu product junction.'
    }
    finally {
        $item = Get-Item -LiteralPath $shortcutDir -Force -ErrorAction SilentlyContinue
        if ($item) {
            Assert-True ($item.LinkType -eq 'Junction') `
                'Refusing to remove a Start-menu path that is no longer the test junction.'
            Remove-Item -LiteralPath $shortcutDir -Force
        }
    }

    Write-Host 'Verifying Programs-parent junction rejection...'
    $ancestorPrograms = Join-Path $resolvedAncestorGuardRoot 'Programs'
    $ancestorTarget = Join-Path $testRoot 'ancestor-junction-target'
    New-Item -ItemType Directory -Path $resolvedAncestorGuardRoot | Out-Null
    New-Item -ItemType Directory -Path $ancestorTarget | Out-Null
    New-Item -ItemType Junction -Path $ancestorPrograms -Target $ancestorTarget | Out-Null
    try {
        $exitCode = Invoke-Executable -FilePath $resolvedAncestorGuardInstaller -Arguments '/S'
        Assert-True ($exitCode -ne 0) 'Installer unexpectedly accepted a Programs-parent junction.'
        Assert-True ((Get-ChildItem -LiteralPath $ancestorTarget -Force | Measure-Object).Count -eq 0) `
            'Installer wrote through the rejected Programs-parent junction.'
        Assert-True (-not (Test-Path -LiteralPath $productRegistry)) `
            'Programs-parent junction rejection created an ownership registration.'
        Assert-True (-not (Test-Path -LiteralPath $uninstallRegistry)) `
            'Programs-parent junction rejection created an uninstall registration.'
    }
    finally {
        $item = Get-Item -LiteralPath $ancestorPrograms -Force -ErrorAction SilentlyContinue
        if ($item) {
            Assert-True ($item.LinkType -eq 'Junction') `
                'Refusing to remove an ancestor fixture path that is no longer the test junction.'
            Remove-Item -LiteralPath $ancestorPrograms -Force
        }
        if ((Test-Path -LiteralPath $resolvedAncestorGuardRoot) -and
            (Get-ChildItem -LiteralPath $resolvedAncestorGuardRoot -Force | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $resolvedAncestorGuardRoot -Force
        }
    }

    Write-Host 'Verifying Start-menu parent junction rejection...'
    $shortcutAncestorTarget = Join-Path $testRoot 'shortcut-ancestor-target'
    New-Item -ItemType Directory -Path $shortcutAncestorTarget | Out-Null
    New-Item -ItemType Junction -Path $resolvedShortcutGuardRoot -Target $shortcutAncestorTarget | Out-Null
    try {
        $exitCode = Invoke-Executable -FilePath $resolvedShortcutGuardInstaller -Arguments '/S'
        Assert-True ($exitCode -ne 0) 'Installer unexpectedly accepted a Start-menu parent junction.'
        Assert-True ((Get-ChildItem -LiteralPath $shortcutAncestorTarget -Force | Measure-Object).Count -eq 0) `
            'Installer wrote through the rejected Start-menu parent junction.'
        Assert-True (-not (Test-Path -LiteralPath $installDir)) `
            'Start-menu parent rejection still created an application directory.'
    }
    finally {
        $item = Get-Item -LiteralPath $resolvedShortcutGuardRoot -Force -ErrorAction SilentlyContinue
        if ($item) {
            Assert-True ($item.LinkType -eq 'Junction') `
                'Refusing to remove a shortcut fixture root that is no longer the test junction.'
            Remove-Item -LiteralPath $resolvedShortcutGuardRoot -Force
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

    if (Test-Path -LiteralPath $resolvedAncestorGuardRoot) {
        $ancestorPrograms = Join-Path $resolvedAncestorGuardRoot 'Programs'
        $item = Get-Item -LiteralPath $ancestorPrograms -Force -ErrorAction SilentlyContinue
        if ($item -and $item.LinkType -eq 'Junction') {
            Remove-Item -LiteralPath $ancestorPrograms -Force
        }
        if ((Get-ChildItem -LiteralPath $resolvedAncestorGuardRoot -Force | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $resolvedAncestorGuardRoot -Force
        }
    }


    if (Test-Path -LiteralPath $resolvedShortcutGuardRoot) {
        $item = Get-Item -LiteralPath $resolvedShortcutGuardRoot -Force -ErrorAction SilentlyContinue
        if ($item -and $item.LinkType -eq 'Junction') {
            Remove-Item -LiteralPath $resolvedShortcutGuardRoot -Force
        } elseif ($item -and (Get-ChildItem -LiteralPath $resolvedShortcutGuardRoot -Force | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $resolvedShortcutGuardRoot -Force
        }
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
