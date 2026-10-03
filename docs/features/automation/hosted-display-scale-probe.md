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
`hosted-automation-public-v1.pem` RSA recipient using OAEP-SHA256. The separate envelope
records the nonce, authentication tag, wrapped key and ciphertext SHA-256; it uses
no associated data. Raw provider text and exception messages are never logged.
The envelope uses schema 2 and protocol `display-scale-inventory-v2` to distinguish
this inventory from the native-runtime encryption protocol and the earlier
schema-1 GUI-recipient inventories. The existing recipient is reused without
rotation or replacement. Before dispatch, verify that its matching protected
private custody is available. Earlier inventories remain preserved with their
original recipients; changing this probe does not make them decryptable.
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

### Measured minimum driver mode

The native driver now accepts `--viewport measured-minimum`. It reads the actual
main frame's existing layout-probe `GetMinSize` value, sizes the outer frame to
that value, and retains its actual client dimensions and measured DPI. It does
not modify the product constraint, infer an em value, or call a fixed 1000 by 600
client rectangle the minimum. Each invocation and process restart adds a
`viewport_observations` receipt.

It also requests an outer frame one physical pixel smaller in both dimensions
using the existing `SetWindowPos` route, records the resulting geometry, restores
the original measured minimum in `finally`, and captures the restored surface.
The minimum receipt references the immediately following existing ready or
restart main-frame capture, without adding a duplicate image. The menu scope
retains exactly 30 images: one ready observation, five outer menu interactions,
and twelve interactions for each of the root and nested menu checks. The driver
limit remains 30, fitting the wrapper's 32-entry manifest with `runtime.json`
and the installation receipt. No per-input image is removed.
Only an exact return to the measured minimum earns `observed_programmatic_clamp`.
Native interactive minimum tracking and programmatic resizing are not equivalent:
if the programmatic request is not clamped, the diagnostic records `not_observed`
without failing a correctly measured minimum tuple. Interactive verification
remains pending. Even a programmatic clamp leaves
`interactive_resize_clamp` explicitly `unverified`; it does not prove mouse-edge
or keyboard resizing. Do not loosen the product minimum to make this probe pass.

On the configured hosted Windows job, after installed-artifact validation:

```powershell
python scripts/md3/drive-native-interface.py --exe $exe --cli $cli --install-receipt $receipt --source-commit $commit --release-tag $tag --output $output --scope slice-controls --viewport measured-minimum --language en --theme light --scale 1.0
```

Repeat only on actually provisioned DPI and independently requested language and
theme tuples. This source change was not executed locally. Hosted results,
encrypted pixel review and interactive clamp evidence remain pending.

### Dedicated interactive minimum scope, hosted verification pending

The opt-in `minimum-resize` scope requires `measured-minimum` at 100% scale.
Higher scales remain excluded until this baseline route has hosted evidence.
The verifier contains the complete driver tree in a named non-breakaway Job
with a 900-second deadline. The existing scopes keep their hidden-only route.

This scope explicitly uses the pinned backend's compatibility MCP stdio server,
through the official installed MCP 1.x client (at least 1.2.0). This is not a
persistent mode of the cheap CLI. One server retains the original Default input
desktop handle and safety banner across show/hide. The real pointer drag runs
through the existing cheap CLI in the existing worker on the owned desktop.
No direct sizing command or fabricated nonclient input message is used.

Before input, the observer requires the same process/start identity, HWND,
thread/input desktop, foreground frame, DPI, unobscured pointer path and a fresh
`HTBOTTOMRIGHT` result from bounded read-only hit testing. The frame must start
exactly 80 pixels above its measured outer minimum in both dimensions and must
not be maximized. One diagonal drag targets 40 pixels below the minimum. Success
requires real shrinkage, unchanged top-left and minimum, exact final outer size,
96 DPI, and observed mouse-button release. Before/after images remain encrypted
and require independent pixel review. This isolated scope adds only two images
to its ready image, leaving the menu scope's capture budget unchanged.

The original frame and input desktop are restored before the persistent server
closes, and its exit is checked. Unknown button release blocks restoration input
and desktop switching; the invocation fails and requires host disposal. Complete
Job termination alone never substitutes for input-release or desktop-restoration
evidence. This source implementation has not been executed locally or hosted.

## Bounded predefined-scale provisioning

`scripts/ci/Invoke-HostedDisplayScale.ps1` is a separate hosted-only helper. It
requires an already opened Display Settings surface for standalone use and the existing pinned
`lowlevel-computer-use-cheap` executable. UI Automation only reads the observed
`SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox`, its selected value,
and supported predefined items. Default input uses HWND-targeted cheap mouse clicks,
after live process, session, desktop, enabled-state and geometry checks. Missing
or ambiguous targets produce an unavailable result without speculative input.

```powershell
& scripts/ci/Invoke-HostedDisplayScale.ps1 -ScalePercent 125 -OutputDirectory "$env:RUNNER_TEMP/scale-125" -CheapExecutable $env:LLCU_CHEAP
```

Allowed percentages are 100, 125, 150 and 200. The helper saves the observed
original selection before input, requires both the requested selection and
`GetDpiForWindow` on the existing Settings frame, and restores the original
selection and DPI in `finally`. A separate bounded recovery worker checks
restoration after the first worker terminates. An unverified termination or
restoration requires disposal of the hosted runner, never a successful result.
The helper's thread uses per-monitor awareness for coordinate conversion only;
this does not provision a scale and is never accepted as scale evidence.

The default helper supports standalone control measurement. It has no general
action-script parameter. The explicit native-runtime integration below uses only
the fixed installed-product driver after the requested selection and Settings DPI
are verified in the current invocation. Standalone measurement proves no product
window DPI, pixels, layout or capture review.

Every helper process starts suspended and is assigned to a non-breakaway Windows
job before its first instruction. A zero active-process count proves termination
of the whole owned tree; direct process exit alone does not. A durable pending
marker precedes each spawn and is renamed only after that proof. Any remaining
pending marker blocks further input, restoration and recovery, and requires
runner disposal. The job has kill-on-close semantics as a containment backstop,
not a substitute for a termination verdict. Normal worker output and all standard
error go to NUL. Only cheap JSON output is captured, capped at 64 KiB, with a
bounded process deadline and a one-second final drain. No unbounded stream read
or generic child-action output is retained.
The pipe reader exclusively owns a `SafeFileHandle` until its task finishes,
including when the bounded drain returns unavailable. Process teardown never
closes a raw pipe handle underneath a queued or active read. Extended startup
attributes restrict inheritance to the intended standard-stream handles.

Only `supervisor.json`, `run.json` and `restore.json` are public-safe receipts.
Never upload `original.json`: it is private recovery state containing process and
session identities. Internal `child-*.pending*` lifecycle markers are also excluded
from publication. The helper forwards no raw child output or UI labels. A source
syntax parse is not hosted execution; standalone provisioning remains unverified
until its selection, native DPI and restoration receipts pass independent review.

Selection diagnostics retain a fixed failure-stage name independently from the
restoration failure-stage name. The helper observes the combo's read-only
`ExpandCollapsePattern` for at most three seconds after input and requires an
expanded state before resolving an option. Fixed scalar observations record the
input target category, cheap exit and termination verdict, expansion state and
matching option count. Successful restoration cannot replace those observations.
These receipts distinguish an input acknowledgement from an observed transition;
they do not establish that posted mouse messages changed Display Settings.

The explicit `-InputRoute hosted-foreground` option instead uses the existing
cheap absolute-coordinate mouse input on a wholly disposable GitHub-hosted
Windows machine. It is unavailable on a local or personal desktop. It requires
the actual input desktop and helper thread to be `WinSta0\Default`, Settings to
already own the foreground root, and the target point to resolve to the same
owned root and process. Live process start identities and fresh enabled UIA
geometry are rechecked before input; foreground and point ownership are checked
again afterward. No window is automatically activated. Expansion and selected
value/DPI observations are still mandatory, and restoration repeats the same
ownership checks. An uncertain child termination still prohibits restoration.

This foreground route is deliberately non-atomic: another foreground change
between the final check and the separate cheap CLI's input remains possible.
The complete disposable-host scope, not an atomic HWND guarantee, bounds that
limitation. Receipts explicitly record the route and `foreground_input_atomic`
as false. A changed desktop, obscured point, missing foreground ownership or
unobserved semantic transition produces unavailable evidence, never success.

Post-input ownership uses a fresh observation of the unique Scale combo's current
physical rectangle, retaining the original Settings process identity and owned
root. A bounded three-second read-only convergence accommodates legitimate layout
movement after DPI changes; it never reuses the selected option's obsolete point
or sends another input. Expanded-option diagnostics publish only counts within
the predefined 100/125/150/200 domain: observed, visible and enabled, matching
selection container, and unavailable container observations. A zero count does
not establish that a scale is unsupported; virtualization and display limits
still require their own evidence.

## Opt-in native-runtime integration, verification pending

`run-scaled-native-interface.py` is a fixed adapter for the existing
native driver. Its bounded request rejects unknown and duplicate fields and
contains predefined choices and hashes rather than executable or script paths.
All paths derive from the current hosted invocation and reject reparse points.
The adapter preserves the driver's actual window-DPI checks and owned process
and desktop teardown. It additionally queries actual holder and product PIDs
against the exact named Job, including product PIDs adopted after restart.

The optional `HostedScaleProcess.RunNamed` entry point creates a fresh random
256-bit Job name with query-only SID and OWNER RIGHTS entries. The existing
unnamed entry point is unchanged. Query handles are short-lived and closed in
`finally`; explicit termination and zero-active-process proof remain mandatory.
This access restriction is not a hostile same-user or privileged-code sandbox.
The hosted lifecycle script now has six bounded cases, including real positive
membership queries and negative assignment, termination, configuration and owner
security access attempts. Source preparation does not mean these new checks have
passed. Product execution under a provisioned scale and pixel review remain
unverified until the exact hosted candidate supplies those results.

`Verify-HostedNativeInterface.ps1 -ProvisionDisplayScale` opts in for 125%, 150%
or 200%; the existing 100% route is unchanged and rejects that switch. Installation,
package digest verification and the pinned tooling bootstrap finish before scale
mutation. The verifier creates a fresh, bounded, hash-bound request under
`RUNNER_TEMP`. A contained validation-only adapter pass checks it before Settings
input, then the supervisor opens the fixed `ms-settings:display` URI with hidden
launch configuration immediately before starting the contained scale worker.
No arbitrary URI or forced foreground activation is supported. The worker still
requires observed Settings identity and foreground ownership. Standalone jobs
continue to open their own Settings surface. The fixed adapter repeats request
validation before launching the product.
No request field selects arbitrary executables, scripts or filesystem locations.

The scale worker invokes the adapter only after it observes the requested
selection and measures the expected Settings-window DPI. The adapter's named
non-breakaway Job has a 1,800-second deadline; the enclosing unnamed scale-worker
Job allows 1,920 seconds, followed by separately bounded 60-second recovery.
Request validation allows 20 seconds. The enclosing workflow must exceed these
bounds plus installation, bootstrap, termination grace periods and encryption.
Timeout is always failure, even if an earlier runtime receipt says success.

Native acceptance requires zero adapter exit, verified complete-tree termination,
fresh request and runtime hashes, exact source/release/run/scope/tuple bindings,
actual holder and product membership, the native driver's per-window DPI and
owned process/desktop teardown checks, and independent restoration success.
Restoration finishes before the existing encryption step runs. If containment is
uncertain, the verifier withholds evidence reading and encryption, records disposal
as required, and never accepts a leftover runtime receipt. Successful encryption
still leaves pixels pending independent review. The request, adapter receipt,
random Job name and internal lifecycle files remain private and are never uploaded.

Do not dispatch the product-scale matrix until the separate lifecycle and
standalone provisioning checks pass for the intended implementation. The opt-in
hook's presence is not a completed scale-matrix verdict.

## Read-only hosted display capability observation

`Probe-HostedDisplayCapabilities.ps1 -OutputDirectory <new RUNNER_TEMP child>`
requires an already opened Display Settings surface on a disposable hosted
Windows machine. An unnamed non-breakaway Job contains its read-only worker for
45 seconds, with the existing bounded termination proof. Upload only `receipt.json`.

The worker binds the unique observed Settings Scale root to its actual monitor,
reads the current display mode and enumerates at most 512 supported modes with
`EnumDisplaySettings`. It also observes a unique Settings resolution combo whose
selected numeric dimensions match the current mode. Process start identities,
monitor identity and the current mode are checked again before acceptance.
`QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS)` must prove exactly one active display
target before and after the observation. Multiple targets, including cloned
targets, remain unavailable rather than assuming Settings selected the monitor
containing its own window. Resolution candidates must share the exact observed
Scale root. Display-path identities remain private. The numeric `fields` mask
accompanies each mode; orientation is null unless `DM_DISPLAYORIENTATION` is set,
and stability checks distinguish absent orientation from a measured zero.
The public receipt contains only fixed status values, numeric dimensions, bit
depth, refresh frequency, orientation, counts and a primary-monitor flag. Device
names, process/window identities, UI labels and raw exceptions are never emitted.

This diagnostic sends no input, expands no control, changes no resolution or
scale, and writes no registry settings. A larger supported resolution is evidence
for a later bounded investigation, not proof that 150% or 200% scaling is offered.
Missing or ambiguous resolution observations and incomplete mode enumeration
remain unavailable. Any future resolution change needs its own supported input,
exact original-state preservation and verified resolution-plus-scale restoration.

## Fixed larger-resolution interval

The scale helper's explicit `-ProvisionResolution` option requires
`-InputRoute hosted-foreground`. Its only native-runtime combination is the
baseline minimum tuple described below; other combinations are rejected before
input. It requests only a freshly enumerated 1920×1080, 32-bit
mode at the current refresh frequency and observed orientation. Unsupported or
ambiguous variants remain unavailable. No arbitrary dimensions or action hook
are accepted.

Before `CDS_TEST` or a mode change, the worker atomically preserves the original
scale/DPI and complete validated 220-byte `DEVMODEW`, together with the private
device and single active source/target identity. A nonzero `dmDriverExtra`, unknown
display-field bits or unsupported structure size is rejected instead of silently
discarding fields. The target preserves valid placement, orientation, display
flags and fixed-output semantics. `ChangeDisplaySettingsEx` first uses `CDS_TEST`
and then flags zero for a dynamic nonpersistent change. It never uses a null mode,
registry persistence, custom scaling, restart or sign-out.

The current mode is reread after the attempt, including a nonzero return. Every
documented valid requested field must match; padding and unspecified orientation
are not interpreted as observations. The worker records any automatic scale/DPI
change caused by resolution before attempting the requested predefined scale.
The active display identity is rechecked before mode changes and scale input.

Restoration independently attempts original scale and exact original mode, so a
UIA failure cannot skip a safe identity-bound mode restore. Final acceptance
requires the original mode plus a fresh original-scale/DPI observation. Recovery
inspects the current mode first and is idempotent after a partially completed
restore; its mode restore runs before UIA initialization so missing Settings
cannot prevent that attempt. Unknown child termination or changed display
identity or uncertain native input recovery blocks mutations and requires disposal.
The standalone resolution worker allows
180 seconds and independent recovery 90 seconds, after complete worker-tree
termination. Existing scale-only and product-adapter deadlines are unchanged.

Only fixed numeric dimensions, native return codes and verification booleans are
added to public receipts. The exact mode bytes and device/path identities remain
in private `original.json`, excluded from uploads. A verified larger resolution
does not by itself prove that 150% or 200% is offered; the existing scale probe
must still observe the option, selected value, actual Settings DPI and restoration.

Run `37090148387` stopped at `capture_original_resolution` before mode or scale
provisioning. That coarse phase does not identify which native observation was
unavailable. The helper now records separate selection and restoration native
diagnostics using fixed phase names and numeric API return codes only. They
distinguish topology, source-device structure and lookup, attached-primary
device validation, monitor binding, and mode validation. The source-device name
structure must measure exactly 84 bytes before its API call. No exception text,
device name, handle or private recovery bytes enter these diagnostics. The cause
remains unverified until a bounded hosted observation supplies the finer phase.

Run `37090623816` narrowed the unavailable capture to
`mode_size_and_driver_extra`. The next diagnostic records only the last inspected
buffer length and numeric `dmSize`/`dmDriverExtra` fields. Null means no sufficiently
large structure was inspected; these values may describe the earlier selection
when recovery fails before another mode read. The original 220-byte, zero-extra
acceptance contract remains unchanged. `EnumDisplaySettingsW` receives an
initialized `dmSize` and zero additional capacity; the documented driver-extra
capacity contract does not justify accepting or restoring truncated private data.
The last enum-success boolean also distinguishes a failed read: previously a
null result passed to the shared validator overwrote the enum phase with the
size/extra phase. Null now has its own `mode_read_returned_null` phase. The earlier
phase therefore did not establish that either returned header field was invalid.
Run `37091086128` subsequently measured enumeration success, a 220-byte buffer,
returned `dmSize=188` and `dmDriverExtra=0`. The helper now accepts only the known
188-byte and 220-byte public layouts in that complete 220-byte allocation. Every
accepted display field ends at or before byte 188. The actual returned `dmSize`
and all buffer bytes are retained for recovery and passed back unchanged; the
helper never promotes the header to 220 or clears private-data capacity. Unknown
sizes, nonzero extra data, unknown valid-field bits and missing required fields
remain rejected. Native identity, test/apply and restoration checks are unchanged.

The existing hosted lifecycle script exercises the actual helper's validation:
five unsupported header/field cases must fail before both known layouts pass,
and successful inspection must preserve the complete bytes and returned size.
This case performs no native display calls. Hosted contract results and actual
mode application/restoration remain pending; no local execution was performed.

### Fixed-resolution baseline minimum proof

The standalone resolution run `37091384649` passed its seven lifecycle cases and
observed 1920 by 1080 at 125% with Settings DPI 120. It restored the original
1024 by 768 mode and 100%/96 DPI, with both worker trees terminated and no disposal
requirement. That evidence does not prove the product's interactive minimum.

`Verify-HostedNativeInterface.ps1 -ProvisionResolution` now selects one bounded
combination: `minimum-resize`, `measured-minimum`, and scale `1`. Combining that
switch with scale provisioning or another scope is rejected. Installation and
package/hash validation finish before display mutation. The fixed request binds
the 1920 by 1080 choice and the display-mode and minimum-helper source hashes,
alongside the existing package, executable, driver, and containment identities.
The adapter passes its exact named Job to the minimum helper. Settings must first
report 100% and DPI 96; the product's existing independent DPI and real-drag
checks remain mandatory. The native worker keeps its 1920-second outer limit,
1800-second adapter limit, and independent 90-second display recovery limit.

Before launching the product, the worker durably creates `native-input.started`
containing the request digest. Only the fixed adapter can publish the matching
`native-input.restored` receipt after the exact minimum operation, button-up,
frame restoration, original input-desktop restoration, server exit, and native
teardown evidence all pass. The minimum helper reobserves button-up on `Default`
after the persistent server exits. These private files are outside the public
upload and encrypted product evidence inventories. The started marker is never
removed, and a Job reaching zero processes cannot supply restoration evidence.

An interrupted adapter, missing or mismatched restored receipt, or uncertain
button/desktop state blocks both the worker's restoration and the independent
recovery worker before mode changes or Settings input. The fixed supervisor
receipt reports `input_recovery_uncertain` and requires disposal. This is
deliberately conservative even if interruption occurred before the first drag.
Successful input recovery permits the existing original-scale and original-mode
restoration; final acceptance still requires complete tree termination and all
original display observations. No forced focus, synthetic resize message, or
simulated DPI is introduced. Combined runtime and genuine capture evidence remain
pending hosted verification; no local execution was performed.

### Encrypted higher-scale capability diagnostics

`Invoke-HostedDisplayScale.ps1 -DiagnosticEvidence` is an explicit opt-in for
standalone `hosted-foreground` discovery. It is rejected with `NativeRuntime`
and is not passed to recovery. It does not enable another input route, focus
activation, scrolling, registry changes, or unsupported scale selection.

At most two observations are attempted: `before_selector`, before the existing
input checks, and `expanded_selector`, after expansion is actually observed but
before requested-option matching. Each observation refreshes the exact Settings
process/start identities, Default desktop and owned root. It records foreground
ownership without forcing it. Bounded read-only UIA data includes control bounds,
enabled/offscreen state, supported patterns and available ScrollPattern geometry.
The subsequent input path resolves fresh geometry after capture and still applies
all existing freshness, foreground, containment and restoration requirements.

The existing cheap `screenshot --hwnd` route captures only that owned root. Native
ownership and frame bounds must match immediately before and after capture; the
bounded PNG must report successful rendering and match the frame dimensions.
Images remain pending independent pixel review, including whether an expanded
popup was rendered inside that root. Missing options are never inferred from an
unreviewed image. No whole-desktop or unrelated-window capture is performed.

Only encrypted evidence is saved inside the output directory. A temporary PNG
outside it is read only after verified child termination and then removed; an
uncertain child leaves the file private on the disposable machine and blocks
further input/recovery through the existing child marker. PNG input is capped at
8 MiB, the JSON envelope plaintext at 16 MiB, controls at 1,000, and labels at
2,048 characters. Labels, identities, rectangles and image bytes exist only in
the encrypted payload. Diagnostics never print native responses or exceptions.

Upload only `before_selector.aesgcm`, `before_selector.envelope.json`,
`expanded_selector.aesgcm`, `expanded_selector.envelope.json`, plus the existing
fixed run/supervisor/restoration receipts. Missing files mean unavailable evidence.
The envelopes use `hosted-scale-diagnostic-v1`, fresh AES-256-GCM keys/nonces,
RSA-OAEP-SHA256 and the existing `hosted-automation-public-v1.pem` recipient.
`aad_base64` supplies the exact authenticated bytes. The authenticated binding
contains the actual source commit, run, fixed phase, capture UTC time, dimensions,
PNG hash, cheap executable hash and both helper source hashes. The recipient
fingerprint identifies existing custody without putting private keys on the host.
This is source preparation only; hosted capture, decryption and pixel review are
still required before drawing conclusions about higher-scale availability.

Two additional hosted lifecycle cases load the exact production tuple and
input-recovery functions from the parsed source, without executing the display
supervisor. They reject unsupported combined tuples, absent or mismatched
restoration evidence, a changed request digest, and an incomplete started marker
before accepting the supported tuple and matching restoration state. The total
is now nine cases; their hosted verdict remains pending.
