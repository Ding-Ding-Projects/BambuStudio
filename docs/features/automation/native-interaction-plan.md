# Offline native interaction plan

`scripts/md3/review-interaction-plan.py` validates preparation for the
[nine-boundary review queue](../../../design/workflow-refresh/built-review-queue.md).
It executes no application, input, capture, file export, printer operation or
callback. A passing result means only that the request fits this narrow schema.
It does not prove that any state exists, is reachable, has been observed, or is
accepted. It does not authorize execution.

## Contract and source binding

Run with an independently reviewed full commit, not a value accepted from an
untrusted plan:

```powershell
& $python scripts/md3/review-interaction-plan.py --repository . --source-commit $reviewedCommit --plan $privatePlan
```

The only top-level fields are `schemaVersion` (integer `1`), `kind`
(`native-review-preparation`), `sourceCommit` (the exact reviewed 40-character
commit), `contractSha256`, and `steps`. `contractSha256` contains exactly these
keys, each mapped to the SHA-256 of the raw committed blob:

- `design/workflow-refresh/built-review-queue.md`
- `design/workflow-refresh/implementation-scopes.json`
- `design/workflow-refresh/manifest.json`

The validator reads those fixed paths with `git show <commit>:<path>`. It never
substitutes working-copy text. An updated review queue therefore needs updated
hashes and an independently reviewed source pin. This is data identity, not a
signature, trust decision, build receipt or proof of source equivalence with a
different executable. The existing local-build provenance checks remain required.

Each step has exactly `boundary`, `state`, `action`, and `timeoutMs`. For example:

```json
{"boundary":"shared-control-callers","state":"menus/keyboard","action":"menu-next","timeoutMs":1000}
```

Boundary IDs come from `remainingVisualCoverage`; states must occur in both that
boundary's queue section and the reference manifest. These are requested review
states, never native control IDs or claims about current state. Only the existing
nine ordered queue boundaries are supported. Reference boards remain references.

## Supported preparation

| Action | Permitted scope | Required later runtime precondition |
| --- | --- | --- |
| `observe-native` | Any state listed for its queue boundary | State independently observed in the current owned process; native geometry only |
| `menu-next` | `shared-control-callers`, `menus/submenu`, `menus/disabled`, `menus/keyboard` | The reviewed options popup is open and its actual focused menu control is resolved |
| `menu-previous` | Same menu states | Same live focused-menu proof |
| `menu-dismiss` | Same menu states | Same proof plus verified non-destructive dismissal route |

The menu intentions correspond to a single Down, Up or Escape key respectively.
They never mean Enter, Space, a click, invoking a menu row, or opening a popup.
Lowlevel documents `win_send_keys` with an explicit `hwnd` and `keys`; its Windows
implementation posts key messages. Posting a message does not prove that a
wxWidgets popup consumed it. Its explicit `hwnd` route targets that handle, not
an automatically discovered focused child. No reviewed local adapter currently
resolves and verifies the popup/focused target, so these operations remain
preparation only. The existing local driver's `cheap()` allowlist does not include
`win_send_keys`; this change does not widen it.

The plan contains no HWND, PID, coordinates, title match, arbitrary key sequence,
text payload, executable, shell, callback, path to import/export, or expected
success claim. Unknown fields, duplicate JSON keys, non-finite numbers, unknown
actions and execution receipts are rejected. Limit: UTF-8 JSON up to 64 KiB,
1 to 64 steps, each integer timeout 100 to 2,000 ms, summed at most 30,000 ms.
Timeouts are future upper bounds, not sleeps or retries. CLI diagnostics do not
echo rejected plan content. Private plans need not be checked into the repository.

## Later integration boundary

The current [local review](local-native-review.md) handles the initial shell and
closes its owned process job in `inspect_shell()`'s `finally` block. Its returned
receipt cannot be used to resume that session. A future reviewed adapter must run
inside that same owned lifetime, after successful build/profile/ownership checks
and live shell discovery, before final provenance validation and job teardown.
It must preserve the existing watchdog and cleanup guarantees.

Before each action the adapter must check process liveness, PID/job ownership,
current window identity, unambiguous native target, focused control and actual
semantic state. It must abort on a missing, disabled, changed or ambiguous target.
It must measure state after each action and retain truthful per-action evidence.
Never turn this validator's output into an execution receipt or infer success
from a transport response. Unknown states remain unobserved. A future integration
requires separate reviewed code, authorization and tests; this module provides no
execution entrypoint, and rejects `--execute`.

Opening controls, changing fields, local fixtures, importing models, resizing,
host display changes, capture, renderer/DOM inspection, login, device operations,
slicing, printing, sending, exports, downloads and submission are unsupported.
Observation of `print/mapping` is only a request to measure a state reached through
a separately authorized route; it cannot create the required slice/device fixture.
Native outer geometry never proves embedded DOM or OpenGL interior geometry.

## Verification

```powershell
& $python -m unittest discover -s scripts/md3/tests -p test_review_interaction_plan.py -v
```

Eight focused tests exercise actual committed examples from all nine boundaries,
menu scope, source/hash mismatches, unknown input and execution claims, parser and
duration bounds, and the offline CLI. No application launch or runtime evidence is
part of these tests.
