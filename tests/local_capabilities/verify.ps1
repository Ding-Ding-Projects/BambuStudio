$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project = Join-Path $PSScriptRoot 'LocalCapabilities.Tests.csproj'
& dotnet run --project $project
if ($LASTEXITCODE -ne 0) { throw 'Baseline boundary tests failed.' }

# Mutate disposable copies, never the active production source or another checkout.
$target = Join-Path $root ('artifacts/local-capabilities-negative/' + [Guid]::NewGuid().ToString('N'))
foreach ($relative in @('automation/BambuAutomation/LocalCapabilities', 'tests/local_capabilities')) {
    $directory = Join-Path $target $relative
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $root $relative) -File |
        Where-Object { $_.Extension -in '.cs', '.csproj' } |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $directory }
}
foreach ($name in @('NativeBridge.cs','Contracts.cs')) {
    Copy-Item -LiteralPath (Join-Path $root "automation/BambuAutomation/$name") -Destination (Join-Path $target 'automation/BambuAutomation')
}
$source = Join-Path $target 'automation/BambuAutomation/LocalCapabilities/LocalCapabilityHost.cs'
$original = [IO.File]::ReadAllText($source)
$needle = 'context.Request.Host.Value != authority'
if (($original.Split($needle).Length - 1) -ne 1) { throw 'Host boundary mutation target is not unique.' }
[IO.File]::WriteAllText($source, $original.Replace($needle, 'false'))
& dotnet run --project (Join-Path $target 'tests/local_capabilities/LocalCapabilities.Tests.csproj')
$negative = $LASTEXITCODE
if ($negative -eq 0) { throw 'Host boundary removal was not detected.' }
[IO.File]::WriteAllText($source, $original)
& dotnet run --project (Join-Path $target 'tests/local_capabilities/LocalCapabilities.Tests.csproj')
if ($LASTEXITCODE -ne 0) { throw 'Restored boundary did not pass.' }
Write-Output 'Host boundary removal: rejected; restored boundary: passed.'
