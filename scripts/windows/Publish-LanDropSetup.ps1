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
function Format-Duration([TimeSpan] $Duration) {
    return '{0:00}:{1:00}:{2:00}' -f [math]::Floor($Duration.TotalHours), $Duration.Minutes, $Duration.Seconds
}
function Read-AllReleases([string] $Repository) {
    $all = @()
    for ($page = 1; $page -le 20; $page++) {
        $batch = @(Read-GhJson @('api', "repos/$Repository/releases?per_page=100&page=$page"))
        $all += $batch
        if ($batch.Count -lt 100) { return $all }
    }
    throw 'Release history exceeds the bounded discovery limit.'
}
function Resolve-ReleaseDish {
    # Metadata only, never download or republish photographs. Failure is an explicit optional decoration state.
    try {
        $catalogRepository = 'Ding-Ding-Projects/dim-sum-photos'
        $revision = (Read-GhJson @('api', "repos/$catalogRepository/commits/main")).sha
        $catalog = Read-GhJson @('api', '-H', 'Accept: application/vnd.github.raw+json', "repos/$catalogRepository/contents/catalog/index.json?ref=$revision")
        $history = @(Read-AllReleases $env:GITHUB_REPOSITORY)
        $used = ($history | ForEach-Object { $_.name; $_.body }) -join "`n"
        $photoReleases = @(Read-AllReleases $catalogRepository | Where-Object { $_.tag_name -like 'catalog-v1*' -and -not $_.draft -and -not $_.prerelease })
        $assets = @{}
        foreach ($photoRelease in $photoReleases) {
            # The release list may truncate assets; enumerate each assets endpoint independently.
            for ($page = 1; $page -le 50; $page++) {
                $batch = @(Read-GhJson @('api', "repos/$catalogRepository/releases/$($photoRelease.id)/assets?per_page=100&page=$page"))
                foreach ($asset in $batch) {
                    if ($asset.state -eq 'uploaded' -and $asset.size -gt 0 -and $asset.browser_download_url -like "https://github.com/$catalogRepository/releases/download/catalog-v1*/*") { $assets[$asset.name] = $asset }
                }
                if ($batch.Count -lt 100) { break }
                if ($page -eq 50) { throw 'Photo discovery limit reached.' }
            }
        }
        foreach ($dish in $catalog.dishes) {
            if ($dish.id -notmatch '^hk-dish-\d{4}$' -or -not $dish.name.en -or -not $dish.name.zhHant) { continue }
            if ($used.Contains($dish.id) -or $used.Contains($dish.name.en) -or $used.Contains($dish.name.zhHant)) { continue }
            $file = [IO.Path]::GetFileName($dish.image.path)
            if ($assets.ContainsKey($file)) {
                return @{ status = 'resolved'; id = $dish.id; name = "$($dish.name.en) · $($dish.name.zhHant)"; photo = $assets[$file].browser_download_url; catalogRevision = $revision }
            }
        }
    } catch { }
    return @{ status = 'unavailable'; name = ''; reason = 'No unused dish with a verified published photo could be resolved from the public catalog and complete bounded release history.' }
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
$buildSteps = @($jobs.jobs | ForEach-Object { $_.steps } | Where-Object { $_.name -ceq 'Build single portable executable through the root entrypoint' })
if ($buildSteps.Count -ne 1 -or $buildSteps[0].conclusion -cne 'success' -or -not $buildSteps[0].started_at -or -not $buildSteps[0].completed_at) { throw 'Completed producer step timing is unavailable.' }
$buildStart = [DateTimeOffset]::Parse($buildSteps[0].started_at).ToUniversalTime()
$buildEnd = [DateTimeOffset]::Parse($buildSteps[0].completed_at).ToUniversalTime()
if ($buildEnd -lt $buildStart -or $buildEnd -gt [DateTimeOffset]::UtcNow) { throw 'Invalid producer timing.' }
$dish = Resolve-ReleaseDish
$lineCountPath = Join-Path $out 'line-count.md'
& (Join-Path $root 'scripts/ci/Measure-LineCount.ps1') -Root $root -OutputMarkdown $lineCountPath
if (-not (Test-Path -LiteralPath $lineCountPath -PathType Leaf)) { throw 'Release line count is unavailable.' }
$notesPath = Join-Path $out 'notes.md'
$notes = @("# LAN hosting setup companion $($package.version)", '',
    "Source: $source", "Channel: dedicated portable companion; not the native application update feed.",
    'Unsigned Windows x64 executable. Unknown-publisher or SmartScreen warnings may appear.',
    'One executable launches without an adjacent runtime folder. Docker is required to host the service.',
    'Existing service data and credentials are retained. Custom credentials, folder storage, prerequisite installation and complete settings remain unfinished.',
    'This workflow builds and publishes only. It runs no tests, lint or GUI verification; source and package checks establish provenance, not runtime acceptance.',
    '', '區域網絡託管設定工具，獨立可攜版本，唔會取代原生應用程式更新頻道。',
    '此執行檔未簽署，可能顯示未知發行者提示。現有服務資料同憑證會保留，其他未完成能力請參閱功能文件。',
    '', "- Build started: $($buildStart.ToString('yyyy-MM-ddTHH:mm:ssZ'))", "- Build completed: $($buildEnd.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "- Build duration: $(Format-Duration ($buildEnd - $buildStart))", '- Whole-workflow terminal timing: unverified while this workflow is running.')
if ($dish.status -eq 'resolved') {
    $notes += @('', "Code name: $($dish.name)", "Catalog dish: $($dish.id)", "Catalog revision: $($dish.catalogRevision)", "[Public dish photo: $($dish.name)]($($dish.photo))")
} else { $notes += @('', 'Code name and photo unavailable: ' + $dish.reason) }
$notes += @('', (Get-Content -LiteralPath $lineCountPath -Raw))
[IO.File]::WriteAllLines($notesPath, $notes, [Text.UTF8Encoding]::new($false))
$assets = @($exe, $checksum, $manifestPath, $proofPath)
# Never retry or overwrite publication blindly. A rerun gets a new attempt-specific tag.
& gh release create $tag @assets --repo $env:GITHUB_REPOSITORY --target $source --title "LAN setup $($package.version) $($dish.name) (run $env:GITHUB_RUN_ID.$env:GITHUB_RUN_ATTEMPT)" --notes-file $notesPath --latest=false
if ($LASTEXITCODE -ne 0) { throw 'Release creation failed; inspect any partial release before rerunning.' }
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
$verifiedAt = [DateTimeOffset]::UtcNow.ToString('o')
@{ schema = 1; sourceCommit = $source; tag = $tag; url = $release.url; normalRelease = $true; assetsReadBack = $true; latestRequested = $false; packageSha256 = $outputs[0].sha256;
   publicationVerifiedAt = $verifiedAt; buildStartedAt = $buildStart.ToString('o'); buildCompletedAt = $buildEnd.ToString('o'); wholeWorkflowTiming = 'unverified'; codename = $dish } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'receipt.json') -Encoding utf8
