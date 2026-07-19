[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$nativeHeader = Join-Path $repoRoot 'src\slic3r\GUI\LanguageMode.hpp'
$nativePo = Join-Path $repoRoot 'bbl\i18n\yue_HK\BambuStudio_yue_HK.po'
$nativeCoverage = Join-Path $repoRoot 'bbl\i18n\yue_HK\coverage.json'
$nativeCompiler = Join-Path $repoRoot 'bbl\i18n\yue_HK\compile_translation.py'
$nativeMo = Join-Path $repoRoot 'resources\i18n\yue_HK\BambuStudio.mo'
$nativeApp = Join-Path $repoRoot 'src\slic3r\GUI\GUI_App.cpp'
$webLogin = Join-Path $repoRoot 'src\slic3r\GUI\WebUserLoginDialog.cpp'
$webPanel = Join-Path $repoRoot 'src\slic3r\GUI\WebViewDialog.cpp'
$deviceRoot = Join-Path $repoRoot 'src\slic3r\GUI\DeviceWeb\device_page'
$legacyValidator = Join-Path $repoRoot 'resources\web\data\validate-text-locales.mjs'
$uiMd3Test = Join-Path $repoRoot 'ui-md3\tests\i18n.test.mjs'

function Assert-True {
    param(
        [Parameter(Mandatory)]
        [bool] $Condition,

        [Parameter(Mandatory)]
        [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string] $Command,

        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $FailureMessage
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit code $LASTEXITCODE)."
    }
}

function Get-InterpolationTokens {
    param([Parameter(Mandatory)][string] $Value)

    return @([regex]::Matches($Value, '\{\{\s*[^{}]+\s*\}\}') |
        ForEach-Object { $_.Value } |
        Sort-Object)
}

Write-Host 'Checking canonical language-mode identifiers...'
Assert-True (Test-Path -LiteralPath $nativeHeader) "Missing native language-mode header '$nativeHeader'."
$nativeHeaderText = Get-Content -LiteralPath $nativeHeader -Raw
$canonicalConstants = [ordered]@{
    LANGUAGE_MODE_ENGLISH = 'en'
    LANGUAGE_MODE_ENGLISH_US = 'en_US'
    LANGUAGE_MODE_CANTONESE_HONG_KONG = 'yue_HK'
    LANGUAGE_MODE_ENGLISH_CANTONESE_HK = 'bilingual_en_yue_HK'
}
foreach ($constant in $canonicalConstants.GetEnumerator()) {
    $pattern = '(?m)^\s*inline\s+constexpr\s+const\s+char\s*\*\s*' +
        [regex]::Escape($constant.Key) + '\s*=\s*"' + [regex]::Escape($constant.Value) + '";\s*$'
    Assert-True ([regex]::IsMatch($nativeHeaderText, $pattern)) `
        "Native language mode '$($constant.Key)' must have canonical ID '$($constant.Value)'."
}

$nativeAppText = Get-Content -LiteralPath $nativeApp -Raw
Assert-True ($nativeAppText.Contains('custom_language_mode || I18N::is_baseline_language_mode(requested_mode_id)')) `
    'Baseline English no longer remains on its canonical language-mode profile.'
$webLoginText = Get-Content -LiteralPath $webLogin -Raw
Assert-True ($webLoginText.Contains('return into_u8(wxGetApp().current_language_code_safe());')) `
    'Remote login routing does not use the service-safe language code.'
$webPanelText = Get-Content -LiteralPath $webPanel -Raw
Assert-True ($webPanelText.Contains('return into_u8(wxGetApp().current_local_web_language());')) `
    'Local embedded pages do not use the explicit local-web language route.'
Assert-True (([regex]::Matches($webPanelText, 'current_language_code_safe\(\)\.BeforeFirst\(')).Count -ge 4) `
    'One or more remote MakerWorld routes do not use the service-safe language prefix.'

$deviceI18nPath = Join-Path $deviceRoot 'src\i18n.tsx'
$deviceI18n = Get-Content -LiteralPath $deviceI18nPath -Raw
Assert-True ($deviceI18n -match '(?m)^\s*yue_HK:\s*\{\s*translation:\s*yue_HK\s*\},\s*$') `
    'DeviceWeb does not register canonical yue_HK resources.'
Assert-True ($deviceI18n -match '(?m)^\s*bilingual_en_yue_HK:\s*\{\s*translation:\s*bilingual_en_yue_HK\s*\},\s*$') `
    'DeviceWeb does not register canonical bilingual_en_yue_HK resources.'

$legacyText = Get-Content -LiteralPath (Join-Path $repoRoot 'resources\web\data\text.js') -Raw
Assert-True ($legacyText -match '(?m)^\s*"yue_HK":\s*\{\s*$') `
    'Legacy web resources do not expose canonical yue_HK.'
Assert-True ($legacyText.Contains('strLang==="bilingual_en_yue_HK"')) `
    'Legacy web resources do not expose canonical bilingual_en_yue_HK.'

$uiMd3Resources = Get-Content -LiteralPath (Join-Path $repoRoot 'ui-md3\app\i18n.resources.js') -Raw
$uiModeIds = @([regex]::Matches($uiMd3Resources, "\{\s*id:\s*'([^']+)'\s*,\s*label:") |
    ForEach-Object { $_.Groups[1].Value })
$requiredUiModeIds = @('en', 'yue_HK', 'bilingual_en_yue_HK')
Assert-True (($uiModeIds -join "`n") -ceq ($requiredUiModeIds -join "`n")) `
    "ui-md3 must expose exactly the canonical mode IDs: $($requiredUiModeIds -join ', ')."

Write-Host 'Checking native PO/MO catalog presence and reproducibility...'
foreach ($requiredPath in @($nativePo, $nativeCoverage, $nativeCompiler, $nativeMo)) {
    Assert-True (Test-Path -LiteralPath $requiredPath -PathType Leaf) "Missing language resource '$requiredPath'."
}
$coverage = Get-Content -LiteralPath $nativeCoverage -Raw | ConvertFrom-Json
Assert-True ($coverage.locale -ceq 'yue_HK') 'Native coverage locale must be yue_HK.'
Assert-True ([int] $coverage.translated_messages -ge 150) 'Native Cantonese coverage must contain at least 150 reviewed messages.'
Assert-True ((@($coverage.categories.PSObject.Properties.Value | Measure-Object -Sum).Sum) -eq [int] $coverage.translated_messages) `
    'Native category counts do not add up to translated_messages.'

$moBytes = [System.IO.File]::ReadAllBytes($nativeMo)
Assert-True ($moBytes.Length -gt 28) 'Native Cantonese MO is too small to be a valid catalog.'
$gnuMoMagic = [Convert]::ToUInt32('950412DE', 16)
Assert-True ([BitConverter]::ToUInt32($moBytes, 0) -eq $gnuMoMagic) `
    'Native Cantonese MO does not have the GNU MO little-endian magic value.'

$pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
if ($null -ne $pythonLauncher) {
    Invoke-Checked -Command $pythonLauncher.Source -Arguments @('-3', $nativeCompiler, '--check') `
        -FailureMessage 'Native Cantonese PO/MO validation failed'
}
else {
    $pythonLauncher = Get-Command python -ErrorAction SilentlyContinue
    Assert-True ($null -ne $pythonLauncher) 'Python 3 is required to validate the native PO/MO catalog.'
    Invoke-Checked -Command $pythonLauncher.Source -Arguments @($nativeCompiler, '--check') `
        -FailureMessage 'Native Cantonese PO/MO validation failed'
}

Write-Host 'Checking DeviceWeb resource counts, parity and placeholders...'
$deviceEnglish = Get-Content -LiteralPath (Join-Path $deviceRoot 'locales\en.json') -Raw | ConvertFrom-Json
$deviceCantonese = Get-Content -LiteralPath (Join-Path $deviceRoot 'locales\yue_HK.json') -Raw | ConvertFrom-Json
$englishKeys = @($deviceEnglish.PSObject.Properties.Name | Sort-Object)
$cantoneseKeys = @($deviceCantonese.PSObject.Properties.Name | Sort-Object)
Assert-True ($englishKeys.Count -eq 178) "Expected 178 DeviceWeb English resources, found $($englishKeys.Count)."
Assert-True (($englishKeys -join "`n") -ceq ($cantoneseKeys -join "`n")) `
    'DeviceWeb yue_HK keys do not exactly match the English resource keys.'

foreach ($key in $englishKeys) {
    $englishValue = [string] $deviceEnglish.PSObject.Properties[$key].Value
    $cantoneseValue = [string] $deviceCantonese.PSObject.Properties[$key].Value
    Assert-True (-not [string]::IsNullOrWhiteSpace($cantoneseValue)) "Empty DeviceWeb yue_HK value: '$key'."
    $englishTokens = @(Get-InterpolationTokens -Value $englishValue)
    $cantoneseTokens = @(Get-InterpolationTokens -Value $cantoneseValue)
    Assert-True (($englishTokens -join "`n") -ceq ($cantoneseTokens -join "`n")) `
        "DeviceWeb placeholder mismatch for '$key'."
}

$node = Get-Command node -ErrorAction SilentlyContinue
Assert-True ($null -ne $node) 'Node.js is required to validate local web language modes.'
Invoke-Checked -Command $node.Source `
    -Arguments @('--experimental-strip-types', '--no-warnings', (Join-Path $deviceRoot 'tests\i18nResources.test.ts')) `
    -FailureMessage 'DeviceWeb language resource test failed'

Write-Host 'Checking legacy web key parity, markup and bilingual accessibility...'
Invoke-Checked -Command $node.Source -Arguments @($legacyValidator) `
    -FailureMessage 'Legacy web language resource test failed'

Write-Host 'Checking ui-md3 language behavior and interpolation...'
Invoke-Checked -Command $node.Source -Arguments @('--test', $uiMd3Test) `
    -FailureMessage 'ui-md3 language mode test failed'

Write-Host "Language mode validation passed: $($coverage.translated_messages) native, $($englishKeys.Count) DeviceWeb and 168 legacy web translations."
