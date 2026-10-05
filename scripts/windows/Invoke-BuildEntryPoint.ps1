# Keep this a simple script: unknown build switches remain literal $args.
param([string] $EntryPoint)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$entry = [IO.Path]::GetFullPath($EntryPoint)
$allowed = @('build.bat', 'build-installer.bat', 'OneClickBuildInstaller.cmd') |
    ForEach-Object { [IO.Path]::Combine($root, $_) }
if ($allowed -notcontains $entry -or -not [IO.File]::Exists($entry)) {
    throw 'Elevation is limited to the supported repository entry points.'
}
$originalArguments = @($args | ForEach-Object { [string]$_ })
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$planOnly = $originalArguments -contains '-Plan'
if (-not $planOnly -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    # Serialize the complete argument array as data. The elevated host calls
    # the original entry point with an array, never an interpolated cmd /c string.
    $handoff = @{ entry = $entry; arguments = $originalArguments } | ConvertTo-Json -Compress
    $encodedData = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($handoff))
    $command = '$handoff = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(' +
        "'" + $encodedData + "'" + ')) | ConvertFrom-Json; $entryArguments = @($handoff.arguments); & $handoff.entry @entryArguments; exit $LASTEXITCODE'
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $processPolicy = Get-ExecutionPolicy -Scope Process
    $policyArguments = if ($processPolicy -eq 'Undefined') { '' } else { ' -ExecutionPolicy ' + [string]$processPolicy }
    try {
        Write-Host 'Administrator approval is required before the build bootstrap starts.'
        $hostExecutable = Join-Path $PSHOME 'powershell.exe'
        if (-not [IO.File]::Exists($hostExecutable)) { $hostExecutable = Join-Path $PSHOME 'pwsh.exe' }
        $child = Start-Process -FilePath $hostExecutable -Verb RunAs -WindowStyle Hidden -Wait -PassThru `
            -ArgumentList ('-NoLogo -NoProfile' + $policyArguments + ' -EncodedCommand ' + $encodedCommand)
        exit $child.ExitCode
    } catch {
        Write-Host ('Administrator launch did not complete: ' + $_.Exception.Message)
        exit 1223
    }
}
# Elevated re-entry reaches this producer branch exactly once. The root build
# route stays build-only; both installer launchers retain packaging behavior.
$buildArguments = @($originalArguments | Where-Object { $_ -notin @('/s', '--silent') })
if ([IO.Path]::GetFileName($entry) -ieq 'build.bat') {
    & (Join-Path $PSScriptRoot 'Invoke-OneClickBuild.ps1') -BuildOnly @buildArguments
} else {
    & (Join-Path $PSScriptRoot 'Invoke-OneClickBuild.ps1') @buildArguments
}
if ($?) { exit 0 } else { exit 1 }
