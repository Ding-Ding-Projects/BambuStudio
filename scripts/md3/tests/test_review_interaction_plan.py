"""Offline negative regressions for bounded native review preparation."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "scripts/md3/review-interaction-plan.py"
spec = importlib.util.spec_from_file_location("interaction_plan", SCRIPT)
plan_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plan_module)


class ReviewPlanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
        cls.contracts = plan_module.contract_at(ROOT, cls.source)

    def setUp(self):
        self.plan = {"schemaVersion": 1, "kind": "native-review-preparation", "sourceCommit": self.source,
                     "contractSha256": {p: hashlib.sha256(b).hexdigest() for p, b in self.contracts.items()},
                     "steps": [{"boundary": "shared-control-callers", "state": "menus/keyboard",
                                "action": "observe-native", "timeoutMs": 1000}]}

    def validate(self, plan=None):
        return plan_module.validate_plan(self.plan if plan is None else plan, self.source, self.contracts)

    def test_actual_nine_boundary_examples(self):
        examples = {
            "shared-control-callers": "menus/keyboard", "shell-nested-fit": "shell/overflow",
            "specialized-continuations": "farm/account", "dense-settings-subforms": "schedules/rule-editor",
            "device-nested-details": "print/mapping", "reader-detail-variants": "regex/invalid",
            "workspace-inherited-details": "project/notes", "embedded-alternate-flows": "home-web/offline",
            "renderer-tool-interiors": "prepare/object-selection"}
        self.plan["steps"] = [dict(boundary=b, state=s, action="observe-native", timeoutMs=1000)
                              for b, s in examples.items()]
        result = self.validate()
        self.assertEqual(result["steps"], 9)
        self.assertEqual(result["execution"], "not_attempted")
        self.assertEqual(result["runtimeAcceptance"], "unverified")

    def test_menu_navigation_only_in_reviewed_menu(self):
        for action in sorted(plan_module.MENU_ACTIONS):
            with self.subTest(action=action):
                self.plan["steps"][0]["action"] = action
                self.validate()
                bad = copy.deepcopy(self.plan)
                bad["steps"][0].update(boundary="device-nested-details", state="print/mapping")
                with self.assertRaises(ValueError):
                    self.validate(bad)

    def test_every_explicit_declared_state(self):
        # Independent inventory: do not derive the expected set with the parser
        # being tested. Read current source so declaration edits are tested before
        # commit; the CLI test separately verifies immutable committed reads.
        inventory = {
            "shared-control-callers": {"menus": "submenu disabled keyboard dismiss"},
            "shell-nested-fit": {"shell": "project-tabs overflow pinned-tabs grouped-tabs minimum-size"},
            "specialized-continuations": {"farm": "account", "confirmations": "untouched cancel", "monitor": "firmware"},
            "dense-settings-subforms": {"preferences": "search no-match scheduled-settings",
                "schedules": "empty rule-editor cross-midnight", "calibration": "results save-result validation"},
            "device-nested-details": {"print": "mapping external-spool incompatible", "monitor": "connected offline"},
            "reader-detail-variants": {"regex": "matches invalid no-match", "import-export": "format options validation",
                "history": "project empty diff", "changelog": "dates search no-match"},
            "workspace-inherited-details": {"project": "workspace empty files notes checklist calendar-month calendar-agenda"},
            "embedded-alternate-flows": {"home-web": "signed-out empty offline",
                "wizard": "region printer-selection filament-selection", "parameters": "material"},
            "renderer-tool-interiors": {"prepare": "model-loaded object-selection"}}
        expected = {boundary: {surface + "/" + state for surface, values in groups.items()
                              for state in values.split()} for boundary, groups in inventory.items()}
        contracts = {path: (ROOT / path).read_bytes() for path in plan_module.CONTRACTS}
        self.assertEqual(plan_module.queue_states(contracts), expected)
        self.assertEqual(sum(map(len, expected.values())), 55)
        self.plan["contractSha256"] = {path: hashlib.sha256(raw).hexdigest() for path, raw in contracts.items()}
        for boundary, states in expected.items():
            for state in sorted(states):
                with self.subTest(boundary=boundary, state=state):
                    self.plan["steps"][0].update(boundary=boundary, state=state)
                    result = plan_module.validate_plan(self.plan, self.source, contracts)
                    self.assertEqual(result["execution"], "not_attempted")
                    self.assertEqual(result["runtimeAcceptance"], "unverified")

    def test_excluded_workspace_history_is_not_a_positive_state(self):
        self.plan["steps"][0].update(boundary="workspace-inherited-details", state="project/history")
        with self.assertRaises(ValueError):
            self.validate()

    def test_no_actions_that_submit_or_invent_input(self):
        for action in ("print", "send", "slice", "bind", "update-firmware", "shell", "callback",
                       "click", "press_keys", "Enter", "resize", "type-text", "open-menu", "capture"):
            with self.subTest(action=action), self.assertRaises(ValueError):
                bad = copy.deepcopy(self.plan)
                bad["steps"][0]["action"] = action
                self.validate(bad)

    def test_unknown_fields_and_executed_claims(self):
        for field, value in (("hwnd", 123), ("x", 10), ("keys", ["Enter"]), ("callback", "print"),
                             ("executed", True), ("status", "passed"), ("observedState", "menus/keyboard"),
                             ("target", "Print"), ("expectedState", "menus/dismiss")):
            for location in ("root", "step"):
                with self.subTest(field=field, location=location), self.assertRaises(ValueError):
                    bad = copy.deepcopy(self.plan)
                    (bad if location == "root" else bad["steps"][0])[field] = value
                    self.validate(bad)

    def test_source_hash_and_cross_boundary_mismatch(self):
        for mutation in (lambda p: p.update(sourceCommit="0" * 40),
                         lambda p: p["contractSha256"].update({plan_module.CONTRACTS[0]: "0" * 64}),
                         lambda p: p["steps"][0].update(state="print/mapping"),
                         lambda p: p["steps"][0].update(state="menus/invented"),
                         lambda p: p.update(kind="execution-receipt"),
                         lambda p: p.update(schemaVersion=True)):
            bad = copy.deepcopy(self.plan)
            mutation(bad)
            with self.assertRaises(ValueError):
                self.validate(bad)

    def test_bounds_and_types(self):
        for timeout in (0, 99, 2001, True, 100.0, "100", None):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                bad = copy.deepcopy(self.plan)
                bad["steps"][0]["timeoutMs"] = timeout
                self.validate(bad)
        for count in (0, 31, 65):
            bad = copy.deepcopy(self.plan)
            bad["steps"] *= count
            with self.assertRaises(ValueError):
                self.validate(bad)
        for field in ("boundary", "state", "action"):
            bad = copy.deepcopy(self.plan)
            bad["steps"][0][field] = []
            with self.assertRaises(ValueError):
                self.validate(bad)

    def test_decoder_rejects_ambiguous_or_large_input(self):
        for raw in (b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":Infinity}', b'\xff',
                    b' ' * (plan_module.MAX_BYTES + 1), b'[' * 2000):
            with self.subTest(raw_length=len(raw)), self.assertRaises(ValueError):
                plan_module.decode(raw)

    def test_cli_offline_and_no_execute_option(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "plan.json"
            path.write_text(json.dumps(self.plan), encoding="utf-8")
            command = [sys.executable, str(SCRIPT), "--repository", str(ROOT), "--source-commit",
                       self.source, "--plan", str(path)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["execution"], "not_attempted")
            self.assertEqual(subprocess.run(command + ["--execute"], capture_output=True,
                                           timeout=30).returncode, 2)


if __name__ == "__main__":
    unittest.main()
