# Portable LAN hosting setup companion

Built portable companion with bounded existing-service runtime verification. Publication and complete setup acceptance remain pending.

The separate companion inspects the local Docker engine and the exact LAN model drop Compose service. It offers configuration, review, startup, cancellation and health results through a sandboxed interface. Existing services are started in place rather than recreated from bundled defaults, preserving their port, volume, queue and station credential. Remote Docker endpoints are rejected.

Fresh setup currently supports a generated station credential and a new dedicated named volume. An already-existing volume without its matching container blocks fresh setup for review. Custom credential input and folder selection remain unavailable. No credential enters renderer state, configuration, process arguments or logs. The server retains its existing protected-volume credential contract.

Cancellation is cooperative between bounded backend phases. It never kills a process by PID, stops the daemon or removes a container/volume. The UI remains attached until worker exit and service reconciliation; failed inspection leaves the operation unresolved and prevents another Start. Replaying an accepted request returns its original operation. Each session retains 128 receipts, then rejects new starts instead of evicting replay protection.

## Build route

- `build.bat /s -Companion LanDropSetup`: build the unpacked companion.
- `build-installer.bat /s -Companion LanDropSetup`: produce its single portable executable.
- Add `-Plan` to inspect the selected route without acquisition or building.

The ordinary native build remains the default. The companion uses exact Electron 45.2.3 and electron-builder 26.18.0 versions with an integrity-bearing npm lockfile. The existing pinned, checksum-verified Node bootstrap is reused. An unpacked build of source `0e9a7c849d91141b77ddde007f975cb35be2b7cf` completed through the root entrypoint after a truncated Node download was rejected and reacquired successfully. That build does not verify subsequent source changes. No publisher workflow or release is added by this checkpoint. The newer portable result is recorded below.

The producer now requires clean committed source/index state and embeds a manifest containing the full source commit, index tree, version, exact runtime/builder versions and relevant input hashes. A separate package proof records executable hashes and the embedded-manifest hash without private machine paths. Embedded and external proof have separate roles; an executable cannot contain its own final hash.

Readiness results re-inspect the current container, exact project/service labels and unchanged data mount after polling. A replacement leaves reconciliation incomplete; an exited container is reported stopped. Existing service admission requires the published digest from the bundled Compose file to appear in the resolved image's repository digests, with matching configured repository identity. Older or unrelated images remain untouched and report upgrade required; a version label alone is insufficient.

## Verification and remaining work

The portable package from `ed55a003f333ffc01b5879b853f8eedc7d9b6f05` was built and exercised on a hidden desktop. That review found a display race: polling stopped between worker exit and completed health reconciliation, and a late Cancel click reported cancellation despite an already-completed operation. The current source separates reconciliation activity from worker lifetime, keeps polling until reconciliation settles, and consumes the actual cancellation response. Failed reconciliation stops automatic polling and retains manual Recheck without allowing a duplicate Start.

The header now reflects the backend's verified published-image identity, obtained only after exact image admission. It expires after ten minutes and is cleared on failed inspection. The expected service version remains separate from actual verification; the companion version is never evidence of the installed service version.

Five focused renderer tests execute the production renderer against a minimal DOM and the real controller with a deferred backend. They cover automatic completion, the terminal cancellation race, bounded reconciliation failure, identity expiry and invalidation. Two causal mutations restore the old polling and cancellation behavior and must fail the same consumer assertions.

Source `9eeafe7dabb4f2b3c5ae7baac66d349f31108862` subsequently built through the root portable entrypoint with exit 0. The unsigned executable SHA-256 is `bc1bd891e8e4ebf91e020eeb211997018fefb0d233d1b3f1e4810f5e2b06cbfa`. Its actual embedded manifest matched the producer and all four bundled hosting inputs. On a named hidden desktop, the existing-service workflow reached the healthy result automatically without Recheck and displayed verified image identity. At 960 x 720 and 640 x 520 outer-window sizes, 96 DPI, inspected images showed the result and retained footer; keyboard scrolling reached the sender-page control at minimum size. All five held process identities exited normally and the empty desktop closed. Selected container identity, image, port, mount and restart state were unchanged.

That run used the persistent Lowlevel MCP compatibility route with identity-bound native input and normal closure. CDP ownership validation stopped attachment, so no DOM/CSS layout-probe receipt is claimed. The completed interface hid Cancel truthfully, but deterministic native injection of the late-Cancel race was unavailable; that branch retains production-consumer fixture proof only. Fresh setup, prerequisite acquisition, another-device health during this particular run, and the complete language/theme/scale matrix remain unverified. These results do not replace prior separately recorded LAN verification.

Eleven focused operation/controller tests passed using injected backend fixtures and a temporary occupied-port listener. They cover active/unresolved exclusion, cancellation reconciliation, preserved settings, unknown operations, bounded history, old-request replay, receipt capacity and verified-absence recovery after preflight failure. A neutral Docker Compose configuration fixture exercises the production environment policy against inherited project names, dotenv files and orphan removal. A production process-reader fixture rejects oversized stdout while draining oversized stderr concurrently. PowerShell and JavaScript source parsing passed. These results do not prove actual Docker startup, installed prerequisite recovery, portable packaging or rendered behavior.

A separate production-consumer fixture checks stopped/running transitions, replacement identities, changed storage and positive/negative published-image digest admission using Docker-shaped JSON. The portable producer of source `dfc272840f97a947268b6dbd48128588bf58a3c4` generated an executable but failed its post-package proof step because a single-file array was unwrapped by PowerShell. The array handling has been corrected; that failed run is not accepted package proof.

Still unfinished: automatic trusted prerequisite acquisition, native license/consent handling, fresh empty-folder storage, separately reviewed custom credential entrypoint, resumable drafts, copy URL/log views, complete localized diagnostics, native close-dialog localization, all language/theme/scale captures and measured minimum-size coverage. The required-feature inventory also remains incomplete, including search with independent regex builder, personal-wording import, action sounds, appearance settings, keyboard/palette coverage and complete embedded documentation. No universal feature-completeness claim is made.

Design state inventory and proposed protocol are under `design/lan-drop-setup/`. Service version 1.0.1 is pinned by the bundled Compose file; companion publication and service publication are separate verdicts. No live service was changed during implementation.

[Cantonese article](README.yue_HK.md)
