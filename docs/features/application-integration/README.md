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
platform does not expose reliable network-capability metadata, so it is reported
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

## Verification and remaining work

The portable presentation-route executable passed 24 behavioral assertions.
The shared credential adapter passed 10 native behavioral assertions using the
real credential implementation and a synthetic in-memory vault.
Native GUI compilation, localization catalogs, runtime interaction and capture are
pending; source controls are not release acceptance evidence.

- [ ] Compile and drive shared-mode enrollment, rename, enable, unlock and credential replacement controls; add a platform passkey choice and atomic recovery for interrupted credential/record updates.
- [ ] Reconstruct every already-open translated surface on shared-mode changes without discarding active user work.
- [ ] Audit all dim-sum producers, keyboard handlers, documentation routes and non-settings surfaces for live suppression.
- [ ] Refresh installed voice choices when platform inventory changes while Preferences remains open.
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
