[CmdletBinding()]
param(
    [string] $RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-True {
    param(
        [Parameter(Mandatory)][bool] $Condition,
        [Parameter(Mandatory)][string] $Message
    )
    if (-not $Condition) { throw $Message }
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$buildFromSourceDir = Join-Path $RepositoryRoot 'packaging\windows\build-from-source'
$buildScript = Join-Path $buildFromSourceDir 'Build-FromSource.ps1'
$toolchainScript = Join-Path $buildFromSourceDir 'Toolchain.ps1'
$nativeVisualScript = Join-Path $RepositoryRoot 'scripts\ci\Test-WindowsNativeVisual.ps1'

foreach ($path in @(
    $buildScript,
    $toolchainScript,
    (Join-Path $buildFromSourceDir 'Opencode.ps1'),
    $nativeVisualScript
)) {
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "Missing helper '$path'."
    # Deliberately use the host's default decoding. The installer launches the
    # orchestrator through Windows PowerShell 5.1, which treats no-BOM files as ANSI.
    $null = [scriptblock]::Create((Get-Content -LiteralPath $path -Raw))
}

$nativeVisualBytes = [System.IO.File]::ReadAllBytes($nativeVisualScript)
Assert-True (@($nativeVisualBytes | Where-Object { $_ -gt 0x7F }).Count -eq 0) `
    'Test-WindowsNativeVisual.ps1 must remain ASCII-safe for Windows PowerShell 5.1.'

$buildText = Get-Content -LiteralPath $buildScript -Raw
Assert-True (-not $buildText.Contains('[System.IO.Path]::GetRelativePath')) `
    'Build-FromSource.ps1 still uses Path.GetRelativePath, which is unavailable in Windows PowerShell 5.1.'
Assert-True ($buildText.Contains('& cmake --install build --config Release --prefix $installDir')) `
    'Build-FromSource.ps1 must stage with cmake --install --prefix.'
Assert-True (-not ($buildText -match 'cmake\s+--build[^\r\n]+CMAKE_INSTALL_PREFIX')) `
    'CMAKE_INSTALL_PREFIX must not be passed to cmake --build.'
Assert-True (@([regex]::Matches($buildText, 'build_win\.bat -v 17 -p \$vsProduct -c Release')).Count -eq 2) `
    'Dependency and application phases must both pin the detected Visual Studio 2022 product and Release.'
Assert-True ($buildText.Contains("[ValidatePattern('^[0-9a-fA-F]{40}$')]") -and
    $buildText.Contains('& git checkout --detach $Tag')) `
    'Build-From-Source.ps1 must accept only a full source commit and check it out detached.'
Assert-True ($buildText.Contains('& git rev-parse HEAD') -and
    $buildText.Contains('does not match requested source commit ''$Tag''')) `
    'Build-From-Source.ps1 must verify the checked-out commit exactly before building.'

$tokens = $null
$parseErrors = $null
$buildAst = [System.Management.Automation.Language.Parser]::ParseInput(
    $buildText,
    [ref]$tokens,
    [ref]$parseErrors
)
Assert-True ($parseErrors.Count -eq 0) 'Build-FromSource.ps1 failed AST parsing.'
$writeManifestAst = $buildAst.Find({
    param($node)
    return ($node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Write-Manifest')
}, $true)
Assert-True ($null -ne $writeManifestAst) 'Write-Manifest was not found in Build-FromSource.ps1.'
. ([scriptblock]::Create($writeManifestAst.Extent.Text))

. $toolchainScript

Assert-True ((Get-SafeRelativePath -Root 'C:\bfs root' -Path 'C:\bfs root\nested\file.txt') -eq 'nested\file.txt') `
    'The Windows PowerShell 5.1 relative-path helper returned the wrong path.'
$outsideRejected = $false
try {
    $null = Get-SafeRelativePath -Root 'C:\bfs root' -Path 'C:\outside\file.txt'
} catch {
    $outsideRejected = $true
}
Assert-True $outsideRejected 'The relative-path helper accepted an item outside its root.'

$testDir = Join-Path ([System.IO.Path]::GetTempPath()) ('bfs-helper-test-' + [guid]::NewGuid().ToString('N'))
$originalPath = $env:Path
try {
    New-Item -ItemType Directory -Path $testDir | Out-Null
    $fakeNpm = Join-Path $testDir 'npm.cmd'
    $fakeNodeLts = Join-Path $testDir 'node-lts.cmd'
    $fakeNodeOldLts = Join-Path $testDir 'node-old-lts.cmd'
    $fakeNodeNewLts = Join-Path $testDir 'node-new-lts.cmd'
    $fakeNodeArm = Join-Path $testDir 'node-arm.cmd'
    $fakeNodeCurrent = Join-Path $testDir 'node-current.cmd'
    $fakeCMakeNew = Join-Path $testDir 'cmake-new.cmd'
    $fakeCMakeOld = Join-Path $testDir 'cmake-old.cmd'
    $fakeCMakeFuture = Join-Path $testDir 'cmake-future.cmd'
    Set-Content -LiteralPath $fakeNpm -Encoding Ascii -Value '@exit /b 0'
    Set-Content -LiteralPath $fakeNodeLts -Encoding Ascii -Value @('@echo off', 'echo maintenance-lts^|22.22.2^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeOldLts -Encoding Ascii -Value @('@echo off', 'echo old-lts^|16.20.2^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeNewLts -Encoding Ascii -Value @('@echo off', 'echo newer-lts^|24.12.0^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeArm -Encoding Ascii -Value @('@echo off', 'echo maintenance-lts^|22.22.2^|arm64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeCurrent -Encoding Ascii -Value @('@echo off', 'echo.', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMakeNew -Encoding Ascii -Value @('@echo off', 'echo cmake version 4.4.0', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMakeOld -Encoding Ascii -Value @('@echo off', 'echo cmake version 3.20.6', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMakeFuture -Encoding Ascii -Value @('@echo off', 'echo cmake version 5.0.0', 'exit /b 0')

    Assert-True (Test-NodeLts -NodePath $fakeNodeLts -NpmPath $fakeNpm) `
        'The Node probe rejected an LTS runtime with npm.'
    Assert-True (-not (Test-NodeLts -NodePath $fakeNodeCurrent -NpmPath $fakeNpm)) `
        'The Node probe accepted a non-LTS runtime.'
    Assert-True (-not (Test-NodeLts -NodePath $fakeNodeOldLts -NpmPath $fakeNpm)) `
        'The Node probe accepted an unsupported historical LTS runtime.'
    Assert-True (Test-NodeLts -NodePath $fakeNodeNewLts -NpmPath $fakeNpm) `
        'The Node probe rejected a newer supported LTS runtime and would force a downgrade.'
    Assert-True (-not (Test-NodeLts -NodePath $fakeNodeArm -NpmPath $fakeNpm)) `
        'The Node probe accepted an unsupported architecture.'
    Assert-True (Test-CMakeVersion -CMakePath $fakeCMakeNew) `
        'The CMake probe rejected a supported version.'
    Assert-True (-not (Test-CMakeVersion -CMakePath $fakeCMakeOld)) `
        'The CMake probe accepted a version below the minimum.'
    Assert-True (-not (Test-CMakeVersion -CMakePath $fakeCMakeFuture)) `
        'The CMake probe accepted an unsupported future major version.'

    $trustedPublisher = @('Microsoft Corporation')
    Assert-True (Test-InstallerSignatureMetadata -Status 'Valid' `
            -Publisher 'Microsoft Corporation' -TrustedPublishers $trustedPublisher) `
        'Installer trust rejected a valid signature from the exact trusted publisher.'
    Assert-True (-not (Test-InstallerSignatureMetadata -Status 'NotSigned' `
            -Publisher 'Microsoft Corporation' -TrustedPublishers $trustedPublisher)) `
        'Installer trust accepted a missing Authenticode signature.'
    Assert-True (-not (Test-InstallerSignatureMetadata -Status 'HashMismatch' `
            -Publisher 'Microsoft Corporation' -TrustedPublishers $trustedPublisher)) `
        'Installer trust accepted an invalid Authenticode signature.'
    Assert-True (-not (Test-InstallerSignatureMetadata -Status 'Valid' `
            -Publisher '' -TrustedPublishers $trustedPublisher)) `
        'Installer trust accepted a signature with no publisher identity.'
    Assert-True (-not (Test-InstallerSignatureMetadata -Status 'Valid' `
            -Publisher 'Contoso Software' -TrustedPublishers $trustedPublisher)) `
        'Installer trust accepted an untrusted publisher.'
    Assert-True (-not (Test-InstallerSignatureMetadata -Status 'Valid' `
            -Publisher 'microsoft corporation' -TrustedPublishers $trustedPublisher)) `
        'Installer trust must compare publisher identities exactly.'

    $fakeSdkRoot = Join-Path $testDir 'Windows Kits\10'
    $fakeSdkVersion = '10.0.26100.0'
    foreach ($directory in @(
        (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\um"),
        (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\shared"),
        (Join-Path $fakeSdkRoot "Lib\$fakeSdkVersion\um\x64")
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\um\Windows.h") `
        -Encoding Ascii -Value 'fixture'
    Set-Content -LiteralPath (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\shared\sdkddkver.h") `
        -Encoding Ascii -Value 'fixture'
    Set-Content -LiteralPath (Join-Path $fakeSdkRoot "Lib\$fakeSdkVersion\um\x64\kernel32.lib") `
        -Encoding Ascii -Value 'fixture'

    Assert-True ($null -eq (Get-WindowsSdkVersion -Roots @($fakeSdkRoot))) `
        'The Windows SDK probe accepted a fixture with no UCRT headers or libraries.'

    foreach ($directory in @(
        (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\ucrt"),
        (Join-Path $fakeSdkRoot "Lib\$fakeSdkVersion\ucrt\x64")
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $fakeSdkRoot "Include\$fakeSdkVersion\ucrt\stdio.h") `
        -Encoding Ascii -Value 'fixture'
    Set-Content -LiteralPath (Join-Path $fakeSdkRoot "Lib\$fakeSdkVersion\ucrt\x64\ucrt.lib") `
        -Encoding Ascii -Value 'fixture'

    $fakeVisualStudio = Join-Path $testDir 'VS2022-Community'
    foreach ($file in @(
        (Join-Path $fakeVisualStudio 'Common7\Tools\VsDevCmd.bat'),
        (Join-Path $fakeVisualStudio 'MSBuild\Current\Bin\MSBuild.exe')
    )) {
        New-Item -ItemType Directory -Path ([System.IO.Path]::GetDirectoryName($file)) -Force | Out-Null
        Set-Content -LiteralPath $file -Encoding Ascii -Value 'fixture'
    }
    $fakeVsWhere = Join-Path $testDir 'vswhere.cmd'
    Set-Content -LiteralPath $fakeVsWhere -Encoding Ascii -Value @(
        '@echo off',
        'echo %* | findstr /L /C:"-products *" >nul || exit /b 7',
        'echo %* | findstr /C:"-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64" >nul || exit /b 8',
        'echo %* | findstr /C:"-property productId" >nul && (echo Microsoft.VisualStudio.Product.Community& exit /b 0)',
        "echo $fakeVisualStudio"
    )
    Assert-True ((Get-WindowsSdkVersion -Roots @($fakeSdkRoot)) -eq [version]$fakeSdkVersion) `
        'The Windows SDK probe rejected a complete supported fixture.'
    Assert-True ((Get-VisualStudio2022Path -VsWherePath $fakeVsWhere) -eq $fakeVisualStudio) `
        'The Visual Studio probe rejected a complete VS 2022 Community fixture.'
    Assert-True ((Get-VisualStudio2022Product -VsWherePath $fakeVsWhere) -eq 'Community') `
        'The Visual Studio product probe did not preserve the installed Community SKU.'
    Assert-True (Test-VisualCppBuildTools -VsWherePath $fakeVsWhere -WindowsSdkRoots @($fakeSdkRoot)) `
        'The combined Visual Studio C++ and Windows SDK fixture probe failed.'

    $payloadDir = Join-Path $testDir 'payload'
    $nestedDir = Join-Path $payloadDir 'nested'
    New-Item -ItemType Directory -Path $nestedDir | Out-Null
    Set-Content -LiteralPath (Join-Path $payloadDir 'app.exe') -Encoding Ascii -Value 'fixture'
    Set-Content -LiteralPath (Join-Path $nestedDir 'resource.txt') -Encoding Ascii -Value 'fixture'
    $manifestPath = Join-Path $testDir 'owned-manifest.txt'
    $script:Utf16Bom = New-Object System.Text.UnicodeEncoding($false, $true)
    Write-Manifest -PayloadDir $payloadDir -OutFile $manifestPath
    $manifestLines = @(Get-Content -LiteralPath $manifestPath -Encoding Unicode)
    Assert-True ($manifestLines -contains 'F|app.exe') 'Manifest omitted its root file.'
    Assert-True ($manifestLines -contains 'F|nested\resource.txt') 'Manifest omitted its nested file.'
    Assert-True ($manifestLines -contains 'D|nested') 'Manifest omitted its nested directory.'

    $portablePath = Join-Path $testDir 'portable-tools'
    $registeredEntries = @(
        [System.Environment]::GetEnvironmentVariable('Path', 'Machine'),
        [System.Environment]::GetEnvironmentVariable('Path', 'User')
    ) | ForEach-Object { @([string]$_ -split ';') } | ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique
    # Begin with process-only portable entries. Update-SessionPath must retain
    # their precedence while appending every registry entry needed to discover
    # a tool installed during this same process.
    $env:Path = "$portablePath;$portablePath\"
    Update-SessionPath
    $sessionEntries = @($env:Path -split ';')
    $portableMatches = @($sessionEntries | Where-Object {
        $_.TrimEnd('\') -ieq $portablePath.TrimEnd('\')
    })
    Assert-True ($portableMatches.Count -eq 1) `
        'PATH refresh dropped or duplicated a process-only portable path.'
    Assert-True ($sessionEntries[0].TrimEnd('\') -ieq $portablePath.TrimEnd('\')) `
        'PATH refresh allowed a registry entry to shadow the caller process PATH.'
    foreach ($registeredEntry in $registeredEntries) {
        Assert-True (@($sessionEntries | Where-Object {
            $_.TrimEnd('\') -ieq $registeredEntry.TrimEnd('\')
        }).Count -eq 1) `
            "PATH refresh did not append registry entry '$registeredEntry' exactly once."
    }
}
finally {
    $env:Path = $originalPath
    if (Test-Path -LiteralPath $testDir) {
        $resolvedTestDir = [System.IO.Path]::GetFullPath($testDir)
        $tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not $resolvedTestDir.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
            [System.IO.Path]::GetFileName($resolvedTestDir) -notlike 'bfs-helper-test-*') {
            throw "Refusing to remove unexpected test directory '$resolvedTestDir'."
        }
        Remove-Item -LiteralPath $resolvedTestDir -Recurse -Force
    }
}

$hostSdkVersion = Get-WindowsSdkVersion
$hostVisualStudio = Get-VisualStudio2022Path
Write-Host "Build-from-source helper checks passed under $($PSVersionTable.PSEdition) PowerShell $($PSVersionTable.PSVersion)."
Write-Host "Host observation (not a fixture assertion): VS2022='$hostVisualStudio'; SDK='$hostSdkVersion'."
