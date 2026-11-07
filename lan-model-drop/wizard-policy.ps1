function Initialize-WizardCompose([string] $Directory, [string] $EmptyEnvironmentFile) {
    if (-not [IO.File]::Exists($EmptyEnvironmentFile) -or (Get-Item -LiteralPath $EmptyEnvironmentFile).Length -ne 0) { throw 'environment_file' }
    # Process-local environment only. Never modify persistent user/machine settings.
    foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'COMPOSE_*' -or $_.Name -like 'DROP_*' })) {
        [Environment]::SetEnvironmentVariable($item.Name, $null, 'Process')
    }
    return @('compose', '-p', 'lan-model-drop', '--env-file', $EmptyEnvironmentFile,
        '--project-directory', $Directory, '-f', (Join-Path $Directory 'compose.yaml'))
}
