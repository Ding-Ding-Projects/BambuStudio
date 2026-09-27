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
    return {"id": flow.id, "status": "probe_confirmed", "proof": {
        "source": "installed-process-probe", "predicate_id": flow.predicate_id,
        "input_action": "owned native input", "before_state": before,
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
