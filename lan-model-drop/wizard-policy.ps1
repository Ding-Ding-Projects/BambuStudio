function Initialize-WizardCompose([string] $Directory, [string] $EmptyEnvironmentFile) {
    if (-not [IO.File]::Exists($EmptyEnvironmentFile) -or (Get-Item -LiteralPath $EmptyEnvironmentFile).Length -ne 0) { throw 'environment_file' }
    # Process-local environment only. Never modify persistent user/machine settings.
    foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'COMPOSE_*' -or $_.Name -like 'DROP_*' })) {
        [Environment]::SetEnvironmentVariable($item.Name, $null, 'Process')
    }
    return @('compose', '-p', 'lan-model-drop', '--env-file', $EmptyEnvironmentFile,
        '--project-directory', $Directory, '-f', (Join-Path $Directory 'compose.yaml'))
}

function Get-WizardCurrentService([string] $ExpectedId, [string] $ExpectedStorage) {
    $ids = @((Invoke-Docker @('ps', '-aq', '--filter', 'label=com.docker.compose.project=lan-model-drop', '--filter', 'label=com.docker.compose.service=lan-model-drop')).Trim() -split '\s+' | Where-Object { $_ })
    if ($ids.Count -ne 1 -or -not $ExpectedId.StartsWith($ids[0]) -or $ids[0] -notmatch '^[a-f0-9]{12,64}$') { throw 'service_changed' }
    $inspection = ConvertFrom-Json -InputObject (Invoke-Docker @('inspect', $ExpectedId))
    if ($inspection -isnot [array] -or $inspection.Count -ne 1) { throw 'service_changed' }
    $current = $inspection[0]
    if ($current.Id -ne $ExpectedId -or $current.Config.Labels.'com.docker.compose.project' -ne 'lan-model-drop' -or $current.Config.Labels.'com.docker.compose.service' -ne 'lan-model-drop') { throw 'service_changed' }
    $mounts = @($current.Mounts | Where-Object { $_.Destination -eq '/data' })
    if ($mounts.Count -ne 1 -or ($mounts[0].Type + ':' + $mounts[0].Source) -cne $ExpectedStorage) { throw 'service_changed' }
    return @{ running = [bool]$current.State.Running; imageId = [string]$current.Image; configuredImage = [string]$current.Config.Image }
}

function Assert-WizardImage([string] $ImageId, [string] $ConfiguredImage, [string] $ExpectedImage) {
    $match = [regex]::Match($ExpectedImage, '^([^@]+)@(sha256:[a-f0-9]{64})$')
    if ($ImageId -notmatch '^sha256:[a-f0-9]{64}$' -or -not $match.Success) { throw 'service_upgrade_required' }
    $repository = $match.Groups[1].Value -replace ':[^/:]+$', ''
    $expectedDigest = $repository + '@' + $match.Groups[2].Value
    if (-not ($ConfiguredImage.StartsWith($repository + ':') -or $ConfiguredImage.StartsWith($repository + '@'))) { throw 'service_upgrade_required' }
    $digests = ConvertFrom-Json -InputObject (Invoke-Docker @('image', 'inspect', $ImageId, '--format', '{{json .RepoDigests}}'))
    if ($digests -isnot [array] -or $digests -cnotcontains $expectedDigest) { throw 'service_upgrade_required' }
}
