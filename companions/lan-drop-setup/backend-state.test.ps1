$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root 'lan-model-drop/wizard-policy.ps1')
$script:id = 'a' * 64
$script:listed = $script:id.Substring(0, 12)
$script:running = $false
$script:storage = '/fixture/data'
$script:repository = 'ghcr.io/ding-ding-projects/bambustudio-lan-model-drop'
$script:digest = 'sha256:' + ('b' * 64)
$script:image = $script:repository + ':v1.0.1@' + $script:digest
$script:availableDigest = $script:repository + '@' + $script:digest
function Invoke-Docker([string[]] $Arguments) {
    switch ($Arguments[0]) {
        'ps' { return $script:listed }
        'inspect' { return (ConvertTo-Json -InputObject @(@{ Id = $script:id; Image = 'sha256:' + ('c' * 64);
            Config = @{ Image = $script:image; Labels = @{ 'com.docker.compose.project' = 'lan-model-drop'; 'com.docker.compose.service' = 'lan-model-drop' } };
            Mounts = @(@{ Destination = '/data'; Type = 'volume'; Source = $script:storage });
            State = @{ Running = $script:running } }) -Depth 7 -Compress) }
        'image' { return (ConvertTo-Json -InputObject @($script:availableDigest) -Compress) }
        default { throw 'unexpected_fixture_operation' }
    }
}
$observed = Get-WizardCurrentService -ExpectedId $script:id -ExpectedStorage 'volume:/fixture/data'
if ($observed.running) { throw 'exited_container_reported_running' }
$script:running = $true
if (-not (Get-WizardCurrentService -ExpectedId $script:id -ExpectedStorage 'volume:/fixture/data').running) { throw 'current_running_state_missing' }
$script:listed = 'd' * 12
$rejected = $false
try { $null = Get-WizardCurrentService -ExpectedId $script:id -ExpectedStorage 'volume:/fixture/data' } catch { if ($_.Exception.Message -eq 'service_changed') { $rejected = $true } else { throw } }
if (-not $rejected) { throw 'replacement_adopted' }
$script:listed = $script:id.Substring(0, 12)
$script:storage = '/fixture/other'
$rejected = $false
try { $null = Get-WizardCurrentService -ExpectedId $script:id -ExpectedStorage 'volume:/fixture/data' } catch { if ($_.Exception.Message -eq 'service_changed') { $rejected = $true } else { throw } }
if (-not $rejected) { throw 'changed_storage_adopted' }
Assert-WizardImage -ImageId ('sha256:' + ('c' * 64)) -ConfiguredImage $script:image -ExpectedImage $script:image
$script:availableDigest = $script:repository + '@sha256:' + ('e' * 64)
$rejected = $false
try { Assert-WizardImage -ImageId ('sha256:' + ('c' * 64)) -ConfiguredImage $script:image -ExpectedImage $script:image } catch { if ($_.Exception.Message -eq 'service_upgrade_required') { $rejected = $true } else { throw } }
if (-not $rejected) { throw 'wrong_published_digest_admitted' }
Write-Output 'PASS: current service state, replacement identity, preserved storage and published image digest are checked by production consumers.'
