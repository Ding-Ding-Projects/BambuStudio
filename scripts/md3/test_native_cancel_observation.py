"""Hosted-only source-isolated tests of the production cancel-target selector.

No desktop, UI provider, application or input service is loaded. The AST loader
compiles the actual helper, rather than copying its implementation into a test.
"""
import ast
from pathlib import Path
import unittest


def production_selector():
    path = Path(__file__).with_name("drive-native-interface.py")
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    names = {"require", "select_cancel_observation"}
    functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    if {node.name for node in functions} != names:
        raise AssertionError("Production observation helpers are missing")
    scope = {}
    exec(compile(ast.Module(body=functions, type_ignores=[]), str(path), "exec"), scope)
    return scope["select_cancel_observation"]


def known_target(age=10, generation=7):
    return {"workerStateKnown": True, "workerRunning": True, "outcome": "running",
            "nativeGeneration": generation, "pending": {"action": "print"},
            "cancelTarget": {"visible": True, "ageMs": age, "nativeGeneration": generation,
                             "coordinateSpace": "screen-pixels", "rect": [10, 20, 30, 40],
                             "canvasRect": [0, 0, 100, 100]}}


class Timeline:
    def __init__(self, observations, read_seconds=0):
        self.observations = list(observations)
        self.read_seconds = read_seconds
        self.now, self.reads = 0.0, 0

    def clock(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds

    def read(self):
        self.reads += 1
        self.now += self.read_seconds
        return self.observations.pop(0) if self.observations else {"workerStateKnown": False}


class CancelObservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.select = staticmethod(production_selector())

    def select_then_input(self, timeline, inputs, timeout=0.2):
        selected = self.select(timeline.read, 7, "print", timeline.clock, timeline.sleep, timeout)
        # This boundary is reached only after the actual production selector
        # authorizes a fresh target. It never calls a native input provider.
        inputs.append(selected[1])
        return selected

    def assert_no_input(self, timeline, timeout=0.2):
        inputs = []
        with self.assertRaisesRegex(RuntimeError, "not freshly observed"):
            self.select_then_input(timeline, inputs, timeout)
        self.assertEqual(inputs, [])

    def test_unknown_only_expires_without_input(self):
        timeline = Timeline([{"workerStateKnown": False}])
        self.assert_no_input(timeline)
        self.assertGreater(timeline.reads, 1)

    def test_known_stale_then_unknown_never_reuses_old_target(self):
        self.assert_no_input(Timeline([known_target(age=501), {"workerStateKnown": False}]))

    def test_known_invalid_coordinates_then_unknown_never_selects(self):
        candidate = known_target()
        candidate["cancelTarget"]["coordinateSpace"] = "unknown"
        self.assert_no_input(Timeline([candidate, {"workerStateKnown": False}]))

    def test_fresh_valid_observation_authorizes_exactly_one_target(self):
        candidate = known_target()
        timeline, inputs = Timeline([candidate]), []
        observed, target, observed_at = self.select_then_input(timeline, inputs)
        self.assertIs(observed, candidate)
        self.assertIs(target, candidate["cancelTarget"])
        self.assertEqual(observed_at, 0)
        self.assertEqual(inputs, [target])
        self.assertEqual(timeline.reads, 1)

    def test_read_returning_after_deadline_cannot_authorize_input(self):
        self.assert_no_input(Timeline([known_target()], read_seconds=0.3))

    def test_transport_time_counts_against_rendered_age(self):
        self.assert_no_input(Timeline([known_target(age=490)], read_seconds=0.02))

    def test_new_generation_never_authorizes_old_request(self):
        inputs = []
        with self.assertRaisesRegex(RuntimeError, "generation ended"):
            self.select_then_input(Timeline([known_target(generation=8)]), inputs)
        self.assertEqual(inputs, [])


if __name__ == "__main__":
    unittest.main()
