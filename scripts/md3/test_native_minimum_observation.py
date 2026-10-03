"""Hosted-only source-isolated checks of the production receipt predicate."""
import ast
import copy
import os
from pathlib import Path

if not (os.environ.get("GITHUB_ACTIONS") == "true" and
        os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
        os.environ.get("RUNNER_OS") == "Windows"):
    raise SystemExit("Disposable hosted Windows execution is required")
source = Path(__file__).with_name("drive-native-interface.py")
tree = ast.parse(source.read_text(encoding="utf-8-sig"))
nodes = [node for node in tree.body if isinstance(node, ast.FunctionDef)
         and node.name == "minimum_observation_valid"]
assert len(nodes) == 1
namespace = {}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), namespace)
valid = namespace["minimum_observation_valid"]
row = {"status": "measured_minimum_contained", "main_hwnd": 123, "minimum_outer": {"w": 1520, "h": 980},
       "interactive_resize_clamp": "unverified", "native_input_target": {
           "outer": [0, 0, 1520, 980], "work_area": [0, 0, 1600, 1120],
           "client": [1504, 942], "dpi": 192, "contained": True,
           "captured_hwnd": 123, "capture_geometry_verified": True}}
for key, value in (("dpi", 96), ("contained", False), ("outer", [0, 0, 1200, 800]),
                   ("work_area", [0, 0, 1500, 1120]), ("client", [0, 942]),
                   ("outer", [False, 0, 1520, 980]), ("captured_hwnd", 456),
                   ("capture_geometry_verified", False)):
    bad = copy.deepcopy(row)
    bad["native_input_target"][key] = value
    assert valid(bad) is False
assert valid(row) is True
print("Native minimum receipt checks passed: 9/9")
