# Hosted display-scale feasibility probe

`scripts/ci/Probe-HostedDisplayScale.ps1` is a read-only discovery step, not a
scale setter or a passing matrix test. It runs only on disposable GitHub-hosted
Windows workers and writes beneath `RUNNER_TEMP`. No local execution was used to
prepare it. Hosted execution and private inventory review remain pending.

The invoking workflow must first open the real Display Settings page in its
owned interactive session and desktop using its supported hosted UI route.
The probe does not launch Settings, alter the display, select a control, sign
out, or restart anything. It requires one `SystemSettings` process in its session
and one accessible top-level window on its current desktop. Missing or ambiguous
surfaces return a fixed `unavailable` reason, rather than guessing selectors.

Run with PowerShell 7.4 or newer:

```powershell
pwsh -NoProfile -File scripts/ci/Probe-HostedDisplayScale.ps1 -OutputDirectory "$env:RUNNER_TEMP/display-scale-probe"
```

The parent limits the isolated UIA worker to 45 seconds by default (5 to 90
seconds configurable), then terminates only that owned worker. At most 1,000
control records are retained. This is a bounded inventory, not completeness
evidence. The probe always exits 2 because no scale was provisioned. Publish
`capability.json` as the fixed public summary. UIA names and automation IDs stay
inside `settings-inventory.json.aesgcm`, encrypted with AES-256-GCM and the existing
`hosted-gui-public-v2.pem` RSA recipient using OAEP-SHA256. The separate envelope
records the nonce, authentication tag, wrapped key and ciphertext SHA-256; it uses
no associated data. Raw provider text and exception messages are never logged.
Use the existing private recipient custody route to decrypt and review the
inventory. The existing image-archive decoder is not compatible with this JSON
envelope without an explicit format adapter. Never upload decrypted inventory.

Always read the separate parent-owned `supervisor.json` first. It records actual
worker exit observation and `teardown_verified`. Spawn, wait and termination
exceptions produce fixed reason codes without raw diagnostics. A stop request
alone never proves termination: the parent waits up to another five seconds.
If termination cannot be observed, `completed` and `inventory_stable` remain
false, `disposal_required` is true, and the worker-owned files must not be treated
as final evidence. The parent never overwrites `capability.json`, preventing a
receipt race with a worker still running. Dispose of the hosted machine through
its normal job lifecycle in that case; no successful teardown is claimed. A
missing supervisor receipt also makes all inventory unavailable for acceptance.

## Next provisioning step

After private review identifies actual controls, implement a separate bounded
Settings interaction using the predefined Scale selector. Record the original
selection and display identity, select each supported value, and require actual
`GetDpiForWindow` readings of 96, 120, 144 and 192 for 100%, 125%, 150% and 200%.
Record target DPI awareness as well: an unaware window returns 96 regardless of
monitor scaling. Restore the original selection in `finally` and verify its DPI.
Disposable worker destruction is the final containment boundary, not a claimed
successful restoration. Unsupported selectors or values remain unavailable.

Do not use custom scaling, registry substitutions, fake `WM_DPICHANGED`, or an
awareness override as evidence of provisioned monitor scale. The current native
driver checks requested scale against measured DPI but does not configure it.

Microsoft documents the [Settings scale selector](https://support.microsoft.com/en-gb/accessibility/windows/make-text-and-apps-bigger)
and [dynamic per-monitor DPI behavior](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows).
[GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow)
is awareness-dependent. [ChangeDisplaySettingsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-changedisplaysettingsexa)
documents graphics-mode changes, not a monitor-scale setter.

## Minimum-size evidence

The current `GUI_App::get_min_size()` returns
`max(1000, 76 * em_unit)` by `max(600, 49 * em_unit)`. `MainFrame` applies it using
`SetMinSize`, an outer-frame constraint. The driver's fixed 1000 by 600 physical
client rectangle is a different quantity and cannot be called the contractual
minimum. Measure the actual em-dependent outer minimum and resulting client
dimensions at each language, theme and DPI tuple. Verify a smaller requested
frame clamps to that minimum and inspect the resulting content. Retain fixed
client sizes as additional achievable tuples. This probe changes neither the
product minimum nor the driver and supplies no layout verdict.
