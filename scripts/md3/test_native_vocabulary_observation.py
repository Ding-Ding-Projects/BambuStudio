"""Hosted source-isolated contracts, not native getter or pixel evidence."""
import ast
import copy
import json
from pathlib import Path
import tempfile
import unittest


def production(mutate=False):
    path = Path(__file__).with_name("drive-native-interface.py")
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    names = {"require", "require_vocabulary_getters"}
    functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    assert {node.name for node in functions} == names
    driver = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == "Driver")
    functions += [next(node for node in driver.body if isinstance(node, ast.FunctionDef) and node.name == "vocabulary")]
    if mutate:
        target = ast.dump(ast.parse('getters.get("getUnwrappedLabelEqualsGetLabel") is True', mode="eval").body)

        class RemoveEquality(ast.NodeTransformer):
            changed = 0

            def visit_Compare(self, node):
                if ast.dump(node) == target:
                    self.changed += 1
                    return ast.copy_location(ast.Constant(value=True), node)
                return self.generic_visit(node)

        mutation = RemoveEquality()
        functions = [mutation.visit(node) for node in functions]
        assert mutation.changed == 1
    namespace = {"json": json}
    exec(compile(ast.Module(body=functions, type_ignores=[]), str(path), "exec"), namespace)
    return namespace


def observation():
    title = {"hwnd": 123, "top": 100, "type": 50020, "name": "Personal vocabulary",
             "offscreen": False, "rect": [10, 20, 210, 50]}
    row = {"kind": "window", "name": "personal-vocabulary-title", "hwnd": 123, "top": 100,
           "on_screen": True, "label": "Personal vocabulary", "screen": {"x": 10, "y": 20, "w": 200, "h": 30},
           "native_getters": {"schemaVersion": 1, "getLabelTextEqualsGetLabel": True,
                              "getUnwrappedLabelEqualsGetLabel": True}}
    return row, title


class VocabularyObservation(unittest.TestCase):
    def check(self, rows, title):
        return production()["require_vocabulary_getters"](rows, title, "Personal vocabulary")

    def test_all_three_getters_are_required(self):
        row, title = observation()
        result = self.check([row], title)
        self.assertTrue(result["getLabelMatchesOriginal"])
        self.assertTrue(result["getLabelTextEqualsGetLabel"])
        self.assertTrue(result["getUnwrappedLabelEqualsGetLabel"])

    def test_missing_and_ambiguous_observations_fail(self):
        row, title = observation()
        for rows in ([], [row, copy.deepcopy(row)]):
            with self.assertRaises(RuntimeError):
                self.check(rows, title)

    def test_wrong_control_or_geometry_fails(self):
        row, title = observation()
        for field, value in (("hwnd", 456), ("hwnd", True), ("top", 101), ("on_screen", False),
                             ("name", "another-control"), ("screen", {"x": 11, "y": 20, "w": 200, "h": 30})):
            bad = copy.deepcopy(row)
            bad[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(RuntimeError):
                self.check([bad], title)

    def test_original_native_label_and_accessibility_are_both_required(self):
        row, title = observation()
        for target, field, value in ((row, "label", "Fixture wording alpha"),
                                     (title, "name", "Fixture wording alpha"),
                                     (title, "hwnd", 0), (title, "offscreen", True)):
            previous = target[field]
            target[field] = value
            with self.assertRaises(RuntimeError):
                self.check([row], title)
            target[field] = previous

    def test_false_or_coerced_getter_results_fail(self):
        row, title = observation()
        for key in ("getLabelTextEqualsGetLabel", "getUnwrappedLabelEqualsGetLabel"):
            for value in (False, 1, "true", None):
                bad = copy.deepcopy(row)
                bad["native_getters"][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(RuntimeError):
                    self.check([bad], title)

    def test_missing_or_wrong_schema_fails(self):
        row, title = observation()
        for getters in (None, {}, {**row["native_getters"], "schemaVersion": True},
                        {**row["native_getters"], "schemaVersion": 2},
                        {**row["native_getters"], "unexpected": True}):
            bad = {**row, "native_getters": getters}
            with self.assertRaises(RuntimeError):
                self.check([bad], title)

    def test_removed_unwrapped_comparison_exposes_the_regression(self):
        row, title = observation()
        row["native_getters"]["getUnwrappedLabelEqualsGetLabel"] = False
        with self.assertRaises(RuntimeError):
            self.check([row], title)
        self.assertFalse(production(mutate=True)["require_vocabulary_getters"](
            [row], title, "Personal vocabulary")["getUnwrappedLabelEqualsGetLabel"])


class ScriptedFlow:
    """Exercise production sequencing with fake observations, never claim UI proof."""
    vocabulary = production()["vocabulary"]

    def __init__(self, scratch, corrupt=None):
        self.scratch, self.corrupt = scratch, corrupt
        self.mapping, self.status = "original", "original"
        self.inputs, self.captures, self.getter_checks = [], 1, 0  # Existing native-ready image.

    def open_vocabulary(self, prefix):
        self.captures += 4  # Edit, Preferences, search focus, search text.
        return 100

    def one(self, name):
        return name

    def label(self, name):
        return name

    def upload_vocabulary(self, prefix, target, fixture, preferences):
        self.inputs.append(prefix)
        self.captures += 4  # Open picker, focus, type, submit.
        try:
            value = json.loads(fixture.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            value = None
        if value is None or value["schemaVersion"] != 1:
            self.status = "rejected"
            if self.corrupt == prefix:
                self.mapping = "unexpected replacement"
        else:
            self.mapping = value["entries"]["Personal vocabulary"]
            if self.corrupt != "reset" or prefix != "valid-1":
                self.status = "active"

    def title_pixels(self, label, verify_getters=False):
        assert verify_getters
        self.getter_checks += 1
        self.captures += 1
        return self.mapping

    def prose_status(self, text):
        return ((text == "Personal vocabulary is active on this device." and self.status == "active") or
                (text.startswith("The vocabulary file could not be applied.") and self.status == "rejected"))

    def candidates(self, name):
        return name == "Personal vocabulary" or (name == "Load JSON" and self.mapping == "original") or (
            name == "Replace JSON" and self.mapping != "original")

    def click(self, label, target):
        assert target == "Clear personal vocabulary"
        self.mapping, self.status = "original", "original"
        self.captures += 1


class VocabularyFlow(unittest.TestCase):
    def test_both_invalid_branches_preserve_state_within_capture_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            flow = ScriptedFlow(Path(directory))
            flow.vocabulary()
            self.assertEqual(flow.inputs, ["valid-0", "malformed", "valid-1", "invalid"])
            self.assertEqual(flow.captures, 28)
            self.assertEqual(flow.getter_checks, 6)
            self.assertEqual(flow.mapping, "original")

    def test_invalid_preservation_and_status_reset_are_required(self):
        for corrupt in ("malformed", "invalid", "reset"):
            with self.subTest(corrupt=corrupt), tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(RuntimeError):
                    ScriptedFlow(Path(directory), corrupt).vocabulary()


if __name__ == "__main__":
    unittest.main()
