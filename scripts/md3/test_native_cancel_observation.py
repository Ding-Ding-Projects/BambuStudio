"""Hosted-only source-isolated tests of production cancellation observations.

No desktop, UI provider, application or input service is loaded. The AST loader
compiles the actual helper, rather than copying its implementation into a test.
"""
import ast
import copy
from pathlib import Path
import unittest


def production_helpers(mutate_epoch=False):
    path = Path(__file__).with_name("drive-native-interface.py")
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    names = {"require", "select_cancel_observation", "cancel_anchor",
             "require_cancel_epoch", "wait_cancel_completion"}
    functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    if {node.name for node in functions} != names:
        raise AssertionError("Production observation helpers are missing")
    if mutate_epoch:
        # Deliberately remove one production check. The same rejected-drift
        # scenario must then be accepted, proving that the regression detects it.
        expected = ast.dump(ast.parse(
            'state.get("nativeGeneration") == anchor["cancellationGeneration"]', mode="eval").body)

        class RemoveEpochCheck(ast.NodeTransformer):
            changed = 0

            def visit_Compare(self, node):
                if ast.dump(node) == expected:
                    self.changed += 1
                    return ast.copy_location(ast.Constant(value=True), node)
                return self.generic_visit(node)

        mutation = RemoveEpochCheck()
        functions = [mutation.visit(node) for node in functions]
        if mutation.changed != 1:
            raise AssertionError("Expected exactly one production epoch check to mutate")
    scope = {}
    exec(compile(ast.Module(body=functions, type_ignores=[]), str(path), "exec"), scope)
    return scope


def known_target(age=10, generation=7):
    return {"workerStateKnown": True, "workerRunning": True, "outcome": "running",
            "nativeGeneration": generation, "requestGeneration": 9, "modelRevision": 13,
            "completionSequence": 0, "completionEvents": [], "continuationSequence": 0,
            "processingPlateIndex": 0, "currentPlate": 0, "cancellationRequested": False,
            "pending": {"action": "print", "nativeGeneration": generation, "requestGeneration": 9,
                        "plateIndex": 0, "matchesCurrentPlate": True, "matchesProcessingPlate": True},
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
        cls.select = staticmethod(production_helpers()["select_cancel_observation"])

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


def cancellation_state(complete=False):
    state = known_target()
    state.update(nativeGeneration=8, cancellationRequested=True, pending={"action": "none"})
    if complete:
        state.update(workerRunning=False, outcome="cancelled", completionSequence=1,
                     completionEvents=[{"sequence": 1, "accepted": True, "rejection": "none",
                                        "status": "cancelled", "eventGeneration": 8, "currentGeneration": 8}])
    return state


class CancelEpochTests(unittest.TestCase):
    def setUp(self):
        self.helpers = production_helpers()
        self.anchor = self.helpers["cancel_anchor"](known_target(), "print")

    def wait(self, observations, timeout=0.3, read_seconds=0, helpers=None):
        timeline = Timeline(observations, read_seconds)
        # Repeat the last valid snapshot to model an event that never arrives.
        last = copy.deepcopy(observations[-1])

        def read():
            if not timeline.observations:
                timeline.observations.append(copy.deepcopy(last))
            return timeline.read()

        result = (helpers or self.helpers)["wait_cancel_completion"](
            read, self.anchor, timeline.clock, timeline.sleep, timeout)
        return result, timeline

    def test_fresh_input_anchors_the_next_cancellation_epoch(self):
        self.assertEqual(self.anchor["inputGeneration"], 7)
        self.assertEqual(self.anchor["cancellationGeneration"], 8)
        self.assertEqual(self.anchor["completionSequence"], 0)
        self.helpers["require_cancel_epoch"](cancellation_state(), self.anchor)

    def test_already_cancelled_input_and_invalid_counters_are_rejected(self):
        for key, value in (("cancellationRequested", True), ("nativeGeneration", True),
                           ("nativeGeneration", 2**64 - 1), ("completionSequence", -1),
                           ("modelRevision", 13.0), ("currentPlate", 1)):
            with self.subTest(key=key, value=value):
                state = known_target()
                state[key] = value
                with self.assertRaises(RuntimeError):
                    self.helpers["cancel_anchor"](state, "print")

    def test_epoch_identity_and_continuation_drift_are_rejected(self):
        for key, value in (("nativeGeneration", 7), ("nativeGeneration", 9),
                           ("requestGeneration", 10), ("modelRevision", 14),
                           ("processingPlateIndex", 1), ("currentPlate", 1),
                           ("cancellationRequested", False), ("continuationSequence", 1),
                           ("pending", {"action": "send"}), ("nativeGeneration", 8.0)):
            with self.subTest(key=key, value=value):
                state = cancellation_state()
                state[key] = value
                with self.assertRaises(RuntimeError):
                    self.helpers["require_cancel_epoch"](state, self.anchor)

    def test_cancelled_outcome_without_delivered_event_times_out(self):
        state = cancellation_state()
        state.update(workerRunning=False, outcome="cancelled")
        with self.assertRaisesRegex(RuntimeError, "not observed before timeout"):
            self.wait([state])

    def test_wrong_or_rejected_completion_never_proves_cancellation(self):
        for changes in ({"accepted": False, "rejection": "ignored"}, {"status": "completed"},
                        {"eventGeneration": 7}, {"currentGeneration": 9},
                        {"eventGeneration": 8.0}, {"accepted": 1}, {"rejection": "stale_generation"}):
            with self.subTest(changes=changes):
                state = cancellation_state(complete=True)
                state["completionEvents"][0].update(changes)
                with self.assertRaisesRegex(RuntimeError, "not observed before timeout"):
                    self.wait([state])

    def test_pre_input_event_is_not_reused(self):
        self.anchor["completionSequence"] = 1
        with self.assertRaisesRegex(RuntimeError, "not observed before timeout"):
            self.wait([cancellation_state(complete=True)])

    def test_unknown_ownership_and_event_delay_require_a_later_complete_observation(self):
        unknown = cancellation_state(complete=True)
        unknown.update(workerStateKnown=False, workerRunning=None)
        result, timeline = self.wait([cancellation_state(), unknown, cancellation_state(complete=True)])
        self.assertEqual(timeline.reads, 3)
        self.assertEqual(result[1]["sequence"], 1)
        self.assertEqual(result[0]["nativeGeneration"], 8)

    def test_unknown_ownership_never_counts_as_release(self):
        unknown = cancellation_state(complete=True)
        unknown.update(workerStateKnown=False, workerRunning=None)
        with self.assertRaisesRegex(RuntimeError, "not observed before timeout"):
            self.wait([unknown])

    def test_overwritten_completion_history_is_rejected(self):
        state = cancellation_state(complete=True)
        state["completionSequence"] = 17
        state["completionEvents"] = [dict(state["completionEvents"][0], sequence=i) for i in range(2, 18)]
        with self.assertRaisesRegex(RuntimeError, "missing or overwritten"):
            self.wait([state])

    def test_complete_response_after_deadline_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "not observed before timeout"):
            self.wait([cancellation_state(complete=True)], read_seconds=0.4)

    def test_epoch_regression_detects_deliberately_removed_production_check(self):
        state = cancellation_state(complete=True)
        state["nativeGeneration"] = 9
        with self.assertRaisesRegex(RuntimeError, "epoch, request"):
            self.wait([state])
        result, _ = self.wait([state], helpers=production_helpers(mutate_epoch=True))
        self.assertEqual(result[0]["nativeGeneration"], 9)
        self.assertEqual(result[1]["eventGeneration"], 8)


if __name__ == "__main__":
    unittest.main()
