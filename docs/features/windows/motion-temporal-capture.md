# Restricted temporal window observations

`scripts/md3/motion_temporal.py` is an opt-in helper for a future installed native
interaction scope. It is not wired into a driver or workflow yet. No runtime,
capture-cadence, rendered-motion, or composition result is claimed from its source
or product-free contracts.

## Interface and ownership

An existing hosted driver may call
`await capture_transition(binding, output_root=..., executable=..., package=..., tool_root=...)`.
The caller must already have verified the release, installer receipt, exact
product source, executable/package hashes, requested language/theme, and actual
native target. The helper independently checks the executable/package bytes,
verifier checkout and helper bytes, current run/attempt, target process start,
HWND, desktop, DPI, geometry, and named Job membership. Product-source metadata
and language/theme remain caller-verified bindings, not new independent claims.

The exact version-1 binding has these fields:

| Fields | Meaning |
| --- | --- |
| `schema`, `run_id`, `run_attempt`, `nonce` | Version 1, hosted invocation and fresh 32-hex nonce |
| `source_commit`, `verifier_commit` | Separate 40-hex product and observer commits |
| `exe_sha256`, `package_sha256` | Previously verified release byte identities |
| `pid`, `process_started`, `hwnd` | Live product PID/start and visible top-level capture window |
| `desktop`, `job_name` | Already owned non-default desktop and exact `Local\BambuNativeScale-<64 hex>` Job |
| `language`, `theme`, `dpi`, `client_size` | Existing supported language/theme, actual 96/120/144/192 DPI, physical client dimensions |
| `roi` | `[left, top, right, bottom]`, fixed decorative paint region inside that client |
| `click` | `{x, y, button, target_hwnd}`, observed point and exact expected descendant, left/right only |

Use a fresh output directory below `RUNNER_TEMP`. The caller must execute the
helper on the owned desktop with per-monitor-v2 thread DPI awareness inside the
existing suspended-start, non-breakaway, kill-on-close named Job. The enclosing
supervisor has a hard deadline of at most 120 seconds. This helper cannot prove
the supervisor's deadline or Job configuration merely from membership.

Two official compatibility MCP stdio sessions are prepared before the action,
using the existing tool bootstrap pinned to
`e6e42f2066d539256d6480401d7cef867f2b8dfe`. The helper compares the installed
`server.py`, `winio.py`, `process.py`, and `processes.py` to those exact source
blobs and records their LF-normalized hashes. It accepts the bootstrap's supported
MCP SDK 1.x version, at least 1.2, and records that version. It installs nothing.
Both server PID/start identities and exact command lines are observed, and both
must be members of the caller's Job. One session captures; the other posts the
single authorized click. No direct native capture, input injection, widget
invocation, desktop switching, or UIA traversal is added.

## Four-frame observation contract

The helper requests one baseline and three frames at approximately 25, 60, and
180 milliseconds after input dispatch. These are deadlines, not claimed achieved
sample times. Each MCP request records monotonic and UTC start/end bounds. The
tool provides no pixel acquisition timestamp. A slow frame never shifts the
remaining deadlines forward to pretend the intended cadence was achieved.

Native read-only identity/geometry checks bracket the frames. The exact target
child is checked before input and against its acknowledgement. These checks are
not atomic with a separate server's input dispatch; a changed target fails the
observation, and the caller must use only a disposable owned product desktop.
Each MCP request is bounded to three seconds; a blocked native capture or a
blocked stdio shutdown is ultimately contained by the outer Job deadline.
Input runs as a separate asynchronous request and is joined before its session
closes, including on capture exceptions. Cancellation never means posted input
was undone. No sleep or UIA traversal occurs inside the capture call. PNG decode
and pixel hashing occur only after the burst.

Acceptance requires exactly four valid frame intervals, a baseline wholly before
input, a final observation starting at least 180 milliseconds after dispatch,
and at least one complete intermediate observation interval after input
acknowledgement and before 100 milliseconds after dispatch. That intermediate
region's actual RGB bytes must differ from both baseline and final regions.
The endpoint regions must differ too; a pulse returning exactly to its baseline
is deliberately outside this initial two-state contract. This measures a
necessary pixel transition, not the correctness of its words, color, contrast,
easing, frame rate, accessibility, or semantic action. Posted input can still be
consumed after its transport acknowledgement. Human review of genuine frames and
the driver-owned semantic postcondition remain necessary.

The pinned window capture uses `PrintWindow` client contents. Its output can be
uncomposited. `native_layered_alpha_verified` is always false: these frames cannot
prove `WS_EX_LAYERED` entrance opacity. Such acceptance needs a genuine controlled
hosted capability fixture first. The helper never broadens capture to an entire
desktop to fill that gap. Paint-owned control/menu transitions may qualify if
their actual frame intervals and pixels meet the contract.

## Bounds, privacy, and teardown

Only four original PNGs are retained, at most 16 MiB each, with width/height at
most 8192 and area at most 16,777,216 pixels. The dimension bound precedes capture;
the encoded-size bound is checked after each returned capture. The tool backend
can allocate the bounded bitmap before its encoded size is known. Frames are
never reconstructed or edited. The selected region is hashed in memory, without
producing a cropped derivative. `temporal.json` contains private bindings,
request intervals, tool/verifier hashes, frame hashes and fixed verdicts. Neither
that receipt nor raw frames are public output.

An exclusive, flushed `temporal-pending.json` exists before the first server is
launched. It remains uncertain if setup, input, observation, session shutdown,
server-exit proof, or the outer deadline fails. Only normal joined-input and
verified two-server exit permits its rename to `temporal-finished.json`. A finished
marker is not proof of product/desktop teardown. The outer caller must prove the
whole Job empty and perform its existing owned desktop restoration/closure before
encrypting evidence with the existing dedicated recipient. Unknown child or
desktop ownership withholds acceptance and encryption. No recovery input is
authorized by this helper. It deletes nothing.

The returned summary has only status, frame count, disposal requirement and
temporal acceptance. `not_observed` fails temporal acceptance while preserving
the genuine frames for restricted encrypted review after verified teardown.
`unverified` likewise never passes. Raw protocol output, exception text, process
paths and identities are not printed. The caller must include this helper in its
verifier hash manifest and explicitly allowlist the five private evidence files
before integration; existing evidence limits must not be silently widened.

## Focused hosted verification

Run `python scripts/md3/test_motion_temporal.py` on the hosted Python 3.12 route.
The 18 product-free cases exercise the production interval/binding predicates,
a deliberately weakened start-only timing mutation, and asynchronous input
joining on normal completion, capture exception, and parent cancellation.
They require no product installation or MCP server. They do not replace the
controlled hosted four-frame trial, exact process/desktop teardown evidence,
encrypted frame review, or the separate composition capability proof. No local
execution was performed when adding this helper.

The dedicated `motion-temporal-contract.yml` workflow runs only these
18 product-free cases on a hosted Windows runner with pinned checkout/setup
actions and Python 3.12. Its required `source_commit` must equal both the checked
out revision and the workflow's `GITHUB_SHA`; dispatching a different revision
fails before execution. No package, product, GUI, MCP server or capture is started,
and no installation step runs. The job has a five-minute limit.

The workflow retains only fixed `receipt.json` and `cases.json` outputs. They bind
run/attempt, exact source, workflow/helper/test hashes, actual Python version,
discovered and recorded counts, each executed case's status and elapsed time,
and the process exit code. Skips and unexpected inventory changes cannot pass.
Exceptions and traceback bodies are not copied into the result files. A passing
contract run establishes only the exercised predicates and asynchronous task
lifetime, never actual MCP shutdown, achieved capture cadence or rendered motion.

Before this workflow exists on the default branch, GitHub's manual dispatch API
can return HTTP 404. Activation therefore also supports pushes to
`feature/ui-integration`, restricted to the workflow, helper and contract file
paths. That event derives the expected revision from `github.sha`; manual events
still require the explicit input to equal the checked-out workflow revision.
An automatic activation run must not be followed by a duplicate manual run of
the unchanged contracts. A dispatch 404 is not a contract execution result.
