# Dot-sourced only by the hosted scale worker. Uses its existing ownership and
# contained-process functions. No UIA mutation, activation, scrolling or input.
function Save-HostedScaleDiagnostic([ValidateSet('before_selector','expanded_selector')][string] $Phase) {
    if (-not $DiagnosticEvidence -or $NativeRuntime -or $Mode -cne 'run' -or $script:DiagnosticResults.ContainsKey($Phase)) { return }
    $script:DiagnosticResults[$Phase] = 'unavailable'
    $png = Join-Path $env:RUNNER_TEMP ('scale-diagnostic-' + [Guid]::NewGuid().ToString('N') + '.png')
    $plain = $null; $key = $null; $rsa = $null
    try {
        if ((Test-UncertainChildren) -or (Test-UncertainInput)) { throw 'Diagnostic containment unavailable.' }
        $git = (Get-Command git -CommandType Application).Source
        $sourceRead = Invoke-BoundedProcess $git @('-C',$PSScriptRoot,'rev-parse','HEAD') 5 $true
        $source = $sourceRead.stdout.Trim()
        if (-not $sourceRead.terminated -or $sourceRead.code -ne 0 -or $source -cnotmatch '^[0-9a-f]{40}$' -or
            $env:GITHUB_RUN_ID -cnotmatch '^\d{1,20}$') { throw 'Diagnostic source unavailable.' }
        $rows = @(Read-Controls)
        $state = Read-Scale $rows
        $root = [ScaleNative]::GetAncestor([IntPtr]$state.combo.top.Current.NativeWindowHandle,2)
        function Read-DiagnosticOwner {
            foreach ($processId in $allowed) {
                $live = Get-Process -Id $processId
                if ($live.SessionId -ne $session -or $live.StartTime.ToUniversalTime().Ticks -ne $processStarts[$processId]) {
                    throw 'Diagnostic process identity changed.'
                }
            }
            [uint32]$owner = 0
            $thread = [ScaleNative]::GetWindowThreadProcessId($root,[ref]$owner)
            if ($root -eq [IntPtr]::Zero -or $allowed -notcontains [int]$owner -or
                (Desktop-Name $thread) -cne 'Default' -or -not [ScaleNative]::IsWindowVisible($root)) { throw 'Diagnostic root unavailable.' }
            $bounds = [ScaleNative+RECT]::new()
            if (-not [ScaleNative]::GetWindowRect($root,[ref]$bounds) -or
                $bounds.Right -le $bounds.Left -or $bounds.Bottom -le $bounds.Top) { throw 'Diagnostic bounds unavailable.' }
            return @($bounds.Left,$bounds.Top,$bounds.Right,$bounds.Bottom)
        }
        $bounds = Read-DiagnosticOwner
        $observedAt = [DateTime]::UtcNow.ToString('o')
        $foregroundOwned = $false
        $rect = $state.combo.element.Current.BoundingRectangle
        try {
            Assert-HostedForeground $root ([int][Math]::Floor($rect.X+$rect.Width/2)) ([int][Math]::Floor($rect.Y+$rect.Height/2))
            $foregroundOwned = $true
        } catch {} # Observation only, never activate an obscured window.
        $controls = [Collections.Generic.List[object]]::new()
        foreach ($row in $rows) {
            if (-not $row.top.Equals($state.combo.top)) { continue }
            $current = $row.element.Current
            if ($current.ProcessId -ne $settingsId) { throw 'Diagnostic control owner changed.' }
            if ($current.Name.Length -gt 2048 -or $current.AutomationId.Length -gt 2048) { throw 'Diagnostic label exceeds bounds.' }
            $r = $current.BoundingRectangle
            $patterns = @($row.element.GetSupportedPatterns() | ForEach-Object ProgrammaticName)
            if ($patterns.Count -gt 32) { throw 'Diagnostic patterns exceed bounds.' }
            $scroll = $null
            try {
                $pattern = $row.element.GetCurrentPattern([Windows.Automation.ScrollPattern]::Pattern)
                $s = $pattern.Current
                $scroll = @{horizontal=$s.HorizontallyScrollable; vertical=$s.VerticallyScrollable
                    horizontal_percent=$s.HorizontalScrollPercent; vertical_percent=$s.VerticalScrollPercent
                    horizontal_view=$s.HorizontalViewSize; vertical_view=$s.VerticalViewSize}
            } catch {}
            $controls.Add(@{name=$current.Name; automation_id=$current.AutomationId; type=$current.ControlType.ProgrammaticName
                enabled=$current.IsEnabled; offscreen=$current.IsOffscreen; rect=@($r.X,$r.Y,$r.Width,$r.Height)
                patterns=$patterns; scroll=$scroll})
            if ($controls.Count -gt 1000) { throw 'Diagnostic inventory exceeds bounds.' }
        }
        # Exact HWND capture, not a whole-desktop image. Refresh its
        # owner immediately before and after the contained cheap invocation.
        if (((Read-DiagnosticOwner) -join ',') -cne ($bounds -join ',')) { throw 'Diagnostic frame changed.' }
        $capturedAt = [DateTime]::UtcNow.ToString('o')
        $capture = Invoke-BoundedProcess $CheapExecutable @('screenshot','--hwnd',"$($root.ToInt64())",'--output_path',$png) 15 $true
        if (-not $capture.terminated -or $capture.code -ne 0) { throw 'Diagnostic capture unavailable.' }
        $reply = $capture.stdout | ConvertFrom-Json
        if ($reply.ok -ne $true -or $reply.rendered_ok -ne $true -or
            ((Read-DiagnosticOwner) -join ',') -cne ($bounds -join ',')) { throw 'Diagnostic render or ownership unavailable.' }
        $file = Get-Item -LiteralPath $png
        if ($file.Length -le 24 -or $file.Length -gt 8388608 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Diagnostic PNG exceeds bounds.' }
        $pixels = [IO.File]::ReadAllBytes($png)
        if ([Convert]::ToHexString($pixels[0..7]) -cne '89504E470D0A1A0A' -or
            [Text.Encoding]::ASCII.GetString($pixels,12,4) -cne 'IHDR') { throw 'Diagnostic PNG invalid.' }
        $width = [uint32]$pixels[16]*16777216 + [uint32]$pixels[17]*65536 + [uint32]$pixels[18]*256 + $pixels[19]
        $height = [uint32]$pixels[20]*16777216 + [uint32]$pixels[21]*65536 + [uint32]$pixels[22]*256 + $pixels[23]
        if ($width -ne ($bounds[2]-$bounds[0]) -or $height -ne ($bounds[3]-$bounds[1]) -or
            $width -gt 8192 -or $height -gt 8192) { throw 'Diagnostic capture dimensions changed.' }
        $binding = [ordered]@{protocol='hosted-scale-diagnostic-v1'; run_id=$env:GITHUB_RUN_ID; source_commit=$source; phase=$Phase
            captured_at_utc=$capturedAt; width=$width; height=$height; capture_method='cheap_exact_hwnd'
            png_sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($pixels)).ToLowerInvariant()
            cheap_sha256=(Get-FileHash -LiteralPath $CheapExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
            helper_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'Invoke-HostedDisplayScale.ps1') -Algorithm SHA256).Hash.ToLowerInvariant()
            diagnostic_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'Save-HostedScaleDiagnostic.ps1') -Algorithm SHA256).Hash.ToLowerInvariant()}
        $plain = [Text.Encoding]::UTF8.GetBytes((@{schema=1; binding=$binding; observed_at_utc=$observedAt
            settings_pid=$settingsId; settings_start=$settingsStart; owned_root=$root.ToInt64(); bounds=$bounds
            foreground_owned=$foregroundOwned; settings_dpi=$state.dpi; selected_percent=$state.percent
            controls=$controls.ToArray(); png_base64=[Convert]::ToBase64String($pixels)} | ConvertTo-Json -Depth 10 -Compress))
        if ($plain.Length -gt 16777216) { throw 'Diagnostic envelope exceeds bounds.' }
        $aad = [Text.Encoding]::UTF8.GetBytes(($binding | ConvertTo-Json -Compress))
        $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
        $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $tag = [byte[]]::new(16); $cipher = [byte[]]::new($plain.Length)
        $rsa = [Security.Cryptography.RSA]::Create()
        $rsa.ImportFromPem([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../md3/hosted-automation-public-v1.pem')))
        $wrapped = $rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [Security.Cryptography.AesGcm]::new($key,16)
        try { $aes.Encrypt($nonce,$plain,$cipher,$tag,$aad) } finally { $aes.Dispose() }
        [IO.File]::WriteAllBytes((Join-Path $output ($Phase + '.aesgcm')),$cipher)
        @{schema=1; protocol='hosted-scale-diagnostic-v1'; recipient='hosted-automation-public-v1.pem'
            recipient_sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo())).ToLowerInvariant()
            aad_base64=[Convert]::ToBase64String($aad); binding=$binding
            wrapped_key=[Convert]::ToBase64String($wrapped); nonce=[Convert]::ToBase64String($nonce); tag=[Convert]::ToBase64String($tag)
            ciphertext_sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($cipher)).ToLowerInvariant()
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output ($Phase + '.envelope.json')) -Encoding utf8
        $script:DiagnosticResults[$Phase] = 'encrypted_pending_pixel_review'
    } catch {
        $script:DiagnosticResults[$Phase] = 'unavailable'
    } finally {
        if ($null -ne $plain) { [Array]::Clear($plain,0,$plain.Length) }
        if ($null -ne $key) { [Array]::Clear($key,0,$key.Length) }
        if ($null -ne $rsa) { $rsa.Dispose() }
        # Never touch a file while an unverified child might still own it.
        if (-not (Test-UncertainChildren) -and (Test-Path -LiteralPath $png)) { Remove-Item -LiteralPath $png }
    }
}
