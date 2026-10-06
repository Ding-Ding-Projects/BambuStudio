[CmdletBinding()]
param(
    [string] $CheckerPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $CheckerPath) { $CheckerPath = Join-Path $PSScriptRoot 'Test-LanguageModes.ps1' }

# Execute the production report reader and completeness branch, without running
# unrelated compilers or web checks. Parse the actual statements rather than
# copying their implementation into this regression.
$parseTokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path -LiteralPath $CheckerPath).Path, [ref] $parseTokens, [ref] $parseErrors)
if ($parseErrors.Count -ne 0) { throw 'The language checker has parse errors.' }
$reader = @($ast.FindAll({ param($node)
    $node -is [System.Management.Automation.Language.AssignmentStatementAst] -and
    $node.Left.Extent.Text -ceq '$missing'
}, $true))
$decision = @($ast.FindAll({ param($node)
    $node -is [System.Management.Automation.Language.IfStatementAst] -and
    $node.Clauses[0].Item1.Extent.Text -ceq '$missing.Count -gt 0'
}, $true))
$assertion = @($ast.FindAll({ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -ceq 'Assert-True'
}, $true))
if ($reader.Count -ne 1 -or $decision.Count -ne 1 -or $assertion.Count -ne 1) {
    throw 'Expected exactly one production report reader, decision and assertion.'
}
$production = [scriptblock]::Create(
    $assertion[0].Extent.Text + "`n" + $reader[0].Extent.Text + "`n" +
    $decision[0].Extent.Text + "`n" + '$missing.Count')

$scratch = Join-Path ([IO.Path]::GetTempPath()) ('language-missing-report-' + [guid]::NewGuid().ToString('N'))
[void] [IO.Directory]::CreateDirectory($scratch)
try {
    $cases = @(
        @{ Name = 'empty'; Json = '[]'; Count = 0 },
        @{ Name = 'single'; Json = '[{"msgid":"first missing translation"}]'; Count = 1 },
        @{ Name = 'multiple'; Json = '[{"msgid":"first missing translation"},{"msgid":"second missing translation"}]'; Count = 2 }
    )
    foreach ($case in $cases) {
        $missingReport = Join-Path $scratch ($case.Name + '.json')
        [IO.File]::WriteAllText($missingReport, $case.Json)
        $requireComplete = $true
        $failure = $null
        $actualCount = $null
        try { $actualCount = & $production }
        catch { $failure = $_ }
        if ($case.Count -eq 0) {
            if ($null -ne $failure) { throw "Empty report must pass: $($failure.Exception.Message)" }
            if ($actualCount -ne 0) { throw "Empty report returned count $actualCount." }
        }
        else {
            $expected = "$($case.Count) English source messages have no Cantonese entry (first: 'first missing translation')."
            if ($null -eq $failure -or $failure.Exception.Message -cne $expected) {
                throw "Case '$($case.Name)' must reject missing translations with the exact count and first msgid. Actual: $failure"
            }
        }
        Write-Host "PASS $($case.Name): expected count $($case.Count), strict result preserved."
    }
    Write-Host "Passed 3 production missing-report cases on PowerShell $($PSVersionTable.PSVersion)."
}
finally {
    Remove-Item -LiteralPath $scratch -Recurse -Force
}
