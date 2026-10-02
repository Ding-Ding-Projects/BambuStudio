# Native interface audit and verification register

This register records the current task's requested behavior and remaining
verification. It is not a claim of complete Material Design 3 conversion.
The starting combined source is `cb7e28b8547b9714450b6bd5c2ddb7aa8698c610`.
All product compilation, tests, installation and runtime interaction for this
task run exclusively on GitHub-hosted Windows runners.

## Owned implementation areas

| Area | Required behavior | Current evidence |
| --- | --- | --- |
| Shared context menus | Search and adjacent regex builder in every root and nested menu, including one-item menus | Source audit found a six-item threshold; repair in progress |
| Empty and narrow menus | Visible localized no-match state; bounded label and shortcut painting | Source audit found zero-height empty lists and unbounded zero-width label drawing |
| Keyboard menu navigation | First Escape clears a query; subsequent Escape returns or closes; focus restores correctly | Repair in progress |
| Dropdowns | Independent search state at each level, original selection indices and disabled state preserved | Repair in progress |
| Canvas menus | Layer actions, filament submenus, rotation method and SVG actions use independent searchable surfaces | Repair in progress |
| Canvas chooser state | Queries and regex-builder results never migrate between unrelated controls | Source audit found shared function-static state |
| Motion | Elapsed-time animation, safe callback lifetime, reduced-motion response, correct easing | Elapsed-time checkpoint exists; further review and runtime proof pending |
| Configuration history | Asynchronous reads and restoration, safe delayed close, contained archive extraction and no overwrite | Initial asynchronous checkpoint exists; accepted safety repairs in progress |
| Personal vocabulary | Visible JSON import, replace and clear; bounded schema validation; local-only persistence; original-wording restoration | New implementation in progress |
| Slice continuation | Ignore obsolete completion/progress events; never reuse mutable slicing state while an older worker is active | Source candidates under repair; independent refutation pending |
| Slice and Print | Adjacent action prepares the intended current plate, then opens the existing print flow after successful slicing | Existing continuation being extended and verified |
| Slice and Send | Adjacent action prepares the intended current plate, then opens transfer flow without starting a print | Implementation in progress |

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
