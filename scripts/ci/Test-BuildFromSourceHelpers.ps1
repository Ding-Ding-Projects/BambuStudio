[CmdletBinding()]
param(
    [string] $RepositoryRoot = '',
    [switch] $ProbeHostCMake
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

function Invoke-BoundedConfigureProbe {
    param([string] $Executable, [string] $Arguments, [int] $TimeoutMilliseconds = 60000)
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $Executable
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        $null = $process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($TimeoutMilliseconds)
        if ($timedOut -and -not $process.HasExited) {
            # Keep the Process handle open while terminating this freshly
            # launched fixture process and its children, never a stored PID.
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F 2>&1 | Out-Null
            if (-not $process.WaitForExit(5000)) { throw 'The owned configure probe did not exit after timeout teardown.' }
        }
        $output = if ($stdout.Wait(1000)) { $stdout.Result } else { '[stdout drain timed out]' }
        $errorOutput = if ($stderr.Wait(1000)) { $stderr.Result } else { '[stderr drain timed out]' }
        return [pscustomobject]@{ ExitCode = $process.ExitCode; TimedOut = $timedOut; Output = $output; ErrorOutput = $errorOutput }
    } finally { $process.Dispose() }
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
Assert-True (@([regex]::Matches($buildText, 'build_win\.bat -v \$vsMajor -p \$vsProduct -c Release')).Count -eq 2) `
    'Dependency and application phases must both pin the detected Visual Studio major, product, and Release.'
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

$hashFixture = [System.IO.Path]::GetTempFileName()
try {
    [System.IO.File]::WriteAllText($hashFixture, 'abc', (New-Object System.Text.UTF8Encoding($false)))
    Assert-True ((Get-FileSha256 -Path $hashFixture) -eq
        'BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD') `
        'The .NET SHA-256 helper returned the wrong digest.'
}
finally {
    Remove-Item -LiteralPath $hashFixture -Force -ErrorAction SilentlyContinue
}

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
    $fakeCMake2022 = Join-Path $testDir 'cmake-2022.cmd'
    $fakeCMakeFuture = Join-Path $testDir 'cmake-future.cmd'
    Set-Content -LiteralPath $fakeNpm -Encoding Ascii -Value '@exit /b 0'
    Set-Content -LiteralPath $fakeNodeLts -Encoding Ascii -Value @('@echo off', 'echo maintenance-lts^|22.22.2^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeOldLts -Encoding Ascii -Value @('@echo off', 'echo old-lts^|16.20.2^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeNewLts -Encoding Ascii -Value @('@echo off', 'echo newer-lts^|24.12.0^|x64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeArm -Encoding Ascii -Value @('@echo off', 'echo maintenance-lts^|22.22.2^|arm64', 'exit /b 0')
    Set-Content -LiteralPath $fakeNodeCurrent -Encoding Ascii -Value @('@echo off', 'echo.', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMakeNew -Encoding Ascii -Value @('@echo off', 'echo cmake version 4.4.0', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMakeOld -Encoding Ascii -Value @('@echo off', 'echo cmake version 3.20.6', 'exit /b 0')
    Set-Content -LiteralPath $fakeCMake2022 -Encoding Ascii -Value @('@echo off', 'echo cmake version 3.21.0', 'exit /b 0')
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
    Assert-True (Test-CMakeVersion -CMakePath $fakeCMake2022) 'The VS 2022 CMake minimum must remain supported.'
    Assert-True (-not (Test-CMakeVersion -CMakePath $fakeCMake2022 -MinimumVersion ([version]'4.2.0'))) `
        'The VS 2026 route must reject CMake without its generator.'
    Assert-True (Test-CMakeVersion -CMakePath $fakeCMakeNew -MinimumVersion ([version]'4.2.0')) `
        'The VS 2026 route rejected a supported CMake version.'

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
        (Join-Path $fakeVisualStudio 'MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $fakeVisualStudio 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'),
        (Join-Path $fakeVisualStudio 'VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe')
    )) {
        New-Item -ItemType Directory -Path ([System.IO.Path]::GetDirectoryName($file)) -Force | Out-Null
        Set-Content -LiteralPath $file -Encoding Ascii -Value 'fixture'
    }
    Set-Content -LiteralPath (Join-Path $fakeVisualStudio 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Encoding Ascii -Value '14.44.35207'
    $fakeMsbuildDirectory = Join-Path $fakeVisualStudio 'MSBuild\Current\Bin\amd64'
    New-Item -ItemType Directory -Path $fakeMsbuildDirectory -Force | Out-Null
    $fakeMsbuild = Join-Path $fakeMsbuildDirectory 'MSBuild.exe'
    Add-Type -OutputAssembly $fakeMsbuild -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.IO;
public static class BootstrapMSBuildFixture {
    public static int Main(string[] args) {
        if (args.Length > 0 && args[0] == "--sleep") { System.Threading.Thread.Sleep(30000); }
        string state = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "probe-state.txt");
        if (File.Exists(state) && File.ReadAllText(state).Trim() == "broken") {
            Console.Error.WriteLine("Fixture assembly startup failure");
            return 7;
        }
        Console.WriteLine("17.14.0.0");
        return 0;
    }
}
'@
    $boundedResult = Invoke-BoundedConfigureProbe -Executable $fakeMsbuild -Arguments '-nologo -version'
    Assert-True (-not $boundedResult.TimedOut -and $boundedResult.ExitCode -eq 0 -and $boundedResult.Output -match '17.14.0.0') `
        'The bounded configure runner lost a successful process exit or output.'
    $boundedResult = Invoke-BoundedConfigureProbe -Executable $fakeMsbuild -Arguments '--sleep' -TimeoutMilliseconds 100
    Assert-True $boundedResult.TimedOut 'The bounded configure runner did not terminate its sleeping fixture.'
    $fakeVsWhere = Join-Path $testDir 'vswhere.cmd'
    $fakeInstancesFile = Join-Path $testDir 'instances.json'
    $staleInstance = @{ installationPath = (Join-Path $testDir 'missing-unrelated-buildtools'); productId = 'Microsoft.VisualStudio.Product.BuildTools'; installationVersion = '17.14.37710.0' }
    $validInstance = @{ installationPath = $fakeVisualStudio; productId = 'Microsoft.VisualStudio.Product.Community'; installationVersion = '17.12.12345.0'; isComplete = $false; isPrerelease = $true }
    ConvertTo-Json -InputObject @($staleInstance, $validInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
    Set-Content -LiteralPath $fakeVsWhere -Encoding Ascii -Value @(
        '@echo off',
        'echo %* | findstr /L /C:"-products *" >nul || exit /b 7',
        'echo %* | findstr /C:"-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64" >nul || exit /b 8',
        'echo %* | findstr /C:"-latest" >nul && exit /b 9',
        'echo %* | findstr /C:"-all" >nul || exit /b 10',
        'echo %* | findstr /C:"-prerelease" >nul || exit /b 11',
        ('type "{0}"' -f $fakeInstancesFile)
    )
    Assert-True ((Get-WindowsSdkVersion -Roots @($fakeSdkRoot)) -eq [version]$fakeSdkVersion) `
        'The Windows SDK probe rejected a complete supported fixture.'
    Assert-True ((Get-VisualStudio2022Path -VsWherePath $fakeVsWhere) -eq $fakeVisualStudio) `
        'The Visual Studio probe rejected a complete VS 2022 Community fixture.'
    Assert-True ((Get-VisualStudio2022Product -VsWherePath $fakeVsWhere) -eq 'Community') `
        'The Visual Studio product probe did not preserve the installed Community SKU.'
    Assert-True (Test-VisualCppBuildTools -VsWherePath $fakeVsWhere -WindowsSdkRoots @($fakeSdkRoot)) `
        'The combined Visual Studio C++ and Windows SDK fixture probe failed.'
    Set-Content -LiteralPath (Join-Path $fakeMsbuildDirectory 'probe-state.txt') -Encoding Ascii -Value 'broken'
    $brokenProbe = Test-VisualStudioMSBuild -Path $fakeMsbuild
    Assert-True (-not $brokenProbe.Succeeded -and $brokenProbe.Reason -match 'exited 7') `
        'The MSBuild probe accepted a process that failed during managed startup.'
    $warningRecords = @()
    Assert-True ($null -eq (Get-VisualStudioInstance -VsWherePath $fakeVsWhere -WarningVariable warningRecords)) `
        'A registration with compiler files but broken MSBuild must not be selected.'
    Assert-True (($warningRecords -join ' ') -match 'Fixture assembly startup failure') `
        'The rejected MSBuild instance must retain its startup diagnosis.'
    $alternateVisualStudio = Join-Path $testDir 'VS2022-Alternate'
    Copy-Item -LiteralPath $fakeVisualStudio -Destination $alternateVisualStudio -Recurse
    Set-Content -LiteralPath (Join-Path $alternateVisualStudio 'MSBuild\Current\Bin\amd64\probe-state.txt') -Encoding Ascii -Value 'working'
    $alternateInstance = @{ installationPath = $alternateVisualStudio; productId = 'Microsoft.VisualStudio.Product.BuildTools'; installationVersion = '17.12.12344.0'; isComplete = $true; isPrerelease = $false }
    ConvertTo-Json -InputObject @($validInstance, $alternateInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
    Assert-True ((Get-VisualStudioInstance -VsWherePath $fakeVsWhere).installationPath -eq $alternateVisualStudio) `
        'A broken MSBuild candidate must not hide a later usable instance.'
    Set-Content -LiteralPath (Join-Path $fakeMsbuildDirectory 'probe-state.txt') -Encoding Ascii -Value 'working'
    $validInstance.installationVersion = '18.0.12345.0'
    ConvertTo-Json -InputObject @($staleInstance, $validInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
    $selected = Get-VisualStudioInstance -VsWherePath $fakeVsWhere
    Assert-True ($selected.installationPath -eq $fakeVisualStudio -and $selected.installationVersion -eq '18.0.12345.0' -and
        $selected.isPrerelease -and -not $selected.isComplete) `
        'The generic probe must preserve a usable VS 2026 preview path, version, and incomplete-registration receipt together.'
    $validInstance.installationVersion = '17.12.12345.0'

    ConvertTo-Json -InputObject @($staleInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
    Assert-True ($null -eq (Get-VisualStudio2022Path -VsWherePath $fakeVsWhere)) `
        'A missing registered instance must not count as an installed compiler.'
    Assert-True ($null -eq (Get-VisualStudio2022Product -VsWherePath $fakeVsWhere)) `
        'A stale instance must not supply a product identity.'
    ConvertTo-Json -InputObject @($validInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
    Set-Content -LiteralPath (Join-Path $fakeVisualStudio 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Encoding Ascii -Value '14.00.00000'
    Assert-True ($null -eq (Get-VisualStudio2022Path -VsWherePath $fakeVsWhere)) `
        'An instance whose default compiler is absent must not count as usable.'

    # Execute the production CMD selection block, with disposable registrations
    # and files, without entering compilation or installing any tools.
    $batchText = (Get-Content -LiteralPath (Join-Path $RepositoryRoot 'build_win.bat') -Raw).Replace("`r`n", "`n")
    $start = $batchText.IndexOf('IF DEFINED BAMBU_VS_INSTALLATION_PATH (')
    $end = $batchText.IndexOf("`n:CHECK_MSVC_PATH`n")
    Assert-True ($start -ge 0 -and $end -gt $start) 'The explicit CMD instance selection block is missing.'
    $batchFixture = Join-Path $testDir 'select-instance.cmd'
    $block = $batchText.Substring($start, $end - $start)
    Set-Content -LiteralPath $batchFixture -Encoding Ascii -Value @(
        '@echo off', 'setlocal disableDelayedExpansion', $block,
        ':CHECK_MSVC_PATH', 'echo %MSVC_DIR%', 'exit /b 0', ':HELP', 'exit /b 1')
    $environmentNames = @('BAMBU_VS_INSTALLATION_PATH', 'BAMBU_VS_INSTALLATION_VERSION', 'VSWHERE', 'PS_VERSION', 'PS_VERSION_EXCEEDED', 'PS_PRODUCT')
    $oldEnvironment = @{}
    foreach ($name in $environmentNames) { $oldEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    try {
        $env:VSWHERE = $fakeVsWhere
        $env:PS_PRODUCT = 'Community'
        foreach ($major in @(17, 18)) {
            $validInstance.installationVersion = "$major.0.12345.0"
            ConvertTo-Json -InputObject @($validInstance) | Set-Content -LiteralPath $fakeInstancesFile -Encoding Ascii
            # This fixture accepts the product filter used by the CMD contract.
            Set-Content -LiteralPath $fakeVsWhere -Encoding Ascii -Value @(
                '@echo off',
                'echo %* | findstr /C:"-all" >nul || exit /b 10',
                'echo %* | findstr /C:"-prerelease" >nul || exit /b 11',
                ('type "{0}"' -f $fakeInstancesFile))
            $env:PS_VERSION = [string]$major
            $env:PS_VERSION_EXCEEDED = [string]($major + 1)
            $env:BAMBU_VS_INSTALLATION_PATH = $fakeVisualStudio
            $env:BAMBU_VS_INSTALLATION_VERSION = $validInstance.installationVersion
            $selectionOutput = @(& cmd.exe /d /c ('call "' + $batchFixture + '" 2>&1'))
            Assert-True ($LASTEXITCODE -eq 0 -and $selectionOutput -contains $fakeVisualStudio) `
                "CMD rejected the exact VS $major fixture path."
        }
        foreach ($invalid in @((Join-Path $testDir 'absent'), 'relative-path', ($fakeVisualStudio + '" & echo INJECTION_EXECUTED & rem "'), ($fakeVisualStudio + "`ninvalid"))) {
            $env:BAMBU_VS_INSTALLATION_PATH = $invalid
            $selectionOutput = @(& cmd.exe /d /c ('call "' + $batchFixture + '" 2>&1'))
            Assert-True ($LASTEXITCODE -ne 0 -and -not ($selectionOutput -match '^INJECTION_EXECUTED$')) `
                'CMD accepted an invalid override or expanded it before validation.'
        }
        $env:BAMBU_VS_INSTALLATION_PATH = $fakeVisualStudio
        foreach ($invalidVersion in @('18.0.99999.0', '18.0', '18.0.12345.0" & echo INJECTION_EXECUTED')) {
            $env:BAMBU_VS_INSTALLATION_VERSION = $invalidVersion
            $selectionOutput = @(& cmd.exe /d /c ('call "' + $batchFixture + '" 2>&1'))
            Assert-True ($LASTEXITCODE -ne 0 -and -not ($selectionOutput -match '^INJECTION_EXECUTED$')) `
                'CMD accepted a mismatched, incomplete, or unsafe instance version.'
        }
        $env:PS_VERSION = '19'
        $env:BAMBU_VS_INSTALLATION_PATH = $fakeVisualStudio
        $null = & cmd.exe /d /c ('call "' + $batchFixture + '" 2>&1')
        Assert-True ($LASTEXITCODE -ne 0) 'CMD accepted an unsupported Visual Studio major override.'
    } finally {
        foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $oldEnvironment[$name], 'Process') }
    }

    # Intercept the installer boundary. No download, installation, registration
    # mutation, or elevation takes place in this regression fixture.
    & {
        $script:probeCount = 0
        $script:capturedInstallerArguments = @()
        $script:capturedInstallerUrl = ''
        function Test-VisualCppBuildTools { $script:probeCount++; return ($script:probeCount -gt 1) }
        function Get-Winget { throw 'Visual Studio bootstrap must not choose an implicit winget instance.' }
        function Write-BuildLog { param($Message) }
        function Invoke-SilentInstaller {
            param($Url, $FileName, $Arguments, $WorkDir, $TrustedPublishers)
            $script:capturedInstallerArguments = $Arguments
            $script:capturedInstallerUrl = $Url
        }
        Install-VisualCppBuildTools -WorkDir $testDir
        $arguments = $script:capturedInstallerArguments
        $pathIndex = [array]::IndexOf($arguments, '--installPath')
        Assert-True ($pathIndex -ge 0 -and $pathIndex + 1 -lt $arguments.Count) `
            'The Visual Studio installer must receive an explicit installation path.'
        $expectedPath = Join-Path $env:LOCALAPPDATA 'BambuBuildTools\VS2026'
        Assert-True ($arguments[$pathIndex + 1] -ceq ('"' + $expectedPath + '"')) `
            'Visual Studio must use the quoted Bambu-owned path, preserving unrelated registrations.'
        $applicationRoot = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'BambuStudioMD3')).TrimEnd('\') + '\'
        $actualInstallPath = [IO.Path]::GetFullPath($arguments[$pathIndex + 1].Trim('"'))
        Assert-True (-not ($actualInstallPath.StartsWith($applicationRoot, [StringComparison]::OrdinalIgnoreCase))) `
            'The compiler installation must remain outside the directory Squirrel replaces.'
        $pdfBuilder = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'scripts/windows/Build-LocalPdfTools.ps1') -Raw
        Assert-True ($pdfBuilder.Contains('BambuBuildTools/VS2026')) `
            'The PDF build default must use the same installer-independent compiler location.'
        Assert-True ($arguments -contains '--norestart' -and $arguments -notcontains '--force') `
            'Visual Studio bootstrap must not restart the host or force-close applications.'
        Assert-True ($arguments -contains 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64') `
            'Visual Studio bootstrap must explicitly request the compiler, not only workload-required components.'
        Assert-True ($script:capturedInstallerUrl -ceq 'https://aka.ms/vs/stable/vs_buildtools.exe') `
            'The fallback must use Microsoft Stable, distinct from stale VS 2022 Release registrations.'
        $nicknameIndex = [array]::IndexOf($arguments, '--nickname')
        Assert-True ($nicknameIndex -ge 0 -and $arguments[$nicknameIndex + 1].Length -le 10) `
            'The Visual Studio nickname must fit the documented ten-character limit.'
    }

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
    if ($ProbeHostCMake) {
        $instance = Get-VisualStudioInstance
        Assert-True ($null -ne $instance) 'The requested host CMake probe requires a usable compiler instance.'
        $major = ([version]$instance.installationVersion).Major
        $generator = if ($major -eq 18) { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
        $minimumCMake = if ($major -eq 18) { '4.2' } else { '3.21' }
        $generatorInstance = if ($major -eq 18) { "$($instance.installationPath),version=$($instance.installationVersion)" } else { [string]$instance.installationPath }
        $cmake = Join-Path $instance.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        Assert-True (Test-Path -LiteralPath $cmake -PathType Leaf) 'The host probe requires the selected instance bundled CMake.'
        $probeSource = Join-Path $testDir 'cmake-probe'
        New-Item -ItemType Directory -Path $probeSource | Out-Null
        Set-Content -LiteralPath (Join-Path $probeSource 'CMakeLists.txt') -Encoding Ascii -Value @(
            "cmake_minimum_required(VERSION $minimumCMake)", 'project(InstanceSelectionProbe NONE)')
        Write-Host "Host CMake probe: generator='$generator'; path='$($instance.installationPath)'; version=$($instance.installationVersion); prerelease=$($instance.isPrerelease); registrationComplete=$($instance.isComplete)."
        $arguments = '-S "{0}" -B "{1}" -G "{2}" -A x64 "-DCMAKE_GENERATOR_INSTANCE={3}"' -f `
            $probeSource, (Join-Path $probeSource 'build'), $generator, $generatorInstance
        $result = Invoke-BoundedConfigureProbe -Executable $cmake -Arguments $arguments
        Write-Host $result.Output
        Write-Host $result.ErrorOutput
        Assert-True (-not $result.TimedOut) 'The host CMake configure probe exceeded its 60-second deadline.'
        Assert-True ($result.ExitCode -eq 0) 'The bounded host CMake configure probe rejected the selected instance.'
        Write-Host 'Host CMake configure-only probe passed; no application compilation or installer ran.'
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
