$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root 'lan-model-drop/wizard-policy.ps1')
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('bambu-compose-fixture-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $fixture
$saved = @{}
foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'COMPOSE_*' -or $_.Name -like 'DROP_*' })) { $saved[$item.Name] = $item.Value }
try {
    $empty = Join-Path $fixture 'empty.env'
    [IO.File]::WriteAllText($empty, '')
    [IO.File]::WriteAllText((Join-Path $fixture '.env'), "COMPOSE_PROJECT_NAME=unrelated`nDROP_STATION_NAME=fixture-dotenv-value`n")
    [IO.File]::WriteAllText((Join-Path $fixture 'compose.yaml'), @'
name: ignored-name
services:
  sample:
    image: busybox:1.37
    environment:
      DROP_STATION_NAME: ${DROP_STATION_NAME:-}
'@)
    $env:COMPOSE_PROJECT_NAME = 'unrelated'
    $env:COMPOSE_ENV_FILES = Join-Path $fixture '.env'
    $env:COMPOSE_REMOVE_ORPHANS = 'true'
    $env:COMPOSE_FILE = 'unrelated.yaml'
    $env:DROP_STATION_NAME = 'fixture-inherited-value'
    $arguments = @(Initialize-WizardCompose -Directory $fixture -EmptyEnvironmentFile $empty)
    if ($arguments[1] -ne '-p' -or $arguments[2] -ne 'lan-model-drop' -or $arguments[3] -ne '--env-file' -or $arguments[4] -ne $empty) { throw 'project_or_environment_not_fixed' }
    if (@(Get-ChildItem Env: | Where-Object { $_.Name -like 'COMPOSE_*' -or $_.Name -like 'DROP_*' }).Count -ne 0) { throw 'inherited_environment_retained' }
    $docker = Get-Command docker.exe -ErrorAction SilentlyContinue
    if (-not $docker) { throw 'Docker Compose CLI unavailable for neutral configuration fixture.' }
    $raw = & $docker.Source @arguments config --format json
    if ($LASTEXITCODE -ne 0) { throw 'neutral_compose_config_failed' }
    $config = ($raw -join "`n") | ConvertFrom-Json
    if ($config.name -ne 'lan-model-drop' -or $config.services.sample.environment.DROP_STATION_NAME) { throw 'dotenv_or_project_contamination' }
    Write-Output 'PASS: production Compose policy rejects inherited project, env files, orphan removal and dotenv settings.'
} finally {
    foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'COMPOSE_*' -or $_.Name -like 'DROP_*' })) { [Environment]::SetEnvironmentVariable($item.Name, $null, 'Process') }
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
    # Delete only the individually named files created by this fixture.
    foreach ($name in @('empty.env', '.env', 'compose.yaml')) {
        $file = Join-Path $fixture $name
        if ([IO.File]::Exists($file)) { [IO.File]::Delete($file) }
    }
    [IO.Directory]::Delete($fixture, $false)
}
