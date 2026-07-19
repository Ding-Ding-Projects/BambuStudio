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

if (-not $CiExecutionApproved -or $env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This script executes the unsigned native application and is restricted to an explicitly approved disposable GitHub-hosted runner.'
}

$runnerTemp = (Resolve-Path -LiteralPath $env:RUNNER_TEMP).Path.TrimEnd('\')
$testRoot = Join-Path $runnerTemp ('BambuStudioMD3-NativeVisual-' + [guid]::NewGuid().ToString('N'))
$resolvedApplication = (Resolve-Path -LiteralPath $Application).Path
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir).TrimEnd('\')
$preferenceRegistry = 'HKCU:\Software\codingmachineedge\BambuStudioMD3Preferences'
$themeRegistry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'

if (Test-Path -LiteralPath $preferenceRegistry) {
    throw 'Refusing to overwrite a pre-existing Bambu Studio MD3 language preference on the runner.'
}
if (-not $resolvedOutputDir.StartsWith($runnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Native visual evidence must be written beneath RUNNER_TEMP.'
}

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class BambuStudioNativeCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int command);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
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
    Assert-True ($width -ge 400 -and $height -ge 200) "Native window is unexpectedly small: ${width}x${height}."

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
        for ($x = 0; $x -lt $width; $x += 16) {
            for ($y = 0; $y -lt $height; $y += 16) {
                [void] $colors.Add($bitmap.GetPixel($x, $y).ToArgb())
            }
        }
        Assert-True ($colors.Count -ge 12) "Native capture appears blank or uniform ($($colors.Count) sampled colors)."
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }

    Assert-True ((Get-Item -LiteralPath $Path).Length -ge 10KB) "Native capture '$Path' is unexpectedly small."
}

$themePropertyExisted = $false
$originalThemeValue = $null
if (Test-Path -LiteralPath $themeRegistry) {
    $themeProperty = Get-ItemProperty -LiteralPath $themeRegistry -Name AppsUseLightTheme -ErrorAction SilentlyContinue
    if ($null -ne $themeProperty) {
        $themePropertyExisted = $true
        $originalThemeValue = $themeProperty.AppsUseLightTheme
    }
}

$scenarios = @(
    [pscustomobject]@{ Name = 'light-en'; Mode = 'en'; Light = 1 },
    [pscustomobject]@{ Name = 'dark-yue_HK'; Mode = 'yue_HK'; Light = 0 },
    [pscustomobject]@{ Name = 'light-bilingual'; Mode = 'bilingual_en_yue_HK'; Light = 1 }
)

New-Item -ItemType Directory -Path $testRoot | Out-Null
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

try {
    foreach ($scenario in $scenarios) {
        New-Item -ItemType Directory -Path $themeRegistry -Force | Out-Null
        New-ItemProperty -LiteralPath $themeRegistry -Name AppsUseLightTheme -PropertyType DWord -Value $scenario.Light -Force | Out-Null
        New-Item -ItemType Directory -Path $preferenceRegistry -Force | Out-Null
        New-ItemProperty -LiteralPath $preferenceRegistry -Name LanguageMode -PropertyType String -Value $scenario.Mode -Force | Out-Null

        $dataDir = Join-Path $testRoot $scenario.Name
        New-Item -ItemType Directory -Path $dataDir | Out-Null
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $resolvedApplication
        $startInfo.UseShellExecute = $false
        $startInfo.ArgumentList.Add('--datadir')
        $startInfo.ArgumentList.Add($dataDir)
        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        Assert-True ($process.Start()) "Could not start the native application for '$($scenario.Name)'."

        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(120)
            $handle = [IntPtr]::Zero
            while ([DateTime]::UtcNow -lt $deadline -and -not $process.HasExited) {
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                $handle = $process.MainWindowHandle
                if ($handle -ne [IntPtr]::Zero) { break }
            }
            Assert-True (-not $process.HasExited) "Native application exited before presenting '$($scenario.Name)'."
            Assert-True ($handle -ne [IntPtr]::Zero) "Native application did not present a window for '$($scenario.Name)'."

            [void] [BambuStudioNativeCapture]::ShowWindow($handle, 9)
            [void] [BambuStudioNativeCapture]::SetForegroundWindow($handle)
            Start-Sleep -Seconds 5
            $process.Refresh()
            if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
                $handle = $process.MainWindowHandle
            }
            $capturePath = Join-Path $resolvedOutputDir ($scenario.Name + '.png')
            Save-WindowCapture -Handle $handle -Path $capturePath
            Write-Host "Captured native $($scenario.Mode) visual smoke evidence at '$capturePath'."
        }
        finally {
            if (-not $process.HasExited) {
                [void] $process.CloseMainWindow()
                if (-not $process.WaitForExit(5000)) {
                    $process.Kill($true)
                    $process.WaitForExit()
                }
            }
            $process.Dispose()
        }
    }
}
finally {
    if (Test-Path -LiteralPath $preferenceRegistry) {
        Remove-Item -LiteralPath $preferenceRegistry -Recurse -Force
    }
    if ($themePropertyExisted) {
        New-ItemProperty -LiteralPath $themeRegistry -Name AppsUseLightTheme -PropertyType DWord -Value $originalThemeValue -Force | Out-Null
    } else {
        Remove-ItemProperty -LiteralPath $themeRegistry -Name AppsUseLightTheme -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
        Assert-True ($resolvedTestRoot.StartsWith($runnerTemp + '\', [System.StringComparison]::OrdinalIgnoreCase)) 'Refusing to clean a native-smoke path outside RUNNER_TEMP.'
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}

Write-Host 'Native Windows light/dark/language visual smoke passed.'
