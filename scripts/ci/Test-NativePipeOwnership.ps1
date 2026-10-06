[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
$definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-LoggedNativeCommand' }, $true)
Invoke-Expression $definition.Extent.Text
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('BambuPipeOwnership-' + [guid]::NewGuid())
$originalReuse = [Environment]::GetEnvironmentVariable('MSBUILDDISABLENODEREUSE', 'Process')
$transcript = Join-Path $fixture 'transcript.txt'
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    $exe = Join-Path $fixture 'pipe fixture.exe'
    Add-Type -OutputAssembly $exe -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Threading;
public class PipeOwnershipFixture {
    public static void Main(string[] args) {
        if (args.Length > 0) { Thread.Sleep(2500); return; }
        string disabled = Environment.GetEnvironmentVariable("MSBUILDDISABLENODEREUSE");
        Console.WriteLine("NODE_REUSE_DISABLED=" + disabled);
        if (disabled != "1") {
            var info = new ProcessStartInfo(Process.GetCurrentProcess().MainModule.FileName, "child");
            info.UseShellExecute = false;
            info.CreateNoWindow = true;
            Process.Start(info);
        }
        Console.Error.WriteLine("PARENT_EXIT=7");
        Environment.Exit(7);
    }
}
'@
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.EnvironmentVariables['MSBUILDDISABLENODEREUSE'] = '0'
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    try {
        [void] $process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        Assert-True ($process.WaitForExit(1500) -and $process.ExitCode -eq 7) 'The fixture parent must exit independently of its descendant.'
        Assert-True (-not $stdout.IsCompleted -and -not $stderr.IsCompleted) 'Inherited descendant handles must keep both pipes open after parent exit.'
        $stdout.Wait()
        $stderr.Wait()
    } finally { $process.Dispose() }
    [Environment]::SetEnvironmentVariable('MSBUILDDISABLENODEREUSE', '0', 'Process')
    Start-Transcript -LiteralPath $transcript | Out-Null
    try { Invoke-LoggedNativeCommand -FilePath $exe -Arguments @() }
    finally { Stop-Transcript | Out-Null }
    Assert-True ($LASTEXITCODE -eq 7) 'Disabling reuse must preserve the actual direct-child exit code.'
    Assert-True ((Get-Content $transcript -Raw).Contains('NODE_REUSE_DISABLED=1')) 'The producer child must receive the no-reuse setting.'
    Assert-True ([Environment]::GetEnvironmentVariable('MSBUILDDISABLENODEREUSE', 'Process') -eq '0') 'The caller environment must remain unchanged.'
} finally {
    [Environment]::SetEnvironmentVariable('MSBUILDDISABLENODEREUSE', $originalReuse, 'Process')
    $resolved = [IO.Path]::GetFullPath($fixture)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary root.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Native pipe ownership checks passed (5 assertions; self-expiring inherited-handle fixture only).'
