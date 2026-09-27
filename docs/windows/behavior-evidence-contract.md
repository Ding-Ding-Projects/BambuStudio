# Hosted behavior evidence contract

`scripts/md3/behavior_contract.py` defines version 2 of the required behavior
ledger for the issue #41 software flows. It is a pure validator. The hosted
driver and collector do not call it yet, so this source change does not make a
release or an existing capture complete.

The driver should submit a separate ordered list of contract rows using `id`
and `status`. A confirmed software row also needs a `proof` object with the
flow's exact `predicate_id`, `source: installed-process-probe`, a recorded input
action, different nonempty semantic states containing that flow's required
`state_fields` before and after, and distinct
before/after capture IDs. A visible label, filename, image alone, or an
unavailable declaration cannot confirm a software flow. The validator checks
the ledger shape and coverage; the driver remains responsible for generating
and verifying the claimed native observations. Pixel review is still required.

## Required software IDs

| Area | Stable IDs |
| --- | --- |
| Prepare layout | `prepare-ink-selected`, `prepare-process-selected`, `prepare-objects-selected`, `prepare-narrow-layout` |
| Prepare interaction | `prepare-ink-search`, `prepare-process-search`, `prepare-objects-search`, `prepare-keyboard-navigation` |
| Project opening | `project-file-open`, `project-recent-open`, `project-open-responsive` |
| Model Creator | `model-creator-open-responsive`, `model-creator-render`, `model-creator-preview`, `model-creator-explicit-import` |
| Workspace | `workspace-open-responsive`, `workspace-save-reopen`, `workspace-checklist-edit`, `workspace-calendar-edit`, `workspace-member-reopen`, `project-portable-history` |
| Print and device | `print-preview`, `print-nozzle-selection`, `device-unpaired-state` |

The exact predicate ID for each row lives beside its ID in `BEHAVIOR_FLOWS`.
`project-file-open` verifies loading a saved 3MF through File > Open;
`project-recent-open` verifies a distinct recent-entry action leading to the
owned saved model and requires `action_route: recently-opened-card`. The Model
Creator and Workspace opening rows verify that
their surfaces become ready for focus and keyboard input. They do not stand in
for rendering a model or reopening saved workspace content.

Each opening responsiveness row records the native input time, ready time,
elapsed milliseconds, a focus target, and a keyboard acknowledgment. The
validator checks that the recorded duration matches the two timestamps and
that the target became ready. The required routes are `file-menu`,
`model-creator-entry`, and `workspace-navigation`, respectively. It makes no
numeric latency promise. A separate
product requirement must set any performance threshold before an opening-speed
claim can pass.

`layout` requires only the four Prepare layout IDs. `diagnostic` requires one
`installed-shell-diagnostic` row with `capture_only` and a capture ID. Neither
scope can return a complete behavior verdict.

## External limitations and verdicts

The behavior ledger also requires explicit rows for
`live-printer-transfer`, `live-camera-stream`, `model-provider-session`, and
`print-send`. When the required hardware or provider session is absent, use
`unavailable` with a concrete reason. That is an honest limitation, not a
verified interaction. Software flows remain required even when an external
service is unavailable.

The validator returns `diagnostic_only`, `layout_only`, `partial_behavior`, or
`ready_for_pixel_review`. A missing, duplicated, unknown, unverified, blocked,
or unavailable software row yields `partial_behavior`. Only full software
coverage plus explicit external limitation rows can reach
`ready_for_pixel_review`, which still requires a separate pixel and provenance
review. Integrate this validator into the hosted driver and collector before
using it as a release condition. The integration must map real actions to
these stable IDs and retain the complete validation result in the report.

The negative fixtures in `scripts/md3/test-behavior-contract.py` cover missing
and duplicate IDs, each distinct opening row, a missing keyboard acknowledgment,
a falsely unavailable software flow, an unknown status, label-only evidence,
and honest hardware limitations. They have not been run
in this source-only change.
