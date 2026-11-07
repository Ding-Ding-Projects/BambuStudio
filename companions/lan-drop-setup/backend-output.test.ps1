$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root 'lan-model-drop/host.ps1') -Mode Library
$script:Docker = (Get-Command node.exe -ErrorAction Stop).Source
$script:Deadline = [DateTime]::UtcNow.AddSeconds(60)
$bounded = $false
try { $null = Invoke-Docker @('-e', 'process.stdout.write("x".repeat(1100000))') 15 }
catch { if ($_.Exception.Message -eq 'output_limit') { $bounded = $true } else { throw } }
if (-not $bounded) { throw 'oversized_stdout_was_not_rejected' }
$result = Invoke-Docker @('-e', 'process.stderr.write("x".repeat(4000000));process.stdout.write("ok")') 15
if ($result -cne 'ok') { throw 'stderr_drain_blocked_stdout' }
Write-Output 'PASS: production process reader bounds stdout and drains oversized stderr concurrently.'
