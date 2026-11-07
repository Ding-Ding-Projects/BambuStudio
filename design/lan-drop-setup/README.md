# LAN Drop Setup design handoff

Status: proposed design, awaiting independent review. This package does not install or start a service. The desktop application remains native; the separate hosting companion is the only portable web-runtime surface proposed here.

## Review the workflow

Open `prototype.html`. The permanent demonstration banner distinguishes simulated outcomes from service results. Choose a scenario, language and theme, then use Back and Next. The normal reference is 960 × 720 CSS pixels and the proposed minimum is 640 × 520 DIP. Query parameters provide deterministic references: `?step=0&scenario=existing&lang=en&theme=light` through step 5. Set the actual viewport externally; the prototype does not claim to emulate operating-system scaling.

The six steps are Discover, Check requirements, Configure, Review, Start and Ready. A persistent footer keeps navigation outside the scrolling content region. At the minimum width the step rail becomes a compact step indicator. Error recovery remains in the current step and preserves nonconfidential drafts.

## Existing setup comes first

Discover the exact existing Compose project, service, published port, storage and health without displaying its credential. Existing storage and credential are retained by default and cannot be changed by this initial wizard. Never adopt an unrelated container or migrate an existing volume implicitly. A configured remote desktop station is not replaced.

Fresh setup may select the standard new named volume or an empty, newly dedicated folder. Validate its resolved destination, ownership, emptiness, write access and overlap with existing data before proceeding. A nonempty folder is rejected without deletion. File-picker selection is required in production; the prototype uses descriptive sample text only.

A fresh station credential is generated securely by default. An optional custom credential uses a masked, explicitly labelled input and a protected configuration channel, never a plaintext environment file, command argument, URL or log. It is never saved in resumable drafts. The prototype intentionally accepts no credential value. Reuse the native local pairing protocol without changing its owner-only pipe access, bounded output or protected persistence requirements.

## Backend contract proposal

The versioned request, progress, cancellation and confidential-data lifetime proposal is specified in `protocol.md`. It is not an existing backend capability. Active-operation navigation and native window-close behavior are specified in `states.md`.

Extend the tracked hosting backend with typed, versioned progress and protected configuration inputs while retaining `host.bat --detached` and `host.bat --logs`. The interface calls a fixed operation allowlist, not arbitrary commands. Validate every request in the privileged boundary, not only in the renderer. Use bounded, monotonic phase events with operation identity and sequence, safe error codes and explicit terminal states. Do not synthesize percentages for work without measurable totals.

Phases: inspect, acquire prerequisites if needed, validate configuration, pull image, start service, check local health, check LAN URLs, finish. The existing host script bounds service work to 600 seconds; the native pairing deadline is 660 seconds. Any separate prerequisite acquisition needs its own reviewed timeout and cancellation contract, not an unbounded extension of those deadlines.

Cancellation closes only owned child operations. It never stops the Docker daemon, removes a volume or tears down a service already started successfully. Reconcile actual service state before reporting cancellation; explain when the service remains running. Retry rechecks state before applying another action. Persist only nonconfidential drafts after explicit changes; resume always revalidates them.

Missing prerequisites use a supported pinned installer or package with verified publisher/hash and provenance. Preserve existing installations. Native elevation consent and license acceptance remain user actions. Never automate either, initiate a restart, weaken host security or manufacture success after cancellation. Unsupported host requirements remain actionable incomplete states.

## Delivery boundary

Package a dedicated `companions/lan-drop-setup/` application with isolated renderer, sandbox, context isolation, disabled renderer Node access, narrow typed IPC, restrictive content security policy and local assets. Deny arbitrary navigation and popups. Open only validated sender URLs through an explicit user action. Do not reuse the concept shell's permissive navigation or installer target.

Use pinned, reviewed runtime and packaging versions with a lockfile; selection and acquisition are pending review. Build and package through the repository root `build.bat` and `build-installer.bat` using a narrow companion target. Publish a normal release containing a single portable executable that launches without an adjacent runtime folder, checksum and source/version manifest. The companion bundles the reviewed hosting scripts and Compose configuration. It does not embed a registry credential or claim the server image is offline-bundled.

Server source baseline: `601003f9b97d8945732d5a166f8dbe2b061d040f`. Published service: `ghcr.io/ding-ding-projects/bambustudio-lan-model-drop:v1.0.1@sha256:9382599181a32a5d78730c82b315612fcadc51beacea0ad3b831500e122ce51a`. Design source baseline: `f1526f80268c67df5a6ff196b865b7b0118412cb`. Display companion version and build provenance separately from service identity.

## Implementation destinations after approval

- `companions/lan-drop-setup/`: interface, localization, narrow bridge and process orchestration.
- `lan-model-drop/host.ps1`: compatible structured operation protocol and protected configuration.
- `scripts/windows/`: root-entrypoint companion packaging/bootstrap integration.
- Focused backend and interface fixtures, paired feature documentation and publication workflow.

None of these destinations are implemented by this design package. Independent review must approve configuration transport, new-storage semantics, prerequisite acquisition, cancellation and package provenance before implementation.

## Acceptance and known gaps

See `states.md` for the full state and reference matrix. Every current layout is a design reference, not a screenshot or runtime result. Native installation, real service lifecycle, portable packaging, localization completeness, accessibility and layout measurements remain unverified. Broader required settings, search/regex, personal wording and companion documentation obligations require explicit inventory during implementation; this prototype does not claim to deliver them.
