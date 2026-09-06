<#
.SYNOPSIS
    Produce the reproducible line-count table used by Windows releases.
.DESCRIPTION
    Counts surviving tracked lines at the checkout's current commit.  The report
    keeps project code separate from tests, styles/markup, generated material,
    and excluded vendored/dependency/lockfile content.  Included-line
    attribution is derived from git blame so the totals describe surviving
    lines rather than historical churn.
#>

[CmdletBinding()]
param(
    [string] $Root = (Get-Location).Path,

    [string] $OutputMarkdown = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# git prints paths and blame authors as UTF-8 (core.quotePath=false below), but
# PowerShell decodes native output with the console code page. On a host whose
# console is a legacy code page the tracked CJK-named file
# deps/libharu/libharu/版本.txt arrived mangled and was reported missing from
# the checkout; the hosted runner's console is already UTF-8, which is why CI
# never saw it. Decode as UTF-8 for this process only and restore on exit.
$script:previousConsoleEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path

function Invoke-Git {
    param(
        [Parameter(Mandatory)][string[]] $Arguments
    )

    $result = @(& git -c core.quotePath=false -C $resolvedRoot @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
    }
    return $result
}

function Get-LogicalLineCounts {
    param([Parameter(Mandatory)][string] $Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) {
        return [pscustomobject] @{ Total = 0; NonBlank = 0; IsBinary = $false }
    }

    $sampleLength = [Math]::Min($bytes.Length, 65536)
    for ($byteIndex = 0; $byteIndex -lt $sampleLength; $byteIndex++) {
        if ($bytes[$byteIndex] -eq 0) {
            return [pscustomobject] @{ Total = 0; NonBlank = 0; IsBinary = $true }
        }
    }
    try {
        $strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)
        $text = $strictUtf8.GetString($bytes)
    } catch [System.Text.DecoderFallbackException] {
        return [pscustomobject] @{ Total = 0; NonBlank = 0; IsBinary = $true }
    }
    $lineBreaks = [regex]::Matches($text, "\r\n|\n|\r").Count
    $endsWithBreak = $text.EndsWith("`n") -or $text.EndsWith("`r")
    $total = $lineBreaks + ($(if ($endsWithBreak) { 0 } else { 1 }))
    $nonBlank = @($text -split "\r\n|\n|\r" | Where-Object { $_.Trim().Length -gt 0 }).Count
    return [pscustomobject] @{ Total = $total; NonBlank = $nonBlank; IsBinary = $false }
}

function Test-AgentCommit {
    param(
        [Parameter(Mandatory)][string] $Sha,
        [Parameter(Mandatory)][string] $Author
    )

    # Every automation commit in this repository is authored as Claude Fable 5,
    # and blame exposes that surviving author without a second git process per
    # line.  The SHA argument is retained so this function can grow a trailer
    # lookup later without changing the caller or the arithmetic contract.
    return $Author -match '(?i)claude\s+fable|codex|openai|automation|agent'
}

function Get-BlameCounts {
    param([Parameter(Mandatory)][string] $RelativePath)

    $blame = @(& git -c core.quotePath=false -C $resolvedRoot blame --line-porcelain -- $RelativePath)
    if ($LASTEXITCODE -ne 0) {
        throw "git blame failed for '$RelativePath'."
    }

    [long] $agent = 0
    [long] $other = 0
    $index = 0
    while ($index -lt $blame.Count) {
        $header = [string] $blame[$index]
        if ($header -notmatch '^([0-9a-f]{40})\s+\d+\s+\d+(?:\s+(\d+))?$') {
            $index++
            continue
        }

        $sha = $Matches[1]
        # --line-porcelain repeats a header for every source line.  The optional
        # fourth field on the first header describes the following run, but the
        # following headers are still emitted, so counting it would double-count.
        $runLength = 1
        $author = ''
        $cursor = $index + 1
        while ($cursor -lt $blame.Count -and
            ([string] $blame[$cursor]) -notmatch '^[0-9a-f]{40}\s+\d+\s+\d+(?:\s+\d+)?$') {
            if ([string] $blame[$cursor] -match '^author (.*)$') {
                $author = $Matches[1]
            }
            $cursor++
        }

        if (Test-AgentCommit -Sha $sha -Author $author) {
            $agent += $runLength
        } else {
            $other += $runLength
        }
        $index = $cursor
    }

    return [pscustomobject] @{ Agent = $agent; Other = $other }
}

function Get-Category {
    param([Parameter(Mandatory)][string] $RelativePath)

    $path = $RelativePath.Replace('\', '/')
    if ($path -match '(?i)(^|/)(generated|gen|routeTree\.gen)(/|\.|$)') {
        return 'Generated'
    }
    if ($path -match '(?i)(^|/)(tests?|__tests__|fixtures?)(/|$)' -or
        $path -match '(?i)(^|/)(test|spec)\.[^.]+$' -or
        $path -match '(?i)(^|/)(test|spec)[^/]*\.[^.]+$' -or
        $path -match '(?i)(^|/)Test-[^/]+\.ps1$') {
        return 'Tests'
    }
    if ($path -match '(?i)\.(css|scss|sass|less|html?|md|mdx|svg|xrc|xml|po|pot)$') {
        return 'Styles/markup/docs'
    }
    return 'Project source'
}

function Test-ExcludedPath {
    param([Parameter(Mandatory)][string] $RelativePath)

    $path = $RelativePath.Replace('\', '/')
    return $path -match '(?i)(^|/)(\.git|build|builds|deps|dist|install-dir|node_modules|vendor|vendors|third_party|third-party|external|cache|\.cache|artifacts)(/|$)' -or
        $path -match '(?i)^src/(admesh|agg|ankerl|boost|clipper2?|earcut|eigen|fast_float|glu-libtess|hidapi|imgui|imguizmo|libigl|libnest2d|mcut|minilzo|minimp4|miniz|nanosvg|nlohmann|qhull|semver|shiny|stb_dxt|tinybvh)(/|$)' -or
        $path -match '(?i)(^|/)(resources/(hms|i18n|web/data|web/include)|src/slic3r/GUI/DeviceWeb/device_page/locales)(/|$)' -or
        $path -match '(?i)(^|/)(package-lock\.json|pnpm-lock\.yaml|yarn\.lock|Cargo\.lock|composer\.lock|Gemfile\.lock|Podfile\.lock)$' -or
        $path -match '(?i)\.(7z|a|bin|bmp|cur|dll|dylib|exe|gif|gcode|icns|ico|jpg|jpeg|lib|map|mo|mp3|mp4|nupkg|obj|pdb|pdf|png|so|stl|ttf|wasm|webp|woff2|zip)$'
}

function Get-GitLineCountMap {
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Pattern)

    $output = @(& git -c core.quotePath=false -C $resolvedRoot grep -I -c $Pattern -- .)
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 1) {
        throw "git grep line count failed for pattern '$Pattern' with exit code $LASTEXITCODE."
    }
    $map = @{}
    foreach ($entry in $output) {
        $text = [string] $entry
        $separator = $text.LastIndexOf(':')
        if ($separator -lt 1) {
            throw "Unexpected git grep line-count output: '$text'."
        }
        $path = $text.Substring(0, $separator).Replace('\', '/')
        $value = 0L
        if (-not [long]::TryParse($text.Substring($separator + 1), [Globalization.NumberStyles]::Integer, [Globalization.CultureInfo]::InvariantCulture, [ref] $value)) {
            throw "Unexpected line count in git grep output: '$text'."
        }
        $map[$path] = $value
    }
    return $map
}

$head = ([string] (Invoke-Git -Arguments @('rev-parse', 'HEAD'))).Trim()
if ($head -notmatch '^[0-9a-f]{40}$') {
    throw "The checkout HEAD is not a full commit SHA: '$head'."
}

$tracked = @((Invoke-Git -Arguments @('ls-files')) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($tracked.Count -eq 0) {
    throw 'No tracked files were found.'
}
$gitlinks = @{}
foreach ($indexEntry in (Invoke-Git -Arguments @('ls-files', '-s'))) {
    if ([string] $indexEntry -match '^160000\s+\S+\s+\d+\t(.+)$') {
        $gitlinks[$Matches[1].Replace('\', '/')] = $true
    }
}

# git grep performs the line scan in one repository process and handles the
# checkout's mixed CRLF/LF files consistently.  Binary files are absent from
# both maps and remain visible as zero-line excluded payloads below.
$totalLineMap = Get-GitLineCountMap -Pattern ''
$nonBlankLineMap = Get-GitLineCountMap -Pattern '[^[:space:]]'

$rows = @{}
foreach ($category in @('Project source', 'Tests', 'Styles/markup/docs', 'Generated', 'Excluded non-source (vendor/dependency/catalog/binary/lockfile)')) {
    $rows[$category] = [pscustomobject] @{ Files = 0; Total = [long] 0; NonBlank = [long] 0; Agent = [long] 0; Other = [long] 0 }
}

foreach ($relativePath in $tracked) {
    $relative = ([string] $relativePath).Replace('/', '\')
    $fullPath = Join-Path $resolvedRoot $relative
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        if ($gitlinks.ContainsKey([string] $relativePath)) {
            $rows['Excluded non-source (vendor/dependency/catalog/binary/lockfile)'].Files++
            continue
        }
        throw "Tracked file '$relativePath' is missing from the checkout."
    }

    $pathKey = ([string] $relativePath).Replace('\', '/')
    $isText = $totalLineMap.ContainsKey($pathKey)
    if ($isText) {
        $counts = [pscustomobject] @{
            Total = [long] $totalLineMap[$pathKey]
            NonBlank = [long] $(if ($nonBlankLineMap.ContainsKey($pathKey)) { $nonBlankLineMap[$pathKey] } else { 0 })
            IsBinary = $false
        }
    } else {
        $counts = [pscustomobject] @{
            Total = 0L
            NonBlank = 0L
            IsBinary = ((Get-Item -LiteralPath $fullPath).Length -gt 0)
        }
    }
    $category = if ((Test-ExcludedPath -RelativePath $relativePath) -or $counts.IsBinary) {
        'Excluded non-source (vendor/dependency/catalog/binary/lockfile)'
    } else {
        Get-Category -RelativePath $relativePath
    }
    $blame = $null
    if ($category -ne 'Excluded non-source (vendor/dependency/catalog/binary/lockfile)' -and $counts.Total -gt 0) {
        $blame = Get-BlameCounts -RelativePath $relativePath
        $blameTotal = $blame.Agent + $blame.Other
        if ($blameTotal -ne $counts.Total -and [Math]::Abs($blameTotal - $counts.Total) -gt 1) {
            throw "Blame arithmetic disagrees for '$relativePath': lines=$($counts.Total), agent=$($blame.Agent), other=$($blame.Other)."
        }
        # git grep intentionally omits a final empty record after a file's
        # trailing newline; blame retains it.  Use blame's one-line correction
        # so the published total and its attribution always agree.
        if ($blameTotal -ne $counts.Total) { $counts.Total = $blameTotal }
    }

    $row = $rows[$category]
    $row.Files++
    $row.Total += $counts.Total
    $row.NonBlank += $counts.NonBlank
    if ($null -ne $blame) {
        $row.Agent += $blame.Agent
        $row.Other += $blame.Other
    }
}

$projectCategories = @('Project source', 'Tests', 'Styles/markup/docs', 'Generated')
$projectFiles = [long] 0
$projectTotal = [long] 0
$projectNonBlank = [long] 0
$projectAgent = [long] 0
$projectOther = [long] 0
foreach ($category in $projectCategories) {
    $row = $rows[$category]
    $projectFiles += $row.Files
    $projectTotal += $row.Total
    $projectNonBlank += $row.NonBlank
    $projectAgent += $row.Agent
    $projectOther += $row.Other
}
if ($projectAgent + $projectOther -ne $projectTotal) {
    throw "Project blame arithmetic disagrees: lines=$projectTotal, agent=$projectAgent, other=$projectOther."
}

$allFiles = [long] 0
$allTotal = [long] 0
$allNonBlank = [long] 0
foreach ($category in $rows.Keys) {
    $row = $rows[$category]
    $allFiles += $row.Files
    $allTotal += $row.Total
    $allNonBlank += $row.NonBlank
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Repository line count")
$lines.Add('')
$lines.Add("Measured by `scripts/ci/Measure-LineCount.ps1` at commit $head.")
$lines.Add('Blank lines are included in **Total lines**; **Non-blank lines** are a readability measure.')
$lines.Add('Agent attribution uses surviving-line `git blame`; an agent line is one whose blame author identifies an automation agent.')
$lines.Add('')
$lines.Add('| Scope | Files | Total lines | Non-blank lines | Agent-attributed lines | Other-attributed lines |')
$lines.Add('| --- | ---: | ---: | ---: | ---: | ---: |')
foreach ($category in $projectCategories) {
    $row = $rows[$category]
    $lines.Add("| $category | $($row.Files) | $($row.Total) | $($row.NonBlank) | $($row.Agent) | $($row.Other) |")
}
$lines.Add("| **Project total (included)** | **$projectFiles** | **$projectTotal** | **$projectNonBlank** | **$projectAgent** | **$projectOther** |")
$excluded = $rows['Excluded non-source (vendor/dependency/catalog/binary/lockfile)']
$lines.Add("| Excluded non-source (vendor/dependency/catalog/binary/lockfile) | $($excluded.Files) | $($excluded.Total) | $($excluded.NonBlank) | — | — |")
$lines.Add("| **Grand total (all tracked files)** | **$allFiles** | **$allTotal** | **$allNonBlank** | — | — |")
$lines.Add('')
$lines.Add('Excluded content is not hand-written project code: vendored and third-party trees, dependency directories, translation/data catalogs, binary/generated payloads, build/output trees, caches, and lockfiles are shown for transparency and excluded from the project total.')

$report = $lines -join [Environment]::NewLine
if (-not [string]::IsNullOrWhiteSpace($OutputMarkdown)) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputMarkdown)) {
        [System.IO.Path]::GetFullPath($OutputMarkdown)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot $OutputMarkdown))
    }
    $outputParent = Split-Path -Parent $outputPath
    New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
    Set-Content -LiteralPath $outputPath -Value $report -Encoding utf8
}
Write-Output $report

[Console]::OutputEncoding = $script:previousConsoleEncoding
