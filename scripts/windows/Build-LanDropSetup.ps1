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
        $builder = Join-Path $project 'node_modules/electron-builder/out/cli/cli.js'
        $builderArguments = @($builder, '--win', '--x64', '--publish', 'never')
        if ($BuildOnly) { $builderArguments += '--dir' }
        else { $builderArguments += @('--config.win.target', 'portable') }
        & node.exe @builderArguments
        if ($LASTEXITCODE -ne 0) { throw 'Companion packaging failed.' }
        $output = Join-Path $project 'dist'
        $files = @(Get-ChildItem -LiteralPath $output -Filter '*.exe' -File)
        if (-not $BuildOnly -and $files.Count -ne 1) { throw 'Expected exactly one portable executable.' }
        foreach ($file in $files) {
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            [IO.File]::WriteAllText(($file.FullName + '.sha256'), ($hash + '  ' + $file.Name + "`n"), [Text.UTF8Encoding]::new($false))
        }
        Write-Host ('Companion build completed: ' + $output)
    } finally { Pop-Location }
    exit 0
} catch {
    Write-Error ('LAN setup companion build incomplete: ' + $_.Exception.Message)
    exit 1
}
