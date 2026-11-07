# Fixed, bounded companion backend. Requests arrive over inherited private stdin.
# Only safe summaries are written to stdout; Docker diagnostics are never relayed.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'host.ps1') -Mode Library
. (Join-Path $PSScriptRoot 'wizard-policy.ps1')
$code = 'invalid_request'
$mutationAttempted = $false
$emptyEnvironmentFile = $null
try {
    $line = [Console]::In.ReadLine()
    if ($null -eq $line -or [Text.Encoding]::UTF8.GetByteCount($line) -gt 8192) { throw 'request' }
    $request = ConvertFrom-Json -InputObject $line
    if ($request.schema -ne 1 -or $request.method -notin @('inspect', 'pull', 'start', 'health')) { throw 'request' }
    $emptyEnvironmentFile = [IO.Path]::GetTempFileName()
    $script:Compose = Initialize-WizardCompose -Directory $PSScriptRoot -EmptyEnvironmentFile $emptyEnvironmentFile
    $imageLines = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'compose.yaml') | Where-Object { $_ -match '^\s+image:\s+\S+\s*$' })
    if ($imageLines.Count -ne 1) { throw 'image_configuration' }
    $expectedImage = ($imageLines[0] -replace '^\s+image:\s+', '').Trim()
    $command = Get-Command docker.exe -ErrorAction SilentlyContinue
    if (-not $command) {
        foreach ($candidate in @((Join-Path $env:ProgramFiles 'Docker/Docker/resources/bin/docker.exe'), (Join-Path $env:LOCALAPPDATA 'Programs/DockerDesktop/resources/bin/docker.exe'))) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $script:Docker = $candidate; break }
        }
    } else { $script:Docker = $command.Source }
    $code = 'docker_missing'
    if (-not $script:Docker) { throw 'docker' }
    $code = 'local_engine_required'
    if ($env:DOCKER_HOST) { throw 'override' }
    $endpoint = (Invoke-Docker @('context', 'inspect', '--format', '{{.Endpoints.docker.Host}}') 15).Trim()
    if ($endpoint -notin @('npipe:////./pipe/docker_engine', 'npipe:////./pipe/dockerDesktopLinuxEngine')) { throw 'remote_engine' }
    $code = 'engine_unavailable'
    $null = Invoke-Docker @('info', '--format', '{{.ServerVersion}}') 15
    $code = 'compose_unavailable'
    $null = Invoke-Docker @('compose', 'version', '--short') 15
    $code = 'inspection_failed'
    $ids = @((Invoke-Docker @('ps', '-aq', '--filter', 'label=com.docker.compose.project=lan-model-drop', '--filter', 'label=com.docker.compose.service=lan-model-drop')).Trim() -split '\s+' | Where-Object { $_ })
    if ($ids.Count -gt 1) { $code = 'service_conflict'; throw 'conflict' }
    $existing = $null
    if ($ids.Count -eq 1) {
        if ($ids[0] -notmatch '^[a-f0-9]{12,64}$') { throw 'identity' }
        $inspection = ConvertFrom-Json -InputObject (Invoke-Docker @('inspect', $ids[0]))
        if ($inspection -isnot [array] -or $inspection.Count -ne 1) { throw 'identity' }
        $existing = $inspection[0]
        if ($existing.Config.Labels.'com.docker.compose.project' -ne 'lan-model-drop' -or $existing.Config.Labels.'com.docker.compose.service' -ne 'lan-model-drop') { throw 'identity' }
        $code = 'service_upgrade_required'
        Assert-WizardImage -ImageId $existing.Image -ConfiguredImage $existing.Config.Image -ExpectedImage $expectedImage
    }
    $port = 8833
    $storage = 'new-volume'
    if ($existing) {
        $bindings = @($existing.HostConfig.PortBindings.'8080/tcp')
        if ($bindings.Count -ne 1 -or $bindings[0].HostIp -notin @('', '0.0.0.0', '::')) { $code = 'service_conflict'; throw 'binding' }
        $port = [int]$bindings[0].HostPort
        $mounts = @($existing.Mounts | Where-Object { $_.Destination -eq '/data' })
        if ($mounts.Count -ne 1) { $code = 'service_conflict'; throw 'storage' }
        $storage = 'existing'
    }
    if ($request.method -eq 'inspect') {
        $dataPresent = [bool](Invoke-Docker @('volume', 'ls', '-q', '--filter', 'name=^lan-model-drop_drop-data$')).Trim()
        $result = @{ ownership = $(if ($existing) { 'existing-owned' } else { 'fresh' }); containerId = $(if ($existing) { $existing.Id } else { '' }); port = $port; storage = $storage; running = [bool]($existing -and $existing.State.Running); ready = $true; dataPresent = $dataPresent }
    } elseif ($request.method -eq 'health') {
        $code = 'service_absent'
        if (-not $existing -or $existing.Id -ne $request.containerId) { throw 'identity' }
        $readyDeadline = [DateTime]::UtcNow.AddSeconds(90)
        do {
            $healthy = Test-ServiceHealth ('http://127.0.0.1:{0}' -f $port)
            if ($healthy -or -not $existing.State.Running) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        $urls = @()
        if ($healthy) { $urls = @(Get-LanUrls $port) }
        $code = 'service_changed'
        $current = Get-WizardCurrentService -ExpectedId $existing.Id -ExpectedStorage ($mounts[0].Type + ':' + $mounts[0].Source)
        $code = 'service_upgrade_required'
        Assert-WizardImage -ImageId $current.imageId -ConfiguredImage $current.configuredImage -ExpectedImage $expectedImage
        if (-not $current.running) { $urls = @() }
        $result = @{ identityVerified = $true; dataPreserved = $true; serviceState = $(if ($current.running) { 'running' } else { 'stopped' }); healthy = [bool]($healthy -and $current.running -and $urls.Count -gt 0); urls = $urls; verificationScope = 'same-host' }
    } else {
        # Existing services are started in place, never recreated from bundled defaults.
        if ($request.containerId) {
            $code = 'service_changed'
            if (-not $existing -or $existing.Id -ne $request.containerId) { throw 'identity' }
            if ($request.method -eq 'start') { $mutationAttempted = $true; $null = Invoke-Docker @('start', $existing.Id) 120 }
            $result = @{ containerId = $existing.Id }
        } else {
            $code = 'service_changed'
            if ($existing) { throw 'existing' }
            $code = 'invalid_configuration'
            if ($request.port -lt 1024 -or $request.port -gt 65535 -or $request.stationName.Length -lt 1 -or $request.stationName.Length -gt 80) { throw 'configuration' }
            # No inherited credential or unrelated .env settings enter fresh setup.
            $env:DROP_PORT = [string]$request.port
            $env:DROP_STATION_NAME = [string]$request.stationName
            $env:DROP_STATION_KEY = ''
            $env:DROP_PUBLIC_URL = ''
            $env:DROP_MAX_BYTES = '268435456'
            $env:DROP_TTL_HOURS = '24'
            $env:DROP_QUEUE_MAX_FILES = '50'
            $env:DROP_QUEUE_MAX_BYTES = '2147483648'
            $volumes = (Invoke-Docker @('volume', 'ls', '-q', '--filter', 'name=^lan-model-drop_drop-data$')).Trim()
            $code = 'existing_storage_requires_review'
            if ($volumes) { throw 'volume' }
            if ($request.method -eq 'pull') {
                $code = 'image_pull_failed'
                $null = Invoke-Docker ($script:Compose + @('pull', 'lan-model-drop')) 300
                $result = @{ containerId = '' }
            } else {
                $code = 'port_unavailable'
                $listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Any, [int]$request.port)
                $listener.ExclusiveAddressUse = $true
                try { $listener.Start() } finally { $listener.Stop() }
                $code = 'service_start_failed'
                $mutationAttempted = $true
                $null = Invoke-Docker ($script:Compose + @('up', '-d', '--no-build', '--no-recreate', '--wait', '--wait-timeout', '90', 'lan-model-drop')) 120
                $id = (Invoke-Docker ($script:Compose + @('ps', '-q', 'lan-model-drop'))).Trim()
                if ($id -notmatch '^[a-f0-9]{12,64}$') { throw 'identity' }
                $result = @{ containerId = $id }
            }
        }
    }
    @{ schema = 1; ok = $true; result = $result } | ConvertTo-Json -Compress -Depth 5
} catch {
    @{ schema = 1; ok = $false; error = @{ code = $code; mutationAttempted = $mutationAttempted } } | ConvertTo-Json -Compress -Depth 3
    exit 1
} finally {
    $line = $null
    $request = $null
    $existing = $null
    if ($emptyEnvironmentFile -and [IO.File]::Exists($emptyEnvironmentFile)) { [IO.File]::Delete($emptyEnvironmentFile) }
}
