"""Product-free assertions for the actual installed checkbox acceptance predicate."""
import ast
import copy
from pathlib import Path
import unittest

from temporal_checkbox import checkbox_valid, transition_valid


class CheckboxContract(unittest.TestCase):
    def setUp(self):
        self.before = {"hwnd": 12, "top": 10, "name": "Case sensitive", "enabled": True,
            "offscreen": False, "focused": True, "toggle": 0, "rect": [10, 20, 54, 64]}
        self.after = {**self.before, "toggle": 1}

    def valid(self, **changes):
        return transition_valid(self.before, {**self.after, **changes}, 10, ["Add Primitive"], [], True)

    def test_actual_semantic_transition(self):
        self.assertTrue(self.valid())

    def test_missing_or_indeterminate_provider(self):
        for value in (None, True, 0, 2, "1"):
            self.assertFalse(self.valid(toggle=value))

    def test_identity_and_geometry_drift(self):
        self.assertFalse(self.valid(hwnd=13))
        self.assertFalse(self.valid(top=11))
        self.assertFalse(self.valid(rect=[11, 20, 55, 64]))

    def test_focus_visibility_enabled_required(self):
        for key, value in (("focused", False), ("offscreen", True), ("enabled", False)):
            self.assertFalse(self.valid(**{key: value}))

    def test_result_transition_required(self):
        for original, remaining, state in (([], [], True), (["Add Primitive"], ["Add Primitive"], True),
                                           (["Add Primitive"], [], False)):
            self.assertFalse(transition_valid(self.before, self.after, 10, original, remaining, state))

    def test_unselected_anchor_required(self):
        self.before["toggle"] = 1
        self.assertFalse(self.valid())

    def test_geometry_comparison_mutation_is_rejected(self):
        source = Path(__file__).with_name("temporal_checkbox.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "transition_valid")
        removed = 0
        class RemoveGeometry(ast.NodeTransformer):
            def visit_Compare(self, node):
                nonlocal removed
                if ast.unparse(node) == "before['rect'] == after['rect']":
                    removed += 1
                    return ast.copy_location(ast.Constant(True), node)
                return self.generic_visit(node)
        mutated = RemoveGeometry().visit(copy.deepcopy(function))
        self.assertEqual(removed, 1)
        namespace = {"checkbox_valid": checkbox_valid}
        exec(compile(ast.fix_missing_locations(ast.Module(body=[mutated], type_ignores=[])), "<mutation>", "exec"), namespace)
        moved = {**self.after, "rect": [11, 20, 55, 64]}
        self.assertFalse(self.valid(rect=moved["rect"]))
        self.assertTrue(namespace["transition_valid"](self.before, moved, 10, ["Add Primitive"], [], True))


if __name__ == "__main__":
    unittest.main()
