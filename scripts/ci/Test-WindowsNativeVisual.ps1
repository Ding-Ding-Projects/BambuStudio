[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Application,

    [Parameter(Mandatory)]
    [string] $OutputDir,

    [Parameter(Mandatory)]
    [switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $CiExecutionApproved -or $env:CI -ne 'true' -or $env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This script executes the unsigned native application and is restricted to an explicitly approved disposable GitHub-hosted runner.'
}
if ([string]::IsNullOrWhiteSpace($env:RUNNER_TEMP) -or -not (Test-Path -LiteralPath $env:RUNNER_TEMP -PathType Container)) {
    throw 'RUNNER_TEMP must identify an existing GitHub-hosted runner directory.'
}
if ([string]::IsNullOrWhiteSpace($env:GITHUB_WORKSPACE) -or -not (Test-Path -LiteralPath $env:GITHUB_WORKSPACE -PathType Container)) {
    throw 'GITHUB_WORKSPACE must identify the checked-out repository on the GitHub-hosted runner.'
}

$runnerTemp = (Resolve-Path -LiteralPath $env:RUNNER_TEMP).Path.TrimEnd('\')
$githubWorkspace = (Resolve-Path -LiteralPath $env:GITHUB_WORKSPACE).Path.TrimEnd('\')
$testRoot = Join-Path $runnerTemp ('BambuStudioMD3-NativeVisual-' + [guid]::NewGuid().ToString('N'))
$resolvedApplication = (Resolve-Path -LiteralPath $Application).Path
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir).TrimEnd('\')

if ([System.IO.Path]::GetFileName($resolvedApplication) -ine 'bambu-studio.exe') {
    throw "Native visual smoke requires the built BambuStudio executable, not '$resolvedApplication'."
}
if (-not $resolvedApplication.StartsWith($githubWorkspace + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Native visual smoke requires an application built beneath GITHUB_WORKSPACE.'
}
$applicationResources = Join-Path ([System.IO.Path]::GetDirectoryName($resolvedApplication)) 'resources'
if (-not (Test-Path -LiteralPath $applicationResources -PathType Container)) {
    throw "The built BambuStudio resources directory is missing beside '$resolvedApplication'."
}
if (-not $resolvedOutputDir.StartsWith($runnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Native visual evidence must be written beneath RUNNER_TEMP.'
}
if (Test-Path -LiteralPath $resolvedOutputDir) {
    $existingEvidence = @(Get-ChildItem -LiteralPath $resolvedOutputDir -Force)
    if ($existingEvidence.Count -ne 0) {
        throw "Refusing to mix native visual evidence with $($existingEvidence.Count) pre-existing item(s)."
    }
}

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class BambuStudioNativeCapture
{
    private const uint WM_GETTEXT = 0x000D;
    private const uint WM_GETTEXTLENGTH = 0x000E;
    private const uint SMTO_ABORTIFHUNG = 0x0002;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    private delegate bool EnumWindowProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int command);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int GetWindowTextLengthW(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int GetWindowTextW(IntPtr hWnd, StringBuilder value, int capacity);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr SendMessageTimeoutW(
        IntPtr hWnd,
        uint message,
        UIntPtr wParam,
        IntPtr lParam,
        uint flags,
        uint timeout,
        out UIntPtr result);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr SendMessageTimeoutW(
        IntPtr hWnd,
        uint message,
        UIntPtr wParam,
        StringBuilder lParam,
        uint flags,
        uint timeout,
        out UIntPtr result);

    public static string ReadWindowText(IntPtr hWnd)
    {
        UIntPtr messageResult;
        int length = 0;
        if (SendMessageTimeoutW(
                hWnd,
                WM_GETTEXTLENGTH,
                UIntPtr.Zero,
                IntPtr.Zero,
                SMTO_ABORTIFHUNG,
                2000,
                out messageResult) != IntPtr.Zero) {
            ulong rawLength = messageResult.ToUInt64();
            if (rawLength <= 16384)
                length = (int)rawLength;
        }
        if (length <= 0)
            length = GetWindowTextLengthW(hWnd);
        if (length <= 0)
            return String.Empty;

        StringBuilder value = new StringBuilder(length + 1);
        if (SendMessageTimeoutW(
                hWnd,
                WM_GETTEXT,
                new UIntPtr((uint)value.Capacity),
                value,
                SMTO_ABORTIFHUNG,
                2000,
                out messageResult) == IntPtr.Zero) {
            GetWindowTextW(hWnd, value, value.Capacity);
        }
        return value.ToString();
    }

    public static string[] ReadDescendantWindowTexts(IntPtr parent)
    {
        List<string> values = new List<string>();
        EnumWindowProc callback = delegate(IntPtr child, IntPtr ignored) {
            string value = ReadWindowText(child);
            if (!String.IsNullOrWhiteSpace(value))
                values.Add(value);
            return true;
        };
        EnumChildWindows(parent, callback, IntPtr.Zero);
        GC.KeepAlive(callback);
        return values.ToArray();
    }
}
'@

function Assert-True {
    param([Parameter(Mandatory)][bool] $Condition, [Parameter(Mandatory)][string] $Message)
    if (-not $Condition) { throw $Message }
}

function Save-WindowCapture {
    param(
        [Parameter(Mandatory)][IntPtr] $Handle,
        [Parameter(Mandatory)][string] $Path
    )

    $rect = [BambuStudioNativeCapture+RECT]::new()
    Assert-True ([BambuStudioNativeCapture]::GetWindowRect($Handle, [ref] $rect)) 'Could not read the native window bounds.'
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    Assert-True ($width -ge 800 -and $height -ge 500) "Native visual smoke window is unexpectedly small: ${width}x${height}."

    $bitmap = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $hdc = $graphics.GetHdc()
            try {
                $printed = [BambuStudioNativeCapture]::PrintWindow($Handle, $hdc, 2)
            }
            finally {
                $graphics.ReleaseHdc($hdc)
            }
            if (-not $printed) {
                $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, [System.Drawing.Size]::new($width, $height))
            }
        }
        finally {
            $graphics.Dispose()
        }

        $colors = [System.Collections.Generic.HashSet[int]]::new()
        [double] $luminanceTotal = 0
        [double] $luminanceSquaredTotal = 0
        [long] $sampleCount = 0
        $sampleStep = [Math]::Max(4, [Math]::Floor([Math]::Min($width, $height) / 140))
        $left = [Math]::Floor($width * 0.04)
        $right = [Math]::Ceiling($width * 0.96)
        $top = [Math]::Floor($height * 0.06)
        $bottom = [Math]::Ceiling($height * 0.96)
        for ($x = $left; $x -lt $right; $x += $sampleStep) {
            for ($y = $top; $y -lt $bottom; $y += $sampleStep) {
                $pixel = $bitmap.GetPixel($x, $y)
                [void] $colors.Add($pixel.ToArgb())
                $luminance = (0.2126 * $pixel.R) + (0.7152 * $pixel.G) + (0.0722 * $pixel.B)
                $luminanceTotal += $luminance
                $luminanceSquaredTotal += ($luminance * $luminance)
                $sampleCount++
            }
        }
        Assert-True ($sampleCount -ge 10000) "Native capture supplied only $sampleCount interior samples."
        Assert-True ($colors.Count -ge 12) "Native capture appears blank or uniform ($($colors.Count) sampled colors)."

        $meanLuminance = $luminanceTotal / $sampleCount
        $variance = [Math]::Max(0, ($luminanceSquaredTotal / $sampleCount) - ($meanLuminance * $meanLuminance))
        $luminanceStdDev = [Math]::Sqrt($variance)
        Assert-True ($luminanceStdDev -ge 18) "Native capture lacks surface contrast (luminance standard deviation $([Math]::Round($luminanceStdDev, 2)))."

        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }

    $captureFile = Get-Item -LiteralPath $Path
    Assert-True ($captureFile.Length -ge 10KB) "Native capture '$Path' is unexpectedly small."
    $captureHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()

    [pscustomobject]@{
        Width = $width
        Height = $height
        UniqueSampledColors = $colors.Count
        MeanLuminance = [Math]::Round($meanLuminance, 3)
        LuminanceStdDev = [Math]::Round($luminanceStdDev, 3)
        Sha256 = $captureHash
        Bytes = $captureFile.Length
    }
}

function Get-SampledImageDifference {
    param(
        [Parameter(Mandatory)][string] $First,
        [Parameter(Mandatory)][string] $Second
    )

    $firstBitmap = [System.Drawing.Bitmap]::new($First)
    $secondBitmap = [System.Drawing.Bitmap]::new($Second)
    try {
        Assert-True ($firstBitmap.Width -eq $secondBitmap.Width -and $firstBitmap.Height -eq $secondBitmap.Height) 'Native evidence images have inconsistent dimensions.'
        [double] $differenceTotal = 0
        [long] $sampleCount = 0
        for ($x = 0; $x -lt $firstBitmap.Width; $x += 6) {
            for ($y = 0; $y -lt $firstBitmap.Height; $y += 6) {
                $firstPixel = $firstBitmap.GetPixel($x, $y)
                $secondPixel = $secondBitmap.GetPixel($x, $y)
                $differenceTotal += ([Math]::Abs([int] $firstPixel.R - [int] $secondPixel.R) +
                    [Math]::Abs([int] $firstPixel.G - [int] $secondPixel.G) +
                    [Math]::Abs([int] $firstPixel.B - [int] $secondPixel.B)) / 3
                $sampleCount++
            }
        }
        return [Math]::Round($differenceTotal / $sampleCount, 3)
    }
    finally {
        $firstBitmap.Dispose()
        $secondBitmap.Dispose()
    }
}

$scenarios = @(
    [pscustomobject]@{
        Name = 'light-en'
        Mode = 'en'
        Theme = 'light'
        Title = 'Bambu Studio MD3 Native Visual Smoke [light-en] | English'
        Headline = 'Prepare, Slice, Print'
        SupportingText = 'English interface mode'
        Action = 'Start a new project'
        RequiresCjk = $false
    },
    [pscustomobject]@{
        Name = 'dark-yue_HK'
        Mode = 'yue_HK'
        Theme = 'dark'
        Title = 'Bambu Studio MD3 Native Visual Smoke [dark-yue_HK] | 廣東話（香港）'
        Headline = '準備、切片、列印'
        SupportingText = '廣東話（香港）介面模式'
        Action = '開始新專案'
        RequiresCjk = $true
    },
    [pscustomobject]@{
        Name = 'light-bilingual'
        Mode = 'bilingual_en_yue_HK'
        Theme = 'light'
        Title = 'Bambu Studio MD3 Native Visual Smoke [light-bilingual] | English + 廣東話（香港）'
        Headline = 'Prepare, Slice, Print · 準備、切片、列印'
        SupportingText = 'English + 廣東話（香港） compact bilingual mode'
        Action = 'New project · 新專案'
        RequiresCjk = $true
    }
)

New-Item -ItemType Directory -Path $testRoot | Out-Null
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null
$results = @()
$comparisons = @()

try {
    foreach ($scenario in $scenarios) {
        $dataDir = Join-Path $testRoot $scenario.Name
        New-Item -ItemType Directory -Path $dataDir | Out-Null

        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $resolvedApplication
        $startInfo.WorkingDirectory = [System.IO.Path]::GetDirectoryName($resolvedApplication)
        $startInfo.UseShellExecute = $false
        $startInfo.ArgumentList.Add('--datadir')
        $startInfo.ArgumentList.Add($dataDir)
        $startInfo.Environment['BAMBU_STUDIO_CI_NATIVE_VISUAL_SMOKE'] = "v1/$($scenario.Name)"

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        $processStarted = $false

        try {
            $processStarted = $process.Start()
            Assert-True $processStarted "Could not start the native application for '$($scenario.Name)'."

            $deadline = [DateTime]::UtcNow.AddSeconds(120)
            $handle = [IntPtr]::Zero
            $title = ''
            while ([DateTime]::UtcNow -lt $deadline -and -not $process.HasExited) {
                Start-Sleep -Milliseconds 250
                $process.Refresh()
                $handle = $process.MainWindowHandle
                if ($handle -ne [IntPtr]::Zero) {
                    $title = [BambuStudioNativeCapture]::ReadWindowText($handle)
                    if ($title -eq $scenario.Title) { break }
                }
            }
            Assert-True (-not $process.HasExited) "Native application exited before presenting '$($scenario.Name)'."
            Assert-True ($handle -ne [IntPtr]::Zero) "Native application did not present a window for '$($scenario.Name)'."
            Assert-True ($title -eq $scenario.Title) "Captured an unexpected native window for '$($scenario.Name)': '$title'."

            [void] [BambuStudioNativeCapture]::ShowWindow($handle, 9)
            [void] [BambuStudioNativeCapture]::SetForegroundWindow($handle)

            $textDeadline = [DateTime]::UtcNow.AddSeconds(30)
            $descendantTexts = @()
            do {
                $descendantTexts = @([BambuStudioNativeCapture]::ReadDescendantWindowTexts($handle))
                if ($descendantTexts -ccontains 'BAMBU_STUDIO_NATIVE_VISUAL_SMOKE_V1') { break }
                Start-Sleep -Milliseconds 200
            } while ([DateTime]::UtcNow -lt $textDeadline -and -not $process.HasExited)

            $expectedTexts = @(
                'BAMBU_STUDIO_NATIVE_VISUAL_SMOKE_V1',
                "SCENARIO=$($scenario.Name)",
                "LANGUAGE_MODE=$($scenario.Mode)",
                "THEME=$($scenario.Theme)",
                $scenario.Headline,
                $scenario.SupportingText,
                $scenario.Action
            )
            foreach ($expectedText in $expectedTexts) {
                Assert-True ($descendantTexts -ccontains $expectedText) "Native '$($scenario.Name)' window did not expose expected text '$expectedText'."
            }
            foreach ($otherScenario in $scenarios) {
                if ($otherScenario.Name -ne $scenario.Name) {
                    Assert-True (-not ($descendantTexts -ccontains "SCENARIO=$($otherScenario.Name)")) "Native '$($scenario.Name)' window exposed another scenario identity."
                }
            }

            $allWindowText = $scenario.Title + "`n" + ($descendantTexts -join "`n")
            $containsCjk = [regex]::IsMatch($allWindowText, '[\u3400-\u4DBF\u4E00-\u9FFF]')
            Assert-True ($containsCjk -eq $scenario.RequiresCjk) "Native '$($scenario.Name)' CJK text presence did not match the scenario contract."

            $capturePath = Join-Path $resolvedOutputDir ($scenario.Name + '.png')
            $metrics = Save-WindowCapture -Handle $handle -Path $capturePath
            if ($scenario.Theme -eq 'dark') {
                Assert-True ($metrics.MeanLuminance -lt 115) "Dark scenario mean luminance $($metrics.MeanLuminance) is too bright."
            } else {
                Assert-True ($metrics.MeanLuminance -gt 145) "Light scenario mean luminance $($metrics.MeanLuminance) is too dark."
            }

            $evidence = [ordered]@{
                schema = 'bambu-studio-native-visual-evidence/v1'
                scenario = $scenario.Name
                languageMode = $scenario.Mode
                theme = $scenario.Theme
                windowTitle = $title
                expectedTexts = $expectedTexts
                observedTexts = $descendantTexts
                cjkPresent = $containsCjk
                image = [ordered]@{
                    file = [System.IO.Path]::GetFileName($capturePath)
                    width = $metrics.Width
                    height = $metrics.Height
                    bytes = $metrics.Bytes
                    sha256 = $metrics.Sha256
                    uniqueSampledColors = $metrics.UniqueSampledColors
                    meanLuminance = $metrics.MeanLuminance
                    luminanceStdDev = $metrics.LuminanceStdDev
                }
            }
            $metadataPath = Join-Path $resolvedOutputDir ($scenario.Name + '.json')
            $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metadataPath -Encoding utf8NoBOM

            $results += [pscustomobject]@{
                Scenario = $scenario.Name
                Theme = $scenario.Theme
                Capture = $capturePath
                Metadata = $metadataPath
                Metrics = $metrics
            }
            Write-Host "Verified native '$($scenario.Name)' identity, text, and visual evidence at '$capturePath'."
        }
        finally {
            if ($processStarted -and -not $process.HasExited) {
                [void] $process.CloseMainWindow()
                if (-not $process.WaitForExit(5000)) {
                    $process.Kill($true)
                    $process.WaitForExit()
                }
            }
            $process.Dispose()
        }
    }

    Assert-True ($results.Count -eq $scenarios.Count) "Expected $($scenarios.Count) native visual results, found $($results.Count)."
    $uniqueHashes = @($results.Metrics.Sha256 | Sort-Object -Unique)
    Assert-True ($uniqueHashes.Count -eq $scenarios.Count) 'Native language/theme scenarios produced duplicate screenshots.'

    $darkResult = $results | Where-Object Scenario -eq 'dark-yue_HK'
    $lightResults = @($results | Where-Object Theme -eq 'light')
    foreach ($lightResult in $lightResults) {
        $luminanceGap = $lightResult.Metrics.MeanLuminance - $darkResult.Metrics.MeanLuminance
        Assert-True ($luminanceGap -ge 55) "Light/dark luminance separation is only $([Math]::Round($luminanceGap, 3))."
    }

    for ($firstIndex = 0; $firstIndex -lt $results.Count; $firstIndex++) {
        for ($secondIndex = $firstIndex + 1; $secondIndex -lt $results.Count; $secondIndex++) {
            $difference = Get-SampledImageDifference -First $results[$firstIndex].Capture -Second $results[$secondIndex].Capture
            Assert-True ($difference -ge 7) "Native '$($results[$firstIndex].Scenario)' and '$($results[$secondIndex].Scenario)' evidence differs by only $difference RGB levels."
            $comparisons += [ordered]@{
                first = $results[$firstIndex].Scenario
                second = $results[$secondIndex].Scenario
                meanAbsoluteRgbDifference = $difference
            }
            Write-Host "Perceptual sample difference $($results[$firstIndex].Scenario) -> $($results[$secondIndex].Scenario): $difference."
        }
    }

    $summary = [ordered]@{
        schema = 'bambu-studio-native-visual-summary/v1'
        commit = $env:GITHUB_SHA
        scenarios = @($results | ForEach-Object {
            [ordered]@{
                scenario = $_.Scenario
                theme = $_.Theme
                image = [System.IO.Path]::GetFileName($_.Capture)
                metadata = [System.IO.Path]::GetFileName($_.Metadata)
                sha256 = $_.Metrics.Sha256
                meanLuminance = $_.Metrics.MeanLuminance
                luminanceStdDev = $_.Metrics.LuminanceStdDev
            }
        })
        comparisons = $comparisons
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $resolvedOutputDir 'summary.json') -Encoding utf8NoBOM
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
        Assert-True ($resolvedTestRoot.StartsWith($runnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) 'Refusing to clean a native-smoke path outside RUNNER_TEMP.'
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}

Write-Host 'Deterministic native Windows MD3 light/dark/language visual smoke passed.'
