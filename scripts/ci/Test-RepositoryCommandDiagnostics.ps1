[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
foreach ($name in @('Invoke-RepositoryCommand', 'Assert-LastExitCode')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    Invoke-Expression $definition.Extent.Text
}
function Write-BuildLog { param($Message) Write-Host $Message }
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$Plan = $false
$transcript = Join-Path ([IO.Path]::GetTempPath()) ('BambuProducerDiagnostics-' + [guid]::NewGuid() + '.txt')
try {
    Start-Transcript -LiteralPath $transcript | Out-Null
    try {
        Invoke-RepositoryCommand -Label 'fixture native success' -Command { & $env:ComSpec /d /c 'echo PRODUCER_NATIVE_STDOUT_MARKER & exit /b 0' }
        $rejected = $false
        try { Invoke-RepositoryCommand -Label 'fixture native failure' -Command { & $env:ComSpec /d /c 'echo PRODUCER_FAILURE_STDOUT_MARKER & exit /b 7' } } catch { $rejected = $_.Exception.Message.Contains('exit code 7') }
        Assert-True $rejected 'The exact failing native exit code must propagate.'
        $rejected = $false
        try { Invoke-RepositoryCommand -Label 'fixture PowerShell exception' -Command { throw 'PRODUCER_POWERSHELL_EXCEPTION' } } catch { $rejected = $_.Exception.Message.Contains('PRODUCER_POWERSHELL_EXCEPTION') }
        Assert-True $rejected 'A terminating PowerShell exception must propagate.'
    } finally { Stop-Transcript | Out-Null }
    $log = Get-Content $transcript -Raw
    Assert-True ($log.Contains('PRODUCER_NATIVE_STDOUT_MARKER')) 'Successful native stdout must reach the transcript.'
    Assert-True ($log.Contains('PRODUCER_FAILURE_STDOUT_MARKER')) 'Failing native stdout must reach the transcript.'
} finally { Remove-Item -LiteralPath $transcript -Force -ErrorAction SilentlyContinue }
Write-Host 'Repository command diagnostics checks passed (4 assertions; native echo/exit and PowerShell exception fixtures only).'
