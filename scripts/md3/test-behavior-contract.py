"""Negative fixtures for the hosted behavior completeness contract."""

import importlib.util
from pathlib import Path
import unittest


MODULE = Path(__file__).with_name("behavior_contract.py")
SPEC = importlib.util.spec_from_file_location("behavior_contract", MODULE)
contract = importlib.util.module_from_spec(SPEC)
import sys
sys.modules[SPEC.name] = contract
SPEC.loader.exec_module(contract)


def confirmed(flow):
    before = {field: "before" for field in flow.state_fields}
    after = {field: "after" for field in flow.state_fields}
    if flow.predicate_id == "responsive-opening-transition":
        before.update(opening_state="closed", input_at_monotonic_ms=None,
                      ready_at_monotonic_ms=None, elapsed_ms=None,
                      focus_target_id="", keyboard_ack_id="")
        after.update(opening_state="ready", input_at_monotonic_ms=1000,
                     ready_at_monotonic_ms=1250, elapsed_ms=250,
                     focus_target_id="owned-control", keyboard_ack_id="key-ack")
    if flow.id == "project-recent-open":
        before.update(recent_entry_id="fixture-entry", project_path="", object_ids=[])
        after.update(recent_entry_id="fixture-entry", project_path="C:/fixture.3mf",
                     object_ids=["fixture-object"])
    routes = {"project-recent-open": "recently-opened-card",
              "project-open-responsive": "file-menu",
              "model-creator-open-responsive": "model-creator-entry",
              "workspace-open-responsive": "workspace-navigation"}
    return {"id": flow.id, "status": "probe_confirmed", "proof": {
        "source": "installed-process-probe", "predicate_id": flow.predicate_id,
        "input_action": "owned native input", "action_route": routes.get(flow.id),
        "before_state": before,
        "after_state": after, "capture_ids": ["before.png", "after.png"]}}


def complete_behavior():
    rows = [confirmed(flow) for flow in contract.BEHAVIOR_FLOWS]
    rows.extend({"id": row_id, "status": "unavailable", "reason": "No paired hardware or provider session"}
                for row_id in contract.EXTERNAL_LIMITATIONS)
    return rows


class BehaviorContractTests(unittest.TestCase):
    def test_complete_software_with_honest_external_limits_is_review_ready(self):
        result = contract.validate_behavior_rows("behavior", "en", complete_behavior())
        self.assertEqual(result.verdict, "ready_for_pixel_review")
        self.assertEqual(set(result.limitations), set(contract.EXTERNAL_LIMITATIONS))

    def test_missing_required_row_is_partial(self):
        rows = [row for row in complete_behavior() if row["id"] != "workspace-member-reopen"]
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertEqual(result.verdict, "partial_behavior")
        self.assertIn("workspace-member-reopen", result.missing)

    def test_new_opening_flows_are_each_required(self):
        for flow_id in ("project-recent-open", "model-creator-open-responsive",
                        "workspace-open-responsive"):
            with self.subTest(flow_id=flow_id):
                rows = [row for row in complete_behavior() if row["id"] != flow_id]
                result = contract.validate_behavior_rows("behavior", "en", rows)
                self.assertEqual(result.verdict, "partial_behavior")
                self.assertIn(flow_id, result.missing)

    def test_opening_without_keyboard_ack_is_partial(self):
        rows = complete_behavior()
        row = next(item for item in rows if item["id"] == "workspace-open-responsive")
        row["proof"]["after_state"]["keyboard_ack_id"] = ""
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("workspace-open-responsive:missing-semantic-proof", result.invalid)

    def test_recent_file_menu_substitution_is_partial(self):
        rows = complete_behavior()
        row = next(item for item in rows if item["id"] == "project-recent-open")
        row["proof"]["action_route"] = "file-menu"
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("project-recent-open:missing-semantic-proof", result.invalid)

    def test_duplicate_id_is_partial(self):
        rows = complete_behavior()
        rows.append(dict(rows[0]))
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("prepare-ink-selected:duplicate-id", result.invalid)

    def test_unavailable_software_flow_is_partial(self):
        rows = complete_behavior()
        row = next(item for item in rows if item["id"] == "model-creator-preview")
        row.update(status="unavailable", reason="Renderer not launched")
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("model-creator-preview:software-unavailable", result.invalid)

    def test_unknown_status_is_partial(self):
        rows = complete_behavior()
        rows[0]["status"] = "probably_passed"
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("prepare-ink-selected:unknown-status", result.invalid)

    def test_label_only_result_cannot_pass(self):
        rows = complete_behavior()
        rows[0]["proof"] = {"source": "installed-process-probe", "predicate_id": rows[0]["proof"]["predicate_id"],
                             "input_action": "click Ink", "before_state": {"label": "Home"},
                             "after_state": {"label": "Ink"}, "capture_ids": ["before.png", "after.png"]}
        result = contract.validate_behavior_rows("behavior", "en", rows)
        self.assertIn("prepare-ink-selected:missing-semantic-proof", result.invalid)

    def test_diagnostic_and_layout_are_distinct_from_behavior(self):
        diagnostic = contract.validate_behavior_rows("diagnostic", "en", [
            {"id": "installed-shell-diagnostic", "status": "capture_only", "capture_id": "shell.png"}])
        layout = contract.validate_behavior_rows("layout", "yue_HK", [confirmed(flow) for flow in contract.LAYOUT_FLOWS])
        self.assertEqual(diagnostic.verdict, "diagnostic_only")
        self.assertEqual(layout.verdict, "layout_only")


if __name__ == "__main__":
    unittest.main()
