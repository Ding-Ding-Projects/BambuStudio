param([switch] $BuildOnly, [switch] $Plan)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project = Join-Path $root 'companions/lan-drop-setup'
if ($Plan) {
    Write-Host ('LAN setup companion: pinned Node bootstrap, npm integrity-checked acquisition, {0}.' -f $(if ($BuildOnly) { 'unpacked build' } else { 'single portable executable' }))
    exit 0
}
try {
    $source = (& git -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $source -notmatch '^[0-9a-f]{40}$') { throw 'Source identity is unavailable.' }
    $sourceStatus = @(& git -C $root status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0 -or $sourceStatus.Count -ne 0) { throw 'Companion builds require a clean committed source and index.' }
    $indexTree = (& git -C $root write-tree).Trim()
    if ($LASTEXITCODE -ne 0 -or $indexTree -notmatch '^[0-9a-f]{40}$') { throw 'Index identity is unavailable.' }
    . (Join-Path $root 'packaging/windows/build-from-source/Toolchain.ps1')
    $cache = Join-Path $env:LOCALAPPDATA 'BambuStudioBuildTools/lan-drop-setup'
    $null = New-Item -ItemType Directory -Path $cache -Force
    Install-Node -WorkDir $cache
    Push-Location $project
    try {
        # The lockfile carries exact package versions and registry integrity hashes.
        # Runtime acquisition uses Electron's publisher checksum validation.
        & npm.cmd ci --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) { throw 'Companion dependency acquisition failed.' }
        $output = Join-Path $project 'dist'
        $null = New-Item -ItemType Directory -Path $output -Force
        $inputs = [ordered]@{}
        foreach ($relative in @('companions/lan-drop-setup/package-lock.json', 'companions/lan-drop-setup/package.json', 'lan-model-drop/compose.yaml', 'lan-model-drop/host.ps1', 'lan-model-drop/wizard-host.ps1', 'lan-model-drop/wizard-policy.ps1')) {
            $inputs[$relative] = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $package = Get-Content -LiteralPath (Join-Path $project 'package.json') -Raw | ConvertFrom-Json
        $manifest = [ordered]@{ schema = 1; sourceCommit = $source; indexTree = $indexTree; sourceClean = $true;
            version = $package.version; electron = $package.devDependencies.electron; builder = $package.devDependencies.'electron-builder'; inputs = $inputs }
        $manifestPath = Join-Path $output 'build-manifest.json'
        [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
        $builder = Join-Path $project 'node_modules/electron-builder/out/cli/cli.js'
        $builderArguments = @($builder, '--win', '--x64', '--publish', 'never')
        if ($BuildOnly) { $builderArguments += '--dir' }
        else { $builderArguments += @('--config.win.target', 'portable') }
        & node.exe @builderArguments
        if ($LASTEXITCODE -ne 0) { throw 'Companion packaging failed.' }
        if ((& git -C $root rev-parse HEAD).Trim() -ne $source -or @(& git -C $root status --porcelain --untracked-files=all).Count -ne 0) { throw 'Source changed during companion packaging.' }
        $files = @(if ($BuildOnly) { Get-ChildItem -LiteralPath (Join-Path $output 'win-unpacked') -Filter '*.exe' -File } else { Get-ChildItem -LiteralPath $output -Filter '*.exe' -File })
        if (-not $BuildOnly -and $files.Count -ne 1) { throw 'Expected exactly one portable executable.' }
        foreach ($file in $files) {
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            [IO.File]::WriteAllText(($file.FullName + '.sha256'), ($hash + '  ' + $file.Name + "`n"), [Text.UTF8Encoding]::new($false))
        }
        $proof = [ordered]@{ schema = 1; sourceCommit = $source; indexTree = $indexTree; sourceClean = $true;
            buildManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();
            outputs = @($files | ForEach-Object { @{ name = $_.Name; bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() } }) }
        [IO.File]::WriteAllText((Join-Path $output 'package-proof.json'), ($proof | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
        Write-Host ('Companion build completed: ' + $output)
    } finally { Pop-Location }
    exit 0
} catch {
    Write-Error ('LAN setup companion build incomplete: ' + $_.Exception.Message)
    exit 1
}
