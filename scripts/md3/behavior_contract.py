"""Versioned, pure completeness contract for hosted Bambu Studio behavior evidence.

This module judges an action ledger. It does not drive the application or turn a
probe label into proof. The hosted driver must supply independently observed
before/after state for each confirmed software flow.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Mapping, Sequence


CONTRACT_VERSION = 1
LANGUAGES = frozenset({"en", "yue_HK", "bilingual_en_yue_HK"})
STATUSES = frozenset({"probe_confirmed", "unavailable", "unverified", "blocked", "capture_only"})


@dataclass(frozen=True)
class Flow:
    id: str
    predicate_id: str
    state_fields: tuple[str, ...]


# Hand-written inventory. Changing an ID or predicate is a contract revision.
LAYOUT_FLOWS = (
    Flow("prepare-ink-selected", "selected-tab-and-panel-transition", ("active_tab_id", "visible_panel_id")),
    Flow("prepare-process-selected", "selected-tab-and-panel-transition", ("active_tab_id", "visible_panel_id")),
    Flow("prepare-objects-selected", "selected-tab-and-panel-transition", ("active_tab_id", "visible_panel_id")),
    Flow("prepare-narrow-layout", "measured-control-geometry-transition", ("viewport", "control_rects")),
)

BEHAVIOR_FLOWS = LAYOUT_FLOWS + (
    Flow("prepare-ink-search", "filtered-list-transition", ("query", "visible_result_ids")),
    Flow("prepare-process-search", "filtered-list-transition", ("query", "visible_result_ids")),
    Flow("prepare-objects-search", "filtered-list-transition", ("query", "visible_result_ids")),
    Flow("prepare-keyboard-navigation", "focus-and-selection-transition", ("focused_control_id", "active_tab_id")),
    Flow("project-file-open", "owned-fixture-model-transition", ("project_path", "object_ids")),
    Flow("project-open-responsive", "bounded-input-to-ready-transition", ("ready_state", "elapsed_ms")),
    Flow("model-creator-render", "validated-mesh-render-transition", ("validated_mesh_sha256",)),
    Flow("model-creator-preview", "preview-geometry-transition", ("preview_mesh_sha256",)),
    Flow("model-creator-explicit-import", "explicit-plate-object-transition", ("plate_object_ids",)),
    Flow("workspace-save-reopen", "archive-and-reopened-state-transition", ("workspace_id", "manifest_sha256")),
    Flow("workspace-checklist-edit", "checklist-item-roundtrip-transition", ("item_id", "item_state")),
    Flow("workspace-calendar-edit", "calendar-item-roundtrip-transition", ("event_id", "event_state")),
    Flow("workspace-member-reopen", "member-bytes-roundtrip-transition", ("member_id", "member_sha256")),
    Flow("project-portable-history", "history-head-roundtrip-transition", ("history_head",)),
    Flow("print-preview", "sliced-preview-transition", ("slice_state", "preview_plate_id")),
    Flow("print-nozzle-selection", "nozzle-choice-state-transition", ("selected_nozzle_id",)),
    Flow("device-unpaired-state", "unpaired-device-state-transition", ("device_connection_state",)),
)

# External availability is reported separately from software behavior. An
# unavailable row is honest evidence of a limitation, never a passed flow.
EXTERNAL_LIMITATIONS = (
    "live-printer-transfer",
    "live-camera-stream",
    "model-provider-session",
    "print-send",
)


@dataclass(frozen=True)
class ContractResult:
    version: int
    scope: str
    language: str
    verdict: str
    missing: tuple[str, ...]
    invalid: tuple[str, ...]
    limitations: tuple[str, ...]
    confirmed: tuple[str, ...]


def _semantic_proof(row: Mapping[str, object], flow: Flow) -> bool:
    proof = row.get("proof")
    if not isinstance(proof, Mapping):
        return False
    if proof.get("source") != "installed-process-probe" or proof.get("predicate_id") != flow.predicate_id:
        return False
    action = proof.get("input_action")
    before = proof.get("before_state")
    after = proof.get("after_state")
    captures = proof.get("capture_ids")
    return (isinstance(action, str) and bool(action.strip())
            and isinstance(before, Mapping) and bool(before)
            and isinstance(after, Mapping) and bool(after) and before != after
            and all(field in before and field in after for field in flow.state_fields)
            and isinstance(captures, list) and len(captures) >= 2
            and all(isinstance(item, str) and bool(item.strip()) for item in captures)
            and len(set(captures)) == len(captures))


def validate_behavior_rows(scope: str, language: str,
                           rows: Sequence[Mapping[str, object]]) -> ContractResult:
    """Classify one hosted tuple without inferring success from labels or files.

    The driver must pass contract rows with stable IDs. It must verify the
    predicate against real installed-process data before reporting a confirmed
    row. This function checks the ledger shape and coverage, not the pixels.
    """
    if scope not in {"diagnostic", "layout", "behavior"}:
        raise ValueError("unknown behavior contract scope")
    if language not in LANGUAGES:
        raise ValueError("unknown behavior contract language")
    if not isinstance(rows, (list, tuple)):
        raise ValueError("behavior contract rows must be an ordered list")

    required = {flow.id: flow for flow in
                (() if scope == "diagnostic" else LAYOUT_FLOWS if scope == "layout" else BEHAVIOR_FLOWS)}
    allowed = set(required)
    if scope == "diagnostic":
        allowed.add("installed-shell-diagnostic")
    elif scope == "behavior":
        allowed.update(EXTERNAL_LIMITATIONS)

    seen: set[str] = set()
    invalid: list[str] = []
    confirmed: list[str] = []
    limitations: list[str] = []
    for index, row in enumerate(rows):
        if not isinstance(row, Mapping):
            invalid.append(f"row[{index}]:not-an-object")
            continue
        row_id = row.get("id")
        if not isinstance(row_id, str) or not row_id:
            invalid.append(f"row[{index}]:missing-id")
            continue
        if row_id in seen:
            invalid.append(f"{row_id}:duplicate-id")
            continue
        seen.add(row_id)
        if row_id not in allowed:
            invalid.append(f"{row_id}:unknown-id")
            continue
        status = row.get("status")
        if not isinstance(status, str) or status not in STATUSES:
            invalid.append(f"{row_id}:unknown-status")
            continue
        if row_id == "installed-shell-diagnostic":
            if status != "capture_only" or not isinstance(row.get("capture_id"), str) or not row["capture_id"]:
                invalid.append(f"{row_id}:invalid-diagnostic-capture")
            else:
                confirmed.append(row_id)
            continue
        if row_id in EXTERNAL_LIMITATIONS:
            if status == "unavailable" and isinstance(row.get("reason"), str) and row["reason"].strip():
                limitations.append(row_id)
            elif status == "probe_confirmed":
                invalid.append(f"{row_id}:external-claim-needs-separate-proof")
            else:
                invalid.append(f"{row_id}:missing-limitation-reason")
            continue
        if status == "probe_confirmed":
            if _semantic_proof(row, required[row_id]):
                confirmed.append(row_id)
            else:
                invalid.append(f"{row_id}:missing-semantic-proof")
        else:
            # A software feature cannot pass by being declared unavailable.
            invalid.append(f"{row_id}:software-{status}")

    missing = tuple(sorted(allowed - seen))
    if scope == "diagnostic":
        verdict = "diagnostic_only" if not missing and not invalid else "partial_behavior"
    elif scope == "layout":
        verdict = "layout_only" if not missing and not invalid else "partial_behavior"
    else:
        verdict = "ready_for_pixel_review" if not missing and not invalid else "partial_behavior"
    return ContractResult(CONTRACT_VERSION, scope, language, verdict, missing,
                          tuple(invalid), tuple(sorted(limitations)), tuple(sorted(confirmed)))
