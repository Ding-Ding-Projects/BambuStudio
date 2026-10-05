# Native service integration

The native application owns the shared presentation watcher before its first
translated startup surface. It stops that watcher and the speech environment timer,
then shuts down narration before application configuration is destroyed.

## Narrator controls

Preferences, Other contains an off-by-default narrator switch, independent speech
language, quiet switch, separate installed English and Cantonese voice choices, and
per-language rate and pitch from -10 through 10, with 0 as normal delivery.
Voice IDs persist rather than localized names. An absent saved voice stays saved;
the status reports the effective fallback or that no voice is available. The
platform inventory is reread every two seconds while Preferences is open, including
an initially empty inventory. Selection handlers use the current stable-ID list.
The platform does not expose reliable network-capability metadata, so it is reported
as unknown. The external Home Assistant mirror discloses that its playback
completion is unavailable and is not covered by local serialization.

The command palette indexes these settings and edits narrator enablement inline.
Windows screen-reader activity and the quiet preference are sampled once per
second and passed into the speech service. The operating-system flag is the
platform's report, not proof that every third-party reader identifies itself.

## Shared presentation

The watcher reads the shared record before translation. Missing records retain
normal presentation; unreadable or corrupt records suppress private presentation.
Restricted settings are omitted from newly constructed Preferences and palette
results. Palette activation and direct setting teleport check current availability
again. Personal wording load and clear handlers also recheck availability.

Base language, voice, and funny-level preferences remain stored. Watcher callbacks
reset the bilingual registry and refresh observed personal-wording controls.

General settings now exposes the shared mode's current display name, rename,
enable, unlock, initial credential enrollment and replacement controls. PIN and
password credentials use the native credential service and the one shared account.
The attempt budget is owned by the application, not the Preferences dialog.
Closing the dialog therefore does not replenish attempts. Credential input fields
are masked and cleared immediately after submission, and no entered answer is
persisted in application configuration. The control discloses the local-record
deletion reset route and reports unavailable vault, watcher and record state.
The chosen display name is used in the palette entry.

## Local model suite destination

File, Local model suite opens the native Ollama suite with the application's
fixed private data directory. The command palette discovers this command from
the actual menu. The destination is linked to the native GUI target, not a
browser page. Catalog authority completeness, native compilation and disabled
operations remain governed by the suite's own outstanding-work inventory.

## Local converter destination

File, Local file converter opens a persistent panel in the Local tools workspace.
The workspace uses the shared browser-style tab strip, including restored hidden
tabs and dock placement. The existing command palette discovers the File command.
The host reads at most 66 receipt bytes and accepts exactly 64 lowercase SHA-256
digits with one optional newline. It passes the installed directory, exact worker
path and digest into the converter's own executable and capability verification.

The production CMake target builds the worker beside its broker implementation,
hashes the linked worker, and installs the executable and receipt together.
Configuration requires `LOCAL_CONVERTER_QPDF_SDK` with the verified qpdf C API
headers. No PATH search substitutes for the SDK. The runtime still verifies its
compiled DLL pins and executes the real sandbox capability probe before enabling
PDF operations. Runtime staging of the complete qpdf bundle is separately required.
The native destination and packaging integration have not yet passed a full build.

## Live surfaces and sensitive controls

The application registers its main frame and each native service root with the
[surface registry](surface-registry.md). Shared-mode credential fields are marked
sensitive before display. Their neutral accessible names and static action labels
retain original message sources for live presentation refresh; values and the
user-selected mode name are never translation sources. The registry integrates
actual appearance adoption, weak-reference child discovery, sensitive-subtree
permission checks, and native capture-affinity readback. Capture, export and
history producers must consult the permission queries immediately before use;
registry metadata alone does not establish end-to-end protection.

## Verification and remaining work

Scheduled settings now composes with a strictly typed in-memory preference overlay.
Only current rule winners enter the overlay. The actual stored map and explicit
`get_base` accessor never contain temporary values, and replacing an invalid map
retains the prior valid map. Unknown and sensitive keys are rejected. The shared
presentation restriction takes precedence over scheduled language, tone and
narration language. The native schedule host reevaluates base settings and local
time every second and uses real language, appearance, font and narrator consumers.
It confirms a snapshot through the existing local history manager before publishing
the schedule document. Timed-out snapshots remain recoverable and are not reported
as an applied schedule. Full crash-transaction acceptance remains pending.

Help and command-palette article routes now open the bundled native documentation
reader. The bundle must be regenerated after final documentation integration.

The portable presentation-route executable passed 24 behavioral assertions with
both MinGW and MSVC 19.51.36260. These compile only the portable route adapter.
The shared credential adapter passed 10 native behavioral assertions using the
real credential implementation and a synthetic in-memory vault.
The typed preference overlay passed 32 portable behavioral assertions. The real
AppConfig integration test is registered but awaits the full native dependencies.
Native GUI compilation, localization catalogs, runtime interaction and capture are
pending; source controls are not release acceptance evidence.

- [ ] Compile and drive shared-mode enrollment, rename, enable, unlock and credential replacement controls; add a platform passkey choice and atomic recovery for interrupted credential/record updates.
- [ ] Reconstruct every already-open translated surface on shared-mode changes without discarding active user work.
- [ ] Audit all dim-sum producers, keyboard handlers, documentation routes and non-settings surfaces for live suppression.
- [ ] Verify delayed voice enumeration and removal through the built native controls.
- [ ] Record narrator changes in local history and exports through the shared history service.
- [ ] Connect all five attention accommodations to actual workspace behavior.
- [ ] Attach converter, authenticator, lock and Ollama destinations and packaged dependency proofs.
- [ ] Extend inline palette editing beyond narrator enablement and existing appearance controls.
- [ ] Compile and drive the real application across the complete required layout matrix.

Material Designer had no callable creation/export/handoff tool in this lane. The
controls extend the application's existing native component route. This is a
recorded availability limit, not a claim of design-reference parity.

Related: [personal presentation services](../personal-modes/README.md),
[narration service](../personal-modes/narration.md).
