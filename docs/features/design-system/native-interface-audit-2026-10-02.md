# Native interface audit and verification register

This register records the current task's requested behavior and remaining
verification. It is not a claim of complete Material Design 3 conversion.
The first focused combined candidate is `cb5981a5977f56a10174f72bad7945100004a355`.
Its hosted verification is run `37055936191`. The focused job passed 46 canvas
checks, 49 source tests, eight slicing contracts, and five print cases with 28
assertions. See [the hashed receipt](hosted-verification-cb5981a5.json).
Native-service compilation failed on catalog review metadata and an unqualified
`apply` call resolving to `std::apply`; repairs at `c68cf4384` passed independent
source review and await a new hosted run. Installed-interface
evidence remains separate. The full run is failed, not successful.
All product compilation, tests, installation and runtime interaction for this
task run exclusively on GitHub-hosted Windows runners.

## Owned implementation areas

| Area | Required behavior | Current evidence |
| --- | --- | --- |
| Shared context menus | Search and adjacent regex builder in every root and nested menu, including one-item menus | Threshold removed in 45f1101; runtime evidence pending |
| Empty and narrow menus | Visible localized no-match state; bounded label and shortcut painting | Source repairs in 45f1101; measured evidence pending |
| Keyboard menu navigation | First Escape clears a query; subsequent Escape returns or closes; focus restores correctly | Implemented; native interaction pending |
| Dropdowns | Independent search state at each level, original selection indices and disabled state preserved | Implemented; native interaction pending |
| Canvas menus | Layer actions, filament submenus, rotation method and SVG actions use independent searchable surfaces | Candidate 4eba2c6 implemented and source reviewed |
| Canvas chooser state | Queries and regex-builder results never migrate between unrelated controls | Independent keyed state and bounded transient retry implemented; 46 checks await hosted verdict |
| Motion | Elapsed-time animation, safe callback lifetime, reduced-motion response, correct easing | Elapsed-time checkpoint exists; further review and runtime proof pending |
| Configuration history | Asynchronous reads and restoration, safe delayed close, contained archive extraction and no overwrite | 13ce9d6 source reviewed; hosted archive tests pending |
| Personal vocabulary | Visible JSON import, replace and clear; bounded schema validation; local-only persistence; original-wording restoration | 26526ed source reviewed; parser, persistence and native display verification pending |
| Slice continuation | Ignore obsolete completion/progress events; never reuse mutable slicing state while an older worker is active | a3143f6 source reviewed; native race evidence pending |
| Slice and Print | Adjacent action prepares the intended current plate, then opens the existing print flow after successful slicing | Implemented; installed confirmation-flow evidence pending |
| Slice and Send | Adjacent action prepares the intended current plate, then opens transfer flow without starting a print | Implemented; installed transfer-flow evidence pending |
| Preview geometry | Wrap notification content before placement, preserve overflow entries, fit status and grouping controls | 35ce8db source reviewed; 68 geometry/timing checks and actual layouts pending |

## Personal vocabulary boundary

The canonical input shape is a versioned JSON object with `schemaVersion: 1`
and an `entries` mapping. Only explicitly display-only adapters may apply the
mapping. General translation helpers also serve logs, errors and exports and
must not become an implicit route for private substitutions.

Verification must exercise valid import, rejected malformed/oversized input,
replacement, persistence, clearing, original wording, overlapping keys and
stable internal identifiers. Public fixtures use neutral synthetic terms.
Private input, private mapped text and private local paths never enter public
source, logs, exports, screenshots or hosted verification payloads. Synthetic
fixture coverage must not be described as verification of a private file.

## Runtime matrix

Use real installed binaries bound to the exact source commit and package hash.
For each changed visible surface, record normal and minimum supported client
dimensions, English, Hong Kong Cantonese and bilingual modes, light and dark
themes, and 100%, 125%, 150% and 200% measured display scales. Requested scale
alone is not evidence of actual native DPI.

Menu scenarios include short and long lists, no matches, nested matches, long
labels and shortcuts, regex validation, builder apply/cancel, keyboard selection,
Escape clearing, focus return and independent state between controls. Motion
scenarios include interrupted/restarted animation, disposal during callbacks,
reduced motion and UI responsiveness during background work.

Slicing scenarios include explicit cancellation, old completion after a new
request, model/configuration/plate changes, failed slicing, close and retry.
Both combined actions must reject stale results. Physical printer transfer and
printing remain separately unverified without authorized hardware; no test may
silently start a real print.

## Design and evidence limitations

No callable Material Designer flow is available in this session, and local
product execution is prohibited. Existing checked-in native design references
remain the design baseline. Source reviews, prototypes and passing managed
transport checks do not establish visible layout correctness.

The existing release-evidence workflow covers language/theme combinations and
normal/minimum dimensions at four scales, but its old behavior inventory does
not by itself prove the new menus, personal vocabulary or combined actions.
Those flows require explicit interaction-ledger entries and genuine captures.
No new visible-surface completion claim is made by this register.
