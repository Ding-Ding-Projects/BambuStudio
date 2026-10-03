"""Hosted-only checks of actual fixture expressions without loading UI providers."""
import ast
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
EXPECTED = ROOT / "tests/automation-fixtures/cube.stl"


def fixture_path(driver_name, invocation):
    source = ROOT / "scripts/md3" / driver_name
    tree = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    # Evaluate only the real path assignments, never the driver or its imports.
    names = {"fixture"} if driver_name == "drive-automation.py" else {"HERE", "source"}
    assignments = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name) and target.id in names:
                if target.id == "HERE" or any(
                    isinstance(part, ast.Constant) and part.value == "tests/automation-fixtures/cube.stl"
                    for part in ast.walk(node.value)
                ):
                    if target.id in assignments:
                        raise AssertionError("Ambiguous production fixture assignment")
                    assignments[target.id] = node.value
    if set(assignments) != names:
        raise AssertionError("Production fixture assignments are missing")
    scope = {"Path": Path, "__file__": str(invocation)}
    for name in ("HERE", "fixture", "source"):
        if name in assignments:
            scope[name] = eval(compile(ast.Expression(assignments[name]), str(source), "eval"), scope)
    return scope["fixture" if driver_name == "drive-automation.py" else "source"]


class AutomationFixturePathTests(unittest.TestCase):
    def check_invocation(self, invocation_parent):
        self.assertTrue(EXPECTED.is_file(), "The tracked fixture must exist")
        for driver in ("drive-automation.py", "drive-native-interface.py"):
            with self.subTest(driver=driver):
                actual = fixture_path(driver, invocation_parent / driver)
                self.assertEqual(actual, EXPECTED)
                self.assertTrue(actual.is_file())

    def test_canonical_invocation(self):
        self.check_invocation(ROOT / "scripts/md3")

    def test_ci_parent_segment_invocation(self):
        self.check_invocation(ROOT / "scripts/ci/../md3")

    def test_multiple_parent_segments(self):
        self.check_invocation(ROOT / "scripts/md3/../../scripts/ci/../md3")


if __name__ == "__main__":
    unittest.main()
