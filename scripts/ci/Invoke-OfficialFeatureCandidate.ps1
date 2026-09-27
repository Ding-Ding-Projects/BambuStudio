<#
.SYNOPSIS
Build and package an unsigned official-source feature candidate on GitHub-hosted Windows.
.DESCRIPTION
Uses the checked-out commit and its own dependency tree. The output is a
diagnostic Squirrel.Windows package, not a published release or install proof.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'The candidate build requires a disposable GitHub-hosted Windows runner.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$diagnostic = Join-Path $root 'diagnostic\candidate'
New-Item -ItemType Directory -Force -Path $diagnostic | Out-Null
$metadataPath = Join-Path $diagnostic 'candidate.json'
$metadata = [ordered]@{
    schema = 1
    status = 'started'
    run_id = $env:GITHUB_RUN_ID
    run_attempt = $env:GITHUB_RUN_ATTEMPT
    source_sha = $env:GITHUB_SHA
    dependency_tree = $null
    generator = 'Visual Studio 18 2026'
    configuration = 'Release'
    visual_studio_version = $null
    cmake_version = $null
    cmake_sha256 = $null
    sdk_version = $null
    dependency_cache = $null
    native_exe_sha256 = $null
    setup_sha256 = $null
    sbom_sha256 = $null
    symbol_count = 0
    symbol_collection_failure_type = $null
    package_release_number = $null
    package_status = 'not_started'
    failure_type = $null
    validation = 'build and package only; no install, GUI, or release publication'
}

function Assert-Tool {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) { throw "Required hosted tool is missing: $Name" }
}

function Invoke-Step {
    param([string]$Name, [scriptblock]$Action)
    Write-Host "== $Name =="
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

function Save-NativeSymbols {
    $sourceRoot = Join-Path $root 'build\src'
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) { return 0 }
    $files = @(Get-ChildItem -LiteralPath $sourceRoot -Filter '*.pdb' -File -Recurse -ErrorAction SilentlyContinue)
    if (-not $files.Count) { return 0 }
    $symbolRoot = Join-Path $diagnostic 'symbols'
    New-Item -ItemType Directory -Force -Path $symbolRoot | Out-Null
    $rows = @()
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($sourceRoot.Length + 1)
        $target = Join-Path $symbolRoot $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        $rows += [ordered]@{
            path = $relative.Replace('\', '/')
            bytes = $file.Length
            sha256 = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    [ordered]@{
        schema = 1
        source_sha = $env:GITHUB_SHA
        native_exe_sha256 = $metadata.native_exe_sha256
        symbols = $rows
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $symbolRoot 'candidate-symbols.json') -Encoding utf8
    return $files.Count
}

try {
    Set-Location $root
    foreach ($tool in @('git', 'cmake.exe', 'msbuild', 'choco')) { Assert-Tool $tool }
    $head = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $env:GITHUB_SHA) { throw 'Checked-out candidate commit differs from the hosted run SHA.' }
    $metadata.dependency_tree = (& git rev-parse 'HEAD:deps').Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the candidate dependency tree.' }

    $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
    $cmakeDir = Split-Path -Parent $cmake
    $cmakeVersionText = (& $cmake --version | Select-Object -First 1)
    if ($cmakeVersionText -notmatch '^cmake version (\d+\.\d+\.\d+)') { throw 'Cannot identify CMake version.' }
    $cmakeVersion = [version]$Matches[1]
    if ($cmakeVersion -lt [version]'4.2.0') { throw 'Visual Studio 2026 requires CMake 4.2 or newer.' }
    $metadata.cmake_version = $cmakeVersion.ToString()
    $metadata.cmake_sha256 = (Get-FileHash -LiteralPath $cmake -Algorithm SHA256).Hash.ToLowerInvariant()

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Visual Studio locator is missing.' }
    $vsArgs = @('-latest', '-version', '[18.0,19.0)', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
    $vs = [string](& $vswhere @vsArgs -property installationPath)
    if ($LASTEXITCODE -ne 0 -or -not $vs.Trim()) { throw 'Visual Studio 2026 C++ toolset is unavailable.' }
    $vsVersion = [string](& $vswhere @vsArgs -property installationVersion)
    if ($LASTEXITCODE -ne 0 -or -not $vsVersion.Trim()) { throw 'Visual Studio 2026 version is unavailable.' }
    $metadata.visual_studio_version = $vsVersion.Trim()
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
    $sdk = Get-ChildItem -LiteralPath $sdkRoot -Directory | Where-Object {
        Test-Path -LiteralPath (Join-Path $_.FullName 'winrt\windows.graphics.printing3d.h')
    } | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $sdk) { throw 'A compatible Windows SDK include tree is unavailable.' }
    $metadata.sdk_version = $sdk.Name

    if (-not (Get-Command pkg-config.exe -ErrorAction SilentlyContinue)) {
        Invoke-Step 'Install pkgconfiglite 0.28.0' { choco install pkgconfiglite --version=0.28.0 --yes --no-progress }
        $env:Path = "{0};{1};{2}" -f [Environment]::GetEnvironmentVariable('Path', 'Machine'),
            [Environment]::GetEnvironmentVariable('Path', 'User'), $env:Path
    }
    Assert-Tool pkg-config.exe
    $perl = 'C:\Strawberry\perl\bin\perl.exe'
    if (-not (Test-Path -LiteralPath $perl -PathType Leaf)) {
        Invoke-Step 'Install Strawberry Perl' { choco install strawberryperl --yes --no-progress }
    }
    if (-not (Test-Path -LiteralPath $perl -PathType Leaf)) { throw 'Strawberry Perl is unavailable after bootstrap.' }
    $env:Path = "$cmakeDir;C:\Strawberry\c\bin;C:\Strawberry\perl\site\bin;C:\Strawberry\perl\bin;$env:Path"
    if ((Get-Command cmake.exe -ErrorAction Stop).Source -cne $cmake) { throw 'Bootstrap changed the verified CMake executable.' }
    $env:LANG = 'C'; $env:LC_ALL = 'C'; $env:LC_CTYPE = 'C'
    Invoke-Step 'Verify Strawberry Perl module' { & $perl -MLocale::Maketext::Simple -e 1 }

    $depDest = Join-Path $root 'deps\build\BambuStudio_dep'
    $prefix = Join-Path $depDest 'usr\local'
    $marker = Join-Path $depDest 'official-feature-cache.json'
    $cacheValid = $false
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        $cache = Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
        $cacheValid = $cache.dependency_tree -ceq $metadata.dependency_tree -and
            $cache.generator -ceq $metadata.generator -and
            $cache.cmake_version -ceq $metadata.cmake_version -and
            (Test-Path -LiteralPath (Join-Path $prefix 'include') -PathType Container)
    }
    $metadata.dependency_cache = if ($cacheValid) { 'verified_hit' } else { 'build_from_candidate_source' }
    if (-not $cacheValid) {
        Invoke-Step 'Configure candidate dependencies' {
            & $cmake -S deps -B deps/build -G 'Visual Studio 18 2026' -A x64 `
                "-DDESTDIR=$depDest" -DDEP_DEBUG=OFF
        }
        Invoke-Step 'Build candidate dependencies' {
            & $cmake --build deps/build --target ALL_BUILD --config Release --parallel 4
        }
        if (-not (Test-Path -LiteralPath (Join-Path $prefix 'include') -PathType Container)) {
            throw 'Candidate dependency build did not produce the expected prefix.'
        }
        [ordered]@{ dependency_tree = $metadata.dependency_tree; generator = $metadata.generator;
            cmake_version = $metadata.cmake_version } | ConvertTo-Json | Set-Content -LiteralPath $marker
    }

    $payload = Join-Path $root 'install-dir'
    Invoke-Step 'Configure candidate native application' {
        & $cmake -S . -B build -G 'Visual Studio 18 2026' -A x64 `
            -DBBL_RELEASE_TO_PUBLIC=1 -DBBL_INTERNAL_TESTING=0 `
            -DSLIC3R_MSVC_PDB=ON -DSLIC3R_BUILD_TESTS=OFF `
            "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_INSTALL_PREFIX=$payload" `
            "-DWIN10SDK_PATH=$($sdk.FullName)"
    }
    Invoke-Step 'Build candidate device page' { & $cmake --build build --target device_page_build --config Release --parallel 4 }
    Invoke-Step 'Build and install candidate native application' { & $cmake --build build --target install --config Release --parallel 4 }
    $exe = @(Get-ChildItem -LiteralPath $payload -Filter 'bambu-studio.exe' -File -Recurse)
    if ($exe.Count -ne 1) { throw 'The native payload must contain exactly one bambu-studio.exe.' }
    $metadata.native_exe_sha256 = (Get-FileHash -LiteralPath $exe[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $metadata.symbol_count = Save-NativeSymbols
    if ($metadata.symbol_count -lt 1) { throw 'Release build produced no native PDB symbols.' }

    & (Join-Path $root 'scripts\windows\Stage-ModelCreatorRenderers.ps1') `
        -PayloadDirectory $payload -CacheDirectory (Join-Path $env:RUNNER_TEMP 'bambu-model-renderers')
    if ($LASTEXITCODE -ne 0) { throw 'Model Creator renderer staging failed.' }
    $sevenZip = 'C:\Program Files\7-Zip\7z.exe'
    if (-not (Test-Path -LiteralPath $sevenZip -PathType Leaf)) {
        Invoke-Step 'Install 7-Zip' { choco install 7zip --yes --no-progress }
    }
    if (-not (Test-Path -LiteralPath $sevenZip -PathType Leaf)) { throw '7-Zip is unavailable after bootstrap.' }
    $mesaVersion = '26.1.3'
    $mesaArchive = Join-Path $env:RUNNER_TEMP "mesa3d-$mesaVersion-release-msvc.7z"
    $mesaUrl = "https://github.com/pal1000/mesa-dist-win/releases/download/$mesaVersion/mesa3d-$mesaVersion-release-msvc.7z"
    Invoke-WebRequest -Uri $mesaUrl -OutFile $mesaArchive -MaximumRetryCount 3 -RetryIntervalSec 15
    if ((Get-FileHash -LiteralPath $mesaArchive -Algorithm SHA256).Hash.ToLowerInvariant() -cne
        '6dd431f4620cea73970b13e3ffa94f721f2a3924306b8a4283c97648cdb6eb9c') {
        throw 'Pinned Mesa archive hash mismatch.'
    }
    $mesaExtract = Join-Path $env:RUNNER_TEMP 'candidate-mesa-x64'
    Invoke-Step 'Extract pinned Mesa fallback' {
        & $sevenZip x $mesaArchive "-o$mesaExtract" 'x64\opengl32.dll' 'x64\libgallium_wgl.dll' -y
    }
    $mesaPayload = Join-Path $payload 'mesa'
    New-Item -ItemType Directory -Force -Path $mesaPayload | Out-Null
    $mesaHashes = @{
        'opengl32.dll' = '12499866437a161d2b250d5105188ae00732dd74b4bebbcdf972e6145af00f9e'
        'libgallium_wgl.dll' = '1895f8c19ede5efd0497f9dfab463b19bf4377e3af7c06c2d4d073e4680c5f69'
    }
    foreach ($name in $mesaHashes.Keys) {
        $source = Join-Path (Join-Path $mesaExtract 'x64') $name
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
            (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $mesaHashes[$name]) {
            throw "Pinned Mesa file is missing or has a different hash: $name"
        }
        Copy-Item -LiteralPath $source -Destination (Join-Path $mesaPayload $name)
    }
    $icon = Join-Path $root 'resources\images\BambuStudio.ico'
    if (-not (Test-Path -LiteralPath $icon -PathType Leaf)) { throw 'The packaged application icon is missing.' }
    $versionContent = Get-Content -LiteralPath (Join-Path $root 'version.inc') -Raw
    if ($versionContent -notmatch 'set\(SLIC3R_VERSION "([^"]+)"\)') { throw 'Product version is missing from version.inc.' }
    $productVersion = $Matches[1]
    $sbom = Join-Path $diagnostic 'BambuStudioMD3.cdx.json'
    & (Join-Path $root 'scripts\ci\New-WindowsCycloneDxSbom.ps1') `
        -PayloadDir $payload -OutputPath $sbom -Version $productVersion `
        -Commit $head -Repository $env:GITHUB_REPOSITORY
    if (-not (Test-Path -LiteralPath $sbom -PathType Leaf)) { throw 'Candidate payload SBOM is missing.' }
    $metadata.sbom_sha256 = (Get-FileHash -LiteralPath $sbom -Algorithm SHA256).Hash.ToLowerInvariant()
    $packageOutput = Join-Path $root 'artifacts\windows'
    $runNumber = [long]$env:GITHUB_RUN_NUMBER
    $runAttempt = [long]$env:GITHUB_RUN_ATTEMPT
    if ($runNumber -lt 1 -or $runAttempt -lt 1 -or $runAttempt -gt 99 -or
        $runNumber -gt 10000000) { throw 'Hosted run numbering exceeds the diagnostic package-version range.' }
    $releaseNumber = [int](1000000 + $runNumber * 100 + $runAttempt)
    $metadata.package_release_number = $releaseNumber
    $metadata.package_status = 'running'
    Invoke-Step 'Build unsigned Squirrel candidate' {
        & (Join-Path $root 'scripts\windows\Invoke-SquirrelPackage.ps1') `
            -PayloadDirectory $payload -OutputDirectory $packageOutput `
            -ProductVersion $productVersion -SourceCommit $head `
            -Repository 'https://github.com/Ding-Ding-Projects/BambuStudio.git' `
            -ReleaseNumber $releaseNumber -IconPath $icon
    }
    $squirrelDir = Join-Path $packageOutput 'squirrel'
    $setup = Join-Path $squirrelDir 'Setup.exe'
    $full = @(Get-ChildItem -LiteralPath $squirrelDir -Filter '*-full.nupkg' -File)
    if (-not (Test-Path -LiteralPath $setup -PathType Leaf) -or $full.Count -ne 1 -or
        -not (Test-Path -LiteralPath (Join-Path $squirrelDir 'RELEASES') -PathType Leaf)) {
        throw 'The unsigned Squirrel candidate is incomplete.'
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $setup
    if ($signature.Status -ne 'NotSigned') { throw 'The Squirrel candidate must remain unsigned.' }
    $metadata.setup_sha256 = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
    $metadata.package_status = 'unsigned_package_created'
    $metadata.status = 'built_and_packaged_unverified_runtime'
}
catch {
    $metadata.status = 'failed'
    $metadata.failure_type = $_.Exception.GetType().Name
    throw
}
finally {
    if ($metadata.symbol_count -lt 1) {
        try { $metadata.symbol_count = Save-NativeSymbols }
        catch { $metadata.symbol_collection_failure_type = $_.Exception.GetType().Name }
    }
    $metadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metadataPath -Encoding utf8
}
