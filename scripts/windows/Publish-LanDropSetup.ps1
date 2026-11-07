$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dist = Join-Path $root 'companions/lan-drop-setup/dist'
foreach ($name in @('GITHUB_SHA', 'GITHUB_REPOSITORY', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'RUNNER_TEMP')) {
    if (-not [Environment]::GetEnvironmentVariable($name)) { throw "Missing publication context: $name" }
}
if ($env:GITHUB_SHA -notmatch '^[a-f0-9]{40}$' -or $env:GITHUB_RUN_ID -notmatch '^\d+$' -or $env:GITHUB_RUN_ATTEMPT -notmatch '^\d+$') { throw 'Invalid source or run identity.' }
function Read-GhJson([string[]] $Arguments) {
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $text = & gh @Arguments
        if ($LASTEXITCODE -eq 0) { return ($text | ConvertFrom-Json) }
        if ($attempt -lt 3) { Start-Sleep -Seconds (2 * $attempt) }
    }
    throw 'GitHub readback failed after three bounded attempts.'
}
$source = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $source -cne $env:GITHUB_SHA -or @(& git -C $root status --porcelain --untracked-files=all).Count) { throw 'Publication requires the exact clean producer source.' }
$manifestPath = Join-Path $dist 'build-manifest.json'
$proofPath = Join-Path $dist 'package-proof.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$proof = Get-Content -LiteralPath $proofPath -Raw | ConvertFrom-Json
$tree = (& git -C $root write-tree).Trim()
foreach ($record in @($manifest, $proof)) {
    if ($record.schema -ne 1 -or $record.sourceCommit -cne $source -or $record.indexTree -cne $tree -or $record.sourceClean -ne $true) { throw 'Source proof mismatch.' }
}
if ((Get-FileHash $manifestPath).Hash.ToLowerInvariant() -cne $proof.buildManifestSha256) { throw 'Build manifest hash mismatch.' }
$embedded = Join-Path $dist 'win-unpacked/resources/build-manifest.json'
if ((Get-FileHash $embedded).Hash.ToLowerInvariant() -cne $proof.buildManifestSha256) { throw 'Embedded producer manifest mismatch.' }
foreach ($property in $manifest.inputs.PSObject.Properties) {
    if ((Get-FileHash (Join-Path $root $property.Name)).Hash.ToLowerInvariant() -cne $property.Value) { throw 'Producer input hash mismatch.' }
}
$package = Get-Content (Join-Path $root 'companions/lan-drop-setup/package.json') -Raw | ConvertFrom-Json
if ($manifest.version -cne $package.version -or $manifest.electron -cne $package.devDependencies.electron -or $manifest.builder -cne $package.devDependencies.'electron-builder') { throw 'Package version proof mismatch.' }
$outputs = @($proof.outputs)
if ($outputs.Count -ne 1 -or $outputs[0].name -cne "BambuStudio-LAN-Drop-Setup-$($package.version)-x64.exe") { throw 'Unexpected portable output inventory.' }
$exe = Join-Path $dist $outputs[0].name
if ((Get-FileHash $exe).Hash.ToLowerInvariant() -cne $outputs[0].sha256 -or (Get-Item $exe).Length -ne $outputs[0].bytes) { throw 'Portable bytes mismatch.' }
$checksum = $exe + '.sha256'
if ((Get-Content $checksum -Raw).Trim() -cne ($outputs[0].sha256 + '  ' + $outputs[0].name)) { throw 'Checksum sidecar mismatch.' }
$tag = "lan-setup-v$($package.version)-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)"
$out = Join-Path $env:RUNNER_TEMP 'lan-setup-publication'
$null = New-Item -ItemType Directory -Path $out -Force
$jobs = Read-GhJson @('api', "repos/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID/attempts/$env:GITHUB_RUN_ATTEMPT/jobs")
$started = @($jobs.jobs | Where-Object started_at | Sort-Object started_at)[0].started_at
$start = [DateTimeOffset]::Parse($started).ToUniversalTime()
$notesPath = Join-Path $out 'notes.md'
$notes = @("# LAN hosting setup companion $($package.version)", '',
    "Source: $source", "Channel: dedicated portable companion; not the native application update feed.",
    'Unsigned Windows x64 executable. Unknown-publisher or SmartScreen warnings may appear.',
    'One executable launches without an adjacent runtime folder. Docker is required to host the service.',
    'Existing service data and credentials are retained. Custom credentials, folder storage, prerequisite installation and complete settings remain unfinished.',
    'This workflow builds and publishes only. It runs no tests, lint or GUI verification; source and package checks establish provenance, not runtime acceptance.',
    '', '區域網絡託管設定工具，獨立可攜版本，唔會取代原生應用程式更新頻道。',
    '此執行檔未簽署，可能顯示未知發行者提示。現有服務資料同憑證會保留，其他未完成能力請參閱功能文件。')
[IO.File]::WriteAllLines($notesPath, $notes, [Text.UTF8Encoding]::new($false))
$assets = @($exe, $checksum, $manifestPath, $proofPath)
# Never retry or overwrite publication blindly. A rerun gets a new attempt-specific tag.
& gh release create $tag @assets --repo $env:GITHUB_REPOSITORY --target $source --title "LAN setup $($package.version) (run $env:GITHUB_RUN_ID.$env:GITHUB_RUN_ATTEMPT)" --notes-file $notesPath --latest=false
if ($LASTEXITCODE -ne 0) { throw 'Release creation failed; inspect any partial release before rerunning.' }
$completed = [DateTimeOffset]::UtcNow
$duration = $completed - $start
$notes += @('', "- Workflow started: $($start.ToString('yyyy-MM-ddTHH:mm:ssZ'))", "- Workflow completed: $($completed.ToString('yyyy-MM-ddTHH:mm:ssZ'))", ('- Workflow duration: {0:00}:{1:00}:{2:00}' -f [int]$duration.TotalHours, $duration.Minutes, $duration.Seconds))
[IO.File]::WriteAllLines($notesPath, $notes, [Text.UTF8Encoding]::new($false))
& gh release edit $tag --repo $env:GITHUB_REPOSITORY --notes-file $notesPath --latest=false
if ($LASTEXITCODE -ne 0) { throw 'Published release timing update failed.' }
$release = Read-GhJson @('release', 'view', $tag, '--repo', $env:GITHUB_REPOSITORY, '--json', 'isDraft,isPrerelease,tagName,assets,url,body')
$ref = Read-GhJson @('api', "repos/$env:GITHUB_REPOSITORY/git/ref/tags/$tag")
if ($release.isDraft -or $release.isPrerelease -or $release.tagName -cne $tag -or $ref.object.type -cne 'commit' -or $ref.object.sha -cne $source) { throw 'Normal release or exact tag source readback mismatch.' }
$latest = Read-GhJson @('api', "repos/$env:GITHUB_REPOSITORY/releases/latest")
if ($latest.tag_name -ceq $tag) { throw 'Companion release unexpectedly became the latest release.' }
if (($release.body -replace "`r`n", "`n").TrimEnd() -cne ([IO.File]::ReadAllText($notesPath) -replace "`r`n", "`n").TrimEnd()) { throw 'Release notes readback mismatch.' }
$download = Join-Path $out 'download'
& gh release download $tag --repo $env:GITHUB_REPOSITORY --dir $download
if ($LASTEXITCODE -ne 0) { throw 'Release byte readback failed.' }
if (@($release.assets).Count -ne $assets.Count) { throw 'Release asset inventory mismatch.' }
foreach ($asset in $assets) {
    $name = [IO.Path]::GetFileName($asset)
    if ((Get-FileHash (Join-Path $download $name)).Hash -cne (Get-FileHash $asset).Hash) { throw 'Published asset hash mismatch.' }
}
@{ schema = 1; sourceCommit = $source; tag = $tag; url = $release.url; normalRelease = $true; assetsReadBack = $true; latestRequested = $false; packageSha256 = $outputs[0].sha256 } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'receipt.json') -Encoding utf8
