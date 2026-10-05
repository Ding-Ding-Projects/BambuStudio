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
# Bind script parameters explicitly. An array of strings cannot reproduce
# named-parameter binding when splatted into an advanced PowerShell script.
$parameterNames = @{
    BuildMode = 'value'; OutputDirectory = 'value'; DependencyCacheDirectory = 'value'
    ReleaseNumber = 'value'; PreviousPackageVersion = 'value'
    Install = 'switch'; BootstrapOnly = 'switch'; Plan = 'switch'; BuildOnly = 'switch'
}
$producerParameters = @{}
for ($index = 0; $index -lt $originalArguments.Count; $index++) {
    $argument = $originalArguments[$index]
    if ($argument -in @('/s', '--silent')) { continue }
    if ($argument -notmatch '^-(\w+)(?::(.*))?$') {
        throw "Unsupported build argument '$argument'. Use the named producer parameters."
    }
    $name = $Matches[1]
    $inlineValue = if ($Matches.ContainsKey(2)) { $Matches[2] } else { $null }
    if (-not $parameterNames.ContainsKey($name) -or $producerParameters.ContainsKey($name)) {
        throw "Unknown or duplicate build parameter '$name'."
    }
    if ($parameterNames[$name] -eq 'switch') {
        $value = $true
        if ($null -ne $inlineValue) {
            if ($inlineValue -match '^\$?true$') { $value = $true }
            elseif ($inlineValue -match '^\$?false$') { $value = $false }
            else { throw "Build switch '$name' requires true or false." }
        }
    } else {
        if ($null -ne $inlineValue) { $value = $inlineValue }
        else {
            $index++
            if ($index -ge $originalArguments.Count) { throw "Build parameter '$name' requires a value." }
            $value = $originalArguments[$index]
        }
    }
    $producerParameters[$name] = $value
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$planOnly = $producerParameters.ContainsKey('Plan') -and $producerParameters['Plan']
if (-not $planOnly -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    # Serialize the complete argument array as data. The elevated host calls
    # the original entry point with an array, never an interpolated cmd /c string.
    $handoff = @{ entry = $entry; arguments = $originalArguments } | ConvertTo-Json -Compress
    $encodedData = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($handoff))
    $command = '$handoff = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(' +
        "'" + $encodedData + "'" + ')) | ConvertFrom-Json; $entryArguments = @($handoff.arguments); & $handoff.entry @entryArguments; exit $LASTEXITCODE'
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    # Windows PowerShell stores its process-only override here. Reading it
    # avoids optional Security-module autoload and never changes persistent policy.
    $processPolicy = [Environment]::GetEnvironmentVariable('PSExecutionPolicyPreference', 'Process')
    $policyArguments = ''
    if (-not [string]::IsNullOrWhiteSpace($processPolicy)) {
        $knownPolicies = @('AllSigned', 'Bypass', 'RemoteSigned', 'Restricted', 'Unrestricted', 'Undefined', 'Default')
        if ($processPolicy -notin $knownPolicies) {
            throw 'The process execution-policy override is not a recognized value.'
        }
        if ($processPolicy -ne 'Undefined') { $policyArguments = ' -ExecutionPolicy ' + $processPolicy }
    }
    try {
        Write-Host 'Administrator approval is required before the build bootstrap starts.'
        $hostExecutable = Join-Path $PSHOME 'powershell.exe'
        if (-not [IO.File]::Exists($hostExecutable)) { $hostExecutable = Join-Path $PSHOME 'pwsh.exe' }
        $child = Start-Process -FilePath $hostExecutable -Verb RunAs -WindowStyle Hidden -PassThru `
            -ArgumentList ('-NoLogo -NoProfile' + $policyArguments + ' -EncodedCommand ' + $encodedCommand)
        try {
            # Pin the process handle before waiting. Start-Process -Wait uses
            # descendant-job completion, which can include idle MSBuild servers.
            # WaitForExit binds only to this exact elevated entry-point host.
            $null = $child.Handle
            $child.WaitForExit()
            $child.Refresh()
            $childExitCode = $child.ExitCode
        } finally {
            $child.Dispose()
        }
        exit $childExitCode
    } catch {
        Write-Host ('Administrator launch did not complete: ' + $_.Exception.Message)
        exit 1223
    }
}
# Elevated re-entry reaches this producer branch exactly once. The root build
# route stays build-only; both installer launchers retain packaging behavior.
if ([IO.Path]::GetFileName($entry) -ieq 'build.bat') {
    $producerParameters['BuildOnly'] = $true
}
& (Join-Path $PSScriptRoot 'Invoke-OneClickBuild.ps1') @producerParameters
if ($?) { exit 0 } else { exit 1 }
