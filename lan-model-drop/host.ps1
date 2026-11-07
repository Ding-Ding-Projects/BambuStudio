<#
.SYNOPSIS
Starts the published LAN model drop service without replacing its data volume.
.DESCRIPTION
Use host.bat --detached or host.bat --logs. The native application supplies an
owner-restricted local named pipe for pairing. Credentials never enter console
output, command arguments, a result file, or a transcript.
#>
[CmdletBinding()]
param(
    [ValidateSet('Choose', 'Detached', 'Logs')][string] $Mode = 'Choose',
    [string] $PairingPipe = '',
    [Parameter(ValueFromRemainingArguments = $true)][string[]] $Options
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:Deadline = [DateTime]::UtcNow.AddSeconds(600)
$script:FailureCode = 'startup_failed'
$script:Docker = $null
$script:Compose = @('compose', '--project-directory', $PSScriptRoot, '-f', (Join-Path $PSScriptRoot 'compose.yaml'))

function Quote-NativeArgument([string] $Value) {
    # Windows CommandLineToArgvW quoting, including backslashes before a quote.
    return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}

function Invoke-Docker([string[]] $Arguments, [int] $TimeoutSeconds = 30) {
    $remaining = [int][Math]::Ceiling(($script:Deadline - [DateTime]::UtcNow).TotalSeconds)
    if ($remaining -le 0) { throw 'deadline' }
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $script:Docker
    $start.Arguments = ($Arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'process_start' }
        # Drain both streams concurrently. Never forward Docker output into a
        # pairing result; config and station-key output may contain credentials.
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(1000 * [Math]::Min($remaining, $TimeoutSeconds))) {
            $process.Kill() # only this owned child, never the daemon or containers
            $process.WaitForExit()
            throw 'process_timeout'
        }
        $output = $stdout.GetAwaiter().GetResult()
        $null = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw 'process_failed' }
        return $output
    } finally { $process.Dispose() }
}

function Send-PairingResult($Result) {
    if (-not $PairingPipe) { return }
    $pipe = New-Object IO.Pipes.NamedPipeClientStream('.', $PairingPipe, [IO.Pipes.PipeDirection]::Out)
    try {
        $pipe.Connect(5000)
        $json = ConvertTo-Json -InputObject $Result -Compress -Depth 4
        $bytes = [Text.Encoding]::UTF8.GetBytes($json + "`n")
        try {
            if ($bytes.Length -gt 8192) { throw 'result_too_large' }
            $pipe.Write($bytes, 0, $bytes.Length)
            $pipe.Flush()
        } finally {
            [Array]::Clear($bytes, 0, $bytes.Length)
            $json = $null
        }
    } finally { $pipe.Dispose() }
}

function Test-ServiceHealth([string] $BaseUrl) {
    try {
        $response = Invoke-WebRequest -UseBasicParsing -Uri ($BaseUrl.TrimEnd('/') + '/healthz') -TimeoutSec 3 -MaximumRedirection 0
        if ($response.StatusCode -ne 200 -or $response.Content.Length -gt 4096) { return $false }
        $health = ConvertFrom-Json -InputObject $response.Content
        return $health.ok -eq $true -and $health.service -eq 'lan-model-drop' -and $health.protocol -eq 1
    } catch { return $false }
}

function Get-LanUrls([int] $Port) {
    $addresses = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() |
        Where-Object {
            $_.OperationalStatus -eq 'Up' -and
            $_.NetworkInterfaceType -in @('Ethernet', 'Wireless80211') -and
            $_.GetIPProperties().GatewayAddresses.Count -gt 0
        } |
        ForEach-Object { $_.GetIPProperties().UnicastAddresses } |
        Where-Object { $_.Address.AddressFamily -eq 'InterNetwork' } |
        ForEach-Object { $_.Address.IPAddressToString } | Sort-Object -Unique)
    foreach ($address in $addresses) {
        # Private LAN interfaces only; do not advertise public or link-local IPs.
        if ($address -notmatch '^(10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.)') { continue }
        $url = 'http://{0}:{1}/' -f $address, $Port
        if (Test-ServiceHealth $url) { $url }
    }
}

function Show-ServiceLogs([string[]] $Urls) {
    $following = $true
    $refresh = $true
    while ($true) {
        if ($refresh -or $following) {
            Clear-Host
            Write-Host 'Bambu Studio | LAN model drop'
            Write-Host 'Selected service: lan-model-drop (1 of 1)'
            Write-Host ('F: follow  P: pause  R: refresh  Q/Escape: leave running | Following: {0}' -f $following)
            foreach ($url in $Urls) { Write-Host $url }
            # Leaving this view never stops the service or deletes a volume.
            $script:Deadline = [DateTime]::UtcNow.AddSeconds(30)
            try {
                Write-Host (Invoke-Docker ($script:Compose + @('ps', 'lan-model-drop')))
                Write-Host (Invoke-Docker ($script:Compose + @('logs', '--no-color', '--tail', '18', 'lan-model-drop')))
            } catch { Write-Host 'Status is unavailable. Press R to retry or Q to leave.' }
            $refresh = $false
        }
        for ($tick = 0; $tick -lt 10; $tick++) {
            if ([Console]::KeyAvailable) {
                switch ([Console]::ReadKey($true).Key) {
                    'Q' { return }
                    'Escape' { return }
                    'F' { $following = $true; $refresh = $true }
                    'P' { $following = $false; $refresh = $true }
                    'R' { $refresh = $true }
                    'LeftArrow' { $refresh = $true }
                    'RightArrow' { $refresh = $true }
                }
                break
            }
            Start-Sleep -Milliseconds 100
        }
    }
}

try {
    foreach ($option in $Options) {
        switch ($option) {
            '--detached' { $Mode = 'Detached' }
            '--logs' { $Mode = 'Logs' }
            default { $script:FailureCode = 'invalid_option'; throw 'option' }
        }
    }
    if ($PairingPipe -and $PairingPipe -notmatch '^BambuLanDrop-[a-fA-F0-9-]{32,36}$') {
        $PairingPipe = ''
        $script:FailureCode = 'invalid_pairing_pipe'
        throw 'pipe_name'
    }
    if ($PairingPipe) { $Mode = 'Detached' }
    if ($Mode -eq 'Choose') {
        if ([Console]::IsInputRedirected) { throw 'Choose --detached or --logs for redirected input.' }
        Write-Host 'Bambu Studio | Start LAN model drop'
        Write-Host 'D: start in background   L: start and view live logs   Q: cancel'
        switch ([Console]::ReadKey($true).Key) {
            'D' { $Mode = 'Detached' }
            'L' { $Mode = 'Logs' }
            default { exit 2 }
        }
    }
    if ($Mode -eq 'Logs' -and [Console]::IsInputRedirected) {
        $script:FailureCode = 'interactive_console_required'; throw 'console'
    }
    $script:FailureCode = 'docker_missing'
    $dockerCommand = Get-Command docker.exe -ErrorAction SilentlyContinue
    if ($dockerCommand) { $script:Docker = $dockerCommand.Source }
    else {
        foreach ($candidate in @(
            (Join-Path $env:ProgramFiles 'Docker\Docker\resources\bin\docker.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\DockerDesktop\resources\bin\docker.exe')
        )) { if (Test-Path -LiteralPath $candidate -PathType Leaf) { $script:Docker = $candidate; break } }
    }
    if (-not $script:Docker) { throw 'docker' }
    $script:FailureCode = 'docker_unavailable'
    $null = Invoke-Docker @('info', '--format', '{{.ServerVersion}}')
    $script:FailureCode = 'compose_unavailable'
    $null = Invoke-Docker @('compose', 'version', '--short')
    $script:FailureCode = 'image_pull_failed'
    if (-not $PairingPipe) { Write-Host 'Pulling the published LAN model drop image...' }
    $null = Invoke-Docker ($script:Compose + @('pull', 'lan-model-drop')) 300
    $script:FailureCode = 'service_start_failed'
    if (-not $PairingPipe) { Write-Host 'Starting the service, preserving its existing data...' }
    $null = Invoke-Docker ($script:Compose + @('up', '-d', '--no-build', '--wait', '--wait-timeout', '90', 'lan-model-drop')) 120
    $script:FailureCode = 'port_unavailable'
    $published = Invoke-Docker ($script:Compose + @('port', 'lan-model-drop', '8080'))
    $portMatch = [regex]::Match($published, '(?m)^(?:0\.0\.0\.0|\[::\]):([0-9]{1,5})\s*$')
    if (-not $portMatch.Success) { throw 'not_lan_bound' }
    $port = [int]$portMatch.Groups[1].Value
    if ($port -lt 1 -or $port -gt 65535) { throw 'port_range' }
    $address = 'http://127.0.0.1:{0}' -f $port
    $script:FailureCode = 'health_check_failed'
    if (-not (Test-ServiceHealth $address)) { throw 'health' }
    $script:FailureCode = 'lan_unreachable'
    $urls = @(Get-LanUrls $port)
    if ($urls.Count -eq 0) { throw 'lan' }
    if ($PairingPipe) {
        $script:FailureCode = 'pairing_failed'
        $key = (Invoke-Docker ($script:Compose + @('exec', '-T', 'lan-model-drop', 'node', 'server/station-key.mjs'))).Trim()
        if ($key -notmatch '^[\x21-\x7e]{16,512}$') { throw 'key' }
        try { Send-PairingResult @{ schema = 1; ok = $true; address = $address; lanUrl = $urls[0]; stationKey = $key } }
        finally { $key = $null }
    } else {
        Write-Host 'LAN model drop is healthy. These LAN addresses answered from this computer:'
        foreach ($url in $urls) { Write-Host $url }
        Write-Host 'Check the selected address from the sending device too. Its network and firewall can differ.'
        Write-Host 'Pair Bambu Studio using Start local drop server in LAN model drop settings.'
        if ($Mode -eq 'Logs') { Show-ServiceLogs $urls }
    }
    exit 0
} catch {
    if ($PairingPipe) {
        try { Send-PairingResult @{ schema = 1; ok = $false; code = $script:FailureCode } } catch { }
    } else {
        [Console]::Error.WriteLine('LAN model drop did not complete: ' + $script:FailureCode + '. Existing data was retained.')
        [Console]::Error.WriteLine('Check Docker Desktop, registry access and the selected host port, then retry. No firewall or host settings were changed.')
    }
    exit 1
}
