# Native local-security integration

The service components and four native UI components are implemented. The native
application has not yet compiled or driven these new controls. This article is an
integration contract and an honest record of outstanding work, not a release claim.

## Required lifetime and storage wiring

Create one `make_application_vault(data_dir)` instance for product-local services.
It keeps a non-secret random instance marker in `local_security/instance-v1`.
Restart reuses that marker; deleting the application-data folder creates a fresh
namespace. Previous OS credential records remain inaccessible to the new instance
and can be removed deliberately through operating-system credential management.
Never use a display name to derive the data directory or namespace.

Create a separate `make_os_vault()` instance for `shared_mode_account`. The shared
presentation-mode credential is intentionally outside the product-local namespace.
The shared-mode owner enforces a persisted `AttemptBudget` in the same vault, under
`shared_mode_attempts_account`, and stores only the configured flag and generation
returned by `Credentials` in its shared settings record.

Keep one `IdentityHistory`, `AuthenticatorStore`, and `SupportTickets` alive for
the application session. Construct `IdentityHistory(data_dir, *local_vault)` and
`SupportTickets(*local_vault, data_dir)`. The history manager needs explicit
first-time credential enrollment before an identity change can be recorded.
Construct `AuthenticatorStore(*local_vault, record_mutation)` with a real,
synchronous history sink. It must call `IdentityHistory::append` and propagate
failure. Never pass an empty or no-op sink.

The sink maps `AuthenticatorAdded` to `HistoryAction::Created`,
`AuthenticatorChanged` and `AuthenticatorReordered` to `Renamed`,
`AuthenticatorRemoved` to `Removed`, and `DisplayNameChanged` to `Renamed`.
Map `AuthenticatorRestored` to `Restored`. The authenticated dispatcher calls
`AuthenticatorStore::restore_entry(identity, snapshot)` only for identities it
knows belong to that store. It validates the complete historical entry, retains
the original identity, and records restoration through the same history sink.
The same original generated 32-character hexadecimal identity follows a record
through every change. `LockCreated` records only its five-byte policy/duration
configuration, with no credential material. Each rendered element needs its own
generated stable identity maintained by the common element registry.

Construct one `ElementLock` per element identity with the same local vault and
real history sink. Keep it alive across every action route. Before pointer,
keyboard, touch, palette, shortcut, drag, and programmatic activation, call
`allows_action(steady_clock::now())`. A false result opens `LockWizard` and does
not execute the original action. A successful unlock also does not replay the
original action: the user activates it again. Call `leave_surface()` when its
surface closes and `relock()` for the explicit relock action.

## Native entrypoints

All entrypoints are in `Slic3r::GUI::LocalSecurityUI`:

| Entrypoint | Host responsibility |
| --- | --- |
| `AuthenticatorPanel(parent, shared_store, hooks)` | Register a normal browser-style destination and its command-palette route. |
| `LockWizard::Open(anchor, shared_lock, target_name, recovery_folder, hooks)` | Supply the exact anchor, target and actual application-data folder; intercept every protected action. |
| `SupportTicketsPanel(parent, shared_store, data_dir, hooks)` | Register Help, lock-setting and forgotten-answer routes to this destination. |
| `IdentityHistoryPanel(parent, shared_history, hooks, restore_dispatcher)` | Register protected history and its typed live-state restore owner. |

`Hooks::text` resolves current language and tone. `factual_text` resolves language
without tone changes and owns the support disclosure. `notify` routes errors to
the existing non-blocking notification service. `register_surface` attaches the
common per-element appearance and context-menu behavior. `register_sensitive`
excludes the supplied controls from history, logs, diagnostics, analytics and
captures. Those callbacks are required. `export_text` connects to the existing
format-aware export workflow; its absence is visible and disables export.
`open_support` opens the actual registered support destination.
`record_label`, `record_tooltip` and `record_name` preserve original public source
copy for live language changes. Static labels and actions carry their source;
input values and generated facts are never replaced through that route. Dynamic
status text stays component-owned. `record_factual_label` is required for the
static history, toy-lock and support disclosures and must use a language-only
renderer without tone changes. Masked fields receive only an accessible-name
source, never a replacement value.

`confirm_retention(anchor, consequence, revisions, callback)` is optional until the
shared confirmation service is wired. It must forward a copy of the actual native
`SuperConfirm::State` after both independent keys and the full slider authorize;
it must never synthesize flags. The consumer creates an authenticated preview
bound to the current head and exact selected revisions, displays their IDs and
the factual consequence, then revalidates before consuming that state. Missing
wiring disables removal explicitly. Active decryption access is removed, while
metadata remains. Backups, vault copies and journals are outside this operation;
no forensic-erasure promise is made. The native manager exposes bounded capacity
and marks retained metadata whose payload access has been removed. Version-one
payloads remain readable and cannot be pruned through this version-two action.

The native surfaces use the existing `SearchField`, `TextInput`, `Button`, `Label`,
and dialog-chrome components. All four local searches have their own anchored
regex builder. No callable design creation/export route or verified design handoff
was available during implementation. Reuse of existing components is source-level
implementation evidence only, not design-parity or rendered-layout evidence.

Add `add_subdirectory(LocalSecurity)` to the library CMake file. After creating
`libslic3r_gui`, add `add_subdirectory(GUI/LocalSecurity)` to the GUI CMake file.
When the existing `libgit2::libgit2package` target is available, the library
subdirectory also defines `libslic3r_identity_history`; the composition owner must
link that target. Add `add_subdirectory(local_security)` to the test tree.

## Implemented behavior and bounds

The authenticator stores at most 32 entries in the native vault, confirms pairing
before enrollment, preserves issuer/account/group metadata, reorders entries,
renders current and next codes, copies codes only on a user action, and exports
redacted metadata. The URI path honors digest/digits/period. The manual UI path
states its fixed SHA-1/six-digit/30-second parameters. Missing QR import routes
are explicitly reported rather than represented as working buttons.

The lock wizard exposes all six policies, three durations, an explicit toy-lock
disclosure, the exact recovery folder, ordered authentication, and PIN keypad plus
manual entry through the same backend. It is non-modal and initially anchored and
viewport-bounded. It does not yet track a moving anchor or provide an unlock-ladder
UI. Search names settings without inspecting secret values or hiding the recovery
disclosure.

Support tickets are local and fictional. The model creates, lists, advances and
exports tickets, with eight retained records and descriptions bounded to 160 UTF-8
bytes. Records are encrypted below `local_security/support-tickets-v1.enc`; only
the key is in the vault. The UI shows and copies the exact application-data folder
and asks the platform file manager to open it. It never deletes application data.
No network code exists in the support service or these components.

## Verification and remaining work

The Windows C++17/OpenSSL core now passes **223 behavioral assertions** under
MSVC. The preceding GCC unit passed 219 with `-Wall -Wextra -Werror`.
Coverage includes the prior standard vectors and
credential/lock checks, persisted authenticator changes, backend factor verification,
OTP replay rejection, encrypted support-ticket lifecycle, real native vault
round trips, marker restart/reset semantics, zero/maximum-size AES-GCM snapshots,
history-compatible element identities and validated authenticator restoration.

The isolated mutation driver is intentionally fail-closed. GCC encountered an
internal compiler error in three bounded attempts. Switching to the verified MSVC
19.51 toolchain completed all twelve mutations: each deliberately broken copy
failed its behavioral checks, while the baseline and restored source passed.
The run used the existing OpenSSL headers and x64 DLL with a temporary MSVC import
library generated only after checking every required C ABI export. This validates
the services, not the final packaged dependency set.

From a configured MSVC developer shell, the reproducible form is:

```sh
python tests/local_security/negative_regression.py --msvc --compiler cl --openssl-include <include-directory> --openssl-dll <matching-x64-crypto-dll>
```

An existing compatible import library may instead be passed with
`--openssl-library`. The driver never installs or replaces shared dependencies.

The separate libgit2 history driver compiled with the supported pinned libgit2
1.9.3 recipe and stable MSVC 19.51 toolchain, passing **118 identity history checks**.
Three isolated retention mutations also produced the expected failures before
the restored candidate passed all 118 checks.
The new QR matrix also passed independent ZXing decoding and exact parameter
comparison using public synthetic material only.
The native panels remain uncompiled and have no built interaction,
accessibility, localization or layout captures.

Outstanding before feature completion: full shell/action interception, localized
copy and live language/tone updates, rendered QR proof and packaged image/clipboard/
camera import, parameter controls for manual enrollment, history-manager built
verification and native retention confirmation, atomic live-state/history reconciliation,
built verification of the persisted attempt budgets and their named mutex, unlock-ladder UI, dynamic anchor tracking, complete bulk actions
and filtered multi-format export, per-element context-menu integration, and genuine
built verification across supported themes, languages, viewports and scales.

Related articles: [service overview](README.md), [encrypted identity history](identity-history.md),
[pairing and import packaging](qr-pairing.md).
