[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '', [switch] $LegacyNativeInvocation)
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
foreach ($name in @('Invoke-RepositoryCommand', 'Invoke-LoggedNativeCommand', 'Assert-LastExitCode')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    Invoke-Expression $definition.Extent.Text
}
function Invoke-FixtureNative([string] $Marker, [int] $ExitCode) {
    $child = '[Console]::Out.WriteLine("' + $Marker + '_STDOUT_MARKER"); [Console]::Error.WriteLine("' + $Marker + '_STDERR_MARKER"); exit ' + $ExitCode
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($child))
    Invoke-LoggedNativeCommand -FilePath 'powershell.exe' -Arguments @('-NoLogo', '-NoProfile', '-EncodedCommand', $encoded)
}
if ($LegacyNativeInvocation) {
    function Invoke-LoggedNativeCommand {
        param([string] $FilePath, [string[]] $Arguments)
        & $FilePath @Arguments | Out-Host
    }
}
function Write-BuildLog { param($Message) Write-Host $Message }
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$Plan = $false
$transcript = Join-Path ([IO.Path]::GetTempPath()) ('BambuProducerDiagnostics-' + [guid]::NewGuid() + '.txt')
$fixtureDirectory = Join-Path ([IO.Path]::GetTempPath()) ('Bambu native arguments ' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path $fixtureDirectory | Out-Null
    $fixtureExecutable = Join-Path $fixtureDirectory 'argument fixture.exe'
    Add-Type -TypeDefinition 'public class NativeArgumentFixture { public static void Main(string[] args) { foreach (string arg in args) System.Console.WriteLine("ARG:" + System.Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(arg))); } }' -OutputAssembly $fixtureExecutable -OutputType ConsoleApplication
    Start-Transcript -LiteralPath $transcript | Out-Null
    try {
        Invoke-RepositoryCommand -Label 'fixture native success' -Command { Invoke-FixtureNative 'PRODUCER_NATIVE' 0 }
        $rejected = $false
        try { Invoke-RepositoryCommand -Label 'fixture native failure' -Command { Invoke-FixtureNative 'PRODUCER_FAILURE' 7 } } catch { $rejected = $_.Exception.Message.Contains('exit code 7') }
        Assert-True $rejected 'The exact failing native exit code must propagate.'
        $rejected = $false
        try { Invoke-RepositoryCommand -Label 'fixture PowerShell exception' -Command { throw 'PRODUCER_POWERSHELL_EXCEPTION' } } catch { $rejected = $_.Exception.Message.Contains('PRODUCER_POWERSHELL_EXCEPTION') }
        Assert-True $rejected 'A terminating PowerShell exception must propagate.'
        $child = 'for ($i=0; $i -lt 1500; $i++) { [Console]::Out.WriteLine("OUT_"+$i+"_"+("x"*128)); [Console]::Error.WriteLine("ERR_"+$i+"_"+("y"*128)) }; exit 0'
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($child))
        Invoke-RepositoryCommand -Label 'fixture concurrent pipes beyond pipe capacity' -Command { Invoke-LoggedNativeCommand -FilePath 'powershell.exe' -Arguments @('-NoLogo', '-NoProfile', '-EncodedCommand', $encoded) }
        Assert-True ($ErrorActionPreference -eq 'Stop') 'Native invocation must retain the caller error preference.'
        $argumentValues = @('', 'path with spaces\', 'embedded"quote', ('Cantonese ' + [char]0x7cb5 + [char]0x8a9e))
        Invoke-RepositoryCommand -Label 'fixture native argument transport' -Command { Invoke-LoggedNativeCommand -FilePath $fixtureExecutable -Arguments $argumentValues }
    } finally { Stop-Transcript | Out-Null }
    $log = Get-Content $transcript -Raw
    Assert-True ($log.Contains('PRODUCER_NATIVE_STDOUT_MARKER')) 'Successful native stdout must reach the transcript.'
    Assert-True ($log.Contains('PRODUCER_FAILURE_STDOUT_MARKER')) 'Failing native stdout must reach the transcript.'
    Assert-True ($log.Contains('PRODUCER_NATIVE_STDERR_MARKER')) 'Successful native stderr must reach the transcript without becoming a PowerShell exception.'
    Assert-True ($log.Contains('PRODUCER_FAILURE_STDERR_MARKER')) 'Failing native stderr must reach the transcript before the exact exit failure.'
    Assert-True ($log.Contains('OUT_1499_') -and $log.Contains('ERR_1499_')) 'Both streams must drain beyond pipe capacity without deadlock or lost final lines.'
    foreach ($value in $argumentValues) {
        Assert-True ($log.Contains('ARG:' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($value)))) 'Native argument values must survive spaces, empty strings, quotes, trailing backslashes, and Unicode.'
    }
} finally {
    Remove-Item -LiteralPath $transcript -Force -ErrorAction SilentlyContinue
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureDirectory)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary directory.' }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Repository command diagnostics checks passed (12 assertions; dual native streams, exit status, pipe capacity, argument transport, and PowerShell exception fixtures only).'
