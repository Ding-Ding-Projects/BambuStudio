# Portable LAN hosting setup companion

Implementation checkpoint, not a released or runtime-verified companion.

The separate companion inspects the local Docker engine and the exact LAN model drop Compose service. It offers configuration, review, startup, cancellation and health results through a sandboxed interface. Existing services are started in place rather than recreated from bundled defaults, preserving their port, volume, queue and station credential. Remote Docker endpoints are rejected.

Fresh setup currently supports a generated station credential and a new dedicated named volume. An already-existing volume without its matching container blocks fresh setup for review. Custom credential input and folder selection remain unavailable. No credential enters renderer state, configuration, process arguments or logs. The server retains its existing protected-volume credential contract.

Cancellation is cooperative between bounded backend phases. It never kills a process by PID, stops the daemon or removes a container/volume. The UI remains attached until worker exit and service reconciliation; failed inspection leaves the operation unresolved and prevents another Start. Replaying an accepted request returns its original operation. Each session retains 128 receipts, then rejects new starts instead of evicting replay protection.

## Build route

- `build.bat /s -Companion LanDropSetup`: build the unpacked companion.
- `build-installer.bat /s -Companion LanDropSetup`: produce its single portable executable.
- Add `-Plan` to inspect the selected route without acquisition or building.

The ordinary native build remains the default. The companion uses exact Electron 45.2.3 and electron-builder 26.18.0 versions with an integrity-bearing npm lockfile. The existing pinned, checksum-verified Node bootstrap is reused. No publisher workflow or release is added by this checkpoint. Build, package and launch results remain pending.

## Verification and remaining work

Eight focused operation/controller tests passed using injected backend fixtures. They cover active/unresolved exclusion, cancellation reconciliation, preserved settings, unknown operations, bounded history, old-request replay and receipt capacity. PowerShell and JavaScript source parsing passed. These results do not prove actual Docker startup, installed prerequisite recovery, portable packaging or rendered behavior.

Still unfinished: automatic trusted prerequisite acquisition, native license/consent handling, fresh empty-folder storage, separately reviewed custom credential entrypoint, resumable drafts, copy URL/log views, complete localized diagnostics, native close-dialog localization, all language/theme/scale captures, minimum-size measurement and independent runtime acceptance. The required-feature inventory also remains incomplete, including search with independent regex builder, personal-wording import, action sounds, appearance settings, keyboard/palette coverage and complete embedded documentation. No universal feature-completeness claim is made.

Design state inventory and proposed protocol are under `design/lan-drop-setup/`. Service version 1.0.1 is pinned by the bundled Compose file; companion publication and service publication are separate verdicts. No live service was changed during implementation.
